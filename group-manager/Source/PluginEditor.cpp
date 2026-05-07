// PluginEditor.cpp
#include "PluginEditor.h"
#include <algorithm>

UberGroupManagerEditor::UberGroupManagerEditor(UberGroupManagerProcessor& p)
    : AudioProcessorEditor(p), processor(p)
{
    addAndMakeVisible(titleLabel);
    titleLabel.setFont(juce::Font(16.0f, juce::Font::bold));
    titleLabel.setJustificationType(juce::Justification::centred);

    addAndMakeVisible(statusLabel);
    statusLabel.setFont(juce::Font(11.0f));

    // Create group name rows
    auto groups = processor.getGroups();
    for (int i = 0; i < (int)groups.size(); ++i)
    {
        auto row = std::make_unique<GroupRow>();
        row->label.setText("Group " + juce::String(i + 1) + ":", juce::dontSendNotification);
        row->nameEditor.setText(groups[i].name, false);

        int groupId = groups[i].id;
        row->nameEditor.onReturnKey = [this, groupId, i]
        {
            processor.setGroupName(groupId, groupRows[i]->nameEditor.getText());
        };
        row->nameEditor.onFocusLost = [this, groupId, i]
        {
            processor.setGroupName(groupId, groupRows[i]->nameEditor.getText());
        };

        addAndMakeVisible(row->label);
        addAndMakeVisible(row->nameEditor);
        groupRows.push_back(std::move(row));
    }

    // Matrix viewport
    matrixViewport.setViewedComponent(&matrixContent, false);
    matrixViewport.setScrollBarsShown(true, false);
    addAndMakeVisible(matrixViewport);

    startTimerHz(2);
    setSize(580, 600);
}

UberGroupManagerEditor::~UberGroupManagerEditor()
{
    stopTimer();
}

void UberGroupManagerEditor::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));
}

void UberGroupManagerEditor::resized()
{
    auto bounds = getLocalBounds().reduced(10);

    titleLabel.setBounds(bounds.removeFromTop(24));
    statusLabel.setBounds(bounds.removeFromTop(18));
    bounds.removeFromTop(8);

    for (auto& row : groupRows)
    {
        auto r = bounds.removeFromTop(24);
        bounds.removeFromTop(2);
        row->label.setBounds(r.removeFromLeft(70));
        row->nameEditor.setBounds(r.removeFromLeft(200));
    }

    bounds.removeFromTop(10);

    // Matrix viewport gets the remaining space
    matrixViewport.setBounds(bounds);
    refreshMatrix();
}

void UberGroupManagerEditor::timerCallback()
{
    if (processor.getReporter().isConnected())
    {
        statusLabel.setText("Connected to middleware", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::green);
    }
    else
    {
        statusLabel.setText("Disconnected - retrying...", juce::dontSendNotification);
        statusLabel.setColour(juce::Label::textColourId, juce::Colours::red);
    }

    auto newAssignments = processor.getTrackAssignments();
    if (newAssignments != cachedAssignments)
    {
        cachedAssignments = newAssignments;
        refreshMatrix();
    }
}

void UberGroupManagerEditor::refreshMatrix()
{
    matrixContent.repaintMatrix(processor.getGroups(), cachedAssignments);

    // Size the content to fit all rows
    int rowH = 18;
    int headerH = 20;
    int contentH = headerH + (int)cachedAssignments.size() * rowH + 10;
    int contentW = matrixViewport.getWidth() - (contentH > matrixViewport.getHeight() ? 12 : 0);
    matrixContent.setSize(std::max(contentW, 100), std::max(contentH, matrixViewport.getHeight()));
}

// ---------------------------------------------------------------------------
// MatrixContent
// ---------------------------------------------------------------------------

void UberGroupManagerEditor::MatrixContent::repaintMatrix(
    const std::vector<GroupDef>& groups,
    std::vector<TrackAssignment> assignments)
{
    currentGroups = groups;

    // Sort assignments: by MCU channel if set, otherwise keep original order at the end
    std::stable_sort(assignments.begin(), assignments.end(),
        [](const TrackAssignment& a, const TrackAssignment& b)
        {
            // Tracks with MCU channel set come first, sorted by channel
            if (a.mcuChannel >= 0 && b.mcuChannel >= 0)
                return a.mcuChannel < b.mcuChannel;
            if (a.mcuChannel >= 0) return true;
            if (b.mcuChannel >= 0) return false;
            return false;  // preserve original order for unassigned
        });

    currentAssignments = assignments;
    repaint();
}

void UberGroupManagerEditor::MatrixContent::paint(juce::Graphics& g)
{
    g.fillAll(juce::Colour(0xff2a2a2a));

    if (currentAssignments.empty() || currentGroups.empty())
    {
        g.setColour(juce::Colours::grey);
        g.setFont(11.0f);
        g.drawText("No track assignments received yet",
                   getLocalBounds().reduced(10), juce::Justification::centredTop);
        return;
    }

    int numGroups = (int)currentGroups.size();
    int numTracks = (int)currentAssignments.size();

    int colW    = 50;
    int rowH    = 18;
    int labelW  = 120;
    int headerH = 20;

    auto bounds = getLocalBounds().reduced(4);

    // Header row
    auto headerRow = bounds.removeFromTop(headerH);
    headerRow.removeFromLeft(labelW);

    g.setColour(juce::Colour(0xff888888));
    g.setFont(9.0f);
    for (int gi = 0; gi < numGroups; ++gi)
    {
        auto col = headerRow.removeFromLeft(colW);
        juce::String hdr = currentGroups[gi].name.isEmpty()
                               ? ("G" + juce::String(gi + 1))
                               : currentGroups[gi].name.substring(0, 6);
        g.drawText(hdr, col, juce::Justification::centred);
    }

    // Track rows
    for (int ti = 0; ti < numTracks; ++ti)
    {
        auto row = bounds.removeFromTop(rowH);
        auto labelArea = row.removeFromLeft(labelW);

        // Alternating row background
        if (ti % 2 == 0)
        {
            g.setColour(juce::Colour(0xff333333));
            g.fillRect(labelArea.getX() - 4, labelArea.getY(),
                       labelW + numGroups * colW + 8, rowH);
        }

        // Track label
        g.setColour(juce::Colour(0xffcccccc));
        g.setFont(10.0f);
        juce::String trackLabel;
        if (currentAssignments[ti].trackName.isNotEmpty())
            trackLabel = currentAssignments[ti].trackName.substring(0, 14);
        else if (currentAssignments[ti].mcuChannel >= 0)
            trackLabel = "Ch " + juce::String(currentAssignments[ti].mcuChannel + 1);
        else
            trackLabel = "Unknown";

        g.drawText(trackLabel, labelArea, juce::Justification::centredLeft);

        // Group dots
        for (int gi = 0; gi < numGroups; ++gi)
        {
            auto cell = row.removeFromLeft(colW);
            bool assigned = (currentAssignments[ti].groupId == currentGroups[gi].id);

            if (assigned)
            {
                g.setColour(juce::Colour(0xff00cc44));
                auto dotArea = cell.toFloat().reduced(cell.getWidth() * 0.3f, 3.0f);
                g.fillEllipse(dotArea);
            }
            else
            {
                g.setColour(juce::Colour(0xff404040));
                auto dotArea = cell.toFloat().reduced(cell.getWidth() * 0.3f, 3.0f);
                g.drawEllipse(dotArea, 1.0f);
            }
        }
    }
}
