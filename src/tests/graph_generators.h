#ifndef GRAPH_GENERATORS_H
#define GRAPH_GENERATORS_H

#include <random>
#include <unordered_set>
#include <unordered_map>
#include <set>
#include <algorithm>
#include <queue>
#include "graph.h"

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
    while (edges.size() < m && attempts < m * 100) { // Prevent infinite loop
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
    while (edges.size() < m && attempts < m * 50) {
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
        for (int attempt = 0; attempt < edges_to_add * 3 && connected.size() < edges_to_add; attempt++) {
            for (int i = 0; i < new_vertex && connected.size() < edges_to_add; i++) {
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