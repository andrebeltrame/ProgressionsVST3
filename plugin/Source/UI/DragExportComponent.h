#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include "ProgressionsLookAndFeel.h"

/** Drag this out of the plugin window and the generated part lands in the DAW
    as a MIDI clip. */
class DragExportComponent : public juce::Component
{
public:
    std::function<juce::File()> prepareFile;

    DragExportComponent()
    {
        setMouseCursor(juce::MouseCursor::DraggingHandCursor);
    }

    void setEnabledState(bool shouldBeEnabled)
    {
        enabledState = shouldBeEnabled;
        setInterceptsMouseClicks(shouldBeEnabled, false);
        repaint();
    }

    void mouseDrag(const juce::MouseEvent&) override
    {
        if (dragging || ! enabledState || prepareFile == nullptr)
            return;

        const auto file = prepareFile();
        if (! file.existsAsFile())
            return;

        dragging = true;
        juce::DragAndDropContainer::performExternalDragDropOfFiles(
            { file.getFullPathName() }, false, this, [safe = juce::Component::SafePointer<DragExportComponent>(this)]
            {
                if (safe != nullptr)
                    safe->dragging = false;
            });
    }

    void paint(juce::Graphics& g) override
    {
        using namespace ProgressionsColours;
        const auto bounds = getLocalBounds().toFloat().reduced(1.0f);

        juce::Path outlinePath;
        outlinePath.addRoundedRectangle(bounds, 5.0f);
        const float dashes[] = { 5.0f, 4.0f };

        g.setColour(enabledState ? accent.withAlpha(0.12f) : panelLight.withAlpha(0.4f));
        g.fillRoundedRectangle(bounds, 5.0f);

        juce::Path dashed;
        juce::PathStrokeType(1.2f).createDashedStroke(dashed, outlinePath, dashes, 2);
        g.setColour(enabledState ? accent.withAlpha(0.8f) : outline);
        g.fillPath(dashed);

        g.setColour(enabledState ? text : textDim);
        g.setFont(juce::FontOptions(13.0f));
        g.drawText("Drag MIDI to your DAW", bounds, juce::Justification::centred, true);
    }

private:
    bool dragging = false;
    bool enabledState = false;
};
