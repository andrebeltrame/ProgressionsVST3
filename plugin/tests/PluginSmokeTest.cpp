// Headless checks for the plugin layer: instantiate the processor, load a clip,
// run the audio callback, verify the MIDI it emits, round-trip its state, and
// render the editor to a PNG so the layout can be eyeballed without a DAW.

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"

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
            editor->setSize(1060, 760);
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
