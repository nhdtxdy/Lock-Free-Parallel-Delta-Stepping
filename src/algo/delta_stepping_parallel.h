#ifndef DELTA_STEPPING_PARALLEL_H
#define DELTA_STEPPING_PARALLEL_H

#include "shortest_path_solver_base.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include "lists/fine_grained_dll.h"
#include "stacks/lock_free_stack.h"
#include "queues/queues.h"
#include "pools/BS_thread_pool.hpp"
#include "pools/flexible_pool.h"
#include <type_traits>
#include "pools/fast_pool.h"
#include "lists/thread_safe_vector.h"

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

        std::vector<int> position_in_bucket(n, -1);
        
        std::vector<ThreadSafeVector<int>> buckets(1);
        
        std::mutex buckets_resize_mutex;  // Add mutex for resize protection
        buckets[0].push_back(source);
        position_in_bucket[source] = 0;
        dist[source] = 0;

        std::vector<int> light_nodes_requested(n), heavy_nodes_requested(n);
        std::atomic<size_t> light_nodes_counter{0}, heavy_nodes_counter{0};

        std::vector<std::atomic<double>> light_request_map(n), heavy_request_map(n);

        std::vector<int> updated_nodes(n);
        std::atomic<size_t> updated_counter{0};

        const int MAX_BUCKET_COUNT = (int)ceil(graph.get_max_edge_weight() / delta) + 5;
        buckets.resize(MAX_BUCKET_COUNT);
        
        for (int i = 0; i < n; ++i) {
            light_request_map[i].store(std::numeric_limits<double>::infinity());
            heavy_request_map[i].store(std::numeric_limits<double>::infinity());
        }

        // positions is vector of atomic to node to Rl, Rh
        // Rl, Rh are lock-free stack

        auto get_bucket = [&] (int v) {
            if (dist[v] == INF_MAX) {
                return -1;
            }
            return int(dist[v] / delta) % MAX_BUCKET_COUNT;
        };

        auto insert_to_corresponding_bucket = [&] (int v) {
            // insert node v to its corresponding bucket
            int bucket_idx = get_bucket(v);
            position_in_bucket[v] = buckets[bucket_idx].push_back(v) - 1;
        };
        
        auto relax = [&] (int v, std::vector<std::atomic<double>> &requests) {
            double new_distance = requests[v].exchange(std::numeric_limits<double>::infinity());

            // note: during light edge relaxation, multiple readers - one writer can happen
            // but that is fine, because the next epoch will take care of this concurrency issue
            if (new_distance < dist[v]) {
                int old_bucket = get_bucket(v);
                if (old_bucket != -1) {
                    buckets[old_bucket][position_in_bucket[v]] = -1;
                }
                
                dist[v] = new_distance;

                size_t write_idx = updated_counter.fetch_add(1);
                updated_nodes[write_idx] = v;
                
                // int new_bucket = get_bucket(v);
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
        FastPool<moodycamel::BlockingConcurrentQueue> pool(num_threads);

        int generations_without_bucket = 0;
        for (int generation = 0; ; ++generation, ++generations_without_bucket) {
            if (generations_without_bucket >= MAX_BUCKET_COUNT) {
                break;
            }
            if (generation >= MAX_BUCKET_COUNT) {
                generation = 0;
            }
            while (!buckets[generation].empty()) {
                generations_without_bucket = 0;

                {
                    // Loop 1: request generation
                    pool.start();
                    ThreadSafeVector<int> &curr_bucket = buckets[generation];
                    int curr_bucket_size = curr_bucket.size();
                    int chunk_size = (curr_bucket_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > curr_bucket_size) {
                            end = curr_bucket_size;
                        }
                        if (start < end) {
                            pool.push([&, start, end] {
                                for (int idx_u = start; idx_u < end; ++idx_u) {
                                    int u = curr_bucket[idx_u];
                                    if (u >= 0) {
                                        gen_light_request(u);
                                    }
                                }
                            });
                            pool.push([&, start, end] {
                                for (int idx_u = start; idx_u < end; ++idx_u) {
                                    int u = curr_bucket[idx_u];
                                    if (u >= 0) {
                                        gen_heavy_request(u);
                                    }
                                }
                            });
                        }
                    }
                    pool.reset(); // equivalent to join()

                    curr_bucket.clear();
                }


                // Loop 2: relax light edges
                {
                    // std::cerr << "loop2\n";
                    pool.start();
                    int requests_size = light_nodes_counter;
                    int chunk_size = (requests_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > requests_size) {
                            end = requests_size;
                        }
                        if (start < end) {
                            pool.push([&, start, end] {
                                for (int idx_r = start; idx_r < end; ++idx_r) {
                                    int request_node = light_nodes_requested[idx_r];
                                    relax(request_node, light_request_map);
                                }
                            });
                        }
                    }
                    pool.reset();

                    light_nodes_counter = 0;
                }

                {
                    // Propagate updates to buckets

                    pool.start();
                    int chunk_size = (updated_counter + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > (int)updated_counter) {
                            end = updated_counter;
                        }
                        if (start < end) {
                            pool.push([&, start, end] {
                                for (int idx = start; idx < end; ++idx) {
                                    insert_to_corresponding_bucket(updated_nodes[idx]);
                                }
                            });
                        }
                    }
                    pool.reset();

                    updated_counter = 0;
                }
            }
            
            // Loop 3: relax heavy edges
            {
                pool.start();
                int requests_size = heavy_nodes_counter;
                int chunk_size = (requests_size + num_threads - 1) / num_threads;
                for (int idx = 0; idx < num_threads; ++idx) {
                    int start = idx * chunk_size;
                    int end = start + chunk_size;
                    if (end > requests_size) {
                        end = requests_size;
                    }
                    if (start < end) {
                        pool.push([&, start, end] {
                            for (int idx_r = start; idx_r < end; ++idx_r) {
                                int request_node = heavy_nodes_requested[idx_r];
                                relax(request_node, heavy_request_map);
                            }
                        });
                    }
                }
                pool.reset();

                heavy_nodes_counter = 0;
            }

            {
                // propagate updates to buckets
                pool.start();
                
                int chunk_size = (updated_counter + num_threads - 1) / num_threads;
                for (int idx = 0; idx < num_threads; ++idx) {
                    int start = idx * chunk_size;
                    int end = start + chunk_size;
                    if (end > (int)updated_counter) {
                        end = updated_counter;
                    }
                    if (start < end) {
                        pool.push([&, start, end] {
                            for (int idx = start; idx < end; ++idx) {
                                insert_to_corresponding_bucket(updated_nodes[idx]);
                            }
                        });
                    }
                }
                pool.reset();

                updated_counter = 0;
            }
        }

        pool.stop();

        return dist;
    }
private:
    double delta;
    int num_threads;
};

#endif