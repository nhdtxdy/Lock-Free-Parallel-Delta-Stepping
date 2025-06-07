#ifndef GRAPH_H
#define GRAPH_H

#include <vector>

using AdjEdge = std::pair<int, double>;

struct Edge {
    int u, v;
    double w;
};

// nodes are 0-indexed
class Graph {
public:
    Graph(int n, const std::vector<Edge> &edges) : n(n) {
        adj.resize(n);
        for (const auto &[u, v, w] : edges) {
            adj[u].push_back({v, w});
            max_L = std::max(max_L, w);
        }
    }

    double get_max_edge_weight() const {
        return max_L;
    }

    const std::vector<AdjEdge>& operator[](int idx) const {
        return adj[idx];
    }

    int size() const {
        return n;
    }
private:
    int n;
    std::vector<std::vector<AdjEdge>> adj;
    double max_L = 0.;
};

#endif