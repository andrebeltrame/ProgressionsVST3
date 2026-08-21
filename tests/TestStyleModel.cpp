#include "Fixtures.h"
#include "TestHarness.h"

#include "harmonia/Engine.h"
#include "harmonia/StyleModel.h"

#include <algorithm>
#include <filesystem>
#include <set>

using namespace harmonia;

namespace
{

/** A clip whose notes only ever land on 16ths 0 and 6 of each bar. */
NoteSequence syncopatedBass(const std::vector<int>& rootPerBar)
{
    auto sequence = fixtures::emptySequence();
    const int64_t bar = kPPQ * 4;
    const int64_t slot = kPPQ / 4;

    for (size_t i = 0; i < rootPerBar.size(); ++i)
    {
        for (int position : { 0, 6 })
        {
            Note note;
            note.startTick = static_cast<int64_t>(i) * bar + position * slot;
            note.lengthTick = slot * 2;
            note.pitch = rootPerBar[i] + (position == 6 ? 12 : 0);
            note.velocity = position == 0 ? 110 : 84;
            sequence.notes.push_back(note);
        }
    }
    sequence.sort();
    return sequence;
}

Analysis analysisFor(const NoteSequence& sequence)
{
    return analyze(sequence);
}

std::set<int> onsetSlots(const NoteSequence& sequence)
{
    std::set<int> slots;
    const int64_t bar = kPPQ * 4;
    const int64_t slot = kPPQ / 4;
    for (const auto& note : sequence.notes)
        slots.insert(static_cast<int>((note.startTick % bar) / slot));
    return slots;
}

GenerateOptions styledOptions(PartType part, const StyleModel& model)
{
    GenerateOptions options;
    options.part = part;
    options.seed = 21;
    options.humanize = 0.0f;
    options.style = &model;
    options.styleAmount = 1.0f;
    return options;
}

} // namespace

TEST(LearningCountsWhatItSees)
{
    StyleModel model;
    CHECK(model.empty());

    const auto clip = syncopatedBass({ 36, 33, 29, 31 });
    learnFromClip(model, clip, analysisFor(clip), "bass");

    CHECK_EQ(model.clipsLearned, 1);
    CHECK_EQ(model.barsLearned, 4);
    CHECK(model.hasPatternsFor(StyleRole::Bass));
    CHECK(! model.hasPatternsFor(StyleRole::Melody));

    // One distinct bar shape: 16ths 0 and 6.
    const auto& bank = model.patterns.at(StyleRole::Bass);
    CHECK_EQ(bank.size(), size_t(1));
    const auto& pattern = bank.begin()->second;
    CHECK_EQ(pattern.count, 4);
    CHECK_EQ(pattern.onsetCount(), 2);
    CHECK(pattern.hasOnset(0));
    CHECK(pattern.hasOnset(6));
    CHECK(! pattern.hasOnset(4));

    CHECK(! model.bassIntervalBySlotClass[0].empty());     // root on the downbeat
    CHECK(! model.bassIntervalBySlotClass[2].empty());     // octave on the 8th offbeat
    CHECK(! model.progressions.empty());
}

TEST(RoleHintBeatsDetection)
{
    StyleModel model;
    const auto clip = fixtures::melodyClip({ 72, 74, 76, 77, 79, 77, 76, 74 });
    learnFromClip(model, clip, analysisFor(clip), "pluck");

    CHECK(model.hasPatternsFor(StyleRole::Pluck));
    CHECK(! model.hasPatternsFor(StyleRole::Melody));
}

TEST(SamplingFollowsTheCounts)
{
    StyleModel model;
    model.stepMarginal[3] = 100;
    model.stepTransitions[3][-1] = 100;

    Rng rng(7);
    for (int i = 0; i < 20; ++i)
        CHECK_EQ(sampleStep(model, 99, rng, 0), 3);   // unseen context falls back to the marginal
    for (int i = 0; i < 20; ++i)
        CHECK_EQ(sampleStep(model, 3, rng, 0), -1);   // seen context uses its own distribution

    CHECK_EQ(sampleStep(StyleModel {}, 0, rng, 42), 42); // nothing learned

    model.bassIntervalBySlotClass[0][7] = 50;
    for (int i = 0; i < 20; ++i)
        CHECK_EQ(sampleBassInterval(model, 0, rng, 0), 7);
    // An empty slot class borrows from one that has data.
    CHECK_EQ(sampleBassInterval(model, 3, rng, 0), 7);
}

