#include "PluginEditor.h"

#include <iterator>

using namespace ProgressionsColours;
using namespace harmonia;

namespace
{
const juce::StringArray kPartLabels { "Pad", "Chords", "Melody", "Counter", "Bass", "Reese", "Arp", "Pluck" };
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

juce::String scanRowText(const ProgressionsProcessor& processor)
{
    if (processor.libraryScanCounting())
        return "looking for MIDI files...";

    const int total = processor.libraryScanTotal();
    if (total <= 0)
        return "starting...";

    const int done = processor.libraryScanDone();
    return juce::String(done) + " / " + juce::String(total) + "   ("
               + juce::String(juce::roundToInt(100.0f * static_cast<float>(done)
                                               / static_cast<float>(total))) + "%)";
}

juce::String styleRowText(const ProgressionsProcessor& processor)
{
    const int clips = processor.styleClipCount();
    auto text = juce::String(clips) + (clips == 1 ? " clip" : " clips");
    const auto source = processor.styleSourceText();
    if (source.isNotEmpty())
        text += "  (" + source + ")";
    return text;
}
} // namespace

ProgressionsEditor::ProgressionsEditor(ProgressionsProcessor& p)
    : AudioProcessorEditor(&p), processor(p)
{
    setLookAndFeel(&lookAndFeel);

    titleLabel.setText("PROGRESSIONS", juce::dontSendNotification);
    titleLabel.setFont(juce::FontOptions(22.0f, juce::Font::bold));
    titleLabel.setColour(juce::Label::textColourId, accent);
    titleLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(titleLabel);

    // Discreet: the version sits beside the name, and the whole block is the
    // way into the about card. No extra button in the header for it.
    versionLabel.setText("v" JucePlugin_VersionString, juce::dontSendNotification);
    versionLabel.setFont(juce::FontOptions(10.0f));
    versionLabel.setColour(juce::Label::textColourId, textDim);
    versionLabel.setJustificationType(juce::Justification::bottomLeft);
    versionLabel.setInterceptsMouseClicks(false, false);
    addAndMakeVisible(versionLabel);

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

    initButton.setTooltip("Back to a fresh instance: no clip, no typed progression, every knob "
                          "at its default. Your learned library stays - it belongs to the plugin, "
                          "not to this patch");
    initButton.onClick = [this] { processor.initialise(); };
    addAndMakeVisible(initButton);

    aboutPanel.setVisible(false);
    addChildComponent(aboutPanel);

    loadButton.setTooltip("Load a MIDI clip to read the harmony from - a bass, a pad, a lead");
    clearButton.setTooltip("Throw away the loaded clip. A progression you typed stays, "
                           "and so does the library the plugin learned from");
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
    buildKnob(craftKnob, "Craft", ParamID::develop);
    buildKnob(styleKnob, "My style", ParamID::styleAmount);

    densityKnob.slider.setTooltip("How busy the generated part is");
    complexityKnob.slider.setTooltip("Chord extensions and chromatic movement");
    humanizeKnob.slider.setTooltip("Timing and velocity jitter");
    swingKnob.slider.setTooltip("Shuffle on the off-beat 16ths");
    octaveKnob.slider.setTooltip("Move the part up or down whole octaves");
    voicesKnob.slider.setTooltip("Maximum notes in a chord voicing");
    craftKnob.slider.setTooltip("How hard a melody works its motif: inversion, retrograde, "
                                "fragmentation and passing notes");
    styleKnob.slider.setTooltip("How much of the material learned from your own MIDI collection to use");

    buildKnob(reharmKnob, "Reharm", ParamID::reharmAmount);
    reharmKnob.slider.setTooltip("How much of the progression the Reharmonise button rewrites");

    // ---- Side panel controls -------------------------------------------------
    for (auto* label : { &lengthLabel, &arpLabel, &harmonyLabel, &levelLabel,
                         &meterLabel, &accidentalLabel })
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

    meterBox.addItemList({ "From clip", "4/4", "3/4", "6/8", "5/4", "7/8" }, 1);
    meterBox.setTooltip("Nothing guesses a meter - it is read from the clip you load, and is 4/4 "
                        "when there is no clip. Set it here to disagree with the file.");
    addAndMakeVisible(meterBox);
    meterAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParamID::meter, meterBox);

    accidentalBox.addItemList({ "Key", "Sharps  #", "Flats  b" }, 1);
    accidentalBox.setTooltip("How notes and chords are spelled. Key uses the accidentals the key "
                             "itself calls for; the other two force sharps or flats everywhere. "
                             "Either spelling can be typed in whichever this is set to.");
    addAndMakeVisible(accidentalBox);
    accidentalAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParamID::accidentals, accidentalBox);

    arpBox.addItemList({ "Up", "Down", "Up-Down", "Down-Up", "Converge", "Random" }, 1);
    addAndMakeVisible(arpBox);
    arpAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment>(
        processor.apvts, ParamID::arpPattern, arpBox);

    for (auto* toggle : { &followToggle, &avoidToggle, &syncToggle, &previewToggle, &stackToggle })
        addAndMakeVisible(toggle);

    stackToggle.setTooltip("Play every part you have generated together, each on its own MIDI "
                           "channel, instead of only the one selected. The pad on channel 1, "
                           "chords on 2, and so on - point a different instrument at each.");
    stackAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamID::stackParts, stackToggle);

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
    {
        // Both spellings, always, so the list reads the same whichever way the
        // switch is set and nobody has to hunt for Eb under D#.
        const juce::String sharp(harmonia::pitchClassName(pitchClass, false));
        const juce::String flat(harmonia::pitchClassName(pitchClass, true));
        keyRootBox.addItem(sharp == flat ? sharp : sharp + " / " + flat, pitchClass + 2);
    }
    keyRootBox.setTooltip("Pin the key instead of letting the detector pick it. "
                          "Roman numerals you type are read in this key.");
    keyRootBox.onChange = [this] { applyKeyFromControls(); };
    addAndMakeVisible(keyRootBox);

    keyModeBox.addItemList(kModeNames, 1);
    keyModeBox.setSelectedId(2, juce::dontSendNotification);
    keyModeBox.onChange = [this] { applyKeyFromControls(); };
    addAndMakeVisible(keyModeBox);

    // ---- The harmony block ------------------------------------------------------
    // Typing a progression is the way this gets used most, so it leads: a wide
    // field with its own label, and the preset list beside it as the offer it
    // is rather than a step you have to go through.
    harmonyHeading.setText("HARMONY", juce::dontSendNotification);
    harmonyHeading.setFont(juce::FontOptions(11.0f, juce::Font::bold));
    harmonyHeading.setColour(juce::Label::textColourId, accent);
    addAndMakeVisible(harmonyHeading);

    for (auto* hint : { &progressionHint, &presetHint })
    {
        hint->setFont(juce::FontOptions(11.0f, juce::Font::bold));
        hint->setColour(juce::Label::textColourId, textDim);
        addAndMakeVisible(hint);
    }

    presetBox.setTextWhenNothingSelected("Style presets (optional)");
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

    // ---- Your own collection ------------------------------------------------
    styleButton.setTooltip("The library the generators write from. Loading one keeps it inside the plugin, "
                           "so every new instance starts with it");
    styleButton.onClick = [this] { showStyleMenu(); };
    addAndMakeVisible(styleButton);

    styleToggle.setTooltip("Write bars, movements and voicings the way your own clips do");
    addAndMakeVisible(styleToggle);
    styleAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment>(
        processor.apvts, ParamID::useStyle, styleToggle);

    // ---- Actions -------------------------------------------------------------
    diceButton.setTooltip("Roll a new seed - same settings, a different idea");
    diceButton.onClick = [this] { processor.rollNewSeed(); };
    addAndMakeVisible(diceButton);

    reharmButton.setTooltip("Rewrite some of the chords into substitutions that still fit: the "
                            "relative minor or major, a secondary dominant leading to the next "
                            "chord, a tritone sub, a borrowed bVII or iv, a triad opened into a "
                            "seventh. The Reharm knob decides how much of the progression changes. "
                            "Undo puts it back.");
    reharmButton.onClick = [this] { processor.reharmonize(processor.reharmonizeAmount()); };
    addAndMakeVisible(reharmButton);

    undoButton.setTooltip("One step back: a knob, a chord you typed, a preset, a reharmonisation");
    undoButton.onClick = [this] { processor.undo(); };
    addAndMakeVisible(undoButton);

    dropPartButton.setTooltip("Stop keeping this part. The others carry on playing.");
    dropPartButton.onClick = [this] { processor.dropPart(processor.activePartIndex()); };
    addAndMakeVisible(dropPartButton);

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
    setSize(1060, 780);
    startTimerHz(30);
}

