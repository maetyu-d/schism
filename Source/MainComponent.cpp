#include "MainComponent.h"

#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <sstream>
#include <set>
#include <unordered_set>

MainComponent::MainComponent()
    : tokeniser(std::make_unique<juce::CPlusPlusCodeTokeniser>()),
      codeEditor(codeDocument, tokeniser.get())
{
    menuBar.setModel(this);
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(this);
   #endif
    addAndMakeVisible(menuBar);
    addAndMakeVisible(graphCanvas);
    addAndMakeVisible(splitter);
    addAndMakeVisible(codeEditor);
    addAndMakeVisible(syncLabel);
    addAndMakeVisible(statusLabel);
    addAndMakeVisible(scopeLabel);
    addAndMakeVisible(spectrumLabel);
    addAndMakeVisible(scopeProbeSelect);
    addAndMakeVisible(spectrumProbeSelect);
    addAndMakeVisible(scopeWidget);
    addAndMakeVisible(spectrumWidget);

    setWantsKeyboardFocus(true);
    addKeyListener(this);
    codeEditor.addKeyListener(this);
    graphCanvas.addKeyListener(this);

    scopeLabel.setText("Scope", juce::dontSendNotification);
    spectrumLabel.setText("Spectrum", juce::dontSendNotification);
    scopeLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    spectrumLabel.setColour(juce::Label::textColourId, juce::Colours::lightgrey);
    scopeProbeSelect.addItem("output", 1);
    scopeProbeSelect.setSelectedId(1, juce::dontSendNotification);
    spectrumProbeSelect.addItem("output", 1);
    spectrumProbeSelect.setSelectedId(1, juce::dontSendNotification);

    splitter.onDragStart = [this]
    {
        splitRatioDragStart = splitRatio;
    };
    splitter.onDragged = [this](const int dragDelta)
    {
        const auto w = static_cast<float>(juce::jmax(1, getWidth()));
        splitRatio = juce::jlimit(0.24f, 0.76f, splitRatioDragStart + static_cast<float>(dragDelta) / w);
        resized();
    };

    graphCanvas.onNodeSelectionChanged = [this](const std::string& nodeId)
    {
        if (const auto* range = currentSyncMap.findByNode(nodeId); range != nullptr)
        {
            suppressEditorEvents = true;
            codeEditor.setHighlightedRegion({ range->start, range->end - range->start });
            suppressEditorEvents = false;
            updateSelectionRail("Node: " + juce::String(nodeId));
        }
        else
        {
            suppressEditorEvents = true;
            codeEditor.setHighlightedRegion({ 0, 0 });
            suppressEditorEvents = false;
        }
    };

    graphCanvas.onAddNodeRequested = [this](const std::string& op, const juce::Point<float> pos)
    {
        applyGraphMutation([&](duodsp::ir::Graph& graph)
        {
            const auto id = nextNodeId(op);
            auto defaultPdLabel = [](const std::string& o) -> std::string
            {
                if (o == "sin")
                    return "osc~ 220";
                if (o == "saw")
                    return "phasor~ 110";
                if (o == "tri")
                    return "tri~ 110";
                if (o == "noise")
                    return "noise~ 0.05";
                if (o == "lpf")
                    return "lop~ 1200";
                if (o == "hpf")
                    return "hip~ 1200";
                if (o == "clip")
                    return "clip~ -1 1";
                if (o == "tanh")
                    return "tanh~ 1";
                if (o == "slew")
                    return "slew~ 50";
                if (o == "mtof")
                    return "mtof 69";
                if (o == "out")
                    return "dac~";
                if (o == "add")
                    return "+~";
                if (o == "sub")
                    return "-~";
                if (o == "mul")
                    return "*~";
                if (o == "div")
                    return "/~";
                if (o == "pow")
                    return "pow~ 2";
                if (o == "mod")
                    return "mod~ 1";
                if (o == "cadd")
                    return "+";
                if (o == "csub")
                    return "-";
                if (o == "cmul")
                    return "*";
                if (o == "cdiv")
                    return "/";
                return o;
            };
            graph.nodes.push_back({ id, op, defaultPdLabel(op), std::nullopt });

            if (op == "floatatom")
            {
                if (!graph.nodes.empty())
                    graph.nodes.back().literal = 0.0;
                pendingInlineEditNodeId = id;
            }
            else if (op == "msg")
            {
                if (!graph.nodes.empty())
                    graph.nodes.back().label = "bang";
                pendingInlineEditNodeId = id;
            }
            else if (op == "obj")
            {
                if (!graph.nodes.empty())
                    graph.nodes.back().label = "obj";
                pendingInlineEditNodeId = id;
            }

            if (op != "out" && op != "constant")
                graph.bindings[nextBindingName()] = id;
            nodeLayout[id] = pos;
        });
    };

    graphCanvas.onNodeTextEditRequested = [this](const std::string& nodeId, const std::string& text)
    {
        applyGraphMutation([&](duodsp::ir::Graph& graph)
        {
            if (auto* n = graph.findNode(nodeId); n != nullptr)
            {
                if (n->op == "obj" || n->op == "msg")
                    n->label = text;
                else if (n->op == "floatatom")
                {
                    const auto v = std::strtod(text.c_str(), nullptr);
                    n->literal = v;
                    n->label = text;
                }
                else
                {
                    const auto trimmed = juce::String(text).trim().toStdString();
                    if (trimmed.empty())
                        return;

                    const auto space = trimmed.find(' ');
                    const auto head = space == std::string::npos ? trimmed : trimmed.substr(0, space);
                    auto mapPdHeadToOp = [](const std::string& h) -> std::string
                    {
                        if (h == "osc~")
                            return "sin";
                        if (h == "phasor~")
                            return "saw";
                        if (h == "tri~")
                            return "tri";
                        if (h == "noise~")
                            return "noise";
                        if (h == "lop~")
                            return "lpf";
                        if (h == "hip~")
                            return "hpf";
                        if (h == "clip~")
                            return "clip";
                        if (h == "tanh~")
                            return "tanh";
                        if (h == "slew~")
                            return "slew";
                        if (h == "mtof")
                            return "mtof";
                        if (h == "dac~")
                            return "out";
                        if (h == "+~")
                            return "add";
                        if (h == "-~")
                            return "sub";
                        if (h == "*~")
                            return "mul";
                        if (h == "/~")
                            return "div";
                        if (h == "+")
                            return "cadd";
                        if (h == "-")
                            return "csub";
                        if (h == "*")
                            return "cmul";
                        if (h == "/")
                            return "cdiv";
                        return {};
                    };

                    const auto mapped = mapPdHeadToOp(head);
                    if (!mapped.empty())
                    {
                        n->op = mapped;
                        n->label = trimmed;
                    }
                    else if (n->op == "obj")
                    {
                        n->label = trimmed;
                    }
                    else
                    {
                        // For typed DSP/control objects, keep current op and treat text as parameterized label.
                        n->label = trimmed;
                    }
                }
            }
        });
    };

    graphCanvas.onConnectRequested = [this](const std::string& fromNodeId, const std::string& toNodeId, const int toPort)
    {
        if (const auto issue = duodsp::ir::validateConnection(currentGraph, fromNodeId, toNodeId, toPort); issue.has_value())
        {
            statusLabel.setText("Connection rejected: " + juce::String(issue->message), juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
            return;
        }

        applyGraphMutation([&](duodsp::ir::Graph& graph)
        {
            graph.edges.erase(std::remove_if(graph.edges.begin(), graph.edges.end(), [&](const auto& e)
                             { return e.toNodeId == toNodeId && e.toPort == toPort; }),
                              graph.edges.end());
            graph.edges.push_back({ fromNodeId, toNodeId, toPort });
        });
    };

    graphCanvas.onDeleteNodeRequested = [this](const std::string& nodeId)
    {
        deleteGraphNodesById({ nodeId });
    };
    graphCanvas.onDeleteNodesRequested = [this](const std::vector<std::string>& ids) { deleteGraphNodesById(ids); };
    graphCanvas.onDeleteEdgesRequested = [this](const std::vector<duodsp::ir::Edge>& edges)
    {
        if (edges.empty())
            return;
        applyGraphMutation([&](duodsp::ir::Graph& graph)
        {
            graph.edges.erase(std::remove_if(graph.edges.begin(), graph.edges.end(), [&](const auto& e)
                             {
                                 return std::find_if(edges.begin(), edges.end(), [&](const auto& d)
                                                    { return d.fromNodeId == e.fromNodeId && d.toNodeId == e.toNodeId && d.toPort == e.toPort; }) != edges.end();
                             }),
                              graph.edges.end());
        });
    };

    graphCanvas.onNodeMoved = [this](const std::string& nodeId, const juce::Point<float> p)
    {
        nodeLayout[nodeId] = p;
    };

    codeDocument.addListener(this);
    codeEditor.setFont(juce::FontOptions("Menlo", 14.0f, juce::Font::plain));
    codeEditor.setColour(juce::CodeEditorComponent::backgroundColourId, juce::Colour(0xffe6e6e6));
    codeEditor.setColour(juce::CodeEditorComponent::highlightColourId, juce::Colour(0x4499bbff));
    codeEditor.setColour(juce::CodeEditorComponent::defaultTextColourId, juce::Colours::black);
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberTextId, juce::Colour(0xff2b2b2b));
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour(0xffd8d8d8));
    syncLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);

    setCodeContent("");

    setSize(1320, 860);
    setAudioChannels(0, 2);
    startTimerHz(20);
}

