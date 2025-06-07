#ifndef GRAPH_GENERATORS_H
#define GRAPH_GENERATORS_H

#include <random>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <queue>
#include <fstream>
#include <iostream>
#include <sstream>
#include <cmath>
#include "graph.h"

// Enum for weight distribution types
enum class WeightDistribution {
    UNIFORM,
    POWER_LAW
};

// Power-law weight generator class
class PowerLawWeightGenerator {
private:
    double alpha;
    double min_weight;
    double max_weight;
    double exponent;
    double min_weight_exp;
    double max_weight_exp;
    double range_exp;
    std::uniform_real_distribution<double> uniform_dist;
    
public:
    PowerLawWeightGenerator(double min_w = 0.0, double max_w = 1.0, double a = 1.287) 
        : alpha(a), min_weight(min_w), max_weight(max_w), uniform_dist(0.0, 1.0) {
        
        // Handle edge case where min_weight is 0 (power-law doesn't work with 0)
        if (min_weight <= 0.0) {
            min_weight = 1e-6; // Small positive value
        }
        
        exponent = 1.0 - alpha; // 1 - 1.287 = -0.287
        min_weight_exp = std::pow(min_weight, exponent);
        max_weight_exp = std::pow(max_weight, exponent);
        range_exp = max_weight_exp - min_weight_exp;
    }
    
    double generate(std::mt19937& gen) {
        double u = uniform_dist(gen);
        double weight_exp = u * range_exp + min_weight_exp;
        return std::pow(weight_exp, 1.0 / exponent);
    }
};

// Function to parse graph from file (u v w format) - optimized for large files
Graph parse_graph_from_file(const std::string& filename, bool normalize_weights = false) {
    std::ifstream in(filename);
    if (!in.is_open()) {
        std::cerr << "Error: Could not open file " << filename << std::endl;
        return Graph(0, {});
    }
    
    // Get file size to estimate number of edges for better memory allocation
    in.seekg(0, std::ios::end);
    std::streamsize file_size = in.tellg();
    in.seekg(0, std::ios::beg);
    
    // Estimate number of lines (rough estimate: assume 20 chars per line on average)
    size_t estimated_edges = file_size / 20;
    
    std::vector<Edge> edges;
    edges.reserve(estimated_edges);  // Pre-allocate to avoid reallocations
    
    std::unordered_map<int, int> index_map;
    index_map.reserve(estimated_edges / 2);  // Rough estimate of unique vertices
    
    int cnt = 0;
    double max_w = 0.0;
    
    // Use larger buffer for faster I/O
    std::string line;
    line.reserve(64);  // Reserve space for typical line length
    
    while (std::getline(in, line)) {
        // Skip empty lines
        if (line.empty()) continue;
        
        // Fast parsing using find and substr (faster than stringstream)
        size_t pos1 = line.find(' ');
        if (pos1 == std::string::npos) continue;
        
        size_t pos2 = line.find(' ', pos1 + 1);
        if (pos2 == std::string::npos) continue;
        
        try {
            int u = std::stoi(line.substr(0, pos1));
            int v = std::stoi(line.substr(pos1 + 1, pos2 - pos1 - 1));
            double w = std::stod(line.substr(pos2 + 1));
            
            // Use emplace for more efficient insertion (single lookup)
            auto result_u = index_map.emplace(u, cnt);
            if (result_u.second) cnt++;  // New vertex inserted
            
            auto result_v = index_map.emplace(v, cnt);
            if (result_v.second) cnt++;  // New vertex inserted
            
            max_w = std::max(max_w, w);
            edges.emplace_back(result_u.first->second, result_v.first->second, w);
            
        } catch (const std::exception&) {
            // Skip malformed lines
            continue;
        }
    }
    
    // Shrink to fit to free unused memory
    edges.shrink_to_fit();
    
    if (normalize_weights && max_w > 0.0) {
        // Vectorized division for better performance
        const double inv_max_w = 1.0 / max_w;
        for (Edge &edge : edges) {
            edge.w *= inv_max_w;
        }
    }
    
    std::cout << "Loaded graph from " << filename << ": " << cnt << " vertices, " << edges.size() << " edges" << std::endl;
    return Graph(cnt, edges);
}

