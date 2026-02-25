#include "DSP/RuntimeEngine.h"
#include "Text/GraphLanguage.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{
struct TestResult
{
    std::string name;
    bool pass = false;
    std::string message;
};

void require(const bool condition, const std::string& message)
{
    if (!condition)
        throw std::runtime_error(message);
}

duodsp::ir::Graph makeConstantOutGraph(const std::string& constId, const double value)
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ constId, "constant", std::to_string(value), value });
    g.nodes.push_back({ "out_1", "out", "out", std::nullopt });
    g.edges.push_back({ constId, "out_1", 0 });
    g.edges.push_back({ constId, "out_1", 1 });
    g.bindings["sig"] = constId;
    return g;
}

float runGraphAndReadSample(duodsp::dsp::RuntimeEngine& engine, const duodsp::ir::Graph& g, const int numSamples = 64)
{
    juce::AudioBuffer<float> buffer(2, numSamples);
    engine.setGraph(g);
    engine.processBlock(buffer);
    return buffer.getSample(0, numSamples - 1);
}

duodsp::ir::Graph makeUnaryOpGraph(const std::string& op, const std::string& label, const double input)
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "src", "constant", std::to_string(input), input });
    g.nodes.push_back({ "op", op, label, std::nullopt });
    g.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    g.edges.push_back({ "src", "op", 0 });
    g.edges.push_back({ "op", "out", 0 });
    g.edges.push_back({ "op", "out", 1 });
    g.bindings["sig"] = "op";
    return g;
}

duodsp::ir::Graph makeSourceFreeOpGraph(const std::string& op, const std::string& label)
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "op", op, label, std::nullopt });
    g.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    g.edges.push_back({ "op", "out", 0 });
    g.edges.push_back({ "op", "out", 1 });
    g.bindings["sig"] = "op";
    return g;
}

duodsp::ir::Graph makeBinaryOpGraph(const std::string& op, const std::string& label, const double a, const double b)
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "a", "constant", std::to_string(a), a });
    g.nodes.push_back({ "b", "constant", std::to_string(b), b });
    g.nodes.push_back({ "op", op, label, std::nullopt });
    g.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    g.edges.push_back({ "a", "op", 0 });
    g.edges.push_back({ "b", "op", 1 });
    g.edges.push_back({ "op", "out", 0 });
    g.edges.push_back({ "op", "out", 1 });
    g.bindings["sig"] = "op";
    return g;
}

void testCompileAndSync()
{
    const std::string source =
        "osc = sin(220);\n"
        "amp = 0.2;\n"
        "sig = osc * amp;\n"
        "dac~(sig, sig);\n";

    const auto result = duodsp::text::compile(source, nullptr);
    require(result.diagnostics.empty(), "Expected no diagnostics for valid graph source.");
    require(!result.graph.nodes.empty(), "Compiled graph should contain nodes.");
    require(!result.graph.edges.empty(), "Compiled graph should contain edges.");
    require(!result.graph.bindings.empty(), "Compiled graph should contain bindings.");
    require(!result.syncMap.ranges().empty(), "Sync map should contain node text ranges.");
}

void testStableNodeIds()
{
    const std::string sourceA =
        "osc = sin(220);\n"
        "amp = 0.2;\n"
        "sig = osc * amp;\n"
        "dac~(sig, sig);\n";
    const auto a = duodsp::text::compile(sourceA, nullptr);
    require(a.graph.bindings.contains("sig"), "Binding sig missing in compile A.");
    const auto sigIdA = a.graph.bindings.at("sig");

    const std::string sourceB =
        "osc = sin(440);\n"
        "amp = 0.2;\n"
        "sig = osc * amp;\n"
        "dac~(sig, sig);\n";
    const auto b = duodsp::text::compile(sourceB, &a.graph);
    require(b.graph.bindings.contains("sig"), "Binding sig missing in compile B.");
    const auto sigIdB = b.graph.bindings.at("sig");
    require(sigIdA == sigIdB, "Stable NodeID preservation failed for binding root.");
}

