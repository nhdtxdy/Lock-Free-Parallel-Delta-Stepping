#ifndef COMPLETELY_BALANCED_DELTA_STEPPING_2_H
#define COMPLETELY_BALANCED_DELTA_STEPPING_2_H

#include "shortest_path_solver_base.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include "lists/fine_grained_dll.h"
#include "stacks/lock_free_stack.h"
#include "queues/queues.h"
#include <type_traits>
#include "pools/fixed_task_pool.h"
#include "lists/thread_safe_vector.h"
#include "lists/circular_vector.h"
#include <cmath>
#include <atomic>
#include <barrier>
#include <algorithm>

class CompletelyBalancedDeltaStepping2 : public ShortestPathSolverBase {
public:
    const std::string name() const override {
        return "Parallel delta stepping with optimized load balancing - parallel prefix sums";
    }

    using Request = Edge;

    CompletelyBalancedDeltaStepping2(double delta, size_t num_threads): delta(delta), num_threads(num_threads) {}

    std::vector<double> compute(const Graph &graph, int source) const override {
        const double INF_MAX = std::numeric_limits<double>::infinity();
        int n = graph.size();
        std::vector<double> dist(n, INF_MAX);

        std::vector<size_t> adj_sizes(n);
        for (int u = 0; u < n; ++u) {
            adj_sizes[u] = graph[u].size();
        }

        const int MAX_BUCKET_COUNT = (int)std::ceil(graph.get_max_edge_weight() / delta) + 5;

        std::vector<int> position_in_bucket(n, -1);
        std::vector<CircularVector<int>> buckets;
        buckets.reserve(MAX_BUCKET_COUNT);

        for (int i = 0; i < MAX_BUCKET_COUNT; ++i) {
            buckets.emplace_back(n);
        }
        
        buckets[0].push(source);
        position_in_bucket[source] = 0;
        dist[source] = 0;

        std::vector<int> light_nodes_requested(n), heavy_nodes_requested(n);
        std::atomic<size_t> light_nodes_counter{0}, heavy_nodes_counter{0};

        std::vector<std::atomic<double>> light_request_map(n), heavy_request_map(n);

        std::vector<int> updated_nodes(n);
        std::atomic<size_t> updated_counter{0};

        int current_generation = 0;
        
        for (int i = 0; i < n; ++i) {
            light_request_map[i].store(std::numeric_limits<double>::infinity());
            heavy_request_map[i].store(std::numeric_limits<double>::infinity());
        }

        auto get_bucket = [&] (int v) {
            if (dist[v] == INF_MAX) {
                return -1;
            }
            return int(dist[v] / delta) % MAX_BUCKET_COUNT;
        };

        auto relax = [&] (int v, std::vector<std::atomic<double>> &requests) {
            double new_distance = requests[v].exchange(std::numeric_limits<double>::infinity());
            // note: during light edge relaxation, multiple readers - one writer can happen
            // but that is fine, because the next epoch will take care of this concurrency issue
            if (new_distance < dist[v]) {
                int old_bucket = get_bucket(v);
                dist[v] = new_distance;
                int new_bucket = get_bucket(v);
                if (old_bucket != -1 && old_bucket != current_generation && old_bucket != new_bucket) { // since current generation bucket is always cleared
                    buckets[old_bucket][position_in_bucket[v]] = -1;
                }
                if (old_bucket == current_generation || old_bucket != new_bucket) {
                    position_in_bucket[v] = buckets[new_bucket].push(v);
                }
            }
        };

        // Strictest request optimization -- No mutexes
        auto add_request = [&] (std::vector<int> &requested_nodes, std::atomic<size_t> &idx_counter, std::vector<std::atomic<double>> &requests, const Request &request) {
            std::atomic<double> &state = requests[request.v];
            double new_distance = dist[request.u] + request.w;
          
            if (std::isinf(state.load())) {
                double curr_state = state.load();
                while (std::isinf(curr_state) && !state.compare_exchange_weak(curr_state, new_distance));
                if (std::isinf(curr_state)) {
                    size_t curr_idx = idx_counter.fetch_add(1);
                    requested_nodes[curr_idx] = request.v;
                }
            }

            double current_distance = state.load();
            while (new_distance < current_distance && !state.compare_exchange_weak(current_distance, new_distance));
        };

        // bucket type is either linked list or vector
        std::barrier<> barrier(num_threads + 1);
        FixedTaskPool pool(num_threads, barrier);
        // Parallel prefix-sum over nodes to build global edge prefix
        std::vector<size_t> prefix(n, 0);   // prefix[0] = 0 by default
        std::vector<size_t> thread_totals(num_threads, 0);
        std::vector<size_t> thread_pref(num_threads, 0);

        int generations_without_bucket = 0;
        for (current_generation = 0; ; ++current_generation, ++generations_without_bucket) {
            if (generations_without_bucket >= MAX_BUCKET_COUNT) {
                break;
            }
            if (current_generation >= MAX_BUCKET_COUNT) {
                current_generation = 0;
            }
            while (!buckets[current_generation].empty()) {
                generations_without_bucket = 0;

                {
                    // Loop 1: request generation
                    CircularVector<int> &curr_bucket = buckets[current_generation];
                    size_t curr_bucket_size = curr_bucket.size();

                    size_t nodes_per_thread = (curr_bucket_size + num_threads - 1) / num_threads;

                    // (A) each thread fills prefix for its slice + counts edges
                    for (size_t tid = 0; tid < num_threads; ++tid) {
                        int l = tid * nodes_per_thread;
                        int r = std::min(curr_bucket_size, l + nodes_per_thread);
                        pool.push(tid, [&curr_bucket, &adj_sizes, &prefix, &thread_totals, tid, l, r] {
                            size_t running = 0;
                            for (int i = l; i < r; ++i) {
                                int u = curr_bucket[i];
                                if (u >= 0) {
                                    running += adj_sizes[u];
                                }
                                prefix[i] = running;
                            }
                            thread_totals[tid] = running;
                        });
                    }
                    barrier.arrive_and_wait();

                    // (B) master thread computes exclusive scan of thread_totals
                    thread_pref[0] = 0;
                    for (size_t tid = 0; tid < num_threads; ++tid) {
                        if (tid > 0) {
                            thread_pref[tid] = thread_pref[tid - 1];
                        }
                        thread_pref[tid] += thread_totals[tid];
                    }
                    
                    size_t total_edges = thread_pref[num_threads - 1];
                    
                    // (C) Even split of edges across threads using the global prefix
                    const size_t edge_chunk = (total_edges + num_threads - 1) / num_threads;
                    size_t curr_ptr = 0; // idx of current node batch

                    for (size_t tid = 0; tid < num_threads; ++tid) {
                        size_t start_e = static_cast<size_t>(tid) * edge_chunk;
                        size_t end_e   = std::min(total_edges, start_e + edge_chunk);
                        while (curr_ptr < num_threads && start_e >= thread_pref[curr_ptr]) {
                            ++curr_ptr;
                        }
                        size_t start_e_batch = start_e;
                        if (curr_ptr > 0) {
                            start_e_batch -= thread_pref[curr_ptr - 1];
                        }

                        pool.push(tid, [&, start_e, end_e, start_e_batch] {
                            if (start_e >= end_e) {
                                return;
                            }

                            size_t node_idx = std::upper_bound(prefix.begin() + curr_ptr * nodes_per_thread, prefix.begin() + std::min((curr_ptr + 1) * nodes_per_thread, curr_bucket_size), start_e_batch) - prefix.begin();
                            size_t edge_off = start_e_batch;
                            if (node_idx > 0) edge_off -= prefix[node_idx - 1];
                            size_t curr_edge = start_e;

                            while (curr_edge < end_e && node_idx < curr_bucket_size) {
                                int u = curr_bucket[node_idx];
                                if (u >= 0) {
                                    size_t deg = adj_sizes[u];
                                    for (size_t k = edge_off; k < deg && curr_edge < end_e; ++k, ++curr_edge) {
                                        const auto &[v, w] = graph[u][k];
                                        if (dist[u] + w < dist[v]) {
                                            if (w < delta) {
                                                add_request(light_nodes_requested, light_nodes_counter, light_request_map, Request{u, v, w});
                                            }
                                            else {
                                                add_request(heavy_nodes_requested, heavy_nodes_counter, heavy_request_map, Request({u, v, w}));
                                            }
                                        }
                                    }
                                }
                                ++node_idx;
                                edge_off = 0;
                            }
                        });
                    }

                    barrier.arrive_and_wait(); // ensure all edge processing done

                    curr_bucket.clear();
                }


                // Loop 2: relax light edges
                {
                    // std::cerr << "loop2\n";
                    size_t requests_size = light_nodes_counter;
                    size_t chunk_size = (requests_size + num_threads - 1) / num_threads;
                    for (size_t idx = 0; idx < num_threads; ++idx) {
                        size_t start = idx * chunk_size;
                        size_t end = start + chunk_size;
                        if (end > requests_size) {
                            end = requests_size;
                        }
                        pool.push(idx, [&, start, end] {
                            for (size_t idx_r = start; idx_r < end; ++idx_r) {
                                int request_node = light_nodes_requested[idx_r];
                                relax(request_node, light_request_map);
                            }
                        });
                    }
                    barrier.arrive_and_wait();

                    light_nodes_counter = 0;
                }
            }
            
            // Loop 3: relax heavy edges
            {
                size_t requests_size = heavy_nodes_counter;
                size_t chunk_size = (requests_size + num_threads - 1) / num_threads;
                for (size_t idx = 0; idx < num_threads; ++idx) {
                    size_t start = idx * chunk_size;
                    size_t end = start + chunk_size;
                    if (end > requests_size) {
                        end = requests_size;
                    }
                    pool.push(idx, [&, start, end] {
                        for (size_t idx_r = start; idx_r < end; ++idx_r) {
                            int request_node = heavy_nodes_requested[idx_r];
                            relax(request_node, heavy_request_map);
                        }
                    });
                }
                barrier.arrive_and_wait();

                heavy_nodes_counter = 0;
            }
        }

        return dist;
    }
private:
    double delta;
    size_t num_threads;
};

#endif