TEST(PatternPickingPrefersTheRequestedDensity)
{
    StyleModel model;
    RhythmPattern sparse;
    sparse.onsets = 0b0000000000000001;
    sparse.count = 50;
    RhythmPattern busy;
    busy.onsets = 0b1111111111111111;
    busy.count = 50;
    model.patterns[StyleRole::Bass] = { { sparse.onsets, sparse }, { busy.onsets, busy } };

    Rng rng(3);
    int sparseWins = 0;
    for (int i = 0; i < 60; ++i)
        if (pickPattern(model, StyleRole::Bass, 1.0f, rng)->onsetCount() == 1)
            ++sparseWins;
    CHECK(sparseWins > 50);

    int busyWins = 0;
    for (int i = 0; i < 60; ++i)
        if (pickPattern(model, StyleRole::Bass, 16.0f, rng)->onsetCount() == 16)
            ++busyWins;
    CHECK(busyWins > 50);

    CHECK(pickPattern(model, StyleRole::Melody, 4.0f, rng) == nullptr);
}

TEST(GeneratedBassUsesTheLearnedBars)
{
    StyleModel model;
    for (const auto& roots : { std::vector<int> { 36, 33, 29, 31 }, std::vector<int> { 41, 41, 36, 36 } })
    {
        const auto clip = syncopatedBass(roots);
        learnFromClip(model, clip, analysisFor(clip), "bass");
    }

    Engine engine;
    engine.setSource(fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 },
                                           { 53, 57, 60 }, { 55, 59, 62 } }));

    const auto plain = engine.generate([]{ GenerateOptions o; o.part = PartType::Bass; o.seed = 21; o.humanize = 0.0f; return o; }());
    const auto learned = engine.generate(styledOptions(PartType::Bass, model));

    CHECK(! learned.empty());
    CHECK_EQ(onsetSlots(learned), (std::set<int> { 0, 6 }));
    CHECK(onsetSlots(plain) != onsetSlots(learned));
}

TEST(GeneratedMelodyUsesTheLearnedBars)
{
    StyleModel model;
    // Melodies that only ever land on the last 16th of each beat.
    auto clip = fixtures::emptySequence();
    const int64_t slot = kPPQ / 4;
    const std::vector<int> pitches { 72, 74, 76, 77, 79, 77, 76, 74 };
    for (size_t i = 0; i < pitches.size(); ++i)
        clip.notes.push_back({ static_cast<int64_t>(i) * slot * 2 + slot * 3, slot, pitches[i], 96, 0 });
    clip.sort();
    learnFromClip(model, clip, analysisFor(clip), "lead");

    Engine engine;
    engine.setSource(fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 } }));
    const auto melody = engine.generate(styledOptions(PartType::Melody, model));

    CHECK(! melody.empty());
    for (int slotIndex : onsetSlots(melody))
        CHECK(slotIndex % 2 == 1); // every onset is on an odd 16th, like the corpus
}

TEST(LearnedVoicingsStayInTheChord)
{
    // A corpus of wide major-triad voicings, applied over minor chords: the
    // spacing should carry over, the major third must not.
    StyleModel model;
    auto clip = fixtures::emptySequence();
    const int64_t bar = kPPQ * 4;
    for (int i = 0; i < 2; ++i)
        for (int pitch : { 48, 64, 67, 76 }) // C major, spread over two octaves
            clip.notes.push_back({ static_cast<int64_t>(i) * bar, bar - 20, pitch, 90, 0 });
    clip.sort();
    learnFromClip(model, clip, analysisFor(clip), "pad");
    CHECK(! model.voicings.empty());

    Engine engine;
    std::string error;
    CHECK(engine.setProgressionText("Am | Dm | Em | Am", error));

    auto options = styledOptions(PartType::Pad, model);
    options.complexity = 0.0f;
    const auto pad = engine.generate(options);
    CHECK(! pad.empty());

    for (const auto& note : pad.notes)
    {
        const auto* segment = engine.analysis().chordAt(note.startTick);
        CHECK(segment != nullptr);
        if (segment != nullptr)
            CHECK(segment->chord.containsPitchClass(note.pitch));
    }
}

