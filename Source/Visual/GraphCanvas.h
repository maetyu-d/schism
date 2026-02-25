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
class GraphCanvas final : public juce::Component,
                          private juce::Timer
{
public:
    GraphCanvas();
    ~GraphCanvas() override;

    void setGraph(const ir::Graph& graph, const std::unordered_map<std::string, juce::Point<float>>& layoutById);
    void selectNodeById(const std::string& nodeId);
    std::string selectedNodeId() const;
    std::vector<std::string> getSelectedNodeIds() const;
    std::unordered_map<std::string, juce::Point<float>> getLayoutById() const;
    void flashBang(const std::string& nodeId, double durationMs = 120.0);
    void clearSelection();
    void selectAllNodes();
    void deleteSelectedNodes();
    bool deleteSelection();
    bool hasAnySelection() const;
    void beginInlineEdit(const std::string& nodeId);
    void setFloatatomLiveValues(const std::unordered_map<std::string, float>& values);

    std::function<void(const std::string&)> onNodeSelectionChanged;
    std::function<void(const std::string&, juce::Point<float>)> onAddNodeRequested;
    std::function<void(const std::string&, const std::string&, int)> onConnectRequested;
    std::function<void(const std::string&)> onDeleteNodeRequested;
    std::function<void(const std::vector<std::string>&)> onDeleteNodesRequested;
    std::function<void(const std::vector<ir::Edge>&)> onDeleteEdgesRequested;
    std::function<void(const std::string&, juce::Point<float>)> onNodeMoved;
    std::function<void(const std::string&, const std::string&)> onNodeTextEditRequested;
    std::function<void(const std::string&)> onBangTriggered;
    std::function<void()> onCopyRequested;
    std::function<void()> onCutRequested;
    std::function<void()> onPasteRequested;
    std::function<void()> onSelectAllRequested;

    void paint(juce::Graphics& g) override;
    void resized() override;
    bool keyPressed(const juce::KeyPress& key) override;
    void mouseDown(const juce::MouseEvent& event) override;
    void mouseDrag(const juce::MouseEvent& event) override;
    void mouseUp(const juce::MouseEvent& event) override;
    void mouseMove(const juce::MouseEvent& event) override;
    void mouseDoubleClick(const juce::MouseEvent& event) override;
    void mouseWheelMove(const juce::MouseEvent& event, const juce::MouseWheelDetails& wheel) override;
    bool keyStateChanged(bool isKeyDown) override;

private:
    void timerCallback() override;

    enum class NodeStyle
    {
        object,
        message,
        floatatom,
        bang
    };

    struct NodeVisual
    {
        ir::Node node;
        juce::String displayText;
        NodeStyle style = NodeStyle::object;
        juce::Rectangle<float> bounds;
        std::vector<ir::PortRate> inputRates;
        std::vector<bool> inputHot;
        ir::PortRate outputRate = ir::PortRate::audio;
    };

    std::optional<size_t> hitNode(juce::Point<float> p) const;
    std::optional<size_t> hitEdge(juce::Point<float> p) const;
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
    std::vector<ir::Edge> selectedEdges;
    std::optional<std::string> primarySelectedNodeId;

    std::optional<size_t> draggingNode;
    std::optional<size_t> pendingDragNode;
    juce::Point<float> dragStartWorld;
    std::unordered_map<std::string, juce::Point<float>> dragStartPositions;

    bool marqueeSelecting = false;
    bool pendingMarqueeStart = false;
    bool pendingMarqueeClearsSelection = false;
    juce::Point<float> marqueeStart;
    juce::Point<float> marqueeCurrent;
    std::unordered_set<std::string> marqueeBaseSelection;

    bool panning = false;
    bool spacePanHeld = false;
    bool pendingContextMenu = false;
    juce::Point<float> panDragStartMouse;
    juce::Point<float> panDragStartOffset;
    juce::Point<float> contextMenuMouseDown;

    std::optional<size_t> connectFromNode;
    juce::Point<float> liveMouse;
    float zoom = 1.0f;
    juce::Point<float> panOffset { 24.0f, 24.0f };
    bool pendingFloatatomEdit = false;
    std::string pendingFloatatomNodeId;
    juce::Point<float> pendingFloatatomMouseDown;
    std::string pendingBangNodeId;
    juce::Point<float> pendingBangMouseDown;

    juce::TextEditor inlineEditor;
    std::optional<std::string> editingNodeId;
    std::unordered_map<std::string, double> bangFlashUntilMs;
    std::unordered_map<std::string, float> floatatomLiveValues;
};
} // namespace duodsp::visual
