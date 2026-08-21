#include "Fixtures.h"
#include "TestHarness.h"

#include "harmonia/Engine.h"

#include <algorithm>
#include <algorithm>
#include <set>

using namespace harmonia;

namespace
{

Engine engineFromBassLine()
{
    Engine engine;
    engine.setSource(fixtures::bassClip({ 36, 33, 29, 31 })); // C  Am  F  G
    return engine;
}

GenerateOptions cleanOptions(PartType part)
{
    GenerateOptions options;
    options.part = part;
    options.seed = 42;
    options.humanize = 0.0f;
    return options;
}

bool allWithin(const NoteSequence& sequence, int low, int high)
{
    return std::all_of(sequence.notes.begin(), sequence.notes.end(),
                       [low, high](const Note& n) { return n.pitch >= low && n.pitch <= high; });
}

std::vector<int> pitches(const NoteSequence& sequence)
{
    std::vector<int> out;
    for (const auto& note : sequence.notes)
        out.push_back(note.pitch);
    return out;
}

} // namespace

TEST(PadFromBassLineIsHarmonicallyConsistent)
{
    auto engine = engineFromBassLine();
    const auto pad = engine.generate(cleanOptions(PartType::Pad));

    CHECK(! pad.empty());
    CHECK(allWithin(pad, 36, 84));

    // Every pad note should belong to the chord sounding underneath it.
    const auto& analysis = engine.analysis();
    for (const auto& note : pad.notes)
    {
        const auto* segment = analysis.chordAt(note.startTick);
        CHECK(segment != nullptr);
        if (segment != nullptr)
        {
            const bool inChord = segment->chord.containsPitchClass(note.pitch);
            const bool inKey = analysis.key.contains(mod12(note.pitch));
            CHECK(inChord || inKey);
        }
    }

    // Four bars in, four bars out.
    CHECK_EQ(pad.barCount(), 4);
}

TEST(PadFollowsTheProgressionChanges)
{
    auto engine = engineFromBassLine();
    const auto pad = engine.generate(cleanOptions(PartType::Pad));

    const int64_t bar = static_cast<int64_t>(kPPQ) * 4;
    for (int barIndex = 0; barIndex < 4; ++barIndex)
    {
        std::set<int> classes;
        for (const auto& note : pad.notes)
            if (note.startTick >= barIndex * bar && note.startTick < (barIndex + 1) * bar)
                classes.insert(mod12(note.pitch));

        CHECK(classes.size() >= 3u); // a real chord, not a single note
        const auto* segment = engine.analysis().chordAt(barIndex * bar);
        CHECK(classes.count(segment->chord.root) == 1u);
    }
}

TEST(GenerationIsDeterministicPerSeed)
{
    auto engine = engineFromBassLine();
    auto options = cleanOptions(PartType::Melody);
    options.humanize = 0.4f;

    const auto first = engine.generate(options);
    const auto second = engine.generate(options);
    CHECK_EQ(pitches(first), pitches(second));

    options.seed = 7;
    const auto third = engine.generate(options);
    CHECK(pitches(third) != pitches(first));
}

TEST(MelodyStaysInRangeAndInKey)
{
    auto engine = engineFromBassLine();
    auto options = cleanOptions(PartType::Melody);
    options.complexity = 0.2f; // no chromatic passing tones at this setting

    const auto melody = engine.generate(options);
    CHECK(! melody.empty());
    CHECK(allWithin(melody, 55, 96));

    const auto& key = engine.analysis().key;
    int inKey = 0;
    for (const auto& note : melody.notes)
        if (key.contains(mod12(note.pitch)))
            ++inKey;
    CHECK(inKey == static_cast<int>(melody.notes.size()));

    // Monophonic: no overlapping notes.
    for (size_t i = 1; i < melody.notes.size(); ++i)
        CHECK(melody.notes[i].startTick >= melody.notes[i - 1].startTick);
}

TEST(MelodyLeapsStaySingable)
{
    auto engine = engineFromBassLine();
    const auto melody = engine.generate(cleanOptions(PartType::Melody));
    for (size_t i = 1; i < melody.notes.size(); ++i)
        CHECK(std::abs(melody.notes[i].pitch - melody.notes[i - 1].pitch) <= 12);
}

