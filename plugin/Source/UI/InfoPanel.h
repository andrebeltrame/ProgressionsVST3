#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "HarmoniaLookAndFeel.h"

/** A small key/value read-out for the analysis results. */
class InfoPanel : public juce::Component
{
public:
    void setRows(std::vector<std::pair<juce::String, juce::String>> newRows)
    {
        rows = std::move(newRows);
        repaint();
    }

    void setTitle(const juce::String& newTitle)
    {
        title = newTitle;
        repaint();
    }

    void paint(juce::Graphics& g) override
    {
        using namespace HarmoniaColours;
        auto area = getLocalBounds().toFloat();
        g.setColour(panel);
        g.fillRoundedRectangle(area, 8.0f);

        auto inner = area.reduced(12.0f, 10.0f);

        g.setColour(textDim);
        g.setFont(juce::FontOptions(11.0f, juce::Font::bold));
        g.drawText(title.toUpperCase(), inner.removeFromTop(16.0f), juce::Justification::topLeft, true);
        inner.removeFromTop(6.0f);

        for (const auto& [key, value] : rows)
        {
            auto row = inner.removeFromTop(19.0f);
            g.setColour(textDim);
            g.setFont(juce::FontOptions(12.0f));
            g.drawText(key, row.removeFromLeft(row.getWidth() * 0.42f), juce::Justification::centredLeft, true);
            g.setColour(text);
            g.setFont(juce::FontOptions(12.5f, juce::Font::bold));
            g.drawFittedText(value, row.toNearestInt(), juce::Justification::centredLeft, 2, 0.8f);
            inner.removeFromTop(2.0f);
        }
    }

private:
    juce::String title { "Analysis" };
    std::vector<std::pair<juce::String, juce::String>> rows;
};
