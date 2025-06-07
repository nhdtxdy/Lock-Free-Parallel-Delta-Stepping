#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <iomanip>
#include <unordered_map>
#include <dirent.h>
#include <sys/stat.h>
#include <algorithm>
#include <cmath>
#include <memory>
#include <numeric>
#include "algos.h"
#include "queues/queues.h"
#include "graph_utils.h"

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

// Benchmark configuration structure
struct SolverConfig {
    std::unique_ptr<ShortestPathSolverBase> solver;
    std::string config_name;
    double delta;
    int threads;
    
    SolverConfig(std::unique_ptr<ShortestPathSolverBase> s, const std::string& name, double d = 0.0, int t = 1)
        : solver(std::move(s)), config_name(name), delta(d), threads(t) {}
};

// Benchmark result structure
struct BenchmarkResult {
    std::string algorithm;
    std::string config_name;
    std::string graph_name;
    int vertices;
    int edges;
    int source;
    double delta;
    int threads;
    long long time_ms;
    long long min_time_ms;
    long long max_time_ms;
    double avg_time_ms;
    int num_runs;
    int reachable_vertices;
    bool correct;
    double speedup_vs_reference;
    double efficiency;
};


template<typename SolverType, typename... Args>
SolverConfig make_solver_config(const std::string& config_name, double delta, int threads, Args&&... args) {
    return SolverConfig(
        std::make_unique<SolverType>(std::forward<Args>(args)...),
        config_name,
        delta,
        threads
    );
}

// Create all solver configurations to benchmark - adapted from correctness_checker.h pattern
std::vector<SolverConfig> create_solver_configurations() {
    std::vector<SolverConfig> configs;
    
    // Configuration parameters
    std::vector<double> deltas = {0.01, 0.05, 0.15, 0.23, 0.6};
    std::vector<double> parallel_deltas = {0.01, 0.05, 0.15, 0.23, 0.6}; // Subset for parallel testing
    std::vector<int> thread_counts = {1, 2, 4, 8, 16};
    
    // Add Dijkstra (reference implementation)
    configs.emplace_back(make_solver_config<Dijkstra>("Dijkstra", 0.0, 1));
    
    // Add Delta Stepping Sequential with different delta values
    for (double delta : deltas) {
        configs.emplace_back(make_solver_config<DeltaSteppingSequential>(
            "Sequential_δ=" + std::to_string(delta), delta, 1, delta));
    }
    
    // Add all parallel implementations with different configurations
    for (double delta : parallel_deltas) {
        for (int threads : thread_counts) {
            // // Delta Stepping Parallel
            configs.emplace_back(make_solver_config<DeltaSteppingParallel>(
                "Parallel_δ=" + std::to_string(delta) + "_t=" + std::to_string(threads),
                delta, threads, delta, threads));
            
            // Delta Stepping OpenMP
            // configs.emplace_back(make_solver_config<DeltaSteppingOpenMP>(
            //     "OpenMP_δ=" + std::to_string(delta) + "_t=" + std::to_string(threads),
            //     delta, threads, delta, threads));
        }
    }
    
    return configs;
}

