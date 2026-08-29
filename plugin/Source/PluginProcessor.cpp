#include "PluginProcessor.h"
#include "PluginEditor.h"

#include "StyleStore.h"

#include "harmonia/MidiFile.h"
#include "harmonia/StyleModel.h"

using namespace harmonia;

namespace
{

const juce::StringArray kPartNames { "Pad", "Chords", "Melody", "Counter Melody", "Bass", "Arp", "Pluck" };
const juce::StringArray kBarChoices { "As source", "1", "2", "4", "8", "16" };
const juce::StringArray kArpChoices { "Up", "Down", "Up-Down", "Down-Up", "Converge", "Random" };
const juce::StringArray kHarmonyChoices { "Auto", "1 per bar", "2 per bar", "1 per beat" };
const juce::StringArray kAccidentalChoices { "Key", "Sharps", "Flats" };
const juce::StringArray kMeterChoices { "From clip", "4/4", "3/4", "6/8", "5/4", "7/8" };

/** The time signature behind each entry of kMeterChoices; index 0 is unused. */
harmonia::TimeSignature meterForChoice(int choice)
{
    switch (choice)
    {
        case 2:  return { 3, 4, 0 };
        case 3:  return { 6, 8, 0 };
        case 4:  return { 5, 4, 0 };
        case 5:  return { 7, 8, 0 };
        default: return { 4, 4, 0 };
    }
}

/** How many chords the detector is allowed to find per bar. 0 lets it decide. */
int chordsPerBarForChoice(int index)
{
    switch (index)
    {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        default: return 0;
    }
}

juce::String percentText(float value, int)
{
    return juce::String(juce::roundToInt(value * 100.0f)) + "%";
}

juce::String octaveText(int value, int)
{
    return value > 0 ? "+" + juce::String(value) : juce::String(value);
}

int barsForChoice(int index)
{
    switch (index)
    {
        case 1: return 1;
        case 2: return 2;
        case 3: return 4;
        case 4: return 8;
        case 5: return 16;
        default: return 0; // follow the source clip
    }
}

} // namespace

// ---------------------------------------------------------------------------

juce::AudioProcessorValueTreeState::ParameterLayout ProgressionsProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { ParamID::part, 1 }, "Part", kPartNames, 0));
    const auto percentAttributes = AudioParameterFloatAttributes().withStringFromValueFunction(percentText);

    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::density, 1 }, "Density",
                                                     NormalisableRange<float> { 0.0f, 1.0f }, 0.5f, percentAttributes));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::complexity, 1 }, "Complexity",
                                                     NormalisableRange<float> { 0.0f, 1.0f }, 0.5f, percentAttributes));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::humanize, 1 }, "Humanize",
                                                     NormalisableRange<float> { 0.0f, 1.0f }, 0.15f, percentAttributes));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::swing, 1 }, "Swing",
                                                     NormalisableRange<float> { 0.0f, 1.0f }, 0.0f, percentAttributes));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::develop, 1 }, "Craft",
                                                     NormalisableRange<float> { 0.0f, 1.0f }, 0.4f, percentAttributes));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID { ParamID::octave, 1 }, "Octave", -2, 2, 0,
                                                   AudioParameterIntAttributes().withStringFromValueFunction(octaveText)));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID { ParamID::voices, 1 }, "Voices", 2, 6, 4));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { ParamID::bars, 1 }, "Length", kBarChoices, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { ParamID::follow, 1 }, "Follow Source Groove", true));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { ParamID::avoid, 1 }, "Stay Clear Of Source", true));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { ParamID::arpPattern, 1 }, "Arp Pattern", kArpChoices, 2));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { ParamID::preview, 1 }, "Preview Sound", true));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::level, 1 }, "Preview Level",
                                                     NormalisableRange<float> { -40.0f, 6.0f }, -6.0f,
                                                     AudioParameterFloatAttributes()
                                                         .withStringFromValueFunction([](float value, int)
                                                         {
                                                             return juce::String(value, 1) + " dB";
                                                         })));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { ParamID::hostSync, 1 }, "Sync To Host", true));
    layout.add(std::make_unique<AudioParameterInt>(ParameterID { ParamID::midiChannel, 1 }, "MIDI Channel", 1, 16, 1));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { ParamID::harmonicRhythm, 1 }, "Harmonic Rhythm",
                                                      kHarmonyChoices, 0));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { ParamID::useStyle, 1 }, "Use My Style", true));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::styleAmount, 1 }, "Style Amount",
                                                     NormalisableRange<float> { 0.0f, 1.0f }, 1.0f, percentAttributes));
    layout.add(std::make_unique<AudioParameterBool>(ParameterID { ParamID::stackParts, 1 }, "Stack Parts", true));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { ParamID::accidentals, 1 }, "Accidentals",
                                                      kAccidentalChoices, 0));
    layout.add(std::make_unique<AudioParameterChoice>(ParameterID { ParamID::meter, 1 }, "Time Signature",
                                                      kMeterChoices, 0));
    layout.add(std::make_unique<AudioParameterFloat>(ParameterID { ParamID::reharmAmount, 1 }, "Reharmonise Amount",
                                                     NormalisableRange<float> { 0.0f, 1.0f }, 0.5f, percentAttributes));

    return layout;
}

