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

class DeltaSteppingParallel : public ShortestPathSolverBase {
public:
    const std::string name() const override {
        return "Naive parallel delta stepping using FlexiblePool";
    }

    using Request = Edge;
    
    // Verify that std::atomic<Request> is valid
    static_assert(std::is_trivially_copyable<Request>::value);
    static_assert(std::is_copy_constructible<Request>::value);
    static_assert(std::is_move_constructible<Request>::value);
    static_assert(std::is_copy_assignable<Request>::value);
    static_assert(std::is_move_assignable<Request>::value);
    static_assert(std::is_same<Request, typename std::remove_cv<Request>::type>::value);

    static_assert(std::is_trivially_copyable<Request*>::value);
    static_assert(std::is_copy_constructible<Request*>::value);
    static_assert(std::is_move_constructible<Request*>::value);
    static_assert(std::is_copy_assignable<Request*>::value);
    static_assert(std::is_move_assignable<Request*>::value);
    static_assert(std::is_same<Request*, typename std::remove_cv<Request*>::type>::value);

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

        std::vector<FineGrainedDLL<int>::DLLNode*> idx_to_addr;
        for (int i = 0; i < n; ++i) {
            auto *node = new FineGrainedDLL<int>::DLLNode;
            node->data = i;
            idx_to_addr.push_back(node);
        }

        std::vector<FineGrainedDLL<int>> buckets(1);
        std::atomic<int> max_bucket_size{1};
        std::mutex buckets_resize_mutex;  // Add mutex for resize protection
        buckets[0].insert(idx_to_addr[source]);
        dist[source] = 0;

        LockFreeStack<int> light_nodes_requested, heavy_nodes_requested;

        std::vector<std::atomic<Request*>> light_request_map(n), heavy_request_map(n);
        
        for (int i = 0; i < n; ++i) {
            light_request_map[i].store(nullptr);
            heavy_request_map[i].store(nullptr);
        }

        // positions is vector of atomic to node to Rl, Rh
        // Rl, Rh are lock-free stack

        auto get_bucket = [&] (int v) {
            if (dist[v] == INF_MAX) {
                return -1;
            }
            return int(dist[v] / delta);
        };
        
        std::mutex lock_ngu;

        auto relax = [&] (int request_node, std::vector<std::atomic<Request*>> &requests) {
            Request* request_ptr = requests[request_node].exchange(nullptr);
            if (request_ptr == nullptr) return; // Already processed

            int u = request_ptr->u;
            int v = request_ptr->v;
            double w = request_ptr->w;

            delete request_ptr;

            // note: during light edge relaxation, multiple readers - one writer can happen
            // but that is fine, because the next epoch will take care of this concurrency issue
            if (dist[u] + w < dist[v]) {
                int old_bucket = get_bucket(v);
                dist[v] = dist[u] + w;
                int new_bucket = get_bucket(v);
                std::lock_guard<std::mutex> lk(lock_ngu);
                if (old_bucket >= 0) {
                    buckets[old_bucket].remove(idx_to_addr[v]);
                }
                if (new_bucket >= max_bucket_size) {
                    std::lock_guard<std::mutex> resize_lock(buckets_resize_mutex);
                    if (new_bucket >= max_bucket_size) {
                        int desired = new_bucket << 1; 
                        buckets.resize(desired);
                        max_bucket_size.store(desired);
                    }
                }
                buckets[new_bucket].insert(idx_to_addr[v]);
            }
        };

        // Strictest request optimization -- No mutexes
        auto add_request = [&] (LockFreeStack<int> &requested_nodes, std::vector<std::atomic<Request*>> &requests, const Request &request) {
            std::atomic<Request*> &state = requests[request.v];
            double new_distance = dist[request.u] + request.w;

            Request *new_request = new Request(request);            
            if (state.load() == nullptr) {
                Request *curr_state = state.load();
                while (curr_state == nullptr && !state.compare_exchange_weak(curr_state, new_request));
                if (curr_state == nullptr) {
                    requested_nodes.push(request.v);
                }
            }

            Request *current = state.load();
            while (current && new_distance < dist[current->u] + current->w) {
                if (state.compare_exchange_weak(current, new_request)) {
                    // delete current;
                    return;
                }
            }

            // delete new_request; // if we reach this point, the new request is not better --> simply delete
        };

        auto gen_light_request = [&] (int u) {
            for (const auto &[v, w] : light[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(light_nodes_requested, light_request_map, Request{u, v, w});
                }
            }
        };

        auto gen_heavy_request = [&] (int u) {
            for (const auto &[v, w] : heavy[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(heavy_nodes_requested, heavy_request_map, Request{u, v, w});
                }
            }
        };



        // bucket type is either linked list or vector
        FastPool<moodycamel::BlockingConcurrentQueue> pool(num_threads);

        for (int i = 0; i < (int)buckets.size(); ++i) {
            while (!buckets[i].empty()) {
                // Loop 1: request generation
                {
                    pool.start();
                    auto curr_bucket = buckets[i].list_all_and_clear();
                    int curr_bucket_size = curr_bucket.size();
                    int chunk_size = (curr_bucket_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > curr_bucket_size) {
                            end = curr_bucket_size;
                        }
                        pool.push([&, start, end] {
                            for (int idx_u = start; idx_u < end; ++idx_u) {
                                int u = curr_bucket[idx_u];
                                gen_light_request(u);
                            }
                        });
                        pool.push([&, start, end] {
                            for (int idx_u = start; idx_u < end; ++idx_u) {
                                int u = curr_bucket[idx_u];
                                gen_heavy_request(u);
                            }
                        });
                    }
                    pool.reset(); // equivalent to join()
                }

                // Loop 2: relax light edges
                {
                    pool.start();
                    int requests_size = light_nodes_requested.size();
                    int chunk_size = (requests_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > requests_size) {
                            end = requests_size;
                        }
                        pool.push([&, start, end] {
                            for (int idx_r = start; idx_r < end; ++idx_r) {
                                int request_node;
                                if (!light_nodes_requested.pop(request_node)) {
                                    std::cerr << "[FATAL] STACK NOT WORKING PROPERLY";
                                }
                                relax(request_node, light_request_map);
                            }
                        });
                    }
                    pool.reset();
                }
            }
            
            // Loop 3: relax heavy edges
            {
                pool.start();
                int requests_size = heavy_nodes_requested.size();
                int chunk_size = (requests_size + num_threads - 1) / num_threads;
                for (int idx = 0; idx < num_threads; ++idx) {
                    int start = idx * chunk_size;
                    int end = start + chunk_size;
                    if (end > requests_size) {
                        end = requests_size;
                    }
                    pool.push([&, start, end] {
                        for (int idx_r = start; idx_r < end; ++idx_r) {
                            int request_node;
                            if (!heavy_nodes_requested.pop(request_node)) {
                                std::cerr << "[FATAL] STACK NOT WORKING PROPERLY";
                            }
                            relax(request_node, heavy_request_map);
                        }
                    });
                }
                pool.reset();
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