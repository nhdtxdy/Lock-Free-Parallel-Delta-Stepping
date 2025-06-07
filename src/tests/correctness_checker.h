#ifndef CORRECTNESS_CHECKER_H
#define CORRECTNESS_CHECKER_H

#include <chrono>
#include <iomanip>
#include <cmath>
#include <fstream>
#include <cstdlib>
#include <memory>
#include <random>
#include "graph_utils.h"
#include "algos.h"
#include "queues/queues.h"


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

// Run correctness test for a single graph with a list of solvers
bool test_graph_with_solvers(const Graph& graph, int source, const std::vector<std::unique_ptr<ShortestPathSolverBase>>& solvers, bool verbose = false) {
    if (solvers.empty()) {
        std::cout << "Error: No solvers provided for testing" << std::endl;
        return false;
    }
    
    // Store results from all solvers
    std::vector<std::vector<double>> all_distances(solvers.size());
    std::vector<std::chrono::duration<double, std::micro>> all_times(solvers.size());
    std::vector<std::string> solver_names(solvers.size());
    
    // Run all solvers
    for (size_t i = 0; i < solvers.size(); i++) {
        std::cerr << "Running " << solvers[i]->name() << '\n';
        auto start = std::chrono::high_resolution_clock::now();
        all_distances[i] = solvers[i]->compute(graph, source);
        auto end = std::chrono::high_resolution_clock::now();
        all_times[i] = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
        solver_names[i] = solvers[i]->name();
    }
    
    // Compare all results against the first solver (typically Dijkstra)
    bool all_correct = true;
    for (size_t i = 1; i < solvers.size(); i++) {
        bool is_correct = are_distances_equal(all_distances[0], all_distances[i]);
        if (!is_correct) {
            all_correct = false;
            
            // Save the failing graph and exit immediately
            save_graph_to_file(graph, "failed_graph_multi_solver.txt");
            std::cout << "=== FAILED MULTI-SOLVER TEST DETECTED ===" << std::endl;
            std::cout << "Graph size: " << graph.size() << ", Source: " << source << std::endl;
            std::cout << "Mismatch between " << solver_names[0] << " and " << solver_names[i] << std::endl;
            
            // Determine relaxation type for the failing solver
            int under_relaxed_count = 0;
            int over_relaxed_count = 0;
            
            for (size_t v = 0; v < all_distances[0].size(); v++) {
                if (!std::isinf(all_distances[0][v]) && !std::isinf(all_distances[i][v])) {
                    if (all_distances[i][v] < all_distances[0][v]) {
                        under_relaxed_count++;
                    } else if (all_distances[i][v] > all_distances[0][v]) {
                        over_relaxed_count++;
                    }
                }
            }
            
            if (under_relaxed_count > over_relaxed_count) {
                std::cout << solver_names[i] << " is predominantly UNDER RELAXED (producing smaller distances)" << std::endl;
            } else if (over_relaxed_count > under_relaxed_count) {
                std::cout << solver_names[i] << " is predominantly OVER RELAXED (producing larger distances)" << std::endl;
            } else {
                std::cout << solver_names[i] << " has mixed relaxation errors" << std::endl;
            }
            std::cout << "Under relaxed vertices: " << under_relaxed_count << ", Over relaxed vertices: " << over_relaxed_count << std::endl;
            
            for (size_t j = 0; j < solvers.size(); j++) {
                std::cout << solver_names[j] << " time: " << all_times[j].count() << " μs" << std::endl;
            }
            
            std::cout << "Distance comparison (vertices with errors only):" << std::endl;
            std::cout << std::setw(8) << "Vertex";
            for (const auto& name : solver_names) {
                std::cout << std::setw(15) << name.substr(0, 14);
            }
            std::cout << std::setw(15) << "Max Diff" << std::setw(15) << "Relax Type" << std::endl;
            
            double max_diff = 0;
            int errors_shown = 0;
            const int max_errors_to_show = 20; // Limit output to avoid overwhelming
            
            for (int v = 0; v < (int)all_distances[0].size() && errors_shown < max_errors_to_show; v++) {
                // Calculate max difference for this vertex
                double vertex_max_diff = 0;
                std::string relax_type = "";
                for (size_t j = 1; j < solvers.size(); j++) {
                    double diff = std::abs(all_distances[0][v] - all_distances[j][v]);
                    vertex_max_diff = std::max(vertex_max_diff, diff);
                    
                    // Check relaxation type for any solver with differences
                    if (diff > 1e-9) {
                        if (all_distances[j][v] < all_distances[0][v]) {
                            relax_type = "UNDER";
                        } else if (all_distances[j][v] > all_distances[0][v]) {
                            relax_type = "OVER";
                        }
                    }
                }
                
                // Only print vertices where there are actual differences
                if (vertex_max_diff > 1e-9) {
                    std::cout << std::setw(8) << v;
                    for (size_t j = 0; j < solvers.size(); j++) {
                        std::cout << std::setw(15) << std::fixed << std::setprecision(6) << all_distances[j][v];
                    }
                    std::cout << std::setw(15) << std::scientific << std::setprecision(2) << vertex_max_diff;
                    std::cout << std::setw(15) << relax_type << std::endl;
                    errors_shown++;
                }
                
                max_diff = std::max(max_diff, vertex_max_diff);
            }
            
            if (errors_shown == max_errors_to_show) {
                std::cout << "  ... (showing first " << max_errors_to_show << " errors only)" << std::endl;
            }
            std::cout << "Total vertices with errors: " << errors_shown << " out of " << all_distances[0].size() << std::endl;
            
            // Find max difference across all vertices
            for (int v = 10; v < (int)all_distances[0].size(); v++) {
                for (size_t j = 1; j < solvers.size(); j++) {
                    double diff = std::abs(all_distances[0][v] - all_distances[j][v]);
                    max_diff = std::max(max_diff, diff);
                }
            }
            std::cout << "\nLargest difference across all vertices: " << std::scientific << std::setprecision(2) << max_diff << std::endl;
            std::cout << "\nMulti-solver test execution stopped at first failure." << std::endl;
            exit(1);
        }
    }
    
    if (verbose) {
        std::cout << "Graph size: " << graph.size() << ", Source: " << source << std::endl;
        for (size_t i = 0; i < solvers.size(); i++) {
            std::cout << solver_names[i] << " time: " << all_times[i].count() << " μs" << std::endl;
        }
        std::cout << "All solvers: PASS" << std::endl << std::endl;
    }
    
    return all_correct;
}

