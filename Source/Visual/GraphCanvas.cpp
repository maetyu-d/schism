#include "GraphCanvas.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <unordered_map>

namespace duodsp::visual
{
namespace
{
constexpr float pdBaseFontSize = 13.0f;
constexpr float minNodeWidth = 36.0f;
constexpr float nodeHeight = 18.0f;
constexpr float portHalf = 4.2f;
constexpr float minHit = 5.0f;
constexpr float portHitRadiusPx = 16.0f;
constexpr float outputHitRadiusPx = 16.0f;
constexpr float cableHitRadiusPx = 6.0f;
constexpr float canvasInset = 0.0f;

juce::Font pdFont(const float size)
{
    return juce::Font(juce::FontOptions("Monaco", size, juce::Font::plain));
}

juce::String pdObjectTextForNode(const ir::Node& node)
{
    if (!node.label.empty() && node.op != "msg" && node.op != "floatatom")
        return juce::String(node.label);
    if (node.op == "sin")
        return "osc~ 220";
    if (node.op == "saw")
        return "phasor~ 110";
    if (node.op == "tri")
        return "tri~ 110";
    if (node.op == "square")
        return "pulse~ 110 0.5";
    if (node.op == "noise")
        return "noise~ 0.05";
    if (node.op == "lpf")
        return "lop~ 1200";
    if (node.op == "hpf")
        return "hip~ 1200";
    if (node.op == "lores")
        return "lores~ 1200 0.5";
    if (node.op == "bpf")
        return "bpf~ 1200 0.7";
    if (node.op == "svf")
        return "svf~ 1200 0.7 0";
    if (node.op == "delay")
        return "delay~ 250";
    if (node.op == "cdelay")
        return "delay 250";
    if (node.op == "apf")
        return "apf~ 20 0.5";
    if (node.op == "comb")
        return "comb~ 30 0.7";
    if (node.op == "clip")
        return "clip~ -1 1";
    if (node.op == "tanh")
        return "tanh~ 1";
    if (node.op == "slew")
        return "slew~ 50";
    if (node.op == "sah")
        return "sah~";
    if (node.op == "mtof")
        return "mtof";
    if (node.op == "mtof_sig")
        return "mtof~";
    if (node.op == "out")
        return "dac~";
    if (node.op == "add")
        return "+~";
    if (node.op == "sub")
        return "-~";
    if (node.op == "mul")
        return "*~";
    if (node.op == "div")
        return "/~";
    if (node.op == "cadd")
        return "+";
    if (node.op == "csub")
        return "-";
    if (node.op == "cmul")
        return "*";
    if (node.op == "cdiv")
        return "/";
    if (node.op == "comp_sig")
        return "comp~";
    if (node.op == "comp")
        return "comp";
    if (node.op == "min_sig")
        return "min~";
    if (node.op == "max_sig")
        return "max~";
    if (node.op == "min")
        return "min";
    if (node.op == "max")
        return "max";
    if (node.op == "abs_sig")
        return "abs~";
    if (node.op == "abs")
        return "abs";
    if (node.op == "random")
        return "random 0 1";
    if (node.op == "and_sig")
        return "and~";
    if (node.op == "or_sig")
        return "or~";
    if (node.op == "xor_sig")
        return "xor~";
    if (node.op == "not_sig")
        return "not~";
    if (node.op == "and")
        return "and";
    if (node.op == "or")
        return "or";
    if (node.op == "xor")
        return "xor";
    if (node.op == "not")
        return "not";
    if (node.op == "pow")
        return "pow~";
    if (node.op == "mod")
        return "mod~";
    if (node.op == "delay1")
        return "z^-1~";
    if (node.op == "scope")
        return "scope~";
    if (node.op == "spectrum")
        return "spectrum~";
    if (node.op == "msg")
        return node.label.empty() ? "message" : juce::String(node.label);
    if (node.op == "bang")
        return "bang";
    if (node.op == "floatatom")
        return node.literal.has_value() ? juce::String(*node.literal, 3) : "0";
    if (node.op == "constant")
        return juce::String(node.literal.value_or(0.0), 3);
    if (node.op == "obj")
        return node.label.empty() ? "obj" : juce::String(node.label);
    return juce::String(node.op);
}

juce::String canonicalPdHeadForOp(const std::string& op)
{
    if (op == "sin")
        return "osc~";
    if (op == "saw")
        return "phasor~";
    if (op == "tri")
        return "tri~";
    if (op == "square")
        return "pulse~";
    if (op == "noise")
        return "noise~";
    if (op == "lpf")
        return "lop~";
    if (op == "hpf")
        return "hip~";
    if (op == "lores")
        return "lores~";
    if (op == "bpf")
        return "bpf~";
    if (op == "svf")
        return "svf~";
    if (op == "delay")
        return "delay~";
    if (op == "cdelay")
        return "delay";
    if (op == "apf")
        return "apf~";
    if (op == "comb")
        return "comb~";
    if (op == "clip")
        return "clip~";
    if (op == "tanh")
        return "tanh~";
    if (op == "slew")
        return "slew~";
    if (op == "sah")
        return "sah~";
    if (op == "mtof")
        return "mtof";
    if (op == "mtof_sig")
        return "mtof~";
    if (op == "out")
        return "dac~";
    if (op == "add")
        return "+~";
    if (op == "sub")
        return "-~";
    if (op == "mul")
        return "*~";
    if (op == "div")
        return "/~";
    if (op == "pow")
        return "pow~";
    if (op == "mod")
        return "mod~";
    if (op == "cadd")
        return "+";
    if (op == "csub")
        return "-";
    if (op == "cmul")
        return "*";
    if (op == "cdiv")
        return "/";
    if (op == "bang")
        return "bang";
    if (op == "comp_sig")
        return "comp~";
    if (op == "comp")
        return "comp";
    if (op == "min_sig")
        return "min~";
    if (op == "max_sig")
        return "max~";
    if (op == "min")
        return "min";
    if (op == "max")
        return "max";
    if (op == "abs_sig")
        return "abs~";
    if (op == "abs")
        return "abs";
    if (op == "random")
        return "random";
    if (op == "and_sig")
        return "and~";
    if (op == "or_sig")
        return "or~";
    if (op == "xor_sig")
        return "xor~";
    if (op == "not_sig")
        return "not~";
    if (op == "and")
        return "and";
    if (op == "or")
        return "or";
    if (op == "xor")
        return "xor";
    if (op == "not")
        return "not";
    return {};
}
} // namespace

GraphCanvas::GraphCanvas()
{
    setWantsKeyboardFocus(true);
    addAndMakeVisible(inlineEditor);
    inlineEditor.setVisible(false);
    inlineEditor.setColour(juce::TextEditor::backgroundColourId, juce::Colour(0xfffafafa));
    inlineEditor.setColour(juce::TextEditor::textColourId, juce::Colours::black);
    inlineEditor.setColour(juce::TextEditor::outlineColourId, juce::Colours::black);
    inlineEditor.setFont(juce::FontOptions("Monaco", 10.0f, juce::Font::plain));
    inlineEditor.onReturnKey = [this] { commitInlineEdit(); };
    inlineEditor.onEscapeKey = [this] { cancelInlineEdit(); };
    inlineEditor.onFocusLost = [this] { commitInlineEdit(); };
}

GraphCanvas::~GraphCanvas() = default;

void GraphCanvas::setGraph(const ir::Graph& g, const std::unordered_map<std::string, juce::Point<float>>& layoutById)
{
    graph = g;
    rebuildVisuals(layoutById);

    for (auto it = selectedNodeIds.begin(); it != selectedNodeIds.end();)
    {
        if (graph.findNode(*it) == nullptr)
            it = selectedNodeIds.erase(it);
        else
            ++it;
    }
    selectedEdges.erase(std::remove_if(selectedEdges.begin(), selectedEdges.end(), [&](const auto& e)
                        { return graph.findNode(e.fromNodeId) == nullptr || graph.findNode(e.toNodeId) == nullptr; }),
                        selectedEdges.end());
    if (primarySelectedNodeId.has_value() && graph.findNode(*primarySelectedNodeId) == nullptr)
        primarySelectedNodeId.reset();
    repaint();
}

void GraphCanvas::rebuildVisuals(const std::unordered_map<std::string, juce::Point<float>>& layoutById)
{
    visuals.clear();
    const auto measureFont = pdFont(pdBaseFontSize);
    for (size_t i = 0; i < graph.nodes.size(); ++i)
    {
        NodeVisual v;
        v.node = graph.nodes[i];
        v.displayText = pdObjectTextForNode(v.node);
        v.style = v.node.op == "bang" ? NodeStyle::bang
                                       : v.node.op == "msg" ? NodeStyle::message
                                       : v.node.op == "floatatom" ? NodeStyle::floatatom
                                                                   : NodeStyle::object;

        const auto found = layoutById.find(v.node.id);
        juce::Point<float> pos;
        if (found != layoutById.end())
            pos = found->second;
        else
            pos = { 30.0f + static_cast<float>((i % 4) * 170), 90.0f + static_cast<float>((i / 4) * 70) };

        const auto textWidth = static_cast<float>(measureFont.getStringWidth(v.displayText)) + 8.0f;
        const auto bangSize = nodeHeight * 2.0f;
        const auto w = v.style == NodeStyle::bang ? bangSize : juce::jmax(minNodeWidth, textWidth);
        const auto h = v.style == NodeStyle::bang ? bangSize : nodeHeight;
        v.bounds = juce::Rectangle<float>(pos.x, pos.y, w, h);

        const auto spec = ir::opSpecFor(v.node.op);
        for (int port = 0; port < static_cast<int>(spec.inputs.size()); ++port)
        {
            const auto& input = spec.inputs[static_cast<size_t>(port)];
            v.inputRates.push_back(input.rate);
            v.inputHot.push_back(spec.hotInput >= 0 && spec.hotInput == port);
        }
        v.outputRate = spec.outputRate;
        visuals.push_back(v);
    }
}

void GraphCanvas::selectNodeById(const std::string& nodeId)
{
    selectedNodeIds.clear();
    if (!nodeId.empty() && graph.findNode(nodeId) != nullptr)
    {
        selectedNodeIds.insert(nodeId);
        primarySelectedNodeId = nodeId;
    }
    else
    {
        primarySelectedNodeId.reset();
    }
    repaint();
}

std::string GraphCanvas::selectedNodeId() const
{
    if (primarySelectedNodeId.has_value())
        return *primarySelectedNodeId;
    if (!selectedNodeIds.empty())
        return *selectedNodeIds.begin();
    return {};
}

std::vector<std::string> GraphCanvas::getSelectedNodeIds() const
{
    std::vector<std::string> ids;
    ids.reserve(selectedNodeIds.size());
    for (const auto& id : selectedNodeIds)
        ids.push_back(id);
    return ids;
}

std::unordered_map<std::string, juce::Point<float>> GraphCanvas::getLayoutById() const
{
    std::unordered_map<std::string, juce::Point<float>> out;
    out.reserve(visuals.size());
    for (const auto& v : visuals)
        out[v.node.id] = v.bounds.getPosition();
    return out;
}

void GraphCanvas::clearSelection()
{
    selectedNodeIds.clear();
    selectedEdges.clear();
    primarySelectedNodeId.reset();
    repaint();
}

void GraphCanvas::selectAllNodes()
{
    selectedNodeIds.clear();
    selectedEdges.clear();
    for (const auto& n : graph.nodes)
        selectedNodeIds.insert(n.id);
    if (!selectedNodeIds.empty())
        primarySelectedNodeId = *selectedNodeIds.begin();
    else
        primarySelectedNodeId.reset();
    repaint();
}

void GraphCanvas::deleteSelectedNodes()
{
    if (selectedNodeIds.empty())
        return;
    std::vector<std::string> ids;
    ids.reserve(selectedNodeIds.size());
    for (const auto& id : selectedNodeIds)
        ids.push_back(id);
    if (onDeleteNodesRequested != nullptr)
        onDeleteNodesRequested(ids);
    else if (onDeleteNodeRequested != nullptr)
        for (const auto& id : ids)
            onDeleteNodeRequested(id);
    clearSelection();
}

bool GraphCanvas::deleteSelection()
{
    bool didSomething = false;
    if (!selectedEdges.empty())
    {
        if (onDeleteEdgesRequested != nullptr)
            onDeleteEdgesRequested(selectedEdges);
        selectedEdges.clear();
        didSomething = true;
    }
    if (!selectedNodeIds.empty())
    {
        deleteSelectedNodes();
        didSomething = true;
    }
    if (didSomething)
        repaint();
    return didSomething;
}

bool GraphCanvas::hasAnySelection() const
{
    return !selectedNodeIds.empty() || !selectedEdges.empty();
}

std::optional<size_t> GraphCanvas::indexForNodeId(const std::string& nodeId) const
{
    for (size_t i = 0; i < visuals.size(); ++i)
        if (visuals[i].node.id == nodeId)
            return i;
    return std::nullopt;
}

void GraphCanvas::beginInlineEdit(const std::string& nodeId)
{
    const auto idx = indexForNodeId(nodeId);
    if (!idx.has_value())
        return;

    // Keep edited node selected for immediate follow-up actions (e.g. rewiring).
    selectedNodeIds.clear();
    selectedNodeIds.insert(nodeId);
    primarySelectedNodeId = nodeId;
    selectedEdges.clear();
    if (onNodeSelectionChanged != nullptr)
        onNodeSelectionChanged(nodeId);

    editingNodeId = nodeId;
    const auto& n = visuals[*idx];
    juce::String text;
    if (n.node.op == "msg" || n.node.op == "obj")
        text = juce::String(n.node.label);
    else if (n.node.op == "floatatom")
        text = juce::String(n.node.literal.value_or(0.0), 3);
    else
        text = n.displayText;

    auto r = nodeScreenBounds(n).reduced(1.0f, 1.0f).getSmallestIntegerContainer();
    r.setSize(juce::jmax(72, r.getWidth()), juce::jmax(20, r.getHeight()));
    inlineEditor.setBounds(r);
    inlineEditor.setFont(pdFont(juce::jlimit(11.0f, 20.0f, pdBaseFontSize * zoom)));
    inlineEditor.setText(text, juce::dontSendNotification);
    inlineEditor.setVisible(true);
    inlineEditor.grabKeyboardFocus();
    inlineEditor.selectAll();
    repaint();
}

void GraphCanvas::commitInlineEdit()
{
    if (!editingNodeId.has_value())
        return;
    const auto nodeId = *editingNodeId;
    const auto value = inlineEditor.getText().toStdString();
    inlineEditor.setVisible(false);
    editingNodeId.reset();
    if (onNodeTextEditRequested != nullptr)
        onNodeTextEditRequested(nodeId, value);
}

void GraphCanvas::cancelInlineEdit()
{
    inlineEditor.setVisible(false);
    editingNodeId.reset();
}

void GraphCanvas::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xffe5e5e5));
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();

    std::unordered_map<std::string, const NodeVisual*> byId;
    byId.reserve(visuals.size());
    for (const auto& v : visuals)
        byId[v.node.id] = &v;

    for (const auto& edge : graph.edges)
    {
        if (!byId.contains(edge.fromNodeId) || !byId.contains(edge.toNodeId))
            continue;
        const auto* fromIt = byId[edge.fromNodeId];
        const auto* toIt = byId[edge.toNodeId];

        const auto isSelected = std::find_if(selectedEdges.begin(), selectedEdges.end(), [&](const auto& e)
                                { return e.fromNodeId == edge.fromNodeId && e.toNodeId == edge.toNodeId && e.toPort == edge.toPort; }) != selectedEdges.end();
        g.setColour(isSelected ? juce::Colour(0xff3b82f6) : juce::Colours::black);

        juce::Path cord;
        cord.startNewSubPath(outputPortPosition(*fromIt));
        cord.lineTo(inputPortPosition(*toIt, edge.toPort));
        g.strokePath(cord, juce::PathStrokeType(isSelected ? 2.0f : 1.0f));
    }

    if (connectFromNode.has_value() && *connectFromNode < visuals.size())
    {
        juce::Path cord;
        cord.startNewSubPath(outputPortPosition(visuals[*connectFromNode]));
        cord.lineTo(liveMouse);
        g.strokePath(cord, juce::PathStrokeType(1.0f));
    }

    for (const auto& visual : visuals)
    {
        const auto rect = nodeScreenBounds(visual);
        const auto isSel = selectedNodeIds.contains(visual.node.id);

        if (visual.style == NodeStyle::bang)
        {
            const auto isBangLit = bangFlashUntilMs.contains(visual.node.id) && bangFlashUntilMs[visual.node.id] > nowMs;
            g.setColour(juce::Colours::white);
            g.fillRect(rect);
            g.setColour(juce::Colours::black);
            g.drawRect(rect, 1.0f);
            const auto circleRect = rect.reduced(rect.getWidth() * 0.22f, rect.getHeight() * 0.22f);
            g.setColour(isBangLit ? juce::Colours::black : juce::Colours::white);
            g.fillEllipse(circleRect);
            g.setColour(juce::Colours::black);
            g.drawEllipse(circleRect, 1.0f);
        }
        else if (visual.style == NodeStyle::message)
        {
            const auto isBang = visual.node.op == "bang";
            const auto isBangLit = isBang && bangFlashUntilMs.contains(visual.node.id) && bangFlashUntilMs[visual.node.id] > nowMs;
            juce::Path msg;
            msg.startNewSubPath(rect.getX(), rect.getY());
            msg.lineTo(rect.getRight() - 7.0f, rect.getY());
            msg.lineTo(rect.getRight(), rect.getY() + 5.0f);
            msg.lineTo(rect.getRight(), rect.getBottom());
            msg.lineTo(rect.getX(), rect.getBottom());
            msg.closeSubPath();
            g.setColour(isBangLit ? juce::Colour(0xffd9d9d9) : juce::Colours::white);
            g.fillPath(msg);
            g.setColour(juce::Colours::black);
            g.strokePath(msg, juce::PathStrokeType(1.0f));
        }
        else
        {
            g.setColour(juce::Colours::white);
            g.fillRect(rect);
            g.setColour(juce::Colours::black);
            g.drawRect(rect, 1);
        }

        if (visual.style == NodeStyle::floatatom)
        {
            juce::Path notch;
            notch.startNewSubPath(rect.getRight() - 8.0f, rect.getY() + 2.0f);
            notch.lineTo(rect.getRight() - 3.0f, rect.getCentreY());
            notch.lineTo(rect.getRight() - 8.0f, rect.getBottom() - 2.0f);
            g.strokePath(notch, juce::PathStrokeType(1.0f));
        }

        if (isSel)
        {
            g.setColour(juce::Colours::black);
            g.drawRect(rect.expanded(1.0f), 2);
        }

        if (visual.style != NodeStyle::bang)
        {
            g.setColour(juce::Colours::black);
            const auto scaledFont = pdFont(juce::jlimit(10.0f, 24.0f, pdBaseFontSize * zoom));
            g.setFont(scaledFont);
            const auto textPadX = juce::jmax(2, static_cast<int>(std::round(3.0f * zoom)));
            const auto textPadY = juce::jmax(0, static_cast<int>(std::round(1.0f * zoom)));
            g.drawFittedText(visual.displayText, rect.getSmallestIntegerContainer().reduced(textPadX, textPadY), juce::Justification::centredLeft, 1);
        }

        for (int port = 0; port < static_cast<int>(visual.inputRates.size()); ++port)
        {
            const auto p = inputPortPosition(visual, port);
            const auto pr = juce::jmax(2.0f, portHalf * zoom);
            const auto inletRect = juce::Rectangle<float>(p.x - pr, p.y - pr * 0.35f, pr * 2.0f, pr * 0.9f);
            const auto isHot = port < static_cast<int>(visual.inputHot.size()) ? visual.inputHot[static_cast<size_t>(port)] : false;
            const auto rate = port < static_cast<int>(visual.inputRates.size()) ? visual.inputRates[static_cast<size_t>(port)] : ir::PortRate::audio;
            if (rate == ir::PortRate::audio)
            {
                g.setColour(juce::Colours::black);
                g.fillRect(inletRect);
            }
            else if (rate == ir::PortRate::control)
            {
                g.setColour(juce::Colours::white);
                g.fillRect(inletRect);
                g.setColour(juce::Colours::black);
                g.drawRect(inletRect, 1.0f);
            }
            else if (rate == ir::PortRate::event)
            {
                g.setColour(juce::Colour(0xffd0d0d0));
                g.fillRect(inletRect);
                g.setColour(juce::Colours::black);
                g.drawRect(inletRect, 1.0f);
                g.fillEllipse(inletRect.withSizeKeepingCentre(pr * 0.65f, pr * 0.65f));
            }
            else
            {
                // PortRate::any: Pd-like "either signal or value" inlet.
                const auto leftHalf = inletRect.withWidth(inletRect.getWidth() * 0.5f);
                const auto rightHalf = juce::Rectangle<float>(leftHalf.getRight(), inletRect.getY(), inletRect.getWidth() - leftHalf.getWidth(), inletRect.getHeight());
                g.setColour(juce::Colours::black);
                g.fillRect(leftHalf);
                g.setColour(juce::Colours::white);
                g.fillRect(rightHalf);
                g.setColour(juce::Colours::black);
                g.drawRect(inletRect, 1.0f);
            }

            // Pd-like hot inlet cue for trigger inlets (e.g. left inlet on control math boxes).
            if (isHot && rate != ir::PortRate::audio)
            {
                juce::Path hot;
                hot.startNewSubPath(p.x, p.y - pr * 1.25f);
                hot.lineTo(p.x - pr * 0.7f, p.y - pr * 0.25f);
                hot.lineTo(p.x + pr * 0.7f, p.y - pr * 0.25f);
                hot.closeSubPath();
                g.setColour(juce::Colours::black);
                g.fillPath(hot);
            }
        }
        const auto outP = outputPortPosition(visual);
        const auto pr = juce::jmax(2.0f, portHalf * zoom);
        const auto outletRect = juce::Rectangle<float>(outP.x - pr, outP.y - pr * 0.55f, pr * 2.0f, pr * 1.1f);
        if (visual.outputRate == ir::PortRate::audio)
        {
            g.setColour(juce::Colours::black);
            g.fillRect(outletRect);
        }
        else if (visual.outputRate == ir::PortRate::control)
        {
            g.setColour(juce::Colours::white);
            g.fillRect(outletRect);
            g.setColour(juce::Colours::black);
            g.drawRect(outletRect, 1.0f);
        }
        else if (visual.outputRate == ir::PortRate::event)
        {
            g.setColour(juce::Colour(0xffd0d0d0));
            g.fillRect(outletRect);
            g.setColour(juce::Colours::black);
            g.drawRect(outletRect, 1.0f);
            g.fillEllipse(outletRect.withSizeKeepingCentre(pr * 0.65f, pr * 0.65f));
        }
        else
        {
            const auto leftHalf = outletRect.withWidth(outletRect.getWidth() * 0.5f);
            const auto rightHalf = juce::Rectangle<float>(leftHalf.getRight(), outletRect.getY(), outletRect.getWidth() - leftHalf.getWidth(), outletRect.getHeight());
            g.setColour(juce::Colours::black);
            g.fillRect(leftHalf);
            g.setColour(juce::Colours::white);
            g.fillRect(rightHalf);
            g.setColour(juce::Colours::black);
            g.drawRect(outletRect, 1.0f);
        }
    }

    if (marqueeSelecting)
    {
        juce::Rectangle<float> r(marqueeStart, marqueeCurrent);
        r = r.getSmallestIntegerContainer().toFloat();
        g.setColour(juce::Colour(0x30222222));
        g.fillRect(r);
        g.setColour(juce::Colours::black);
        g.drawRect(r, 1.0f);
    }
}