ProgressionsProcessor::ProgressionsProcessor()
    : AudioProcessor(BusesProperties().withOutput("Output", juce::AudioChannelSet::stereo(), true)),
      apvts(*this, nullptr, "HARMONIA", createLayout())
{
    for (const char* id : { ParamID::part, ParamID::density, ParamID::complexity, ParamID::humanize,
                            ParamID::swing, ParamID::develop, ParamID::octave, ParamID::voices, ParamID::bars,
                            ParamID::follow, ParamID::avoid, ParamID::arpPattern,
                            ParamID::harmonicRhythm, ParamID::useStyle, ParamID::styleAmount,
                            ParamID::stackParts, ParamID::accidentals, ParamID::meter })
        apvts.addParameterListener(id, this);

    previewSynth.addSound(new PreviewSound());
    for (int i = 0; i < 16; ++i)
        previewSynth.addVoice(new PreviewVoice(previewCharacters));

    seed = static_cast<juce::uint32>(juce::Random::getSystemRandom().nextInt(1000000) + 1);

    loadDefaultStyleModel();

    // The first change has to be undoable too, so there is a state to go back
    // to before anything has happened.
    captureSettledState();
}

ProgressionsProcessor::~ProgressionsProcessor()
{
    for (const char* id : { ParamID::part, ParamID::density, ParamID::complexity, ParamID::humanize,
                            ParamID::swing, ParamID::develop, ParamID::octave, ParamID::voices, ParamID::bars,
                            ParamID::follow, ParamID::avoid, ParamID::arpPattern,
                            ParamID::harmonicRhythm, ParamID::useStyle, ParamID::styleAmount })
        apvts.removeParameterListener(id, this);

    if (dragFile.existsAsFile())
        dragFile.deleteFile();
}

// ---------------------------------------------------------------------------

void ProgressionsProcessor::prepareToPlay(double sampleRate, int samplesPerBlock)
{
    currentSampleRate = sampleRate;
    previewSynth.setCurrentPlaybackSampleRate(sampleRate);
    scratchMidi.ensureSize(static_cast<size_t>(samplesPerBlock) * 4);
    internalPPQ = 0.0;
}

void ProgressionsProcessor::releaseResources()
{
    previewSynth.allNotesOff(0, false);
}

bool ProgressionsProcessor::isBusesLayoutSupported(const BusesLayout& layouts) const
{
    const auto& main = layouts.getMainOutputChannelSet();
    return main == juce::AudioChannelSet::mono() || main == juce::AudioChannelSet::stereo();
}

void ProgressionsProcessor::panic(juce::MidiBuffer& midi)
{
    // Parts sit on a channel each now, and the part being replaced is not
    // necessarily the one that was sounding, so silence all sixteen. Sixteen
    // controller messages once per part change costs nothing.
    for (int channel = 1; channel <= 16; ++channel)
        midi.addEvent(juce::MidiMessage::allNotesOff(channel), 0);
    previewSynth.allNotesOff(0, true);
}

void ProgressionsProcessor::renderEvents(juce::MidiBuffer& midi, int numSamples, double bpm, double startPPQ)
{
    RenderedPart::Ptr part;
    {
        const juce::SpinLock::ScopedTryLockType lock(partLock);
        if (! lock.isLocked())
            return;
        part = activePart;
    }

    if (part == nullptr || part->events.empty() || part->lengthPPQ <= 0.0)
        return;

    const double ppqPerSample = bpm / 60.0 / currentSampleRate;
    if (ppqPerSample <= 0.0)
        return;

    const double blockLengthPPQ = ppqPerSample * numSamples;
    const double loopLength = part->lengthPPQ;

    double cursor = std::fmod(startPPQ, loopLength);
    if (cursor < 0.0)
        cursor += loopLength;

    double consumed = 0.0;
    while (consumed < blockLengthPPQ)
    {
        const double chunk = std::min(loopLength - cursor, blockLengthPPQ - consumed);

        for (const auto& event : part->events)
        {
            if (event.ppq < cursor)
                continue;
            if (event.ppq >= cursor + chunk)
                break;

            const auto offset = juce::jlimit(0, numSamples - 1,
                                             static_cast<int>((consumed + event.ppq - cursor) / ppqPerSample));
            const int channel = juce::jlimit(1, 16, event.channel);
            const auto message = event.noteOn
                                     ? juce::MidiMessage::noteOn(channel, event.pitch,
                                                                 static_cast<juce::uint8>(event.velocity))
                                     : juce::MidiMessage::noteOff(channel, event.pitch);
            midi.addEvent(message, offset);
        }

        consumed += chunk;
        cursor = 0.0;
    }
}

