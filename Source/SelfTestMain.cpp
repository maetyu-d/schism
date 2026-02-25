#include "DSP/RuntimeEngine.h"
#include "Text/GraphLanguage.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <algorithm>
#include <cmath>
#include <exception>
#include <iostream>
#include <optional>
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
    g.nodes.push_back({ "bus_0", "constant", "0", 0.0 });
    g.nodes.push_back({ "out_1", "out", "out", std::nullopt });
    g.edges.push_back({ "bus_0", "out_1", 0 });
    g.edges.push_back({ constId, "out_1", 1 });
    g.bindings["sig"] = constId;
    return g;
}

void testCompileAndSync()
{
    const std::string source =
        "osc = sin(220);\n"
        "amp = 0.2;\n"
        "sig = osc * amp;\n"
        "out(0, sig);\n";

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
        "out(0, sig);\n";
    const auto a = duodsp::text::compile(sourceA, nullptr);
    require(a.graph.bindings.contains("sig"), "Binding sig missing in compile A.");
    const auto sigIdA = a.graph.bindings.at("sig");

    const std::string sourceB =
        "osc = sin(440);\n"
        "amp = 0.2;\n"
        "sig = osc * amp;\n"
        "out(0, sig);\n";
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

void testTypedPortValidation()
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "osc", "sin", "sin", std::nullopt });
    g.nodes.push_back({ "out", "out", "out", std::nullopt });
    g.nodes.push_back({ "bus", "constant", "0", 0.0 });
    g.edges.push_back({ "bus", "out", 0 });
    g.edges.push_back({ "osc", "out", 1 });

    const auto issue = duodsp::ir::validateConnection(g, "osc", "out", 0);
    require(issue.has_value(), "Audio->control connection should be rejected.");
}

void testProbeTargeting()
{
    duodsp::ir::Graph g;
    g.nodes.push_back({ "src", "constant", "1.0", 1.0 });
    g.nodes.push_back({ "scopeTap", "scope", "scope", std::nullopt });
    g.nodes.push_back({ "specTap", "spectrum", "spectrum", std::nullopt });
    g.nodes.push_back({ "bus", "constant", "0", 0.0 });
    g.nodes.push_back({ "out", "out", "out", std::nullopt });
    g.edges.push_back({ "src", "scopeTap", 0 });
    g.edges.push_back({ "src", "specTap", 0 });
    g.edges.push_back({ "bus", "out", 0 });
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
        { "typed_ports", &testTypedPortValidation },
        { "probe_targeting", &testProbeTargeting },
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