void testHotSwapCrossfade()
{
    duodsp::dsp::RuntimeEngine engine;
    engine.prepare(48000.0, 128, 2);
    engine.setCrossfadeTimeMs(20.0);

    auto gA = makeConstantOutGraph("sig_const", 1.0);
    auto gB = makeConstantOutGraph("sig_const", -1.0);

    juce::AudioBuffer<float> buffer(2, 128);
    engine.setGraph(gA);
    engine.processBlock(buffer);
    const auto steadyA = buffer.getSample(0, 127);
    require(std::isfinite(steadyA), "Output A must be finite.");

    engine.setGraph(gB);
    engine.processBlock(buffer);
    float minSample = 1.0e9f;
    float maxSample = -1.0e9f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const auto v = buffer.getSample(0, i);
        minSample = std::min(minSample, v);
        maxSample = std::max(maxSample, v);
    }
    require(maxSample > -0.95f, "Crossfade expected intermediate values (max too low).");
    require(minSample < 0.95f, "Crossfade expected intermediate values (min too high).");
}

void testCycleRule()
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "a", "add", "add", std::nullopt });
    g.nodes.push_back({ "b", "mul", "mul", std::nullopt });
    g.edges.push_back({ "a", "b", 0 });
    g.edges.push_back({ "b", "a", 0 });
    const auto issues = duodsp::ir::validateGraph(g);
    require(!issues.empty(), "Cycle without delay1 should be rejected.");

    g.nodes.push_back({ "d", "delay1", "delay1", std::nullopt });
    g.edges.clear();
    g.edges.push_back({ "a", "d", 0 });
    g.edges.push_back({ "d", "b", 0 });
    g.edges.push_back({ "b", "a", 0 });
    const auto fixed = duodsp::ir::validateGraph(g);
    require(fixed.empty(), "Cycle with delay1 break should be accepted.");
}

void testPrettyPrintCycleSafety()
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "k", "cadd", "+", std::nullopt });
    g.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    g.edges.push_back({ "k", "k", 0 });
    g.edges.push_back({ "k", "k", 1 });
    g.edges.push_back({ "k", "out", 0 });
    g.edges.push_back({ "k", "out", 1 });
    g.bindings["n1"] = "k";

    const auto printed = duodsp::text::prettyPrint(g);
    require(!printed.empty(), "Pretty print should not crash or return empty text for cyclic graph.");
    require(printed.find("dac~(") != std::string::npos, "Pretty print should still emit dac~ line for cyclic graph.");
}

void testTypedPortValidation()
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "osc", "sin", "sin", std::nullopt });
    g.nodes.push_back({ "aud", "noise", "noise~", std::nullopt });
    g.nodes.push_back({ "k", "floatatom", "2", 2.0 });
    g.nodes.push_back({ "m", "mtof", "mtof", std::nullopt });
    g.nodes.push_back({ "mul", "mul", "*~", std::nullopt });
    g.nodes.push_back({ "out", "out", "out", std::nullopt });
    g.edges.push_back({ "osc", "out", 0 });
    g.edges.push_back({ "osc", "out", 1 });

    const auto issue = duodsp::ir::validateConnection(g, "osc", "m", 0);
    require(issue.has_value(), "Audio->control connection should be rejected.");

    const auto rhsControl = duodsp::ir::validateConnection(g, "k", "mul", 1);
    require(!rhsControl.has_value(), "Control->right inlet of *~ should be accepted.");

    const auto lhsControl = duodsp::ir::validateConnection(g, "k", "mul", 0);
    require(!lhsControl.has_value(), "Control->left inlet of *~ should still be accepted.");

    const auto oscFreqAudio = duodsp::ir::validateConnection(g, "aud", "osc", 0);
    require(!oscFreqAudio.has_value(), "Audio->oscillator frequency inlet should be accepted.");
}

void testProbeTargeting()
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "src", "constant", "1.0", 1.0 });
    g.nodes.push_back({ "scopeTap", "scope", "scope", std::nullopt });
    g.nodes.push_back({ "specTap", "spectrum", "spectrum", std::nullopt });
    g.nodes.push_back({ "out", "out", "out", std::nullopt });
    g.edges.push_back({ "src", "scopeTap", 0 });
    g.edges.push_back({ "src", "specTap", 0 });
    g.edges.push_back({ "src", "out", 0 });
    g.edges.push_back({ "src", "out", 1 });

    duodsp::dsp::RuntimeEngine engine;
    engine.prepare(48000.0, 64, 2);
    engine.setGraph(g);
    juce::AudioBuffer<float> buffer(2, 64);
    for (int i = 0; i < 8; ++i)
        engine.processBlock(buffer);

    const auto scopeIds = engine.getScopeProbeIds();
    const auto spectrumIds = engine.getSpectrumProbeIds();
    require(std::find(scopeIds.begin(), scopeIds.end(), "scopeTap") != scopeIds.end(), "Scope probe ID should be discoverable.");
    require(std::find(spectrumIds.begin(), spectrumIds.end(), "specTap") != spectrumIds.end(), "Spectrum probe ID should be discoverable.");

    const auto scope = engine.getScopeSnapshotForProbe("scopeTap", 32);
    const auto spectrum = engine.getSpectrumSnapshotForProbe("specTap", 16);
    require(!scope.empty(), "Scope probe snapshot should contain samples.");
    require(!spectrum.empty(), "Spectrum probe snapshot should contain bins.");
}