// Run comprehensive benchmark on a single graph
std::vector<BenchmarkResult> benchmark_graph(const Graph& graph, const std::string& graph_name, int source = 0, int num_runs = 5) {
    std::vector<BenchmarkResult> results;
    auto configs = create_solver_configurations();
    
    std::cout << "\n=== Benchmarking: " << graph_name << " ===" << std::endl;
    std::cout << "Vertices: " << graph.size() << ", Edges: ";
    int edge_count = 0;
    for (int u = 0; u < graph.size(); u++) {
        edge_count += graph[u].size();
    }
    std::cout << edge_count << ", Source: " << source << std::endl;
    std::cout << "Runs per configuration: " << num_runs << std::endl;
    
    // Ensure source is valid
    source = std::min(source, graph.size() - 1);
    
    // Store reference distances (from Dijkstra)
    std::vector<double> reference_distances;
    long long reference_time = 0;
    int reachable_vertices = 0;
    
    // Run all solvers
    for (auto& config : configs) {
        std::cout << "\nRunning " << config.solver->name() << " (" << config.config_name << ")..." << std::endl;
        
        std::vector<long long> run_times;
        std::vector<double> final_distances;
        bool all_runs_correct = true;
        
        // Run multiple times
        std::cout << "  Runs: ";
        for (int run = 0; run < num_runs; run++) {
            auto start = std::chrono::high_resolution_clock::now();
            std::vector<double> distances = config.solver->compute(graph, source);
            auto end = std::chrono::high_resolution_clock::now();
            auto time_taken = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
            
            run_times.push_back(time_taken.count());
            
            // Store distances from first run for correctness checking
            if (run == 0) {
                final_distances = distances;
            }
            
            std::cout << time_taken.count() << "ms ";
            if ((run + 1) % 10 == 0) std::cout << "\n         ";
        }
        std::cout << std::endl;
        
        // Calculate statistics
        long long min_time = *std::min_element(run_times.begin(), run_times.end());
        long long max_time = *std::max_element(run_times.begin(), run_times.end());
        double avg_time = std::accumulate(run_times.begin(), run_times.end(), 0.0) / run_times.size();
        
        // For the first solver (Dijkstra), store as reference
        bool is_reference = reference_distances.empty();
        if (is_reference) {
            reference_distances = final_distances;
            reference_time = min_time;
            
            // Count reachable vertices
            for (double d : final_distances) {
                if (!std::isinf(d)) reachable_vertices++;
            }
        }
        
        // Check correctness against reference
        bool correct = are_distances_equal(reference_distances, final_distances);
        
        // Calculate speedup and efficiency using minimum time
        double speedup = reference_time > 0 ? (double)reference_time / min_time : 1.0;
        double efficiency = speedup / config.threads;
        
        BenchmarkResult result = {
            config.solver->name(),
            config.config_name,
            graph_name,
            graph.size(),
            edge_count,
            source,
            config.delta,
            config.threads,
            min_time,  // Use minimum time as the primary result
            min_time,
            max_time,
            avg_time,
            num_runs,
            reachable_vertices,
            correct,
            speedup,
            efficiency
        };
        results.push_back(result);
        
        std::cout << "  Min time: " << min_time << " ms" << std::endl;
        std::cout << "  Max time: " << max_time << " ms" << std::endl;
        std::cout << "  Avg time: " << std::fixed << std::setprecision(1) << avg_time << " ms" << std::endl;
        std::cout << "  Variance: " << std::fixed << std::setprecision(1) << ((double)(max_time - min_time) / min_time * 100) << "%" << std::endl;
        if (!is_reference) {
            std::cout << "  Speedup vs reference: " << std::fixed << std::setprecision(2) << speedup << "x" << std::endl;
            std::cout << "  Efficiency: " << std::fixed << std::setprecision(2) << efficiency << std::endl;
        }
        std::cout << "  Correctness: " << (correct ? "PASS" : "FAIL") << std::endl;
        
        if (!correct) {
            std::cout << "  WARNING: Algorithm produced incorrect results!" << std::endl;
        }
    }
    
    std::cout << "Reachable vertices: " << reachable_vertices << "/" << graph.size() << std::endl;
    
    return results;
}

