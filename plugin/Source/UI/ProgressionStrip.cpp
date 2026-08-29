#include "ProgressionStrip.h"
#include "ProgressionsLookAndFeel.h"

using namespace ProgressionsColours;

ProgressionStrip::ProgressionStrip()
{
    setTooltip("Click a chord to move it up a degree, right-click to move it down.");
}

void ProgressionStrip::setPreferFlats(bool shouldPreferFlats)
{
    if (preferFlats == shouldPreferFlats)
        return;
    preferFlats = shouldPreferFlats;
    repaint();
}

void ProgressionStrip::setAnalysis(const harmonia::Analysis& analysis)
{
    currentAnalysis = analysis;
    repaint();
}

juce::Rectangle<float> ProgressionStrip::chipBounds(size_t index) const
{
    const auto area = getLocalBounds().toFloat().reduced(2.0f);
    const auto total = static_cast<float>(juce::jmax<juce::int64>(1, currentAnalysis.lengthTicks));
    const auto& segment = currentAnalysis.progression[index];

    const float x = area.getX() + area.getWidth() * static_cast<float>(segment.startTick) / total;
    const float width = area.getWidth() * static_cast<float>(segment.lengthTick) / total;
    return juce::Rectangle<float>(x, area.getY(), width, area.getHeight()).reduced(1.5f, 0.0f);
}

int ProgressionStrip::indexAt(juce::Point<int> position) const
{
    for (size_t i = 0; i < currentAnalysis.progression.size(); ++i)
        if (chipBounds(i).contains(position.toFloat()))
            return static_cast<int>(i);
    return -1;
}

void ProgressionStrip::paint(juce::Graphics& g)
{
    if (currentAnalysis.progression.empty())
    {
        g.setColour(textDim);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("No progression yet", getLocalBounds(), juce::Justification::centred, true);
        return;
    }

    for (size_t i = 0; i < currentAnalysis.progression.size(); ++i)
    {
        const auto& segment = currentAnalysis.progression[i];
        const auto bounds = chipBounds(i);
        const bool hovered = static_cast<int>(i) == hoveredIndex;

        g.setColour(hovered ? panelLight.brighter(0.15f) : panelLight);
        g.fillRoundedRectangle(bounds, 5.0f);
        g.setColour(hovered ? accent : outline);
        g.drawRoundedRectangle(bounds, 5.0f, 1.0f);

        const auto name = juce::String(segment.chord.name(preferFlats));
        const auto numeral = juce::String(juce::CharPointer_UTF8(segment.chord.romanNumeral(currentAnalysis.key).c_str()));

        auto textArea = bounds.reduced(4.0f, 2.0f);
        g.setColour(text);
        g.setFont(juce::FontOptions(15.0f, juce::Font::bold));
        g.drawText(name, textArea.removeFromTop(textArea.getHeight() * 0.58f),
                   juce::Justification::centred, true);
        g.setColour(accent.withAlpha(0.85f));
        g.setFont(juce::FontOptions(12.0f));
        g.drawText(numeral, textArea, juce::Justification::centred, true);
    }
}

void ProgressionStrip::mouseDown(const juce::MouseEvent& event)
{
    const int index = indexAt(event.getPosition());
    if (index < 0 || onChordNudged == nullptr)
        return;

    const bool down = event.mods.isPopupMenu() || event.mods.isAltDown();
    onChordNudged(index, down ? -1 : 1);
}

void ProgressionStrip::mouseMove(const juce::MouseEvent& event)
{
    const int index = indexAt(event.getPosition());
    if (index != hoveredIndex)
    {
        hoveredIndex = index;
        repaint();
    }
}

void ProgressionStrip::mouseExit(const juce::MouseEvent&)
{
    if (hoveredIndex != -1)
    {
        hoveredIndex = -1;
        repaint();
    }
}