void testControlMathCodeRoundTrip()
{
    const std::string source =
        "a = 3;\n"
        "b = 5;\n"
        "k = @k (a + b * 2);\n"
        "sig = sin(k);\n"
        "dac~(sig, sig);\n";

    const auto r = duodsp::text::compile(source, nullptr);
    require(r.diagnostics.empty(), "Control-math source should compile without diagnostics.");

    bool hasControlMath = false;
    for (const auto& n : r.graph.nodes)
    {
        if (n.op == "cadd" || n.op == "cmul" || n.op == "csub" || n.op == "cdiv")
        {
            hasControlMath = true;
            break;
        }
    }
    require(hasControlMath, "Expected control-rate math nodes for @k expression.");

    const auto printed = duodsp::text::prettyPrint(r.graph);
    const auto r2 = duodsp::text::compile(printed, &r.graph);
    require(r2.diagnostics.empty(), "Pretty-printed control-math code should recompile cleanly.");
}

void testLabelDefaultsForNodes()
{
    duodsp::dsp::RuntimeEngine engine;
    engine.prepare(48000.0, 128, 2);
    engine.setCrossfadeTimeMs(0.0);

    auto near = [](const float a, const float b, const float eps)
    {
        return std::abs(a - b) <= eps;
    };

    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("mul", "*~ 0.1", 1.0), 64), 0.1f, 0.02f), "*~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("comp_sig", "comp~ 0.5", 1.0), 64), 1.0f, 0.02f), "comp~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("comp", "comp 0.5", 1.0), 64), 1.0f, 0.02f), "comp arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("add", "+~ 0.25", 1.0), 64), 1.25f, 0.02f), "+~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("sub", "-~ 0.25", 1.0), 64), 0.75f, 0.02f), "-~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("div", "/~ 4", 1.0), 64), 0.25f, 0.02f), "/~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("pow", "pow~ 2", 3.0), 64), 9.0f, 0.05f), "pow~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("mod", "mod~ 2", 5.0), 64), 1.0f, 0.05f), "mod~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("clip", "clip~ -0.2 0.2", 1.0), 64), 0.2f, 0.02f), "clip~ args defaults not applied.");

    const auto tanhOut = runGraphAndReadSample(engine, makeUnaryOpGraph("tanh", "tanh~ 0.5", 1.0), 64);
    require(near(tanhOut, std::tanh(0.5f), 0.03f), "tanh~ arg default not applied.");

    const auto mtofOut = runGraphAndReadSample(engine, makeSourceFreeOpGraph("mtof", "mtof 69"), 64);
    require(near(mtofOut, 440.0f, 0.5f), "mtof arg default not applied.");
    const auto mtofSigOut = runGraphAndReadSample(engine, makeSourceFreeOpGraph("mtof_sig", "mtof~ 69"), 64);
    require(near(mtofSigOut, 440.0f, 0.5f), "mtof~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("delay", "delay~ 1", 1.0), 128), 1.0f, 0.05f), "delay~ arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("cdelay", "delay 1", 1.0), 128), 1.0f, 0.05f), "delay (control) arg default not applied.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("not_sig", "not~", 1.0), 64), 0.0f, 0.02f), "not~ default behavior incorrect.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("not", "not", 1.0), 64), 0.0f, 0.02f), "not default behavior incorrect.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("min_sig", "min~ 0.5", 1.0), 64), 0.5f, 0.02f), "min~ arg default behavior incorrect.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("max_sig", "max~ 0.5", 0.1), 64), 0.5f, 0.02f), "max~ arg default behavior incorrect.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("min", "min 0.5", 1.0), 64), 0.5f, 0.02f), "min arg default behavior incorrect.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("max", "max 0.5", 0.1), 64), 0.5f, 0.02f), "max arg default behavior incorrect.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("abs_sig", "abs~", -0.5), 64), 0.5f, 0.02f), "abs~ behavior incorrect.");
    require(near(runGraphAndReadSample(engine, makeUnaryOpGraph("abs", "abs", -0.5), 64), 0.5f, 0.02f), "abs behavior incorrect.");

    duodsp::ir::Graph randomG;
    randomG.nodes.push_back({ "trig", "constant", "1", 1.0 });
    randomG.nodes.push_back({ "r", "random", "random 2 4", std::nullopt });
    randomG.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    randomG.edges.push_back({ "trig", "r", 0 });
    randomG.edges.push_back({ "r", "out", 0 });
    randomG.edges.push_back({ "r", "out", 1 });
    const auto randomOut = runGraphAndReadSample(engine, randomG, 64);
    require(randomOut >= 2.0f && randomOut <= 4.0f, "random bounds/defaults not applied.");

    const auto bpfOut = runGraphAndReadSample(engine, makeUnaryOpGraph("bpf", "bpf~ 1000 0.7", 1.0), 128);
    require(std::isfinite(bpfOut), "bpf~ output must be finite.");
    const auto loresOut = runGraphAndReadSample(engine, makeUnaryOpGraph("lores", "lores~ 1000 0.5", 1.0), 128);
    require(std::isfinite(loresOut), "lores~ output must be finite.");
    const auto svfOut = runGraphAndReadSample(engine, makeUnaryOpGraph("svf", "svf~ 1000 0.7 1", 1.0), 128);
    require(std::isfinite(svfOut), "svf~ output must be finite.");
    const auto apfOut = runGraphAndReadSample(engine, makeUnaryOpGraph("apf", "apf~ 2 0.5", 1.0), 128);
    require(std::isfinite(apfOut), "apf~ output must be finite.");
    const auto combOut = runGraphAndReadSample(engine, makeUnaryOpGraph("comb", "comb~ 2 0.7", 1.0), 128);
    require(std::isfinite(combOut), "comb~ output must be finite.");

    const auto phasorOut = runGraphAndReadSample(engine, makeSourceFreeOpGraph("saw", "phasor~ 400"), 101);
    require(phasorOut > 0.4f, "phasor~ arg default likely not applied.");

    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("comp_sig", "comp~", 0.8, 0.2), 64), 1.0f, 0.02f), "comp~ compare true should output 1.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("comp_sig", "comp~", 0.1, 0.2), 64), 0.0f, 0.02f), "comp~ compare false should output 0.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("comp", "comp", 0.8, 0.2), 64), 1.0f, 0.02f), "comp compare true should output 1.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("comp", "comp", 0.1, 0.2), 64), 0.0f, 0.02f), "comp compare false should output 0.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("and_sig", "and~", 1.0, 1.0), 64), 1.0f, 0.02f), "and~ true/true should output 1.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("or_sig", "or~", 0.0, 1.0), 64), 1.0f, 0.02f), "or~ false/true should output 1.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("xor_sig", "xor~", 1.0, 1.0), 64), 0.0f, 0.02f), "xor~ true/true should output 0.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("and", "and", 1.0, 0.0), 64), 0.0f, 0.02f), "and true/false should output 0.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("or", "or", 0.0, 1.0), 64), 1.0f, 0.02f), "or false/true should output 1.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("xor", "xor", 1.0, 0.0), 64), 1.0f, 0.02f), "xor true/false should output 1.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("min_sig", "min~", 0.2, 0.8), 64), 0.2f, 0.02f), "min~ should output lower value.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("max_sig", "max~", 0.2, 0.8), 64), 0.8f, 0.02f), "max~ should output higher value.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("min", "min", 0.2, 0.8), 64), 0.2f, 0.02f), "min should output lower value.");
    require(near(runGraphAndReadSample(engine, makeBinaryOpGraph("max", "max", 0.2, 0.8), 64), 0.8f, 0.02f), "max should output higher value.");
}

