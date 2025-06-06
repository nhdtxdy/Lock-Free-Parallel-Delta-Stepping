#ifndef DELTA_STEPPING_PARALLEL_H
#define DELTA_STEPPING_PARALLEL_H

#include "shortest_path_solver_base.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include "queues/coarse_grained_unbounded_queue.h"
#include "lists/fine_grained_dll.h"
#include "queues/lock_free_queue.h"
#include "stacks/lock_free_stack.h"
#include "queues/head_tail_lock_queue_blocking.h"
#include "queues/two_stacks_queue_blocking.h"
#include "queues/lib/blockingconcurrentqueue.h"
#include "pools/BS_thread_pool.hpp"
#include "pools/flexible_pool.h"
#include <type_traits>

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
        std::vector<Request> Rl, Rh;
        // LockFreeStack<Request> Rl, Rh;
        std::mutex Rl_lock, Rh_lock;
        std::unordered_map<int, int> Rl_map, Rh_map;

        // std::vector<std::atomic<LockFreeStack

        // positions is vector of atomic to node to Rl, Rh
        // Rl, Rh are lock-free stack

        auto get_bucket = [&] (int v) {
            if (dist[v] == INF_MAX) {
                return -1;
            }
            return int(dist[v] / delta);
        };
        
        std::mutex lock_ngu;

        auto relax = [&] (const Request &request) {
            int u = request.u;
            int v = request.v;
            double w = request.w;
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

        // Strictest request optimization
        auto add_request = [&] (std::vector<Request> &requests, std::unordered_map<int, int> &positions, std::mutex &mtx, const Request &request) {
            std::lock_guard<std::mutex> lk(mtx);
            if (!positions.count(request.v)) {
                positions[request.v] = requests.size();
                requests.emplace_back(request);
            }
            else {
                int idx = positions[request.v];
                Request &existing = requests[idx];
                if (dist[request.u] + request.w < dist[existing.u] + existing.w) {
                    existing = request;
                }
            }
        };

        auto gen_light_request = [&] (int u) {
            for (const auto &[v, w] : light[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(Rl, Rl_map, Rl_lock, Request{u, v, w});
                }
            }
        };

        auto gen_heavy_request = [&] (int u) {
            for (const auto &[v, w] : heavy[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(Rh, Rh_map, Rh_lock, Request{u, v, w});
                }
            }
        };



        // bucket type is either linked list or vector
        // #define BS_THREAD_POOL

        #ifndef BS_THREAD_POOL
            FlexiblePool<moodycamel::BlockingConcurrentQueue> pool(num_threads);

            for (int i = 0; i < (int)buckets.size(); ++i) {
                while (!buckets[i].empty()) {
                    Rl.clear();
                    Rl_map.clear();

                    // Loop 1: request generation
                    std::vector<int> curr_bucket = buckets[i].list_all_and_clear();
                    // std::cerr << "bucket: " << i << '\n';
                    // for (auto u : curr_bucket) std::cerr << u << ' ';
                    // std::cerr << '\n';
                    
                    {
                        pool.start();
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
                        int requests_size = Rl.size();
                        int chunk_size = (requests_size + num_threads - 1) / num_threads;
                        for (int idx = 0; idx < num_threads; ++idx) {
                            int start = idx * chunk_size;
                            int end = start + chunk_size;
                            if (end > requests_size) {
                                end = requests_size;
                            }
                            pool.push([&, start, end] {
                                for (int idx_r = start; idx_r < end; ++idx_r) {
                                    const Request &request = Rl[idx_r];
                                    relax(request);
                                }
                            });
                        }
                        pool.reset();
                    }
                }
                
                // Loop 3: relax heavy edges
                {
                    pool.start();
                    int requests_size = Rh.size();
                    int chunk_size = (requests_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > requests_size) {
                            end = requests_size;
                        }
                        pool.push([&, start, end] {
                            for (int idx_r = start; idx_r < end; ++idx_r) {
                                const Request &request = Rh[idx_r];
                                relax(request);
                            }
                        });
                    }
                    pool.reset();
                }
                
                Rh.clear();
                Rh_map.clear();
            }

            pool.stop();
        #else
            BS::thread_pool pool(num_threads);

            for (int i = 0; i < (int)buckets.size(); ++i) {
                while (!buckets[i].empty()) {
                    Rl.clear();
                    Rl_map.clear();

                    // Loop 1: request generation
                    std::vector<int> curr_bucket = buckets[i].list_all_and_clear();
                    // std::cerr << "bucket: " << i << '\n';
                    // for (auto u : curr_bucket) std::cerr << u << ' ';
                    // std::cerr << '\n';
                    
                    int curr_bucket_size = curr_bucket.size();
                    {
                        int chunk_size = (curr_bucket_size + num_threads - 1) / num_threads;
                        for (int idx = 0; idx < num_threads; ++idx) {
                            int start = idx * chunk_size;
                            int end = start + chunk_size;
                            if (end > curr_bucket_size) {
                                end = curr_bucket_size;
                            }
                            pool.detach_task([&, start, end] {
                                for (int idx_u = start; idx_u < end; ++idx_u) {
                                    int u = curr_bucket[idx_u];
                                    gen_light_request(u);
                                }
                            });
                            pool.detach_task([&, start, end] {
                                for (int idx_u = start; idx_u < end; ++idx_u) {
                                    int u = curr_bucket[idx_u];
                                    gen_heavy_request(u);
                                }
                            });
                        }
                    }
                    pool.wait(); // equivalent to join()

                    // Loop 2: relax light edges
                    {
                        int requests_size = Rl.size();
                        int chunk_size = (requests_size + num_threads - 1) / num_threads;
                        for (int idx = 0; idx < num_threads; ++idx) {
                            int start = idx * chunk_size;
                            int end = start + chunk_size;
                            if (end > requests_size) {
                                end = requests_size;
                            }
                            pool.detach_task([&, start, end] {
                                for (int idx_r = start; idx_r < end; ++idx_r) {
                                    const Request &request = Rl[idx_r];
                                    relax(request);
                                }
                            });
                        }
                    }
                    pool.wait();
                }
                
                // Loop 3: relax heavy edges
                {
                    int requests_size = Rh.size();
                    int chunk_size = (requests_size + num_threads - 1) / num_threads;
                    for (int idx = 0; idx < num_threads; ++idx) {
                        int start = idx * chunk_size;
                        int end = start + chunk_size;
                        if (end > requests_size) {
                            end = requests_size;
                        }
                        pool.detach_task([&, start, end] {
                            for (int idx_r = start; idx_r < end; ++idx_r) {
                                const Request &request = Rh[idx_r];
                                relax(request);
                            }
                        });
                    }
                    pool.wait();
                }
                
                Rh.clear();
                Rh_map.clear();
            }

        #endif

        return dist;
    }
private:
    double delta;
    int num_threads;
};

#endif