// Print comprehensive benchmark summary
void print_benchmark_summary(const std::vector<BenchmarkResult>& all_results) {
    std::cout << "\n" << std::string(160, '=') << std::endl;
    std::cout << "COMPREHENSIVE BENCHMARK SUMMARY" << std::endl;
    std::cout << std::string(160, '=') << std::endl;
    
    // Header
    std::cout << std::left 
              << std::setw(20) << "Graph"
              << std::setw(25) << "Algorithm" 
              << std::setw(30) << "Configuration"
              << std::setw(8) << "Vertices"
              << std::setw(10) << "Edges"
              << std::setw(8) << "Threads"
              << std::setw(10) << "Min(ms)"
              << std::setw(10) << "Avg(ms)"
              << std::setw(10) << "Max(ms)"
              << std::setw(8) << "Runs"
              << std::setw(10) << "Speedup"
              << std::setw(12) << "Efficiency"
              << std::setw(10) << "Correct"
              << std::endl;
    std::cout << std::string(160, '-') << std::endl;
    
    // Group results by graph
    std::string current_graph = "";
    for (const auto& result : all_results) {
        if (result.graph_name != current_graph) {
            if (!current_graph.empty()) {
                std::cout << std::string(160, '-') << std::endl;
            }
            current_graph = result.graph_name;
        }
        
        std::cout << std::left
                  << std::setw(20) << result.graph_name
                  << std::setw(25) << result.algorithm
                  << std::setw(30) << result.config_name
                  << std::setw(8) << result.vertices
                  << std::setw(10) << result.edges
                  << std::setw(8) << result.threads
                  << std::setw(10) << result.min_time_ms
                  << std::setw(10) << std::fixed << std::setprecision(1) << result.avg_time_ms
                  << std::setw(10) << result.max_time_ms
                  << std::setw(8) << result.num_runs
                  << std::setw(10) << std::fixed << std::setprecision(2) << result.speedup_vs_reference << "x"
                  << std::setw(12) << std::fixed << std::setprecision(2) << result.efficiency
                  << std::setw(10) << (result.correct ? "PASS" : "FAIL")
                  << std::endl;
    }
    
    std::cout << std::string(160, '=') << std::endl;
    
    // Analysis section
    std::cout << "\nPERFORMANCE ANALYSIS:" << std::endl;
    std::cout << std::string(50, '-') << std::endl;
    
    // Find best performing configurations by algorithm type
    auto best_sequential = std::max_element(all_results.begin(), all_results.end(),
        [](const BenchmarkResult& a, const BenchmarkResult& b) {
            return (a.algorithm.find("Sequential") != std::string::npos && 
                    b.algorithm.find("Sequential") != std::string::npos) ? 
                   a.speedup_vs_reference < b.speedup_vs_reference : false;
        });
    
    auto best_parallel = std::max_element(all_results.begin(), all_results.end(),
        [](const BenchmarkResult& a, const BenchmarkResult& b) {
            return (a.algorithm.find("Parallel") != std::string::npos && 
                    b.algorithm.find("Parallel") != std::string::npos) ? 
                   a.speedup_vs_reference < b.speedup_vs_reference : false;
        });
    
    if (best_sequential != all_results.end() && best_sequential->algorithm.find("Sequential") != std::string::npos) {
        std::cout << "Best Sequential Configuration: " << best_sequential->config_name
                  << " (Speedup: " << std::fixed << std::setprecision(2) << best_sequential->speedup_vs_reference << "x)" << std::endl;
    }
    
    if (best_parallel != all_results.end() && best_parallel->algorithm.find("Parallel") != std::string::npos) {
        std::cout << "Best Parallel Configuration: " << best_parallel->config_name
                  << " (Speedup: " << std::fixed << std::setprecision(2) << best_parallel->speedup_vs_reference << "x, "
                  << "Efficiency: " << std::fixed << std::setprecision(2) << best_parallel->efficiency << ")" << std::endl;
    }
    
    // Timing consistency analysis
    std::cout << "\nTIMING CONSISTENCY:" << std::endl;
    double total_variance = 0.0;
    int variance_count = 0;
    for (const auto& result : all_results) {
        if (result.min_time_ms > 0) {
            double variance = (double)(result.max_time_ms - result.min_time_ms) / result.min_time_ms * 100;
            total_variance += variance;
            variance_count++;
        }
    }
    if (variance_count > 0) {
        std::cout << "Average timing variance: " << std::fixed << std::setprecision(1) 
                  << (total_variance / variance_count) << "%" << std::endl;
    }
}

// Save results to CSV file
void save_results_to_csv(const std::vector<BenchmarkResult>& results, const std::string& filename) {
    std::ofstream csv(filename);
    csv << "Graph,Algorithm,Configuration,Vertices,Edges,Source,Delta,Threads,Min_Time_ms,Avg_Time_ms,Max_Time_ms,Num_Runs,Speedup,Efficiency,Correct\n";
    
    for (const auto& result : results) {
        csv << result.graph_name << ","
            << result.algorithm << ","
            << result.config_name << ","
            << result.vertices << ","
            << result.edges << ","
            << result.source << ","
            << result.delta << ","
            << result.threads << ","
            << result.min_time_ms << ","
            << std::fixed << std::setprecision(1) << result.avg_time_ms << ","
            << result.max_time_ms << ","
            << result.num_runs << ","
            << result.speedup_vs_reference << ","
            << result.efficiency << ","
            << (result.correct ? "PASS" : "FAIL") << "\n";
    }
    
    std::cout << "\nResults saved to: " << filename << std::endl;
}