MainComponent::~MainComponent()
{
   #if JUCE_MAC
    juce::MenuBarModel::setMacMainMenu(nullptr);
   #endif
    menuBar.setModel(nullptr);
    shutdownAudio();
}

void MainComponent::prepareToPlay(const int samplesPerBlockExpected, const double sampleRate)
{
    runtime.prepare(sampleRate, samplesPerBlockExpected, 2);
    runtime.setCrossfadeTimeMs(35.0);
    compileFromText();
}

void MainComponent::getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill)
{
    if (bufferToFill.buffer == nullptr)
        return;
    runtime.processBlock(*bufferToFill.buffer, bufferToFill.startSample, bufferToFill.numSamples);
}

void MainComponent::releaseResources() {}

bool MainComponent::keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent)
{
    juce::ignoreUnused(originatingComponent);
    const auto mods = key.getModifiers();
    if (mods.isCommandDown())
    {
        const auto code = static_cast<char>(std::tolower(key.getTextCharacter()));
        if (code == 'z' && mods.isShiftDown())
        {
            redo();
            return true;
        }
        if (code == 'z')
        {
            undo();
            return true;
        }
        if (code == 'c')
        {
            editCopy();
            return true;
        }
        if (code == 'x')
        {
            editCut();
            return true;
        }
        if (code == 'v')
        {
            editPaste();
            return true;
        }
        if (code == 'a')
        {
            editSelectAll();
            return true;
        }
    }
    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (!codeEditor.hasKeyboardFocus(true) && graphCanvas.hasAnySelection())
        {
            graphCanvas.deleteSelection();
            return true;
        }
    }
    return false;
}