void testSampleAndHoldNode()
{
    duodsp::dsp::RuntimeEngine engine;
    engine.prepare(48000.0, 64, 2);
    engine.setCrossfadeTimeMs(0.0);

    duodsp::ir::Graph noTrigger;
    noTrigger.nodes.push_back({ "src", "constant", "0.8", 0.8 });
    noTrigger.nodes.push_back({ "trig", "constant", "0", 0.0 });
    noTrigger.nodes.push_back({ "sah", "sah", "sah~", std::nullopt });
    noTrigger.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    noTrigger.edges.push_back({ "src", "sah", 0 });
    noTrigger.edges.push_back({ "trig", "sah", 1 });
    noTrigger.edges.push_back({ "sah", "out", 0 });
    noTrigger.edges.push_back({ "sah", "out", 1 });
    const auto held0 = runGraphAndReadSample(engine, noTrigger, 64);
    require(std::abs(held0) < 0.01f, "sah~ should hold initial value when trigger never rises.");

    duodsp::ir::Graph triggerRise = noTrigger;
    if (auto* trig = triggerRise.findNode("trig"); trig != nullptr)
        trig->literal = 1.0;
    const auto held1 = runGraphAndReadSample(engine, triggerRise, 64);
    if (!(std::abs(held1 - 0.8f) < 0.01f))
    {
        std::ostringstream msg;
        msg << "sah~ should sample input on trigger rising edge (got " << held1 << ")";
        require(false, msg.str());
    }
}