void GraphCanvas::resized() {}

bool GraphCanvas::keyPressed(const juce::KeyPress& key)
{
    if (key == juce::KeyPress('c', juce::ModifierKeys::commandModifier, 0))
    {
        if (onCopyRequested != nullptr)
            onCopyRequested();
        return true;
    }
    if (key == juce::KeyPress('x', juce::ModifierKeys::commandModifier, 0))
    {
        if (onCutRequested != nullptr)
            onCutRequested();
        return true;
    }
    if (key == juce::KeyPress('v', juce::ModifierKeys::commandModifier, 0))
    {
        if (onPasteRequested != nullptr)
            onPasteRequested();
        return true;
    }
    if (key == juce::KeyPress('a', juce::ModifierKeys::commandModifier, 0))
    {
        if (onSelectAllRequested != nullptr)
            onSelectAllRequested();
        return true;
    }

    if (key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey)
    {
        if (deleteSelection())
            return true;
    }

    const auto mods = key.getModifiers();
    const auto ch = key.getTextCharacter();
    const auto printable = ch >= 32 && ch < 127;
    if (printable && !mods.isCommandDown() && !mods.isCtrlDown() && !mods.isAltDown())
    {
        if (!primarySelectedNodeId.has_value())
            return false;
        const auto idx = indexForNodeId(*primarySelectedNodeId);
        if (!idx.has_value())
            return false;

        const auto& n = visuals[*idx].node;
        if (n.op == "constant")
            return false;

        beginInlineEdit(n.id);
        if (inlineEditor.isVisible())
        {
            const auto head = canonicalPdHeadForOp(n.op);
            if (head.isNotEmpty() && n.op != "obj" && n.op != "msg" && n.op != "floatatom")
                inlineEditor.setText(head + " ", juce::dontSendNotification);
            inlineEditor.setCaretPosition(inlineEditor.getTotalNumChars());
            inlineEditor.insertTextAtCaret(juce::String::charToString(ch));
            return true;
        }
    }
    return false;
}

