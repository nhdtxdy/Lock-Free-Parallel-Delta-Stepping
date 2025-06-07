#include <iostream>
#include <fstream>
#include <string>
#include <vector>
#include <chrono>
#include <random>
#include "graph.h"
#include "graph_utils.h"

int main() {
    // Initialize random number generator for seeds
    std::random_device rd;
    std::mt19937 seed_gen(rd());
    std::cout << "=== Large-Scale Graph Generator with Weight Distributions ===" << std::endl;
    std::cout << "Generating large graphs with both uniform and power-law weight distributions..." << std::endl << std::endl;
    
    // 1. Large random sparse graph - both weight distributions
    {
        std::cout << "1. Generating Large Random Sparse Graphs (Uniform & Power-law weights)..." << std::endl;
        int n = 2e6;   // 2M vertices
        int m = 6e6;   // 6M edges (avg degree ~6)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        
        // Uniform weights version
        std::cout << "  -> Generating uniform weights version..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph_uniform = generate_large_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::UNIFORM, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_uniform, "assets/test_cases/lrs_2e6_6e6_uniform.txt");
        
        // Power-law weights version
        std::cout << "  -> Generating power-law weights version..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        Graph graph_powerlaw = generate_large_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::POWER_LAW, seed);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_powerlaw, "assets/test_cases/lrs_2e6_6e6_powerlaw.txt");
        std::cout << std::endl;
    }
    
    // 2. Large random dense graph - both weight distributions
    {
        std::cout << "2. Generating Large Random Dense Graphs (Uniform & Power-law weights)..." << std::endl;
        int n = 1e6;  // 100K vertices (reduced from 5M for practical reasons)
        int m = 1e8; // 2M edges (avg degree ~40)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        
        // Uniform weights version
        std::cout << "  -> Generating uniform weights version..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph_uniform = generate_large_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::UNIFORM, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_uniform, "assets/test_cases/lrd_1e6_1e8_uniform.txt");
        
        // Power-law weights version
        std::cout << "  -> Generating power-law weights version..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        Graph graph_powerlaw = generate_large_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::POWER_LAW, seed);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_powerlaw, "assets/test_cases/lrd_1e6_1e8_powerlaw.txt");
        std::cout << std::endl;
    }
    
    
    // 3. Large grid graph - both weight distributions
    {
        std::cout << "4. Generating Large Grid Graphs (Uniform & Power-law weights)..." << std::endl;
        int rows = 2000;   // 2000x2000 = 4M vertices
        int cols = 2000;   // ~16M edges
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        
        // Uniform weights version
        std::cout << "  -> Generating uniform weights version..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph_uniform = generate_grid_graph(rows, cols, 0.0, 1.0, true, WeightDistribution::UNIFORM, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_uniform, "assets/test_cases/lg_2k_2k_uniform.txt");
        
        // Power-law weights version
        std::cout << "  -> Generating power-law weights version..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        Graph graph_powerlaw = generate_grid_graph(rows, cols, 0.0, 1.0, true, WeightDistribution::POWER_LAW, seed);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_powerlaw, "assets/test_cases/lg_2k_2k_powerlaw.txt");
        std::cout << std::endl;
    }
    
    // 6. RMAT graph with skewed degree distribution - both weight distributions
    {
        std::cout << "6. Generating RMAT Graphs with Skewed Degree Distribution (Uniform & Power-law weights)..." << std::endl;
        int n = 1000000;  // 1M vertices
        int m = 5000000;  // 5M edges (avg degree ~10)
        double A = 0.45, B = 0.22, C = 0.22;  // Parameters for highly skewed degrees
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        std::cout << "RMAT parameters: A=" << A << ", B=" << B << ", C=" << C << ", D=" << (1-A-B-C) << std::endl;
        
        // Uniform weights version
        std::cout << "  -> Generating uniform weights version..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph_uniform = generate_rmat_graph(n, m, A, B, C, 0.0, 1.0, true, WeightDistribution::UNIFORM, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_uniform, "assets/test_cases/rmat_1e6_5e6_uniform.txt");
        
        // Power-law weights version
        std::cout << "  -> Generating power-law weights version..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        Graph graph_powerlaw = generate_rmat_graph(n, m, A, B, C, 0.0, 1.0, true, WeightDistribution::POWER_LAW, seed);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_powerlaw, "assets/test_cases/rmat_1e6_5e6_powerlaw.txt");
        std::cout << std::endl;
    }
    
    // 7. Large undirected random graph (social network simulation) - both weight distributions
    {
        std::cout << "7. Generating Large Undirected Random Graphs (Uniform & Power-law weights)..." << std::endl;
        int n = 500000;    // 500K vertices
        int m = 5000000;   // 5M edges (bidirectional)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        
        // Uniform weights version
        std::cout << "  -> Generating uniform weights version..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph_uniform = generate_large_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::UNIFORM, seed); // undirected=true
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_uniform, "assets/test_cases/lu_500k_5e6_uniform.txt");
        
        // Power-law weights version
        std::cout << "  -> Generating power-law weights version..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        Graph graph_powerlaw = generate_large_random_graph(n, m, 0.0, 1.0, true, WeightDistribution::POWER_LAW, seed); // undirected=true
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_powerlaw, "assets/test_cases/lu_500k_5e6_powerlaw.txt");
        std::cout << std::endl;
    }
    
    // 12. Large RMAT graph (similar to social networks)
    {
        std::cout << "12. Generating Large RMAT Graph (Social Network-like)..." << std::endl;
        int n = 2e6;  // 2M vertices
        int m = 1e7; // 10M edges (avg degree ~10)
        double A = 0.45, B = 0.22, C = 0.22;  // Same skewed parameters
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        std::cout << "RMAT parameters: A=" << A << ", B=" << B << ", C=" << C << ", D=" << (1-A-B-C) << std::endl;
        
        // Uniform weights version
        std::cout << "  -> Generating uniform weights version..." << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph_uniform = generate_rmat_graph(n, m, A, B, C, 0.0, 1.0, true, WeightDistribution::UNIFORM, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_uniform, "assets/test_cases/rmat_2e6_10e6_uniform.txt");
        
        // Power-law weights version
        std::cout << "  -> Generating power-law weights version..." << std::endl;
        start = std::chrono::high_resolution_clock::now();
        Graph graph_powerlaw = generate_rmat_graph(n, m, A, B, C, 0.0, 1.0, true, WeightDistribution::POWER_LAW, seed);
        end = std::chrono::high_resolution_clock::now();
        duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "     Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph_powerlaw, "assets/test_cases/rmat_2e6_10e6_powerlaw.txt");
        std::cout << std::endl;
    }


    std::cout << "=== Large-Scale Graph Generation Complete ===" << std::endl;
    std::cout << "Generated 8 graph types with both uniform and power-law weight distributions (16 total graphs)." << std::endl;
    std::cout << "Graph sizes range from 5K to 5M vertices with millions of edges." << std::endl;
    std::cout << "Weight distributions:" << std::endl;
    std::cout << "  - Uniform: weights randomly distributed in [0,1)" << std::endl;
    std::cout << "  - Power-law: P(w) ∝ w^(-1.287), most weights are small with few large ones" << std::endl;
    std::cout << "Each file contains edges in format: u v w (one edge per line)" << std::endl;
    std::cout << "These graphs are suitable for testing parallel shortest path algorithms." << std::endl;
    
    return 0;
} 