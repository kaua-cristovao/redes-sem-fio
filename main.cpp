#include <cmath>
#include <algorithm>
#include <fstream>
#include <functional>
#include <iostream>
#include <limits>
#include <queue>
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>
#include "olsr.h"

struct Link {
    int origin;
    int destination;
    double weight;
};

struct Point {
    double x;
    double y;
};

bool generateRandomGraph(int nodeCount, const std::string& filename) {
    if (nodeCount < 1) {
        std::cerr << "Erro: o numero de nos deve ser maior que zero.\n";
        return false;
    }

    std::ofstream output(filename);
    if (!output) {
        std::cerr << "Erro: nao foi possivel criar o arquivo \"" << filename << "\".\n";
        return false;
    }

    std::random_device randomDevice;
    std::mt19937 generator(randomDevice());
    std::bernoulli_distribution addExtraEdge(0.3);

    if (nodeCount == 1) {
        output << "1 1 1\n";
    } else {
        std::vector<int> parent(nodeCount + 1, 0);
        for (int node = 2; node <= nodeCount; ++node) {
            std::uniform_int_distribution<int> chooseParent(1, node - 1);
            parent[node] = chooseParent(generator);
            output << node << ' ' << parent[node] << " 1\n";
        }

        for (int origin = 1; origin <= nodeCount; ++origin) {
            for (int destination = origin + 1; destination <= nodeCount; ++destination) {
                if (parent[destination] != origin && addExtraEdge(generator)) {
                    output << origin << ' ' << destination << " 1\n";
                }
            }
        }
    }

    if (!output) {
        std::cerr << "Erro: falha ao gravar o arquivo \"" << filename << "\".\n";
        return false;
    }
    return true;
}

bool loadGraph(const std::string& filename, Graph& graph, std::vector<Link>& links) {
    std::ifstream input(filename);
    if (!input) {
        std::cerr << "Erro: nao foi possivel abrir o arquivo \"" << filename << "\".\n";
        return false;
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        std::istringstream row(line);
        row >> std::ws;
        if (row.eof()) {
            continue;
        }

        int origin;
        int destination;
        double weight;
        std::string extra;
        if (!(row >> origin >> destination >> weight) || (row >> extra) ||
            !std::isfinite(weight) || weight < 0) {
            std::cerr << "Erro: linha " << lineNumber
                      << " invalida. Use: ORIGEM DESTINO PESO, com peso nao negativo.\n";
            return false;
        }

        graph[origin].push_back({destination, weight});
        graph[destination].push_back({origin, weight});
        links.push_back({origin, destination, weight});
    }

    if (graph.empty()) {
        std::cerr << "Erro: o arquivo nao contem enlaces.\n";
        return false;
    }
    return true;
}

void printGraph(const std::vector<Link>& links) {
    std::cout << "\nGrafo de entrada (nao direcionado):\n";
    for (const Link& link : links) {
        std::cout << "(" << link.origin << ") --[" << link.weight << "]-- ("
                  << link.destination << ")\n";
    }
    std::cout << '\n';
}