void ProgressionsProcessor::processBlock(juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();
    if (numSamples == 0)
        return;

    double bpm = clipBpm.load();
    double startPPQ = internalPPQ;
    bool playing = internalPlaying.load();

    const bool followHost = apvts.getRawParameterValue(ParamID::hostSync)->load() > 0.5f;

    if (auto* playHead = getPlayHead())
    {
        if (const auto position = playHead->getPosition())
        {
            if (const auto tempo = position->getBpm())
            {
                bpm = *tempo;
                hostBpm.store(*tempo);
            }

            if (followHost && position->getIsPlaying())
            {
                playing = true;
                if (const auto ppq = position->getPpqPosition())
                    startPPQ = *ppq;
            }
            else if (followHost && ! position->getIsPlaying() && ! internalPlaying.load())
            {
                playing = false;
            }
        }
    }

    scratchMidi.clear();

    if (pendingPanic.exchange(false))
        panic(scratchMidi);

    if (playing)
    {
        renderEvents(scratchMidi, numSamples, bpm, startPPQ);
    }
    else if (wasPlaying)
    {
        panic(scratchMidi);
    }
    wasPlaying = playing;

    // Advance the internal transport even when the host is driving, so the UI
    // playhead keeps working in either mode.
    const double blockPPQ = bpm / 60.0 / currentSampleRate * numSamples;
    if (playing)
        internalPPQ = startPPQ + blockPPQ;

    {
        const juce::SpinLock::ScopedTryLockType lock(partLock);
        if (lock.isLocked() && activePart != nullptr && activePart->lengthPPQ > 0.0)
        {
            double local = std::fmod(internalPPQ, activePart->lengthPPQ);
            if (local < 0.0)
                local += activePart->lengthPPQ;
            reportedPosition.store(playing ? local / activePart->lengthPPQ : 0.0);
        }
    }

    if (apvts.getRawParameterValue(ParamID::preview)->load() > 0.5f)
    {
        previewSynth.renderNextBlock(buffer, scratchMidi, 0, numSamples);
        const auto gain = juce::Decibels::decibelsToGain(apvts.getRawParameterValue(ParamID::level)->load(), -40.0f);
        buffer.applyGain(gain);
    }

    // Generated events go out of the MIDI port alongside whatever came in.
    midiMessages.addEvents(scratchMidi, 0, numSamples, 0);
}

// ---------------------------------------------------------------------------

namespace
{
/** A cheap content hash of a part, used to tell whether the pad under the other
    parts is still the one they were written over. */
juce::uint32 fingerprint(const harmonia::NoteSequence& sequence)
{
    juce::uint32 hash = 2166136261u;
    const auto mix = [&hash](juce::uint32 value)
    {
        hash = (hash ^ value) * 16777619u;
    };

    for (const auto& note : sequence.notes)
    {
        mix(static_cast<juce::uint32>(note.startTick));
        mix(static_cast<juce::uint32>(note.lengthTick));
        mix(static_cast<juce::uint32>(note.pitch));
        mix(static_cast<juce::uint32>(note.velocity));
    }
    return hash == 0 ? 1u : hash;
}
} // namespace

int ProgressionsProcessor::activePartIndex() const
{
    return juce::jlimit(0, kNumParts - 1,
                        static_cast<int>(apvts.getRawParameterValue(ParamID::part)->load()));
}

bool ProgressionsProcessor::isPartLive(int partIndex) const
{
    return partIndex >= 0 && partIndex < kNumParts && partLive[static_cast<size_t>(partIndex)];
}

bool ProgressionsProcessor::isPartStale(int partIndex) const
{
    if (! isPartLive(partIndex) || partIndex == 0)
        return false;
    // Only meaningful while there is a pad to be out of step with.
    return partLive[0] && partPadStamp[static_cast<size_t>(partIndex)] != padStamp;
}

int ProgressionsProcessor::channelForPart(int partIndex) const
{
    const int base = juce::jlimit(1, 16, static_cast<int>(apvts.getRawParameterValue(ParamID::midiChannel)->load()));
    return ((base - 1 + juce::jlimit(0, kNumParts - 1, partIndex)) % 16) + 1;
}

const harmonia::NoteSequence& ProgressionsProcessor::partSequence(int partIndex) const
{
    return partSequences[static_cast<size_t>(juce::jlimit(0, kNumParts - 1, partIndex))];
}

void ProgressionsProcessor::dropPart(int partIndex)
{
    if (partIndex < 0 || partIndex >= kNumParts)
        return;
    pushUndoState(true);
    partLive[static_cast<size_t>(partIndex)] = false;
    partSequences[static_cast<size_t>(partIndex)] = harmonia::NoteSequence {};
    if (partIndex == activePartIndex())
        generated = harmonia::NoteSequence {};
    republishParts();
    captureSettledState();
    sendChangeMessage();
}

