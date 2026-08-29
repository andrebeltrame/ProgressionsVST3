#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "harmonia/Analysis.h"
#include "harmonia/Types.h"

/** Shows the source clip as a ghost behind the generated part, with the bar grid
    and a playhead. */
class PianoRollComponent : public juce::Component
{
public:
    PianoRollComponent();

    void setContent(const harmonia::NoteSequence& source,
                    const harmonia::NoteSequence& generated,
                    const harmonia::Analysis& analysis);
    void setPlayPosition(double normalised);
    /** Which spelling to name notes with - the b/# switch. */
    void setPreferFlats(bool shouldPreferFlats);
    void setPlaceholder(const juce::String& text);

    void paint(juce::Graphics&) override;
    void mouseMove(const juce::MouseEvent&) override;
    void mouseExit(const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> rectangleFor(const harmonia::Note& note, juce::Rectangle<float> area) const;
    /** The note under the cursor, generated part first: that is the one being
        auditioned, so it is the one worth naming. */
    const harmonia::Note* noteAt(juce::Point<float> position, bool& fromSource) const;

    harmonia::NoteSequence sourceSequence;
    harmonia::NoteSequence generatedSequence;
    harmonia::Analysis currentAnalysis;
    juce::String placeholder;

    /** Kept as a pitch rather than a pointer: the sequences are replaced on
        every regenerate, and a pointer into them would dangle. */
    int hoveredPitch = -1;
    bool preferFlats = false;
    bool hoveredIsSource = false;
    juce::Point<float> hoverPosition;

    double playPosition = 0.0;
    int lowestPitch = 48;
    int highestPitch = 72;
    juce::int64 totalTicks = 3840;
};