bool generateGraphSvg(const Graph& graph, const std::vector<Link>& links,
                      const std::string& filename) {
    std::vector<int> vertices;
    vertices.reserve(graph.size());
    for (const auto& vertex : graph) {
        vertices.push_back(vertex.first);
    }
    std::sort(vertices.begin(), vertices.end());

    const double radius = std::max(180.0, static_cast<double>(vertices.size()) * 32.0);
    const double center = radius + 70.0;
    const double canvasSize = center * 2.0;
    const double nodeRadius = 24.0;
    const double pi = std::acos(-1.0);
    std::unordered_map<int, Point> positions;
    for (std::size_t i = 0; i < vertices.size(); ++i) {
        const double angle = -pi / 2.0 + 2.0 * pi * static_cast<double>(i) /
                                                   static_cast<double>(vertices.size());
        positions[vertices[i]] = {
            center + radius * std::cos(angle),
            center + radius * std::sin(angle)
        };
    }

    std::ofstream output(filename);
    if (!output) {
        std::cerr << "Erro: nao foi possivel criar o arquivo SVG \"" << filename << "\".\n";
        return false;
    }

    output << "<svg xmlns=\"http://www.w3.org/2000/svg\" width=\"" << canvasSize
           << "\" height=\"" << canvasSize << "\" viewBox=\"0 0 " << canvasSize
           << ' ' << canvasSize << "\">\n"
           << "  <rect width=\"100%\" height=\"100%\" fill=\"#f8fafc\"/>\n"
           << "  <text x=\"" << center
           << "\" y=\"32\" text-anchor=\"middle\" font-family=\"Arial, sans-serif\" "
              "font-size=\"20\" font-weight=\"bold\" fill=\"#1e293b\">Grafo de entrada</text>\n";

    for (const Link& link : links) {
        const Point& origin = positions.at(link.origin);
        const Point& destination = positions.at(link.destination);
        output << "  <g>\n";
        if (link.origin == link.destination) {
            output << "    <path d=\"M " << origin.x - 12 << ' ' << origin.y - 20
                   << " C " << origin.x - 55 << ' ' << origin.y - 85 << ", "
                   << origin.x + 55 << ' ' << origin.y - 85 << ", "
                   << origin.x + 12 << ' ' << origin.y - 20
                   << "\" fill=\"none\" stroke=\"#64748b\" stroke-width=\"2\"/>\n"
                   << "    <text x=\"" << origin.x << "\" y=\"" << origin.y - 73
                   << "\" text-anchor=\"middle\" font-family=\"Arial, sans-serif\" "
                      "font-size=\"13\" fill=\"#334155\">"
                   << link.weight << "</text>\n";
        } else {
            const double middleX = (origin.x + destination.x) / 2.0;
            const double middleY = (origin.y + destination.y) / 2.0;
            output << "    <line x1=\"" << origin.x << "\" y1=\"" << origin.y
                   << "\" x2=\"" << destination.x << "\" y2=\"" << destination.y
                   << "\" stroke=\"#64748b\" stroke-width=\"2\"/>\n"
                   << "    <rect x=\"" << middleX - 18 << "\" y=\"" << middleY - 11
                   << "\" width=\"36\" height=\"22\" rx=\"6\" fill=\"#f8fafc\"/>\n"
                   << "    <text x=\"" << middleX << "\" y=\"" << middleY + 5
                   << "\" text-anchor=\"middle\" font-family=\"Arial, sans-serif\" "
                      "font-size=\"13\" fill=\"#334155\">"
                   << link.weight << "</text>\n";
        }
        output << "  </g>\n";
    }

    for (int vertex : vertices) {
        const Point& position = positions.at(vertex);
        output << "  <circle cx=\"" << position.x << "\" cy=\"" << position.y
               << "\" r=\"" << nodeRadius
               << "\" fill=\"#2563eb\" stroke=\"#1e40af\" stroke-width=\"2\"/>\n"
               << "  <text x=\"" << position.x << "\" y=\"" << position.y + 5
               << "\" text-anchor=\"middle\" font-family=\"Arial, sans-serif\" "
                  "font-size=\"15\" font-weight=\"bold\" fill=\"white\">"
               << vertex << "</text>\n";
    }
    output << "</svg>\n";

    if (!output) {
        std::cerr << "Erro: falha ao gravar o arquivo SVG \"" << filename << "\".\n";
        return false;
    }
    return true;
}

PathResult findShortestPath(const Graph& graph, int origin, int destination) {
    if (graph.find(origin) == graph.end() || graph.find(destination) == graph.end()) {
        return {{}, 0, false};
    }

    const double infinity = std::numeric_limits<double>::infinity();
    std::unordered_map<int, double> distances;
    std::unordered_map<int, int> previous;
    for (const auto& vertex : graph) {
        distances[vertex.first] = infinity;
    }
    distances[origin] = 0;

    using QueueEntry = std::pair<double, int>;
    std::priority_queue<QueueEntry, std::vector<QueueEntry>,
                        std::greater<QueueEntry>> pending;
    pending.push({0, origin});

    while (!pending.empty()) {
        const double currentDistance = pending.top().first;
        const int current = pending.top().second;
        pending.pop();

        if (currentDistance > distances[current]) {
            continue;
        }
        if (current == destination) {
            break;
        }

        for (const Edge& edge : graph.at(current)) {
            const double newDistance = currentDistance + edge.weight;
            if (newDistance < distances[edge.destination]) {
                distances[edge.destination] = newDistance;
                previous[edge.destination] = current;
                pending.push({newDistance, edge.destination});
            }
        }
    }
    if (!std::isfinite(distances[destination])) {
        return {{}, infinity, true};
    }

    std::vector<int> path;
    for (int vertex = destination;; vertex = previous.at(vertex)) {
        path.push_back(vertex);
        if (vertex == origin) {
            break;
        }
    }
    std::reverse(path.begin(), path.end());
    return {path, distances[destination], true};
}

