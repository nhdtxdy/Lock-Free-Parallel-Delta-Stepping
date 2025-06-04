#ifndef CORRECTNESS_CHECKER_H
#define CORRECTNESS_CHECKER_H

#include <random>
#include <chrono>
#include <iomanip>
#include <cmath>
#include <set>

// Generate a random graph with n vertices and m edges with uniform random weights in [0, 1)
Graph generate_random_graph(int n, int m, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> vertex_dist(0, n - 1);
    std::uniform_real_distribution<double> weight_dist(0.0, 1.0);
    
    std::vector<Edge> edges;
    std::set<std::pair<int, int>> edge_set; // To avoid duplicate edges
    
    int attempts = 0;
    while (edges.size() < m && attempts < m * 100) { // Prevent infinite loop
        int u = vertex_dist(gen);
        int v = vertex_dist(gen);
        if (u != v && edge_set.find({u, v}) == edge_set.end()) {
            double w = weight_dist(gen);
            edges.push_back({u, v, w});
            edge_set.insert({u, v});
        }
        attempts++;
    }
    
    return Graph(n, edges);
}

// Generate a complete graph with n vertices and uniform random weights in [0, 1)
Graph generate_complete_graph(int n, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> weight_dist(0.0, 1.0);
    
    std::vector<Edge> edges;
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            if (u != v) {
                double w = weight_dist(gen);
                edges.push_back({u, v, w});
            }
        }
    }
    
    return Graph(n, edges);
}

// Check if two distance vectors are approximately equal
bool are_distances_equal(const std::vector<double>& dist1, const std::vector<double>& dist2, double epsilon = 1e-9) {
    if (dist1.size() != dist2.size()) return false;
    
    for (size_t i = 0; i < dist1.size(); i++) {
        if (std::isinf(dist1[i]) && std::isinf(dist2[i])) continue;
        if (std::isinf(dist1[i]) || std::isinf(dist2[i])) return false;
        if (std::abs(dist1[i] - dist2[i]) > epsilon) return false;
    }
    return true;
}

// Run correctness test for a single graph
bool test_graph(const Graph& graph, int source, double delta, bool verbose = false) {
    Dijkstra dijkstra;
    DeltaSteppingSequential delta_stepping(delta);
    
    auto start_dijkstra = std::chrono::high_resolution_clock::now();
    std::vector<double> dijkstra_distances = dijkstra.compute(graph, source);
    auto end_dijkstra = std::chrono::high_resolution_clock::now();
    
    auto start_delta = std::chrono::high_resolution_clock::now();
    std::vector<double> delta_distances = delta_stepping.compute(graph, source);
    auto end_delta = std::chrono::high_resolution_clock::now();
    
    bool correct = are_distances_equal(dijkstra_distances, delta_distances);
    
    if (verbose || !correct) {
        auto dijkstra_time = std::chrono::duration_cast<std::chrono::microseconds>(end_dijkstra - start_dijkstra);
        auto delta_time = std::chrono::duration_cast<std::chrono::microseconds>(end_delta - start_delta);
        
        std::cout << "Graph size: " << graph.size() << ", Source: " << source << ", Delta: " << delta << std::endl;
        std::cout << "Dijkstra time: " << dijkstra_time.count() << " μs" << std::endl;
        std::cout << "Delta stepping time: " << delta_time.count() << " μs" << std::endl;
        std::cout << "Correctness: " << (correct ? "PASS" : "FAIL") << std::endl;
        
        if (!correct) {
            std::cout << "Distance comparison (first 10 vertices):" << std::endl;
            std::cout << std::setw(8) << "Vertex" << std::setw(15) << "Dijkstra" << std::setw(15) << "Delta Step" << std::setw(15) << "Difference" << std::endl;
            for (int i = 0; i < std::min(10, (int)dijkstra_distances.size()); i++) {
                double diff = std::abs(dijkstra_distances[i] - delta_distances[i]);
                std::cout << std::setw(8) << i 
                         << std::setw(15) << std::fixed << std::setprecision(6) << dijkstra_distances[i]
                         << std::setw(15) << std::fixed << std::setprecision(6) << delta_distances[i]
                         << std::setw(15) << std::scientific << std::setprecision(2) << diff << std::endl;
            }
        }
        std::cout << std::endl;
    }
    
    return correct;
}