void testBangNode()
{
    duodsp::dsp::RuntimeEngine engine;
    engine.prepare(48000.0, 64, 2);
    engine.setCrossfadeTimeMs(0.0);

    duodsp::ir::Graph g;
    g.nodes.push_back({ "b", "bang", "bang", std::nullopt });
    g.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    g.edges.push_back({ "b", "out", 0 });
    g.edges.push_back({ "b", "out", 1 });
    engine.setGraph(g);

    juce::AudioBuffer<float> buffer(2, 64);
    engine.processBlock(buffer);
    require(std::abs(buffer.getSample(0, 0)) < 0.001f, "bang should be idle before trigger.");

    engine.triggerBang("b");
    engine.processBlock(buffer);
    require(buffer.getSample(0, 0) > 0.5f, "bang should output pulse on trigger.");
    require(std::abs(buffer.getSample(0, 1)) < 0.001f, "bang pulse should be one sample.");

    duodsp::dsp::RuntimeEngine engineIn;
    engineIn.prepare(48000.0, 64, 2);
    engineIn.setCrossfadeTimeMs(0.0);
    duodsp::ir::Graph gIn;
    gIn.nodes.push_back({ "trig", "constant", "1", 1.0 });
    gIn.nodes.push_back({ "b", "bang", "bang", std::nullopt });
    gIn.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    gIn.edges.push_back({ "trig", "b", 0 });
    gIn.edges.push_back({ "b", "out", 0 });
    gIn.edges.push_back({ "b", "out", 1 });
    engineIn.setGraph(gIn);
    engineIn.processBlock(buffer);
    float peak = 0.0f;
    for (int i = 0; i < buffer.getNumSamples(); ++i)
        peak = juce::jmax(peak, buffer.getSample(0, i));
    if (!(peak > 0.5f))
    {
        std::ostringstream msg;
        msg << "bang should trigger from input rising edge (peak=" << peak << ")";
        require(false, msg.str());
    }

    // Bang should also be delayed by control-rate delay object.
    duodsp::dsp::RuntimeEngine delayEngine;
    delayEngine.prepare(48000.0, 512, 2);
    delayEngine.setCrossfadeTimeMs(0.0);
    duodsp::ir::Graph delayed;
    delayed.nodes.push_back({ "b", "bang", "bang", std::nullopt });
    delayed.nodes.push_back({ "d", "cdelay", "delay 10", std::nullopt });
    delayed.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    delayed.edges.push_back({ "b", "d", 0 });
    delayed.edges.push_back({ "d", "out", 0 });
    delayed.edges.push_back({ "d", "out", 1 });
    delayEngine.setGraph(delayed);
    delayEngine.triggerBang("b");
    juce::AudioBuffer<float> longBuf(2, 512);
    delayEngine.processBlock(longBuf);
    int firstPulse = -1;
    for (int i = 0; i < longBuf.getNumSamples(); ++i)
    {
        if (longBuf.getSample(0, i) > 0.5f)
        {
            firstPulse = i;
            break;
        }
    }
    require(firstPulse >= 430 && firstPulse <= 510, "delay should postpone bang pulse by roughly 10ms at 48k.");
}

