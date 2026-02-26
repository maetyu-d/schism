#include "MainComponent.h"

#include <algorithm>
#include <cctype>
#include <cmath>
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
    addAndMakeVisible(applyCodeButton);
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
                if (o == "lores")
                    return "lores~ 1200 0.5";
                if (o == "bpf")
                    return "bpf~ 1200 0.7";
                if (o == "svf")
                    return "svf~ 1200 0.7 0";
                if (o == "freeverb")
                    return "freeverb~ 0.75 0.3 1 0.2";
                if (o == "plate")
                    return "plate~ 0.7 0.3 0.2";
                if (o == "reverb")
                    return "reverb~ 0.7 0.3 0.2";
                if (o == "fdn")
                    return "fdn~ 0.7 0.3 0.2";
                if (o == "convrev")
                    return "convrev~ 0.5 0.2";
                if (o == "delay")
                    return "delay~ 250";
                if (o == "cdelay")
                    return "delay 250";
                if (o == "metro")
                    return "metro 250";
                if (o == "apf")
                    return "apf~ 20 0.5";
                if (o == "comb")
                    return "comb~ 30 0.7";
                if (o == "clip")
                    return "clip~ -1 1";
                if (o == "tanh")
                    return "tanh~ 1";
                if (o == "slew")
                    return "slew~ 50";
                if (o == "sah")
                    return "sah~";
                if (o == "sah_c")
                    return "sah";
                if (o == "line")
                    return "line 50";
                if (o == "line_sig")
                    return "line~ 50";
                if (o == "vline")
                    return "vline~ 50";
                if (o == "ad")
                    return "ad~ 10 120";
                if (o == "toggle")
                    return "toggle";
                if (o == "select")
                    return "select 1";
                if (o == "trigger")
                    return "trigger";
                if (o == "pack")
                    return "pack";
                if (o == "unpack")
                    return "unpack";
                if (o == "snapshot")
                    return "snapshot~";
                if (o == "pan")
                    return "pan~ 0.5";
                if (o == "env")
                    return "env~ 50";
                if (o == "peak")
                    return "peak~ 150";
                if (o == "mtof")
                    return "mtof 69";
                if (o == "mtof_sig")
                    return "mtof~ 69";
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
                if (o == "comp_sig")
                    return "comp~";
                if (o == "comp")
                    return "comp";
                if (o == "min_sig")
                    return "min~";
                if (o == "max_sig")
                    return "max~";
                if (o == "min")
                    return "min";
                if (o == "max")
                    return "max";
                if (o == "abs_sig")
                    return "abs~";
                if (o == "abs")
                    return "abs";
                if (o == "random")
                    return "random 0 1";
                if (o == "bang")
                    return "bang";
                if (o == "and_sig")
                    return "and~";
                if (o == "or_sig")
                    return "or~";
                if (o == "xor_sig")
                    return "xor~";
                if (o == "not_sig")
                    return "not~";
                if (o == "and")
                    return "and";
                if (o == "or")
                    return "or";
                if (o == "xor")
                    return "xor";
                if (o == "not")
                    return "not";
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
                        if (h == "lores~")
                            return "lores";
                        if (h == "bpf~")
                            return "bpf";
                        if (h == "svf~")
                            return "svf";
                        if (h == "freeverb~")
                            return "freeverb";
                        if (h == "plate~")
                            return "plate";
                        if (h == "reverb~")
                            return "reverb";
                        if (h == "fdn~")
                            return "fdn";
                        if (h == "convrev~")
                            return "convrev";
                        if (h == "delay~")
                            return "delay";
                        if (h == "delay")
                            return "cdelay";
                        if (h == "apf~")
                            return "apf";
                        if (h == "comb~")
                            return "comb";
                        if (h == "clip~")
                            return "clip";
                        if (h == "tanh~")
                            return "tanh";
                        if (h == "slew~")
                            return "slew";
                        if (h == "sah~")
                            return "sah";
                        if (h == "sah")
                            return "sah_c";
                        if (h == "line")
                            return "line";
                        if (h == "line~")
                            return "line_sig";
                        if (h == "vline~")
                            return "vline";
                        if (h == "ad~")
                            return "ad";
                        if (h == "metro")
                            return "metro";
                        if (h == "toggle")
                            return "toggle";
                        if (h == "select")
                            return "select";
                        if (h == "trigger" || h == "t")
                            return "trigger";
                        if (h == "pack")
                            return "pack";
                        if (h == "unpack")
                            return "unpack";
                        if (h == "snapshot~")
                            return "snapshot";
                        if (h == "pan~")
                            return "pan";
                        if (h == "env~")
                            return "env";
                        if (h == "peak~")
                            return "peak";
                        if (h == "mtof")
                            return "mtof";
                        if (h == "mtof~")
                            return "mtof_sig";
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
                        if (h == "comp~")
                            return "comp_sig";
                        if (h == "comp")
                            return "comp";
                        if (h == "min~")
                            return "min_sig";
                        if (h == "max~")
                            return "max_sig";
                        if (h == "min")
                            return "min";
                        if (h == "max")
                            return "max";
                        if (h == "abs~")
                            return "abs_sig";
                        if (h == "abs")
                            return "abs";
                        if (h == "random")
                            return "random";
                        if (h == "bang")
                            return "bang";
                        if (h == "and~")
                            return "and_sig";
                        if (h == "or~")
                            return "or_sig";
                        if (h == "xor~")
                            return "xor_sig";
                        if (h == "not~")
                            return "not_sig";
                        if (h == "and")
                            return "and";
                        if (h == "or")
                            return "or";
                        if (h == "xor")
                            return "xor";
                        if (h == "not")
                            return "not";
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
    graphCanvas.onBangTriggered = [this](const std::string& nodeId)
    {
        runtime.triggerBang(nodeId);
    };
    graphCanvas.onCopyRequested = [this] { editCopy(); };
    graphCanvas.onCutRequested = [this] { editCut(); };
    graphCanvas.onPasteRequested = [this] { editPaste(); };
    graphCanvas.onSelectAllRequested = [this] { editSelectAll(); };

    codeDocument.addListener(this);
    codeEditor.setFont(juce::FontOptions("Menlo", 14.0f, juce::Font::plain));
    codeEditor.setColour(juce::CodeEditorComponent::backgroundColourId, juce::Colour(0xffe6e6e6));
    codeEditor.setColour(juce::CodeEditorComponent::highlightColourId, juce::Colour(0x4499bbff));
    codeEditor.setColour(juce::CodeEditorComponent::defaultTextColourId, juce::Colours::black);
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberTextId, juce::Colour(0xff2b2b2b));
    codeEditor.setColour(juce::CodeEditorComponent::lineNumberBackgroundId, juce::Colour(0xffd8d8d8));
    syncLabel.setColour(juce::Label::textColourId, juce::Colours::white);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
    applyCodeButton.onClick = [this] { applyPendingCodeEdits(); };
    applyCodeButton.setTooltip("Compile/apply code edits to update patch view");
    applyCodeButton.setColour(juce::TextButton::buttonColourId, juce::Colour(0xff365d8d));
    applyCodeButton.setColour(juce::TextButton::buttonOnColourId, juce::Colour(0xff4f7bb0));
    applyCodeButton.setColour(juce::TextButton::textColourOffId, juce::Colours::white);
    applyCodeButton.setColour(juce::TextButton::textColourOnId, juce::Colours::white);

    setCodeContent("");
    lastAppliedCodeText = codeDocument.getAllContent().toStdString();
    refreshCodeApplyUi();

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
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier | juce::ModifierKeys::shiftModifier, 0))
    {
        redo();
        return true;
    }
    if (key == juce::KeyPress('z', juce::ModifierKeys::commandModifier, 0))
    {
        undo();
        return true;
    }
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        editCopy();
        return true;
    }
    if (key == juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0))
    {
        editCut();
        return true;
    }
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        editPaste();
        return true;
    }
    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
    {
        editSelectAll();
        return true;
    }
    if (key == juce::KeyPress(juce::KeyPress::returnKey, juce::ModifierKeys::commandModifier, 0))
    {
        applyPendingCodeEdits();
        return true;
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
    auto statusArea = juce::Rectangle<int>(right.getX(), topBar.getY(), right.getWidth(), topBar.getHeight());
    applyCodeButton.setBounds(statusArea.removeFromRight(118).reduced(0, 2));
    statusArea.removeFromRight(8);
    statusLabel.setBounds(statusArea);

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

void MainComponent::codeDocumentTextInserted(const juce::String& insertedText, int)
{
    if (!suppressEditorEvents)
    {
        preferredPreviousGraphForCompile.reset();
        compilePending = autoApplyCodeEdits;
        hasUnappliedCodeEdits = true;
        autoApplyPreflightSnapshotTaken = false;
        if (insertedText.containsChar('\n') || insertedText.containsChar(';') || insertedText.containsChar('}'))
            autoApplySawStatementBoundary = true;
        textEditedSinceLastCompile = true;
        lastTextEditTimeMs = juce::Time::getMillisecondCounterHiRes();
        pendingEditOrigin = EditOrigin::text;
        if (!autoApplyCodeEdits)
        {
            statusLabel.setText("Code edited (not applied)", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        }
        refreshCodeApplyUi();
    }
}

void MainComponent::codeDocumentTextDeleted(int, int)
{
    if (!suppressEditorEvents)
    {
        preferredPreviousGraphForCompile.reset();
        compilePending = autoApplyCodeEdits;
        hasUnappliedCodeEdits = true;
        autoApplyPreflightSnapshotTaken = false;
        textEditedSinceLastCompile = true;
        lastTextEditTimeMs = juce::Time::getMillisecondCounterHiRes();
        pendingEditOrigin = EditOrigin::text;
        if (!autoApplyCodeEdits)
        {
            statusLabel.setText("Code edited (not applied)", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        }
        refreshCodeApplyUi();
    }
}

void MainComponent::timerCallback()
{
    if (compilePending)
    {
        bool shouldCompileNow = true;
        if (autoApplyCodeEdits && pendingEditOrigin == EditOrigin::text)
        {
            const auto nowMs = juce::Time::getMillisecondCounterHiRes();
            const auto idleMs = nowMs - lastTextEditTimeMs;
            constexpr double autoApplyDebounceMs = 1600.0;
            constexpr double autoApplyForceAfterMs = 5000.0;
            if (idleMs < autoApplyDebounceMs)
                shouldCompileNow = false;
            if (!autoApplySawStatementBoundary && idleMs < autoApplyForceAfterMs)
                shouldCompileNow = false;
            if (shouldCompileNow && !autoApplyPreflightSnapshotTaken && !isRestoringHistory)
            {
                // Keep a rollback point before any automatic graph change.
                pushHistorySnapshot(false);
                autoApplyPreflightSnapshotTaken = true;
            }
        }
        if (shouldCompileNow)
        {
            compileFromText();
            compilePending = false;
        }
    }

    const auto triggeredBangs = runtime.consumeTriggeredBangIds();
    for (const auto& id : triggeredBangs)
        graphCanvas.flashBang(id);

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

    const auto floatValues = runtime.getFloatatomValues();
    const auto hasFloatatom = std::any_of(currentGraph.nodes.begin(), currentGraph.nodes.end(), [](const auto& n)
    {
        return n.op == "floatatom";
    });
    if (!hasFloatatom)
    {
        lastKnownFloatatomValues.clear();
        graphCanvas.setFloatatomLiveValues({});
    }
    else
    {
        if (!floatValues.empty())
            lastKnownFloatatomValues = floatValues;
        graphCanvas.setFloatatomLiveValues(lastKnownFloatatomValues);
    }

    refreshCodeApplyUi();
}

void MainComponent::applyPendingCodeEdits()
{
    if (!hasUnappliedCodeEdits)
        return;
    autoApplySawStatementBoundary = true;
    compilePending = true;
    compileFromText();
    compilePending = false;
    refreshCodeApplyUi();
}

void MainComponent::refreshCodeApplyUi()
{
    applyCodeButton.setEnabled(hasUnappliedCodeEdits);
    applyCodeButton.setButtonText(hasUnappliedCodeEdits ? "Apply Code*" : "Apply Code");
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
    hasUnappliedCodeEdits = false;
    refreshCodeApplyUi();
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

    const auto pretty = juce::String(duodsp::text::prettyPrint(mutated, codeViewVerbose));
    setCodeContent(pretty, false);
    lastAppliedCodeText = pretty.toStdString();
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
                                const auto pretty = juce::String(duodsp::text::prettyPrint(currentGraph, codeViewVerbose));
                                setCodeContent(pretty, false);
                                compilePending = false;
                                hasUnappliedCodeEdits = false;
                                refreshCodeApplyUi();

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
                                hasUnappliedCodeEdits = false;
                                autoApplySawStatementBoundary = false;
                                autoApplyPreflightSnapshotTaken = false;
                                lastAppliedCodeText = codeDocument.getAllContent().toStdString();
                                refreshCodeApplyUi();

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
    lastAppliedCodeText = codeDocument.getAllContent().toStdString();
    compileFromText();
    compilePending = false;
    hasUnappliedCodeEdits = false;
    refreshCodeApplyUi();
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
        juce::PopupMenu preferences;
        preferences.addItem(6, "Verbose Code View", true, codeViewVerbose);
        preferences.addItem(7, "Auto Apply Code While Typing", true, autoApplyCodeEdits);
        preferences.addItem(8, "Safe Text Apply (Topology Lock)", true, strictTextApplySafety);
        menu.addItem(1, "New");
        menu.addSeparator();
        menu.addItem(2, "Open...");
        menu.addItem(3, "Save");
        menu.addItem(4, "Save As...");
        menu.addSeparator();
        menu.addSubMenu("Preferences", preferences);
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
        case 6:
        {
            codeViewVerbose = !codeViewVerbose;
            const auto pretty = juce::String(duodsp::text::prettyPrint(currentGraph, codeViewVerbose));
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
            statusLabel.setText(codeViewVerbose ? "Code view: verbose" : "Code view: compact", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
            break;
        }
        case 7:
        {
            autoApplyCodeEdits = !autoApplyCodeEdits;
            statusLabel.setText(autoApplyCodeEdits ? "Code auto-apply: on" : "Code auto-apply: off", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
            if (autoApplyCodeEdits && hasUnappliedCodeEdits)
                applyPendingCodeEdits();
            refreshCodeApplyUi();
            break;
        }
        case 8:
        {
            strictTextApplySafety = !strictTextApplySafety;
            statusLabel.setText(strictTextApplySafety ? "Text apply safety: topology lock on" : "Text apply safety: topology lock off",
                                juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
            break;
        }
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
        return;

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
    const auto previousGraphSnapshot = currentGraph;
    const auto previousSyncSnapshot = currentSyncMap;
    const auto previousLayoutSnapshot = nodeLayout;
    const auto result = duodsp::text::compile(source, previousGraph);
    preferredPreviousGraphForCompile.reset();
    if (!result.diagnostics.empty())
    {
        // Keep last valid graph/audio running while user text is invalid.
        statusLabel.setText("Compile issues: " + juce::String(static_cast<int>(result.diagnostics.size())) + " (" + juce::String(result.diagnostics.front().message) + ")",
                            juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
        hasUnappliedCodeEdits = true;
        refreshCodeApplyUi();
        return;
    }

    auto usedSafeParamTransplant = false;
    std::optional<duodsp::ir::Graph> safeMergedGraph;
    if (pendingEditOrigin == EditOrigin::text && !previousGraphSnapshot.nodes.empty())
    {
        auto normalizeNumericAgnostic = [](const std::string& text)
        {
            std::string stripped = text;
            const std::string marker = "\n// === DAC ROUTING ===\n";
            if (const auto pos = stripped.find(marker); pos != std::string::npos)
                stripped.erase(pos);

            std::string out;
            out.reserve(stripped.size());
            for (size_t i = 0; i < stripped.size();)
            {
                const auto c = stripped[i];
                if (c == '/' && i + 1 < stripped.size() && stripped[i + 1] == '/')
                {
                    while (i < stripped.size() && stripped[i] != '\n')
                        ++i;
                    continue;
                }
                const auto identStart = std::isalpha(static_cast<unsigned char>(c)) || c == '_';
                if (identStart)
                {
                    size_t j = i + 1;
                    while (j < stripped.size())
                    {
                        const auto cj = stripped[j];
                        if (!std::isalnum(static_cast<unsigned char>(cj)) && cj != '_' && cj != '~')
                            break;
                        ++j;
                    }
                    out.append(stripped.substr(i, j - i));
                    i = j;
                    continue;
                }
                const auto isNumericStart = std::isdigit(static_cast<unsigned char>(c))
                                            || (c == '-' && i + 1 < stripped.size() && std::isdigit(static_cast<unsigned char>(stripped[i + 1])));
                if (isNumericStart)
                {
                    size_t j = i + (c == '-' ? 1 : 0);
                    while (j < stripped.size() && std::isdigit(static_cast<unsigned char>(stripped[j])))
                        ++j;
                    if (j < stripped.size() && stripped[j] == '.')
                    {
                        ++j;
                        while (j < stripped.size() && std::isdigit(static_cast<unsigned char>(stripped[j])))
                            ++j;
                    }
                    i = j;
                    continue;
                }
                if (!std::isspace(static_cast<unsigned char>(c)))
                    out.push_back(c);
                ++i;
            }
            return out;
        };
        auto localNodeSignature = [](const duodsp::ir::Graph& g, const duodsp::ir::Node& n)
        {
            std::vector<std::string> ins;
            int outDegree = 0;
            for (const auto& e : g.edges)
            {
                if (e.toNodeId == n.id)
                {
                    const auto* src = g.findNode(e.fromNodeId);
                    const auto srcOp = src != nullptr ? src->op : "missing";
                    ins.push_back(std::to_string(e.toPort) + ":" + srcOp);
                }
                if (e.fromNodeId == n.id)
                    ++outDegree;
            }
            std::sort(ins.begin(), ins.end());
            std::string sig = n.op + "|out=" + std::to_string(outDegree) + "|in=";
            for (size_t i = 0; i < ins.size(); ++i)
            {
                sig += ins[i];
                if (i + 1 < ins.size())
                    sig += ",";
            }
            return sig;
        };
        auto transplantParamsOntoPrevious = [&](const duodsp::ir::Graph& previousG, const duodsp::ir::Graph& nextG)
        {
            auto merged = previousG;
            auto extractNumericValue = [&](const duodsp::ir::Node& node) -> std::optional<double>
            {
                if (node.op == "constant")
                    return node.literal;
                if (node.op == "floatatom")
                {
                    if (node.literal.has_value())
                        return node.literal;
                    for (const auto& e : nextG.edges)
                    {
                        if (e.toNodeId != node.id || e.toPort != 0)
                            continue;
                        const auto* src = nextG.findNode(e.fromNodeId);
                        if (src != nullptr && src->op == "constant" && src->literal.has_value())
                            return src->literal;
                    }
                }
                return std::nullopt;
            };
            auto inferredFloatatomValue = [&](const duodsp::ir::Node& nextNode) -> std::optional<double>
            {
                if (nextNode.op != "floatatom")
                    return nextNode.literal;
                for (const auto& e : nextG.edges)
                {
                    if (e.toNodeId != nextNode.id || e.toPort != 0)
                        continue;
                    const auto* src = nextG.findNode(e.fromNodeId);
                    if (src != nullptr && src->op == "constant" && src->literal.has_value())
                        return src->literal;
                }
                return nextNode.literal;
            };
            std::unordered_map<std::string, const duodsp::ir::Node*> nextById;
            nextById.reserve(nextG.nodes.size());
            for (const auto& n : nextG.nodes)
                nextById[n.id] = &n;

            std::unordered_set<std::string> usedNextIds;
            auto applyNode = [&](duodsp::ir::Node& dst, const duodsp::ir::Node& src) -> bool
            {
                const auto numericCompat = (dst.op == "floatatom" || dst.op == "constant") && (src.op == "floatatom" || src.op == "constant");
                if (dst.op != src.op && !numericCompat)
                    return false;
                auto changed = false;
                auto srcLabel = src.label;
                auto srcLiteral = src.literal;
                if (dst.op == "floatatom" && numericCompat)
                {
                    if (const auto v = extractNumericValue(src); v.has_value())
                    {
                        srcLiteral = v;
                        std::ostringstream s;
                        s << *v;
                        srcLabel = s.str();
                    }
                }
                else if (dst.op == "constant" && numericCompat)
                {
                    if (const auto v = extractNumericValue(src); v.has_value())
                    {
                        srcLiteral = v;
                        std::ostringstream s;
                        s << *v;
                        srcLabel = s.str();
                    }
                }
                else if (dst.op == "floatatom")
                {
                    if (const auto inferred = inferredFloatatomValue(src); inferred.has_value())
                    {
                        srcLiteral = inferred;
                        std::ostringstream s;
                        s << *inferred;
                        srcLabel = s.str();
                    }
                }
                else
                {
                    // For non-numeric carriers, only accept label updates that include at least one
                    // numeric token. This keeps argument-bearing objects (e.g. random 0 500) in sync,
                    // while avoiding regressions where bare op names overwrite rich labels.
                    auto hasNumericToken = [](const std::string& label)
                    {
                        std::istringstream iss(label);
                        std::string tok;
                        while (iss >> tok)
                        {
                            char* end = nullptr;
                            const auto v = std::strtod(tok.c_str(), &end);
                            juce::ignoreUnused(v);
                            if (end != tok.c_str() && end != nullptr && *end == '\0')
                                return true;
                        }
                        return false;
                    };
                    if (hasNumericToken(srcLabel) && dst.label != srcLabel)
                    {
                        dst.label = srcLabel;
                        changed = true;
                    }
                    usedNextIds.insert(src.id);
                    return changed;
                }
                if (dst.label != srcLabel)
                {
                    dst.label = srcLabel;
                    changed = true;
                }
                if (dst.literal != srcLiteral)
                {
                    dst.literal = srcLiteral;
                    changed = true;
                }
                usedNextIds.insert(src.id);
                return changed;
            };

            int changed = 0;
            // Phase 1: stable id
            for (auto& oldNode : merged.nodes)
            {
                if (!nextById.contains(oldNode.id))
                    continue;
                if (applyNode(oldNode, *nextById[oldNode.id]))
                    ++changed;
            }

            // Phase 2: binding name
            for (const auto& [name, oldId] : previousG.bindings)
            {
                if (!nextG.bindings.contains(name))
                    continue;
                const auto& nextId = nextG.bindings.at(name);
                if (!nextById.contains(nextId) || usedNextIds.contains(nextId))
                    continue;
                auto* oldNode = merged.findNode(oldId);
                if (oldNode == nullptr)
                    continue;
                if (applyNode(*oldNode, *nextById[nextId]))
                    ++changed;
            }

            // Phase 3: local structural signature
            std::unordered_map<std::string, std::vector<const duodsp::ir::Node*>> nextBySig;
            for (const auto& n : nextG.nodes)
            {
                if (usedNextIds.contains(n.id))
                    continue;
                nextBySig[localNodeSignature(nextG, n)].push_back(&n);
            }
            for (auto& oldNode : merged.nodes)
            {
                if (usedNextIds.contains(oldNode.id))
                    continue;
                const auto sig = localNodeSignature(previousG, oldNode);
                if (!nextBySig.contains(sig))
                    continue;
                auto& cands = nextBySig[sig];
                while (!cands.empty() && usedNextIds.contains(cands.back()->id))
                    cands.pop_back();
                if (cands.empty())
                    continue;
                if (applyNode(oldNode, *cands.back()))
                    ++changed;
            }

            // Phase 4: op-order fallback
            std::unordered_map<std::string, std::vector<const duodsp::ir::Node*>> nextByOp;
            std::unordered_map<std::string, size_t> opCursor;
            for (const auto& n : nextG.nodes)
            {
                if (!usedNextIds.contains(n.id))
                    nextByOp[n.op].push_back(&n);
            }
            for (auto& oldNode : merged.nodes)
            {
                auto& vec = nextByOp[oldNode.op];
                auto& cursor = opCursor[oldNode.op];
                if (cursor >= vec.size())
                    continue;
                if (applyNode(oldNode, *vec[cursor]))
                    ++changed;
                ++cursor;
            }

            return std::pair { merged, changed };
        };
        const auto oldNodeCount = static_cast<int>(previousGraphSnapshot.nodes.size());
        const auto newNodeCount = static_cast<int>(result.graph.nodes.size());
        const auto oldEdgeCount = static_cast<int>(previousGraphSnapshot.edges.size());
        const auto newEdgeCount = static_cast<int>(result.graph.edges.size());
        const auto oldOutCount = static_cast<int>(std::count_if(previousGraphSnapshot.nodes.begin(), previousGraphSnapshot.nodes.end(),
                                                                [](const auto& n)
                                                                { return n.op == "out"; }));
        const auto newOutCount = static_cast<int>(std::count_if(result.graph.nodes.begin(), result.graph.nodes.end(),
                                                                [](const auto& n)
                                                                { return n.op == "out"; }));

        const auto suspiciousShrink = oldNodeCount >= 6 && newNodeCount <= oldNodeCount / 2;
        const auto suspiciousEdgeDrop = oldEdgeCount >= 6 && newEdgeCount <= oldEdgeCount / 3;
        const auto suspiciousLostOutput = oldOutCount > 0 && newOutCount == 0;
        if (suspiciousShrink || suspiciousEdgeDrop || suspiciousLostOutput)
        {
            statusLabel.setText("Apply blocked: suspicious destructive graph change", juce::dontSendNotification);
            statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
            hasUnappliedCodeEdits = true;
            refreshCodeApplyUi();
            return;
        }

        if (strictTextApplySafety)
        {
            auto opHistogram = [](const duodsp::ir::Graph& g)
            {
                std::unordered_map<std::string, int> hist;
                for (const auto& n : g.nodes)
                    ++hist[n.op];
                return hist;
            };
            auto edgeHistogram = [](const duodsp::ir::Graph& g)
            {
                std::unordered_map<std::string, int> hist;
                for (const auto& e : g.edges)
                {
                    const auto* from = g.findNode(e.fromNodeId);
                    const auto* to = g.findNode(e.toNodeId);
                    if (from == nullptr || to == nullptr)
                        continue;
                    const auto key = from->op + "->" + to->op + ":" + std::to_string(e.toPort);
                    ++hist[key];
                }
                return hist;
            };
            auto bindingShape = [](const duodsp::ir::Graph& g)
            {
                std::unordered_map<std::string, std::string> shape;
                for (const auto& [name, id] : g.bindings)
                {
                    const auto* n = g.findNode(id);
                    shape[name] = n != nullptr ? n->op : std::string("missing");
                }
                return shape;
            };

            const auto sameTopology = opHistogram(previousGraphSnapshot) == opHistogram(result.graph)
                                      && edgeHistogram(previousGraphSnapshot) == edgeHistogram(result.graph)
                                      && bindingShape(previousGraphSnapshot) == bindingShape(result.graph);
            auto needsSafeTransplant = false;
            if (!sameTopology)
            {
                const auto numericOnlyEdit = normalizeNumericAgnostic(source) == normalizeNumericAgnostic(lastAppliedCodeText);
                if (!numericOnlyEdit)
                {
                    statusLabel.setText("Apply blocked: text edit changed graph topology (safe mode)", juce::dontSendNotification);
                    statusLabel.setColour(juce::Label::textColourId, juce::Colours::orange);
                    hasUnappliedCodeEdits = true;
                    refreshCodeApplyUi();
                    return;
                }
                needsSafeTransplant = true;
            }

            if (needsSafeTransplant)
            {
                const auto [merged, changed] = transplantParamsOntoPrevious(previousGraphSnapshot, result.graph);
                safeMergedGraph = merged;
                usedSafeParamTransplant = changed > 0;
            }
        }
    }

    if (safeMergedGraph.has_value())
        currentGraph = *safeMergedGraph;
    else
        currentGraph = result.graph;
    currentSyncMap = result.syncMap;

    // Robust numeric binding sync:
    // when code edits change a bound numeric value (e.g. tempo 50->500),
    // propagate that value onto the existing bound numeric carrier node.
    auto bindingNumericValue = [&](const duodsp::ir::Graph& g, const std::string& bindingName) -> std::optional<double>
    {
        if (!g.bindings.contains(bindingName))
            return std::nullopt;
        const auto& id = g.bindings.at(bindingName);
        const auto* n = g.findNode(id);
        if (n == nullptr)
            return std::nullopt;
        if (n->op == "constant")
            return n->literal;
        if (n->op == "floatatom")
        {
            if (n->literal.has_value())
                return n->literal;
            for (const auto& e : g.edges)
            {
                if (e.toNodeId != n->id || e.toPort != 0)
                    continue;
                const auto* src = g.findNode(e.fromNodeId);
                if (src != nullptr && src->op == "constant" && src->literal.has_value())
                    return src->literal;
            }
        }
        return std::nullopt;
    };
    for (const auto& [bindingName, nodeId] : currentGraph.bindings)
    {
        const auto v = bindingNumericValue(result.graph, bindingName);
        if (!v.has_value())
            continue;
        if (auto* dst = currentGraph.findNode(nodeId); dst != nullptr)
        {
            if (dst->op == "floatatom" || dst->op == "constant")
            {
                dst->literal = v;
                std::ostringstream s;
                s << *v;
                dst->label = s.str();
            }
        }
    }

    // Normalize floatatom stored values from text representation:
    // - floatatom(<number>) is compiled as constant -> floatatom
    // - some forms may keep number in label text
    // Mirror whichever is present into literal so number boxes and runtime state stay in sync.
    for (auto& node : currentGraph.nodes)
    {
        if (node.op != "floatatom")
            continue;
        std::optional<double> inferred = node.literal;
        for (const auto& e : currentGraph.edges)
        {
            if (e.toNodeId != node.id || e.toPort != 0)
                continue;
            const auto* src = currentGraph.findNode(e.fromNodeId);
            if (src != nullptr && src->op == "constant" && src->literal.has_value())
            {
                inferred = src->literal;
                break;
            }
        }
        if (!inferred.has_value())
        {
            std::istringstream iss(node.label);
            std::string tok;
            while (iss >> tok)
            {
                char* end = nullptr;
                const auto v = std::strtod(tok.c_str(), &end);
                if (end != tok.c_str() && end != nullptr && *end == '\0')
                    inferred = v;
            }
            if (!inferred.has_value())
            {
                char* end = nullptr;
                const auto v = std::strtod(node.label.c_str(), &end);
                if (end != node.label.c_str() && end != nullptr && *end == '\0')
                    inferred = v;
            }
        }
        if (inferred.has_value())
        {
            node.literal = inferred;
            std::ostringstream s;
            s << *inferred;
            node.label = s.str();
        }
    }

    // Keep explicit DAC routing visible in the code pane even for text-first workflows.
    auto stripDacRoutingBlock = [](std::string text)
    {
        const std::string marker = "\n// === DAC ROUTING ===\n";
        if (const auto pos = text.find(marker); pos != std::string::npos)
            text.erase(pos);
        return text;
    };
    auto collectDacRoutingLines = [this](const duodsp::ir::Graph& g)
    {
        std::vector<std::string> lines;
        std::istringstream iss(duodsp::text::prettyPrint(g, codeViewVerbose));
        std::string line;
        while (std::getline(iss, line))
        {
            if (const auto pos = line.find("// dac "); pos != std::string::npos)
                lines.push_back(line.substr(pos));
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

    std::unordered_set<std::string> usedOldIds;
    for (const auto& node : currentGraph.nodes)
    {
        if (nodeLayout.contains(node.id))
            usedOldIds.insert(node.id);
    }

    auto equalLiterals = [](const std::optional<double>& a, const std::optional<double>& b)
    {
        if (!a.has_value() && !b.has_value())
            return true;
        if (a.has_value() != b.has_value())
            return false;
        return std::abs(*a - *b) < 1.0e-9;
    };

    // Fallback remap for layout stability when node IDs cannot be preserved.
    for (const auto& node : currentGraph.nodes)
    {
        if (nodeLayout.contains(node.id))
            continue;
        for (const auto& oldNode : previousGraphSnapshot.nodes)
        {
            if (usedOldIds.contains(oldNode.id))
                continue;
            if (oldNode.op != node.op || oldNode.label != node.label || !equalLiterals(oldNode.literal, node.literal))
                continue;
            if (const auto it = nodeLayout.find(oldNode.id); it != nodeLayout.end())
            {
                nodeLayout[node.id] = it->second;
                usedOldIds.insert(oldNode.id);
                break;
            }
        }
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

    // Transaction guard: if committed graph is invalid, rollback immediately.
    const auto postIssues = duodsp::ir::validateGraph(currentGraph);
    if (!postIssues.empty())
    {
        currentGraph = previousGraphSnapshot;
        currentSyncMap = previousSyncSnapshot;
        nodeLayout = previousLayoutSnapshot;
        syncCanvasFromGraph();
        runtime.setGraph(currentGraph);
        refreshProbeSelectors();
        statusLabel.setText("Apply rolled back: " + juce::String(postIssues.front().message), juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
        hasUnappliedCodeEdits = true;
        refreshCodeApplyUi();
        return;
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
    hasUnappliedCodeEdits = false;
    autoApplySawStatementBoundary = false;
    autoApplyPreflightSnapshotTaken = false;
    lastAppliedCodeText = codeDocument.getAllContent().toStdString();
    lastKnownFloatatomValues.clear();
    for (const auto& n : currentGraph.nodes)
    {
        if (n.op == "floatatom")
            lastKnownFloatatomValues[n.id] = static_cast<float>(n.literal.value_or(0.0));
    }
    refreshCodeApplyUi();
    statusLabel.setText(usedSafeParamTransplant ? "Compiled (safe param apply)" : "Compiled", juce::dontSendNotification);
    statusLabel.setColour(juce::Label::textColourId, juce::Colours::lightgreen);
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