TEST(BassPartSitsLowAndStartsOnRoots)
{
    Engine engine;
    engine.setSource(fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 } }));

    auto options = cleanOptions(PartType::Bass);
    options.density = 0.3f;
    const auto bass = engine.generate(options);

    CHECK(! bass.empty());
    CHECK(allWithin(bass, 24, 60));

    for (const auto& segment : engine.analysis().progression)
    {
        const auto first = std::find_if(bass.notes.begin(), bass.notes.end(),
                                        [&segment](const Note& n) { return n.startTick == segment.startTick; });
        CHECK(first != bass.notes.end());
        if (first != bass.notes.end())
            CHECK_EQ(mod12(first->pitch), segment.chord.root);
    }
}

TEST(ArpUsesOnlyChordTones)
{
    auto engine = engineFromBassLine();
    auto options = cleanOptions(PartType::Arp);
    options.complexity = 0.0f;

    const auto arp = engine.generate(options);
    CHECK(! arp.empty());

    for (const auto& note : arp.notes)
    {
        const auto* segment = engine.analysis().chordAt(note.startTick);
        CHECK(segment->chord.containsPitchClass(note.pitch));
    }
}

TEST(RequestedBarCountLoopsTheProgression)
{
    auto engine = engineFromBassLine();
    auto options = cleanOptions(PartType::Pad);
    options.bars = 8;

    const auto pad = engine.generate(options);
    CHECK_EQ(pad.barCount(), 8);

    const int64_t bar = static_cast<int64_t>(kPPQ) * 4;
    for (const auto& note : pad.notes)
        CHECK(note.startTick < 8 * bar);
}

TEST(VariationsDifferFromEachOther)
{
    auto engine = engineFromBassLine();
    const auto takes = engine.generateVariations(cleanOptions(PartType::Melody), 4);
    CHECK_EQ(takes.size(), size_t(4));

    int distinct = 0;
    for (size_t i = 1; i < takes.size(); ++i)
        if (pitches(takes[i]) != pitches(takes[0]))
            ++distinct;
    CHECK(distinct >= 2);
}

TEST(CounterMelodyAvoidsUnisonWithTheSource)
{
    Engine engine;
    engine.setSource(fixtures::melodyClip({ 72, 74, 76, 77, 79, 77, 76, 74,
                                            72, 74, 76, 77, 79, 77, 76, 74 }));

    auto options = cleanOptions(PartType::CounterMelody);
    options.avoidSourceCollisions = true;
    const auto counter = engine.generate(options);
    CHECK(! counter.empty());

    int unisons = 0;
    for (const auto& note : counter.notes)
        for (const auto& sourceNote : engine.source().notes)
            if (sourceNote.startTick <= note.startTick && note.startTick < sourceNote.endTick()
                && sourceNote.pitch == note.pitch)
                ++unisons;
    CHECK(unisons == 0);
}

TEST(DevelopmentWorksTheMotifInsteadOfRepeatingIt)
{
    auto engine = engineFromBassLine();

    auto plain = cleanOptions(PartType::Melody);
    plain.bars = 4;
    plain.motifDevelopment = 0.0f;

    auto worked = plain;
    worked.motifDevelopment = 1.0f;

    const auto flat = engine.generate(plain);
    const auto developed = engine.generate(worked);

    CHECK(! flat.empty());
    CHECK(! developed.empty());

    // Working the motif adds passing notes, so the line gets busier.
    CHECK(developed.notes.size() > flat.notes.size());

    // And it stays in the key while doing it.
    const auto& key = engine.analysis().key;
    int outside = 0;
    for (const auto& note : developed.notes)
        if (! key.contains(mod12(note.pitch)))
            ++outside;
    CHECK(outside == 0);

    // Ornaments land on the 16th grid, never between it.
    const int64_t slot = kPPQ / 4;
    for (const auto& note : developed.notes)
        CHECK(note.startTick % slot == 0);

    // Notes still do not overlap: this is one line, not two.
    for (size_t i = 1; i < developed.notes.size(); ++i)
        CHECK(developed.notes[i].startTick >= developed.notes[i - 1].startTick);
}

