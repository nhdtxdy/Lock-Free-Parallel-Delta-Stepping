#ifndef DELTA_STEPPING_PARALLEL_PROFILED_H
#define DELTA_STEPPING_PARALLEL_PROFILED_H

#include "shortest_path_solver_base.h"
#include <limits>
#include <unordered_map>
#include <unordered_set>
#include <chrono>
#include <iostream>
#include <iomanip>
#include <atomic>
#include "pools/flexible_pool.h"
#include "queues/coarse_grained_unbounded_queue.h"
#include "lists/fine_grained_dll.h"
#include "queues/lock_free_queue.h"
#include "stacks/lock_free_stack.h"
#include "queues/head_tail_lock_queue_blocking.h"
#include "queues/two_stacks_queue_blocking.h"
#include "queues/lib/blockingconcurrentqueue.h"

class DeltaSteppingParallelProfiled : public ShortestPathSolverBase {
public:
    // Fine-grained performance profiling structure
    struct DetailedPerformanceStats {
        // Time measurements
        std::atomic<double> preprocessing_time{0.0};
        std::atomic<double> bucket_processing_time{0.0};
        
        // Lock contention times
        std::atomic<double> rl_lock_wait_time{0.0};
        std::atomic<double> rh_lock_wait_time{0.0};
        std::atomic<double> bucket_resize_lock_time{0.0};
        
        // DLL operation times
        std::atomic<double> dll_insert_time{0.0};
        std::atomic<double> dll_remove_time{0.0};
        std::atomic<double> dll_list_all_time{0.0};
        
        // Request generation breakdown
        std::atomic<double> light_edge_computation_time{0.0};
        std::atomic<double> heavy_edge_computation_time{0.0};
        std::atomic<double> request_dedup_time{0.0};
        
        // Memory and bucket operations
        std::atomic<double> bucket_resize_time{0.0};
        std::atomic<double> memory_allocation_time{0.0};
        
        // Thread pool overhead
        std::atomic<double> pool_start_time{0.0};
        std::atomic<double> pool_reset_time{0.0};
        
        // Relaxation breakdown
        std::atomic<double> distance_check_time{0.0};
        std::atomic<double> bucket_calculation_time{0.0};
        std::atomic<double> bucket_update_time{0.0};
        
        // Counters
        std::atomic<int> total_buckets_processed{0};
        std::atomic<int> total_light_requests{0};
        std::atomic<int> total_heavy_requests{0};
        std::atomic<int> total_relaxations{0};
        std::atomic<int> max_bucket_iterations{0};
        std::atomic<int> bucket_resizes{0};
        std::atomic<int> rl_lock_acquisitions{0};
        std::atomic<int> rh_lock_acquisitions{0};
        std::atomic<int> dll_operations{0};
        
