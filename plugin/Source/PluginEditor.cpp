#include "PluginEditor.h"

using namespace HarmoniaColours;
using namespace harmonia;

namespace
{
const juce::StringArray kPartLabels { "Pad", "Chords", "Melody", "Counter", "Bass", "Arp" };

juce::String formatBpm(double bpm)
{
    return juce::String(bpm, 1) + " BPM";
}
} // namespace

HarmoniaEditor::HarmoniaEditor(HarmoniaProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);

    titleLabel.setText("HARMONIA", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(titleLabel);

    taglineLabel.setText("drop a clip, get ideas that fit it", juce::dontSendNotification);
    taglineLabel.setFont(juce::FontOptions(12.0f));
    taglineLabel.setColour(juce::Label::textColourId, textDim);
    addAndMakeVisible(taglineLabel);

    sourceLabel.setFont(juce::FontOptions(13.0f));
    sourceLabel.setJustificationType(juce::Justification::centredRight);
    sourceLabel.setColour(juce::Label::textColourId, textDim);
    addAndMakeVisible(sourceLabel);

    statusLabel.setFont(juce::FontOptions(12.0f));
    statusLabel.setColour(juce::Label::textColourId, warning);
    statusLabel.setJustificationType(juce::Justification::centredLeft);
    addAndMakeVisible(statusLabel);

    loadButton.onClick = [this] { showLoadDialog(); };
    addAndMakeVisible(loadButton);

    clearButton.onClick = [this] { processor.clearSource(); };
    addAndMakeVisible(clearButton);

    addAndMakeVisible(pianoRoll);
    addAndMakeVisible(infoPanel);

    progressionStrip.onChordNudged = [this](int index, int direction) { processor.nudgeChord(index, direction); };
    addAndMakeVisible(progressionStrip);

    // ---- Part selector ------------------------------------------------------
    auto* partParameter = processor.apvts.getParameter(ParamID::part);
    for (int i = 0; i < kPartLabels.size(); ++i)
    {
        auto* button = partButtons.add(new juce::TextButton(kPartLabels[i]));
        button->setClickingTogglesState(false);
        button->setTooltip("Write a " + kPartLabels[i].toLowerCase() + " over the detected harmony");
        button->onClick = [this, i]
        {
            if (partAttachment != nullptr)
                partAttachment->setValueAsCompleteGesture(static_cast<float>(i));
        };
        addAndMakeVisible(button);
    }

    partAttachment = std::make_unique<juce::ParameterAttachment>(*partParameter, [this](float value)
    {
        const int index = juce::roundToInt(value);
        for (int i = 0; i < partButtons.size(); ++i)
            partButtons[i]->setToggleState(i == index, juce::dontSendNotification);
        refresh();
    });
    partAttachment->sendInitialUpdate();

    // ---- Knobs --------------------------------------------------------------
    buildKnob(densityKnob, "Density", ParamID::density);
    buildKnob(complexityKnob, "Colour", ParamID::complexity);
    buildKnob(humanizeKnob, "Human", ParamID::humanize);
    buildKnob(swingKnob, "Swing", ParamID::swing);
    buildKnob(octaveKnob, "Octave", ParamID::octave);
    buildKnob(voicesKnob, "Voices", ParamID::voices);

    densityKnob.slider.setTooltip("How busy the generated part is");
    complexityKnob.slider.setTooltip("Chord extensions and chromatic movement");
    humanizeKnob.slider.setTooltip("Timing and velocity jitter");
    swingKnob.slider.setTooltip("Shuffle on the off-beat 16ths");
    octaveKnob.slider.setTooltip("Move the part up or down whole octaves");
    voicesKnob.slider.setTooltip("Maximum notes in a chord voicing");

    // ---- Side panel controls -------------------------------------------------
    for (auto* label : { &lengthLabel, &arpLabel, &harmonyLabel, &levelLabel })
    {
        label->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, textDim);
        addAndMakeVisible(label);
    }

    lengthBox.addItemList({ "As source", "1 bar", "2 bars", "4 bars", "8 bars", "16 bars" }, 1);
    addAndMakeVisible(lengthBox);
    lengthAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParamID::bars, lengthBox);

    harmonyBox.addItemList({ "Auto", "1 per bar", "2 per bar", "1 per beat" }, 1);
    harmonyBox.setTooltip("How often the detector is allowed to change chord. Auto reads it from the clip.");
    addAndMakeVisible(harmonyBox);
    harmonyAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParamID::harmonicRhythm, harmonyBox);

    arpBox.addItemList({ "Up", "Down", "Up-Down", "Down-Up", "Converge", "Random" }, 1);
    addAndMakeVisible(arpBox);
    arpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParamID::arpPattern, arpBox);

    for (auto* toggle : { &followToggle, &avoidToggle, &syncToggle, &previewToggle })
        addAndMakeVisible(toggle);

    followToggle.setTooltip("Borrow the onset pattern of the clip you loaded");
    avoidToggle.setTooltip("Keep the new part out of the register the clip already occupies");
    syncToggle.setTooltip("Play in time with the host transport instead of the Play button");

    followAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamID::follow, followToggle);
    avoidAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamID::avoid, avoidToggle);
    syncAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamID::hostSync, syncToggle);
    previewAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamID::preview, previewToggle);

    levelSlider.setSliderStyle(juce::Slider::LinearHorizontal);
    levelSlider.setTextBoxStyle(juce::Slider::TextBoxRight, false, 58, 18);
    levelSlider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    levelSlider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    levelSlider.setColour(juce::Slider::textBoxTextColourId, text);
    levelSlider.setColour(juce::Slider::trackColourId, accent);
    levelSlider.setColour(juce::Slider::backgroundColourId, panelLight);
    levelSlider.setColour(juce::Slider::thumbColourId, text);
    addAndMakeVisible(levelSlider);
    levelAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, ParamID::level, levelSlider);

    // ---- Actions -------------------------------------------------------------
    diceButton.setTooltip("Roll a new seed - same settings, a different idea");
    diceButton.onClick = [this] { processor.rollNewSeed(); };
    addAndMakeVisible(diceButton);

    reharmButton.setTooltip("Substitute chords in the detected progression");
    reharmButton.onClick = [this] { processor.reharmonize(0.5f); };
    addAndMakeVisible(reharmButton);

    resetChordsButton.setTooltip("Go back to the progression detected from the clip");
    resetChordsButton.onClick = [this] { processor.resetProgression(); };
    addAndMakeVisible(resetChordsButton);

    playButton.setClickingTogglesState(true);
    playButton.setTooltip("Audition the generated part without the host transport");
    playButton.onClick = [this]
    {
        if (playButton.getToggleState())
            processor.startPlayback();
        else
            processor.stopPlayback();
        playButton.setButtonText(playButton.getToggleState() ? "Stop" : "Play");
    };
    addAndMakeVisible(playButton);

    exportButton.onClick = [this] { showExportDialog(); };
    addAndMakeVisible(exportButton);

    dragArea.prepareFile = [this] { return processor.writeDragFile(); };
    addAndMakeVisible(dragArea);

    processor.addChangeListener(this);
    refresh();

    setResizable(true, true);
    setResizeLimits(940, 700, 1800, 1200);
    setSize(1020, 720);
    startTimerHz(30);
}

