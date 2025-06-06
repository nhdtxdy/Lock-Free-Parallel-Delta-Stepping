#include <iostream>
#include <fstream>
#include <unordered_map>
#include "graph.h"
#include "dijkstra.h"
#include "delta_stepping_sequential.h"
#include "correctness_checker.h"

int main() {
    // std::ios::sync_with_stdio(false);
    // std::cin.tie(0); std::cout.tie(0);
    std::cout << "Parallel Shortest Paths - Algorithm Testing" << std::endl;
    std::cout << "===========================================" << std::endl;
        
    run_all_correctness_tests();
    
    return 0;
}