std::optional<size_t> GraphCanvas::hitNode(const juce::Point<float> p) const
{
    for (size_t i = visuals.size(); i > 0; --i)
        if (nodeScreenBounds(visuals[i - 1]).contains(p))
            return i - 1;
    return std::nullopt;
}

std::optional<size_t> GraphCanvas::hitEdge(const juce::Point<float> p) const
{
    auto distanceToSegment = [](const juce::Point<float> pt, const juce::Point<float> a, const juce::Point<float> b)
    {
        const auto ab = b - a;
        const auto ap = pt - a;
        const auto len2 = ab.x * ab.x + ab.y * ab.y;
        if (len2 <= 1.0e-6f)
            return pt.getDistanceFrom(a);
        const auto t = juce::jlimit(0.0f, 1.0f, (ap.x * ab.x + ap.y * ab.y) / len2);
        const juce::Point<float> proj { a.x + ab.x * t, a.y + ab.y * t };
        return pt.getDistanceFrom(proj);
    };

    for (size_t i = graph.edges.size(); i > 0; --i)
    {
        const auto& e = graph.edges[i - 1];
        const auto from = std::find_if(visuals.begin(), visuals.end(), [&](const auto& n) { return n.node.id == e.fromNodeId; });
        const auto to = std::find_if(visuals.begin(), visuals.end(), [&](const auto& n) { return n.node.id == e.toNodeId; });
        if (from == visuals.end() || to == visuals.end())
            continue;
        const auto a = outputPortPosition(*from);
        const auto b = inputPortPosition(*to, e.toPort);
        if (distanceToSegment(p, a, b) <= cableHitRadiusPx)
            return i - 1;
    }
    return std::nullopt;
}

