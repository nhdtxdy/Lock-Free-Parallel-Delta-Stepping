#ifndef DELTA_STEPPING_H
#define DELTA_STEPPING_H

#include "shortest_path_solver.h"
#include <limits>
#include <unordered_set>

class DeltaSteppingSequential : public ShortestPathSolver {
public:
    DeltaSteppingSequential(double delta): delta(delta) {}

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

        // FURTHER RESEARCH: more memory efficient implementation of buckets (linked list instead of vector?)
        std::vector<std::unordered_set<int>> buckets(1);
        buckets[0].insert(source);
        dist[source] = 0;

        auto get_bucket = [&] (int v) {
            if (dist[v] == INF_MAX) {
                return -1;
            }
            return int(dist[v] / delta);
        };
        
        auto relax = [&] (int u, int v, double w) {
            if (dist[u] + w < dist[v]) {
                int old_bucket = get_bucket(v);
                dist[v] = dist[u] + w;
                int new_bucket = get_bucket(v);
                if (old_bucket >= 0) {
                    buckets[old_bucket].erase(v);
                }
                if (new_bucket >= buckets.size()) {
                    buckets.resize(new_bucket + 1);
                }
                buckets[new_bucket].insert(v);
            }
        };

        for (int i = 0; i < buckets.size(); ++i) {
            std::unordered_set<int> S;
            while (!buckets[i].empty()) {
                std::vector<int> curr_bucket(buckets[i].begin(), buckets[i].end());
                buckets[i].clear();
                // we can combine light edge relaxation with request generation
                for (const int &u : curr_bucket) {
                    for (const auto &[v, w] : light[u]) {
                        relax(u, v, w);
                    }
                    S.insert(u); // strictest request optimization; change back to r_heavy if needed
                }
            }
            for (const int &u : S) {
                for (const auto &[v, w] : heavy[u]) {
                    relax(u, v, w);
                }
            }
            S.clear();
        }

        return dist;
    }
private:
    double delta;
};

#endif