void save_graph_to_file(const Graph& graph, const std::string& filename) {
    std::ofstream file(filename);
    if (!file.is_open()) {
        std::cerr << "Error: Could not open file " << filename << " for writing." << std::endl;
        return;
    }
    
    // Set large buffer size for faster I/O (64MB)
    const size_t buffer_size = 64 * 1024 * 1024;
    std::vector<char> buffer(buffer_size);
    file.rdbuf()->pubsetbuf(buffer.data(), buffer_size);
    
    // Disable synchronization with C stdio for faster output
    std::ios_base::sync_with_stdio(false);
    
    // Use string batching for better performance
    std::ostringstream batch;
    
    int edge_count = 0;
    int batch_count = 0;
    const int batch_size = 50000; // Write every 50K edges
    
    for (int u = 0; u < graph.size(); u++) {
        for (const auto& [v, w] : graph[u]) {
            // Use faster formatting (space instead of " ", '\n' instead of endl)
            batch << u << ' ' << v << ' ' << w << '\n';
            edge_count++;
            batch_count++;
            
            // Flush batch to file periodically to avoid memory issues
            if (batch_count >= batch_size) {
                file << batch.str();
                batch.str(""); // Clear the stringstream
                batch.clear();
                batch_count = 0;
            }
        }
    }
    
    // Write remaining edges
    if (batch_count > 0) {
        file << batch.str();
    }
    
    file.close();
    std::cout << "Graph saved to: " << filename << " (" << edge_count << " edges)" << std::endl;
}

// Hash function for pair<int, int>
namespace std {
    template <>
    struct hash<pair<int, int>> {
        size_t operator()(const pair<int, int>& p) const {
            return hash<int>()(p.first) ^ (hash<int>()(p.second) << 1);
        }
    };
}

// Helper function to find connected components using DFS
std::vector<std::vector<int>> find_connected_components(int n, const std::vector<Edge>& edges) {
    // Build adjacency list
    std::vector<std::vector<int>> adj(n);
    for (const auto& edge : edges) {
        adj[edge.u].push_back(edge.v);
        adj[edge.v].push_back(edge.u); // Treat as undirected for connectivity
    }
    
    std::vector<bool> visited(n, false);
    std::vector<std::vector<int>> components;
    
    for (int start = 0; start < n; start++) {
        if (!visited[start]) {
            std::vector<int> component;
            std::queue<int> q;
            q.push(start);
            visited[start] = true;
            
            while (!q.empty()) {
                int u = q.front();
                q.pop();
                component.push_back(u);
                
                for (int v : adj[u]) {
                    if (!visited[v]) {
                        visited[v] = true;
                        q.push(v);
                    }
                }
            }
            components.push_back(component);
        }
    }
    
    return components;
}

// Helper function to extract largest connected component and remap indices
Graph extract_largest_connected_component(int n, const std::vector<Edge>& edges) {
    if (edges.empty()) {
        return Graph(1, {}); // Return single vertex if no edges
    }
    
    auto components = find_connected_components(n, edges);
    
    // Find largest component
    auto largest_component = std::max_element(components.begin(), components.end(),
        [](const std::vector<int>& a, const std::vector<int>& b) {
            return a.size() < b.size();
        });
    
    if (largest_component->empty()) {
        return Graph(1, {}); // Fallback
    }
    
    // Create mapping from old vertex ID to new vertex ID
    std::unordered_map<int, int> vertex_mapping;
    for (size_t i = 0; i < largest_component->size(); i++) {
        vertex_mapping[(*largest_component)[i]] = i;
    }
    
    // Extract edges within the largest component and remap vertices
    std::vector<Edge> remapped_edges;
    for (const auto& edge : edges) {
        auto u_it = vertex_mapping.find(edge.u);
        auto v_it = vertex_mapping.find(edge.v);
        
        if (u_it != vertex_mapping.end() && v_it != vertex_mapping.end()) {
            remapped_edges.push_back({u_it->second, v_it->second, edge.w});
        }
    }
    
    return Graph(largest_component->size(), remapped_edges);
}

