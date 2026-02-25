#pragma once

#include "../IR/GraphIR.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <atomic>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace duodsp::dsp
{
class RuntimeEngine
{
public:
    RuntimeEngine();
    ~RuntimeEngine();

    void prepare(double sampleRate, int blockSize, int outputChannels);
    void setCrossfadeTimeMs(double ms);
    void setGraph(const ir::Graph& graph);
    void triggerBang(const std::string& nodeId);
    std::vector<std::string> consumeTriggeredBangIds();
    void processBlock(juce::AudioBuffer<float>& buffer);
    void processBlock(juce::AudioBuffer<float>& buffer, int startSample, int numSamples);
    std::vector<float> getScopeSnapshot(size_t maxSamples) const;
    std::vector<float> getSpectrumSnapshot(size_t bins) const;
    std::vector<std::string> getScopeProbeIds() const;
    std::vector<std::string> getSpectrumProbeIds() const;
    std::vector<float> getScopeSnapshotForProbe(const std::string& probeNodeId, size_t maxSamples) const;
    std::vector<float> getSpectrumSnapshotForProbe(const std::string& probeNodeId, size_t bins) const;
    std::unordered_map<std::string, float> getFloatatomValues() const;

private:
    struct RuntimeNode
    {
        ir::Node node;
        std::vector<int> inputIndices;
        std::vector<float> paramDefaults;
        float stateA = 0.0f;
        float stateB = 0.0f;
        float stateC = 0.0f;
        float stateD = 0.0f;
        std::vector<float> delayBuffer;
        int delayWriteIndex = 0;
    };

    struct CompiledGraph
    {
        std::vector<RuntimeNode> nodes;
        std::unordered_map<std::string, size_t> indexByNodeId;
        std::vector<size_t> outputNodes;
        std::vector<std::pair<std::string, size_t>> scopeProbeNodes;
        std::vector<std::pair<std::string, size_t>> spectrumProbeNodes;
        std::vector<size_t> executionOrder;
    };

    std::unique_ptr<CompiledGraph> compileGraph(const ir::Graph& graph) const;
    static float runNode(RuntimeNode& n, const std::vector<float>& values, int sampleIndex, double sampleRate, juce::Random& random);
    float processSingleSample(CompiledGraph& graph, int sampleIndex, std::vector<float>* valuesOut = nullptr, float* rightOut = nullptr);
    static void transferState(const CompiledGraph* from, CompiledGraph& to);

    mutable juce::CriticalSection graphLock;
    std::unique_ptr<CompiledGraph> currentGraph;
    std::unique_ptr<CompiledGraph> fadingOutGraph;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;
    int currentOutputChannels = 2;
    int64_t absoluteSample = 0;
    int crossfadeRemainingSamples = 0;
    int crossfadeTotalSamples = 0;
    double crossfadeMs = 35.0;
    juce::Random noise;
    std::vector<float> scopeRing;
    std::atomic<int> scopeWriteIndex { 0 };
    std::atomic<float> lastOutputSample { 0.0f };
    std::atomic<float> lastOutputRightSample { 0.0f };
    std::unordered_map<std::string, std::vector<float>> scopeProbeRings;
    std::unordered_map<std::string, std::vector<float>> spectrumProbeRings;
    std::unordered_map<std::string, int> scopeProbeWriteIndices;
    std::unordered_map<std::string, int> spectrumProbeWriteIndices;
};
} // namespace duodsp::dsp