void ProgressionsProcessor::dropAllParts()
{
    partLive.fill(false);
    for (auto& sequence : partSequences)
        sequence = harmonia::NoteSequence {};
    generated = harmonia::NoteSequence {};
}

harmonia::GenerateOptions ProgressionsProcessor::optionsForPart(int partIndex) const
{
    auto options = currentOptions();
    options.part = static_cast<PartType>(juce::jlimit(0, kNumParts - 1, partIndex));
    options.channel = 0;

    // The pad is what everything else is written around. It never anchors to
    // itself, and there is nothing to anchor to until one has been generated.
    if (partIndex != 0 && partLive[0] && ! partSequences[0].empty())
        options.anchor = &partSequences[0];

    return options;
}

void ProgressionsProcessor::republishParts()
{
    const bool stack = apvts.getRawParameterValue(ParamID::stackParts)->load() > 0.5f;
    const int active = activePartIndex();

    RenderedPart::Ptr mix = new RenderedPart();
    mix->lengthPPQ = 0.0;
    bool any = false;

    for (int part = 0; part < kNumParts; ++part)
    {
        if (! partLive[static_cast<size_t>(part)] || partSequences[static_cast<size_t>(part)].empty())
            continue;
        if (! stack && part != active)
            continue;

        mix->add(partSequences[static_cast<size_t>(part)], channelForPart(part));
        any = true;
    }

    if (! any)
    {
        publish(nullptr);
        return;
    }

    mix->sortEvents();

    // Tell the preview voices which part each channel is carrying, so the
    // built-in sound is at least the right shape for what it is playing.
    for (auto& entry : previewCharacters)
        entry.store(0, std::memory_order_relaxed);
    for (int part = 0; part < kNumParts; ++part)
        previewCharacters[static_cast<size_t>(channelForPart(part))].store(part, std::memory_order_relaxed);

    publish(mix);
}

harmonia::GenerateOptions ProgressionsProcessor::currentOptions() const
{
    GenerateOptions options;
    options.part = static_cast<PartType>(activePartIndex());
    options.seed = seed;
    options.density = apvts.getRawParameterValue(ParamID::density)->load();
    options.complexity = apvts.getRawParameterValue(ParamID::complexity)->load();
    options.humanize = apvts.getRawParameterValue(ParamID::humanize)->load();
    options.motifDevelopment = apvts.getRawParameterValue(ParamID::develop)->load();
    options.swing = apvts.getRawParameterValue(ParamID::swing)->load();
    options.octaveShift = static_cast<int>(apvts.getRawParameterValue(ParamID::octave)->load());
    options.maxVoices = static_cast<int>(apvts.getRawParameterValue(ParamID::voices)->load());
    options.bars = barsForChoice(static_cast<int>(apvts.getRawParameterValue(ParamID::bars)->load()));
    options.followSourceRhythm = apvts.getRawParameterValue(ParamID::follow)->load() > 0.5f;
    options.avoidSourceCollisions = apvts.getRawParameterValue(ParamID::avoid)->load() > 0.5f;
    options.arpPattern = static_cast<ArpPattern>(juce::jlimit(0, 5, static_cast<int>(apvts.getRawParameterValue(ParamID::arpPattern)->load())));
    options.channel = 0;

    if (! styleModel.empty() && apvts.getRawParameterValue(ParamID::useStyle)->load() > 0.5f)
    {
        options.style = &styleModel;
        options.styleAmount = apvts.getRawParameterValue(ParamID::styleAmount)->load();
    }
    return options;
}

void ProgressionsProcessor::publish(RenderedPart::Ptr part)
{
    {
        const juce::SpinLock::ScopedLockType lock(partLock);
        activePart = std::move(part);
    }

    // Whatever the old part was holding will never get its note-off, so the
    // audio thread has to let go of it before the new part starts.
    pendingPanic.store(true);
}

void ProgressionsProcessor::applyAnalysisOptions()
{
    const int wanted = chordsPerBarForChoice(
        static_cast<int>(apvts.getRawParameterValue(ParamID::harmonicRhythm)->load()));
    const int meterChoice = juce::jlimit(0, kMeterChoices.size() - 1,
                                         static_cast<int>(apvts.getRawParameterValue(ParamID::meter)->load()));
    const bool forceMeter = meterChoice > 0;
    const auto meter = meterForChoice(meterChoice);

    auto options = engine.analysisOptions();
    const bool meterChanged = options.forceTimeSignature != forceMeter
                           || (forceMeter && (options.timeSignature.numerator != meter.numerator
                                              || options.timeSignature.denominator != meter.denominator));

    if (wanted == options.chordsPerBar && ! meterChanged)
        return;

    // Re-running the analysis drops any hand-edited or reharmonised chords,
    // which is what you would expect from changing the harmonic grid.
    options.chordsPerBar = wanted;
    options.forceTimeSignature = forceMeter;
    options.timeSignature = meter;
    engine.setAnalysisOptions(options);
}