std::optional<int> GraphCanvas::hitInputPort(const NodeVisual& visual, const juce::Point<float> p) const
{
    for (int port = 0; port < static_cast<int>(visual.inputRates.size()); ++port)
        if (inputPortPosition(visual, port).getDistanceFrom(p) <= juce::jmax(minHit, portHitRadiusPx))
            return port;
    return std::nullopt;
}

bool GraphCanvas::hitOutputPort(const NodeVisual& visual, const juce::Point<float> p) const
{
    return outputPortPosition(visual).getDistanceFrom(p) <= juce::jmax(minHit, outputHitRadiusPx);
}

juce::Point<float> GraphCanvas::inputPortPosition(const NodeVisual& visual, const int port) const
{
    const auto count = juce::jmax(1, static_cast<int>(visual.inputRates.size()));
    const auto rect = nodeScreenBounds(visual);
    const auto stride = rect.getWidth() / static_cast<float>(count + 1);
    return { rect.getX() + stride * static_cast<float>(port + 1), rect.getY() };
}

juce::Point<float> GraphCanvas::outputPortPosition(const NodeVisual& visual) const
{
    const auto rect = nodeScreenBounds(visual);
    return { rect.getCentreX(), rect.getBottom() };
}

juce::Rectangle<float> GraphCanvas::nodeScreenBounds(const NodeVisual& visual) const
{
    const auto p = worldToScreen(visual.bounds.getPosition());
    return juce::Rectangle<float>(p.x, p.y, visual.bounds.getWidth() * zoom, visual.bounds.getHeight() * zoom);
}

