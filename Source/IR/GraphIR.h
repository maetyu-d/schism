#pragma once

#include <optional>
#include <string>
#include <unordered_map>
#include <vector>

namespace duodsp::ir
{
enum class PortRate
{
    audio,
    control,
    event
};

struct Node
{
    std::string id;
    std::string op;
    std::string label;
    std::optional<double> literal;
};

struct Edge
{
    std::string fromNodeId;
    std::string toNodeId;
    int toPort = 0;
};

struct PortSpec
{
    std::string name;
    PortRate rate = PortRate::audio;
    bool optional = false;
};

struct OpSpec
{
    PortRate outputRate = PortRate::audio;
    std::vector<PortSpec> inputs;
    int hotInput = -1;
};

struct Graph
{
    std::vector<Node> nodes;
    std::vector<Edge> edges;
    std::unordered_map<std::string, std::string> bindings;

    const Node* findNode(const std::string& id) const;
    Node* findNode(const std::string& id);
};

enum class OpType
{
    addNode,
    removeNode,
    addEdge,
    removeEdge,
    updateNode
};

struct GraphOp
{
    OpType type = OpType::updateNode;
    std::string nodeId;
};

struct ValidationIssue
{
    std::string message;
    std::string fromNodeId;
    std::string toNodeId;
    int toPort = -1;
};

OpSpec opSpecFor(const std::string& op);
PortRate effectiveOutputRate(const Node& node);
bool isConnectionRateCompatible(PortRate outputRate, PortRate inputRate);
std::vector<ValidationIssue> validateGraph(const Graph& graph);
std::optional<ValidationIssue> validateConnection(const Graph& graph, const std::string& fromNodeId, const std::string& toNodeId, int toPort);

std::vector<GraphOp> diff(const Graph& before, const Graph& after);
} // namespace duodsp::ir