HarmoniaEditor::~HarmoniaEditor()
{
    processor.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void HarmoniaEditor::buildKnob(Knob& knob, const juce::String& name, const char* parameterID, bool rotary)
{
    knob.slider.setSliderStyle(rotary ? juce::Slider::RotaryHorizontalVerticalDrag : juce::Slider::LinearHorizontal);
    knob.slider.setTextBoxStyle(juce::Slider::TextBoxBelow, false, 64, 16);
    knob.slider.setColour(juce::Slider::textBoxTextColourId, text);
    knob.slider.setColour(juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    knob.slider.setColour(juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    addAndMakeVisible(knob.slider);

    knob.label.setText(name, juce::dontSendNotification);
    knob.label.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    knob.label.setJustificationType(juce::Justification::centred);
    knob.label.setColour(juce::Label::textColourId, textDim);
    addAndMakeVisible(knob.label);

    knob.attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment>(
        processor.apvts, parameterID, knob.slider);
}

// ---------------------------------------------------------------------------

void HarmoniaEditor::refresh()
{
    const auto& analysis = processor.getEngine().analysis();
    const bool hasSource = processor.getEngine().hasSource();

    pianoRoll.setContent(processor.getEngine().source(), processor.getGeneratedSequence(), analysis);
    progressionStrip.setAnalysis(analysis);

    sourceLabel.setText(hasSource ? processor.getSourceName() : juce::String("no clip loaded"),
                        juce::dontSendNotification);
    statusLabel.setText(processor.getLastError(), juce::dontSendNotification);

    std::vector<std::pair<juce::String, juce::String>> rows;
    if (hasSource && analysis.valid)
    {
        rows.emplace_back("Key", juce::String(analysis.key.name()) + "  ("
                                     + juce::String(juce::roundToInt(analysis.keyConfidence * 100.0f)) + "%)");
        rows.emplace_back("Tempo", formatBpm(analysis.bpm) + "  " + juce::String(analysis.timeSignature.numerator)
                                       + "/" + juce::String(analysis.timeSignature.denominator));
        rows.emplace_back("Length", juce::String(analysis.bars) + " bars");
        rows.emplace_back("Clip reads as", juce::String(toString(analysis.role)));
        rows.emplace_back("Register", juce::String(noteName(analysis.rhythm.lowestPitch)) + " - "
                                          + juce::String(noteName(analysis.rhythm.highestPitch)));
        rows.emplace_back("Density", juce::String(analysis.rhythm.notesPerBar, 1) + " notes/bar");
        rows.emplace_back("Seed", juce::String(static_cast<juce::int64>(processor.getSeed())));
        rows.emplace_back("Generated", juce::String(static_cast<int>(processor.getGeneratedSequence().notes.size()))
                                           + " notes");
    }
    else
    {
        rows.emplace_back("Status", "waiting for a clip");
    }
    infoPanel.setRows(std::move(rows));

    const int part = juce::roundToInt(processor.apvts.getRawParameterValue(ParamID::part)->load());
    const bool chordPart = part == 0 || part == 1 || part == 5;
    voicesKnob.slider.setEnabled(chordPart);
    voicesKnob.label.setEnabled(chordPart);
    arpBox.setEnabled(part == 5);
    arpLabel.setEnabled(part == 5);

    const bool hasOutput = ! processor.getGeneratedSequence().empty();
    exportButton.setEnabled(hasOutput);
    dragArea.setEnabledState(hasOutput);
    playButton.setEnabled(hasOutput);
    diceButton.setEnabled(hasSource);
    reharmButton.setEnabled(hasSource);
    resetChordsButton.setEnabled(hasSource);
    clearButton.setEnabled(hasSource);

    pianoRoll.setPlaceholder(hasSource ? "Nothing generated for these settings"
                                       : "Drop a MIDI clip here, or use Load MIDI");
}

void HarmoniaEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refresh();
}

void HarmoniaEditor::timerCallback()
{
    pianoRoll.setPlayPosition(processor.getPlayPositionNormalised());

    if (! processor.isPlayingInternally() && playButton.getToggleState())
    {
        playButton.setToggleState(false, juce::dontSendNotification);
        playButton.setButtonText("Play");
    }
}

// ---------------------------------------------------------------------------

void HarmoniaEditor::showLoadDialog()
{
    chooser = std::make_unique<juce::FileChooser>("Choose a MIDI clip", juce::File(), "*.mid;*.midi");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc)
                         {
                             const auto file = fc.getResult();
                             if (file.existsAsFile())
                                 processor.loadMidiFile(file);
                         });
}

