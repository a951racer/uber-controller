// PluginEditor.h
// Group Manager UI: group name editors + scrollable assignment matrix.
#pragma once
#include <JuceHeader.h>
#include "PluginProcessor.h"

class UberGroupManagerEditor : public juce::AudioProcessorEditor,
                                private juce::Timer
{
public:
    explicit UberGroupManagerEditor(UberGroupManagerProcessor&);
    ~UberGroupManagerEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;
    void refreshMatrix();

    UberGroupManagerProcessor& processor;

    juce::Label titleLabel { {}, "Uber Group Manager" };
    juce::Label statusLabel { {}, "Disconnected" };

    // Group name editors
    struct GroupRow
    {
        juce::Label      label;
        juce::TextEditor nameEditor;
    };
    std::vector<std::unique_ptr<GroupRow>> groupRows;

    // Scrollable matrix
    class MatrixContent : public juce::Component
    {
    public:
        void paint(juce::Graphics&) override;
        void repaintMatrix(const std::vector<GroupDef>& groups,
                           std::vector<TrackAssignment> assignments);
    private:
        std::vector<GroupDef> currentGroups;
        std::vector<TrackAssignment> currentAssignments;
    };

    juce::Viewport matrixViewport;
    MatrixContent  matrixContent;

    std::vector<TrackAssignment> cachedAssignments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(UberGroupManagerEditor)
};
