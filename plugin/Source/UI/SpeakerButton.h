#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ProgressionsLookAndFeel.h"

/** The built-in sound, on or off, drawn rather than written.

    This used to be a tick box called "Preview sound" among five other tick
    boxes, and whether the plugin was making the noise or the instrument on the
    track was is the one thing you need to be able to read at a glance. So it is
    a speaker now: waves when it is on, a slash through it when it is off, and
    it changes colour with its state. */
class SpeakerButton : public juce::Button
{
public:
    SpeakerButton() : juce::Button("Preview sound")
    {
        setClickingTogglesState(true);
        setTooltip("The plug-in's own sound, for auditioning without loading an instrument. "
                   "Turn it off when your own instruments are playing the parts - the MIDI "
                   "keeps going out either way.");
    }

    void paintButton(juce::Graphics& g, bool highlighted, bool) override
    {
        using namespace ProgressionsColours;

        const bool on = getToggleState();
        const auto area = getLocalBounds().toFloat().reduced(1.0f);

        g.setColour(on ? accent.withAlpha(highlighted ? 0.28f : 0.18f)
                       : panelLight.withAlpha(highlighted ? 1.0f : 0.75f));
        g.fillRoundedRectangle(area, 5.0f);
        g.setColour(on ? accent : outline);
        g.drawRoundedRectangle(area, 5.0f, 1.0f);

        // The speaker itself: a box and a cone, sized off the shorter edge so
        // the icon keeps its shape whatever the row height works out to.
        const auto icon = area.reduced(area.getWidth() * 0.14f, area.getHeight() * 0.22f);
        const float unit = juce::jmin(icon.getWidth(), icon.getHeight());
        const float cx = icon.getX() + unit * 0.10f;
        const float cy = icon.getCentreY();

        juce::Path speaker;
        speaker.addRectangle(cx, cy - unit * 0.16f, unit * 0.22f, unit * 0.32f);
        speaker.startNewSubPath(cx + unit * 0.22f, cy - unit * 0.16f);
        speaker.lineTo(cx + unit * 0.56f, cy - unit * 0.42f);
        speaker.lineTo(cx + unit * 0.56f, cy + unit * 0.42f);
        speaker.lineTo(cx + unit * 0.22f, cy + unit * 0.16f);
        speaker.closeSubPath();

        g.setColour(on ? accent : textDim);
        g.fillPath(speaker);

        if (on)
        {
            // Two arcs coming off the cone: sound is leaving the plug-in.
            // JUCE measures these angles clockwise from twelve o'clock, so the
            // pair has to straddle half pi to open to the right rather than up.
            for (int wave = 1; wave <= 2; ++wave)
            {
                const float radius = unit * (0.16f + 0.19f * static_cast<float>(wave));
                juce::Path arc;
                arc.addCentredArc(cx + unit * 0.50f, cy, radius, radius, 0.0f,
                                  juce::MathConstants<float>::halfPi - 0.85f,
                                  juce::MathConstants<float>::halfPi + 0.85f, true);
                g.strokePath(arc, juce::PathStrokeType(unit * 0.09f));
            }
        }
        else
        {
            // A slash, the way every mute button anywhere says the same thing.
            g.setColour(warning);
            g.drawLine(icon.getX() + unit * 0.05f, icon.getY() + unit * 0.05f,
                       icon.getX() + unit * 0.95f, icon.getY() + unit * 0.95f,
                       unit * 0.12f);
        }
    }
};
