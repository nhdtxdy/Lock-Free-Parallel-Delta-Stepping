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
    std::cout << "=== Large-Scale Graph Generator ===" << std::endl;
    std::cout << "Generating large graphs for performance testing and benchmarking..." << std::endl << std::endl;
    
    // 1. Large random sparse graph
    {
        std::cout << "1. Generating Large Random Sparse Graph..." << std::endl;
        int n = 3e6;   // 500K vertices
        int m = 6e7;  // 5M edges (avg degree ~20)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_large_random_graph(n, m, 0.0, 1.0, false, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_random_sparse.txt");
        std::cout << std::endl;
    }
    
    // 2. Large random dense graph
    {
        std::cout << "2. Generating Large Random Dense Graph..." << std::endl;
        int n = 5e6;     // 20K vertices
        int m = 1e8;   // 2M edges (avg degree ~200)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_large_random_graph(n, m, 0.0, 1.0, false, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_random_dense.txt");
        std::cout << std::endl;
    }
    
    // 3. Large complete graph
    {
        std::cout << "3. Generating Large Complete Graph..." << std::endl;
        int n = 10000;     // 1K vertices (complete = ~500K edges)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_complete_graph(n, 0.0, 1.0, false, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_complete.txt");
        std::cout << std::endl;
    }
    
    // 4. Large scale-free graph (power law degree distribution)
    {
        std::cout << "4. Generating Large Scale-Free Graph..." << std::endl;
        int n = 1e7;   // 100K vertices
        int m = 8;        // edges per new vertex (~800K edges)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_scale_free_graph(n, m, 2.5, 0.0, 1.0, false, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_scale_free.txt");
        std::cout << std::endl;
    }
    
    // 5. Large grid graph
    {
        std::cout << "5. Generating Large Grid Graph..." << std::endl;
        int rows = 10000;   // 1000x1000 = 1M vertices
        int cols = 10000;   // ~4M edges
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_grid_graph(rows, cols, 0.0, 1.0, false, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_grid.txt");
        std::cout << std::endl;
    }
    
    // 6. Large path graph
    {
        std::cout << "6. Generating Large Path Graph..." << std::endl;
        int n = 1000000;  // 1M vertices, 1M-1 edges
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_path_graph(n, 0.0, 1.0, false, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_path.txt");
        std::cout << std::endl;
    }
    
    // 7. Massive random graph
    {
        std::cout << "7. Generating Massive Random Graph..." << std::endl;
        int n = 200000;   // 200K vertices
        int m = 10000000; // 10M edges (avg degree ~100)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_large_random_graph(n, m, 0.0, 1.0, true, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "massive_random.txt");
        std::cout << std::endl;
    }
    
    // 8. Large undirected graph (social network simulation)
    {
        std::cout << "8. Generating Large Undirected Graph..." << std::endl;
        int n = 50000;    // 50K vertices
        int m = 2000000;  // 2M directed edges = 1M undirected edges
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_large_random_graph(n, m, 0.0, 1.0, true, seed); // undirected=true
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_undirected.txt");
        std::cout << std::endl;
    }
    
    // 9. Large grid with edge removal (sparse grid network)
    {
        std::cout << "9. Generating Large Grid with Edge Removal..." << std::endl;
        int rows = 800;   // 800x800 = 640K vertices
        int cols = 800;   // with 10% removal probability
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_grid_graph(rows, cols, 0.0, 1.0, true, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "large_grid_with_removal.txt");
        std::cout << std::endl;
    }
    
    // 10. Road network-like graph (extremely large but sparse)
    {
        std::cout << "10. Generating Road Network-like Graph..." << std::endl;
        int n = 1000000;  // 1M vertices (road intersection scale)
        int m = 2500000;  // 2.5M edges (avg degree ~5, like road networks)
        unsigned int seed = seed_gen();
        std::cout << "Using random seed: " << seed << std::endl;
        auto start = std::chrono::high_resolution_clock::now();
        Graph graph = generate_large_random_graph(n, m, 0.0, 1.0, true, seed);
        auto end = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(end - start);
        std::cout << "Generation time: " << duration.count() << " ms" << std::endl;
        save_graph_to_file(graph, "road_network_like.txt");
        std::cout << std::endl;
    }
    
    std::cout << "=== Large-Scale Graph Generation Complete ===" << std::endl;
    std::cout << "Generated 10 large-scale graph types for performance testing and benchmarking." << std::endl;
    std::cout << "Graph sizes range from 1K to 1M vertices with millions of edges." << std::endl;
    std::cout << "Each file contains edges in format: u v w (one edge per line)" << std::endl;
    std::cout << "These graphs are suitable for testing parallel shortest path algorithms." << std::endl;
    
    return 0;
} 