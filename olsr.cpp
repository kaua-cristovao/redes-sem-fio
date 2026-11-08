#include "olsr.h"

#include <algorithm>
#include <cmath>
#include <iostream>
#include <limits>
#include <map>
#include <queue>
#include <set>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using NodeSet = std::set<int>;
using TopologyDatabase = std::map<int, std::map<int, double>>;

std::vector<int> getNodes(const Graph& graph) {
    NodeSet nodes;
    for (const auto& entry : graph) {
        nodes.insert(entry.first);
        for (const Edge& edge : entry.second) {
            nodes.insert(edge.destination);
        }
    }
    return {nodes.begin(), nodes.end()};
}

std::unordered_map<int, NodeSet> getNeighbors(const Graph& graph,
                                               const std::vector<int>& nodes) {
    std::unordered_map<int, NodeSet> neighbors;
    for (int node : nodes) {
        neighbors[node];
        const auto adjacent = graph.find(node);
        if (adjacent == graph.end()) {
            continue;
        }
        for (const Edge& edge : adjacent->second) {
            if (edge.destination != node) {
                neighbors[node].insert(edge.destination);
            }
        }
    }
    return neighbors;
}

bool findLinkWeight(const Graph& graph, int origin, int destination, double& weight) {
    const auto adjacent = graph.find(origin);
    if (adjacent == graph.end()) {
        return false;
    }
    weight = std::numeric_limits<double>::infinity();
    for (const Edge& edge : adjacent->second) {
        if (edge.destination == destination) {
            weight = std::min(weight, edge.weight);
        }
    }
    return std::isfinite(weight);
}

std::unordered_map<int, NodeSet> selectMultipointRelays(
    const std::vector<int>& nodes,
    const std::unordered_map<int, NodeSet>& neighbors) {
    std::unordered_map<int, NodeSet> selected;

    for (int node : nodes) {
        selected[node];
        NodeSet twoHopNeighbors;
        for (int neighbor : neighbors.at(node)) {
            for (int twoHop : neighbors.at(neighbor)) {
                if (twoHop != node && neighbors.at(node).count(twoHop) == 0) {
                    twoHopNeighbors.insert(twoHop);
                }
            }
        }

        NodeSet uncovered = twoHopNeighbors;
        for (int twoHop : twoHopNeighbors) {
            std::vector<int> coveringNeighbors;
            for (int neighbor : neighbors.at(node)) {
                if (neighbors.at(neighbor).count(twoHop) != 0) {
                    coveringNeighbors.push_back(neighbor);
                }
            }
            if (coveringNeighbors.size() == 1) {
                selected[node].insert(coveringNeighbors.front());
                uncovered.erase(twoHop);
            }
        }

        while (!uncovered.empty()) {
            int bestNeighbor = 0;
            std::size_t bestCoverage = 0;
            std::size_t bestDegree = 0;
            for (int neighbor : neighbors.at(node)) {
                if (selected[node].count(neighbor) != 0) {
                    continue;
                }
                std::size_t coverage = 0;
                for (int twoHop : uncovered) {
                    if (neighbors.at(neighbor).count(twoHop) != 0) {
                        ++coverage;
                    }
                }
                const std::size_t degree = neighbors.at(neighbor).size();
                if (coverage > bestCoverage ||
                    (coverage == bestCoverage && coverage > 0 &&
                     (degree > bestDegree ||
                      (degree == bestDegree && neighbor < bestNeighbor)))) {
                    bestNeighbor = neighbor;
                    bestCoverage = coverage;
                    bestDegree = degree;
                }
            }
            if (bestCoverage == 0) {
                break;
            }
            selected[node].insert(bestNeighbor);
            for (auto twoHop = uncovered.begin(); twoHop != uncovered.end();) {
                if (neighbors.at(bestNeighbor).count(*twoHop) != 0) {
                    twoHop = uncovered.erase(twoHop);
                } else {
                    ++twoHop;
                }
            }
        }
    }
    return selected;
}

void printNodeList(const NodeSet& nodes) {
    std::cout << '{';
    bool first = true;
    for (int node : nodes) {
        if (!first) {
            std::cout << ", ";
        }
        std::cout << node;
        first = false;
    }
    std::cout << '}';
}

std::string formatPath(const std::vector<int>& path) {
    std::string result;
    for (std::size_t i = 0; i < path.size(); ++i) {
        if (i != 0) {
            result += " -> ";
        }
        result += std::to_string(path[i]);
    }
    return result;
}