juce::Rectangle<float> GraphCanvas::canvasBounds() const
{
    return getLocalBounds().toFloat().reduced(canvasInset);
}

juce::Point<float> GraphCanvas::worldToScreen(const juce::Point<float> world) const
{
    const auto c = canvasBounds();
    return { c.getX() + panOffset.x + world.x * zoom, c.getY() + panOffset.y + world.y * zoom };
}

juce::Point<float> GraphCanvas::screenToWorld(const juce::Point<float> screen) const
{
    const auto c = canvasBounds();
    return { (screen.x - c.getX() - panOffset.x) / zoom, (screen.y - c.getY() - panOffset.y) / zoom };
}

void GraphCanvas::showPutPopup(const juce::Point<float> screenPos)
{
    if (onAddNodeRequested == nullptr)
        return;

    juce::PopupMenu put;
    juce::PopupMenu create;
    create.addItem(1, "object (obj)");
    create.addItem(2, "message (msg)");
    create.addItem(3, "number (floatatom)");
    create.addItem(4, "bang");

    juce::PopupMenu generators;
    generators.addItem(10, "osc~");
    generators.addItem(11, "phasor~");
    generators.addItem(12, "tri~");
    generators.addItem(13, "noise~");

    juce::PopupMenu filtersShapers;
    filtersShapers.addItem(14, "lop~");
    filtersShapers.addItem(15, "hip~");
    filtersShapers.addItem(16, "bpf~");
    filtersShapers.addItem(17, "svf~");
    filtersShapers.addItem(40, "lores~");
    filtersShapers.addSeparator();
    filtersShapers.addItem(21, "clip~");
    filtersShapers.addItem(22, "tanh~");
    filtersShapers.addItem(23, "slew~");

    juce::PopupMenu delayTime;
    delayTime.addItem(18, "delay~");
    delayTime.addItem(41, "delay");
    delayTime.addItem(19, "apf~");
    delayTime.addItem(20, "comb~");
    delayTime.addItem(24, "sah~");

    juce::PopupMenu math;
    math.addItem(30, "+~");
    math.addItem(31, "-~");
    math.addItem(32, "*~");
    math.addItem(33, "/~");
    math.addItem(27, "pow~");
    math.addItem(28, "mod~");
    math.addSeparator();
    math.addItem(34, "+");
    math.addItem(35, "-");
    math.addItem(36, "*");
    math.addItem(37, "/");
    math.addSeparator();
    math.addItem(50, "min~");
    math.addItem(51, "max~");
    math.addItem(52, "min");
    math.addItem(53, "max");
    math.addItem(54, "abs~");
    math.addItem(55, "abs");

    juce::PopupMenu logic;
    logic.addItem(38, "comp~");
    logic.addItem(42, "and~");
    logic.addItem(43, "or~");
    logic.addItem(44, "xor~");
    logic.addItem(45, "not~");
    logic.addSeparator();
    logic.addItem(39, "comp");
    logic.addItem(46, "and");
    logic.addItem(47, "or");
    logic.addItem(48, "xor");
    logic.addItem(49, "not");
    logic.addSeparator();
    logic.addItem(56, "random");

    juce::PopupMenu routingConv;
    routingConv.addItem(25, "mtof");
    routingConv.addItem(26, "mtof~");
    routingConv.addItem(29, "dac~");

    put.addSubMenu("Create", create);
    put.addSubMenu("Generators", generators);
    put.addSubMenu("Filters/Shapers", filtersShapers);
    put.addSubMenu("Delay/Time", delayTime);
    put.addSubMenu("Math", math);
    put.addSubMenu("Logic", logic);
    put.addSubMenu("Routing/Convert", routingConv);

    const auto p = screenToWorld(screenPos);
    juce::Component::SafePointer<GraphCanvas> safeThis(this);
    put.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(this).withTargetScreenArea(
                          juce::Rectangle<int>(static_cast<int>(screenPos.x), static_cast<int>(screenPos.y), 1, 1)),
                      [safeThis, p](const int res)
                      {
                          if (safeThis == nullptr || safeThis->onAddNodeRequested == nullptr || res == 0)
                              return;
                          if (res == 1)
                              safeThis->onAddNodeRequested("obj", p);
                          else if (res == 2)
                              safeThis->onAddNodeRequested("msg", p);
                          else if (res == 3)
                              safeThis->onAddNodeRequested("floatatom", p);
                          else if (res == 4)
                              safeThis->onAddNodeRequested("bang", p);
                          else if (res == 10)
                              safeThis->onAddNodeRequested("sin", p);
                          else if (res == 11)
                              safeThis->onAddNodeRequested("saw", p);
                          else if (res == 12)
                              safeThis->onAddNodeRequested("tri", p);
                          else if (res == 13)
                              safeThis->onAddNodeRequested("noise", p);
                          else if (res == 14)
                              safeThis->onAddNodeRequested("lpf", p);
                          else if (res == 15)
                              safeThis->onAddNodeRequested("hpf", p);
                          else if (res == 16)
                              safeThis->onAddNodeRequested("bpf", p);
                          else if (res == 17)
                              safeThis->onAddNodeRequested("svf", p);
                          else if (res == 18)
                              safeThis->onAddNodeRequested("delay", p);
                          else if (res == 19)
                              safeThis->onAddNodeRequested("apf", p);
                          else if (res == 20)
                              safeThis->onAddNodeRequested("comb", p);
                          else if (res == 21)
                              safeThis->onAddNodeRequested("clip", p);
                          else if (res == 22)
                              safeThis->onAddNodeRequested("tanh", p);
                          else if (res == 23)
                              safeThis->onAddNodeRequested("slew", p);
                          else if (res == 24)
                              safeThis->onAddNodeRequested("sah", p);
                          else if (res == 25)
                              safeThis->onAddNodeRequested("mtof", p);
                          else if (res == 26)
                              safeThis->onAddNodeRequested("mtof_sig", p);
                          else if (res == 27)
                              safeThis->onAddNodeRequested("pow", p);
                          else if (res == 28)
                              safeThis->onAddNodeRequested("mod", p);
                          else if (res == 29)
                              safeThis->onAddNodeRequested("out", p);
                          else if (res == 30)
                              safeThis->onAddNodeRequested("add", p);
                          else if (res == 31)
                              safeThis->onAddNodeRequested("sub", p);
                          else if (res == 32)
                              safeThis->onAddNodeRequested("mul", p);
                          else if (res == 33)
                              safeThis->onAddNodeRequested("div", p);
                          else if (res == 34)
                              safeThis->onAddNodeRequested("cadd", p);
                          else if (res == 35)
                              safeThis->onAddNodeRequested("csub", p);
                          else if (res == 36)
                              safeThis->onAddNodeRequested("cmul", p);
                          else if (res == 37)
                              safeThis->onAddNodeRequested("cdiv", p);
                          else if (res == 38)
                              safeThis->onAddNodeRequested("comp_sig", p);
                          else if (res == 39)
                              safeThis->onAddNodeRequested("comp", p);
                          else if (res == 40)
                              safeThis->onAddNodeRequested("lores", p);
                          else if (res == 41)
                              safeThis->onAddNodeRequested("cdelay", p);
                          else if (res == 42)
                              safeThis->onAddNodeRequested("and_sig", p);
                          else if (res == 43)
                              safeThis->onAddNodeRequested("or_sig", p);
                          else if (res == 44)
                              safeThis->onAddNodeRequested("xor_sig", p);
                          else if (res == 45)
                              safeThis->onAddNodeRequested("not_sig", p);
                          else if (res == 46)
                              safeThis->onAddNodeRequested("and", p);
                          else if (res == 47)
                              safeThis->onAddNodeRequested("or", p);
                          else if (res == 48)
                              safeThis->onAddNodeRequested("xor", p);
                          else if (res == 49)
                              safeThis->onAddNodeRequested("not", p);
                          else if (res == 50)
                              safeThis->onAddNodeRequested("min_sig", p);
                          else if (res == 51)
                              safeThis->onAddNodeRequested("max_sig", p);
                          else if (res == 52)
                              safeThis->onAddNodeRequested("min", p);
                          else if (res == 53)
                              safeThis->onAddNodeRequested("max", p);
                          else if (res == 54)
                              safeThis->onAddNodeRequested("abs_sig", p);
                          else if (res == 55)
                              safeThis->onAddNodeRequested("abs", p);
                          else if (res == 56)
                              safeThis->onAddNodeRequested("random", p);
                      });
}

