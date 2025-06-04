#include <iostream>
#include <fstream>
#include <unordered_map>
#include "dijkstra.h"
#include "delta_stepping_sequential.h"

Graph parse_graph_from_file(std::string filename, bool normalize_weights = false) {
    std::ifstream in(filename);
    int u, v;
    double w;
    std::vector<Edge> edges;
    std::unordered_map<int, int> index_map;
    int cnt = 0;
    double max_w = 0.;
    while (in >> u >> v >> w) {
        if (!index_map.count(u)) {
            index_map[u] = cnt++;
        }
        if (!index_map.count(v)) {
            index_map[v] = cnt++;
        }
        max_w = std::max(max_w, w);

        edges.push_back({index_map[u], index_map[v], w});
    }

    if (normalize_weights) {
        for (Edge &edge : edges) {
            edge.w /= max_w;
        }
    }

    return Graph(cnt, edges);
}

int main() {
    return 0;
}