ProgressionsEditor::~ProgressionsEditor()
{
    processor.removeChangeListener(this);
    setLookAndFeel(nullptr);
}

void ProgressionsEditor::buildKnob(Knob& knob, const juce::String& name, const char* parameterID, bool rotary)
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

bool ProgressionsEditor::preferFlats() const
{
    const auto style = static_cast<harmonia::AccidentalStyle>(
        juce::jlimit(0, 2, juce::roundToInt(processor.apvts.getRawParameterValue(ParamID::accidentals)->load())));
    return harmonia::useFlatsFor(processor.getEngine().analysis().key, style);
}

void ProgressionsEditor::refreshPartButtons()
{
    for (int i = 0; i < partButtons.size(); ++i)
    {
        const bool live = processor.isPartLive(i);
        const bool stale = processor.isPartStale(i);

        // A dot for "this one is written and playing", an asterisk for "it was
        // written over a pad that is no longer there". The MIDI channel used to
        // be here too, but Ableton Live discards channel information when it
        // routes from a plug-in, so for a Live user the number is a piece of
        // furniture that never means anything. It stays in the tooltip, where
        // whoever is on a host that honours channels can still find it.
        // Built from the code point rather than a literal: a raw UTF-8 byte
        // string reaches juce::String as Latin-1 and the dot arrives as mojibake.
        static const juce::String bullet = juce::String::charToString(juce::juce_wchar(0x2022));

        auto text = kPartLabels[i];
        if (live)
            text += "  " + bullet + (stale ? "*" : "");
        if (partButtons[i]->getButtonText() != text)
            partButtons[i]->setButtonText(text);

        partButtons[i]->setTooltip(live
            ? juce::String("Written and playing")
                  + (stale ? ", over a different pad than the one playing now" : "")
                  + ".  MIDI channel " + juce::String(processor.channelForPart(i))
                  + " - useful in a host that keeps channels; Live does not."
            : "Write a " + kPartLabels[i].toLowerCase() + " over the harmony");
    }
}

