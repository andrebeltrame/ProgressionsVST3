#include "PianoRollComponent.h"
#include "HarmoniaLookAndFeel.h"

using namespace HarmoniaColours;
using namespace harmonia;

PianoRollComponent::PianoRollComponent()
{
    setInterceptsMouseClicks(false, false);
    placeholder = "Drop a MIDI clip here";
}

void PianoRollComponent::setContent(const NoteSequence& source,
                                    const NoteSequence& generated,
                                    const Analysis& analysis)
{
    sourceSequence = source;
    generatedSequence = generated;
    currentAnalysis = analysis;

    lowestPitch = 127;
    highestPitch = 0;
    for (const auto* sequence : { &sourceSequence, &generatedSequence })
    {
        for (const auto& note : sequence->notes)
        {
            lowestPitch = juce::jmin(lowestPitch, note.pitch);
            highestPitch = juce::jmax(highestPitch, note.pitch);
        }
    }
    if (lowestPitch > highestPitch)
    {
        lowestPitch = 48;
        highestPitch = 72;
    }

    // Always show at least two octaves so a bass line does not fill the view.
    while (highestPitch - lowestPitch < 24)
    {
        if (highestPitch < 127) ++highestPitch;
        if (lowestPitch > 0) --lowestPitch;
    }
    lowestPitch = juce::jmax(0, lowestPitch - 2);
    highestPitch = juce::jmin(127, highestPitch + 2);

    totalTicks = juce::jmax<juce::int64>(1, juce::jmax(sourceSequence.lengthTicks(), generatedSequence.lengthTicks()));
    const auto barTicks = juce::jmax<juce::int64>(1, generatedSequence.notes.empty()
                                                         ? sourceSequence.ticksPerBar()
                                                         : generatedSequence.ticksPerBar());
    totalTicks = ((totalTicks + barTicks - 1) / barTicks) * barTicks;

    repaint();
}

void PianoRollComponent::setPlayPosition(double normalised)
{
    if (std::abs(normalised - playPosition) < 0.001)
        return;
    playPosition = normalised;
    repaint();
}

void PianoRollComponent::setPlaceholder(const juce::String& text)
{
    placeholder = text;
    repaint();
}

juce::Rectangle<float> PianoRollComponent::rectangleFor(const Note& note, juce::Rectangle<float> area) const
{
    const int span = juce::jmax(1, highestPitch - lowestPitch + 1);
    const float rowHeight = area.getHeight() / static_cast<float>(span);
    const float x = area.getX() + area.getWidth() * static_cast<float>(note.startTick) / static_cast<float>(totalTicks);
    const float width = juce::jmax(2.0f, area.getWidth() * static_cast<float>(note.lengthTick) / static_cast<float>(totalTicks));
    const float y = area.getY() + rowHeight * static_cast<float>(highestPitch - note.pitch);
    return { x, y, width, juce::jmax(2.0f, rowHeight - 1.0f) };
}

void PianoRollComponent::paint(juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour(panel);
    g.fillRoundedRectangle(area, 8.0f);

    const auto inner = area.reduced(8.0f);

    if (sourceSequence.notes.empty() && generatedSequence.notes.empty())
    {
        g.setColour(textDim);
        g.setFont(juce::FontOptions(15.0f));
        g.drawText(placeholder, inner, juce::Justification::centred, true);
        return;
    }

    // Rows for the black keys, so the register is readable at a glance.
    const int span = juce::jmax(1, highestPitch - lowestPitch + 1);
    const float rowHeight = inner.getHeight() / static_cast<float>(span);
    for (int pitch = lowestPitch; pitch <= highestPitch; ++pitch)
    {
        const int pc = ((pitch % 12) + 12) % 12;
        const bool blackKey = pc == 1 || pc == 3 || pc == 6 || pc == 8 || pc == 10;
        if (! blackKey)
            continue;
        g.setColour(background.withAlpha(0.35f));
        g.fillRect(inner.getX(), inner.getY() + rowHeight * static_cast<float>(highestPitch - pitch),
                   inner.getWidth(), rowHeight);
    }

    // Bar and beat grid.
    const auto& reference = generatedSequence.notes.empty() ? sourceSequence : generatedSequence;
    const auto barTicks = juce::jmax<juce::int64>(1, reference.ticksPerBar());
    const auto beatTicks = juce::jmax<juce::int64>(1, reference.ppq);
    for (juce::int64 tick = 0; tick <= totalTicks; tick += beatTicks)
    {
        const bool barLine = (tick % barTicks) == 0;
        const float x = inner.getX() + inner.getWidth() * static_cast<float>(tick) / static_cast<float>(totalTicks);
        g.setColour(barLine ? outline : outline.withAlpha(0.4f));
        g.fillRect(x, inner.getY(), barLine ? 1.2f : 0.6f, inner.getHeight());
    }

    // Chord regions, tinted behind the notes.
    for (size_t i = 0; i < currentAnalysis.progression.size(); ++i)
    {
        const auto& segment = currentAnalysis.progression[i];
        const float x = inner.getX() + inner.getWidth() * static_cast<float>(segment.startTick) / static_cast<float>(totalTicks);
        const float width = inner.getWidth() * static_cast<float>(segment.lengthTick) / static_cast<float>(totalTicks);
        if (i % 2 == 1)
        {
            g.setColour(panelLight.withAlpha(0.35f));
            g.fillRect(x, inner.getY(), width, inner.getHeight());
        }
    }

    for (const auto& note : sourceSequence.notes)
    {
        const auto rect = rectangleFor(note, inner);
        g.setColour(source.withAlpha(0.28f));
        g.fillRoundedRectangle(rect, 2.0f);
        g.setColour(source.withAlpha(0.55f));
        g.drawRoundedRectangle(rect, 2.0f, 1.0f);
    }

    for (const auto& note : generatedSequence.notes)
    {
        const auto rect = rectangleFor(note, inner);
        const float velocity = juce::jlimit(0.35f, 1.0f, static_cast<float>(note.velocity) / 110.0f);
        g.setColour(accent.withAlpha(velocity));
        g.fillRoundedRectangle(rect, 2.0f);
    }

    if (playPosition > 0.0)
    {
        const float x = inner.getX() + inner.getWidth() * static_cast<float>(playPosition);
        g.setColour(juce::Colours::white.withAlpha(0.75f));
        g.fillRect(x, inner.getY(), 1.5f, inner.getHeight());
    }
}
