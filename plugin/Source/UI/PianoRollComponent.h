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
    void setPlaceholder(const juce::String& text);

    void paint(juce::Graphics&) override;

private:
    juce::Rectangle<float> rectangleFor(const harmonia::Note& note, juce::Rectangle<float> area) const;

    harmonia::NoteSequence sourceSequence;
    harmonia::NoteSequence generatedSequence;
    harmonia::Analysis currentAnalysis;
    juce::String placeholder;

    double playPosition = 0.0;
    int lowestPitch = 48;
    int highestPitch = 72;
    juce::int64 totalTicks = 3840;
};