void GraphCanvas::mouseDown(const juce::MouseEvent& event)
{
    if (inlineEditor.isVisible())
    {
        const auto insideEditor = inlineEditor.getBounds().contains(event.getPosition());
        if (!insideEditor)
            commitInlineEdit();
    }

    grabKeyboardFocus();
    liveMouse = event.position;

    if (event.mods.isRightButtonDown())
    {
        showPutPopup(event.position);
        return;
    }

    if (event.mods.isMiddleButtonDown() || event.mods.isAltDown())
    {
        pendingDragNode.reset();
        pendingMarqueeStart = false;
        panning = true;
        panDragStartMouse = event.position;
        panDragStartOffset = panOffset;
        return;
    }

    const auto hit = hitNode(event.position);
    const auto edgeHit = hitEdge(event.position);
    if (!hit.has_value() && edgeHit.has_value())
    {
        const auto& e = graph.edges[*edgeHit];
        const auto isSelected = std::find_if(selectedEdges.begin(), selectedEdges.end(), [&](const auto& x)
                                { return x.fromNodeId == e.fromNodeId && x.toNodeId == e.toNodeId && x.toPort == e.toPort; }) != selectedEdges.end();
        if (!event.mods.isShiftDown())
        {
            selectedNodeIds.clear();
            primarySelectedNodeId.reset();
            selectedEdges.clear();
        }
        if (isSelected && event.mods.isShiftDown())
            selectedEdges.erase(std::remove_if(selectedEdges.begin(), selectedEdges.end(), [&](const auto& x)
                               { return x.fromNodeId == e.fromNodeId && x.toNodeId == e.toNodeId && x.toPort == e.toPort; }),
                                selectedEdges.end());
        else
            selectedEdges.push_back(e);
        repaint();
        return;
    }
    if (hit.has_value())
    {
        auto& visual = visuals[*hit];
        const auto inPortHit = hitInputPort(visual, event.position).has_value();
        const auto outPortHit = hitOutputPort(visual, event.position);
        const auto strictOutHitForFloat = outputPortPosition(visual).getDistanceFrom(event.position) <= juce::jmax(2.0f, portHalf * zoom * 1.25f);

        if (outPortHit)
        {
            pendingDragNode.reset();
            pendingMarqueeStart = false;
            pendingFloatatomEdit = false;
            pendingFloatatomNodeId.clear();
            connectFromNode = hit;
            return;
        }

        const auto id = visual.node.id;
        if (event.mods.isShiftDown())
        {
            if (selectedNodeIds.contains(id))
                selectedNodeIds.erase(id);
            else
                selectedNodeIds.insert(id);
        }
        else
        {
            if (!selectedNodeIds.contains(id))
            {
                selectedNodeIds.clear();
                selectedNodeIds.insert(id);
            }
            selectedEdges.clear();
        }
        primarySelectedNodeId = id;
        if (onNodeSelectionChanged != nullptr)
            onNodeSelectionChanged(id);

        pendingDragNode = hit;
        draggingNode.reset();
        dragStartWorld = screenToWorld(event.position);
        dragStartPositions.clear();
        for (auto& v : visuals)
            if (selectedNodeIds.contains(v.node.id))
                dragStartPositions[v.node.id] = v.bounds.getPosition();

        pendingFloatatomEdit = (visual.style == NodeStyle::floatatom && !inPortHit && !strictOutHitForFloat);
        pendingFloatatomNodeId = visual.node.id;
        pendingFloatatomMouseDown = event.position;
        if (visual.node.op == "bang" && !inPortHit && !outPortHit)
        {
            pendingBangNodeId = visual.node.id;
            pendingBangMouseDown = event.position;
        }
        else
        {
            pendingBangNodeId.clear();
        }
        repaint();
        return;
    }

    pendingDragNode.reset();
    pendingFloatatomEdit = false;
    pendingFloatatomNodeId.clear();
    pendingBangNodeId.clear();
    pendingMarqueeStart = true;
    pendingMarqueeClearsSelection = !event.mods.isShiftDown();
    marqueeSelecting = false;
    marqueeStart = event.position;
    marqueeCurrent = event.position;
    marqueeBaseSelection = selectedNodeIds;
}