        void print_stats() const {
            std::cout << "\n=== Detailed Delta Stepping Performance Analysis ===" << std::endl;
            std::cout << std::fixed << std::setprecision(6);
            
            double total_time = preprocessing_time.load() + bucket_processing_time.load();
            std::cout << "Total Runtime: " << total_time << " seconds" << std::endl;
            
            std::cout << "\n📊 High-Level Breakdown:" << std::endl;
            std::cout << "  Preprocessing:       " << preprocessing_time.load() << "s (" 
                      << (preprocessing_time.load()/total_time*100) << "%)" << std::endl;
            std::cout << "  Bucket Processing:   " << bucket_processing_time.load() << "s (" 
                      << (bucket_processing_time.load()/total_time*100) << "%)" << std::endl;
            
            std::cout << "\n🔒 Lock Contention Analysis:" << std::endl;
            double total_lock_time = rl_lock_wait_time.load() + rh_lock_wait_time.load() + bucket_resize_lock_time.load();
            std::cout << "  Total Lock Time:     " << total_lock_time << "s (" 
                      << (total_lock_time/total_time*100) << "%)" << std::endl;
            std::cout << "    Rl Lock Wait:      " << rl_lock_wait_time.load() << "s (" 
                      << rl_lock_acquisitions.load() << " acquisitions)" << std::endl;
            std::cout << "    Rh Lock Wait:      " << rh_lock_wait_time.load() << "s (" 
                      << rh_lock_acquisitions.load() << " acquisitions)" << std::endl;
            std::cout << "    Bucket Resize Lock:" << bucket_resize_lock_time.load() << "s (" 
                      << bucket_resizes.load() << " resizes)" << std::endl;
            
            std::cout << "\n📋 DLL Operations Analysis:" << std::endl;
            double total_dll_time = dll_insert_time.load() + dll_remove_time.load() + dll_list_all_time.load();
            std::cout << "  Total DLL Time:      " << total_dll_time << "s (" 
                      << (total_dll_time/total_time*100) << "%)" << std::endl;
            std::cout << "    Insert Operations: " << dll_insert_time.load() << "s" << std::endl;
            std::cout << "    Remove Operations: " << dll_remove_time.load() << "s" << std::endl;
            std::cout << "    List All Operations:" << dll_list_all_time.load() << "s" << std::endl;
            std::cout << "  Total DLL Ops:       " << dll_operations.load() << std::endl;
            
            std::cout << "\n⚙️ Request Generation Analysis:" << std::endl;
            std::cout << "  Light Edge Computation: " << light_edge_computation_time.load() << "s" << std::endl;
            std::cout << "  Heavy Edge Computation: " << heavy_edge_computation_time.load() << "s" << std::endl;
            std::cout << "  Request Deduplication:  " << request_dedup_time.load() << "s" << std::endl;
            
            std::cout << "\n🧵 Thread Pool Overhead:" << std::endl;
            double total_pool_time = pool_start_time.load() + pool_reset_time.load();
            std::cout << "  Total Pool Overhead: " << total_pool_time << "s (" 
                      << (total_pool_time/total_time*100) << "%)" << std::endl;
            std::cout << "    Pool Start Time:   " << pool_start_time.load() << "s" << std::endl;
            std::cout << "    Pool Reset Time:   " << pool_reset_time.load() << "s" << std::endl;
            
            std::cout << "\n🎯 Relaxation Breakdown:" << std::endl;
            double total_relax_time = distance_check_time.load() + bucket_calculation_time.load() + bucket_update_time.load();
            std::cout << "  Distance Checks:     " << distance_check_time.load() << "s" << std::endl;
            std::cout << "  Bucket Calculations: " << bucket_calculation_time.load() << "s" << std::endl;
            std::cout << "  Bucket Updates:      " << bucket_update_time.load() << "s" << std::endl;
            
            std::cout << "\n💾 Memory Operations:" << std::endl;
            std::cout << "  Bucket Resize Time:  " << bucket_resize_time.load() << "s" << std::endl;
            std::cout << "  Memory Allocation:   " << memory_allocation_time.load() << "s" << std::endl;
            
            std::cout << "\n📈 Workload Statistics:" << std::endl;
            std::cout << "  Total Buckets Processed: " << total_buckets_processed.load() << std::endl;
            std::cout << "  Max Bucket Iterations:   " << max_bucket_iterations.load() << std::endl;
            std::cout << "  Total Light Requests:    " << total_light_requests.load() << std::endl;
            std::cout << "  Total Heavy Requests:    " << total_heavy_requests.load() << std::endl;
            std::cout << "  Total Relaxations:       " << total_relaxations.load() << std::endl;
            
            // Performance per operation metrics
            if (rl_lock_acquisitions.load() > 0) {
                std::cout << "  Avg Rl Lock Time:        " << (rl_lock_wait_time.load()/rl_lock_acquisitions.load()*1000) << " ms" << std::endl;
            }
            if (dll_operations.load() > 0) {
                std::cout << "  Avg DLL Op Time:         " << (total_dll_time/dll_operations.load()*1000) << " ms" << std::endl;
            }
            
            std::cout << "\n🎯 Top Bottlenecks:" << std::endl;
            std::vector<std::pair<std::string, double>> bottlenecks = {
                {"Lock Contention", total_lock_time},
                {"DLL Operations", total_dll_time},
                {"Thread Pool Overhead", total_pool_time},
                {"Light Edge Computation", light_edge_computation_time.load()},
                {"Heavy Edge Computation", heavy_edge_computation_time.load()},
                {"Distance Checks", distance_check_time.load()},
                {"Bucket Updates", bucket_update_time.load()},
                {"Request Deduplication", request_dedup_time.load()}
            };
            
            std::sort(bottlenecks.begin(), bottlenecks.end(), 
                     [](const auto& a, const auto& b) { return a.second > b.second; });
            
            for (size_t i = 0; i < bottlenecks.size() && i < 5; ++i) {
                if (bottlenecks[i].second > 0.001) { // Only show significant times
                    std::cout << "  " << (i+1) << ". " << bottlenecks[i].first 
                              << ": " << bottlenecks[i].second << "s (" 
                              << (bottlenecks[i].second/total_time*100) << "%)" << std::endl;
                }
            }
            std::cout << "=======================================================" << std::endl;
        }
    };

