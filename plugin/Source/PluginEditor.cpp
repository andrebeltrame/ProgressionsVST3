#include "PluginEditor.h"

using namespace HarmoniaColours;
using namespace harmonia;

namespace
{
const juce::StringArray kPartLabels { "Pad", "Chords", "Melody", "Counter", "Bass", "Arp", "Pluck" };
const juce::StringArray kModeNames { "Major", "Minor", "Dorian", "Mixolydian", "Lydian", "Phrygian", "Harmonic Minor" };

harmonia::ScaleType scaleForModeIndex(int index)
{
    switch (index)
    {
        case 0:  return harmonia::ScaleType::Major;
        case 2:  return harmonia::ScaleType::Dorian;
        case 3:  return harmonia::ScaleType::Mixolydian;
        case 4:  return harmonia::ScaleType::Lydian;
        case 5:  return harmonia::ScaleType::Phrygian;
        case 6:  return harmonia::ScaleType::HarmonicMinor;
        default: return harmonia::ScaleType::NaturalMinor;
    }
}

int modeIndexForScale(harmonia::ScaleType scale)
{
    switch (scale)
    {
        case harmonia::ScaleType::Major:         return 0;
        case harmonia::ScaleType::Dorian:        return 2;
        case harmonia::ScaleType::Mixolydian:    return 3;
        case harmonia::ScaleType::Lydian:        return 4;
        case harmonia::ScaleType::Phrygian:      return 5;
        case harmonia::ScaleType::HarmonicMinor: return 6;
        default:                                 return 1;
    }
}

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

    // ---- Key ------------------------------------------------------------------
    for (auto* label : { &keyRootLabel, &keyModeLabel })
    {
        label->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        label->setColour(juce::Label::textColourId, textDim);
        addAndMakeVisible(label);
    }

    keyRootBox.addItem("Auto", 1);
    for (int pitchClass = 0; pitchClass < 12; ++pitchClass)
        keyRootBox.addItem(harmonia::pitchClassName(pitchClass), pitchClass + 2);
    keyRootBox.setTooltip("Pin the key instead of letting the detector pick it. "
                          "Roman numerals you type are read in this key.");
    keyRootBox.onChange = [this] { applyKeyFromControls(); };
    addAndMakeVisible(keyRootBox);

    keyModeBox.addItemList(kModeNames, 1);
    keyModeBox.setSelectedId(2, juce::dontSendNotification);
    keyModeBox.onChange = [this] { applyKeyFromControls(); };
    addAndMakeVisible(keyModeBox);

    // ---- Written progressions ---------------------------------------------------
    presetBox.setTextWhenNothingSelected("Style presets...");
    presetBox.setTooltip("Drop in a ready-made progression, transposed to the current key.");
    {
        int itemId = 1;
        juce::String currentStyle;
        for (const auto& preset : harmonia::progressionPresets())
        {
            if (preset.style != currentStyle.toStdString())
            {
                currentStyle = preset.style;
                presetBox.addSectionHeading(currentStyle);
            }
            presetBox.addItem(juce::String(preset.name) + "   " + juce::String(preset.numerals), itemId++);
        }
    }
    presetBox.onChange = [this]
    {
        const int index = presetBox.getSelectedItemIndex();
        const auto& presets = harmonia::progressionPresets();
        if (juce::isPositiveAndBelow(index, static_cast<int>(presets.size())))
            processor.applyPreset(presets[static_cast<size_t>(index)].id);
    };
    addAndMakeVisible(presetBox);

    progressionField.setMultiLine(false);
    progressionField.setReturnKeyStartsNewLine(false);
    progressionField.setTextToShowWhenEmpty("Type chords: Am | F | C | G   or   i VI III VII",
                                            textDim.withAlpha(0.7f));
    progressionField.setColour(juce::TextEditor::backgroundColourId, panelLight);
    progressionField.setColour(juce::TextEditor::outlineColourId, outline);
    progressionField.setColour(juce::TextEditor::focusedOutlineColourId, accent);
    progressionField.setColour(juce::TextEditor::textColourId, text);
    progressionField.setColour(juce::TextEditor::highlightColourId, accent.withAlpha(0.3f));
    progressionField.setFont(juce::FontOptions(14.0f));
    progressionField.onReturnKey = [this] { applyTypedProgression(); };
    progressionField.onFocusLost = [this] { applyTypedProgression(); };
    addAndMakeVisible(progressionField);