void ProgressionsEditor::refresh()
{
    const auto& analysis = processor.getEngine().analysis();
    const bool hasSource = processor.getEngine().hasSource();

    pianoRoll.setPreferFlats(preferFlats());
    progressionStrip.setPreferFlats(preferFlats());
    pianoRoll.setContent(processor.getEngine().source(), processor.getGeneratedSequence(), analysis);
    progressionStrip.setAnalysis(analysis);
    refreshPartButtons();

    sourceLabel.setText(hasSource ? processor.getSourceName() : juce::String("no clip loaded"),
                        juce::dontSendNotification);
    statusLabel.setText(processor.getLastError(), juce::dontSendNotification);

    std::vector<std::pair<juce::String, juce::String>> rows;
    if (hasSource && analysis.valid)
    {
        rows.emplace_back("Key", juce::String(analysis.key.name(preferFlats())) + "  ("
                                     + juce::String(juce::roundToInt(analysis.keyConfidence * 100.0f)) + "%)");
        rows.emplace_back("Tempo", formatBpm(analysis.bpm) + "  " + juce::String(analysis.timeSignature.numerator)
                                       + "/" + juce::String(analysis.timeSignature.denominator));
        rows.emplace_back("Length", juce::String(analysis.bars) + " bars");
        rows.emplace_back("Clip reads as", juce::String(toString(analysis.role)));
        rows.emplace_back("Register", juce::String(noteName(analysis.rhythm.lowestPitch, preferFlats())) + " - "
                                          + juce::String(noteName(analysis.rhythm.highestPitch, preferFlats())));
        rows.emplace_back("Density", juce::String(analysis.rhythm.notesPerBar, 1) + " notes/bar");
        rows.emplace_back("Seed", juce::String(static_cast<juce::int64>(processor.getSeed())));
        rows.emplace_back("Generated", juce::String(static_cast<int>(processor.getGeneratedSequence().notes.size()))
                                           + " notes");
        if (processor.hasStyleModel())
            rows.emplace_back("My style", styleRowText(processor));
    }
    else
    {
        rows.emplace_back("Status", "waiting for a clip");
        if (processor.hasStyleModel())
            rows.emplace_back("My style", styleRowText(processor));
    }

    if (processor.isScanningLibrary())
    {
        rows.emplace_back("Scanning", processor.libraryScanFolder().getFileName());
        rows.emplace_back("Progress", scanRowText(processor));
    }
    else if (processor.lastScanSummary().isNotEmpty())
    {
        rows.emplace_back("Learned", processor.lastScanSummary());
    }
    infoPanel.setRows(std::move(rows));

    // Named rather than counted: adding a part shifts every index after it, and
    // the last time these were positions the Arp controls followed the wrong tab.
    const auto part = static_cast<harmonia::PartType>(
        juce::jlimit(0, kNumParts - 1,
                     juce::roundToInt(processor.apvts.getRawParameterValue(ParamID::part)->load())));

    const bool chordPart = part == PartType::Pad || part == PartType::Chords || part == PartType::Arp;
    voicesKnob.slider.setEnabled(chordPart);
    voicesKnob.label.setEnabled(chordPart);
    const bool melodicPart = part == PartType::Melody || part == PartType::CounterMelody;
    craftKnob.slider.setEnabled(melodicPart);
    craftKnob.label.setEnabled(melodicPart);
    arpBox.setEnabled(part == PartType::Arp);
    arpLabel.setEnabled(part == PartType::Arp);

    const bool hasOutput = ! processor.getGeneratedSequence().empty();
    exportButton.setEnabled(hasOutput);
    dragArea.setEnabledState(hasOutput);
    playButton.setEnabled(hasOutput);
    clearButton.setEnabled(hasSource);

    pianoRoll.setPlaceholder(hasSource ? "Nothing generated for these settings"
                                       : "Drop a MIDI clip here, or just type a progression below");

    refreshProgressionField();

    // The preset box is the only control that is not a parameter, so nothing
    // resets it for us. When no written progression is in force - after Init,
    // or after Reset chords - it must stop claiming one is.
    if (! processor.hasWrittenProgression() && presetBox.getSelectedId() != 0)
        presetBox.setSelectedId(0, juce::dontSendNotification);

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

    const bool scanning = processor.isScanningLibrary();
    styleToggle.setEnabled(processor.hasStyleModel() && ! scanning);
    styleKnob.slider.setEnabled(processor.hasStyleModel() && ! scanning);
    styleKnob.label.setEnabled(processor.hasStyleModel() && ! scanning);
    styleButton.setButtonText(scanning ? "Stop scanning"
                                       : (processor.hasStyleModel() ? "Change my library..."
                                                                    : "Learn from my library..."));

    resetChordsButton.setEnabled(hasSource || processor.hasWrittenProgression());
    diceButton.setEnabled(hasSource || analysis.valid);
    reharmButton.setEnabled(analysis.valid);
    reharmKnob.slider.setEnabled(analysis.valid);
    reharmKnob.label.setEnabled(analysis.valid);
    undoButton.setEnabled(processor.canUndo());
    dropPartButton.setEnabled(processor.isPartLive(processor.activePartIndex()));
}