// Run correctness test for a single graph with parallel implementation
bool test_graph_parallel(const Graph& graph, int source, double delta, int num_threads = 4, bool verbose = false) {
    // Create solvers using smart pointers
    std::vector<std::unique_ptr<ShortestPathSolverBase>> solvers;
    solvers.push_back(std::make_unique<Dijkstra>());

    solvers.push_back(std::make_unique<DeltaSteppingSequential>(delta));
    
    solvers.push_back(std::make_unique<DeltaSteppingParallel>(delta, num_threads));
    solvers.push_back(std::make_unique<DSPRecycleBucket>(delta, num_threads));
    // solvers.push_back(std::make_unique<DeltaSteppingOpenMP>(delta, num_threads));
    // solvers.push_back(std::make_unique<DeltaSteppingDynamic>(delta, num_threads));
    // solvers.push_back(std::make_unique<DeltaSteppingStatic>(delta, num_threads));
    
    // solvers.push_back(std::make_unique<DeltaSteppingParallelProfiled>(delta, num_threads));

    return test_graph_with_solvers(graph, source, solvers, verbose);
}

void run_parallel_correctness_tests() {
    std::cout << "=== Delta Stepping Parallel Correctness Tests ===" << std::endl << std::endl;
    
    // Initialize random number generator with current time
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> seed_dist(1, 100000);
    
    std::cout << "Using random seeds for test reproducibility" << std::endl << std::endl;
    
    int total_tests = 0;
    int passed_tests = 0;
    int current_test = 0;
    
    std::vector<int> thread_counts = {1, 4, 8};
    
    // First, calculate the total number of tests
    int total_estimated = 0;
    // Test 1: Small complete graphs
    for (int n = 3; n <= 6; n++) {
        total_estimated += 4 * 4 * n; // 4 deltas * 4 thread counts * n sources
    }
    // Test 2: Random sparse graphs
    total_estimated += 10 * 3 * 4; // 10 graphs * 3 deltas * 4 thread counts
    // Test 3: Edge cases
    total_estimated += (1 + 1 + 3 * 4) * 4; // (single + disconnected + path) * 4 thread counts
    // Test 4: Stress test
    total_estimated += 3 * 4; // 3 stress tests * 4 thread counts
    
    std::cout << "Total estimated parallel tests: " << total_estimated << std::endl << std::endl;
    
    // Test 1: Small complete graphs with different delta values and thread counts
    std::cout << "Test 1: Small complete graphs with parallel implementation" << std::endl;
    for (int n = 3; n <= 6; n++) {
        int random_seed = seed_dist(gen);
        Graph graph = generate_complete_graph(n, 0.0, 1.0, true, WeightDistribution::UNIFORM, random_seed);
        std::cout << "  Complete graph n=" << n << " using seed: " << random_seed << std::endl;
        std::vector<double> deltas = {0.01, 0.09, 0.18};
        
        for (double delta : deltas) {
            for (int threads : thread_counts) {
                for (int source = 0; source < n; source++) {
                    current_test++;
                    total_tests++;
                    std::cout << "  Running test " << current_test << "/" << total_estimated 
                             << " (Complete graph n=" << n << ", delta=" << delta << ", threads=" << threads << ", source=" << source << ")";
                    if (test_graph_parallel(graph, source, delta, threads)) {
                        passed_tests++;
                        std::cout << " - PASS" << std::endl;
                    } else {
                        std::cout << " - FAIL" << std::endl;
                    }
                }
            }
        }
    }
    std::cout << "Test 1 completed: " << (current_test - (total_tests - passed_tests)) << "/" << current_test << " passed" << std::endl << std::endl;
    
    // Test 2: Random sparse graphs
    std::cout << "Test 2: Random sparse graphs with parallel implementation" << std::endl;
    int test2_start = current_test;
    for (int test = 0; test < 20; test++) {
        int n = 2000; // 10 to 24 vertices
        int m = 6000; // n to 3n edges
        int random_seed = seed_dist(gen);
        Graph graph = generate_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::UNIFORM, random_seed);
        std::cout << "  Sparse graph " << (test+1) << "/10 (n=" << graph.size() << ", m=" << m << ") using seed: " << random_seed << std::endl;
        
        std::vector<double> deltas = {0.02, 0.05, 0.15};
        for (double delta : deltas) {
            for (int threads : thread_counts) {
                current_test++;
                total_tests++;
                int source = test % graph.size();
                std::cout << "  Running test " << current_test << "/" << total_estimated 
                         << " (Sparse graph " << (test+1) << "/10, n=" << graph.size() << ", delta=" << delta << ", threads=" << threads << ")";
                if (test_graph_parallel(graph, source, delta, threads)) {
                    passed_tests++;
                    std::cout << " - PASS" << std::endl;
                } else {
                    std::cout << " - FAIL" << std::endl;
                }
            }
        }
    }
    std::cout << "Test 2 completed: " << (current_test - test2_start - (total_tests - passed_tests - (current_test - test2_start))) << "/" << (current_test - test2_start) << " passed" << std::endl << std::endl;
    
    // Test 3: Edge cases
    std::cout << "Test 3: Edge cases with parallel implementation" << std::endl;
    int test3_start = current_test;
    
    // Single vertex
    Graph single_vertex(1, {});
    for (int threads : thread_counts) {
        current_test++;
        total_tests++;
        std::cout << "  Running test " << current_test << "/" << total_estimated 
                 << " (Single vertex, threads=" << threads << ")";
        if (test_graph_parallel(single_vertex, 0, 0.1, threads)) {
            passed_tests++;
            std::cout << " - PASS" << std::endl;
        } else {
            std::cout << " - FAIL" << std::endl;
        }
    }
    
    // Two disconnected vertices
    Graph disconnected(2, {});
    for (int threads : thread_counts) {
        current_test++;
        total_tests++;
        std::cout << "  Running test " << current_test << "/" << total_estimated 
                 << " (Disconnected vertices, threads=" << threads << ")";
        if (test_graph_parallel(disconnected, 0, 0.1, threads)) {
            passed_tests++;
            std::cout << " - PASS" << std::endl;
        } else {
            std::cout << " - FAIL" << std::endl;
        }
    }
    
    // Path graph
    std::vector<Edge> path_edges = {{0, 1, 0.3}, {1, 2, 0.7}, {2, 3, 0.2}};
    Graph path_graph(4, path_edges);
    std::vector<double> deltas = {0.02, 0.1, 0.2};
    for (double delta : deltas) {
        for (int threads : thread_counts) {
            for (int source = 0; source < 4; source++) {
                current_test++;
                total_tests++;
                std::cout << "  Running test " << current_test << "/" << total_estimated 
                         << " (Path graph, delta=" << delta << ", threads=" << threads << ", source=" << source << ")";
                if (test_graph_parallel(path_graph, source, delta, threads)) {
                    passed_tests++;
                    std::cout << " - PASS" << std::endl;
                } else {
                    std::cout << " - FAIL" << std::endl;
                }
            }
        }
    }
    std::cout << "Test 3 completed: " << (current_test - test3_start - (total_tests - passed_tests - (current_test - test3_start))) << "/" << (current_test - test3_start) << " passed" << std::endl << std::endl;
    
    // Test 4: Stress test with larger graphs
    std::cout << "Test 4: Stress test with parallel implementation" << std::endl;
    int test4_start = current_test;
    for (int test = 0; test < 3; test++) {
        int n = 3 + test * 15; // 30 to 60 vertices
        int m = n * 3; // Sparse
        int random_seed = seed_dist(gen);
        Graph graph = generate_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::UNIFORM, random_seed);
        std::cout << "  Stress test " << (test+1) << "/3 (n=" << graph.size() << ", m=" << m << ") using seed: " << random_seed << std::endl;
        
        double delta = 0.02 + test * 0.02;
        for (int threads : thread_counts) {
            current_test++;
            total_tests++;
            int source = test % graph.size();
            std::cout << "  Running test " << current_test << "/" << total_estimated 
                     << " (Stress test " << (test+1) << "/3, n=" << graph.size() << ", delta=" << delta << ", threads=" << threads << ")";
            if (test_graph_parallel(graph, source, delta, threads, true)) { // Verbose for stress tests
                passed_tests++;
                std::cout << " - PASS" << std::endl;
            } else {
                std::cout << " - FAIL" << std::endl;
            }
        }
    }
    std::cout << "Test 4 completed: " << (current_test - test4_start - (total_tests - passed_tests - (current_test - test4_start))) << "/" << (current_test - test4_start) << " passed" << std::endl << std::endl;
    
    // Summary
    std::cout << "=== Parallel Test Summary ===" << std::endl;
    std::cout << "Total tests: " << total_tests << std::endl;
    std::cout << "Passed: " << passed_tests << std::endl;
    std::cout << "Failed: " << (total_tests - passed_tests) << std::endl;
    std::cout << "Success rate: " << std::fixed << std::setprecision(2) 
              << (100.0 * passed_tests / total_tests) << "%" << std::endl;
    
    if (passed_tests == total_tests) {
        std::cout << std::endl << "🎉 All parallel tests passed! Your delta stepping parallel implementation appears to be correct." << std::endl;
    } else {
        std::cout << std::endl << "❌ Some parallel tests failed. Please check the implementation." << std::endl;
    }
}

// Combined test runner that runs both sequential and parallel tests
void run_all_correctness_tests() {
    run_parallel_correctness_tests();
}

#endif