// Generate a random graph with n vertices and m edges with uniform or power-law random weights in [min_weight, max_weight)
Graph generate_random_graph(int n, int m, double min_weight = 0.0, double max_weight = 1.0, bool undirected = false, 
                           WeightDistribution weight_dist_type = WeightDistribution::UNIFORM, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> vertex_dist(0, n - 1);
    std::uniform_real_distribution<double> uniform_weight_dist(min_weight, max_weight);
    PowerLawWeightGenerator powerlaw_gen(min_weight, max_weight);
    
    std::vector<Edge> edges;
    std::set<std::pair<int, int>> edge_set; // To avoid duplicate edges
    
    int attempts = 0;
    while ((int)edges.size() < m && attempts < m * 100) { // Prevent infinite loop
        int u = vertex_dist(gen);
        int v = vertex_dist(gen);
        if (u != v && edge_set.find({u, v}) == edge_set.end()) {
            double w;
            if (weight_dist_type == WeightDistribution::UNIFORM) {
                w = uniform_weight_dist(gen);
            } else {
                w = powerlaw_gen.generate(gen);
            }
            
            edges.push_back({u, v, w});
            edge_set.insert({u, v});
            
            if (undirected && edge_set.find({v, u}) == edge_set.end()) {
                edges.push_back({v, u, w});
                edge_set.insert({v, u});
            }
        }
        attempts++;
    }
    
    return extract_largest_connected_component(n, edges);
}

// Generate a large random graph with extended attempt limit for dense graphs
Graph generate_large_random_graph(int n, int m, double min_weight = 0., double max_weight = 1.0, bool undirected = false, 
                                  WeightDistribution weight_dist_type = WeightDistribution::UNIFORM, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> vertex_dist(0, n - 1);
    std::uniform_real_distribution<double> uniform_weight_dist(min_weight, max_weight);
    PowerLawWeightGenerator powerlaw_gen(min_weight, max_weight);
    
    std::vector<Edge> edges;
    std::unordered_set<std::pair<int, int>, std::hash<std::pair<int, int>>> edge_set;
    
    int attempts = 0;
    while ((int)edges.size() < m && attempts < m * 50) {
        int u = vertex_dist(gen);
        int v = vertex_dist(gen);
        if (u != v && edge_set.find({u, v}) == edge_set.end()) {
            double w;
            if (weight_dist_type == WeightDistribution::UNIFORM) {
                w = uniform_weight_dist(gen);
            } else {
                w = powerlaw_gen.generate(gen);
            }
            
            edges.push_back({u, v, w});
            edge_set.insert({u, v});
            
            if (undirected && edge_set.find({v, u}) == edge_set.end()) {
                edges.push_back({v, u, w});
                edge_set.insert({v, u});
            }
        }
        attempts++;
    }
    
    return extract_largest_connected_component(n, edges);
}

// Generate a complete graph with n vertices and uniform or power-law random weights in [min_weight, max_weight)
Graph generate_complete_graph(int n, double min_weight = 0., double max_weight = 1.0, bool undirected = false, 
                             WeightDistribution weight_dist_type = WeightDistribution::UNIFORM, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> uniform_weight_dist(min_weight, max_weight);
    PowerLawWeightGenerator powerlaw_gen(min_weight, max_weight);
    
    std::vector<Edge> edges;
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            if (u != v) {
                double w;
                if (weight_dist_type == WeightDistribution::UNIFORM) {
                    w = uniform_weight_dist(gen);
                } else {
                    w = powerlaw_gen.generate(gen);
                }
                
                edges.push_back({u, v, w});
                
                // For undirected complete graphs, we still add all edges but they represent bidirectional connections
                // The complete graph already includes both (u,v) and (v,u) so no additional edges needed
            }
        }
    }
    
    // Complete graphs are always connected, but apply for consistency
    return Graph(n, edges);
}

