#include "GraphCanvas.h"

#include <algorithm>
#include <cmath>

namespace duodsp::visual
{
namespace
{
constexpr float minNodeWidth = 54.0f;
constexpr float nodeHeight = 19.0f;
constexpr float portHalf = 2.5f;
constexpr float minHit = 5.0f;
constexpr float canvasInset = 2.0f;

juce::String pdObjectTextForNode(const ir::Node& node)
{
    if (node.op == "sin")
        return "osc~ 220";
    if (node.op == "saw")
        return "phasor~ 110";
    if (node.op == "square")
        return "pulse~ 110 0.5";
    if (node.op == "noise")
        return "noise~ 0.05";
    if (node.op == "lpf")
        return "lop~ 1200";
    if (node.op == "hpf")
        return "hip~ 1200";
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
    if (node.op == "delay1")
        return "z^-1~";
    if (node.op == "scope")
        return "scope~";
    if (node.op == "spectrum")
        return "spectrum~";
    if (node.op == "msg")
        return node.label.empty() ? "message" : juce::String(node.label);
    if (node.op == "floatatom")
        return node.literal.has_value() ? juce::String(*node.literal, 3) : "0";
    if (node.op == "constant")
        return juce::String(node.literal.value_or(0.0), 3);
    if (node.op == "obj")
        return node.label.empty() ? "obj" : juce::String(node.label);
    return juce::String(node.op);
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
    if (primarySelectedNodeId.has_value() && graph.findNode(*primarySelectedNodeId) == nullptr)
        primarySelectedNodeId.reset();
    repaint();
}

void GraphCanvas::rebuildVisuals(const std::unordered_map<std::string, juce::Point<float>>& layoutById)
{
    visuals.clear();
    for (size_t i = 0; i < graph.nodes.size(); ++i)
    {
        NodeVisual v;
        v.node = graph.nodes[i];
        v.displayText = pdObjectTextForNode(v.node);
        v.style = v.node.op == "msg" ? NodeStyle::message : v.node.op == "floatatom" ? NodeStyle::floatatom : NodeStyle::object;

        const auto found = layoutById.find(v.node.id);
        juce::Point<float> pos;
        if (found != layoutById.end())
            pos = found->second;
        else
            pos = { 30.0f + static_cast<float>((i % 4) * 170), 90.0f + static_cast<float>((i / 4) * 70) };

        const auto textWidth = 6.15f * static_cast<float>(v.displayText.length()) + 14.0f;
        const auto w = juce::jmax(minNodeWidth, textWidth);
        v.bounds = juce::Rectangle<float>(pos.x, pos.y, w, nodeHeight);

        const auto spec = ir::opSpecFor(v.node.op);
        for (const auto& input : spec.inputs)
            v.inputRates.push_back(input.rate);
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

void GraphCanvas::clearSelection()
{
    selectedNodeIds.clear();
    primarySelectedNodeId.reset();
    repaint();
}

void GraphCanvas::selectAllNodes()
{
    selectedNodeIds.clear();
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
    if (onDeleteNodeRequested != nullptr)
        for (const auto& id : selectedNodeIds)
            onDeleteNodeRequested(id);
    clearSelection();
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

    editingNodeId = nodeId;
    const auto& n = visuals[*idx];
    juce::String text;
    if (n.node.op == "msg" || n.node.op == "obj")
        text = juce::String(n.node.label);
    else if (n.node.op == "floatatom")
        text = juce::String(n.node.literal.value_or(0.0), 3);
    else
        text = n.displayText;

    const auto r = nodeScreenBounds(n).reduced(1.0f, 1.0f).getSmallestIntegerContainer();
    inlineEditor.setBounds(r);
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
    g.fillAll(juce::Colour(0xffefefef));
    const auto canvas = canvasBounds();
    g.setColour(juce::Colour(0xfff6f6f6));
    g.fillRect(canvas);

    g.setColour(juce::Colours::black);
    for (const auto& edge : graph.edges)
    {
        const auto fromIt = std::find_if(visuals.begin(), visuals.end(), [&edge](const auto& n) { return n.node.id == edge.fromNodeId; });
        const auto toIt = std::find_if(visuals.begin(), visuals.end(), [&edge](const auto& n) { return n.node.id == edge.toNodeId; });
        if (fromIt == visuals.end() || toIt == visuals.end())
            continue;

        juce::Path cord;
        cord.startNewSubPath(outputPortPosition(*fromIt));
        cord.lineTo(inputPortPosition(*toIt, edge.toPort));
        g.strokePath(cord, juce::PathStrokeType(1.0f));
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

        if (visual.style == NodeStyle::message)
        {
            juce::Path msg;
            msg.startNewSubPath(rect.getX(), rect.getY());
            msg.lineTo(rect.getRight() - 7.0f, rect.getY());
            msg.lineTo(rect.getRight(), rect.getY() + 5.0f);
            msg.lineTo(rect.getRight(), rect.getBottom());
            msg.lineTo(rect.getX(), rect.getBottom());
            msg.closeSubPath();
            g.setColour(juce::Colour(0xfffafafa));
            g.fillPath(msg);
            g.setColour(juce::Colours::black);
            g.strokePath(msg, juce::PathStrokeType(1.0f));
        }
        else
        {
            g.setColour(juce::Colour(0xfffafafa));
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
            g.setColour(juce::Colour(0xff3b82f6));
            g.drawRect(rect.expanded(1.0f), 1);
        }

        g.setColour(juce::Colours::black);
        g.setFont(juce::FontOptions("Monaco", 10.0f, juce::Font::plain));
        g.drawText(visual.displayText, rect.getSmallestIntegerContainer().reduced(4, 1), juce::Justification::centredLeft, false);

        for (int port = 0; port < static_cast<int>(visual.inputRates.size()); ++port)
        {
            const auto p = inputPortPosition(visual, port);
            const auto pr = juce::jmax(2.0f, portHalf * zoom);
            g.fillRect(juce::Rectangle<float>(p.x - pr, p.y - pr * 0.35f, pr * 2.0f, pr * 0.9f));
        }
        const auto outP = outputPortPosition(visual);
        const auto pr = juce::jmax(2.0f, portHalf * zoom);
        g.fillRect(juce::Rectangle<float>(outP.x - pr, outP.y - pr * 0.55f, pr * 2.0f, pr * 1.1f));
    }

    if (marqueeSelecting)
    {
        juce::Rectangle<float> r(marqueeStart, marqueeCurrent);
        r = r.getSmallestIntegerContainer().toFloat();
        g.setColour(juce::Colour(0x303b82f6));
        g.fillRect(r);
        g.setColour(juce::Colour(0xff3b82f6));
        g.drawRect(r, 1.0f);
    }
}

void GraphCanvas::resized() {}

bool GraphCanvas::keyPressed(const juce::KeyPress& key)
{
    if ((key == juce::KeyPress::deleteKey || key == juce::KeyPress::backspaceKey) && !selectedNodeIds.empty())
    {
        deleteSelectedNodes();
        return true;
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

std::optional<int> GraphCanvas::hitInputPort(const NodeVisual& visual, const juce::Point<float> p) const
{
    for (int port = 0; port < static_cast<int>(visual.inputRates.size()); ++port)
        if (inputPortPosition(visual, port).getDistanceFrom(p) <= juce::jmax(minHit, portHalf * zoom * 2.2f))
            return port;
    return std::nullopt;
}

bool GraphCanvas::hitOutputPort(const NodeVisual& visual, const juce::Point<float> p) const
{
    return outputPortPosition(visual).getDistanceFrom(p) <= juce::jmax(minHit, portHalf * zoom * 2.2f);
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
    put.addItem(1, "Put object (obj)");
    put.addItem(2, "Put message (msg)");
    put.addItem(3, "Put number (floatatom)");
    put.addSeparator();
    put.addItem(10, "osc~");
    put.addItem(11, "phasor~");
    put.addItem(12, "noise~");
    put.addItem(13, "lop~");
    put.addItem(14, "dac~");

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
                          else if (res == 10)
                              safeThis->onAddNodeRequested("sin", p);
                          else if (res == 11)
                              safeThis->onAddNodeRequested("saw", p);
                          else if (res == 12)
                              safeThis->onAddNodeRequested("noise", p);
                          else if (res == 13)
                              safeThis->onAddNodeRequested("lpf", p);
                          else if (res == 14)
                              safeThis->onAddNodeRequested("out", p);
                      });
}

void GraphCanvas::mouseDown(const juce::MouseEvent& event)
{
    if (inlineEditor.isVisible())
        commitInlineEdit();

    grabKeyboardFocus();
    liveMouse = event.position;

    if (event.mods.isRightButtonDown())
    {
        showPutPopup(event.position);
        return;
    }

    if (event.mods.isMiddleButtonDown() || event.mods.isAltDown())
    {
        panning = true;
        panDragStartMouse = event.position;
        panDragStartOffset = panOffset;
        return;
    }

    const auto hit = hitNode(event.position);
    if (hit.has_value())
    {
        auto& visual = visuals[*hit];
        if (hitOutputPort(visual, event.position))
        {
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
        }
        primarySelectedNodeId = id;
        if (onNodeSelectionChanged != nullptr)
            onNodeSelectionChanged(id);

        draggingNode = hit;
        dragStartWorld = screenToWorld(event.position);
        dragStartPositions.clear();
        for (auto& v : visuals)
            if (selectedNodeIds.contains(v.node.id))
                dragStartPositions[v.node.id] = v.bounds.getPosition();
        repaint();
        return;
    }

    marqueeSelecting = true;
    marqueeStart = event.position;
    marqueeCurrent = event.position;
    marqueeBaseSelection = selectedNodeIds;
    if (!event.mods.isShiftDown())
        selectedNodeIds.clear();
    repaint();
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

    if (draggingNode.has_value())
    {
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

    if (connectFromNode.has_value() && *connectFromNode < visuals.size())
    {
        const auto source = visuals[*connectFromNode].node.id;
        const auto targetHit = hitNode(event.position);
        if (targetHit.has_value() && *targetHit < visuals.size() && *targetHit != *connectFromNode)
        {
            const auto port = hitInputPort(visuals[*targetHit], event.position);
            if (port.has_value() && onConnectRequested != nullptr)
                onConnectRequested(source, visuals[*targetHit].node.id, *port);
        }
    }
    connectFromNode.reset();
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
    if (n.op == "obj" || n.op == "msg" || n.op == "floatatom")
        beginInlineEdit(n.id);
}

void GraphCanvas::mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel)
{
    const auto before = screenToWorld(event.position);
    const auto zoomFactor = wheel.deltaY > 0.0f ? 1.1f : 0.91f;
    zoom = juce::jlimit(0.3f, 3.0f, zoom * zoomFactor);
    const auto after = screenToWorld(event.position);
    panOffset.x += (after.x - before.x) * zoom;
    panOffset.y += (after.y - before.y) * zoom;
    repaint();
}
} // namespace duodsp::visual
