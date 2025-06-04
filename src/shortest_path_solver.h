#ifndef SHORTEST_PATH_SOLVER_H
#define SHORTEST_PATH_SOLVER_H

#include <vector>
#include "graph.h"

class ShortestPathSolver {
public:
    virtual ~ShortestPathSolver() = default;
    virtual std::vector<double> compute(const Graph &graph, int source) const = 0;
};

#endif