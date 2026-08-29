#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "harmonia/Analysis.h"

/** The detected progression as clickable chips: click a chord to walk it up a
    diatonic degree, right-click (or alt-click) to walk it back down. */
class ProgressionStrip : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    ProgressionStrip();

    void setAnalysis(const harmonia::Analysis& analysis);
    /** Which spelling to name chords with - the b/# switch. */
    void setPreferFlats(bool shouldPreferFlats);
    std::function<void(int index, int direction)> onChordNudged;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int indexAt(juce::Point<int> position) const;
    juce::Rectangle<float> chipBounds(size_t index) const;

    harmonia::Analysis currentAnalysis;
    int hoveredIndex = -1;
    bool preferFlats = false;
};