// Generate a grid graph with 10% chance of dropping edges (useful for testing algorithms on structured graphs)
Graph generate_grid_graph(int rows, int cols, double min_weight = 0., double max_weight = 1.0, bool undirected = true, 
                          WeightDistribution weight_dist_type = WeightDistribution::UNIFORM, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> uniform_weight_dist(min_weight, max_weight);
    PowerLawWeightGenerator powerlaw_gen(min_weight, max_weight);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    
    std::vector<Edge> edges;
    int n = rows * cols;
    
    auto get_index = [cols](int row, int col) { return row * cols + col; };
    
    // Add horizontal edges with 10% chance of dropping each edge
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols - 1; col++) {
            if (prob_dist(gen) > 0.1) { // 90% chance to include edge
                int u = get_index(row, col);
                int v = get_index(row, col + 1);
                double w;
                if (weight_dist_type == WeightDistribution::UNIFORM) {
                    w = uniform_weight_dist(gen);
                } else {
                    w = powerlaw_gen.generate(gen);
                }
                
                edges.push_back({u, v, w});
                
                if (undirected) {
                    edges.push_back({v, u, w});
                }
            }
        }
    }
    
    // Add vertical edges with 10% chance of dropping each edge
    for (int row = 0; row < rows - 1; row++) {
        for (int col = 0; col < cols; col++) {
            if (prob_dist(gen) > 0.1) { // 90% chance to include edge
                int u = get_index(row, col);
                int v = get_index(row + 1, col);
                double w;
                if (weight_dist_type == WeightDistribution::UNIFORM) {
                    w = uniform_weight_dist(gen);
                } else {
                    w = powerlaw_gen.generate(gen);
                }
                
                edges.push_back({u, v, w});
                
                if (undirected) {
                    edges.push_back({v, u, w});
                }
            }
        }
    }
    
    // With dropped edges, the graph might not be connected, so extract largest component
    return extract_largest_connected_component(n, edges);
}

// Generate a path graph (linear chain)
Graph generate_path_graph(int n, double min_weight = 0., double max_weight = 1.0, bool undirected = false, 
                         WeightDistribution weight_dist_type = WeightDistribution::UNIFORM, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> uniform_weight_dist(min_weight, max_weight);
    PowerLawWeightGenerator powerlaw_gen(min_weight, max_weight);
    
    std::vector<Edge> edges;
    for (int i = 0; i < n - 1; i++) {
        double w;
        if (weight_dist_type == WeightDistribution::UNIFORM) {
            w = uniform_weight_dist(gen);
        } else {
            w = powerlaw_gen.generate(gen);
        }
        
        edges.push_back({i, i + 1, w});
        
        if (undirected) {
            edges.push_back({i + 1, i, w});
        }
    }
    
    // Path graphs are always connected by construction
    return Graph(n, edges);
}