void ProgressionsEditor::applyTypedProgression()
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

void ProgressionsEditor::applyKeyFromControls()
{
    const int rootIndex = keyRootBox.getSelectedItemIndex(); // 0 = Auto
    const bool forced = rootIndex > 0;
    keyModeBox.setEnabled(forced);
    keyModeLabel.setEnabled(forced);

    processor.setForcedKey(forced, juce::jmax(0, rootIndex - 1),
                           scaleForModeIndex(keyModeBox.getSelectedItemIndex()));
}

void ProgressionsEditor::refreshProgressionField()
{
    // Do not fight the user while they are typing.
    if (progressionField.hasKeyboardFocus(true))
        return;

    const auto current = processor.getProgressionText();
    if (current != progressionField.getText())
        progressionField.setText(current, juce::dontSendNotification);
}

void ProgressionsEditor::changeListenerCallback(juce::ChangeBroadcaster*)
{
    refresh();
}

void ProgressionsEditor::timerCallback()
{
    pianoRoll.setPlayPosition(processor.getPlayPositionNormalised());

    // A scan takes minutes; the counter has to move or it looks hung.
    if (processor.isScanningLibrary())
        refresh();

    if (! processor.isPlayingInternally() && playButton.getToggleState())
    {
        playButton.setToggleState(false, juce::dontSendNotification);
        playButton.setButtonText("Play");
    }
}

