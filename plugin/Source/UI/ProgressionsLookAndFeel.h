#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace ProgressionsColours
{
const juce::Colour background { 0xff14161c };
const juce::Colour panel      { 0xff1c1f27 };
const juce::Colour panelLight { 0xff262a35 };
const juce::Colour outline    { 0xff333846 };
const juce::Colour text       { 0xffe6e8ee };
const juce::Colour textDim    { 0xff8b91a0 };
const juce::Colour accent     { 0xff5ad1a5 };
const juce::Colour source     { 0xff7aa2f7 };
const juce::Colour warning    { 0xffef8f6e };
} // namespace ProgressionsColours

class ProgressionsLookAndFeel : public juce::LookAndFeel_V4
{
public:
    ProgressionsLookAndFeel();

    void drawRotarySlider(juce::Graphics&, int x, int y, int width, int height,
                          float sliderPos, float startAngle, float endAngle, juce::Slider&) override;

    void drawButtonBackground(juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                              bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawToggleButton(juce::Graphics&, juce::ToggleButton&,
                          bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox(juce::Graphics&, int width, int height, bool isButtonDown,
                      int buttonX, int buttonY, int buttonW, int buttonH, juce::ComboBox&) override;

    juce::Font getLabelFont(juce::Label&) override;
    juce::Font getTextButtonFont(juce::TextButton&, int buttonHeight) override;
};