void MainComponent::resized()
{
    auto bounds = getLocalBounds().reduced(10);
    menuBar.setBounds(bounds.removeFromTop(22));
    bounds.removeFromTop(4);
    auto topBar = bounds.removeFromTop(28);
    topBar.removeFromLeft(10);
    syncLabel.setBounds(topBar.removeFromLeft(bounds.getWidth() / 3));
    bounds.removeFromTop(8);

    auto left = bounds.removeFromLeft(static_cast<int>(bounds.getWidth() * splitRatio));
    const auto splitterW = 8;
    auto rail = bounds.removeFromLeft(splitterW);
    auto right = bounds;
    rightPaneBounds = right;
    statusLabel.setBounds(right.getX(), topBar.getY(), right.getWidth(), topBar.getHeight());

    auto probesArea = right.removeFromBottom(220);
    probesArea.removeFromTop(6);
    auto scopeArea = probesArea.removeFromTop(104);
    auto scopeRow = scopeArea.removeFromTop(18);
    scopeLabel.setBounds(scopeRow.removeFromLeft(58));
    scopeProbeSelect.setBounds(scopeRow);
    scopeWidget.setBounds(scopeArea);
    probesArea.removeFromTop(8);
    auto specArea = probesArea;
    auto specRow = specArea.removeFromTop(18);
    spectrumLabel.setBounds(specRow.removeFromLeft(70));
    spectrumProbeSelect.setBounds(specRow);
    spectrumWidget.setBounds(specArea);

    graphCanvas.setBounds(left);
    splitter.setBounds(rail.reduced(2, 0));
    codeEditor.setBounds(right);
    juce::ignoreUnused(rail);
}

void MainComponent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff1c1c1c));
    g.setColour(juce::Colour(0xffe6e6e6));
    g.fillRect(rightPaneBounds);
    const auto splitX = static_cast<int>(getWidth() * splitRatio);
    g.setColour(juce::Colour(0xff3a3a3a));
    g.fillRect(splitX - 1, 40, 2, getHeight() - 50);
}

void MainComponent::codeDocumentTextInserted(const juce::String&, int)
{
    if (!suppressEditorEvents)
    {
        preferredPreviousGraphForCompile.reset();
        compilePending = true;
        textEditedSinceLastCompile = true;
        lastTextEditTimeMs = juce::Time::getMillisecondCounterHiRes();
        pendingEditOrigin = EditOrigin::text;
    }
}

void MainComponent::codeDocumentTextDeleted(int, int)
{
    if (!suppressEditorEvents)
    {
        preferredPreviousGraphForCompile.reset();
        compilePending = true;
        textEditedSinceLastCompile = true;
        lastTextEditTimeMs = juce::Time::getMillisecondCounterHiRes();
        pendingEditOrigin = EditOrigin::text;
    }
}