void run_correctness_tests() {
    std::cout << "=== Delta Stepping Sequential Correctness Tests ===" << std::endl << std::endl;
    
    int total_tests = 0;
    int passed_tests = 0;
    
    // Test 1: Small complete graphs with different delta values
    std::cout << "Test 1: Small complete graphs" << std::endl;
    for (int n = 3; n <= 8; n++) {
        Graph graph = generate_complete_graph(n, 42 + n);
        std::vector<double> deltas = {0.1, 0.2, 0.5, 1.0};
        
        for (double delta : deltas) {
            for (int source = 0; source < n; source++) {
                total_tests++;
                if (test_graph(graph, source, delta)) {
                    passed_tests++;
                }
            }
        }
    }
    
    // Test 2: Random sparse graphs
    std::cout << "Test 2: Random sparse graphs" << std::endl;
    for (int test = 0; test < 20; test++) {
        int n = 10 + (test % 20); // 10 to 29 vertices
        int m = n + (test % (2 * n)); // n to 3n edges
        Graph graph = generate_random_graph(n, m, 100 + test);
        
        std::vector<double> deltas = {0.1, 0.3, 0.7};
        for (double delta : deltas) {
            int source = test % n;
            total_tests++;
            if (test_graph(graph, source, delta)) {
                passed_tests++;
            }
        }
    }
    
    // Test 3: Random dense graphs
    std::cout << "Test 3: Random dense graphs" << std::endl;
    for (int test = 0; test < 10; test++) {
        int n = 5 + (test % 10); // 5 to 14 vertices
        int m = n * (n - 1) / 2; // Nearly complete
        Graph graph = generate_random_graph(n, m, 200 + test);
        
        std::vector<double> deltas = {0.05, 0.2, 0.5};
        for (double delta : deltas) {
            int source = 0;
            total_tests++;
            if (test_graph(graph, source, delta)) {
                passed_tests++;
            }
        }
    }
    
    std::cout << "Test 4: Edge cases" << std::endl;
    
    Graph single_vertex(1, {});
    total_tests++;
    if (test_graph(single_vertex, 0, 0.1)) {
        passed_tests++;
    }
    
    // Two disconnected vertices
    Graph disconnected(2, {});
    total_tests++;
    if (test_graph(disconnected, 0, 0.1)) {
        passed_tests++;
    }
    
    // Path graph
    std::vector<Edge> path_edges = {{0, 1, 0.3}, {1, 2, 0.7}, {2, 3, 0.2}};
    Graph path_graph(4, path_edges);
    std::vector<double> deltas = {0.1, 0.4, 0.8};
    for (double delta : deltas) {
        for (int source = 0; source < 4; source++) {
            total_tests++;
            if (test_graph(path_graph, source, delta)) {
                passed_tests++;
            }
        }
    }
    
    // Test 5: Stress test with larger graphs
    std::cout << "Test 5: Stress test" << std::endl;
    for (int test = 0; test < 5; test++) {
        int n = 50 + test * 20; // 50 to 130 vertices
        int m = n * 2; // Sparse
        Graph graph = generate_random_graph(n, m, 300 + test);
        
        double delta = 0.1 + test * 0.1;
        int source = test % n;
        total_tests++;
        if (test_graph(graph, source, delta, true)) { // Verbose for stress tests
            passed_tests++;
        }
    }
    
    // Summary
    std::cout << "=== Test Summary ===" << std::endl;
    std::cout << "Total tests: " << total_tests << std::endl;
    std::cout << "Passed: " << passed_tests << std::endl;
    std::cout << "Failed: " << (total_tests - passed_tests) << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
              << (100.0 * passed_tests / total_tests) << "%" << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << std::endl << "🎉 All tests passed! Your delta stepping implementation appears to be correct." << std::endl;
    } else {
        std::cout << std::endl << "❌ Some tests failed. Please check the implementation." << std::endl;
    }
}

#endif