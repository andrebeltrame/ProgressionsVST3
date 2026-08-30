#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "harmonia/Analysis.h"

#include <vector>

/** The detected progression as clickable chips: click a chord to walk it up a
    diatonic degree, right-click (or alt-click) to walk it back down.

    Each chip also carries a padlock in its top-right corner. Clicking that pins
    the chord: Reharmonise and Surprise me then write around it and leave it
    alone, which is how you keep the two chords you like and roll the rest. A
    pinned chip will not walk either - the lock has to mean the same thing
    wherever you push at it. */
class ProgressionStrip : public juce::Component,
                         public juce::SettableTooltipClient
{
public:
    ProgressionStrip();

    void setAnalysis(const harmonia::Analysis& analysis);
    /** In step with the progression; anything past the end reads as unlocked. */
    void setLocks(const std::vector<bool>& locks);
    /** Which spelling to name chords with - the b/# switch. */
    void setPreferFlats(bool shouldPreferFlats);
    std::function<void(int index, int direction)> onChordNudged;
    std::function<void(int index)> onLockToggled;

    void paint(juce::Graphics&) override;
    void mouseDown(const juce::MouseEvent&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    int indexAt(juce::Point<int> position) const;
    juce::Rectangle<float> chipBounds(size_t index) const;
    juce::Rectangle<float> lockBounds(size_t index) const;
    bool isLocked(size_t index) const;

    harmonia::Analysis currentAnalysis;
    std::vector<bool> locks;
    int hoveredIndex = -1;
    bool hoveredLock = false;
    bool preferFlats = false;
};