int main(int argc, char* argv[]) {
    std::cout << "=== SHORTEST PATH ALGORITHMS BENCHMARK TOOL ===" << std::endl;
    std::cout << "Polymorphic benchmark supporting multiple algorithm implementations" << std::endl;
    std::cout << "Usage: " << argv[0] << " [--runs <number>] [graph_files...]" << std::endl;
    std::cout << "  --runs <number>: Number of iterations per benchmark (default: 5)" << std::endl;
    std::cout << "  graph_files:     Specific graph files to benchmark (default: scan assets/test_cases/)" << std::endl;
    
    std::vector<std::string> graph_files;
    int num_runs = 5; // Default number of runs per benchmark
    
    // Parse command line arguments
    int file_arg_start = 1;
    if (argc > 1 && std::string(argv[1]) == "--runs") {
        if (argc > 2) {
            num_runs = std::atoi(argv[2]);
            if (num_runs <= 0) {
                std::cout << "Error: Number of runs must be positive" << std::endl;
                return 1;
            }
            file_arg_start = 3;
            std::cout << "Configured to run " << num_runs << " iterations per benchmark" << std::endl;
        } else {
            std::cout << "Error: --runs option requires a number" << std::endl;
            return 1;
        }
    }
    
    // If specific files are provided as arguments, use them
    if (argc > file_arg_start) {
        for (int i = file_arg_start; i < argc; i++) {
            graph_files.push_back(argv[i]);
        }
    } else {
        // Default: scan the assets/test_cases directory for all .txt files
        std::string test_cases_dir = "assets/test_cases";
        
        // Check if directory exists and scan for .txt files
        DIR* dir = opendir(test_cases_dir.c_str());
        if (dir != nullptr) {
            struct dirent* entry;
            while ((entry = readdir(dir)) != nullptr) {
                std::string filename = entry->d_name;
                // Check if file ends with .txt
                if (filename.length() > 4 && 
                    filename.substr(filename.length() - 4) == ".txt") {
                    std::string full_path = test_cases_dir + "/" + filename;
                    graph_files.push_back(full_path);
                }
            }
            closedir(dir);
            
            // Sort files for consistent ordering
            std::sort(graph_files.begin(), graph_files.end());
        }
        
        // Fallback: look in current directory for generated graph files
        if (graph_files.empty()) {
            std::cout << "No graph files found in " << test_cases_dir << "." << std::endl;
            std::cout << "Looking for graph files in current directory..." << std::endl;
            
            std::vector<std::string> fallback_files = {
                "large_random_sparse.txt",
                "large_random_dense.txt", 
                "large_complete.txt",
                "large_scale_free.txt",
                "large_grid.txt",
                "large_path.txt",
                "massive_random.txt",
                "large_undirected.txt",
                "large_grid_with_removal.txt",
                "road_network_like.txt"
            };
            
            for (const auto& file : fallback_files) {
                std::ifstream test(file);
                if (test.good()) {
                    graph_files.push_back(file);
                }
            }
        }
        
        if (graph_files.empty()) {
            std::cout << "No graph files found in " << test_cases_dir << " or current directory." << std::endl;
            std::cout << "Please ensure graph files exist in " << test_cases_dir << "," << std::endl;
            std::cout << "or run 'make graph_generator && ./graph_generator' to generate test graphs," << std::endl;
            std::cout << "or specify graph files as command line arguments." << std::endl;
            return 1;
        }
    }
    
    std::cout << "\nFound " << graph_files.size() << " graph files to benchmark:" << std::endl;
    for (const auto& file : graph_files) {
        std::cout << "  - " << file << std::endl;
    }
    
    // Show configured algorithms
    auto configs = create_solver_configurations();
    std::cout << "\nConfigured " << configs.size() << " solver configurations:" << std::endl;
    for (const auto& config : configs) {
        std::cout << "  - " << config.solver->name() << " (" << config.config_name << ")" << std::endl;
    }
    
    std::vector<BenchmarkResult> all_results;
    
    // Benchmark each graph
    for (const auto& file : graph_files) {
        try {
            Graph graph = parse_graph_from_file(file, false); // Enable weight normalization to [0, 1]
            if (graph.size() == 0) {
                std::cout << "Skipping empty graph: " << file << std::endl;
                continue;
            }
            
            // Extract graph name from filename
            std::string graph_name = file;
            if (graph_name.find('.') != std::string::npos) {
                graph_name = graph_name.substr(0, graph_name.find_last_of('.'));
            }
            
            auto results = benchmark_graph(graph, graph_name, 0, num_runs);
            all_results.insert(all_results.end(), results.begin(), results.end());
            
        } catch (const std::exception& e) {
            std::cout << "Error processing " << file << ": " << e.what() << std::endl;
        }
    }
    
    // Print comprehensive summary
    print_benchmark_summary(all_results);
    
    // Save results to CSV
    save_results_to_csv(all_results, "benchmark_results.csv");
    
    std::cout << "\n=== BENCHMARK COMPLETE ===" << std::endl;
    std::cout << "Total configurations tested: " << all_results.size() << std::endl;
    
    return 0;
} 