void printShortestPath(const PathResult& result, int origin, int destination) {
    if (!result.endpointsExist) {
        std::cout << "Origem ou destino nao encontrado no grafo.\n";
        return;
    }
    if (result.vertices.empty()) {
        std::cout << "Nao existe caminho entre " << origin << " e " << destination << ".\n";
        return;
    }

    std::cout << "Menor caminho: ";
    for (std::size_t i = 0; i < result.vertices.size(); ++i) {
        std::cout << result.vertices[i];
        if (i + 1 < result.vertices.size()) {
            std::cout << " -> ";
        }
    }
    std::cout << "\nPeso total: " << result.totalWeight << "\n";
}

int main(int argc, char* argv[]) {
    if (argc >= 2 && std::string(argv[1]) == "--gerar") {
        if (argc < 3 || argc > 4) {
            std::cerr << "Uso: " << argv[0] << " --gerar NUM_NOS [arquivo.txt]\n";
            return 1;
        }

        std::istringstream nodeCountInput(argv[2]);
        int nodeCount;
        std::string extra;
        if (!(nodeCountInput >> nodeCount) || (nodeCountInput >> extra) || nodeCount < 1) {
            std::cerr << "Erro: informe um numero de nos inteiro maior que zero.\n";
            return 1;
        }

        const std::string filename = argc == 4 ? argv[3] : "grafo_aleatorio.txt";
        if (!generateRandomGraph(nodeCount, filename)) {
            return 1;
        }
        std::cout << "Grafo aleatorio com " << nodeCount << " nos salvo em \""
                  << filename << "\".\n";
        return 0;
    }

    bool runOlsr = false;
    int olsrSource = 0;
    std::string filename;
    if (argc >= 2 && std::string(argv[1]) == "--olsr") {
        if (argc < 3 || argc > 4) {
            std::cerr << "Uso: " << argv[0] << " --olsr NO_ORIGEM [arquivo.txt]\n";
            return 1;
        }
        std::istringstream sourceInput(argv[2]);
        std::string extra;
        if (!(sourceInput >> olsrSource) || (sourceInput >> extra)) {
            std::cerr << "Erro: informe um identificador inteiro para o no origem.\n";
            return 1;
        }
        runOlsr = true;
        filename = argc == 4 ? argv[3] : "grafo_aleatorio.txt";
    } else if (argc > 2) {
        std::cerr << "Uso: " << argv[0] << " [arquivo.txt] ou " << argv[0]
                  << " --gerar NUM_NOS [arquivo.txt] ou " << argv[0]
                  << " --olsr NO_ORIGEM [arquivo.txt]\n";
        return 1;
    } else if (argc == 2) {
        filename = argv[1];
    } else {
        std::cout << "Arquivo com os enlaces: ";
        if (!std::getline(std::cin, filename) || filename.empty()) {
            std::cerr << "Erro: nome de arquivo nao informado.\n";
            return 1;
        }
    }

    Graph graph;
    std::vector<Link> links;
    if (!loadGraph(filename, graph, links)) {
        return 1;
    }
    printGraph(links);
    if (!generateGraphSvg(graph, links, "grafo.svg")) {
        return 1;
    }
    std::cout << "Representacao SVG criada em \"grafo.svg\".\n\n";

    if (runOlsr &&
        !runOlsrSimulation(graph, olsrSource, findShortestPath)) {
        return 1;
    }
    if (runOlsr) {
        return 0;
    }

    while (true) {
        std::string input;
        std::cout << "Origem e destino (ou 'sair' para encerrar): ";
        if (!std::getline(std::cin, input)) {
            break;
        }

        if (input == "sair" || input == "Sair" || input == "SAIR") {
            break;
        }

        std::istringstream query(input);
        int origin;
        int destination;
        std::string extra;
        if (!(query >> origin >> destination) || (query >> extra)) {
            std::cout << "Entrada invalida. Informe dois pontos, por exemplo: 1 4.\n";
            continue;
        }
        const PathResult result = findShortestPath(graph, origin, destination);
        printShortestPath(result, origin, destination);
    }

    return 0;
}
