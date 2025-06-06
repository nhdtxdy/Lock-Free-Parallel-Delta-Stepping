#include <iostream>
#include <chrono>
#include <vector>
#include <random>
#include <iomanip>
#include <cassert>
#include <map>
#include "delta_stepping_parallel.h"
#include "delta_stepping_openmp.h"
#include "graph.h"
#include "graph_utils.h"

struct BenchmarkResult {
    double execution_time_ms;
    size_t num_nodes;
    size_t num_edges;
    int num_threads;
    double delta;
    std::string implementation;
};

class DeltaSteppingBenchmark {
private:
    std::vector<BenchmarkResult> results;
    
public:
    void run_benchmark(const Graph& graph, int source, double delta, 
                      int num_threads, int iterations = 5) {
        
        std::cout << "\n========================================\n";
        std::cout << "Benchmarking Graph: " << graph.size() << " nodes, ";
        size_t total_edges = 0;
        for (int i = 0; i < graph.size(); ++i) {
            const auto &adj_list = graph[i];
            total_edges += adj_list.size();
        }
        std::cout << total_edges << " edges\n";
        std::cout << "Delta: " << delta << ", Threads: " << num_threads << "\n";
        std::cout << "========================================\n";

        // Benchmark FlexiblePool implementation
        std::cout << "Testing FlexiblePool implementation...\n";
        std::vector<double> flexible_times;
        std::vector<double> flexible_result;
        
        for (int i = 0; i < iterations; ++i) {
            DeltaSteppingParallel flexible_solver(delta, num_threads);
            auto start = std::chrono::high_resolution_clock::now();
            auto result = flexible_solver.compute(graph, source);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double time_ms = duration.count() / 1000.0;
            flexible_times.push_back(time_ms);
            
            if (i == 0) flexible_result = result;
            
            std::cout << "  Iteration " << (i+1) << ": " << std::fixed 
                      << std::setprecision(3) << time_ms << " ms\n";
        }

        // Benchmark OpenMP implementation
        std::cout << "\nTesting OpenMP implementation...\n";
        std::vector<double> openmp_times;
        std::vector<double> openmp_result;
        
        for (int i = 0; i < iterations; ++i) {
            DeltaSteppingOpenMP openmp_solver(delta, num_threads);
            auto start = std::chrono::high_resolution_clock::now();
            auto result = openmp_solver.compute(graph, source);
            auto end = std::chrono::high_resolution_clock::now();
            
            auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end - start);
            double time_ms = duration.count() / 1000.0;
            openmp_times.push_back(time_ms);
            
            if (i == 0) openmp_result = result;
            
            std::cout << "  Iteration " << (i+1) << ": " << std::fixed 
                      << std::setprecision(3) << time_ms << " ms\n";
        }

        // Verify results are consistent
        verify_results(flexible_result, openmp_result);

        // Calculate statistics
        auto calc_avg = [](const std::vector<double>& times) {
            double sum = 0;
            for (double t : times) sum += t;
            return sum / times.size();
        };
        
        auto calc_min = [](const std::vector<double>& times) {
            return *std::min_element(times.begin(), times.end());
        };

        double flexible_avg = calc_avg(flexible_times);
        double flexible_min = calc_min(flexible_times);
        double openmp_avg = calc_avg(openmp_times);
        double openmp_min = calc_min(openmp_times);

        // Store results
        results.push_back({flexible_avg, (int)graph.size(), total_edges, num_threads, delta, "FlexiblePool"});
        results.push_back({openmp_avg, (int)graph.size(), total_edges, num_threads, delta, "OpenMP"});

        // Print comparison
        std::cout << "\n--- RESULTS ---\n";
        std::cout << "FlexiblePool - Avg: " << std::fixed << std::setprecision(3) 
                  << flexible_avg << " ms, Min: " << flexible_min << " ms\n";
        std::cout << "OpenMP       - Avg: " << std::fixed << std::setprecision(3) 
                  << openmp_avg << " ms, Min: " << openmp_min << " ms\n";
        
