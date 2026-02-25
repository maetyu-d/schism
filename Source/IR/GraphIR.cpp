#include "GraphIR.h"

#include <algorithm>
#include <functional>
#include <unordered_set>

namespace duodsp::ir
{
const Node* Graph::findNode(const std::string& id) const
{
    for (const auto& n : nodes)
        if (n.id == id)
            return &n;
    return nullptr;
}

Node* Graph::findNode(const std::string& id)
{
    for (auto& n : nodes)
        if (n.id == id)
            return &n;
    return nullptr;
}

OpSpec opSpecFor(const std::string& op)
{
    if (op == "constant")
        return { PortRate::control, {} };
    if (op == "floatatom")
        return { PortRate::control, { { "in", PortRate::control, true } }, 0 };
    if (op == "bang")
        return { PortRate::control, { { "trig", PortRate::any, true } }, 0 };
    if (op == "msg")
        return { PortRate::control, { { "trig", PortRate::any, true } }, 0 };
    if (op == "obj")
        return { PortRate::audio, { { "in", PortRate::audio, true } }, 0 };
    if (op == "input")
        return { PortRate::audio, {} };
    if (op == "control")
        return { PortRate::control, {} };
    if (op == "sin" || op == "saw" || op == "tri")
        return { PortRate::audio, { { "freq", PortRate::any, true } }, 0 };
    if (op == "square")
        return { PortRate::audio, { { "freq", PortRate::any, true }, { "duty", PortRate::control, true } }, 0 };
    if (op == "noise")
        return { PortRate::audio, { { "amp", PortRate::control, true } }, 0 };
    if (op == "lpf" || op == "hpf")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "cutoff", PortRate::control, true } }, 0 };
    if (op == "lores")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "cutoff", PortRate::any, true }, { "res", PortRate::control, true } }, 0 };
    if (op == "bpf")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "cutoff", PortRate::any, true }, { "q", PortRate::control, true } }, 0 };
    if (op == "svf")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "cutoff", PortRate::any, true }, { "q", PortRate::control, true }, { "mode", PortRate::control, true } }, 0 };
    if (op == "delay")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "ms", PortRate::any, true } }, 0 };
    if (op == "cdelay")
        return { PortRate::control, { { "in", PortRate::control, false }, { "ms", PortRate::control, true } }, 0 };
    if (op == "apf")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "ms", PortRate::any, true }, { "feedback", PortRate::control, true } }, 0 };
    if (op == "comb")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "ms", PortRate::any, true }, { "feedback", PortRate::control, true } }, 0 };
    if (op == "clip")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "lo", PortRate::control, true }, { "hi", PortRate::control, true } }, 0 };
    if (op == "tanh")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "drive", PortRate::control, true } }, 0 };
    if (op == "slew")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "rate", PortRate::control, true } }, 0 };
    if (op == "sah")
        return { PortRate::audio, { { "in", PortRate::audio, false }, { "trig", PortRate::any, false } }, 1 };
    if (op == "mtof")
        return { PortRate::control, { { "midi", PortRate::control, false } }, 0 };
    if (op == "mtof_sig")
        return { PortRate::audio, { { "midi", PortRate::any, false } }, 0 };
    if (op == "delay1")
        return { PortRate::audio, { { "in", PortRate::audio, false } }, 0 };
    if (op == "scope" || op == "spectrum")
        return { PortRate::audio, { { "in", PortRate::audio, false } }, 0 };
    if (op == "out")
        return { PortRate::audio, { { "left", PortRate::audio, true }, { "right", PortRate::audio, true } }, 0 };
    if (op == "comp_sig")
        return { PortRate::audio, { { "a", PortRate::audio, false }, { "b", PortRate::any, true } }, 0 };
    if (op == "comp")
        return { PortRate::control, { { "hot", PortRate::control, false }, { "cold", PortRate::control, true } }, 0 };
    if (op == "abs_sig")
        return { PortRate::audio, { { "in", PortRate::audio, false } }, 0 };
    if (op == "abs")
        return { PortRate::control, { { "in", PortRate::control, false } }, 0 };
    if (op == "random")
        return { PortRate::control, { { "trig", PortRate::any, true }, { "lo", PortRate::control, true }, { "hi", PortRate::control, true } }, 0 };
    if (op == "min_sig" || op == "max_sig")
        return { PortRate::audio, { { "a", PortRate::audio, false }, { "b", PortRate::any, true } }, 0 };
    if (op == "min" || op == "max")
        return { PortRate::control, { { "hot", PortRate::control, false }, { "cold", PortRate::control, true } }, 0 };
    if (op == "and_sig" || op == "or_sig" || op == "xor_sig")
        return { PortRate::audio, { { "a", PortRate::audio, false }, { "b", PortRate::any, true } }, 0 };
    if (op == "not_sig")
        return { PortRate::audio, { { "in", PortRate::audio, false } }, 0 };
    if (op == "and" || op == "or" || op == "xor")
        return { PortRate::control, { { "hot", PortRate::control, false }, { "cold", PortRate::control, true } }, 0 };
    if (op == "not")
        return { PortRate::control, { { "in", PortRate::control, false } }, 0 };
    if (op == "add" || op == "sub" || op == "mul")
        return { PortRate::audio, { { "a", PortRate::audio, false }, { "b", PortRate::any, true } }, 0 };
    if (op == "div" || op == "pow" || op == "mod")
        return { PortRate::audio, { { "a", PortRate::audio, false }, { "b", PortRate::audio, false } }, 0 };
    if (op == "cadd" || op == "csub" || op == "cmul" || op == "cdiv")
        return { PortRate::control, { { "hot", PortRate::control, false }, { "cold", PortRate::control, false } }, 0 };
    return { PortRate::audio, { { "in", PortRate::audio, true } } };
}

