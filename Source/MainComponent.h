#pragma once

#include "DSP/RuntimeEngine.h"
#include "IR/GraphIR.h"
#include "Patch/PatchFile.h"
#include "Sync/SyncMap.h"
#include "Text/GraphLanguage.h"
#include "UI/ProbeWidgets.h"
#include "Visual/GraphCanvas.h"

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_extra/juce_gui_extra.h>
#include <functional>
#include <optional>
#include <unordered_map>
#include <vector>

class MainComponent final : public juce::AudioAppComponent,
                            private juce::CodeDocument::Listener,
                            private juce::Timer,
                            private juce::KeyListener,
                            private juce::MenuBarModel
{
public:
    MainComponent();
    ~MainComponent() override;

    void prepareToPlay(int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock(const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

    void resized() override;
    void paint(juce::Graphics& g) override;

private:
    class SplitterBar final : public juce::Component
    {
    public:
        std::function<void()> onDragStart;
        std::function<void(int)> onDragged;
        void paint(juce::Graphics& g) override
        {
            g.fillAll(juce::Colour(0xff4b5563));
        }
        void mouseDown(const juce::MouseEvent& e) override
        {
            dragStartX = e.getPosition().x;
            if (onDragStart != nullptr)
                onDragStart();
        }
        void mouseDrag(const juce::MouseEvent& e) override
        {
            if (onDragged != nullptr)
                onDragged(e.getDistanceFromDragStartX());
        }
        void mouseEnter(const juce::MouseEvent&) override
        {
            setMouseCursor(juce::MouseCursor::LeftRightResizeCursor);
        }
        void mouseExit(const juce::MouseEvent&) override
        {
            setMouseCursor(juce::MouseCursor::NormalCursor);
        }

    private:
        int dragStartX = 0;
    };

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex(int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected(int menuItemID, int topLevelMenuIndex) override;

    bool keyPressed(const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void codeDocumentTextInserted(const juce::String&, int) override;
    void codeDocumentTextDeleted(int, int) override;

    void timerCallback() override;
    void compileFromText();
    void setCodeContent(const juce::String& content, bool queueCompile = true);
    void applyGraphMutation(const std::function<void(duodsp::ir::Graph&)>& mutator, bool pushHistorySnapshot = true);
    void syncCanvasFromGraph();
    void updateSelectionRail(const juce::String& text);
    std::string nextBindingName() const;
    std::string nextNodeId(const std::string& seed) const;
    void pushHistorySnapshot(bool allowCoalesceTextEdit);
    void restoreHistory(size_t index);
    void undo();
    void redo();
    void refreshProbeSelectors();
    void newPatch();
    void savePatchToFile(bool saveAs = false);
    void loadPatchFromFile();
    void editCopy();
    void editCut();
    void editPaste();
    void editDuplicate();
    void editDelete();
    void editSelectAll();
    void deleteGraphNodesById(const std::vector<std::string>& ids);

    duodsp::visual::GraphCanvas graphCanvas;
    juce::MenuBarComponent menuBar { this };
    juce::CodeDocument codeDocument;
    std::unique_ptr<juce::CodeTokeniser> tokeniser;
    juce::CodeEditorComponent codeEditor;
    SplitterBar splitter;
    juce::Label syncLabel;
    juce::Label statusLabel;
    juce::Label scopeLabel;
    juce::Label spectrumLabel;
    juce::ComboBox scopeProbeSelect;
    juce::ComboBox spectrumProbeSelect;
    duodsp::ui::ScopeWidget scopeWidget;
    duodsp::ui::SpectrumWidget spectrumWidget;

    duodsp::ir::Graph currentGraph;
    std::optional<duodsp::ir::Graph> preferredPreviousGraphForCompile;
    duodsp::sync::SyncMap currentSyncMap;
    duodsp::dsp::RuntimeEngine runtime;
    std::unordered_map<std::string, juce::Point<float>> nodeLayout;
    float splitRatio = 0.54f;
    float splitRatioDragStart = 0.54f;
    juce::Rectangle<int> rightPaneBounds;

    bool compilePending = true;
    bool suppressEditorEvents = false;
    bool isRestoringHistory = false;
    bool hasCommittedInitialSnapshot = false;
    bool textEditedSinceLastCompile = false;
    int lastCaret = -1;
    double lastTextEditTimeMs = 0.0;
    double lastHistoryCommitTimeMs = 0.0;
    bool lastHistoryCommitWasText = false;

    enum class EditOrigin
    {
        none,
        text,
        visual,
        history
    };
    EditOrigin pendingEditOrigin = EditOrigin::none;
    std::optional<std::string> pendingInlineEditNodeId;

    struct HistoryState
    {
        juce::String source;
        std::unordered_map<std::string, juce::Point<float>> layout;
    };
    std::vector<HistoryState> history;
    size_t historyIndex = 0;
    struct NodeClipboard
    {
        std::vector<duodsp::ir::Node> nodes;
        std::vector<duodsp::ir::Edge> edges;
        std::unordered_map<std::string, juce::Point<float>> layout;
    };
    std::optional<NodeClipboard> nodeClipboard;
    int pasteSerial = 0;
    std::unique_ptr<juce::FileChooser> activeSaveChooser;
    std::unique_ptr<juce::FileChooser> activeLoadChooser;
    juce::File currentPatchFile;
};
