#pragma once

#include "../IR/GraphIR.h"

#include <juce_gui_basics/juce_gui_basics.h>

#include <functional>
#include <optional>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

namespace duodsp::visual
{
class GraphCanvas final : public juce::Component
{
public:
    GraphCanvas();
    ~GraphCanvas() override;

    void setGraph(const ir::Graph& graph, const std::unordered_map<std::string, juce::Point<float>>& layoutById);
    void selectNodeById(const std::string& nodeId);
    std::string selectedNodeId() const;
    std::vector<std::string> getSelectedNodeIds() const;
    void clearSelection();
    void selectAllNodes();
    void deleteSelectedNodes();
    void beginInlineEdit(const std::string& nodeId);

    std::function<void(const std::string&)> onNodeSelectionChanged;
    std::function<void(const std::string&, juce::Point<float>)> onAddNodeRequested;
    std::function<void(const std::string&, const std::string&, int)> onConnectRequested;
    std::function<void(const std::string&)> onDeleteNodeRequested;
    std::function<void(const std::string&, juce::Point<float>)> onNodeMoved;
    std::function<void(const std::string&, const std::string&)> onNodeTextEditRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;

private:
    enum class NodeStyle
    {
        object,
        message,
        floatatom
    };

    struct NodeVisual
    {
        ir::Node node;
        juce::String displayText;
        NodeStyle style = NodeStyle::object;
        juce::Rectangle<float> bounds;
        std::vector<ir::PortRate> inputRates;
        ir::PortRate outputRate = ir::PortRate::audio;
    };

    std::optional<size_t> hitNode(juce::Point<float> p) const;
    std::optional<int> hitInputPort(const NodeVisual& visual, juce::Point<float> p) const;
    bool hitOutputPort(const NodeVisual& visual, juce::Point<float> p) const;
    juce::Point<float> inputPortPosition(const NodeVisual& visual, int port) const;
    juce::Point<float> outputPortPosition(const NodeVisual& visual) const;
    juce::Rectangle<float> nodeScreenBounds(const NodeVisual& visual) const;
    juce::Rectangle<float> canvasBounds() const;
    juce::Point<float> worldToScreen(juce::Point<float> world) const;
    juce::Point<float> screenToWorld(juce::Point<float> screen) const;
    void rebuildVisuals(const std::unordered_map<std::string, juce::Point<float>>& layoutById);
    void showPutPopup(juce::Point<float> screenPos);
    void commitInlineEdit();
    void cancelInlineEdit();
    std::optional<size_t> indexForNodeId(const std::string& nodeId) const;

    ir::Graph graph;
    std::vector<NodeVisual> visuals;
    std::unordered_set<std::string> selectedNodeIds;
    std::optional<std::string> primarySelectedNodeId;

    std::optional<size_t> draggingNode;
    juce::Point<float> dragStartWorld;
    std::unordered_map<std::string, juce::Point<float>> dragStartPositions;

    bool marqueeSelecting = false;
    juce::Point<float> marqueeStart;
    juce::Point<float> marqueeCurrent;
    std::unordered_set<std::string> marqueeBaseSelection;

    bool panning = false;
    juce::Point<float> panDragStartMouse;
    juce::Point<float> panDragStartOffset;

    std::optional<size_t> connectFromNode;
    juce::Point<float> liveMouse;
    float zoom = 1.0f;
    juce::Point<float> panOffset { 24.0f, 24.0f };

    juce::TextEditor inlineEditor;
    std::optional<std::string> editingNodeId;
};
} // namespace duodsp::visual