PortRate effectiveOutputRate(const Node& node)
{
    if (node.op == "constant")
        return PortRate::control;
    return opSpecFor(node.op).outputRate;
}

bool isConnectionRateCompatible(const PortRate outputRate, const PortRate inputRate)
{
    if (inputRate == PortRate::any)
        return outputRate == PortRate::audio || outputRate == PortRate::control;
    if (inputRate == outputRate)
        return true;
    // Allow control -> audio for modulation convenience; keep audio -> control forbidden.
    if (inputRate == PortRate::audio && outputRate == PortRate::control)
        return true;
    return false;
}

static bool hasCycleIgnoringDelay(const Graph& graph)
{
    std::unordered_set<std::string> nonDelay;
    for (const auto& n : graph.nodes)
        if (n.op != "delay1" && n.op != "delay" && n.op != "cdelay" && n.op != "apf" && n.op != "comb")
            nonDelay.insert(n.id);

    std::unordered_map<std::string, std::vector<std::string>> adjacency;
    for (const auto& e : graph.edges)
    {
        if (!nonDelay.contains(e.fromNodeId) || !nonDelay.contains(e.toNodeId))
            continue;
        adjacency[e.fromNodeId].push_back(e.toNodeId);
    }

    enum class Mark
    {
        none,
        visiting,
        done
    };
    std::unordered_map<std::string, Mark> marks;
    for (const auto& id : nonDelay)
        marks[id] = Mark::none;

    std::function<bool(const std::string&)> dfs = [&](const std::string& id)
    {
        const auto mark = marks[id];
        if (mark == Mark::visiting)
            return true;
        if (mark == Mark::done)
            return false;
        marks[id] = Mark::visiting;
        for (const auto& to : adjacency[id])
            if (dfs(to))
                return true;
        marks[id] = Mark::done;
        return false;
    };

    for (const auto& id : nonDelay)
        if (marks[id] == Mark::none && dfs(id))
            return true;

    return false;
}

std::vector<ValidationIssue> validateGraph(const Graph& graph)
{
    std::vector<ValidationIssue> issues;

    std::unordered_set<std::string> nodeIds;
    for (const auto& node : graph.nodes)
        nodeIds.insert(node.id);

    for (const auto& edge : graph.edges)
    {
        const auto* from = graph.findNode(edge.fromNodeId);
        const auto* to = graph.findNode(edge.toNodeId);
        if (from == nullptr || to == nullptr)
        {
            issues.push_back({ "Edge endpoint does not exist.", edge.fromNodeId, edge.toNodeId, edge.toPort });
            continue;
        }

        const auto spec = opSpecFor(to->op);
        if (edge.toPort < 0 || edge.toPort >= static_cast<int>(spec.inputs.size()))
        {
            issues.push_back({ "Target port index out of range for node type.", edge.fromNodeId, edge.toNodeId, edge.toPort });
            continue;
        }

        const auto inRate = spec.inputs[static_cast<size_t>(edge.toPort)].rate;
        const auto outRate = effectiveOutputRate(*from);
        if (!isConnectionRateCompatible(outRate, inRate))
        {
            issues.push_back({ "Port rate mismatch (typed connection rejected).", edge.fromNodeId, edge.toNodeId, edge.toPort });
            continue;
        }
    }

    if (hasCycleIgnoringDelay(graph))
        issues.push_back({ "Illegal feedback cycle detected. Insert delay1 node in each feedback loop.", "", "", -1 });

    return issues;
}

std::optional<ValidationIssue> validateConnection(const Graph& graph, const std::string& fromNodeId, const std::string& toNodeId, const int toPort)
{
    auto draft = graph;
    draft.edges.erase(std::remove_if(draft.edges.begin(), draft.edges.end(), [&](const auto& e)
                     { return e.toNodeId == toNodeId && e.toPort == toPort; }),
                      draft.edges.end());
    draft.edges.push_back({ fromNodeId, toNodeId, toPort });
    const auto issues = validateGraph(draft);
    if (!issues.empty())
        return issues.front();
    return std::nullopt;
}

static std::string edgeKey(const Edge& e)
{
    return e.fromNodeId + "->" + e.toNodeId + ":" + std::to_string(e.toPort);
}

std::vector<GraphOp> diff(const Graph& before, const Graph& after)
{
    std::vector<GraphOp> ops;

    std::unordered_set<std::string> beforeNodes;
    for (const auto& n : before.nodes)
        beforeNodes.insert(n.id);

    std::unordered_set<std::string> afterNodes;
    for (const auto& n : after.nodes)
        afterNodes.insert(n.id);

    for (const auto& n : before.nodes)
        if (!afterNodes.contains(n.id))
            ops.push_back({ OpType::removeNode, n.id });

    for (const auto& n : after.nodes)
        if (!beforeNodes.contains(n.id))
            ops.push_back({ OpType::addNode, n.id });

    std::unordered_set<std::string> beforeEdges;
    for (const auto& e : before.edges)
        beforeEdges.insert(edgeKey(e));

    std::unordered_set<std::string> afterEdges;
    for (const auto& e : after.edges)
        afterEdges.insert(edgeKey(e));

    for (const auto& e : before.edges)
        if (!afterEdges.contains(edgeKey(e)))
            ops.push_back({ OpType::removeEdge, e.fromNodeId });

    for (const auto& e : after.edges)
        if (!beforeEdges.contains(edgeKey(e)))
            ops.push_back({ OpType::addEdge, e.toNodeId });

    return ops;
}
} // namespace duodsp::ir