void MainComponent::timerCallback()
{
    if (compilePending)
    {
        compileFromText();
        compilePending = false;
    }

    const auto caret = codeEditor.getCaretPos().getPosition();
    if (caret != lastCaret)
    {
        lastCaret = caret;
        if (const auto* range = currentSyncMap.findByPosition(caret); range != nullptr)
        {
            graphCanvas.selectNodeById(range->nodeId);
            updateSelectionRail("Code -> Node: " + juce::String(range->nodeId));
        }
        else
        {
            graphCanvas.clearSelection();
            updateSelectionRail("Code");
        }
    }

    const auto scopeProbe = scopeProbeSelect.getText().toStdString();
    if (scopeProbe.empty() || scopeProbe == "output")
        scopeWidget.setSamples(runtime.getScopeSnapshot(512));
    else
        scopeWidget.setSamples(runtime.getScopeSnapshotForProbe(scopeProbe, 512));

    const auto spectrumProbe = spectrumProbeSelect.getText().toStdString();
    if (spectrumProbe.empty() || spectrumProbe == "output")
        spectrumWidget.setBins(runtime.getSpectrumSnapshot(48));
    else
        spectrumWidget.setBins(runtime.getSpectrumSnapshotForProbe(spectrumProbe, 48));
}

void MainComponent::setCodeContent(const juce::String& content, const bool queueCompile)
{
    suppressEditorEvents = true;
    codeDocument.replaceAllContent(content);
    suppressEditorEvents = false;
    compilePending = queueCompile;
}

void MainComponent::pushHistorySnapshot(const bool allowCoalesceTextEdit)
{
    const HistoryState snap { codeDocument.getAllContent(), nodeLayout };
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

    if (historyIndex + 1 < history.size())
        history.erase(history.begin() + static_cast<long>(historyIndex + 1), history.end());

    if (!history.empty() && historyIndex < history.size())
    {
        const auto& curr = history[historyIndex];
        if (curr.source == snap.source && curr.layout == snap.layout)
            return;
    }

    if (allowCoalesceTextEdit && !history.empty() && historyIndex == history.size() - 1 && lastHistoryCommitWasText &&
        nowMs - lastHistoryCommitTimeMs < 850.0)
    {
        history.back() = snap;
        historyIndex = history.size() - 1;
        lastHistoryCommitTimeMs = nowMs;
        return;
    }

    history.push_back(snap);
    historyIndex = history.size() - 1;
    lastHistoryCommitTimeMs = nowMs;
    lastHistoryCommitWasText = allowCoalesceTextEdit;
}

void MainComponent::restoreHistory(const size_t index)
{
    if (index >= history.size())
        return;
    isRestoringHistory = true;
    pendingEditOrigin = EditOrigin::history;
    nodeLayout = history[index].layout;
    setCodeContent(history[index].source);
    compileFromText();
    compilePending = false;
    historyIndex = index;
    isRestoringHistory = false;
}

void MainComponent::undo()
{
    if (history.empty() || historyIndex == 0)
        return;
    restoreHistory(historyIndex - 1);
}

void MainComponent::redo()
{
    if (history.empty() || historyIndex + 1 >= history.size())
        return;
    restoreHistory(historyIndex + 1);
}