// ---------------------------------------------------------------------------

void ProgressionsEditor::showLoadDialog()
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

void ProgressionsEditor::mouseDown(const juce::MouseEvent& event)
{
    if (aboutHotspot.contains(event.getPosition()))
    {
        aboutPanel.setVisible(true);
        aboutPanel.toFront(false);
    }
}

void ProgressionsEditor::showStyleMenu()
{
    if (processor.isScanningLibrary())
    {
        processor.cancelLibraryScan();
        return;
    }

    juce::PopupMenu menu;

    if (processor.hasStyleModel())
        menu.addSectionHeader(juce::String(processor.styleClipCount()) + " clips, "
                                  + processor.styleSourceText());

    menu.addItem(1, "Scan a MIDI folder...");

    // The folders a producer's collection actually lives in, so the common
    // case is one click rather than a trip through a file browser.
    const auto suggestions = ProgressionsProcessor::suggestedLibraryFolders();
    if (! suggestions.isEmpty())
    {
        juce::PopupMenu quick;
        for (int i = 0; i < suggestions.size(); ++i)
            quick.addItem(100 + i, suggestions[i].getFullPathName());
        menu.addSubMenu("Scan a folder I already have", quick);
    }

    menu.addSeparator();
    menu.addItem(2, "Load a .style.json...");
    menu.addItem(3, "Forget this library",
                 processor.hasStyleModel()
                     && processor.styleSource() != ProgressionsProcessor::StyleSource::BuiltIn);

    menu.showMenuAsync(juce::PopupMenu::Options().withTargetComponent(styleButton),
                       [this, suggestions](int choice)
                       {
                           if (choice == 1)
                               showScanDialog();
                           else if (choice == 2)
                               showStyleDialog();
                           else if (choice == 3)
                               processor.forgetInstalledStyleModel();
                           else if (choice >= 100 && choice - 100 < suggestions.size())
                               processor.startLibraryScan(suggestions[choice - 100]);
                       });
}

void ProgressionsEditor::showScanDialog()
{
    chooser = std::make_unique<juce::FileChooser>("Choose the folder your MIDI lives in",
                                                  juce::File::getSpecialLocation(juce::File::userMusicDirectory),
                                                  juce::String());
    chooser->launchAsync(juce::FileBrowserComponent::openMode
                             | juce::FileBrowserComponent::canSelectDirectories,
                         [this](const juce::FileChooser& fc)
                         {
                             const auto folder = fc.getResult();
                             if (folder.isDirectory())
                                 processor.startLibraryScan(folder);
                         });
}