void testMessageSetsFloatatom()
{
    duodsp::dsp::RuntimeEngine engine;
    engine.prepare(48000.0, 64, 2);
    engine.setCrossfadeTimeMs(0.0);

    duodsp::ir::Graph g;
    g.nodes.push_back({ "trig", "constant", "1", 1.0 });
    g.nodes.push_back({ "m", "msg", "250", std::nullopt });
    g.nodes.push_back({ "f", "floatatom", "0", 0.0 });
    g.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    g.edges.push_back({ "trig", "m", 0 });
    g.edges.push_back({ "m", "f", 0 });
    g.edges.push_back({ "f", "out", 0 });
    g.edges.push_back({ "f", "out", 1 });
    engine.setGraph(g);

    juce::AudioBuffer<float> buffer(2, 64);
    engine.processBlock(buffer);
    require(std::abs(buffer.getSample(0, 63) - 250.0f) < 0.01f, "message should set floatatom value when banged.");

    duodsp::dsp::RuntimeEngine clickEngine;
    clickEngine.prepare(48000.0, 64, 2);
    clickEngine.setCrossfadeTimeMs(0.0);
    duodsp::ir::Graph gClick;
    gClick.nodes.push_back({ "m", "msg", "72", std::nullopt });
    gClick.nodes.push_back({ "f", "floatatom", "0", 0.0 });
    gClick.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    gClick.edges.push_back({ "m", "f", 0 });
    gClick.edges.push_back({ "f", "out", 0 });
    gClick.edges.push_back({ "f", "out", 1 });
    clickEngine.setGraph(gClick);
    clickEngine.triggerBang("m");
    clickEngine.processBlock(buffer);
    require(std::abs(buffer.getSample(0, 63) - 72.0f) < 0.01f, "click-banged message should set floatatom value.");

    duodsp::dsp::RuntimeEngine bangChainEngine;
    bangChainEngine.prepare(48000.0, 64, 2);
    bangChainEngine.setCrossfadeTimeMs(0.0);
    duodsp::ir::Graph gChain;
    gChain.nodes.push_back({ "b", "bang", "bang", std::nullopt });
    gChain.nodes.push_back({ "m", "msg", "72", std::nullopt });
    gChain.nodes.push_back({ "f", "floatatom", "0", 0.0 });
    gChain.nodes.push_back({ "out", "out", "dac~", std::nullopt });
    gChain.edges.push_back({ "b", "m", 0 });
    gChain.edges.push_back({ "m", "f", 0 });
    gChain.edges.push_back({ "f", "out", 0 });
    gChain.edges.push_back({ "f", "out", 1 });
    bangChainEngine.setGraph(gChain);
    bangChainEngine.triggerBang("b");
    bangChainEngine.processBlock(buffer);
    require(std::abs(buffer.getSample(0, 63) - 72.0f) < 0.01f, "bang->msg->floatatom chain should output 72.");
}

void testNewFilterDelayCodeRoundTrip()
{
    const std::string source =
        "n = noise~(0.1);\n"
        "d = delay~(n, 2);\n"
        "k = delay(1, 2);\n"
        "lg = and(k, 1);\n"
        "lg2 = not(lg);\n"
        "b = bpf~(d, 1200, 0.7);\n"
        "gx = xor~(b, 0);\n"
        "gy = not~(gx);\n"
        "m1 = min~(gy, 0.7);\n"
        "m2 = max~(m1, 0.1);\n"
        "k2 = min(k, 1);\n"
        "k3 = max(k2, 0);\n"
        "k4 = abs(k3);\n"
        "lr = lores~(m2, 900, 0.4);\n"
        "ar = abs~(lr);\n"
        "rv = random(1, 3);\n"
        "s = svf~(ar, 800, 0.7, 1);\n"
        "a = apf~(s, 5, 0.5);\n"
        "c = comb~(a, 10, 0.6);\n"
        "dac~(c, c);\n";

    const auto r = duodsp::text::compile(source, nullptr);
    require(r.diagnostics.empty(), "New filter/delay objects should compile without diagnostics.");
    const auto printed = duodsp::text::prettyPrint(r.graph);
    require(printed.find("delay~(") != std::string::npos, "Pretty print should include delay~.");
    require(printed.find("delay(") != std::string::npos, "Pretty print should include control delay.");
    require(printed.find("and(") != std::string::npos, "Pretty print should include control logic.");
    require(printed.find("not(") != std::string::npos, "Pretty print should include control NOT.");
    require(printed.find("xor~(") != std::string::npos, "Pretty print should include audio logic.");
    require(printed.find("not~(") != std::string::npos, "Pretty print should include audio NOT.");
    require(printed.find("min~(") != std::string::npos, "Pretty print should include audio min.");
    require(printed.find("max~(") != std::string::npos, "Pretty print should include audio max.");
    require(printed.find("min(") != std::string::npos, "Pretty print should include control min.");
    require(printed.find("max(") != std::string::npos, "Pretty print should include control max.");
    require(printed.find("abs~(") != std::string::npos, "Pretty print should include audio abs.");
    require(printed.find("abs(") != std::string::npos, "Pretty print should include control abs.");
    require(printed.find("random(") != std::string::npos, "Pretty print should include random.");
    require(printed.find("bpf~(") != std::string::npos, "Pretty print should include bpf~.");
    require(printed.find("lores~(") != std::string::npos, "Pretty print should include lores~.");
    require(printed.find("svf~(") != std::string::npos, "Pretty print should include svf~.");
    require(printed.find("apf~(") != std::string::npos, "Pretty print should include apf~.");
    require(printed.find("comb~(") != std::string::npos, "Pretty print should include comb~.");
}

