#include "RuntimeEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <cstdlib>
#include <limits>
#include <queue>
#include <sstream>

namespace duodsp::dsp
{
namespace
{
std::vector<float> readRingSnapshot(const std::vector<float>& ring, const int write, const size_t maxSamples)
{
    std::vector<float> out;
    if (ring.empty() || maxSamples == 0)
        return out;

    const auto count = std::min(maxSamples, ring.size());
    out.resize(count, 0.0f);
    for (size_t i = 0; i < count; ++i)
    {
        const auto src = (write - static_cast<int>(count) + static_cast<int>(i));
        const auto wrapped = ((src % static_cast<int>(ring.size())) + static_cast<int>(ring.size())) % static_cast<int>(ring.size());
        out[i] = ring[static_cast<size_t>(wrapped)];
    }
    return out;
}

std::vector<float> spectrumFromSamples(const std::vector<float>& samples, const size_t bins)
{
    if (bins == 0)
        return {};
    if (samples.empty())
        return std::vector<float>(bins, 0.0f);

    std::vector<float> out(bins, 0.0f);
    const auto n = static_cast<int>(samples.size());
    for (size_t k = 0; k < bins; ++k)
    {
        std::complex<double> sum(0.0, 0.0);
        const auto bin = static_cast<double>(k + 1) / static_cast<double>(bins);
        const auto freqIndex = bin * static_cast<double>(n / 2);
        for (int i = 0; i < n; ++i)
        {
            const auto angle = -2.0 * juce::MathConstants<double>::pi * freqIndex * static_cast<double>(i) / static_cast<double>(n);
            sum += std::complex<double>(std::cos(angle), std::sin(angle)) * static_cast<double>(samples[static_cast<size_t>(i)]);
        }
        out[k] = static_cast<float>(juce::jlimit(0.0, 1.0, std::abs(sum) / static_cast<double>(n) * 6.0));
    }
    return out;
}

std::vector<float> parseLabelNumbers(const std::string& label)
{
    std::vector<float> vals;
    if (label.empty())
        return vals;

    std::istringstream iss(label);
    std::string tok;
    while (iss >> tok)
    {
        char* end = nullptr;
        const auto v = std::strtof(tok.c_str(), &end);
        if (end != tok.c_str() && end != nullptr && *end == '\0')
            vals.push_back(v);
    }
    if (!vals.empty())
        return vals;

    // Support numeric-only labels like "440" for quick Pd-style value entry.
    char* end = nullptr;
    const auto v = std::strtof(label.c_str(), &end);
    if (end != label.c_str() && end != nullptr && *end == '\0')
        vals.push_back(v);
    return vals;
}
} // namespace

RuntimeEngine::RuntimeEngine() = default;
RuntimeEngine::~RuntimeEngine() = default;

void RuntimeEngine::prepare(const double sampleRate, const int blockSize, const int outputChannels)
{
    currentSampleRate = sampleRate;
    currentBlockSize = blockSize;
    currentOutputChannels = outputChannels;
    scopeRing.assign(4096, 0.0f);
    scopeWriteIndex.store(0);
}

void RuntimeEngine::setCrossfadeTimeMs(const double ms)
{
    crossfadeMs = juce::jmax(0.0, ms);
}

std::unique_ptr<RuntimeEngine::CompiledGraph> RuntimeEngine::compileGraph(const ir::Graph& graph) const
{
    auto compiled = std::make_unique<CompiledGraph>();

    for (size_t i = 0; i < graph.nodes.size(); ++i)
    {
        RuntimeNode rn;
        rn.node = graph.nodes[i];
        rn.paramDefaults = parseLabelNumbers(rn.node.label);
        if (rn.node.op == "cadd" || rn.node.op == "csub" || rn.node.op == "cmul" || rn.node.op == "cdiv" || rn.node.op == "comp"
            || rn.node.op == "and" || rn.node.op == "or" || rn.node.op == "xor" || rn.node.op == "min" || rn.node.op == "max")
            rn.stateB = std::numeric_limits<float>::quiet_NaN(); // last hot input for Pd-like hot/cold behavior
        if (rn.node.op == "floatatom")
            rn.stateB = std::numeric_limits<float>::quiet_NaN(); // lazy-init marker for stored value
        if (rn.node.op == "delay" || rn.node.op == "cdelay" || rn.node.op == "comb" || rn.node.op == "apf")
        {
            const auto maxDelaySamples = juce::jmax(64, static_cast<int>(std::round(currentSampleRate * 5.0)));
            rn.delayBuffer.assign(static_cast<size_t>(maxDelaySamples), 0.0f);
            rn.delayWriteIndex = 0;
        }
        compiled->indexByNodeId[rn.node.id] = i;
        compiled->nodes.push_back(std::move(rn));
    }

    for (const auto& e : graph.edges)
    {
        if (!compiled->indexByNodeId.contains(e.toNodeId))
            continue;
        auto& target = compiled->nodes[compiled->indexByNodeId[e.toNodeId]];
        if (static_cast<int>(target.inputIndices.size()) <= e.toPort)
            target.inputIndices.resize(static_cast<size_t>(e.toPort + 1), -1);
        if (compiled->indexByNodeId.contains(e.fromNodeId))
            target.inputIndices[static_cast<size_t>(e.toPort)] = static_cast<int>(compiled->indexByNodeId[e.fromNodeId]);
    }

    for (size_t i = 0; i < compiled->nodes.size(); ++i)
    {
        const auto& op = compiled->nodes[i].node.op;
        if (op == "out")
            compiled->outputNodes.push_back(i);
        else if (op == "scope")
            compiled->scopeProbeNodes.push_back({ compiled->nodes[i].node.id, i });
        else if (op == "spectrum")
            compiled->spectrumProbeNodes.push_back({ compiled->nodes[i].node.id, i });
    }

    // Build execution order. delay1 is a sample-delay break, so its input edge
    // should not constrain same-sample scheduling.
    std::vector<int> indegree(compiled->nodes.size(), 0);
    std::vector<std::vector<size_t>> adjacency(compiled->nodes.size());
    for (const auto& e : graph.edges)
    {
        if (!compiled->indexByNodeId.contains(e.fromNodeId) || !compiled->indexByNodeId.contains(e.toNodeId))
            continue;
        const auto from = compiled->indexByNodeId[e.fromNodeId];
        const auto to = compiled->indexByNodeId[e.toNodeId];
        if (compiled->nodes[to].node.op == "delay1")
            continue;
        adjacency[from].push_back(to);
        indegree[to] += 1;
    }

    std::queue<size_t> q;
    for (size_t i = 0; i < indegree.size(); ++i)
        if (indegree[i] == 0)
            q.push(i);
    while (!q.empty())
    {
        const auto n = q.front();
        q.pop();
        compiled->executionOrder.push_back(n);
        for (const auto to : adjacency[n])
        {
            --indegree[to];
            if (indegree[to] == 0)
                q.push(to);
        }
    }
    if (compiled->executionOrder.size() != compiled->nodes.size())
    {
        compiled->executionOrder.clear();
        for (size_t i = 0; i < compiled->nodes.size(); ++i)
            compiled->executionOrder.push_back(i);
    }

    return compiled;
}

void RuntimeEngine::transferState(const CompiledGraph* from, CompiledGraph& to)
{
    if (from == nullptr)
        return;

    for (auto& toNode : to.nodes)
    {
        if (!from->indexByNodeId.contains(toNode.node.id))
            continue;
        const auto& fromNode = from->nodes[from->indexByNodeId.at(toNode.node.id)];
        if (fromNode.node.op == toNode.node.op)
        {
            toNode.stateA = fromNode.stateA;
            toNode.stateB = fromNode.stateB;
            toNode.stateC = fromNode.stateC;
            toNode.stateD = fromNode.stateD;
            if (!toNode.delayBuffer.empty() && !fromNode.delayBuffer.empty())
            {
                const auto copySize = juce::jmin(toNode.delayBuffer.size(), fromNode.delayBuffer.size());
                std::copy_n(fromNode.delayBuffer.begin(), copySize, toNode.delayBuffer.begin());
                toNode.delayWriteIndex = fromNode.delayWriteIndex % static_cast<int>(toNode.delayBuffer.size());
            }
        }
    }
}

void RuntimeEngine::setGraph(const ir::Graph& graph)
{
    auto compiled = compileGraph(graph);
    const juce::ScopedLock lock(graphLock);

    transferState(currentGraph.get(), *compiled);
    if (currentGraph != nullptr)
    {
        fadingOutGraph = std::move(currentGraph);
        crossfadeTotalSamples = juce::jmax(1, static_cast<int>(std::round(crossfadeMs * 0.001 * currentSampleRate)));
        crossfadeRemainingSamples = crossfadeTotalSamples;
    }
    else
    {
        fadingOutGraph.reset();
        crossfadeRemainingSamples = 0;
        crossfadeTotalSamples = 0;
    }
    currentGraph = std::move(compiled);

    // Keep probe rings keyed by stable node IDs.
    std::unordered_map<std::string, std::vector<float>> nextScopeRings;
    std::unordered_map<std::string, int> nextScopeWrites;
    for (const auto& [id, _] : currentGraph->scopeProbeNodes)
    {
        juce::ignoreUnused(_);
        if (scopeProbeRings.contains(id))
            nextScopeRings[id] = scopeProbeRings[id];
        else
            nextScopeRings[id] = std::vector<float>(4096, 0.0f);
        nextScopeWrites[id] = scopeProbeWriteIndices.contains(id) ? scopeProbeWriteIndices[id] : 0;
    }

    std::unordered_map<std::string, std::vector<float>> nextSpectrumRings;
    std::unordered_map<std::string, int> nextSpectrumWrites;
    for (const auto& [id, _] : currentGraph->spectrumProbeNodes)
    {
        juce::ignoreUnused(_);
        if (spectrumProbeRings.contains(id))
            nextSpectrumRings[id] = spectrumProbeRings[id];
        else
            nextSpectrumRings[id] = std::vector<float>(4096, 0.0f);
        nextSpectrumWrites[id] = spectrumProbeWriteIndices.contains(id) ? spectrumProbeWriteIndices[id] : 0;
    }

    scopeProbeRings = std::move(nextScopeRings);
    scopeProbeWriteIndices = std::move(nextScopeWrites);
    spectrumProbeRings = std::move(nextSpectrumRings);
    spectrumProbeWriteIndices = std::move(nextSpectrumWrites);
}

void RuntimeEngine::triggerBang(const std::string& nodeId)
{
    const juce::ScopedLock lock(graphLock);
    if (currentGraph == nullptr || !currentGraph->indexByNodeId.contains(nodeId))
        return;
    auto& n = currentGraph->nodes[currentGraph->indexByNodeId[nodeId]];
    if (n.node.op == "bang")
        n.stateA = 1.0f;
    else if (n.node.op == "msg")
        n.stateC = 1.0f;
}

std::vector<std::string> RuntimeEngine::consumeTriggeredBangIds()
{
    std::vector<std::string> ids;
    const juce::ScopedTryLock lock(graphLock);
    if (!lock.isLocked() || currentGraph == nullptr)
        return ids;

    for (auto& n : currentGraph->nodes)
    {
        if (n.node.op != "bang")
            continue;
        if (n.stateC > 0.5f)
        {
            ids.push_back(n.node.id);
            n.stateC = 0.0f;
        }
    }
    return ids;
}

float RuntimeEngine::runNode(RuntimeNode& n, const std::vector<float>& values, const int sampleIndex, const double sampleRate, juce::Random& random)
{
    auto inputAt = [&](const size_t idx, const float fallback = 0.0f) -> float
    {
        if (idx >= n.inputIndices.size())
            return fallback;
        const auto inputIndex = n.inputIndices[idx];
        if (inputIndex < 0 || static_cast<size_t>(inputIndex) >= values.size())
            return fallback;
        return values[static_cast<size_t>(inputIndex)];
    };
    auto defaultAt = [&](const size_t idx, const float fallback) -> float
    {
        if (idx < n.paramDefaults.size())
            return n.paramDefaults[idx];
        return fallback;
    };

    if (n.node.op == "constant")
        return static_cast<float>(n.node.literal.value_or(0.0));
    if (n.node.op == "floatatom")
    {
        if (!std::isfinite(n.stateB))
        {
            n.stateA = static_cast<float>(n.node.literal.value_or(0.0));
            n.stateB = 0.0f;
        }
        const auto hasInput = !n.inputIndices.empty() && n.inputIndices[0] >= 0;
        if (hasInput)
            n.stateA = inputAt(0, n.stateA);
        return n.stateA;
    }
    if (n.node.op == "msg")
    {
        const auto trigIn = inputAt(0, 0.0f);
        const auto rising = trigIn > 0.5f && n.stateB <= 0.5f;
        const auto clicked = n.stateC > 0.5f;
        n.stateB = trigIn;
        n.stateC = 0.0f;
        if (rising || clicked)
            n.stateA = defaultAt(0, 0.0f);
        return n.stateA;
    }
    if (n.node.op == "bang")
    {
        const auto trigIn = inputAt(0, 0.0f);
        const auto rising = trigIn > 0.5f && n.stateB <= 0.5f;
        const auto out = (n.stateA > 0.5f || rising) ? 1.0f : 0.0f;
        if (out > 0.5f)
            n.stateC = 1.0f;
        n.stateA = 0.0f;
        n.stateB = trigIn;
        return out;
    }
    if (n.node.op == "random")
    {
        const auto trig = inputAt(0, 0.0f);
        const auto rising = trig > 0.5f && n.stateB <= 0.5f;
        if (rising)
        {
            auto lo = inputAt(1, defaultAt(0, 0.0f));
            auto hi = inputAt(2, defaultAt(1, 1.0f));
            if (hi < lo)
                std::swap(lo, hi);
            n.stateA = lo + (hi - lo) * random.nextFloat();
        }
        n.stateB = trig;
        return n.stateA;
    }
    if (n.node.op == "obj")
        return inputAt(0);
    if (n.node.op == "input" || n.node.op == "control")
        return 0.0f;
    if (n.node.op == "add")
        return inputAt(0) + inputAt(1, defaultAt(0, 0.0f));
    if (n.node.op == "sub")
        return inputAt(0) - inputAt(1, defaultAt(0, 0.0f));
    if (n.node.op == "mul")
        return inputAt(0) * inputAt(1, defaultAt(0, 1.0f));
    if (n.node.op == "div")
    {
        const auto d = inputAt(1, defaultAt(0, 1.0f));
        return d == 0.0f ? 0.0f : inputAt(0) / d;
    }
    if (n.node.op == "comp_sig")
        return inputAt(0) > inputAt(1, defaultAt(0, 0.0f)) ? 1.0f : 0.0f;
    if (n.node.op == "abs_sig")
        return std::abs(inputAt(0));
    if (n.node.op == "min_sig")
        return juce::jmin(inputAt(0), inputAt(1, defaultAt(0, 0.0f)));
    if (n.node.op == "max_sig")
        return juce::jmax(inputAt(0), inputAt(1, defaultAt(0, 0.0f)));
    if (n.node.op == "and_sig")
        return (inputAt(0) > 0.5f && inputAt(1, defaultAt(0, 0.0f)) > 0.5f) ? 1.0f : 0.0f;
    if (n.node.op == "or_sig")
        return (inputAt(0) > 0.5f || inputAt(1, defaultAt(0, 0.0f)) > 0.5f) ? 1.0f : 0.0f;
    if (n.node.op == "xor_sig")
        return ((inputAt(0) > 0.5f) != (inputAt(1, defaultAt(0, 0.0f)) > 0.5f)) ? 1.0f : 0.0f;
    if (n.node.op == "not_sig")
        return inputAt(0) > 0.5f ? 0.0f : 1.0f;
    if (n.node.op == "comp")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = hot > cold ? 1.0f : 0.0f;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "abs")
        return std::abs(inputAt(0));
    if (n.node.op == "min")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = juce::jmin(hot, cold);
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "max")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = juce::jmax(hot, cold);
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "and")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = (hot > 0.5f && cold > 0.5f) ? 1.0f : 0.0f;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "or")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = (hot > 0.5f || cold > 0.5f) ? 1.0f : 0.0f;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "xor")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = ((hot > 0.5f) != (cold > 0.5f)) ? 1.0f : 0.0f;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "not")
        return inputAt(0) > 0.5f ? 0.0f : 1.0f;
    if (n.node.op == "cadd")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = hot + cold;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "csub")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 0.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = hot - cold;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "cmul")
    {
        const auto hot = inputAt(0);
        const auto cold = inputAt(1, defaultAt(0, 1.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = hot * cold;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "cdiv")
    {
        const auto hot = inputAt(0);
        const auto d = inputAt(1, defaultAt(0, 1.0f));
        if (!std::isfinite(n.stateB) || std::abs(hot - n.stateB) > 1.0e-6f)
        {
            n.stateA = d == 0.0f ? 0.0f : hot / d;
            n.stateB = hot;
        }
        return n.stateA;
    }
    if (n.node.op == "pow")
        return std::pow(inputAt(0), inputAt(1, defaultAt(0, 1.0f)));
    if (n.node.op == "mod")
    {
        const auto d = inputAt(1, defaultAt(0, 1.0f));
        return d == 0.0f ? 0.0f : std::fmod(inputAt(0), d);
    }
    if (n.node.op == "sin")
    {
        const auto freq = juce::jmax(1.0f, inputAt(0, defaultAt(0, 220.0f)));
        n.stateA += freq / static_cast<float>(sampleRate);
        n.stateA -= std::floor(n.stateA);
        return std::sin(n.stateA * 2.0f * juce::MathConstants<float>::pi);
    }
    if (n.node.op == "saw")
    {
        const auto freq = juce::jmax(1.0f, inputAt(0, defaultAt(0, 110.0f)));
        n.stateA += freq / static_cast<float>(sampleRate);
        n.stateA -= std::floor(n.stateA);
        return n.stateA * 2.0f - 1.0f;
    }
    if (n.node.op == "tri")
    {
        const auto freq = juce::jmax(1.0f, inputAt(0, defaultAt(0, 110.0f)));
        n.stateA += freq / static_cast<float>(sampleRate);
        n.stateA -= std::floor(n.stateA);
        return 4.0f * std::fabs(n.stateA - 0.5f) - 1.0f;
    }
    if (n.node.op == "square")
    {
        const auto freq = juce::jmax(1.0f, inputAt(0, defaultAt(0, 110.0f)));
        const auto duty = juce::jlimit(0.01f, 0.99f, inputAt(1, defaultAt(1, 0.5f)));
        n.stateA += freq / static_cast<float>(sampleRate);
        n.stateA -= std::floor(n.stateA);
        return n.stateA < duty ? 1.0f : -1.0f;
    }
    if (n.node.op == "noise")
    {
        const auto amp = inputAt(0, defaultAt(0, 0.05f));
        return random.nextFloat() * 2.0f * amp - amp;
    }
    if (n.node.op == "lpf")
    {
        const auto input = inputAt(0);
        const auto cutoff = juce::jlimit(20.0f, 20000.0f, inputAt(1, defaultAt(0, 1200.0f)));
        const auto alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
        n.stateA = (1.0f - alpha) * input + alpha * n.stateA;
        return n.stateA;
    }
    if (n.node.op == "lores")
    {
        const auto input = inputAt(0);
        const auto cutoff = juce::jlimit(20.0f, 20000.0f, inputAt(1, defaultAt(0, 1200.0f)));
        const auto res = juce::jlimit(0.0f, 4.0f, inputAt(2, defaultAt(1, 0.5f)));
        const auto alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
        const auto g = 1.0f - alpha;
        const auto x = std::tanh(input - n.stateD * res);
        n.stateA += g * (x - n.stateA);
        n.stateB += g * (n.stateA - n.stateB);
        n.stateC += g * (n.stateB - n.stateC);
        n.stateD += g * (n.stateC - n.stateD);
        return n.stateD;
    }
    if (n.node.op == "hpf")
    {
        const auto input = inputAt(0);
        const auto cutoff = juce::jlimit(20.0f, 20000.0f, inputAt(1, defaultAt(0, 1200.0f)));
        const auto alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
        const auto out = alpha * (n.stateA + input - n.stateB);
        n.stateA = out;
        n.stateB = input;
        return out;
    }
    if (n.node.op == "bpf" || n.node.op == "svf")
    {
        const auto input = inputAt(0);
        const auto cutoff = juce::jlimit(20.0f, 20000.0f, inputAt(1, defaultAt(0, 1200.0f)));
        const auto q = juce::jlimit(0.05f, 20.0f, inputAt(2, defaultAt(1, 0.7f)));
        const auto f = 2.0f * std::sin(juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
        n.stateA += f * n.stateB; // lp
        const auto hp = input - n.stateA - (n.stateB / juce::jmax(0.05f, q));
        n.stateB += f * hp; // bp
        if (n.node.op == "bpf")
            return n.stateB;
        const auto mode = static_cast<int>(std::round(inputAt(3, defaultAt(2, 0.0f))));
        if (mode == 1)
            return n.stateB;
        if (mode == 2)
            return hp;
        return n.stateA;
    }
    if (n.node.op == "delay" || n.node.op == "cdelay" || n.node.op == "comb" || n.node.op == "apf")
    {
        if (n.delayBuffer.empty())
            return inputAt(0);
        const auto in = inputAt(0);
        const auto ms = juce::jlimit(1.0f, 5000.0f, inputAt(1, defaultAt(0, (n.node.op == "delay" || n.node.op == "cdelay") ? 250.0f : 30.0f)));
        const auto delaySamples = juce::jlimit(1, static_cast<int>(n.delayBuffer.size()) - 1,
                                               static_cast<int>(std::round(ms * 0.001f * static_cast<float>(sampleRate))));
        const auto readIndex = (n.delayWriteIndex - delaySamples + static_cast<int>(n.delayBuffer.size())) % static_cast<int>(n.delayBuffer.size());
        const auto delayed = n.delayBuffer[static_cast<size_t>(readIndex)];

        float out = delayed;
        if (n.node.op == "delay" || n.node.op == "cdelay")
        {
            n.delayBuffer[static_cast<size_t>(n.delayWriteIndex)] = in;
        }
        else if (n.node.op == "comb")
        {
            const auto fb = juce::jlimit(-0.995f, 0.995f, inputAt(2, defaultAt(1, 0.7f)));
            n.delayBuffer[static_cast<size_t>(n.delayWriteIndex)] = in + delayed * fb;
        }
        else // apf
        {
            const auto g = juce::jlimit(-0.995f, 0.995f, inputAt(2, defaultAt(1, 0.5f)));
            out = delayed - g * in;
            n.delayBuffer[static_cast<size_t>(n.delayWriteIndex)] = in + g * out;
        }

        n.delayWriteIndex = (n.delayWriteIndex + 1) % static_cast<int>(n.delayBuffer.size());
        return out;
    }
    if (n.node.op == "clip")
        return juce::jlimit(inputAt(1, defaultAt(0, -1.0f)), inputAt(2, defaultAt(1, 1.0f)), inputAt(0));
    if (n.node.op == "tanh")
        return std::tanh(inputAt(0) * inputAt(1, defaultAt(0, 1.0f)));
    if (n.node.op == "slew")
    {
        const auto target = inputAt(0);
        const auto hz = juce::jlimit(0.1f, 20000.0f, inputAt(1, defaultAt(0, 50.0f)));
        const auto alpha = 1.0f - std::exp(-2.0f * juce::MathConstants<float>::pi * hz / static_cast<float>(sampleRate));
        n.stateA += (target - n.stateA) * alpha;
        return n.stateA;
    }
    if (n.node.op == "sah")
    {
        const auto in = inputAt(0);
        const auto trig = inputAt(1);
        if (trig > 0.0f && n.stateB <= 0.0f)
            n.stateA = in;
        n.stateB = trig;
        return n.stateA;
    }
    if (n.node.op == "mtof")
        return 440.0f * std::pow(2.0f, (inputAt(0, defaultAt(0, 69.0f)) - 69.0f) / 12.0f);
    if (n.node.op == "mtof_sig")
        return 440.0f * std::pow(2.0f, (inputAt(0, defaultAt(0, 69.0f)) - 69.0f) / 12.0f);
    if (n.node.op == "delay1")
    {
        const auto out = n.stateA;
        n.stateA = inputAt(0);
        return out;
    }
    if (n.node.op == "scope" || n.node.op == "spectrum")
        return inputAt(0);
    if (n.node.op == "out")
        return inputAt(0);

    return inputAt(0);
}

float RuntimeEngine::processSingleSample(CompiledGraph& graph, const int sampleIndex, std::vector<float>* valuesOut, float* rightOut)
{
    std::vector<float> scratch;
    auto* values = valuesOut;
    if (values == nullptr)
    {
        scratch.resize(graph.nodes.size(), 0.0f);
        values = &scratch;
    }
    else if (values->size() != graph.nodes.size())
        values->assign(graph.nodes.size(), 0.0f);

    for (const auto i : graph.executionOrder)
        (*values)[i] = runNode(graph.nodes[i], *values, sampleIndex, currentSampleRate, noise);

    float outLeft = 0.0f;
    float outRight = 0.0f;
    for (const auto idx : graph.outputNodes)
    {
        const auto& outNode = graph.nodes[idx];
        auto readInput = [&](const int port, const float fallback) -> float
        {
            if (port < 0 || static_cast<size_t>(port) >= outNode.inputIndices.size())
                return fallback;
            const auto inputIndex = outNode.inputIndices[static_cast<size_t>(port)];
            if (inputIndex < 0 || static_cast<size_t>(inputIndex) >= values->size())
                return fallback;
            return (*values)[static_cast<size_t>(inputIndex)];
        };

        const auto l = readInput(0, 0.0f);
        const auto hasRight = outNode.inputIndices.size() > 1 && outNode.inputIndices[1] >= 0;
        const auto r = readInput(1, l);
        outLeft += l;
        outRight += hasRight ? r : l;
    }

    if (rightOut != nullptr)
        *rightOut = outRight;
    return outLeft;
}

void RuntimeEngine::processBlock(juce::AudioBuffer<float>& buffer)
{
    processBlock(buffer, 0, buffer.getNumSamples());
}

void RuntimeEngine::processBlock(juce::AudioBuffer<float>& buffer, const int startSample, const int numSamples)
{
    const auto boundedStart = juce::jlimit(0, buffer.getNumSamples(), startSample);
    const auto boundedNum = juce::jlimit(0, buffer.getNumSamples() - boundedStart, numSamples);
    if (boundedNum <= 0)
        return;

    juce::ScopedTryLock lock(graphLock);
    if (!lock.isLocked() || currentGraph == nullptr)
    {
        const auto holdL = lastOutputSample.load();
        const auto holdR = lastOutputRightSample.load();
        for (int s = 0; s < boundedNum; ++s)
        {
            if (buffer.getNumChannels() > 0)
                buffer.setSample(0, boundedStart + s, holdL);
            if (buffer.getNumChannels() > 1)
                buffer.setSample(1, boundedStart + s, holdR);
            for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample(ch, boundedStart + s, 0.5f * (holdL + holdR));
        }
        absoluteSample += boundedNum;
        return;
    }

    std::vector<float> nodeValues;
    for (int sample = 0; sample < boundedNum; ++sample)
    {
        const auto globalSample = static_cast<int>(absoluteSample + sample);
        float valueR = 0.0f;
        auto valueL = processSingleSample(*currentGraph, globalSample, &nodeValues, &valueR);

        if (fadingOutGraph != nullptr && crossfadeRemainingSamples > 0)
        {
            float oldValueR = 0.0f;
            const auto oldValueL = processSingleSample(*fadingOutGraph, globalSample, nullptr, &oldValueR);
            const auto t = 1.0f - static_cast<float>(crossfadeRemainingSamples) / static_cast<float>(juce::jmax(1, crossfadeTotalSamples));
            valueL = oldValueL * (1.0f - t) + valueL * t;
            valueR = oldValueR * (1.0f - t) + valueR * t;
            --crossfadeRemainingSamples;
            if (crossfadeRemainingSamples <= 0)
                fadingOutGraph.reset();
        }

        if (buffer.getNumChannels() > 0)
            buffer.setSample(0, boundedStart + sample, valueL);
        if (buffer.getNumChannels() > 1)
            buffer.setSample(1, boundedStart + sample, valueR);
        for (int ch = 2; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, boundedStart + sample, 0.5f * (valueL + valueR));
        lastOutputSample.store(valueL);
        lastOutputRightSample.store(valueR);

        if (!scopeRing.empty())
        {
            const auto idx = scopeWriteIndex.fetch_add(1);
            scopeRing[static_cast<size_t>(idx) % scopeRing.size()] = 0.5f * (valueL + valueR);
        }

        for (const auto& [probeId, idx] : currentGraph->scopeProbeNodes)
        {
            if (!scopeProbeRings.contains(probeId) || idx >= nodeValues.size())
                continue;
            auto& ring = scopeProbeRings[probeId];
            auto& write = scopeProbeWriteIndices[probeId];
            if (!ring.empty())
                ring[static_cast<size_t>(write++) % ring.size()] = nodeValues[idx];
        }
        for (const auto& [probeId, idx] : currentGraph->spectrumProbeNodes)
        {
            if (!spectrumProbeRings.contains(probeId) || idx >= nodeValues.size())
                continue;
            auto& ring = spectrumProbeRings[probeId];
            auto& write = spectrumProbeWriteIndices[probeId];
            if (!ring.empty())
                ring[static_cast<size_t>(write++) % ring.size()] = nodeValues[idx];
        }
    }

    absoluteSample += boundedNum;
}

std::vector<float> RuntimeEngine::getScopeSnapshot(const size_t maxSamples) const
{
    return readRingSnapshot(scopeRing, scopeWriteIndex.load(), maxSamples);
}

std::vector<float> RuntimeEngine::getSpectrumSnapshot(const size_t bins) const
{
    return spectrumFromSamples(getScopeSnapshot(1024), bins);
}

std::vector<std::string> RuntimeEngine::getScopeProbeIds() const
{
    std::vector<std::string> ids;
    const juce::ScopedTryLock lock(graphLock);
    if (!lock.isLocked())
        return ids;
    for (const auto& [id, _] : scopeProbeRings)
    {
        juce::ignoreUnused(_);
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<std::string> RuntimeEngine::getSpectrumProbeIds() const
{
    std::vector<std::string> ids;
    const juce::ScopedTryLock lock(graphLock);
    if (!lock.isLocked())
        return ids;
    for (const auto& [id, _] : spectrumProbeRings)
    {
        juce::ignoreUnused(_);
        ids.push_back(id);
    }
    std::sort(ids.begin(), ids.end());
    return ids;
}

std::vector<float> RuntimeEngine::getScopeSnapshotForProbe(const std::string& probeNodeId, const size_t maxSamples) const
{
    const juce::ScopedTryLock lock(graphLock);
    if (!lock.isLocked())
        return {};
    if (!scopeProbeRings.contains(probeNodeId))
        return {};
    return readRingSnapshot(scopeProbeRings.at(probeNodeId), scopeProbeWriteIndices.contains(probeNodeId) ? scopeProbeWriteIndices.at(probeNodeId) : 0, maxSamples);
}

std::vector<float> RuntimeEngine::getSpectrumSnapshotForProbe(const std::string& probeNodeId, const size_t bins) const
{
    const juce::ScopedTryLock lock(graphLock);
    if (!lock.isLocked())
        return {};
    if (!spectrumProbeRings.contains(probeNodeId))
        return {};
    const auto samples = readRingSnapshot(spectrumProbeRings.at(probeNodeId),
                                          spectrumProbeWriteIndices.contains(probeNodeId) ? spectrumProbeWriteIndices.at(probeNodeId) : 0,
                                          1024);
    return spectrumFromSamples(samples, bins);
}

std::unordered_map<std::string, float> RuntimeEngine::getFloatatomValues() const
{
    std::unordered_map<std::string, float> out;
    const juce::ScopedTryLock lock(graphLock);
    if (!lock.isLocked() || currentGraph == nullptr)
        return out;

    for (const auto& n : currentGraph->nodes)
    {
        if (n.node.op != "floatatom")
            continue;
        auto v = n.stateA;
        if (!std::isfinite(v))
            v = static_cast<float>(n.node.literal.value_or(0.0));
        out[n.node.id] = v;
    }
    return out;
}
} // namespace duodsp::dsp