void GraphCanvas::mouseDrag(const juce::MouseEvent& event)
{
    liveMouse = event.position;

    if (panning)
    {
        panOffset = panDragStartOffset + (event.position - panDragStartMouse);
        repaint();
        return;
    }

    if (pendingDragNode.has_value() && !draggingNode.has_value())
    {
        if (event.getDistanceFromDragStart() > 2)
        {
            draggingNode = pendingDragNode;
            pendingDragNode.reset();
            if (pendingFloatatomEdit)
                pendingFloatatomEdit = false;
            pendingBangNodeId.clear();
        }
    }

    if (draggingNode.has_value())
    {
        if (pendingFloatatomEdit && event.getDistanceFromDragStart() > 2)
            pendingFloatatomEdit = false;
        const auto delta = screenToWorld(event.position) - dragStartWorld;
        for (auto& v : visuals)
        {
            if (!selectedNodeIds.contains(v.node.id))
                continue;
            if (!dragStartPositions.contains(v.node.id))
                continue;
            const auto next = dragStartPositions[v.node.id] + delta;
            v.bounds.setPosition(next);
            if (onNodeMoved != nullptr)
                onNodeMoved(v.node.id, next);
        }
        repaint();
        return;
    }

    if (connectFromNode.has_value())
    {
        repaint();
        return;
    }

    if (pendingMarqueeStart && !marqueeSelecting && event.getDistanceFromDragStart() > 2)
    {
        marqueeSelecting = true;
        if (pendingMarqueeClearsSelection)
        {
            selectedNodeIds.clear();
            selectedEdges.clear();
        }
    }

    if (marqueeSelecting)
    {
        marqueeCurrent = event.position;
        juce::Rectangle<float> r(marqueeStart, marqueeCurrent);
        const auto selectRect = r.getSmallestIntegerContainer().toFloat();
        selectedNodeIds = marqueeBaseSelection;
        for (const auto& v : visuals)
            if (selectRect.intersects(nodeScreenBounds(v)))
                selectedNodeIds.insert(v.node.id);
        if (!selectedNodeIds.empty())
            primarySelectedNodeId = *selectedNodeIds.begin();
        repaint();
    }
}

