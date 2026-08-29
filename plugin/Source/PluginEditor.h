#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "PluginProcessor.h"
#include "UI/AboutPanel.h"
#include "UI/DragExportComponent.h"
#include "UI/InfoPanel.h"
#include "UI/PianoRollComponent.h"
#include "UI/ProgressionStrip.h"
#include "UI/ProgressionsLookAndFeel.h"

class ProgressionsEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget,
                       private juce::ChangeListener,
                       private juce::Timer
{
public:
    explicit ProgressionsEditor(ProgressionsProcessor&);
    ~ProgressionsEditor() override;

    void paint(juce::Graphics&) override;
    void resized() override;
    void mouseDown(const juce::MouseEvent&) override;

    /** For the smoke test: a click handler that quietly does nothing is not
        something a headless build should be able to ship. */
    bool isAboutVisible() const { return aboutPanel.isVisible(); }

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
    void showStyleMenu();
    void showScanDialog();
    void showStyleDialog();
    void applyTypedProgression();
    void applyKeyFromControls();
    void refreshProgressionField();
    void refreshPartButtons();
    bool preferFlats() const;

    ProgressionsProcessor& processor;
    ProgressionsLookAndFeel lookAndFeel;
    juce::TooltipWindow tooltips { this, 500 };

    juce::Label titleLabel, taglineLabel, sourceLabel, statusLabel;
    juce::TextButton loadButton { "Load MIDI..." }, clearButton { "Clear clip" };
    juce::TextButton initButton { "Init" };
    juce::Label versionLabel;
    /** The header block that opens the about card - kept from resized() so
        mouseDown can hit-test it without asking the labels, which do not take
        clicks. */
    juce::Rectangle<int> aboutHotspot;
    AboutPanel aboutPanel;

    PianoRollComponent pianoRoll;
    ProgressionStrip progressionStrip;
    InfoPanel infoPanel;

    juce::OwnedArray<juce::TextButton> partButtons;
    std::unique_ptr<juce::ParameterAttachment> partAttachment;

    Knob densityKnob, complexityKnob, humanizeKnob, swingKnob, octaveKnob, voicesKnob, craftKnob, styleKnob,
         reharmKnob;

    juce::ComboBox lengthBox, arpBox, harmonyBox, keyRootBox, keyModeBox, meterBox, accidentalBox;
    juce::Label lengthLabel { {}, "Length" }, arpLabel { {}, "Arp shape" },
                harmonyLabel { {}, "Chord changes" },
                keyRootLabel { {}, "Key" }, keyModeLabel { {}, "Mode" },
                meterLabel { {}, "Time" }, accidentalLabel { {}, "Spelling" };

    /** The harmony block: the chords you type, and everything that decides how
        they are read. Grouped because typing a progression you looked up
        somewhere is the way most of this gets used. */
    juce::Label harmonyHeading { {}, "HARMONY" };
    juce::Label progressionHint { {}, "Type the chords" };
    juce::Label presetHint { {}, "or start from a preset" };
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
    juce::ToggleButton stackToggle { "Stack parts" };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> lengthAttachment, arpAttachment,
                                                                            harmonyAttachment, meterAttachment,
                                                                            accidentalAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> followAttachment, avoidAttachment,
                                                                          syncAttachment, previewAttachment,
                                                                          styleAttachment, stackAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> levelAttachment;

    juce::TextButton diceButton { "New idea" }, reharmButton { "Reharmonise" },
                     resetChordsButton { "Reset chords" }, playButton { "Play" }, exportButton { "Save .mid" },
                     undoButton { "Undo" }, dropPartButton { "Remove part" };
    DragExportComponent dragArea;

    std::unique_ptr<juce::FileChooser> chooser;
    bool fileDragActive = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionsEditor)
};
