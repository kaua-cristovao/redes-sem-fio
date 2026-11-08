#pragma once

#include <functional>
#include <unordered_map>
#include <vector>

struct Edge {
    int destination;
    double weight;
};

struct PathResult {
    std::vector<int> vertices;
    double totalWeight;
    bool endpointsExist;
};

using Graph = std::unordered_map<int, std::vector<Edge>>;
using ShortestPathFinder = std::function<PathResult(const Graph&, int, int)>;

bool runOlsrSimulation(const Graph& physicalGraph, int source,
                       const ShortestPathFinder& findShortestPath);