bool floodTopologyMessage(
    int originator, int source, const NodeSet& advertisedNeighbors,
    const std::unordered_map<int, NodeSet>& neighbors,
    const std::unordered_map<int, NodeSet>& mprSelectors,
    const Graph& physicalGraph,
    std::unordered_map<int, TopologyDatabase>& topologyDatabases) {
    std::queue<std::pair<int, int>> pending;
    NodeSet received;
    NodeSet forwarded;
    received.insert(originator);
    forwarded.insert(originator);

    std::cout << "TC do no " << originator << " anuncia MPR selectors ";
    printNodeList(advertisedNeighbors);
    std::cout << ".\n";
    for (int neighbor : neighbors.at(originator)) {
        std::cout << "  TC enviado: " << originator << " -> " << neighbor;
        if (originator == source) {
            std::cout << " [enviado pelo no origem]";
        }
        std::cout << '\n';
        pending.push({originator, neighbor});
    }

    while (!pending.empty()) {
        const int sender = pending.front().first;
        const int receiver = pending.front().second;
        pending.pop();

        const bool firstReception = received.insert(receiver).second;
        if (firstReception) {
            topologyDatabases[receiver][originator];
            for (int advertisedNeighbor : advertisedNeighbors) {
                double weight;
                if (!findLinkWeight(physicalGraph, originator, advertisedNeighbor, weight)) {
                    std::cerr << "Erro: TC anuncia enlace inexistente entre " << originator
                              << " e " << advertisedNeighbor << ".\n";
                    return false;
                }
                topologyDatabases[receiver][originator][advertisedNeighbor] = weight;
            }
        }
        std::cout << "  TC recebido: " << receiver << " <- " << sender
                  << " (originador " << originator;
        if (!firstReception) {
            std::cout << ", duplicado";
        }
        std::cout << ')';
        if (receiver == source || sender == source) {
            std::cout << " [mensagem envolvendo o no origem]";
        }
        std::cout << '\n';

        if (mprSelectors.at(receiver).count(sender) == 0 ||
            !forwarded.insert(receiver).second) {
            continue;
        }

        std::cout << "  TC retransmitido pelo no " << receiver << '\n';
        for (int neighbor : neighbors.at(receiver)) {
            pending.push({receiver, neighbor});
        }
    }
    return true;
}

}  // namespace

bool runOlsrSimulation(const Graph& physicalGraph, int source,
                       const ShortestPathFinder& findShortestPath) {
    const std::vector<int> nodes = getNodes(physicalGraph);
    if (nodes.empty()) {
        std::cerr << "Erro: nao ha nos para executar a descoberta OLSR.\n";
        return false;
    }
    if (std::find(nodes.begin(), nodes.end(), source) == nodes.end()) {
        std::cerr << "Erro: o no origem " << source << " nao existe no grafo.\n";
        return false;
    }
    if (!findShortestPath) {
        std::cerr << "Erro: algoritmo de menor caminho nao informado.\n";
        return false;
    }

    const auto neighbors = getNeighbors(physicalGraph, nodes);
    std::cout << "\n=== Descoberta de topologia OLSR (origem " << source << ") ===\n";
    std::cout << "\nFase HELLO: cada no anuncia seus vizinhos de um salto.\n";
    for (int node : nodes) {
        std::cout << "  HELLO enviado pelo no " << node << ": vizinhos ";
        printNodeList(neighbors.at(node));
        if (node == source) {
            std::cout << " [no origem]";
        }
        std::cout << '\n';
        for (int neighbor : neighbors.at(node)) {
            std::cout << "  HELLO recebido: " << neighbor << " <- " << node;
            if (neighbor == source || node == source) {
                std::cout << " [mensagem envolvendo o no origem]";
            }
            std::cout << '\n';
        }
    }

    const auto mprSets = selectMultipointRelays(nodes, neighbors);
    std::unordered_map<int, NodeSet> mprSelectors;
    for (int node : nodes) {
        mprSelectors[node];
    }
    std::cout << "\nSelecao de MPR por no:\n";
    for (int node : nodes) {
        std::cout << "  No " << node << " seleciona ";
        printNodeList(mprSets.at(node));
        std::cout << '\n';
        for (int mpr : mprSets.at(node)) {
            mprSelectors[mpr].insert(node);
        }
    }

    std::unordered_map<int, TopologyDatabase> topologyDatabases;
    for (int node : nodes) {
        topologyDatabases[node];
    }
    std::cout << "\nFase TC: disseminacao da topologia por flooding via MPR.\n";
    for (int node : nodes) {
        if (mprSelectors.at(node).empty()) {
            continue;
        }
        if (!floodTopologyMessage(node, source, mprSelectors.at(node), neighbors,
                                  mprSelectors, physicalGraph, topologyDatabases)) {
            return false;
        }
    }
    if (std::all_of(nodes.begin(), nodes.end(), [&mprSelectors](int node) {
            return mprSelectors.at(node).empty();
        })) {
        std::cout << "  Nenhum TC necessario: nenhum no foi selecionado como MPR.\n";
    }

    std::cout << "\nTabelas de roteamento (rotas calculadas com Dijkstra):\n";
    for (int node : nodes) {
        Graph discoveredTopology;
        for (int networkNode : nodes) {
            discoveredTopology[networkNode];
        }
        for (int neighbor : neighbors.at(node)) {
            double weight;
            if (!findLinkWeight(physicalGraph, node, neighbor, weight)) {
                std::cerr << "Erro: enlace HELLO inexistente entre " << node
                          << " e " << neighbor << ".\n";
                return false;
            }
            discoveredTopology[node].push_back({neighbor, weight});
        }
        for (const auto& originator : topologyDatabases.at(node)) {
            for (const auto& advertisedNeighbor : originator.second) {
                discoveredTopology[originator.first].push_back(
                    {advertisedNeighbor.first, advertisedNeighbor.second});
            }
        }

        std::cout << "\nNo " << node << ":\n";
        std::cout << "  Destino | Proximo salto | Custo | Caminho\n";
        for (int destination : nodes) {
            const PathResult route = findShortestPath(discoveredTopology, node, destination);
            if (!route.endpointsExist || route.vertices.empty()) {
                std::cout << "  " << destination << " | - | inalcancavel | -\n";
                continue;
            }
            const int nextHop = route.vertices.size() > 1 ? route.vertices[1] : node;
            std::cout << "  " << destination << " | ";
            if (destination == node) {
                std::cout << "-";
            } else {
                std::cout << nextHop;
            }
            std::cout << " | " << route.totalWeight << " | "
                      << formatPath(route.vertices) << '\n';
        }
    }
    return true;
}