void ProgressionsEditor::showStyleDialog()
{
    chooser = std::make_unique<juce::FileChooser>("Choose a style model built by 'harmonia-cli learn'",
                                                  processor.styleModelFile().existsAsFile()
                                                      ? processor.styleModelFile()
                                                      : juce::File::getSpecialLocation(juce::File::userHomeDirectory),
                                                  "*.style.json;*.json");
    chooser->launchAsync(juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                         [this](const juce::FileChooser& fc)
                         {
                             const auto file = fc.getResult();
                             if (file.existsAsFile())
                                 processor.loadStyleModelFile(file);
                         });
}

void ProgressionsEditor::showExportDialog()
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

bool ProgressionsEditor::isInterestedInFileDrag(const juce::StringArray& files)
{
    for (const auto& path : files)
        if (path.endsWithIgnoreCase(".mid") || path.endsWithIgnoreCase(".midi"))
            return true;
    return false;
}

void ProgressionsEditor::fileDragEnter(const juce::StringArray&, int, int)
{
    fileDragActive = true;
    repaint();
}

void ProgressionsEditor::fileDragExit(const juce::StringArray&)
{
    fileDragActive = false;
    repaint();
}

void ProgressionsEditor::filesDropped(const juce::StringArray& files, int, int)
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

void ProgressionsEditor::paint(juce::Graphics& g)
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

