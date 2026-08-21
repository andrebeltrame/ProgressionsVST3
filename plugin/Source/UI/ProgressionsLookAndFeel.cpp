#include "ProgressionsLookAndFeel.h"

using namespace ProgressionsColours;

ProgressionsLookAndFeel::ProgressionsLookAndFeel()
{
    setColour(juce::ResizableWindow::backgroundColourId, background);
    setColour(juce::Label::textColourId, text);
    setColour(juce::Slider::textBoxTextColourId, text);
    setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour(juce::TextButton::buttonColourId, panelLight);
    setColour(juce::TextButton::buttonOnColourId, accent);
    setColour(juce::TextButton::textColourOffId, text);
    setColour(juce::TextButton::textColourOnId, background);
    setColour(juce::ComboBox::backgroundColourId, panelLight);
    setColour(juce::ComboBox::textColourId, text);
    setColour(juce::ComboBox::outlineColourId, outline);
    setColour(juce::ComboBox::arrowColourId, textDim);
    setColour(juce::PopupMenu::backgroundColourId, panel);
    setColour(juce::PopupMenu::textColourId, text);
    setColour(juce::PopupMenu::highlightedBackgroundColourId, accent.withAlpha(0.25f));
    setColour(juce::PopupMenu::highlightedTextColourId, text);
    setColour(juce::TooltipWindow::backgroundColourId, panel);
    setColour(juce::TooltipWindow::textColourId, text);
    setColour(juce::TooltipWindow::outlineColourId, outline);
}

void ProgressionsLookAndFeel::drawRotarySlider(juce::Graphics& g, int x, int y, int width, int height,
                                           float sliderPos, float startAngle, float endAngle, juce::Slider& slider)
{
    // A disabled control has to look disabled, or the greyed-out label is the
    // only hint and people keep dragging it.
    const bool enabled = slider.isEnabled();
    const auto valueColour = enabled ? accent : outline.brighter(0.1f);
    const auto pointerColour = enabled ? text : textDim.withAlpha(0.5f);

    const auto bounds = juce::Rectangle<int>(x, y, width, height).toFloat().reduced(3.0f);
    const auto radius = juce::jmin(bounds.getWidth(), bounds.getHeight()) / 2.0f;
    const auto centre = bounds.getCentre();
    const auto angle = startAngle + sliderPos * (endAngle - startAngle);
    const float thickness = juce::jmax(3.0f, radius * 0.22f);

    juce::Path track;
    track.addCentredArc(centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f,
                        0.0f, startAngle, endAngle, true);
    g.setColour(panelLight);
    g.strokePath(track, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    if (sliderPos > 0.001f)
    {
        juce::Path value;
        value.addCentredArc(centre.x, centre.y, radius - thickness * 0.5f, radius - thickness * 0.5f,
                            0.0f, startAngle, angle, true);
        g.setColour(valueColour);
        g.strokePath(value, juce::PathStrokeType(thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Path pointer;
    pointer.startNewSubPath(centre.x, centre.y - radius * 0.28f);
    pointer.lineTo(centre.x, centre.y - radius + thickness * 1.4f);
    pointer.applyTransform(juce::AffineTransform::rotation(angle, centre.x, centre.y));
    g.setColour(pointerColour);
    g.strokePath(pointer, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

void ProgressionsLookAndFeel::drawButtonBackground(juce::Graphics& g, juce::Button& button,
                                               const juce::Colour& backgroundColour,
                                               bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown)
{
    const auto bounds = button.getLocalBounds().toFloat().reduced(0.5f);
    auto fill = backgroundColour;

    if (shouldDrawButtonAsDown)
        fill = fill.brighter(0.18f);
    else if (shouldDrawButtonAsHighlighted)
        fill = fill.brighter(0.08f);

    g.setColour(fill);
    g.fillRoundedRectangle(bounds, 5.0f);
    g.setColour(button.getToggleState() ? accent : outline);
    g.drawRoundedRectangle(bounds, 5.0f, 1.0f);
}

void ProgressionsLookAndFeel::drawToggleButton(juce::Graphics& g, juce::ToggleButton& button,
                                           bool shouldDrawButtonAsHighlighted, bool)
{
    const auto bounds = button.getLocalBounds().toFloat();
    const float boxSize = juce::jmin(16.0f, bounds.getHeight() - 2.0f);
    const juce::Rectangle<float> box(bounds.getX(), bounds.getCentreY() - boxSize * 0.5f, boxSize, boxSize);
    const bool enabled = button.isEnabled();
    const auto tick = enabled ? accent : outline.brighter(0.1f);

    g.setColour(button.getToggleState() ? tick : panelLight);
    g.fillRoundedRectangle(box, 4.0f);
    g.setColour(shouldDrawButtonAsHighlighted && enabled ? accent.withAlpha(0.7f) : outline);
    g.drawRoundedRectangle(box, 4.0f, 1.0f);

    if (button.getToggleState())
    {
        juce::Path tick;
        tick.startNewSubPath(box.getX() + boxSize * 0.24f, box.getCentreY());
        tick.lineTo(box.getCentreX() - boxSize * 0.02f, box.getBottom() - boxSize * 0.28f);
        tick.lineTo(box.getRight() - boxSize * 0.2f, box.getY() + boxSize * 0.28f);
        g.setColour(background);
        g.strokePath(tick, juce::PathStrokeType(2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    g.setColour(button.isEnabled() ? text : textDim);
    g.setFont(juce::FontOptions(13.0f));
    g.drawText(button.getButtonText(), bounds.withTrimmedLeft(boxSize + 8.0f), juce::Justification::centredLeft, true);
}

void ProgressionsLookAndFeel::drawComboBox(juce::Graphics& g, int width, int height, bool,
                                       int, int, int, int, juce::ComboBox& box)
{
    const juce::Rectangle<float> bounds(0.0f, 0.0f, static_cast<float>(width), static_cast<float>(height));
    g.setColour(panelLight);
    g.fillRoundedRectangle(bounds.reduced(0.5f), 5.0f);
    g.setColour(box.hasKeyboardFocus(false) ? accent.withAlpha(0.6f) : outline);
    g.drawRoundedRectangle(bounds.reduced(0.5f), 5.0f, 1.0f);

    juce::Path arrow;
    const float cx = static_cast<float>(width) - 14.0f;
    const float cy = static_cast<float>(height) * 0.5f;
    arrow.addTriangle(cx - 4.0f, cy - 2.0f, cx + 4.0f, cy - 2.0f, cx, cy + 3.0f);
    g.setColour(textDim);
    g.fillPath(arrow);
}

juce::Font ProgressionsLookAndFeel::getLabelFont(juce::Label& label)
{
    return juce::Font(juce::FontOptions(static_cast<float>(juce::jmin(15, label.getHeight() - 2))));
}

juce::Font ProgressionsLookAndFeel::getTextButtonFont(juce::TextButton&, int buttonHeight)
{
    return juce::Font(juce::FontOptions(static_cast<float>(juce::jmin(14, buttonHeight - 8))));
}
