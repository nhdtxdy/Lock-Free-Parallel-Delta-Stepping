#ifndef SHORTEST_PATH_SOLVER_BASE_H
#define SHORTEST_PATH_SOLVER_BASE_H

#include <vector>
#include "graph.h"

class ShortestPathSolverBase {
public:
    virtual ~ShortestPathSolverBase() = default;
    virtual std::vector<double> compute(const Graph &graph, int source) const = 0;
    virtual const std::string name() const = 0;
};

#endif