void ProgressionsEditor::resized()
{
    aboutPanel.setBounds(getLocalBounds());

    auto area = getLocalBounds().reduced(12);

    // ---- Header --------------------------------------------------------------
    auto header = area.removeFromTop(52);
    {
        auto buttons = header.removeFromRight(272);
        buttons.removeFromTop(8);
        clearButton.setBounds(buttons.removeFromRight(84).reduced(2, 6));
        loadButton.setBounds(buttons.removeFromRight(120).reduced(2, 6));
        initButton.setBounds(buttons.removeFromRight(58).reduced(2, 6));

        auto titleArea = header.removeFromLeft(250);
        aboutHotspot = titleArea;
        auto titleRow = titleArea.removeFromTop(30);
        titleLabel.setBounds(titleRow.removeFromLeft(178));
        versionLabel.setBounds(titleRow.withTrimmedBottom(6));
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
        Knob* knobs[] = { &densityKnob, &complexityKnob, &craftKnob, &humanizeKnob,
                          &swingKnob, &octaveKnob, &voicesKnob, &reharmKnob, &styleKnob };
        const int knobWidth = knobRow.getWidth() / static_cast<int>(std::size(knobs));
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
        juce::TextButton* actions[] = { &diceButton, &playButton, &undoButton, &reharmButton,
                                        &resetChordsButton, &dropPartButton, &exportButton };
        const int actionWidth = actionRow.getWidth() / static_cast<int>(std::size(actions));
        for (auto* button : actions)
            button->setBounds(actionRow.removeFromLeft(actionWidth).reduced(3, 4));
    }

    // ---- Side panel ----------------------------------------------------------
    auto side = area.removeFromLeft(268);
    area.removeFromLeft(12);
    {
        // Everything below the read-out is fixed height; the read-out takes the rest.
        // Key, Length and the spelling switch live with the typed chords now,
        // so what is left here is what shapes the part rather than the harmony.
        constexpr int controlsHeight = 2 * (14 + 26) + 8 + 12 + 3 * (24 + 4)
                                     + 12 + 14 + 24 + 10 + 26 + 4 + 24;
        infoPanel.setBounds(side.removeFromTop(juce::jmax(150, side.getHeight() - controlsHeight - 10)));
        side.removeFromTop(10);

        const int half = side.getWidth() / 2;
        auto labelRow = side.removeFromTop(14);
        auto comboRow = side.removeFromTop(26);
        harmonyLabel.setBounds(labelRow.removeFromLeft(half).reduced(2, 0));
        arpLabel.setBounds(labelRow.reduced(2, 0));
        harmonyBox.setBounds(comboRow.removeFromLeft(half).reduced(2, 0));
        arpBox.setBounds(comboRow.reduced(2, 0));

        side.removeFromTop(12);
        auto toggleRow = side.removeFromTop(24);
        followToggle.setBounds(toggleRow.removeFromLeft(half).reduced(2, 0));
        syncToggle.setBounds(toggleRow.reduced(2, 0));

        side.removeFromTop(4);
        toggleRow = side.removeFromTop(24);
        avoidToggle.setBounds(toggleRow.removeFromLeft(half).reduced(2, 0));
        previewToggle.setBounds(toggleRow.reduced(2, 0));

        side.removeFromTop(4);
        stackToggle.setBounds(side.removeFromTop(24).reduced(2, 0));

        side.removeFromTop(12);
        levelLabel.setBounds(side.removeFromTop(14));
        levelSlider.setBounds(side.removeFromTop(24));

        side.removeFromTop(10);
        styleButton.setBounds(side.removeFromTop(26).reduced(2, 0));
        side.removeFromTop(4);
        styleToggle.setBounds(side.removeFromTop(24).reduced(2, 0));
    }

    // ---- Main view -----------------------------------------------------------
    progressionStrip.setBounds(area.removeFromBottom(54));
    area.removeFromBottom(6);

    // ---- Harmony block --------------------------------------------------------
    // One place for everything that decides the chords: what you type, what key
    // they are read in, how long the loop is, how they are spelled.
    {
        // 14 heading + 4 + 14 labels + 28 field + 8 + 14 labels + 26 boxes.
        auto block = area.removeFromBottom(108);

        auto settingsRow = block.removeFromBottom(26);
        auto settingsLabels = block.removeFromBottom(14);
        block.removeFromBottom(8);

        // Key, Mode, Length, Time, Spelling - five cells across the block.
        const int cell = settingsRow.getWidth() / 5;
        juce::Label* settingLabels[] = { &keyRootLabel, &keyModeLabel, &lengthLabel,
                                         &meterLabel, &accidentalLabel };
        juce::ComboBox* settingBoxes[] = { &keyRootBox, &keyModeBox, &lengthBox,
                                           &meterBox, &accidentalBox };
        for (int i = 0; i < 5; ++i)
        {
            const bool last = i == 4;
            auto labelCell = last ? settingsLabels : settingsLabels.removeFromLeft(cell);
            auto boxCell = last ? settingsRow : settingsRow.removeFromLeft(cell);
            settingLabels[i]->setBounds(labelCell.reduced(2, 0));
            settingBoxes[i]->setBounds(boxCell.reduced(2, 0));
        }

        auto typeRow = block.removeFromBottom(28);
        auto typeLabels = block.removeFromBottom(14);
        block.removeFromBottom(4);
        harmonyHeading.setBounds(block.removeFromBottom(14).reduced(2, 0));

        // The typed field leads and the presets sit beside it - the proportions
        // say which one is the main way in.
        auto presetCell = typeRow.removeFromRight(220);
        auto presetLabelCell = typeLabels.removeFromRight(220);
        typeRow.removeFromRight(10);
        typeLabels.removeFromRight(10);

        presetHint.setBounds(presetLabelCell.reduced(2, 0));
        presetBox.setBounds(presetCell.reduced(2, 1));

        progressionHint.setBounds(typeLabels.reduced(2, 0));
        applyProgressionButton.setBounds(typeRow.removeFromRight(56).reduced(2, 1));
        typeRow.removeFromRight(4);
        progressionField.setBounds(typeRow.reduced(2, 1));
    }

    area.removeFromBottom(8);
    pianoRoll.setBounds(area);
}
