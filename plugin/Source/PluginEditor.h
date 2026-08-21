#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/DragExportComponent.h"
#include "UI/HarmoniaLookAndFeel.h"
#include "UI/InfoPanel.h"
#include "UI/PianoRollComponent.h"
#include "UI/ProgressionStrip.h"

class HarmoniaEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit HarmoniaEditor(HarmoniaProcessor&);
    ~HarmoniaEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag(const juce::StringArray& files) override;
    void fileDragEnter(const juce::StringArray&, int, int) override;
    void fileDragExit(const juce::StringArray&) override;
    void filesDropped(const juce::StringArray& files, int x, int y) override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void changeListenerCallback(juce::ChangeBroadcaster*) override;
    void timerCallback() override;

    void buildKnob(Knob& knob, const juce::String& name, const char* parameterID, bool rotary = true);
    void refresh();
    void showLoadDialog();
    void showExportDialog();
    void showStyleDialog();
    void applyTypedProgression();
    void applyKeyFromControls();
    void refreshProgressionField();

    HarmoniaProcessor& processor;
    HarmoniaLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 500 };

    juce::Label titleLabel, taglineLabel, sourceLabel, statusLabel;
    juce::TextButton loadButton { "Load MIDI..." }, clearButton { "Clear" };

    PianoRollComponent pianoRoll;
    ProgressionStrip progressionStrip;
    InfoPanel infoPanel;

    juce::OwnedArray<juce::TextButton> partButtons;
    std::unique_ptr<juce::ParameterAttachment> partAttachment;

    Knob densityKnob, complexityKnob, humanizeKnob, swingKnob, octaveKnob, voicesKnob, styleKnob;

    juce::ComboBox lengthBox, arpBox, harmonyBox, keyRootBox, keyModeBox;
    juce::Label lengthLabel { {}, "Length" }, arpLabel { {}, "Arp shape" },
                harmonyLabel { {}, "Chord changes" },
                keyRootLabel { {}, "Key" }, keyModeLabel { {}, "Mode" };

    juce::ComboBox presetBox;
    juce::TextEditor progressionField;
    juce::TextButton applyProgressionButton { "Set" };
    juce::ToggleButton followToggle { "Follow groove" };
    juce::ToggleButton avoidToggle { "Stay clear" };
    juce::ToggleButton syncToggle { "Host sync" };
    juce::ToggleButton previewToggle { "Preview sound" };
    juce::Slider levelSlider;
    juce::Label levelLabel { {}, "Preview level" };
    juce::TextButton styleButton { "Learn from my library..." };
    juce::ToggleButton styleToggle { "Write in my style" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lengthAttachment, arpAttachment,
                                                                            harmonyAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment, avoidAttachment,
                                                                          syncAttachment, previewAttachment,
                                                                          styleAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;

    juce::TextButton diceButton { "New idea" }, reharmButton { "Reharmonise" },
                     resetChordsButton { "Reset chords" }, playButton { "Play" }, exportButton { "Save .mid" };
    DragExportComponent dragArea;

    std::unique_ptr<juce::FileChooser> chooser;
    bool fileDragActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(HarmoniaEditor)
};