void HarmoniaEditor::showExportDialog()
{
    const auto suggested = juce::File::getSpecialLocation(juce::File::userMusicDirectory)
                               .getChildFile(processor.getSourceName().isNotEmpty()
                                                 ? processor.getSourceName() + "_idea.mid"
                                                 : juce::String("harmonia_idea.mid"));

    chooser = std::make_unique<juce::FileChooser>("Save the generated part", suggested, "*.mid");
    chooser->launchAsync(juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                         [this](const juce::FileChooser& fc)
                         {
                             const auto file = fc.getResult();
                             if (file != juce::File())
                                 processor.exportGenerated(file.withFileExtension(".mid"));
                         });
}

// ---------------------------------------------------------------------------

bool HarmoniaEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase(".mid") || path.endsWithIgnoreCase(".midi"))
            return true;
    return false;
}

void HarmoniaEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    fileDragActive = true;
    repaint();
}

void HarmoniaEditor::fileDragExit(const juce::StringArray&)
{
    fileDragActive = false;
    repaint();
}

void HarmoniaEditor::filesDropped(const juce::StringArray& files, int, int)
{
    fileDragActive = false;
    repaint();

    for (const auto& path : files)
    {
        const juce::File file(path);
        if (file.hasFileExtension("mid;midi"))
        {
            processor.loadMidiFile(file);
            return;
        }
    }
}

// ---------------------------------------------------------------------------

void HarmoniaEditor::paint(juce::Graphics& g)
{
    g.fillAll(background);

    if (fileDragActive)
    {
        g.setColour(accent.withAlpha(0.10f));
        g.fillAll();
        g.setColour(accent);
        g.drawRoundedRectangle(getLocalBounds().toFloat().reduced(4.0f), 10.0f, 2.0f);
    }
}

