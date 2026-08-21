// Headless checks for the plugin layer: instantiate the processor, load a clip,
// run the audio callback, verify the MIDI it emits, round-trip its state, and
// render the editor to a PNG so the layout can be eyeballed without a DAW.

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "StyleStore.h"

#include "harmonia/StyleModel.h"

#include <cstdlib>
#include <iostream>
#include <map>

namespace
{

int failures = 0;

void check(bool condition, const juce::String& what)
{
    if (condition)
    {
        std::cout << "  ok    " << what << "\n";
    }
    else
    {
        std::cout << "  FAIL  " << what << "\n";
        ++failures;
    }
}

juce::File findExample()
{
    auto directory = juce::File::getCurrentWorkingDirectory();
    for (int i = 0; i < 6 && directory.exists(); ++i)
    {
        const auto candidate = directory.getChildFile("resources/examples/bass_loop.mid");
        if (candidate.existsAsFile())
            return candidate;
        directory = directory.getParentDirectory();
    }
    return {};
}

} // namespace

int main(int argc, char** argv)
{
    const juce::ScopedJuceInitialiser_GUI juceInit;

    // Point the installed-model folder at a scratch directory before anything
    // constructs a processor: the test installs models, and it must never
    // overwrite the one a real installation is using.
    const auto styleDirectory = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                    .getChildFile("harmonia_smoke_style");
    styleDirectory.deleteRecursively();
   #if JUCE_WINDOWS
    _putenv_s("HARMONIA_STYLE_DIR", styleDirectory.getFullPathName().toRawUTF8());
   #else
    setenv("HARMONIA_STYLE_DIR", styleDirectory.getFullPathName().toRawUTF8(), 1);
   #endif

    const auto example = findExample();
    if (! example.existsAsFile())
    {
        std::cerr << "Could not find resources/examples/bass_loop.mid - run from the repo root.\n";
        return 2;
    }

    HarmoniaProcessor processor;

    std::cout << "Loading " << example.getFileName() << "\n";
    check(processor.loadMidiFile(example), "clip loads");
    check(processor.getEngine().analysis().valid, "analysis is valid");
    check(processor.getEngine().analysis().progression.size() >= 2, "a progression was detected");
    check(! processor.getGeneratedSequence().empty(), "a part was generated on load");

    std::cout << "  key        : " << processor.getEngine().analysis().key.name() << "\n";
    std::cout << "  progression: " << processor.getEngine().analysis().progressionString() << "\n";
    std::cout << "  generated  : " << processor.getGeneratedSequence().notes.size() << " notes\n";

    // ---- Audio callback ----------------------------------------------------
    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    processor.setPlayConfigDetails(0, 2, sampleRate, blockSize);
    processor.prepareToPlay(sampleRate, blockSize);
    processor.startPlayback();

    juce::AudioBuffer<float> audio(2, blockSize);
    std::map<int, int> balance; // pitch -> outstanding note-ons
    int noteOns = 0;
    float peak = 0.0f;

    for (int block = 0; block < 400; ++block)
    {
        juce::MidiBuffer midi;
        audio.clear();
        processor.processBlock(audio, midi);

        for (const auto metadata : midi)
        {
            const auto message = metadata.getMessage();
            if (metadata.samplePosition < 0 || metadata.samplePosition >= blockSize)
                check(false, "event inside the block");
            if (message.isNoteOn())
            {
                ++noteOns;
                ++balance[message.getNoteNumber()];
            }
            else if (message.isNoteOff())
            {
                --balance[message.getNoteNumber()];
            }
        }
        peak = juce::jmax(peak, audio.getMagnitude(0, blockSize));
    }

    processor.stopPlayback();
    {
        juce::MidiBuffer midi;
        audio.clear();
        processor.processBlock(audio, midi);
    }

    std::cout << "  played " << noteOns << " note-ons over 400 blocks, peak " << peak << "\n";
    check(noteOns > 0, "the transport emits MIDI");

    bool hanging = false;
    for (const auto& [pitch, count] : balance)
        if (count > 1 || count < 0)
            hanging = true;
    check(! hanging, "every note-on is matched by a note-off");
    check(peak > 0.001f, "the preview synth makes sound");

    // ---- Parameters and regeneration ----------------------------------------
    const auto firstIdea = processor.getGeneratedSequence().notes.size();
    processor.apvts.getParameter(ParamID::part)->setValueNotifyingHost(
        processor.apvts.getParameter(ParamID::part)->convertTo0to1(2.0f)); // Melody
    processor.regenerate();
    check(! processor.getGeneratedSequence().empty(), "switching part regenerates");
    juce::ignoreUnused(firstIdea);

    const auto seedBefore = processor.getSeed();
    processor.rollNewSeed();
    check(processor.getSeed() != seedBefore, "the dice changes the seed");

    processor.reharmonize(1.0f);
    check(processor.getEngine().analysis().progression.size() >= 2, "reharmonisation keeps the grid");
    processor.resetProgression();

    // ---- Written progressions --------------------------------------------------
    check(processor.setProgressionText("Dm7 | G7 | Cmaj7 | Cmaj7"), "a typed progression is accepted");
    check(processor.hasWrittenProgression(), "the engine knows the chords were written");
    check(processor.getEngine().analysis().progression.size() == 4, "four chords in, four chords out");
    check(! processor.getGeneratedSequence().empty(), "typing chords regenerates the part");
    std::cout << "  typed      : " << processor.getEngine().analysis().progressionString() << "\n";

    check(! processor.setProgressionText("Am | nonsense | C"), "a typo is rejected");
    check(processor.getLastError().isNotEmpty(), "and explains itself");
    check(processor.getEngine().analysis().progression.size() == 4, "the old chords survive a typo");

    check(processor.applyPreset("deep-warm"), "a style preset applies");
    check(processor.getEngine().analysis().progression.size() == 4, "the preset has four chords");
    std::cout << "  preset     : " << processor.getEngine().analysis().progressionString()
              << "   (" << processor.getEngine().analysis().romanNumeralString() << ")\n";
    check(! processor.applyPreset("no-such-preset"), "an unknown preset is refused");

    processor.setForcedKey(true, 5, harmonia::ScaleType::NaturalMinor); // F minor
    check(processor.getEngine().analysis().key.tonic == 5, "the key can be pinned");
    processor.setForcedKey(false, 5, harmonia::ScaleType::NaturalMinor);

    processor.resetProgression();
    check(! processor.hasWrittenProgression(), "reset goes back to the detected chords");
    check(processor.getEngine().analysis().progressionString() == "Am | F | C | G",
          "and the detected chords are the ones from the clip");

    // ---- Chords with no clip at all ---------------------------------------------
    {
        HarmoniaProcessor blank;
        blank.setPlayConfigDetails(0, 2, sampleRate, blockSize);
        blank.prepareToPlay(sampleRate, blockSize);
        check(blank.setProgressionText("Fm | Db | Ab | Eb"), "chords work with no clip loaded");
        check(blank.getEngine().analysis().valid, "and produce a valid analysis");
        check(! blank.getGeneratedSequence().empty(), "and a part");
        std::cout << "  no clip    : " << blank.getEngine().analysis().key.name() << "  "
                  << blank.getEngine().analysis().progressionString() << "\n";

        blank.startPlayback();
        int blankNoteOns = 0;
        for (int block = 0; block < 100; ++block)
        {
            juce::MidiBuffer midi;
            audio.clear();
            blank.processBlock(audio, midi);
            for (const auto metadata : midi)
                if (metadata.getMessage().isNoteOn())
                    ++blankNoteOns;
        }
        check(blankNoteOns > 0, "and play back");
    }

    // ---- A style model learned from a collection ---------------------------------
    {
        // Build a tiny corpus, learn from it, and check the plugin picks it up.
        harmonia::StyleModel model;
        harmonia::NoteSequence corpus;
        corpus.ppq = harmonia::kPPQ;
        corpus.tempos.push_back({ 122.0, 0 });
        corpus.timeSignatures.push_back({ 4, 4, 0 });
        const int64_t slot = harmonia::kPPQ / 4;
        for (int bar = 0; bar < 4; ++bar)
            for (int position : { 0, 6 })
                corpus.notes.push_back({ bar * harmonia::kPPQ * 4 + position * slot, slot * 2,
                                         36 + (position == 6 ? 12 : 0), position == 0 ? 110 : 84, 0 });
        corpus.sort();
        harmonia::learnFromClip(model, corpus, harmonia::analyze(corpus), "bass");

        const auto modelFile = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                   .getChildFile("harmonia_smoke.style.json");
        std::string styleError;
        check(harmonia::saveStyleModel(modelFile.getFullPathName().toStdString(), model, styleError),
              "a style model can be written");

        check(processor.loadStyleModelFile(modelFile), "the plugin loads a style model");
        check(processor.hasStyleModel(), "and knows it has one");
        std::cout << "  style      : " << processor.styleSummary() << "\n";

        processor.apvts.getParameter(ParamID::part)->setValueNotifyingHost(
            processor.apvts.getParameter(ParamID::part)->convertTo0to1(4.0f)); // Bass
        processor.apvts.getParameter(ParamID::humanize)->setValueNotifyingHost(0.0f);
        processor.regenerate();

        const auto& bass = processor.getGeneratedSequence();
        check(! bass.empty(), "and writes a bass with it");

        bool onLearnedSlots = ! bass.empty();
        for (const auto& note : bass.notes)
        {
            const auto position = (note.startTick % (harmonia::kPPQ * 4)) / slot;
            if (position != 0 && position != 6)
                onLearnedSlots = false;
        }
        check(onLearnedSlots, "on exactly the 16ths the corpus plays");

        processor.clearStyleModel();
        check(! processor.hasStyleModel(), "and the model can be unloaded");
        check(processor.loadStyleModelFile(modelFile), "and loaded again");

        // Loading keeps it inside the plugin, so the next instance - in any
        // project, on any day - starts with the same brain and no file path.
        check(processor.styleSource() == HarmoniaProcessor::StyleSource::Installed,
              "loading a model keeps it in the plugin");
        check(styleStore::hasInstalled(), "and writes it to the plugin's own folder");

        {
            HarmoniaProcessor fresh;
            check(fresh.hasStyleModel(), "a brand new instance already has a model");
            check(fresh.styleClipCount() == processor.styleClipCount(), "the same one");
            check(fresh.styleSource() == HarmoniaProcessor::StyleSource::Installed,
                  "and says where it came from");

            // A project that referenced a model by path must not rewrite the
            // installed one when it opens.
            juce::MemoryBlock sessionState;
            fresh.getStateInformation(sessionState);
            const auto xml = juce::AudioProcessor::getXmlFromBinary(sessionState.getData(),
                                                                    static_cast<int>(sessionState.getSize()));
            check(xml != nullptr && ! xml->hasAttribute("styleFile"),
                  "and does not write the path into the project");
        }

        processor.forgetInstalledStyleModel();
        check(! styleStore::hasInstalled(), "forgetting removes the installed copy");
        std::cout << "  built in   : " << (styleStore::hasBuiltIn()
                                               ? styleStore::builtInName()
                                               : juce::String("nothing (-DHARMONIA_STYLE_MODEL not set)"))
                  << "\n";
        check(processor.styleSource() == (styleStore::hasBuiltIn()
                                              ? HarmoniaProcessor::StyleSource::BuiltIn
                                              : HarmoniaProcessor::StyleSource::None),
              "and falls back to whatever is baked into the build");

        check(processor.loadStyleModelFile(modelFile), "and it can be installed again");
    }

    // ---- State round trip ----------------------------------------------------
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    check(state.getSize() > 0, "state is written");

    processor.applyPreset("melodic-lift");
    processor.getStateInformation(state);

    HarmoniaProcessor restored;
    restored.setStateInformation(state.getData(), static_cast<int>(state.getSize()));
    check(restored.getEngine().hasSource(), "state restores the source clip");
    check(restored.getSeed() == processor.getSeed(), "state restores the seed");
    check(restored.getGeneratedSequence().notes.size() == processor.getGeneratedSequence().notes.size(),
          "state restores the same idea");
    check(restored.hasWrittenProgression(), "state restores the written progression");
    check(restored.getEngine().analysis().progressionString()
              == processor.getEngine().analysis().progressionString(),
          "including the exact chords");

    // ---- Export ---------------------------------------------------------------
    const auto exported = juce::File::getSpecialLocation(juce::File::tempDirectory)
                              .getChildFile("harmonia_smoke_export.mid");
    check(processor.exportGenerated(exported), "export writes a MIDI file");
    check(exported.getSize() > 20, "the exported file has content");
    exported.deleteFile();

    // ---- Editor ---------------------------------------------------------------
    {
        std::unique_ptr<juce::AudioProcessorEditor> editor(processor.createEditor());
        check(editor != nullptr, "editor is created");

        if (editor != nullptr)
        {
            editor->setSize(1060, 780);
            juce::Image image(juce::Image::ARGB, editor->getWidth(), editor->getHeight(), true);
            {
                juce::Graphics g(image);
                editor->paintEntireComponent(g, true);
            }

            const juce::File output(argc > 1 ? juce::String(argv[1])
                                             : juce::File::getCurrentWorkingDirectory()
                                                   .getChildFile("harmonia-ui.png").getFullPathName());
            output.deleteFile();
            if (auto stream = output.createOutputStream())
            {
                juce::PNGImageFormat png;
                check(png.writeImageToStream(image, *stream), "editor renders to a PNG");
                std::cout << "  wrote " << output.getFullPathName() << "\n";
            }
        }
    }

    std::cout << (failures == 0 ? "\nAll plugin smoke checks passed\n"
                                : "\n" + juce::String(failures) + " plugin smoke checks failed\n");
    return failures == 0 ? 0 : 1;
}
