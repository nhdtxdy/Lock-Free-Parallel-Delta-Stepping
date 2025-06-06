#ifndef DELTA_STEPPING_OPENMP_H
#define DELTA_STEPPING_OPENMP_H

#include "shortest_path_solver_base.h"
#include <limits>
#include <unordered_set>
#include <omp.h>
#include "lists/fine_grained_dll.h"
#include <mutex>

class DeltaSteppingOpenMP : public ShortestPathSolverBase {
public:
    using Request = Edge;

    const std::string name() const override {
        return "Naive parallel delta stepping using OpenMP";
    }

    DeltaSteppingOpenMP(double delta, int num_threads): delta(delta), num_threads(num_threads) {}

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
        std::mutex buckets_resize_mutex;
        buckets[0].insert(idx_to_addr[source]);
        dist[source] = 0;
        std::vector<Request> Rl, Rh;
        std::mutex Rl_lock, Rh_lock;
        std::unordered_map<int, int> Rl_map, Rh_map;

        auto get_bucket = [&] (int v) {
            if (dist[v] == INF_MAX) {
                return -1;
            }
            return int(dist[v] / delta);
        };
        
        auto relax = [&] (const Request &request) {
            int u = request.u;
            int v = request.v;
            double w = request.w;
            if (dist[u] + w < dist[v]) {
                int old_bucket, new_bucket;
                {
                    old_bucket = get_bucket(v);
                    dist[v] = dist[u] + w;
                    new_bucket = get_bucket(v);
                }
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

        // Set number of OpenMP threads
        omp_set_num_threads(num_threads);

        for (int i = 0; i < (int)buckets.size(); ++i) {
            while (!buckets[i].empty()) {
                Rl.clear();
                Rl_map.clear();

                // Loop 1: request generation
                std::vector<int> curr_bucket = buckets[i].list_all_and_clear();
                
                // OpenMP parallel for with dynamic scheduling
                #pragma omp parallel for
                for (int idx = 0; idx < (int)curr_bucket.size(); ++idx) {
                    int u = curr_bucket[idx];
                    gen_light_request(u);
                    gen_heavy_request(u);
                }

                // Loop 2: relax light edges
                #pragma omp parallel for
                for (int idx = 0; idx < (int)Rl.size(); ++idx) {
                    const Request &request = Rl[idx];
                    relax(request);
                }
            }
            
            // Loop 3: relax heavy edges
            #pragma omp parallel for schedule(dynamic)
            for (int idx = 0; idx < (int)Rh.size(); ++idx) {
                const Request &request = Rh[idx];
                #pragma omp critical
                {
                    relax(request);
                }
            }
            
            Rh.clear();
            Rh_map.clear();
        }

        return dist;
    }
private:
    double delta;
    int num_threads;
};

#endif 