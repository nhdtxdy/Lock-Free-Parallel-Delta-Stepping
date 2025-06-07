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
#include "graph.h"

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

// Generate a random graph with n vertices and m edges with uniform random weights in [min_weight, max_weight)
Graph generate_random_graph(int n, int m, double min_weight = 0.0, double max_weight = 1.0, bool undirected = false, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> vertex_dist(0, n - 1);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    
    std::vector<Edge> edges;
    std::set<std::pair<int, int>> edge_set; // To avoid duplicate edges
    
    int attempts = 0;
    while ((int)edges.size() < m && attempts < m * 100) { // Prevent infinite loop
        int u = vertex_dist(gen);
        int v = vertex_dist(gen);
        if (u != v && edge_set.find({u, v}) == edge_set.end()) {
            double w = weight_dist(gen);
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
Graph generate_large_random_graph(int n, int m, double min_weight = 0., double max_weight = 1.0, bool undirected = false, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_int_distribution<int> vertex_dist(0, n - 1);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    
    std::vector<Edge> edges;
    std::unordered_set<std::pair<int, int>, std::hash<std::pair<int, int>>> edge_set;
    
    int attempts = 0;
    while ((int)edges.size() < m && attempts < m * 50) {
        int u = vertex_dist(gen);
        int v = vertex_dist(gen);
        if (u != v && edge_set.find({u, v}) == edge_set.end()) {
            double w = weight_dist(gen);
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

// Generate a complete graph with n vertices and uniform random weights in [min_weight, max_weight)
Graph generate_complete_graph(int n, double min_weight = 0., double max_weight = 1.0, bool undirected = false, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    
    std::vector<Edge> edges;
    for (int u = 0; u < n; u++) {
        for (int v = 0; v < n; v++) {
            if (u != v) {
                double w = weight_dist(gen);
                edges.push_back({u, v, w});
                
                // For undirected complete graphs, we still add all edges but they represent bidirectional connections
                // The complete graph already includes both (u,v) and (v,u) so no additional edges needed
            }
        }
    }
    
    // Complete graphs are always connected, but apply for consistency
    return Graph(n, edges);
}

// Generate a scale-free graph (power law degree distribution)
Graph generate_scale_free_graph(int n, int m, double gamma = 2.5, double min_weight = 0., double max_weight = 1.0, bool undirected = false, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    std::uniform_real_distribution<double> prob_dist(0.0, 1.0);
    
    std::vector<Edge> edges;
    std::vector<int> degrees(n, 0);
    
    // Start with a small complete graph to ensure connectivity
    for (int i = 0; i < std::min(3, n); i++) {
        for (int j = i + 1; j < std::min(3, n); j++) {
            double w = weight_dist(gen);
            edges.push_back({i, j, w});
            degrees[i]++;
            degrees[j]++;
            
            if (undirected) {
                edges.push_back({j, i, w});
            }
        }
    }
    
    // Add remaining vertices with preferential attachment
    for (int new_vertex = 3; new_vertex < n; new_vertex++) {
        int total_degree = 0;
        for (int i = 0; i < new_vertex; i++) {
            total_degree += degrees[i];
        }
        
        int edges_to_add = std::min(m, new_vertex);
        std::unordered_set<int> connected;
        
        // Ensure at least one connection to maintain connectivity
        if (new_vertex > 0) {
            int random_existing = std::uniform_int_distribution<int>(0, new_vertex - 1)(gen);
            double w = weight_dist(gen);
            edges.push_back({new_vertex, random_existing, w});
            degrees[new_vertex]++;
            degrees[random_existing]++;
            connected.insert(random_existing);
            
            if (undirected) {
                edges.push_back({random_existing, new_vertex, w});
            }
        }
        
        // Add additional edges using preferential attachment
        for (int attempt = 0; attempt < edges_to_add * 3 && (int)connected.size() < edges_to_add; attempt++) {
            for (int i = 0; i < new_vertex && (int)connected.size() < edges_to_add; i++) {
                if (connected.count(i) == 0) {
                    double prob = (degrees[i] + 1.0) / (total_degree + new_vertex);
                    if (prob_dist(gen) < prob) {
                        double w = weight_dist(gen);
                        edges.push_back({new_vertex, i, w});
                        degrees[new_vertex]++;
                        degrees[i]++;
                        connected.insert(i);
                        
                        if (undirected) {
                            edges.push_back({i, new_vertex, w});
                        }
                    }
                }
            }
        }
    }
    
    // Scale-free graphs built this way should be connected, but check to be safe
    return extract_largest_connected_component(n, edges);
}

// Generate a grid graph (useful for testing algorithms on structured graphs)
Graph generate_grid_graph(int rows, int cols, double min_weight = 0., double max_weight = 1.0, bool undirected = true, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    
    std::vector<Edge> edges;
    int n = rows * cols;
    
    auto get_index = [cols](int row, int col) { return row * cols + col; };
    
    // Add horizontal edges
    for (int row = 0; row < rows; row++) {
        for (int col = 0; col < cols - 1; col++) {
            int u = get_index(row, col);
            int v = get_index(row, col + 1);
            double w = weight_dist(gen);
            edges.push_back({u, v, w});
            
            if (undirected) {
                edges.push_back({v, u, w});
            }
        }
    }
    
    // Add vertical edges
    for (int row = 0; row < rows - 1; row++) {
        for (int col = 0; col < cols; col++) {
            int u = get_index(row, col);
            int v = get_index(row + 1, col);
            double w = weight_dist(gen);
            edges.push_back({u, v, w});
            
            if (undirected) {
                edges.push_back({v, u, w});
            }
        }
    }
    
    // Grid graphs are always connected by construction
    return Graph(n, edges);
}

// Generate a path graph (linear chain)
Graph generate_path_graph(int n, double min_weight = 0., double max_weight = 1.0, bool undirected = false, unsigned int seed = 42) {
    std::mt19937 gen(seed);
    std::uniform_real_distribution<double> weight_dist(min_weight, max_weight);
    
    std::vector<Edge> edges;
    for (int i = 0; i < n - 1; i++) {
        double w = weight_dist(gen);
        edges.push_back({i, i + 1, w});
        
        if (undirected) {
            edges.push_back({i + 1, i, w});
        }
    }
    
    // Path graphs are always connected by construction
    return Graph(n, edges);
}

#endif 