void testCodePanelUsesDisplayAliases()
{
    const std::string source =
        "a = delay(1, 5);\n"
        "b = mtof~(69);\n"
        "c = comp~(b, 100);\n"
        "d = and~(c, 1);\n"
        "e = min~(d, 0.8);\n"
        "f = max~(e, 0.1);\n"
        "fa = abs~(f);\n"
        "g = and(1, 1);\n"
        "h = max(g, 0);\n"
        "ha = abs(h);\n"
        "r = random(0, 1);\n"
        "dac~(fa, fa);\n";

    const auto r = duodsp::text::compile(source, nullptr);
    require(r.diagnostics.empty(), "Alias source should compile without diagnostics.");
    const auto printed = duodsp::text::prettyPrint(r.graph);

    require(printed.find("cdelay(") == std::string::npos, "Code panel leaked internal cdelay name.");
    require(printed.find("mtof_sig(") == std::string::npos, "Code panel leaked internal mtof_sig name.");
    require(printed.find("comp_sig(") == std::string::npos, "Code panel leaked internal comp_sig name.");
    require(printed.find("and_sig(") == std::string::npos, "Code panel leaked internal and_sig name.");
    require(printed.find("or_sig(") == std::string::npos, "Code panel leaked internal or_sig name.");
    require(printed.find("xor_sig(") == std::string::npos, "Code panel leaked internal xor_sig name.");
    require(printed.find("not_sig(") == std::string::npos, "Code panel leaked internal not_sig name.");
    require(printed.find("min_sig(") == std::string::npos, "Code panel leaked internal min_sig name.");
    require(printed.find("max_sig(") == std::string::npos, "Code panel leaked internal max_sig name.");
    require(printed.find("abs_sig(") == std::string::npos, "Code panel leaked internal abs_sig name.");
}

void testCodeViewDisplaysNodeParameters()
{
    auto expectContains = [](const std::string& source, const std::string& needle, const std::string& label)
    {
        const auto r = duodsp::text::compile(source, nullptr);
        require(r.diagnostics.empty(), "Compile failed for " + label);
        const auto printed = duodsp::text::prettyPrint(r.graph);
        require(printed.find(needle) != std::string::npos, "Code view missing expected parameter display for " + label);
    };

    expectContains("x = delay~(1, 250);\ndac~(x, x);\n", "delay~(1, 250)", "delay~");
    expectContains("x = delay(1, 250);\ndac~(x, x);\n", "delay(1, 250)", "delay");
    expectContains("x = lores~(1, 1200, 0.5);\ndac~(x, x);\n", "lores~(1, 1200, 0.5)", "lores~");
    expectContains("x = bpf~(1, 1200, 0.7);\ndac~(x, x);\n", "bpf~(1, 1200, 0.7)", "bpf~");
    expectContains("x = svf~(1, 1200, 0.7, 1);\ndac~(x, x);\n", "svf~(1, 1200, 0.7, 1)", "svf~");
    expectContains("x = apf~(1, 20, 0.5);\ndac~(x, x);\n", "apf~(1, 20, 0.5)", "apf~");
    expectContains("x = comb~(1, 30, 0.7);\ndac~(x, x);\n", "comb~(1, 30, 0.7)", "comb~");
    expectContains("x = clip~(1, 0, 0.2);\ndac~(x, x);\n", "clip~(1, 0, 0.2)", "clip~");
    expectContains("x = tanh~(1, 0.5);\ndac~(x, x);\n", "tanh~(1, 0.5)", "tanh~");
    expectContains("x = slew~(1, 50);\ndac~(x, x);\n", "slew~(1, 50)", "slew~");
    expectContains("x = sah~(1, 1);\ndac~(x, x);\n", "sah~(1, 1)", "sah~");
    expectContains("x = comp~(1, 0.5);\ndac~(x, x);\n", "comp~(1, 0.5)", "comp~");
    expectContains("x = min~(1, 0.5);\ndac~(x, x);\n", "min~(1, 0.5)", "min~");
    expectContains("x = max~(1, 0.5);\ndac~(x, x);\n", "max~(1, 0.5)", "max~");
    expectContains("x = random(1, 2, 4);\ndac~(x, x);\n", "random(1, 2, 4)", "random");
    expectContains("x = mtof~(69);\ndac~(x, x);\n", "mtof~(69)", "mtof~");
    expectContains("x = mtof(69);\ndac~(x, x);\n", "mtof(69)", "mtof");
    expectContains("x = abs~(0.5);\ndac~(x, x);\n", "abs~(0.5)", "abs~");
    expectContains("x = abs(0.5);\ndac~(x, x);\n", "abs(0.5)", "abs");
}