void HarmoniaEditor::resized()
{
    auto area = getLocalBounds().reduced(12);

    // ---- Header --------------------------------------------------------------
    auto header = area.removeFromTop(52);
    {
        auto buttons = header.removeFromRight(220);
        buttons.removeFromTop(8);
        clearButton.setBounds(buttons.removeFromRight(80).reduced(2, 6));
        loadButton.setBounds(buttons.removeFromRight(130).reduced(2, 6));

        auto titleArea = header.removeFromLeft(220);
        titleLabel.setBounds(titleArea.removeFromTop(30));
        taglineLabel.setBounds(titleArea);

        sourceLabel.setBounds(header.removeFromTop(26).reduced(8, 0));
        statusLabel.setBounds(header.reduced(8, 0));
    }

    area.removeFromTop(8);

    // ---- Footer --------------------------------------------------------------
    auto footer = area.removeFromBottom(196);
    area.removeFromBottom(10);
    {
        auto partRow = footer.removeFromTop(34);
        const int buttonWidth = partRow.getWidth() / juce::jmax(1, partButtons.size());
        for (auto* button : partButtons)
            button->setBounds(partRow.removeFromLeft(buttonWidth).reduced(3, 0));

        footer.removeFromTop(10);

        auto knobRow = footer.removeFromTop(90);
        Knob* knobs[] = { &densityKnob, &complexityKnob, &humanizeKnob, &swingKnob, &octaveKnob, &voicesKnob };
        const int knobWidth = knobRow.getWidth() / 6;
        for (auto* knob : knobs)
        {
            auto cell = knobRow.removeFromLeft(knobWidth).reduced(4, 0);
            knob->label.setBounds(cell.removeFromTop(14));
            knob->slider.setBounds(cell);
        }

        footer.removeFromTop(8);

        auto actionRow = footer;
        dragArea.setBounds(actionRow.removeFromRight(190).reduced(2, 4));
        actionRow.removeFromRight(8);
        const int actionWidth = juce::jmin(140, actionRow.getWidth() / 5);
        for (auto* button : { &diceButton, &playButton, &reharmButton, &resetChordsButton, &exportButton })
            button->setBounds(actionRow.removeFromLeft(actionWidth).reduced(3, 4));
    }

    // ---- Side panel ----------------------------------------------------------
    auto side = area.removeFromLeft(268);
    area.removeFromLeft(12);
    {
        // Everything below the read-out is fixed height; the read-out takes the rest.
        constexpr int controlsHeight = 14 + 26 + 8 + 14 + 26 + 12 + 24 + 4 + 24 + 12 + 14 + 24;
        infoPanel.setBounds(side.removeFromTop(juce::jmax(150, side.getHeight() - controlsHeight - 10)));
        side.removeFromTop(10);

        auto labelRow = side.removeFromTop(14);
        auto comboRow = side.removeFromTop(26);
        const int half = side.getWidth() / 2;
        lengthLabel.setBounds(labelRow.removeFromLeft(half).reduced(2, 0));
        harmonyLabel.setBounds(labelRow.reduced(2, 0));
        lengthBox.setBounds(comboRow.removeFromLeft(half).reduced(2, 0));
        harmonyBox.setBounds(comboRow.reduced(2, 0));

        side.removeFromTop(8);
        arpLabel.setBounds(side.removeFromTop(14).removeFromLeft(half).reduced(2, 0));
        arpBox.setBounds(side.removeFromTop(26).removeFromLeft(half).reduced(2, 0));

        side.removeFromTop(12);
        auto toggleRow = side.removeFromTop(24);
        followToggle.setBounds(toggleRow.removeFromLeft(half).reduced(2, 0));
        syncToggle.setBounds(toggleRow.reduced(2, 0));

        side.removeFromTop(4);
        toggleRow = side.removeFromTop(24);
        avoidToggle.setBounds(toggleRow.removeFromLeft(half).reduced(2, 0));
        previewToggle.setBounds(toggleRow.reduced(2, 0));

        side.removeFromTop(12);
        levelLabel.setBounds(side.removeFromTop(14));
        levelSlider.setBounds(side.removeFromTop(24));
    }

    // ---- Main view -----------------------------------------------------------
    progressionStrip.setBounds(area.removeFromBottom(54));
    area.removeFromBottom(8);
    pianoRoll.setBounds(area);
}