void MainComponent::applyGraphMutation(const std::function<void(duodsp::ir::Graph&)>& mutator, const bool pushHistorySnapshotBefore)
{
    if (pushHistorySnapshotBefore)
        pushHistorySnapshot(false);

    auto mutated = currentGraph;
    mutator(mutated);

    std::unordered_set<std::string> nodeIds;
    for (const auto& n : mutated.nodes)
        nodeIds.insert(n.id);
    mutated.edges.erase(std::remove_if(mutated.edges.begin(), mutated.edges.end(), [&](const auto& e)
                       { return !nodeIds.contains(e.fromNodeId) || !nodeIds.contains(e.toNodeId); }),
                        mutated.edges.end());
    for (auto it = mutated.bindings.begin(); it != mutated.bindings.end();)
    {
        if (!nodeIds.contains(it->second))
            it = mutated.bindings.erase(it);
        else
            ++it;
    }

    const auto issues = duodsp::ir::validateGraph(mutated);
    if (!issues.empty())
    {
        statusLabel.setText("Graph rejected: " + juce::String(issues.front().message), juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        return;
    }

    std::set<std::string> boundIds;
    for (const auto& [name, id] : mutated.bindings)
    {
        juce::ignoreUnused(name);
        boundIds.insert(id);
    }

    auto nextBindingNameFor = [&](const duodsp::ir::Graph& graph)
    {
        int i = 1;
        while (true)
        {
            const auto name = "n" + std::to_string(i);
            if (!graph.bindings.contains(name))
                return name;
            ++i;
        }
    };

    for (const auto& node : mutated.nodes)
    {
        if (node.op == "out" || node.op == "constant")
            continue;
        if (!boundIds.contains(node.id))
        {
            const auto name = nextBindingNameFor(mutated);
            mutated.bindings[name] = node.id;
            boundIds.insert(node.id);
        }
    }

    currentGraph = mutated;

    const auto pretty = juce::String(duodsp::text::prettyPrint(mutated));
    setCodeContent(pretty, false);
    compilePending = false;

    currentSyncMap.clear();
    const auto prettyStd = pretty.toStdString();
    std::vector<std::pair<std::string, std::string>> bindingPairs;
    for (const auto& b : currentGraph.bindings)
        bindingPairs.push_back(b);
    std::sort(bindingPairs.begin(), bindingPairs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });

    size_t cursor = 0;
    for (const auto& [name, nodeId] : bindingPairs)
    {
        const auto needle = name + " = ";
        auto pos = prettyStd.find(needle, cursor);
        if (pos == std::string::npos)
            pos = prettyStd.find(needle);
        if (pos == std::string::npos)
            continue;
        auto end = prettyStd.find(';', pos);
        if (end == std::string::npos)
            end = pos + needle.size();
        currentSyncMap.addRange(nodeId, static_cast<int>(pos), static_cast<int>(end + 1));
        cursor = end + 1;
    }

    size_t outCursor = 0;
    for (const auto& n : currentGraph.nodes)
    {
        if (n.op != "out")
            continue;
        auto pos = prettyStd.find("dac~(", outCursor);
        if (pos == std::string::npos)
            pos = prettyStd.find("out(", outCursor);
        if (pos == std::string::npos)
            break;
        auto end = prettyStd.find(';', pos);
        if (end == std::string::npos)
            end = pos + 4;
        currentSyncMap.addRange(n.id, static_cast<int>(pos), static_cast<int>(end + 1));
        outCursor = end + 1;
    }

    syncCanvasFromGraph();
    runtime.setGraph(currentGraph);
    refreshProbeSelectors();
    pendingEditOrigin = EditOrigin::none;
    textEditedSinceLastCompile = false;

    pushHistorySnapshot(false);
    statusLabel.setText("Patched", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
}

void MainComponent::refreshProbeSelectors()
{
    auto refreshOne = [](juce::ComboBox& box, const std::vector<std::string>& probeIds)
    {
        const auto prev = box.getText();
        box.clear(juce::dontSendNotification);
        box.addItem("output", 1);
        for (int i = 0; i < static_cast<int>(probeIds.size()); ++i)
            box.addItem(probeIds[static_cast<size_t>(i)], i + 2);

        int prevId = 0;
        for (int i = 0; i < box.getNumItems(); ++i)
        {
            if (box.getItemText(i) == prev)
            {
                prevId = box.getItemId(i);
                break;
            }
        }
        if (prev.isNotEmpty() && prevId != 0)
            box.setSelectedId(prevId, juce::dontSendNotification);
        else
            box.setSelectedId(1, juce::dontSendNotification);
    };

    refreshOne(scopeProbeSelect, runtime.getScopeProbeIds());
    refreshOne(spectrumProbeSelect, runtime.getSpectrumProbeIds());
}

void MainComponent::loadPatchFromFile()
{
    activeLoadChooser = std::make_unique<juce::FileChooser>("Load Schism Patch", juce::File(), "*.schism");
    auto* chooserPtr = activeLoadChooser.get();
    chooserPtr->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                            [this](const juce::FileChooser& chooser)
                            {
                                const auto file = chooser.getResult();
                                if (file == juce::File())
                                    return;

                                duodsp::patch::PatchDocument doc;
                                juce::String err;
                                if (!duodsp::patch::loadFromFile(file, doc, err))
                                {
                                    statusLabel.setText("Load failed: " + err, juce::dontSendNotification);
                                    statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
                                    activeLoadChooser.reset();
                                    return;
                                }

                                splitRatio = juce::jlimit(0.24f, 0.76f, doc.splitRatio);
                                nodeLayout.clear();
                                for (const auto& [id, p] : doc.layout)
                                    nodeLayout[id] = { p.x, p.y };

                                // Load from canonical graph first; avoid reparsing stale/older code text on load.
                                currentGraph = doc.graph;
                                {
                                    std::unordered_set<std::string> ids;
                                    for (const auto& n : currentGraph.nodes)
                                        ids.insert(n.id);
                                    currentGraph.edges.erase(std::remove_if(currentGraph.edges.begin(), currentGraph.edges.end(), [&](const auto& e)
                                                    { return !ids.contains(e.fromNodeId) || !ids.contains(e.toNodeId); }),
                                                             currentGraph.edges.end());
                                    for (auto it = currentGraph.bindings.begin(); it != currentGraph.bindings.end();)
                                    {
                                        if (!ids.contains(it->second))
                                            it = currentGraph.bindings.erase(it);
                                        else
                                            ++it;
                                    }
                                }

                                // Rebuild code pane from loaded graph to keep both views in sync.
                                const auto pretty = juce::String(duodsp::text::prettyPrint(currentGraph));
                                setCodeContent(pretty, false);
                                compilePending = false;

                                // Rebuild sync map from the pretty-printed code for selection linking.
                                currentSyncMap.clear();
                                const auto prettyStd = pretty.toStdString();
                                std::vector<std::pair<std::string, std::string>> bindingPairs;
                                for (const auto& b : currentGraph.bindings)
                                    bindingPairs.push_back(b);
                                std::sort(bindingPairs.begin(), bindingPairs.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
                                size_t cursor = 0;
                                for (const auto& [name, nodeId] : bindingPairs)
                                {
                                    const auto needle = name + " = ";
                                    auto pos = prettyStd.find(needle, cursor);
                                    if (pos == std::string::npos)
                                        pos = prettyStd.find(needle);
                                    if (pos == std::string::npos)
                                        continue;
                                    auto end = prettyStd.find(';', pos);
                                    if (end == std::string::npos)
                                        end = pos + needle.size();
                                    currentSyncMap.addRange(nodeId, static_cast<int>(pos), static_cast<int>(end + 1));
                                    cursor = end + 1;
                                }
                                size_t outCursor = 0;
                                for (const auto& n : currentGraph.nodes)
                                {
                                    if (n.op != "out")
                                        continue;
                                    auto pos = prettyStd.find("dac~(", outCursor);
                                    if (pos == std::string::npos)
                                        break;
                                    auto end = prettyStd.find(';', pos);
                                    if (end == std::string::npos)
                                        end = pos + 5;
                                    currentSyncMap.addRange(n.id, static_cast<int>(pos), static_cast<int>(end + 1));
                                    outCursor = end + 1;
                                }

                                syncCanvasFromGraph();
                                runtime.setGraph(currentGraph);
                                refreshProbeSelectors();
                                compilePending = false;

                                currentPatchFile = file;
                                statusLabel.setText("Loaded: " + file.getFileName(), juce::dontSendNotification);
                                statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
                                resized();
                                activeLoadChooser.reset();
                            });
}

void MainComponent::newPatch()
{
    currentPatchFile = juce::File();
    nodeLayout.clear();
    history.clear();
    historyIndex = 0;
    hasCommittedInitialSnapshot = false;
    setCodeContent("");
    compileFromText();
    compilePending = false;
    statusLabel.setText("New patch", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
}

void MainComponent::savePatchToFile(const bool saveAs)
{
    auto writeFile = [this](juce::File file)
    {
        if (!file.hasFileExtension(".schism"))
            file = file.withFileExtension(".schism");

        // Persist exactly what the user sees on the canvas at save time.
        nodeLayout = graphCanvas.getLayoutById();

        duodsp::patch::PatchDocument doc;
        doc.codeText = codeDocument.getAllContent();
        doc.graph = currentGraph;
        for (const auto& [id, p] : nodeLayout)
            doc.layout[id] = { p.x, p.y };
        doc.splitRatio = splitRatio;

        juce::String err;
        if (duodsp::patch::saveToFile(file, doc, err))
        {
            currentPatchFile = file;
            statusLabel.setText("Saved: " + file.getFileName(), juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
        }
        else
        {
            statusLabel.setText("Save failed: " + err, juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        }
    };

    if (!saveAs && currentPatchFile != juce::File())
    {
        writeFile(currentPatchFile);
        return;
    }

    activeSaveChooser = std::make_unique<juce::FileChooser>("Save Schism Patch", currentPatchFile, "*.schism");
    auto* chooserPtr = activeSaveChooser.get();
    chooserPtr->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::canSelectFiles,
                            [this, writeFile](const juce::FileChooser& chooser)
                            {
                                const auto file = chooser.getResult();
                                if (file == juce::File())
                                    return;
                                writeFile(file);
                                activeSaveChooser.reset();
                            });
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { "File", "Edit" };
}

juce::PopupMenu MainComponent::getMenuForIndex(const int topLevelMenuIndex, const juce::String& menuName)
{
    juce::ignoreUnused(menuName);
    juce::PopupMenu menu;
    if (topLevelMenuIndex == 0)
    {
        menu.addItem(1, "New");
        menu.addSeparator();
        menu.addItem(2, "Open...");
        menu.addItem(3, "Save");
        menu.addItem(4, "Save As...");
        menu.addSeparator();
        menu.addItem(5, "Quit");
    }
    else if (topLevelMenuIndex == 1)
    {
        menu.addItem(101, "Undo");
        menu.addItem(102, "Redo");
        menu.addSeparator();
        menu.addItem(103, "Cut");
        menu.addItem(104, "Copy");
        menu.addItem(105, "Paste");
        menu.addItem(106, "Duplicate");
        menu.addItem(107, "Delete");
        menu.addSeparator();
        menu.addItem(108, "Select All");
    }
    return menu;
}

void MainComponent::menuItemSelected(const int menuItemID, const int topLevelMenuIndex)
{
    juce::ignoreUnused(topLevelMenuIndex);
    switch (menuItemID)
    {
        case 1:
            newPatch();
            break;
        case 2:
            loadPatchFromFile();
            break;
        case 3:
            savePatchToFile(false);
            break;
        case 4:
            savePatchToFile(true);
            break;
        case 5:
            if (auto* app = juce::JUCEApplication::getInstance(); app != nullptr)
                app->systemRequestedQuit();
            break;
        case 101:
            undo();
            break;
        case 102:
            redo();
            break;
        case 103:
            editCut();
            break;
        case 104:
            editCopy();
            break;
        case 105:
            editPaste();
            break;
        case 106:
            editDuplicate();
            break;
        case 107:
            editDelete();
            break;
        case 108:
            editSelectAll();
            break;
        default:
            break;
    }
}

void MainComponent::editCopy()
{
    if (codeEditor.hasKeyboardFocus(true))
    {
        codeEditor.copyToClipboard();
        return;
    }

    const auto selected = graphCanvas.getSelectedNodeIds();
    if (selected.empty())
    {
        graphCanvas.deleteSelection();
        return;
    }

    std::unordered_set<std::string> selectedSet(selected.begin(), selected.end());
    NodeClipboard clip;
    for (const auto& n : currentGraph.nodes)
        if (selectedSet.contains(n.id))
            clip.nodes.push_back(n);
    for (const auto& e : currentGraph.edges)
        if (selectedSet.contains(e.fromNodeId) && selectedSet.contains(e.toNodeId))
            clip.edges.push_back(e);
    for (const auto& id : selected)
        if (const auto it = nodeLayout.find(id); it != nodeLayout.end())
            clip.layout[id] = it->second;
    nodeClipboard = std::move(clip);
}

void MainComponent::editCut()
{
    if (codeEditor.hasKeyboardFocus(true))
    {
        codeEditor.cutToClipboard();
        return;
    }

    editCopy();
    editDelete();
}

void MainComponent::editPaste()
{
    if (codeEditor.hasKeyboardFocus(true))
    {
        codeEditor.pasteFromClipboard();
        return;
    }

    if (!nodeClipboard.has_value() || nodeClipboard->nodes.empty())
        return;

    const auto clip = *nodeClipboard;
    std::unordered_map<std::string, std::string> idMap;
    const auto offset = 24.0f * static_cast<float>(++pasteSerial);

    applyGraphMutation([&](duodsp::ir::Graph& graph)
    {
        for (const auto& node : clip.nodes)
        {
            auto n = node;
            n.id = nextNodeId(node.op);
            idMap[node.id] = n.id;
            graph.nodes.push_back(n);
            if (const auto it = clip.layout.find(node.id); it != clip.layout.end())
                nodeLayout[n.id] = it->second + juce::Point<float>(offset, offset);
        }

        for (const auto& edge : clip.edges)
        {
            const auto fromIt = idMap.find(edge.fromNodeId);
            const auto toIt = idMap.find(edge.toNodeId);
            if (fromIt == idMap.end() || toIt == idMap.end())
                continue;
            graph.edges.push_back({ fromIt->second, toIt->second, edge.toPort });
        }
    });
}

void MainComponent::editDuplicate()
{
    if (codeEditor.hasKeyboardFocus(true))
        return;
    editCopy();
    editPaste();
}

void MainComponent::editDelete()
{
    if (codeEditor.hasKeyboardFocus(true))
    {
        const auto region = codeEditor.getHighlightedRegion();
        if (region.getLength() > 0)
            codeDocument.deleteSection(region.getStart(), region.getEnd());
        return;
    }

    const auto selected = graphCanvas.getSelectedNodeIds();
    if (selected.empty())
        return;
    deleteGraphNodesById(selected);
    graphCanvas.clearSelection();
}

void MainComponent::editSelectAll()
{
    if (codeEditor.hasKeyboardFocus(true))
    {
        codeEditor.selectAll();
        return;
    }
    graphCanvas.selectAllNodes();
}

void MainComponent::deleteGraphNodesById(const std::vector<std::string>& ids)
{
    if (ids.empty())
        return;

    std::unordered_set<std::string> selectedSet(ids.begin(), ids.end());
    applyGraphMutation([&](duodsp::ir::Graph& graph)
    {
        graph.nodes.erase(std::remove_if(graph.nodes.begin(), graph.nodes.end(), [&](const auto& n)
                         { return selectedSet.contains(n.id); }),
                          graph.nodes.end());
        graph.edges.erase(std::remove_if(graph.edges.begin(), graph.edges.end(), [&](const auto& e)
                         { return selectedSet.contains(e.fromNodeId) || selectedSet.contains(e.toNodeId); }),
                          graph.edges.end());
        for (auto it = graph.bindings.begin(); it != graph.bindings.end();)
        {
            if (selectedSet.contains(it->second))
                it = graph.bindings.erase(it);
            else
                ++it;
        }
        for (const auto& id : ids)
            nodeLayout.erase(id);
    });
}

void MainComponent::compileFromText()
{
    const auto source = codeDocument.getAllContent().toStdString();
    const auto* previousGraph = preferredPreviousGraphForCompile.has_value() ? &(*preferredPreviousGraphForCompile) : &currentGraph;
    const auto result = duodsp::text::compile(source, previousGraph);
    preferredPreviousGraphForCompile.reset();
    currentGraph = result.graph;
    currentSyncMap = result.syncMap;

    // Keep explicit DAC routing visible in the code pane even for text-first workflows.
    auto stripDacRoutingBlock = [](std::string text)
    {
        const std::string marker = "\n// === DAC ROUTING ===\n";
        if (const auto pos = text.find(marker); pos != std::string::npos)
            text.erase(pos);
        return text;
    };
    auto collectDacRoutingLines = [](const duodsp::ir::Graph& g)
    {
        std::vector<std::string> lines;
        std::istringstream iss(duodsp::text::prettyPrint(g));
        std::string line;
        while (std::getline(iss, line))
        {
            if (line.find("// dac signal <- ") != std::string::npos)
                lines.push_back(line.substr(line.find("// dac signal <- ")));
        }
        return lines;
    };
    {
        auto base = stripDacRoutingBlock(source);
        const auto routing = collectDacRoutingLines(currentGraph);
        if (!routing.empty())
        {
            while (!base.empty() && (base.back() == '\n' || base.back() == '\r' || base.back() == ' ' || base.back() == '\t'))
                base.pop_back();
            base += "\n\n// === DAC ROUTING ===\n";
            for (const auto& line : routing)
                base += line + "\n";
        }
        if (base != source)
            setCodeContent(juce::String(base), false);
    }

    for (const auto& node : currentGraph.nodes)
    {
        if (!nodeLayout.contains(node.id))
        {
            const auto idx = static_cast<int>(nodeLayout.size());
            nodeLayout[node.id] = { 30.0f + static_cast<float>((idx % 4) * 170), 90.0f + static_cast<float>((idx / 4) * 90) };
        }
    }
    for (auto it = nodeLayout.begin(); it != nodeLayout.end();)
    {
        if (currentGraph.findNode(it->first) == nullptr)
            it = nodeLayout.erase(it);
        else
            ++it;
    }

    syncCanvasFromGraph();
    runtime.setGraph(currentGraph);
    refreshProbeSelectors();
    if (pendingInlineEditNodeId.has_value())
    {
        graphCanvas.beginInlineEdit(*pendingInlineEditNodeId);
        pendingInlineEditNodeId.reset();
    }

    if (!isRestoringHistory)
    {
        if (!hasCommittedInitialSnapshot)
        {
            pushHistorySnapshot(false);
            hasCommittedInitialSnapshot = true;
        }
        else if (pendingEditOrigin == EditOrigin::text && textEditedSinceLastCompile)
        {
            const auto nowMs = juce::Time::getMillisecondCounterHiRes();
            const auto allowCoalesce = nowMs - lastTextEditTimeMs < 1000.0;
            pushHistorySnapshot(allowCoalesce);
        }
        else if (pendingEditOrigin == EditOrigin::visual)
        {
            pushHistorySnapshot(false);
        }
    }

    pendingEditOrigin = EditOrigin::none;
    textEditedSinceLastCompile = false;

    if (result.diagnostics.empty())
    {
        statusLabel.setText("Compiled", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    }
    else
    {
        statusLabel.setText("Compile issues: " + juce::String(static_cast<int>(result.diagnostics.size())) + " (" + juce::String(result.diagnostics.front().message) + ")",
                            juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
    }
}

void MainComponent::syncCanvasFromGraph()
{
    const auto selectedId = graphCanvas.selectedNodeId();
    graphCanvas.setGraph(currentGraph, nodeLayout);
    if (!selectedId.empty())
        graphCanvas.selectNodeById(selectedId);
}

void MainComponent::updateSelectionRail(const juce::String& text)
{
    syncLabel.setText(text, juce::dontSendNotification);
}

std::string MainComponent::nextBindingName() const
{
    int i = 1;
    while (true)
    {
        const auto name = "n" + std::to_string(i);
        if (!currentGraph.bindings.contains(name))
            return name;
        ++i;
    }
}

std::string MainComponent::nextNodeId(const std::string& seed) const
{
    int i = 1;
    while (true)
    {
        const auto id = seed + "_" + std::to_string(i);
        if (currentGraph.findNode(id) == nullptr)
            return id;
        ++i;
    }
}