    const std::string name() const override {
        return "Delta Stepping Parallel (Detailed Profiling)";
    }

    using Request = Edge;

    DeltaSteppingParallelProfiled(double delta, int num_threads): delta(delta), num_threads(num_threads) {}

    std::vector<double> compute(const Graph &graph, int source) const override {        
        DetailedPerformanceStats stats;
        auto start_total = std::chrono::high_resolution_clock::now();
        
        const double INF_MAX = std::numeric_limits<double>::infinity();
        int n = graph.size();
        std::vector<double> dist(n, INF_MAX);

        // Preprocessing phase with timing
        auto start_preprocessing = std::chrono::high_resolution_clock::now();
        
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

        // Memory allocation timing
        auto mem_start = std::chrono::high_resolution_clock::now();
        std::vector<FineGrainedDLL<int>::DLLNode*> idx_to_addr;
        for (int i = 0; i < n; ++i) {
            auto *node = new FineGrainedDLL<int>::DLLNode;
            node->data = i;
            idx_to_addr.push_back(node);
        }
        auto mem_end = std::chrono::high_resolution_clock::now();
        stats.memory_allocation_time.fetch_add(std::chrono::duration<double>(mem_end - mem_start).count());

        std::vector<FineGrainedDLL<int>> buckets(1);
        std::atomic<int> max_bucket_size{1};
        std::mutex buckets_resize_mutex;  // Add mutex for resize protection
        
        auto dll_start = std::chrono::high_resolution_clock::now();
        buckets[0].insert(idx_to_addr[source]);
        auto dll_end = std::chrono::high_resolution_clock::now();
        stats.dll_insert_time.fetch_add(std::chrono::duration<double>(dll_end - dll_start).count());
        stats.dll_operations.fetch_add(1);
        
        dist[source] = 0;
        std::vector<Request> Rl, Rh;
        std::mutex Rl_lock, Rh_lock;
        std::unordered_map<int, int> Rl_map, Rh_map;

        auto end_preprocessing = std::chrono::high_resolution_clock::now();
        stats.preprocessing_time.store(std::chrono::duration<double>(end_preprocessing - start_preprocessing).count());

        auto get_bucket = [&] (int v) {
            auto calc_start = std::chrono::high_resolution_clock::now();
            int result;
            if (dist[v] == INF_MAX) {
                result = -1;
            } else {
                result = int(dist[v] / delta);
            }
            auto calc_end = std::chrono::high_resolution_clock::now();
            stats.bucket_calculation_time.fetch_add(std::chrono::duration<double>(calc_end - calc_start).count());
            return result;
        };
        
        auto relax = [&] (const Request &request) {
            auto dist_check_start = std::chrono::high_resolution_clock::now();
            int u = request.u;
            int v = request.v;
            double w = request.w;
            bool should_relax = dist[u] + w < dist[v];
            auto dist_check_end = std::chrono::high_resolution_clock::now();
            stats.distance_check_time.fetch_add(std::chrono::duration<double>(dist_check_end - dist_check_start).count());
            
            if (should_relax) {
                auto bucket_update_start = std::chrono::high_resolution_clock::now();
                
                int old_bucket = get_bucket(v);
                dist[v] = dist[u] + w;
                int new_bucket = get_bucket(v);
                
                if (old_bucket >= 0) {
                    auto dll_remove_start = std::chrono::high_resolution_clock::now();
                    buckets[old_bucket].remove(idx_to_addr[v]);
                    auto dll_remove_end = std::chrono::high_resolution_clock::now();
                    stats.dll_remove_time.fetch_add(std::chrono::duration<double>(dll_remove_end - dll_remove_start).count());
                    stats.dll_operations.fetch_add(1);
                }
                
                if (new_bucket >= max_bucket_size) {
                    auto resize_lock_start = std::chrono::high_resolution_clock::now();
                    std::lock_guard<std::mutex> resize_lock(buckets_resize_mutex);
                    auto resize_lock_end = std::chrono::high_resolution_clock::now();
                    stats.bucket_resize_lock_time.fetch_add(std::chrono::duration<double>(resize_lock_end - resize_lock_start).count());
                    
                    if (new_bucket >= max_bucket_size) {
                        auto resize_start = std::chrono::high_resolution_clock::now();
                        int desired = new_bucket << 1; 
                        buckets.resize(desired);
                        max_bucket_size.store(desired);
                        auto resize_end = std::chrono::high_resolution_clock::now();
                        stats.bucket_resize_time.fetch_add(std::chrono::duration<double>(resize_end - resize_start).count());
                        stats.bucket_resizes.fetch_add(1);
                    }
                }
                
                auto dll_insert_start = std::chrono::high_resolution_clock::now();
                buckets[new_bucket].insert(idx_to_addr[v]);
                auto dll_insert_end = std::chrono::high_resolution_clock::now();
                stats.dll_insert_time.fetch_add(std::chrono::duration<double>(dll_insert_end - dll_insert_start).count());
                stats.dll_operations.fetch_add(1);
                
                auto bucket_update_end = std::chrono::high_resolution_clock::now();
                stats.bucket_update_time.fetch_add(std::chrono::duration<double>(bucket_update_end - bucket_update_start).count());
                stats.total_relaxations.fetch_add(1);
            }
        };

        // Strictest request optimization with timing
        auto add_request = [&] (std::vector<Request> &requests, std::unordered_map<int, int> &positions, std::mutex &mtx, const Request &request, std::atomic<double> &lock_time_stat, std::atomic<int> &lock_count_stat) {
            auto lock_start = std::chrono::high_resolution_clock::now();
            std::lock_guard<std::mutex> lk(mtx);
            auto lock_end = std::chrono::high_resolution_clock::now();
            lock_time_stat.fetch_add(std::chrono::duration<double>(lock_end - lock_start).count());
            lock_count_stat.fetch_add(1);
            
            auto dedup_start = std::chrono::high_resolution_clock::now();
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
            auto dedup_end = std::chrono::high_resolution_clock::now();
            stats.request_dedup_time.fetch_add(std::chrono::duration<double>(dedup_end - dedup_start).count());
        };

        auto gen_light_request = [&] (int u) {
            auto comp_start = std::chrono::high_resolution_clock::now();
            for (const auto &[v, w] : light[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(Rl, Rl_map, Rl_lock, Request{u, v, w}, stats.rl_lock_wait_time, stats.rl_lock_acquisitions);
                }
            }
            auto comp_end = std::chrono::high_resolution_clock::now();
            stats.light_edge_computation_time.fetch_add(std::chrono::duration<double>(comp_end - comp_start).count());
        };

        auto gen_heavy_request = [&] (int u) {
            auto comp_start = std::chrono::high_resolution_clock::now();
            for (const auto &[v, w] : heavy[u]) {
                if (dist[u] + w < dist[v]) {
                    add_request(Rh, Rh_map, Rh_lock, Request{u, v, w}, stats.rh_lock_wait_time, stats.rh_lock_acquisitions);
                }
            }
            auto comp_end = std::chrono::high_resolution_clock::now();
            stats.heavy_edge_computation_time.fetch_add(std::chrono::duration<double>(comp_end - comp_start).count());
        };

        FlexiblePool<moodycamel::BlockingConcurrentQueue> pool(num_threads);
        
        auto start_bucket_processing = std::chrono::high_resolution_clock::now();

        // bucket type is either linked list or vector
        for (int i = 0; i < (int)buckets.size(); ++i) {
            int bucket_iterations = 0;
            while (!buckets[i].empty()) {
                bucket_iterations++;
                Rl.clear();
                Rl_map.clear();

                // Loop 1: request generation
                auto dll_list_start = std::chrono::high_resolution_clock::now();
                std::vector<int> curr_bucket = buckets[i].list_all_and_clear();
                auto dll_list_end = std::chrono::high_resolution_clock::now();
                stats.dll_list_all_time.fetch_add(std::chrono::duration<double>(dll_list_end - dll_list_start).count());
                stats.dll_operations.fetch_add(1);
                
                auto pool_start_time = std::chrono::high_resolution_clock::now();
                pool.start();
                auto pool_start_end = std::chrono::high_resolution_clock::now();
                stats.pool_start_time.fetch_add(std::chrono::duration<double>(pool_start_end - pool_start_time).count());
                
                for (int u : curr_bucket) {
                    pool.push(gen_light_request, u);
                    pool.push(gen_heavy_request, u);
                }
                
                auto pool_reset_start = std::chrono::high_resolution_clock::now();
                pool.reset(); // equivalent to join()
                auto pool_reset_end = std::chrono::high_resolution_clock::now();
                stats.pool_reset_time.fetch_add(std::chrono::duration<double>(pool_reset_end - pool_reset_start).count());

                // Loop 2: relax light edges
                auto pool_start_time2 = std::chrono::high_resolution_clock::now();
                pool.start();
                auto pool_start_end2 = std::chrono::high_resolution_clock::now();
                stats.pool_start_time.fetch_add(std::chrono::duration<double>(pool_start_end2 - pool_start_time2).count());
                
                for (const Request &request : Rl) {
                    pool.push(relax, request);
                }
                
                auto pool_reset_start2 = std::chrono::high_resolution_clock::now();
                pool.reset();
                auto pool_reset_end2 = std::chrono::high_resolution_clock::now();
                stats.pool_reset_time.fetch_add(std::chrono::duration<double>(pool_reset_end2 - pool_reset_start2).count());
                
                stats.total_light_requests.fetch_add(Rl.size());
            }
            stats.max_bucket_iterations.store(std::max(stats.max_bucket_iterations.load(), bucket_iterations));
            stats.total_buckets_processed.fetch_add(1);
            
            // Loop 3: relax heavy edges
            auto pool_start_time3 = std::chrono::high_resolution_clock::now();
            pool.start();
            auto pool_start_end3 = std::chrono::high_resolution_clock::now();
            stats.pool_start_time.fetch_add(std::chrono::duration<double>(pool_start_end3 - pool_start_time3).count());
            
            for (const Request &request : Rh) {
                pool.push(relax, request);    
            }
            
            auto pool_reset_start3 = std::chrono::high_resolution_clock::now();
            pool.reset();
            auto pool_reset_end3 = std::chrono::high_resolution_clock::now();
            stats.pool_reset_time.fetch_add(std::chrono::duration<double>(pool_reset_end3 - pool_reset_start3).count());
            
            stats.total_heavy_requests.fetch_add(Rh.size());
            
            Rh.clear();
            Rh_map.clear();
        }
        
        auto end_bucket_processing = std::chrono::high_resolution_clock::now();
        stats.bucket_processing_time.store(std::chrono::duration<double>(end_bucket_processing - start_bucket_processing).count());
        
        pool.stop();

        // Print detailed performance analysis
        stats.print_stats();

        return dist;
    }
private:
    double delta;
    int num_threads;
};

#endif