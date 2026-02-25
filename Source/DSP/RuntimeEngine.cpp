#include "RuntimeEngine.h"

#include <algorithm>
#include <cmath>
#include <complex>
#include <queue>

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

    if (n.node.op == "constant")
        return static_cast<float>(n.node.literal.value_or(0.0));
    if (n.node.op == "floatatom")
        return static_cast<float>(n.node.literal.value_or(0.0));
    if (n.node.op == "msg")
        return 0.0f;
    if (n.node.op == "obj")
        return inputAt(0);
    if (n.node.op == "input" || n.node.op == "control")
        return 0.0f;
    if (n.node.op == "add")
        return inputAt(0) + inputAt(1);
    if (n.node.op == "sub")
        return inputAt(0) - inputAt(1);
    if (n.node.op == "mul")
        return inputAt(0) * inputAt(1, 1.0f);
    if (n.node.op == "div")
        return inputAt(1) == 0.0f ? 0.0f : inputAt(0) / inputAt(1);
    if (n.node.op == "sin")
    {
        const auto freq = juce::jmax(1.0f, inputAt(0, 220.0f));
        n.stateA += freq / static_cast<float>(sampleRate);
        n.stateA -= std::floor(n.stateA);
        return std::sin(n.stateA * 2.0f * juce::MathConstants<float>::pi);
    }
    if (n.node.op == "saw")
    {
        const auto freq = juce::jmax(1.0f, inputAt(0, 110.0f));
        n.stateA += freq / static_cast<float>(sampleRate);
        n.stateA -= std::floor(n.stateA);
        return n.stateA * 2.0f - 1.0f;
    }
    if (n.node.op == "square")
    {
        const auto freq = juce::jmax(1.0f, inputAt(0, 110.0f));
        const auto duty = juce::jlimit(0.01f, 0.99f, inputAt(1, 0.5f));
        n.stateA += freq / static_cast<float>(sampleRate);
        n.stateA -= std::floor(n.stateA);
        return n.stateA < duty ? 1.0f : -1.0f;
    }
    if (n.node.op == "noise")
    {
        const auto amp = inputAt(0, 0.05f);
        return random.nextFloat() * 2.0f * amp - amp;
    }
    if (n.node.op == "lpf")
    {
        const auto input = inputAt(0);
        const auto cutoff = juce::jlimit(20.0f, 20000.0f, inputAt(1, 1200.0f));
        const auto alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
        n.stateA = (1.0f - alpha) * input + alpha * n.stateA;
        return n.stateA;
    }
    if (n.node.op == "hpf")
    {
        const auto input = inputAt(0);
        const auto cutoff = juce::jlimit(20.0f, 20000.0f, inputAt(1, 1200.0f));
        const auto alpha = std::exp(-2.0f * juce::MathConstants<float>::pi * cutoff / static_cast<float>(sampleRate));
        const auto out = alpha * (n.stateA + input - n.stateB);
        n.stateA = out;
        n.stateB = input;
        return out;
    }
    if (n.node.op == "delay1")
    {
        const auto out = n.stateA;
        n.stateA = inputAt(0);
        return out;
    }
    if (n.node.op == "scope" || n.node.op == "spectrum")
        return inputAt(0);
    if (n.node.op == "out")
        return inputAt(1);

    return inputAt(0);
}

float RuntimeEngine::processSingleSample(CompiledGraph& graph, const int sampleIndex, std::vector<float>* valuesOut)
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

    float outValue = 0.0f;
    if (graph.outputNodes.empty() && !graph.nodes.empty())
        outValue = (*values).back();
    else
        for (const auto idx : graph.outputNodes)
            outValue += (*values)[idx];
    return outValue;
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
        const auto hold = lastOutputSample.load();
        for (int s = 0; s < boundedNum; ++s)
            for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
                buffer.setSample(ch, boundedStart + s, hold);
        absoluteSample += boundedNum;
        return;
    }

    std::vector<float> nodeValues;
    for (int sample = 0; sample < boundedNum; ++sample)
    {
        const auto globalSample = static_cast<int>(absoluteSample + sample);
        auto value = processSingleSample(*currentGraph, globalSample, &nodeValues);

        if (fadingOutGraph != nullptr && crossfadeRemainingSamples > 0)
        {
            const auto oldValue = processSingleSample(*fadingOutGraph, globalSample);
            const auto t = 1.0f - static_cast<float>(crossfadeRemainingSamples) / static_cast<float>(juce::jmax(1, crossfadeTotalSamples));
            value = oldValue * (1.0f - t) + value * t;
            --crossfadeRemainingSamples;
            if (crossfadeRemainingSamples <= 0)
                fadingOutGraph.reset();
        }

        for (int ch = 0; ch < buffer.getNumChannels(); ++ch)
            buffer.setSample(ch, boundedStart + sample, value);
        lastOutputSample.store(value);

        if (!scopeRing.empty())
        {
            const auto idx = scopeWriteIndex.fetch_add(1);
            scopeRing[static_cast<size_t>(idx) % scopeRing.size()] = value;
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
} // namespace duodsp::dsp