void ProgressionsProcessor::regenerate()
{
    applyAnalysisOptions();

    // With no clip to take a tempo from, follow the host - and the meter the
    // user asked for, since there is no file to read one from.
    if (! engine.hasSource())
    {
        const int meterChoice = juce::jlimit(0, kMeterChoices.size() - 1,
                                             static_cast<int>(apvts.getRawParameterValue(ParamID::meter)->load()));
        engine.setBlankCanvas(hostBpm.load(), 1, meterForChoice(meterChoice));
    }

    // A written progression is enough on its own - no clip required.
    if (! engine.analysis().valid)
    {
        dropAllParts();
        publish(nullptr);
        sendChangeMessage();
        return;
    }

    const int active = activePartIndex();
    generated = engine.generate(optionsForPart(active));
    partSequences[static_cast<size_t>(active)] = generated;
    partLive[static_cast<size_t>(active)] = ! generated.empty();

    // A regenerated pad is a different pad: the parts written over the old one
    // are now out of step. They are left exactly as they are - losing a melody
    // you had just got right because you nudged a chord would be worse - and
    // simply marked, so the change is visible instead of only audible.
    // Stamped from the notes rather than the seed: turning a knob rewrites the
    // pad without touching the seed, and that counts just as much.
    if (active == 0)
        padStamp = fingerprint(partSequences[0]);
    partPadStamp[static_cast<size_t>(active)] = padStamp;

    clipBpm.store(juce::jlimit(20.0, 300.0, engine.analysis().bpm));
    republishParts();
    captureSettledState();
    sendChangeMessage();
}

void ProgressionsProcessor::parameterChanged(const juce::String&, float)
{
    triggerAsyncUpdate();
}

void ProgressionsProcessor::handleAsyncUpdate()
{
    pushUndoState(false);
    regenerate();
}

void ProgressionsProcessor::initialise()
{
    cancelLibraryScan();
    pushUndoState(true);

    for (auto* parameter : getParameters())
        if (auto* ranged = dynamic_cast<juce::RangedAudioParameter*>(parameter))
            ranged->setValueNotifyingHost(ranged->getDefaultValue());

    engine.resetProgression();
    engine.clear();

    auto options = engine.analysisOptions();
    options.forceKey = false;
    engine.setAnalysisOptions(options);
    keyForced = false;
    keyTonic = 9;
    keyScale = harmonia::ScaleType::NaturalMinor;

    dropAllParts();
    padStamp = 0;
    partPadStamp.fill(0);
    sourceName.clear();
    sourceMidiData.reset();
    lastError.clear();
    scanSummary.clear();
    seed = static_cast<juce::uint32>(juce::Random::getSystemRandom().nextInt(1000000) + 1);

    publish(nullptr);
    regenerate();
    sendChangeMessage();
}

void ProgressionsProcessor::rollNewSeed()
{
    pushUndoState(true);
    setSeed(static_cast<juce::uint32>(juce::Random::getSystemRandom().nextInt(1000000) + 1));
}

void ProgressionsProcessor::setSeed(juce::uint32 newSeed)
{
    seed = newSeed == 0 ? 1 : newSeed;
    regenerate();
}

void ProgressionsProcessor::reharmonize(float amount)
{
    pushUndoState(true);
    engine.applyReharmonization(seed * 2654435761u + 17u, amount);
    regenerate();
}

void ProgressionsProcessor::resetProgression()
{
    pushUndoState(true);
    engine.resetProgression();
    regenerate();
}

bool ProgressionsProcessor::setProgressionText(const juce::String& text)
{
    pushUndoState(true);
    std::string error;
    if (! engine.setProgressionText(text.toStdString(), error))
    {
        lastError = error;
        sendChangeMessage();
        return false;
    }

    lastError.clear();
    regenerate();
    return true;
}

bool ProgressionsProcessor::applyPreset(const juce::String& presetId)
{
    pushUndoState(true);
    std::string error;
    if (! engine.applyPreset(presetId.toStdString(), error))
    {
        lastError = error;
        sendChangeMessage();
        return false;
    }

    lastError.clear();
    regenerate();
    return true;
}

juce::String ProgressionsProcessor::getProgressionText() const
{
    if (engine.hasWrittenProgression())
        return juce::String(engine.analysis().progressionString());
    return engine.analysis().valid ? juce::String(engine.analysis().progressionString()) : juce::String();
}

bool ProgressionsProcessor::loadStyleModelFile(const juce::File& file, bool install)
{
    std::string error;
    harmonia::StyleModel loaded;
    if (! harmonia::loadStyleModel(file.getFullPathName().toStdString(), loaded, error))
    {
        lastError = error;
        sendChangeMessage();
        return false;
    }

    styleModel = std::move(loaded);
    styleFile = file;
    styleOrigin = StyleSource::Session;
    lastError.clear();

    if (install)
    {
        juce::String installError;
        if (styleStore::install(file, installError))
        {
            styleFile = styleStore::installedFile();
            styleOrigin = StyleSource::Installed;
        }
        else
        {
            // The model still works for this instance; it just will not be
            // there next time, and the user should know why.
            lastError = "Loaded, but could not keep it in the plugin: " + installError.toStdString();
        }
    }

    regenerate();
    return true;
}

