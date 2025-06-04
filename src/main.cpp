#include <iostream>
#include <fstream>
#include <unordered_map>
#include "graph.h"
#include "dijkstra.h"
#include "delta_stepping_sequential.h"
#include "correctness_checker.h"
#include "performance_test.h"

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
    std::cout << "Parallel Shortest Paths - Algorithm Testing" << std::endl;
    std::cout << "===========================================" << std::endl;
    std::cout << "Choose test type:" << std::endl;
    std::cout << "1. Correctness Tests (quick)" << std::endl;
    std::cout << "2. Performance Tests (large graphs, several seconds)" << std::endl;
    std::cout << "3. Both" << std::endl;
    std::cout << "Enter choice (1-3): ";
    
    int choice;
    std::cin >> choice;
    
    switch (choice) {
        case 1:
            run_correctness_tests();
            break;
        case 2:
            run_large_graph_tests();
            break;
        case 3:
            run_correctness_tests();
            std::cout << "\n" << std::endl;
            run_large_graph_tests();
            break;
        default:
            std::cout << "Invalid choice. Running correctness tests by default." << std::endl;
            run_correctness_tests();
            break;
    }
    
    return 0;
}