// Generate RMAT graph with parameters A, B, C, D (where D = 1 - A - B - C)
// RMAT graphs produce skewed degree distributions similar to real-world networks
Graph generate_rmat_graph(int n, int m, double A = 0.45, double B = 0.22, double C = 0.22, 
                         double min_weight = 0.0, double max_weight = 1.0, 
                         bool undirected = false, WeightDistribution weight_dist_type = WeightDistribution::UNIFORM, 
                         unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    std::uniform_real_distribution<double> uniform_weight_dist(min_weight, max_weight);
    PowerLawWeightGenerator powerlaw_gen(min_weight, max_weight);
    
    double D = 1.0 - A - B - C;
    if (D < 0 || A < 0 || B < 0 || C < 0) {
        std::cerr << "Error: Invalid RMAT parameters. A, B, C must be non-negative and A+B+C <= 1" << std::endl;
        return Graph(n, {});
    }
    
    // Ensure n is a power of 2 for proper RMAT generation
    int log_n = 0;
    int actual_n = 1;
    while (actual_n < n) {
        actual_n *= 2;
        log_n++;
    }
    
    std::unordered_set<std::pair<int, int>> edge_set;
    std::vector<Edge> edges;
    edges.reserve(undirected ? m * 2 : m);
    
    // Generate m edges using RMAT algorithm
    for (int edge_count = 0; edge_count < m; ) {
        int u = 0, v = 0;
        int bit_position = actual_n;
        
        // Recursively choose quadrant for each bit level
        for (int level = 0; level < log_n; level++) {
            bit_position /= 2;
            double rand_val = prob_dist(gen);
            
            if (rand_val < A) {
                // Top-left quadrant: (0,0) + offset
                // u and v remain unchanged
            } else if (rand_val < A + B) {
                // Top-right quadrant: (0,1) + offset
                v += bit_position;
            } else if (rand_val < A + B + C) {
                // Bottom-left quadrant: (1,0) + offset
                u += bit_position;
            } else {
                // Bottom-right quadrant: (1,1) + offset
                u += bit_position;
                v += bit_position;
            }
        }
        
        // Ensure vertices are within bounds
        u = u % n;
        v = v % n;
        
        // Avoid self-loops and duplicate edges
        if (u != v && edge_set.find({u, v}) == edge_set.end()) {
            double weight;
            if (weight_dist_type == WeightDistribution::UNIFORM) {
                weight = uniform_weight_dist(gen);
            } else {
                weight = powerlaw_gen.generate(gen);
            }
            
            edges.emplace_back(u, v, weight);
            edge_set.insert({u, v});
            edge_count++;
            
            if (undirected && edge_set.find({v, u}) == edge_set.end()) {
                edges.emplace_back(v, u, weight);
                edge_set.insert({v, u});
            }
        }
    }
    
    std::cout << "Generated RMAT graph: " << n << " vertices, " << edges.size() << " edges" << std::endl;
    std::cout << "RMAT parameters: A=" << A << ", B=" << B << ", C=" << C << ", D=" << D << std::endl;
    
    return Graph(n, edges);
}

// Convenience functions for generating graphs with power-law weight distributions
// These functions use the same parameters as their uniform counterparts but with power-law weights

// Generate a random graph with power-law weights
Graph generate_random_graph_powerlaw(int n, int m, double min_weight = 0.0, double max_weight = 1.0, 
                                     bool undirected = false, unsigned int seed = 42) {
    return generate_random_graph(n, m, min_weight, max_weight, undirected, WeightDistribution::POWER_LAW, seed);
}

// Generate a large random graph with power-law weights
Graph generate_large_random_graph_powerlaw(int n, int m, double min_weight = 0.0, double max_weight = 1.0, 
                                           bool undirected = false, unsigned int seed = 42) {
    return generate_large_random_graph(n, m, min_weight, max_weight, undirected, WeightDistribution::POWER_LAW, seed);
}

// Generate a complete graph with power-law weights
Graph generate_complete_graph_powerlaw(int n, double min_weight = 0.0, double max_weight = 1.0, 
                                      bool undirected = false, unsigned int seed = 42) {
    return generate_complete_graph(n, min_weight, max_weight, undirected, WeightDistribution::POWER_LAW, seed);
}

// Generate a grid graph with power-law weights
Graph generate_grid_graph_powerlaw(int rows, int cols, double min_weight = 0.0, double max_weight = 1.0, 
                                   bool undirected = true, unsigned int seed = 42) {
    return generate_grid_graph(rows, cols, min_weight, max_weight, undirected, WeightDistribution::POWER_LAW, seed);
}

// Generate a path graph with power-law weights
Graph generate_path_graph_powerlaw(int n, double min_weight = 0.0, double max_weight = 1.0, 
                                   bool undirected = false, unsigned int seed = 42) {
    return generate_path_graph(n, min_weight, max_weight, undirected, WeightDistribution::POWER_LAW, seed);
}

// Generate an RMAT graph with power-law weights
Graph generate_rmat_graph_powerlaw(int n, int m, double A = 0.45, double B = 0.22, double C = 0.22, 
                                   double min_weight = 0.0, double max_weight = 1.0, 
                                   bool undirected = false, unsigned int seed = 42) {
    return generate_rmat_graph(n, m, A, B, C, min_weight, max_weight, undirected, WeightDistribution::POWER_LAW, seed);
}

#endif 