void GraphCanvas::mouseUp(const juce::MouseEvent& event)
{
    liveMouse = event.position;
    panning = false;
    draggingNode.reset();
    pendingDragNode.reset();

    if (connectFromNode.has_value() && *connectFromNode < visuals.size())
    {
        const auto source = visuals[*connectFromNode].node.id;
        const auto targetHit = hitNode(event.position);
        if (targetHit.has_value() && *targetHit < visuals.size() && *targetHit != *connectFromNode)
        {
            auto port = hitInputPort(visuals[*targetHit], event.position);
            if (!port.has_value())
            {
                // Forgiving drop: if not exactly on an inlet, pick nearest inlet within a generous radius.
                float bestDist = 1.0e9f;
                int bestPort = -1;
                const auto& targetVisual = visuals[*targetHit];
                for (int p = 0; p < static_cast<int>(targetVisual.inputRates.size()); ++p)
                {
                    const auto d = inputPortPosition(targetVisual, p).getDistanceFrom(event.position);
                    if (d < bestDist)
                    {
                        bestDist = d;
                        bestPort = p;
                    }
                }
                if (bestPort >= 0 && bestDist <= 16.0f)
                    port = bestPort;
            }
            if (port.has_value() && onConnectRequested != nullptr)
                onConnectRequested(source, visuals[*targetHit].node.id, *port);
        }
    }
    connectFromNode.reset();
    if (pendingFloatatomEdit && !pendingFloatatomNodeId.empty() && event.position.getDistanceFrom(pendingFloatatomMouseDown) <= 2.0f)
        beginInlineEdit(pendingFloatatomNodeId);
    if (!pendingBangNodeId.empty() && event.position.getDistanceFrom(pendingBangMouseDown) <= 2.0f && onBangTriggered != nullptr)
    {
        bangFlashUntilMs[pendingBangNodeId] = juce::Time::getMillisecondCounterHiRes() + 120.0;
        startTimerHz(60);
        onBangTriggered(pendingBangNodeId);
    }
    pendingFloatatomEdit = false;
    pendingFloatatomNodeId.clear();
    pendingBangNodeId.clear();
    if (pendingMarqueeStart && !marqueeSelecting && pendingMarqueeClearsSelection)
    {
        selectedNodeIds.clear();
        selectedEdges.clear();
        primarySelectedNodeId.reset();
    }
    pendingMarqueeStart = false;
    marqueeSelecting = false;
    repaint();
}

void GraphCanvas::mouseMove(const juce::MouseEvent& event)
{
    liveMouse = event.position;
}

void GraphCanvas::mouseDoubleClick(const juce::MouseEvent& event)
{
    const auto hit = hitNode(event.position);
    if (!hit.has_value())
        return;
    const auto& n = visuals[*hit].node;
    if (n.op != "constant")
        beginInlineEdit(n.id);
}

void GraphCanvas::flashBang(const std::string& nodeId, const double durationMs)
{
    if (nodeId.empty())
        return;
    bangFlashUntilMs[nodeId] = juce::Time::getMillisecondCounterHiRes() + juce::jmax(1.0, durationMs);
    startTimerHz(60);
    repaint();
}

void GraphCanvas::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto oldZoom = zoom;
    const auto worldAtCursor = screenToWorld(event.position);
    const auto dy = juce::jlimit(-1.25f, 1.25f, wheel.deltaY);
    const auto sensitivity = wheel.isSmooth ? 0.42f : 0.30f;
    const auto zoomFactor = std::exp(dy * sensitivity);
    zoom = juce::jlimit(0.3f, 3.0f, oldZoom * zoomFactor);
    if (std::abs(zoom - oldZoom) < 1.0e-6f)
        return;

    // Keep world point under cursor fixed while zooming.
    const auto c = canvasBounds();
    panOffset.x = event.position.x - c.getX() - worldAtCursor.x * zoom;
    panOffset.y = event.position.y - c.getY() - worldAtCursor.y * zoom;
    repaint();
}

void GraphCanvas::timerCallback()
{
    const auto nowMs = juce::Time::getMillisecondCounterHiRes();
    bool hasActive = false;
    for (auto it = bangFlashUntilMs.begin(); it != bangFlashUntilMs.end();)
    {
        if (it->second <= nowMs)
            it = bangFlashUntilMs.erase(it);
        else
        {
            hasActive = true;
            ++it;
        }
    }

    repaint();
    if (!hasActive)
        stopTimer();
}
} // namespace duodsp::visual
