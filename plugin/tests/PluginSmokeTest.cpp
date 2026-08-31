// Headless checks for the plugin layer: instantiate the processor, load a clip,
// run the audio callback, verify the MIDI it emits, round-trip its state, and
// render the editor to a PNG so the layout can be eyeballed without a DAW.

#include <juce_gui_basics/juce_gui_basics.h>

#include "PluginEditor.h"
#include "PluginProcessor.h"
#include "StyleStore.h"

#include "harmonia/MidiFile.h"
#include "harmonia/StyleModel.h"

#include <cmath>
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

    ProgressionsProcessor processor;

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

    // ---- Parts stack, each on its own channel -----------------------------------
    {
        const auto selectPart = [&processor](int index)
        {
            auto* parameter = processor.apvts.getParameter(ParamID::part);
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(index)));
            processor.regenerate();
        };

        selectPart(0); // Pad
        check(processor.isPartLive(0), "generating a part keeps it");
        const auto padNotes = processor.partSequence(0).notes.size();

        selectPart(4); // Bass
        check(processor.isPartLive(4), "the bass is kept too");
        check(processor.isPartLive(0), "and the pad is still there");
        check(processor.partSequence(0).notes.size() == padNotes, "untouched by writing the bass");
        check(processor.channelForPart(0) != processor.channelForPart(4),
              "each part gets its own MIDI channel");

        // Regenerating the pad unchanged is still the same pad.
        selectPart(0);
        check(! processor.isPartStale(4), "an identical pad leaves the other parts alone");

        // A different pad does not: the bass belongs to one that is gone.
        processor.rollNewSeed();
        check(processor.isPartStale(4), "a new pad marks the parts written over the old one");
        check(! processor.isPartStale(0), "the pad is never stale against itself");

        processor.dropPart(4);
        check(! processor.isPartLive(4), "a part can be dropped");
        check(processor.isPartLive(0), "without taking the others with it");
    }

    // ---- Undo --------------------------------------------------------------------
    {
        auto* density = processor.apvts.getParameter(ParamID::density);
        const float before = density->getValue();
        density->setValueNotifyingHost(before > 0.5f ? 0.1f : 0.9f);
        processor.regenerate();

        check(processor.canUndo(), "a change leaves something to undo");
        check(processor.undo(), "undo runs");
        check(std::abs(density->getValue() - before) < 0.001f, "and puts the knob back");

        check(processor.setProgressionText("Fm | Db | Ab | Eb"), "a progression to undo");
        check(processor.hasWrittenProgression(), "which took effect");
        check(processor.undo(), "undo runs again");
        check(! processor.hasWrittenProgression(), "and takes the typed chords away");
    }

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
        ProgressionsProcessor blank;
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
        check(processor.styleSource() == ProgressionsProcessor::StyleSource::Installed,
              "loading a model keeps it in the plugin");
        check(styleStore::hasInstalled(), "and writes it to the plugin's own folder");

        {
            ProgressionsProcessor fresh;
            check(fresh.hasStyleModel(), "a brand new instance already has a model");
            check(fresh.styleClipCount() == processor.styleClipCount(), "the same one");
            check(fresh.styleSource() == ProgressionsProcessor::StyleSource::Installed,
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
                                              ? ProgressionsProcessor::StyleSource::BuiltIn
                                              : ProgressionsProcessor::StyleSource::None),
              "and falls back to whatever is baked into the build");

        check(processor.loadStyleModelFile(modelFile), "and it can be installed again");
    }

    // ---- Learning straight from a folder on disk ---------------------------
    {
        // The whole point of the plugin having a brain: point it at a folder,
        // get a model, no terminal involved.
        const auto libraryFolder = juce::File::getSpecialLocation(juce::File::tempDirectory)
                                       .getChildFile("progressions_smoke_library");
        libraryFolder.deleteRecursively();
        libraryFolder.getChildFile("Bass").createDirectory();
        libraryFolder.getChildFile("Leads").createDirectory();

        const auto writeClip = [](const juce::File& file, const std::vector<int>& pitches, int channel)
        {
            harmonia::NoteSequence sequence;
            sequence.ppq = harmonia::kPPQ;
            sequence.tempos.push_back({ 124.0, 0 });
            sequence.timeSignatures.push_back({ 4, 4, 0 });
            for (size_t i = 0; i < pitches.size(); ++i)
                sequence.notes.push_back({ static_cast<int64_t>(i) * harmonia::kPPQ,
                                           harmonia::kPPQ, pitches[i], 100, channel });
            sequence.sort();
            std::string writeError;
            return harmonia::midi::writeToFile(file.getFullPathName().toStdString(), sequence, writeError);
        };

        check(writeClip(libraryFolder.getChildFile("Bass/loop_a.mid"), { 36, 41, 43, 38 }, 0),
              "a corpus can be written to disk");
        writeClip(libraryFolder.getChildFile("Bass/loop_b.mid"), { 33, 40, 45, 40 }, 0);
        writeClip(libraryFolder.getChildFile("Leads/hook.mid"), { 72, 74, 76, 79 }, 0);

        processor.forgetInstalledStyleModel();
        check(processor.startLibraryScan(libraryFolder), "a folder scan starts");
        check(! processor.startLibraryScan(libraryFolder), "and a second one is refused while it runs");

        // No message loop here, so drive the completion step directly - the
        // plugin does the same thing off a timer.
        for (int i = 0; i < 400 && processor.isScanningLibrary(); ++i)
        {
            juce::Thread::sleep(25);
            processor.pollLibraryScan();
        }

        check(! processor.isScanningLibrary(), "the scan finishes");
        check(processor.hasStyleModel(), "and the plugin has a model it learned itself");
        check(processor.styleClipCount() == 3, "one clip per file in the folder");
        check(processor.styleSource() == ProgressionsProcessor::StyleSource::Installed,
              "kept in the plugin, like a loaded one");
        check(styleStore::hasInstalled(), "and written to the plugin's folder");
        check(processor.lastScanSummary().isNotEmpty(), "with something to show the user");
        std::cout << "  scan       : " << processor.lastScanSummary() << "\n";

        // Every folder offered must actually be there, or the menu points at
        // nothing.
        bool suggestionsExist = true;
        for (const auto& suggestion : ProgressionsProcessor::suggestedLibraryFolders())
            suggestionsExist = suggestionsExist && suggestion.isDirectory();
        check(suggestionsExist, "every suggested folder exists");

        libraryFolder.deleteRecursively();
    }

    // ---- Every part writes something ------------------------------------------
    {
        // A part added to the enum but missed in a switch comes out silent, and
        // nothing else would notice: the tab is there, the plug-in is happy, and
        // the part is empty. Walk all of them.
        auto* parameter = processor.apvts.getParameter(ParamID::part);
        for (int index = 0; index < kNumParts; ++index)
        {
            parameter->setValueNotifyingHost(parameter->convertTo0to1(static_cast<float>(index)));
            processor.regenerate();
            check(! processor.getGeneratedSequence().empty(),
                  ("part " + juce::String(index) + " writes notes").toRawUTF8());
        }
        processor.dropAllParts();
    }

    // ---- The Sub is one voice on the root --------------------------------------
    {
        // Named, not counted. This said kNumParts - 1 until a part was appended
        // after the Sub, and then it quietly started asserting the Sub's rules
        // against a different part - which is the exact mistake the comment in
        // the editor warns about.
        auto* parameter = processor.apvts.getParameter(ParamID::part);
        parameter->setValueNotifyingHost(
            parameter->convertTo0to1(static_cast<float>(harmonia::PartType::Sub)));
        processor.apvts.getParameter(ParamID::density)->setValueNotifyingHost(1.0f);
        processor.regenerate();

        const auto& subPart = processor.getGeneratedSequence();
        check(! subPart.empty(), "the sub writes notes");

        bool single = true;
        for (size_t i = 1; i < subPart.notes.size(); ++i)
            single = single && subPart.notes[i].startTick >= subPart.notes[i - 1].endTick();
        check(single, "and never two of them at once, however busy it is set");

        processor.apvts.getParameter(ParamID::density)->setValueNotifyingHost(0.5f);
        processor.dropAllParts();
        processor.regenerate();
    }

    // ---- Locked chords --------------------------------------------------------
    {
        processor.resetProgression();
        processor.setProgressionText("Am | F | C | G");
        const auto before = processor.getEngine().analysis().progressionString();

        processor.toggleChordLock(0);
        processor.toggleChordLock(2);
        check(processor.isChordLocked(0) && processor.isChordLocked(2), "chords can be pinned");
        check(! processor.isChordLocked(1), "and the others are not");

        // A pinned chord does not move, however you push at it.
        processor.nudgeChord(0, 1);
        check(processor.getEngine().analysis().progression[0].chord.root == 9,
              "clicking a pinned chord leaves it alone");

        processor.reharmonize(1.0f);
        const auto& after = processor.getEngine().analysis().progression;
        check(after.size() == 4, "reharmonisation keeps the number of chords");
        check(after[0].chord.root == 9, "the first pinned chord survived");
        check(after[2].chord.root == 0, "and so did the third");
        check(processor.getEngine().analysis().progressionString() != before,
              "while the free ones did change");

        processor.undo();

        // Typing a new progression means new slots, so the old pins go.
        processor.setProgressionText("Dm | Bb | F | C");
        check(! processor.isChordLocked(0), "typing new chords clears the pins");
    }

    // ---- Surprise me ----------------------------------------------------------
    {
        const auto seedBefore = processor.getSeed();

        processor.surpriseMe();
        check(processor.getSeed() != seedBefore, "a surprise rolls a new seed");
        check(processor.getEngine().analysis().valid, "and leaves a valid analysis");
        check(processor.getEngine().analysis().progression.size() >= 4,
              "with a progression to write over");
        check(! processor.getGeneratedSequence().empty(), "and a part written on it");
        check(processor.isKeyForced(), "the key it picked is pinned, so it can be read");

        // Pinned chords survive a surprise too - that is the point of pinning one.
        processor.toggleChordLock(1);
        const auto kept = processor.getEngine().analysis().progression[1].chord;
        processor.surpriseMe();
        check(processor.getEngine().analysis().progression[1].chord.root == kept.root
                  && processor.getEngine().analysis().progression[1].chord.type == kept.type,
              "a pinned chord survives a surprise");
        processor.toggleChordLock(1);
    }

    // ---- Favourites -----------------------------------------------------------
    {
        // The style folder is redirected for this run, so the favourites and
        // their MIDI land in the temp folder with it and no real one is touched.
        processor.reloadFavourites();
        const auto countBefore = processor.favouriteList().size();

        check(processor.saveFavourite(), "a favourite is kept");
        check(processor.favouriteList().size() == countBefore + 1, "and turns up in the list");

        const auto name = processor.favouriteList().back().name;
        check(name.isNotEmpty(), "with a name that says what it is");

        const juce::File folder(processor.favouriteList().back().folder);
        check(folder.isDirectory(), "and a folder of MIDI beside it");
        check(folder.findChildFiles(juce::File::findFiles, false, "*.mid").size() > 0,
              "with a file per part in it");

        // A second instance has to see it: favourites are global, not per project.
        ProgressionsProcessor other;
        other.reloadFavourites();
        bool found = false;
        for (const auto& entry : other.favouriteList())
            found = found || entry.name == name;
        check(found, "another instance sees the same favourites");

        // Recalling one puts the idea back exactly.
        const auto chords = processor.getEngine().analysis().progressionString();
        processor.surpriseMe();
        processor.recallFavourite(static_cast<int>(processor.favouriteList().size()) - 1);
        check(processor.getEngine().analysis().progressionString() == chords,
              "recalling a favourite brings the chords back");

        check(processor.deleteFavourite(static_cast<int>(processor.favouriteList().size()) - 1),
              "a favourite can be forgotten");
        check(processor.favouriteList().size() == countBefore, "and leaves the list as it was");
        check(folder.isDirectory(), "but its MIDI files are left alone");
    }

    // ---- State round trip ----------------------------------------------------
    juce::MemoryBlock state;
    processor.getStateInformation(state);
    check(state.getSize() > 0, "state is written");

    processor.applyPreset("melodic-lift");
    processor.getStateInformation(state);

    ProgressionsProcessor restored;
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
    {
        // A host sizes an imported clip by the end of the file, so it has to be
        // a whole number of bars however the last note happens to fall.
        juce::MemoryBlock block;
        check(exported.loadFileAsData(block), "the exported file reads back");
        std::string error;
        harmonia::NoteSequence readBack;
        check(harmonia::midi::readFromMemory(static_cast<const juce::uint8*>(block.getData()),
                                             block.getSize(), readBack, error),
              "and parses as a MIDI file");
        check(readBack.loopLengthTicks > 0, "the file declares its length");
        check(readBack.loopLengthTicks % readBack.ticksPerBar() == 0,
              "and that length is a whole number of bars");
        for (const auto& note : readBack.notes)
            check(note.endTick() <= readBack.loopLengthTicks, "nothing rings past the loop");
    }
    exported.deleteFile();

    // ---- Editor ---------------------------------------------------------------
    {
        // Pin one chord before rendering, so the picture shows both states of
        // the padlock rather than four identical chips, and open the newest
        // part - the screenshot is what the documentation ships.
        processor.toggleChordLock(1);
        {
            auto* selected = processor.apvts.getParameter(ParamID::part);
            selected->setValueNotifyingHost(
                selected->convertTo0to1(static_cast<float>(harmonia::PartType::Pattern)));
            processor.regenerate();
        }

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

            // Clicking the name opens the about card. Checked because the
            // handler is easy to break in a way nothing else notices.
            auto* progressionsEditor = dynamic_cast<ProgressionsEditor*>(editor.get());
            check(progressionsEditor != nullptr, "the editor is ours");
            if (progressionsEditor != nullptr)
            {
                check(! progressionsEditor->isAboutVisible(), "about starts hidden");

                const auto click = [progressionsEditor](juce::Point<float> where)
                {
                    const juce::MouseEvent event(juce::Desktop::getInstance().getMainMouseSource(),
                                                 where, juce::ModifierKeys(), 1.0f, 0.0f, 0.0f, 0.0f, 0.0f,
                                                 progressionsEditor, progressionsEditor,
                                                 juce::Time::getCurrentTime(), where,
                                                 juce::Time::getCurrentTime(), 1, false);
                    progressionsEditor->mouseDown(event);
                };

                click({ 60.0f, 26.0f });
                check(progressionsEditor->isAboutVisible(), "clicking the name opens it");

                click({ 600.0f, 400.0f });
                check(progressionsEditor->isAboutVisible(),
                      "and a click elsewhere in the editor does not close it behind the overlay");
            }
        }
    }

    // ---- Init ------------------------------------------------------------
    // Last, because it throws away everything the checks above set up.
    {
        processor.setProgressionText("Dm | Bb | F | C");
        processor.apvts.getParameter(ParamID::density)->setValueNotifyingHost(1.0f);
        processor.setForcedKey(true, 2, harmonia::ScaleType::NaturalMinor);
        const int clipsBefore = processor.styleClipCount();

        processor.initialise();

        check(! processor.getEngine().hasSource(), "init drops the clip");
        check(! processor.hasWrittenProgression(), "and the typed progression");
        check(! processor.isKeyForced(), "and unpins the key");
        auto* density = processor.apvts.getParameter(ParamID::density);
        const float densityDefault = density->convertFrom0to1(density->getDefaultValue());
        check(std::abs(processor.apvts.getRawParameterValue(ParamID::density)->load() - densityDefault) < 1.0e-4f,
              "and puts the knobs back");
        check(processor.getSourceName().isEmpty(), "and forgets the clip name");

        // The library is the installation's, not the patch's: rebuilding it
        // costs minutes, so Init must never take it away.
        check(processor.styleClipCount() == clipsBefore, "but keeps the learned library");
        check(processor.getProgressionText().isEmpty(), "and leaves no progression to show");
        check(processor.hasStyleModel() == (clipsBefore > 0), "which is still loaded");
    }

    std::cout << (failures == 0 ? "\nAll plugin smoke checks passed\n"
                                : "\n" + juce::String(failures) + " plugin smoke checks failed\n");
    return failures == 0 ? 0 : 1;
}