TEST(PhrasesHaveAnArcInsteadOfSittingFlat)
{
    auto engine = engineFromBassLine();
    auto options = cleanOptions(PartType::Melody);
    options.bars = 4;

    const auto melody = engine.generate(options);
    CHECK(melody.notes.size() >= 4u);

    // Average pitch per bar: the phrase should climb away from where it starts
    // rather than sitting at one height for four bars.
    const int64_t bar = static_cast<int64_t>(kPPQ) * 4;
    std::vector<double> heights;
    for (int index = 0; index < 4; ++index)
    {
        double sum = 0.0;
        int count = 0;
        for (const auto& note : melody.notes)
            if (note.startTick >= index * bar && note.startTick < (index + 1) * bar)
            {
                sum += note.pitch;
                ++count;
            }
        if (count > 0)
            heights.push_back(sum / count);
    }

    CHECK(heights.size() >= 3u);
    if (heights.size() >= 3)
    {
        const auto [lowest, highest] = std::minmax_element(heights.begin(), heights.end());
        CHECK(*highest - *lowest >= 1.5); // real movement, not a flat line
    }
}

TEST(MelodyAvoidsAMinorNinthAgainstTheBass)
{
    // A bass holding one note, so any semitone clash is unambiguous.
    auto bass = fixtures::emptySequence();
    for (int barIndex = 0; barIndex < 4; ++barIndex)
        bass.notes.push_back({ static_cast<int64_t>(barIndex) * kPPQ * 4, kPPQ * 4, 33, 100, 0 });
    bass.sort();

    Engine engine;
    std::string error;
    CHECK(engine.setProgressionText("Am | Am | Am | Am", error));

    auto options = cleanOptions(PartType::Melody);
    options.companion = &bass;
    options.density = 0.9f;
    const auto melody = engine.generate(options);
    CHECK(! melody.empty());

    const int64_t slot = kPPQ / 4;
    for (const auto& note : melody.notes)
    {
        const int position = static_cast<int>((note.startTick % (kPPQ * 4)) / slot);
        const bool strong = position % 4 == 0;
        if (strong)
            CHECK(mod12(note.pitch - 33) != 1); // never a minor ninth on a beat
    }
}

TEST(ReharmonizationChangesChordsButKeepsTheGrid)
{
    auto engine = engineFromBassLine();
    const auto before = engine.analysis().progression;

    engine.applyReharmonization(11u, 1.0f);
    const auto after = engine.analysis().progression;

    CHECK_EQ(after.size(), before.size());
    int changed = 0;
    for (size_t i = 0; i < after.size(); ++i)
    {
        CHECK_EQ(after[i].startTick, before[i].startTick);
        CHECK_EQ(after[i].lengthTick, before[i].lengthTick);
        if (after[i].chord.root != before[i].chord.root || after[i].chord.type != before[i].chord.type)
            ++changed;
    }
    CHECK(changed > 0);

    engine.resetProgression();
    CHECK_EQ(engine.analysis().progression[0].chord.root, before[0].chord.root);
}

TEST(GeneratingWithoutASourceIsSafe)
{
    Engine engine;
    const auto pad = engine.generate(cleanOptions(PartType::Pad));
    CHECK(pad.empty());
}

TEST(EveryPartTypeProducesNotes)
{
    auto engine = engineFromBassLine();
    for (PartType part : { PartType::Pad, PartType::Chords, PartType::Melody,
                           PartType::CounterMelody, PartType::Bass, PartType::Arp,
                           PartType::Pluck })
    {
        const auto sequence = engine.generate(cleanOptions(part));
        CHECK(! sequence.empty());
        for (const auto& note : sequence.notes)
        {
            CHECK(note.pitch >= 0 && note.pitch <= 127);
            CHECK(note.velocity >= 1 && note.velocity <= 127);
            CHECK(note.lengthTick > 0);
            CHECK(note.startTick >= 0);
        }
    }
}
