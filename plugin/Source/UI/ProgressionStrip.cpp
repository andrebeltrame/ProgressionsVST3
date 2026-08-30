#include "ProgressionStrip.h"
#include "ProgressionsLookAndFeel.h"

using namespace ProgressionsColours;

ProgressionStrip::ProgressionStrip()
{
    setTooltip("Click a chord to move it up a degree, right-click to move it down. "
               "Click the padlock to pin it, and Reharmonise and Surprise me will "
               "leave that one alone.");
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

void ProgressionStrip::setLocks(const std::vector<bool>& newLocks)
{
    if (locks == newLocks)
        return;
    locks = newLocks;
    repaint();
}

bool ProgressionStrip::isLocked(size_t index) const
{
    return index < locks.size() && locks[index];
}

juce::Rectangle<float> ProgressionStrip::lockBounds(size_t index) const
{
    // The corner of the chip, and never more than a third of it: on a
    // sixteen-chord progression the chips get narrow and the padlock must not
    // swallow the chord underneath it.
    const auto chip = chipBounds(index);
    const float size = juce::jlimit(12.0f, 20.0f, juce::jmin(chip.getWidth() / 3.0f, chip.getHeight() / 2.4f));
    return { chip.getRight() - size - 2.0f, chip.getY() + 2.0f, size, size };
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
        const bool locked = isLocked(i);

        g.setColour(hovered ? panelLight.brighter(0.15f) : panelLight);
        g.fillRoundedRectangle(bounds, 5.0f);
        // A pinned chord reads as decided: a solid accent edge, thicker than
        // the hover outline, so it is obvious without hovering anything.
        g.setColour(locked ? accent : (hovered ? accent : outline));
        g.drawRoundedRectangle(bounds, 5.0f, locked ? 2.0f : 1.0f);

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

        // The padlock. Drawn faintly until the chip is hovered, so a progression
        // nobody is touching stays readable as chords rather than as controls.
        const auto lock = lockBounds(i);
        const bool overLock = hovered && hoveredLock;
        const float alpha = locked ? 1.0f : (overLock ? 0.9f : (hovered ? 0.45f : 0.16f));

        const float bodyHeight = lock.getHeight() * 0.42f;
        const auto body = juce::Rectangle<float>(lock.getX() + lock.getWidth() * 0.18f,
                                                 lock.getBottom() - bodyHeight - lock.getHeight() * 0.16f,
                                                 lock.getWidth() * 0.64f, bodyHeight);

        juce::Path shackle;
        const float shackleWidth = body.getWidth() * 0.62f;
        const float shackleHeight = lock.getHeight() * 0.34f;
        // Open when unlocked - the shackle leans out of the body - and closed
        // when it is not, which is the whole message at this size.
        shackle.addCentredArc(body.getCentreX() + (locked ? 0.0f : shackleWidth * 0.35f),
                              body.getY(), shackleWidth * 0.5f, shackleHeight * 0.5f,
                              0.0f, -juce::MathConstants<float>::halfPi,
                              juce::MathConstants<float>::halfPi, true);

        g.setColour((locked ? accent : textDim).withAlpha(alpha));
        g.fillRoundedRectangle(body, 1.5f);
        g.strokePath(shackle, juce::PathStrokeType(juce::jmax(1.0f, lock.getHeight() * 0.11f)));
    }
}

void ProgressionStrip::mouseDown(const juce::MouseEvent& event)
{
    const int index = indexAt(event.getPosition());
    if (index < 0)
        return;

    if (lockBounds(static_cast<size_t>(index)).contains(event.getPosition().toFloat()))
    {
        if (onLockToggled != nullptr)
            onLockToggled(index);
        return;
    }

    if (onChordNudged == nullptr)
        return;

    const bool down = event.mods.isPopupMenu() || event.mods.isAltDown();
    onChordNudged(index, down ? -1 : 1);
}

void ProgressionStrip::mouseMove(const juce::MouseEvent& event)
{
    const int index = indexAt(event.getPosition());
    const bool overLock = index >= 0
                       && lockBounds(static_cast<size_t>(index)).contains(event.getPosition().toFloat());

    if (index != hoveredIndex || overLock != hoveredLock)
    {
        hoveredIndex = index;
        hoveredLock = overLock;
        repaint();
    }
}

void ProgressionStrip::mouseExit(const juce::MouseEvent&)
{
    if (hoveredIndex != -1 || hoveredLock)
    {
        hoveredIndex = -1;
        hoveredLock = false;
        repaint();
    }
}
