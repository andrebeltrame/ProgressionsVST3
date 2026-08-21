#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ProgressionsLookAndFeel.h"

#ifndef JucePlugin_VersionString
 #define JucePlugin_VersionString "1.0.0"
#endif

/** The about card: who made this, which version, and the one thing a buyer
    should know about what happens to their MIDI collection.

    An overlay rather than a dialog window: JUCE_MODAL_LOOPS_PERMITTED is off,
    and a plugin that opens an OS window for an about box is a plugin that
    misbehaves in half the hosts out there. */
class AboutPanel : public juce::Component
{
public:
    AboutPanel()
    {
        closeButton.onClick = [this] { setVisible(false); };
        addAndMakeVisible(closeButton);
        setAlwaysOnTop(true);
    }

    void paint(juce::Graphics& g) override
    {
        using namespace ProgressionsColours;

        // Dim whatever is behind, so the card reads as a layer above the plugin.
        g.fillAll(background.withAlpha(0.82f));

        auto card = cardBounds().toFloat();
        g.setColour(panel);
        g.fillRoundedRectangle(card, 10.0f);
        g.setColour(outline);
        g.drawRoundedRectangle(card, 10.0f, 1.0f);

        auto inner = card.reduced(26.0f, 22.0f);

        g.setColour(accent);
        g.setFont(juce::FontOptions(24.0f, juce::Font::bold));
        g.drawText("PROGRESSIONS", inner.removeFromTop(30.0f), juce::Justification::topLeft, false);

        g.setColour(text);
        g.setFont(juce::FontOptions(14.0f));
        g.drawText("Nowhr Dynamics", inner.removeFromTop(22.0f), juce::Justification::topLeft, false);

        g.setColour(textDim);
        g.setFont(juce::FontOptions(12.0f));
        g.drawText("Version " JucePlugin_VersionString, inner.removeFromTop(20.0f),
                   juce::Justification::topLeft, false);

        inner.removeFromTop(14.0f);
        g.setColour(outline);
        g.fillRect(inner.removeFromTop(1.0f));
        inner.removeFromTop(14.0f);

        g.setColour(text);
        g.setFont(juce::FontOptions(13.0f));
        const juce::String body =
            "Drop in a MIDI clip, or type a progression, and Progressions writes the parts "
            "that go with it: pads, melodies, counter melodies, basslines, arps and plucks.\n"
            "\n"
            "Point it at your own MIDI collection and it learns how you write - which bars "
            "carry the groove, how your lines move, how you voice a chord - and writes with "
            "your habits instead of a factory preset.\n"
            "\n"
            "Your collection never leaves this machine. What the plugin keeps is statistics, "
            "never phrases: no clip of yours can be reconstructed from it, and none is stored.";
        g.drawFittedText(body, inner.removeFromTop(inner.getHeight() - 42.0f).toNearestInt(),
                         juce::Justification::topLeft, 20);

        g.setColour(textDim);
        g.setFont(juce::FontOptions(11.0f));
        g.drawText(juce::String(juce::CharPointer_UTF8("\xc2\xa9")) + " Nowhr Dynamics",
                   inner.removeFromBottom(18.0f), juce::Justification::bottomLeft, false);
    }

    void resized() override
    {
        closeButton.setBounds(cardBounds().removeFromBottom(46).removeFromRight(120).reduced(26, 8));
    }

    void mouseDown(const juce::MouseEvent& event) override
    {
        // Clicking off the card dismisses it, the way every overlay should.
        if (! cardBounds().contains(event.getPosition()))
            setVisible(false);
    }

private:
    juce::Rectangle<int> cardBounds() const
    {
        return getLocalBounds().withSizeKeepingCentre(juce::jmin(520, getWidth() - 80),
                                                      juce::jmin(336, getHeight() - 80));
    }

    juce::TextButton closeButton { "Close" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(AboutPanel)
};