        double speedup = openmp_avg / flexible_avg;
        std::cout << "FlexiblePool speedup: " << std::fixed << std::setprecision(2) 
                  << speedup << "x ";
        if (speedup > 1.0) {
            std::cout << "(FlexiblePool is faster)\n";
        } else {
            std::cout << "(OpenMP is faster)\n";
        }
    }

    void verify_results(const std::vector<double>& result1, const std::vector<double>& result2) {
        if (result1.size() != result2.size()) {
            std::cerr << "ERROR: Result sizes don't match!\n";
            return;
        }
        
        const double EPSILON = 1e-9;
        for (size_t i = 0; i < result1.size(); ++i) {
            if (std::abs(result1[i] - result2[i]) > EPSILON) {
                std::cerr << "ERROR: Results differ at node " << i 
                          << ": " << result1[i] << " vs " << result2[i] << "\n";
                return;
            }
        }
        std::cout << "✓ Results verified - both implementations produce identical outputs\n";
    }

    void print_summary() {
        std::cout << "\n\n============ BENCHMARK SUMMARY ============\n";
        std::cout << std::left << std::setw(12) << "Impl" 
                  << std::setw(8) << "Nodes" 
                  << std::setw(10) << "Edges"
                  << std::setw(10) << "Threads"
                  << std::setw(10) << "Delta"
                  << std::setw(12) << "Time (ms)" << "\n";
        std::cout << std::string(62, '-') << "\n";
        
        for (const auto& result : results) {
            std::cout << std::left << std::setw(12) << result.implementation
                      << std::setw(8) << result.num_nodes
                      << std::setw(10) << result.num_edges
                      << std::setw(10) << result.num_threads
                      << std::setw(10) << std::fixed << std::setprecision(2) << result.delta
                      << std::setw(12) << std::fixed << std::setprecision(3) << result.execution_time_ms << "\n";
        }
        
        // Calculate average speedups by configuration
        std::map<std::tuple<size_t, int, double>, std::pair<double, double>> config_times;
        for (const auto& result : results) {
            auto key = std::make_tuple(result.num_nodes, result.num_threads, result.delta);
            if (result.implementation == "FlexiblePool") {
                config_times[key].first = result.execution_time_ms;
            } else {
                config_times[key].second = result.execution_time_ms;
            }
        }
        
        std::cout << "\n============ SPEEDUP ANALYSIS ============\n";
        std::cout << std::left << std::setw(8) << "Nodes" 
                  << std::setw(10) << "Threads"
                  << std::setw(10) << "Delta"
                  << std::setw(15) << "FlexiblePool Speedup" << "\n";
        std::cout << std::string(43, '-') << "\n";
        
        for (const auto& [config, times] : config_times) {
            auto [nodes, threads, delta] = config;
            auto [flexible_time, openmp_time] = times;
            double speedup = openmp_time / flexible_time;
            
            std::cout << std::left << std::setw(8) << nodes
                      << std::setw(10) << threads
                      << std::setw(10) << std::fixed << std::setprecision(2) << delta
                      << std::setw(15) << std::fixed << std::setprecision(3) << speedup << "x\n";
        }
    }
};

int main() {
    DeltaSteppingBenchmark benchmark;
    
    // Test configurations
    std::vector<int> node_counts = {1000, 5000};
    std::vector<int> thread_counts = {1, 2, 4, 8, 24};
    std::vector<double> delta_values = {0.01, 0.02, 0.05, 0.06};
    
    std::cout << "=== Delta Stepping: FlexiblePool vs OpenMP Benchmark ===\n";
    std::cout << "Testing various graph sizes, thread counts, and delta values...\n";

    for (int nodes : node_counts) {
        for (int threads : thread_counts) {
            for (double delta : delta_values) {
                // Generate random graph with appropriate edge count
                int edges = nodes * 7; // Average degree of 7
                auto graph = generate_random_graph(nodes, edges, 0., 1., true, 12345);
                int source = 0;
                
                benchmark.run_benchmark(graph, source, delta, threads, 3);
            }
            
            // Also test with delta = vertices/edges
            {
                int edges = nodes * 7; // Average degree of 7
                auto graph = generate_random_graph(nodes, edges, 0., 1., true, 12345);
                double adaptive_delta = static_cast<double>(nodes) / edges;
                int source = 0;
                
                std::cout << "\n[ADAPTIVE DELTA TEST: delta = vertices/edges = " 
                          << nodes << "/" << edges << " = " 
                          << std::fixed << std::setprecision(4) << adaptive_delta << "]\n";
                benchmark.run_benchmark(graph, source, adaptive_delta, threads, 3);
            }
        }
    }

    benchmark.print_summary();
    
    return 0;
} 