void ProgressionsProcessor::loadDefaultStyleModel()
{
    harmonia::StyleModel loaded;
    std::string error;

    if (styleStore::hasInstalled())
    {
        const auto file = styleStore::installedFile();
        if (harmonia::loadStyleModel(file.getFullPathName().toStdString(), loaded, error))
        {
            styleModel = std::move(loaded);
            styleFile = file;
            styleOrigin = StyleSource::Installed;
            return;
        }
    }

    if (styleStore::hasBuiltIn())
    {
        const auto json = styleStore::builtInJson();
        if (harmonia::parseStyleModel(json, loaded, error, "built-in style model"))
        {
            styleModel = std::move(loaded);
            styleFile = juce::File();
            styleOrigin = StyleSource::BuiltIn;
            return;
        }
    }

    styleModel = harmonia::StyleModel {};
    styleFile = juce::File();
    styleOrigin = StyleSource::None;
}

void ProgressionsProcessor::forgetInstalledStyleModel()
{
    styleStore::forget();
    loadDefaultStyleModel();
    regenerate();
}

void ProgressionsProcessor::clearStyleModel()
{
    styleModel = harmonia::StyleModel {};
    styleFile = juce::File();
    styleOrigin = StyleSource::None;
    regenerate();
}

juce::String ProgressionsProcessor::styleSummary() const
{
    return styleModel.empty() ? juce::String() : juce::String(styleModel.summary());
}

// ---------------------------------------------------------------------------
// Learning straight from a folder on disk

bool ProgressionsProcessor::startLibraryScan(const juce::File& folder)
{
    if (scan != nullptr || ! folder.isDirectory())
        return false;

    scanSummary.clear();
    lastError.clear();
    scan = std::make_unique<LibraryScan>(folder);
    scan->start();
    startTimer(200);
    sendChangeMessage();
    return true;
}

void ProgressionsProcessor::cancelLibraryScan()
{
    if (scan != nullptr)
        scan->cancel();
}

bool ProgressionsProcessor::libraryScanCounting() const
{
    return scan != nullptr && scan->counting();
}

int ProgressionsProcessor::libraryScanDone() const
{
    return scan != nullptr ? scan->filesDone() : 0;
}

int ProgressionsProcessor::libraryScanTotal() const
{
    return scan != nullptr ? scan->filesTotal() : 0;
}

juce::File ProgressionsProcessor::libraryScanFolder() const
{
    return scan != nullptr ? scan->folder() : juce::File();
}

void ProgressionsProcessor::timerCallback()
{
    pollLibraryScan();
}

bool ProgressionsProcessor::pollLibraryScan()
{
    if (scan == nullptr || ! scan->finished())
        return false;

    stopTimer();
    const auto done = std::move(scan);
    scan.reset();

    const auto& stats = done->stats();
    const auto learned = done->result();

    if (learned.empty())
    {
        scanSummary = stats.midiFilesFound == 0
                          ? "No MIDI files in " + done->folder().getFileName()
                          : "Nothing could be read in " + done->folder().getFileName();
        lastError = scanSummary.toStdString();
        sendChangeMessage();
        return true;
    }

    styleModel = learned;
    styleOrigin = StyleSource::Session;
    styleFile = juce::File();

    juce::String installError;
    if (styleStore::installModel(styleModel, installError))
    {
        styleFile = styleStore::installedFile();
        styleOrigin = StyleSource::Installed;
    }
    else
    {
        lastError = "Learned, but could not keep it in the plugin: " + installError.toStdString();
    }

    scanSummary = juce::String(styleModel.clipsLearned) + " clips learned from "
                      + juce::String(static_cast<int>(stats.midiFilesFound)) + " files";
    if (stats.cancelled)
        scanSummary += " (stopped early)";
    if (stats.walkErrors > 0)
        scanSummary += ", " + juce::String(static_cast<int>(stats.walkErrors)) + " folders unreadable";

    regenerate();
    sendChangeMessage();
    return true;
}

juce::Array<juce::File> ProgressionsProcessor::suggestedLibraryFolders()
{
    juce::Array<juce::File> found;

    const auto add = [&found](const juce::File& file)
    {
        if (file.isDirectory() && ! found.contains(file))
            found.add(file);
    };

    const auto home = juce::File::getSpecialLocation(juce::File::userHomeDirectory);
    add(home.getChildFile("Music/Ableton/User Library"));
    add(home.getChildFile("Documents/Ableton"));
    add(juce::File::getSpecialLocation(juce::File::userMusicDirectory));
    add(home.getChildFile("Documents"));

   #if JUCE_MAC
    // External drives, which is where a collection this size usually lives.
    for (const auto& volume : juce::File("/Volumes").findChildFiles(juce::File::findDirectories, false))
        if (! volume.isSymbolicLink())
            add(volume);
   #endif

    return found;
}

