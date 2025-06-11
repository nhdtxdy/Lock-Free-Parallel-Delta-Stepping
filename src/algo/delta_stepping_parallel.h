#ifndef DELTA_STEPPING_PARALLEL_H
#define DELTA_STEPPING_PARALLEL_H

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

class DeltaSteppingParallel : public ShortestPathSolverBase {
public:
    const std::string name() const override {
        return "Optimized parallel delta stepping";
    }

    using Request = Edge;

    DeltaSteppingParallel(double delta, int num_threads): delta(delta), num_threads(num_threads) {}

    std::vector<double> compute(const Graph &graph, int source) const override {
        const double INF_MAX = std::numeric_limits<double>::infinity();
        int n = graph.size();
        std::vector<double> dist(n, INF_MAX);

        std::vector<std::vector<AdjEdge>> heavy(n), light(n);
        for (int u = 0; u < n; ++u) {
            for (const auto &[v, w] : graph[u]) {
                if (w < delta) {
                    light[u].push_back({v, w});
                }
                else {
                    heavy[u].push_back({v, w});
                }
            }
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

        auto gen_light_request = [&] (int u) {
            for (const auto &[v, w] : light[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(light_nodes_requested, light_nodes_counter, light_request_map, Request{u, v, w});
                }
            }
        };

        auto gen_heavy_request = [&] (int u) {
            for (const auto &[v, w] : heavy[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(heavy_nodes_requested, heavy_nodes_counter, heavy_request_map, Request{u, v, w});
                }
            }
        };

        // bucket type is either linked list or vector
        std::barrier<> barrier(num_threads + 1);
        FixedTaskPool pool(num_threads, barrier);

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
                    int curr_bucket_size = curr_bucket.size();
                    int chunk_size = (curr_bucket_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > curr_bucket_size) {
                            end = curr_bucket_size;
                        }
                        pool.push(idx, [&, start, end] {
                            for (int idx_u = start; idx_u < end; ++idx_u) {
                                int u = curr_bucket[idx_u];
                                if (u >= 0) {
                                    gen_light_request(u);
                                    gen_heavy_request(u);
                                }
                            }
                        });
                    }
                    barrier.arrive_and_wait(); // manual sync

                    curr_bucket.clear();
                }


                // Loop 2: relax light edges
                {
                    // std::cerr << "loop2\n";
                    int requests_size = light_nodes_counter;
                    int chunk_size = (requests_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > requests_size) {
                            end = requests_size;
                        }
                        pool.push(idx, [&, start, end] {
                            for (int idx_r = start; idx_r < end; ++idx_r) {
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
                int requests_size = heavy_nodes_counter;
                int chunk_size = (requests_size + num_threads - 1) / num_threads;
                for (int idx = 0; idx < num_threads; ++idx) {
                    int start = idx * chunk_size;
                    int end = start + chunk_size;
                    if (end > requests_size) {
                        end = requests_size;
                    }
                    pool.push(idx, [&, start, end] {
                        for (int idx_r = start; idx_r < end; ++idx_r) {
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
    int num_threads;
};

#endif