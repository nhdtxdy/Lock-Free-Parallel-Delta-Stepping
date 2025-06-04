#ifndef PERFORMANCE_TEST_H
#define PERFORMANCE_TEST_H

#include <iostream>
#include <chrono>
#include <iomanip>
#include "graph_generators.h"
#include "dijkstra.h"
#include "delta_stepping_sequential.h"
#include "correctness_checker.h"

// Run performance benchmark
void run_performance_benchmark(const Graph& graph, int source, const std::string& graph_name) {
    std::cout << "\n=== Performance Benchmark: " << graph_name << " ===" << std::endl;
    std::cout << "Graph size: " << graph.size() << " vertices" << std::endl;
    std::cout << "Source vertex: " << source << std::endl;
    
    // Test Dijkstra
    {
        Dijkstra dijkstra;
        std::cout << "\nRunning Dijkstra's algorithm..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<double> distances_dijkstra = dijkstra.compute(graph, source);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Dijkstra time: " << duration.count() << " ms" << std::endl;
        
        // Count reachable vertices
        int reachable = 0;
        for (double d : distances_dijkstra) {
            if (!std::isinf(d)) reachable++;
        }
        std::cout << "Reachable vertices: " << reachable << "/" << graph.size() << std::endl;
    }
    
    // Test Delta Stepping Sequential with different delta values
    std::vector<double> deltas = {0.01, 0.05, 0.1, 0.2};
    for (double delta : deltas) {
        DeltaSteppingSequential delta_stepping(delta);
        std::cout << "\nRunning Delta Stepping Sequential (δ=" << delta << ")..." << std::endl;
        
        auto start = std::chrono::high_resolution_clock::now();
        std::vector<double> distances = delta_stepping.compute(graph, source);
        auto end = std::chrono::high_resolution_clock::now();
        
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Delta Stepping Sequential time: " << duration.count() << " ms" << std::endl;
    }
}

void run_large_graph_tests() {
    std::cout << "=== Large Graph Performance Tests ===" << std::endl;
    std::cout << "These tests will take several seconds to complete..." << std::endl;
    
    // Test 1: Large random sparse graph
    {
        std::cout << "\n--- Test 1: Large Random Sparse Graph ---" << std::endl;
        int n_requested = 1e5;
        int m_requested = 1e7; // Average degree ~6
        Graph graph = generate_large_random_graph(n_requested, m_requested, 0., 1.0, true, 12345);
        int n_actual = graph.size();
        int m_actual = 0;
        for (int u = 0; u < n_actual; u++) {
            m_actual += graph[u].size();
        }
        std::cout << "Generated graph with " << n_actual << " vertices and " << m_actual << " edges ";
        std::cout << "(requested " << n_requested << " vertices, " << m_requested << " edges)" << std::endl;
        run_performance_benchmark(graph, 0, "Large Random Sparse");
    }
    
    // Test 2: Large random dense graph
    {
        std::cout << "\n--- Test 2: Large Random Dense Graph ---" << std::endl;
        int n_requested = 5e5;
        int m_requested = 5e8; // Average degree ~50
        Graph graph = generate_large_random_graph(n_requested, m_requested, 0., 1.0, true, 23456);
        int n_actual = graph.size();
        int m_actual = 0;
        for (int u = 0; u < n_actual; u++) {
            m_actual += graph[u].size();
        }
        std::cout << "Generated graph with " << n_actual << " vertices and " << m_actual << " edges ";
        std::cout << "(requested " << n_requested << " vertices, " << m_requested << " edges)" << std::endl;
        run_performance_benchmark(graph, 0, "Large Random Dense");
    }
    
    // Test 3: Scale-free graph (realistic network topology)
    {
        std::cout << "\n--- Test 3: Scale-Free Graph ---" << std::endl;
        int n_requested = 3000;
        int m = 8; // Edges per new vertex
        Graph graph = generate_scale_free_graph(n_requested, m, 2.5, 0., 1.0, true, 34567);
        int n_actual = graph.size();
        int m_actual = 0;
        for (int u = 0; u < n_actual; u++) {
            m_actual += graph[u].size();
        }
        std::cout << "Generated scale-free graph with " << n_actual << " vertices and " << m_actual << " edges ";
        std::cout << "(requested " << n_requested << " vertices)" << std::endl;
        run_performance_benchmark(graph, 0, "Scale-Free Network");
    }
    
    // Test 4: Very large sparse graph
    {
        std::cout << "\n--- Test 4: Very Large Sparse Graph ---" << std::endl;
        int n_requested = 10000;
        int m_requested = 20000; // Very sparse
        Graph graph = generate_large_random_graph(n_requested, m_requested, 0., 1.0, true, 45678);
        int n_actual = graph.size();
        int m_actual = 0;
        for (int u = 0; u < n_actual; u++) {
            m_actual += graph[u].size();
        }
        std::cout << "Generated graph with " << n_actual << " vertices and " << m_actual << " edges ";
        std::cout << "(requested " << n_requested << " vertices, " << m_requested << " edges)" << std::endl;
        run_performance_benchmark(graph, 0, "Very Large Sparse");
    }
    
    std::cout << "\n=== Performance Tests Completed ===" << std::endl;
}

#endif 