juce::String ProgressionsProcessor::styleSourceText() const
{
    switch (styleOrigin)
    {
        case StyleSource::BuiltIn:   return "built into the plugin";
        case StyleSource::Installed: return "kept in the plugin";
        case StyleSource::Session:   return styleFile.getFileName();
        case StyleSource::None:      break;
    }
    return {};
}

void ProgressionsProcessor::setForcedKey(bool forced, int tonic, harmonia::ScaleType scale)
{
    keyForced = forced;
    keyTonic = juce::jlimit(0, 11, tonic);
    keyScale = scale;

    auto options = engine.analysisOptions();
    options.forceKey = forced;
    options.key = { keyTonic, keyScale };
    engine.setAnalysisOptions(options);
    regenerate();
}

void ProgressionsProcessor::nudgeChord(int index, int direction)
{
    pushUndoState(true);
    const auto& progression = engine.analysis().progression;
    if (! juce::isPositiveAndBelow(index, static_cast<int>(progression.size())))
        return;

    // Step the chord through the diatonic degrees of the detected key.
    const auto triads = diatonicTriads(engine.analysis().key);
    if (triads.empty())
        return;

    const Chord current = progression[static_cast<size_t>(index)].chord;
    int degree = engine.analysis().key.degreeOf(current.root);
    if (degree < 0)
        degree = 0;

    degree = (degree + direction + static_cast<int>(triads.size())) % static_cast<int>(triads.size());
    const_cast<harmonia::Engine&>(engine).setChord(static_cast<size_t>(index), triads[static_cast<size_t>(degree)]);
    regenerate();
}

// ---------------------------------------------------------------------------

bool ProgressionsProcessor::loadMidiFile(const juce::File& file)
{
    pushUndoState(true);
    juce::MemoryBlock block;
    if (! file.loadFileAsData(block))
    {
        lastError = "Could not read " + file.getFileName();
        sendChangeMessage();
        return false;
    }
    return loadMidiFromData(block.getData(), block.getSize(), file.getFileNameWithoutExtension());
}

bool ProgressionsProcessor::loadMidiFromData(const void* data, size_t size, const juce::String& displayName)
{
    pushUndoState(true);
    std::string error;
    if (! engine.loadSourceFromMemory(static_cast<const juce::uint8*>(data), size, error))
    {
        lastError = error;
        sendChangeMessage();
        return false;
    }

    lastError.clear();
    sourceName = displayName;
    sourceMidiData.replaceAll(data, size);
    regenerate();
    return true;
}

void ProgressionsProcessor::clearSource()
{
    pushUndoState(true);
    engine.clear();
    generated = NoteSequence {};
    sourceName.clear();
    sourceMidiData.reset();
    publish(nullptr);
    sendChangeMessage();
}

bool ProgressionsProcessor::exportGenerated(const juce::File& destination)
{
    if (generated.empty())
        return false;

    const auto bytes = midi::writeToMemory(generated);
    return destination.replaceWithData(bytes.data(), bytes.size());
}

juce::File ProgressionsProcessor::writeDragFile()
{
    if (generated.empty())
        return {};

    const auto partName = kPartNames[juce::jlimit(0, kPartNames.size() - 1,
                                                 static_cast<int>(apvts.getRawParameterValue(ParamID::part)->load()))];
    const auto stem = (sourceName.isNotEmpty() ? sourceName : juce::String("harmonia")) + "_" + partName.replace(" ", "");

    if (dragFile.existsAsFile())
        dragFile.deleteFile();

    dragFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                   .getChildFile(juce::File::createLegalFileName(stem) + ".mid");
    return exportGenerated(dragFile) ? dragFile : juce::File();
}

// ---------------------------------------------------------------------------

void ProgressionsProcessor::startPlayback()
{
    internalPPQ = 0.0;
    internalPlaying.store(true);
}

void ProgressionsProcessor::stopPlayback()
{
    internalPlaying.store(false);
    reportedPosition.store(0.0);
}

double ProgressionsProcessor::getPlayPositionNormalised() const
{
    return reportedPosition.load();
}

// ---------------------------------------------------------------------------

juce::ValueTree ProgressionsProcessor::stateTree()
{
    auto state = apvts.copyState();
    state.setProperty("seed", static_cast<juce::int64>(seed), nullptr);
    state.setProperty("sourceName", sourceName, nullptr);
    state.setProperty("keyForced", keyForced, nullptr);
    // Only a model loaded into this instance alone is worth writing down. The
    // installed and built-in ones are found again wherever the project opens,
    // which is the point of keeping them inside the plugin.
    if (styleOrigin == StyleSource::Session && styleFile != juce::File())
        state.setProperty("styleFile", styleFile.getFullPathName(), nullptr);
    state.setProperty("keyTonic", keyTonic, nullptr);
    state.setProperty("keyScale", static_cast<int>(keyScale), nullptr);
    if (engine.hasWrittenProgression())
        state.setProperty("progression", juce::String(engine.progressionText()), nullptr);
    if (sourceMidiData.getSize() > 0)
        state.setProperty("sourceMidi", sourceMidiData.toBase64Encoding(), nullptr);
    return state;
}