    applyProgressionButton.setTooltip("Use these chords instead of the detected ones");
    applyProgressionButton.onClick = [this] { applyTypedProgression(); };
    addAndMakeVisible(applyProgressionButton);

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
    setResizeLimits(980, 720, 1800, 1300);
    setSize(1060, 760);
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
    clearButton.setEnabled(hasSource);

    pianoRoll.setPlaceholder(hasSource ? "Nothing generated for these settings"
                                       : "Drop a MIDI clip here, or just type a progression below");

    refreshProgressionField();

    if (! keyRootBox.hasKeyboardFocus(true))
    {
        const int wanted = processor.isKeyForced() ? processor.forcedTonic() + 2 : 1;
        if (keyRootBox.getSelectedId() != wanted)
            keyRootBox.setSelectedId(wanted, juce::dontSendNotification);
        keyModeBox.setSelectedItemIndex(modeIndexForScale(processor.isKeyForced() ? processor.forcedScale()
                                                                                  : analysis.key.scale),
                                        juce::dontSendNotification);
        keyModeBox.setEnabled(processor.isKeyForced());
        keyModeLabel.setEnabled(processor.isKeyForced());
    }

    resetChordsButton.setEnabled(hasSource || processor.hasWrittenProgression());
    diceButton.setEnabled(hasSource || analysis.valid);
    reharmButton.setEnabled(analysis.valid);
}

void HarmoniaEditor::applyTypedProgression()
{
    const auto text = progressionField.getText().trim();
    if (text.isEmpty())
        return;

    // Nothing to do if it already matches what is playing.
    if (text == processor.getProgressionText())
        return;

    if (! processor.setProgressionText(text))
        progressionField.setColour(juce::TextEditor::outlineColourId, warning);
    else
        progressionField.setColour(juce::TextEditor::outlineColourId, outline);
    progressionField.repaint();
}

void HarmoniaEditor::applyKeyFromControls()
{
    const int rootIndex = keyRootBox.getSelectedItemIndex(); // 0 = Auto
    const bool forced = rootIndex > 0;
    keyModeBox.setEnabled(forced);
    keyModeLabel.setEnabled(forced);

    processor.setForcedKey(forced, juce::jmax(0, rootIndex - 1),
                           scaleForModeIndex(keyModeBox.getSelectedItemIndex()));
}

void HarmoniaEditor::refreshProgressionField()
{
    // Do not fight the user while they are typing.
    if (progressionField.hasKeyboardFocus(true))
        return;

    const auto current = processor.getProgressionText();
    if (current != progressionField.getText())
        progressionField.setText(current, juce::dontSendNotification);
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
        constexpr int controlsHeight = 3 * (14 + 26) + 2 * 8 + 12 + 24 + 4 + 24 + 12 + 14 + 24;
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
        labelRow = side.removeFromTop(14);
        comboRow = side.removeFromTop(26);
        keyRootLabel.setBounds(labelRow.removeFromLeft(half).reduced(2, 0));
        keyModeLabel.setBounds(labelRow.reduced(2, 0));
        keyRootBox.setBounds(comboRow.removeFromLeft(half).reduced(2, 0));
        keyModeBox.setBounds(comboRow.reduced(2, 0));

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
    area.removeFromBottom(6);
    {
        auto typeRow = area.removeFromBottom(28);
        presetBox.setBounds(typeRow.removeFromLeft(230).reduced(0, 1));
        typeRow.removeFromLeft(8);
        applyProgressionButton.setBounds(typeRow.removeFromRight(56).reduced(0, 1));
        typeRow.removeFromRight(6);
        progressionField.setBounds(typeRow.reduced(0, 1));
    }
    area.removeFromBottom(8);
    pianoRoll.setBounds(area);
}