std::string jsonEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 8);
    for (const auto c : s)
    {
        switch (c)
        {
            case '\\':
                out += "\\\\";
                break;
            case '"':
                out += "\\\"";
                break;
            case '\n':
                out += "\\n";
                break;
            case '\r':
                out += "\\r";
                break;
            case '\t':
                out += "\\t";
                break;
            default:
                out += c;
                break;
        }
    }
    return out;
}
} // namespace

int main(int argc, char** argv)
{
    bool run = false;
    std::string format = "text";
    std::optional<std::string> onlyTest;

    for (int i = 1; i < argc; ++i)
    {
        const std::string arg = argv[i];
        if (arg == "--headless-selftest")
            run = true;
        else if (arg == "--format=json")
            format = "json";
        else if (arg == "--format=text")
            format = "text";
        else if (arg.rfind("--only=", 0) == 0)
            onlyTest = arg.substr(7);
    }

    if (!run)
    {
        std::cout << "SchismSelfTest: pass --headless-selftest to run tests.\n";
        std::cout << "Optional: --format=json|text --only=<test-name>\n";
        return 0;
    }

    struct TestCase
    {
        std::string name;
        void (*fn)();
    };
    const std::vector<TestCase> tests = {
        { "compile_sync", &testCompileAndSync },
        { "stable_node_ids", &testStableNodeIds },
        { "hot_swap_crossfade", &testHotSwapCrossfade },
        { "cycle_rule", &testCycleRule },
        { "pretty_cycle_safety", &testPrettyPrintCycleSafety },
        { "typed_ports", &testTypedPortValidation },
        { "probe_targeting", &testProbeTargeting },
        { "control_math_code", &testControlMathCodeRoundTrip },
        { "label_defaults", &testLabelDefaultsForNodes },
        { "sample_hold", &testSampleAndHoldNode },
        { "bang_node", &testBangNode },
        { "msg_floatatom", &testMessageSetsFloatatom },
        { "filter_delay_code", &testNewFilterDelayCodeRoundTrip },
        { "code_aliases", &testCodePanelUsesDisplayAliases },
        { "code_params", &testCodeViewDisplaysNodeParameters },
    };

    std::vector<TestResult> results;
    for (const auto& test : tests)
    {
        if (onlyTest.has_value() && *onlyTest != test.name)
            continue;

        TestResult r;
        r.name = test.name;
        try
        {
            test.fn();
            r.pass = true;
            r.message = "ok";
        }
        catch (const std::exception& e)
        {
            r.pass = false;
            r.message = e.what();
        }
        catch (...)
        {
            r.pass = false;
            r.message = "unknown error";
        }
        results.push_back(std::move(r));
    }

    if (results.empty())
    {
        if (format == "json")
            std::cout << "{\"ok\":false,\"error\":\"no tests selected\"}\n";
        else
            std::cerr << "SELFTEST FAIL: no tests selected\n";
        return 1;
    }

    const auto allPassed = std::all_of(results.begin(), results.end(), [](const auto& r) { return r.pass; });

    if (format == "json")
    {
        std::cout << "{";
        std::cout << "\"ok\":" << (allPassed ? "true" : "false") << ",";
        std::cout << "\"results\":[";
        for (size_t i = 0; i < results.size(); ++i)
        {
            const auto& r = results[i];
            std::cout << "{"
                      << "\"name\":\"" << jsonEscape(r.name) << "\","
                      << "\"pass\":" << (r.pass ? "true" : "false") << ","
                      << "\"message\":\"" << jsonEscape(r.message) << "\""
                      << "}";
            if (i + 1 < results.size())
                std::cout << ",";
        }
        std::cout << "]}\n";
    }
    else
    {
        for (const auto& r : results)
            std::cout << (r.pass ? "PASS " : "FAIL ") << r.name << " - " << r.message << "\n";
        std::cout << (allPassed ? "SELFTEST PASS" : "SELFTEST FAIL") << "\n";
    }

    return allPassed ? 0 : 1;
}