void ProgressionsProcessor::getStateInformation(juce::MemoryBlock& destData)
{
    if (auto xml = stateTree().createXml())
        copyXmlToBinary(*xml, destData);
}

void ProgressionsProcessor::setStateInformation(const void* data, int sizeInBytes)
{
    auto xml = getXmlFromBinary(data, sizeInBytes);
    if (xml == nullptr)
        return;

    auto state = juce::ValueTree::fromXml(*xml);
    if (! state.isValid())
        return;

    applyStateTree(state);
    undoStack.clear();
    captureSettledState();
}

void ProgressionsProcessor::applyStateTree(const juce::ValueTree& source)
{
    auto state = source.createCopy();
    const juce::ScopedValueSetter<bool> guard(restoringState, true);

    seed = static_cast<juce::uint32>(static_cast<juce::int64>(state.getProperty("seed", 1)));
    sourceName = state.getProperty("sourceName", "").toString();

    const auto encoded = state.getProperty("sourceMidi", "").toString();
    if (encoded.isNotEmpty())
    {
        juce::MemoryBlock block;
        if (block.fromBase64Encoding(encoded))
        {
            std::string error;
            if (engine.loadSourceFromMemory(static_cast<const juce::uint8*>(block.getData()), block.getSize(), error))
                sourceMidiData = block;
        }
    }

    // A per-session model overrides what the plugin carries, but only for this
    // instance - reopening a project must never rewrite the installed model.
    // When the path is gone (another machine, a moved drive) we simply keep
    // the model the plugin came with.
    const juce::File savedStyle(state.getProperty("styleFile", "").toString());
    if (savedStyle.existsAsFile())
        loadStyleModelFile(savedStyle, false);
    state.removeProperty("styleFile", nullptr);

    keyForced = state.getProperty("keyForced", false);
    keyTonic = juce::jlimit(0, 11, static_cast<int>(state.getProperty("keyTonic", 9)));
    keyScale = static_cast<harmonia::ScaleType>(static_cast<int>(state.getProperty("keyScale", 1)));
    if (keyForced)
    {
        auto analysisOptions = engine.analysisOptions();
        analysisOptions.forceKey = true;
        analysisOptions.key = { keyTonic, keyScale };
        engine.setAnalysisOptions(analysisOptions);
    }

    const auto progression = state.getProperty("progression", "").toString();
    if (progression.isNotEmpty())
    {
        std::string error;
        engine.setProgressionText(progression.toStdString(), error);
    }
    else
    {
        // Undoing back past a typed progression has to take it away again.
        engine.resetProgression();
    }

    state.removeProperty("sourceMidi", nullptr);
    state.removeProperty("progression", nullptr);
    apvts.replaceState(state);
    dropAllParts();
    regenerate();
}

// --- Undo -------------------------------------------------------------------
// One step back for anything on screen. Parameter changes arrive one at a time
// while a knob is being dragged, so a snapshot is only taken when the last one
// has had a moment to settle - otherwise a single turn of Density would fill
// the stack. A discrete action (a preset, a typed progression, a new idea)
// always gets its own step.

void ProgressionsProcessor::captureSettledState()
{
    if (auto xml = stateTree().createXml())
    {
        settledState.reset();
        copyXmlToBinary(*xml, settledState);
    }
}

void ProgressionsProcessor::pushUndoState(bool discrete)
{
    if (restoringState || settledState.getSize() == 0)
        return;

    const auto now = juce::Time::getMillisecondCounter();
    if (! discrete && now - lastUndoPushMs < 500)
        return;
    lastUndoPushMs = now;

    if (! undoStack.empty() && undoStack.back() == settledState)
        return;

    undoStack.push_back(settledState);
    constexpr size_t kMaxUndoSteps = 32;
    if (undoStack.size() > kMaxUndoSteps)
        undoStack.erase(undoStack.begin());
}

bool ProgressionsProcessor::undo()
{
    if (undoStack.empty())
        return false;

    const auto previous = undoStack.back();
    undoStack.pop_back();

    if (auto xml = getXmlFromBinary(previous.getData(), static_cast<int>(previous.getSize())))
    {
        auto state = juce::ValueTree::fromXml(*xml);
        if (state.isValid())
            applyStateTree(state);
    }

    settledState = previous;
    sendChangeMessage();
    return true;
}

float ProgressionsProcessor::reharmonizeAmount() const
{
    return apvts.getRawParameterValue(ParamID::reharmAmount)->load();
}

juce::AudioProcessorEditor* ProgressionsProcessor::createEditor()
{
    return new ProgressionsEditor(*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new ProgressionsProcessor();
}
