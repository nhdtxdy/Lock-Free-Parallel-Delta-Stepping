#ifndef DIJKSTRA_H
#define DIJKSTRA_H

#include "shortest_path_solver.h"
#include <queue>
#include <limits>

class Dijkstra : public ShortestPathSolver {
public:
    std::vector<double> compute(const Graph &graph, int source) const override {
        std::priority_queue<std::pair<double, int>> pq;
        int n = graph.size();
        std::vector<double> dist(n, std::numeric_limits<double>::infinity());
        std::vector<bool> vis(n, false);
        dist[source] = 0;
        pq.push({0, source});
        while (!pq.empty()) {
            auto u = pq.top().second;
            pq.pop();
            if (vis[u]) continue;
            vis[u] = true;
            for (const auto &[v, w] : graph[u]) {
                if (dist[u] + w < dist[v]) {
                    dist[v] = dist[u] + w;
                    pq.push({-dist[v], v});
                }
            }
        }
        return dist;
    }
};

#endif