TEST(StyleAmountZeroIgnoresTheCorpus)
{
    StyleModel model;
    const auto clip = syncopatedBass({ 36, 33, 29, 31 });
    learnFromClip(model, clip, analysisFor(clip), "bass");

    Engine engine;
    engine.setSource(fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 } }));

    auto options = styledOptions(PartType::Bass, model);
    options.styleAmount = 0.0f;
    const auto ignored = engine.generate(options);

    auto plain = options;
    plain.style = nullptr;
    CHECK_EQ(onsetSlots(ignored), onsetSlots(engine.generate(plain)));
}

TEST(ModelSurvivesSaveAndLoad)
{
    StyleModel model;
    for (const auto& roots : { std::vector<int> { 36, 33 }, std::vector<int> { 41, 36 } })
    {
        const auto clip = syncopatedBass(roots);
        learnFromClip(model, clip, analysisFor(clip), "bass");
    }
    const auto pad = fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 } });
    learnFromClip(model, pad, analysisFor(pad), "pad");

    const auto path = (std::filesystem::temp_directory_path() / "harmonia_test_style.json").string();
    std::string error;
    CHECK(saveStyleModel(path, model, error));
    CHECK(error.empty());

    StyleModel loaded;
    CHECK(loadStyleModel(path, loaded, error));
    CHECK_EQ(loaded.clipsLearned, model.clipsLearned);
    CHECK_EQ(loaded.barsLearned, model.barsLearned);
    CHECK_EQ(loaded.stepMarginal.size(), model.stepMarginal.size());
    CHECK_EQ(loaded.voicings.size(), model.voicings.size());
    CHECK_EQ(loaded.progressions.size(), model.progressions.size());
    CHECK_EQ(loaded.patterns.at(StyleRole::Bass).size(), model.patterns.at(StyleRole::Bass).size());

    const auto& original = model.patterns.at(StyleRole::Bass).begin()->second;
    const auto& restored = loaded.patterns.at(StyleRole::Bass).begin()->second;
    CHECK_EQ(restored.onsets, original.onsets);
    CHECK_EQ(restored.count, original.count);
    CHECK_EQ(restored.velocity[0], original.velocity[0]);

    std::filesystem::remove(path);
}

TEST(PruningKeepsTheCommonMaterial)
{
    StyleModel model;
    for (int i = 0; i < 40; ++i)
    {
        RhythmPattern pattern;
        pattern.onsets = static_cast<uint16_t>(1u << (i % 16)) | static_cast<uint16_t>(1u << (i % 13));
        pattern.count = i;
        model.patterns[StyleRole::Bass][pattern.onsets] = pattern;
    }
    for (int i = 0; i < 40; ++i)
        model.progressions["loop " + std::to_string(i)] = i;

    model.prune(5, 5, 5);
    CHECK(model.patterns[StyleRole::Bass].size() <= size_t(5));
    CHECK(model.progressions.size() <= size_t(5));

    int lowest = 1000;
    for (const auto& [mask, pattern] : model.patterns[StyleRole::Bass])
        lowest = std::min(lowest, pattern.count);
    CHECK(lowest >= 30); // the rare bars were dropped, not the common ones
}

TEST(MinedProgressionsAreRanked)
{
    StyleModel model;
    for (int i = 0; i < 3; ++i)
    {
        const auto clip = fixtures::chordClip({ { 48, 60, 64, 67 }, { 43, 55, 59, 62 } });
        learnFromClip(model, clip, analysisFor(clip), "pad");
    }
    const auto other = fixtures::chordClip({ { 45, 57, 60, 64 }, { 41, 53, 57, 60 } });
    learnFromClip(model, other, analysisFor(other), "pad");

    const auto top = model.topProgressions(5);
    CHECK(top.size() >= 2u);
    CHECK_EQ(top.front().second, 3);
    CHECK(top.front().first.find("I") != std::string::npos);
}
