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
                           PartType::CounterMelody, PartType::Bass, PartType::Reese,
                           PartType::Arp, PartType::Pluck })
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

TEST(NoPartRingsPastTheLoop)
{
    auto engine = engineFromBassLine();
    for (PartType part : { PartType::Pad, PartType::Chords, PartType::Melody,
                           PartType::CounterMelody, PartType::Bass, PartType::Reese,
                           PartType::Arp, PartType::Pluck })
    {
        const auto sequence = engine.generate(cleanOptions(part));
        CHECK(sequence.loopLengthTicks > 0);
        // A whole number of bars, and nothing sounding past the last one - that
        // is what stops a host importing a four-bar loop as a five-bar clip.
        CHECK(sequence.loopLengthTicks % sequence.ticksPerBar() == 0);
        for (const auto& note : sequence.notes)
            CHECK(note.endTick() <= sequence.loopLengthTicks);
    }
}

TEST(ArpRespondsToANewSeed)
{
    auto engine = engineFromBassLine();

    // Every pattern, not just Random: the fixed runs used to be fully determined
    // by the chord, so "New idea" left the arp exactly as it was.
    for (ArpPattern pattern : { ArpPattern::Up, ArpPattern::Down, ArpPattern::UpDown,
                                ArpPattern::DownUp, ArpPattern::Converge, ArpPattern::Random })
    {
        auto options = cleanOptions(PartType::Arp);
        options.arpPattern = pattern;

        options.seed = 42;
        const auto first = engine.generate(options);
        options.seed = 43;
        const auto second = engine.generate(options);

        CHECK(! first.empty() && ! second.empty());
        CHECK(pitches(first) != pitches(second));

        // And still deterministic: the same seed has to give the same notes.
        options.seed = 42;
        CHECK(pitches(engine.generate(options)) == pitches(first));
    }
}

TEST(PartsWrittenAgainstAPadStayOutOfItsWay)
{
    auto engine = engineFromBassLine();
    const auto pad = engine.generate(cleanOptions(PartType::Pad));
    CHECK(! pad.empty());

    const auto topAt = [&pad](int64_t tick)
    {
        int top = -1;
        for (const auto& note : pad.notes)
            if (note.startTick <= tick && tick < note.endTick())
                top = std::max(top, note.pitch);
        return top;
    };

    // How often a line either doubles the pad's top voice or sits a semitone
    // under it - the two ways a melody disappears into the chord it is over.
    const auto clashes = [&topAt](const NoteSequence& part)
    {
        int count = 0;
        for (const auto& note : part.notes)
        {
            const int top = topAt(note.startTick);
            if (top >= 0 && (note.pitch == top || mod12(note.pitch - top) == 1))
                ++count;
        }
        return count;
    };

    int alone = 0;
    int overPad = 0;
    int different = 0;
    for (uint32_t seed = 1; seed <= 8; ++seed)
    {
        auto options = cleanOptions(PartType::Melody);
        options.seed = seed;
        const auto without = engine.generate(options);
        options.anchor = &pad;
        const auto with = engine.generate(options);

        CHECK(! with.empty());
        if (pitches(without) != pitches(with))
            ++different;
        alone += clashes(without);
        overPad += clashes(with);
    }

    // Reading the pad has to change the line, or the anchor does nothing at all.
    CHECK(different == 8);
    CHECK(overPad < alone);
}

TEST(ReeseIsLegatoSoASynthCanGlide)
{
    auto engine = engineFromBassLine();
    const auto reese = engine.generate(cleanOptions(PartType::Reese));
    CHECK(! reese.empty());

    // Every note has to start before the one before it ends. A mono synth only
    // portamentos between overlapping notes, so a gap here is a Reese that can
    // never glide however the synth is set up.
    for (size_t i = 1; i < reese.notes.size(); ++i)
    {
        const auto& previous = reese.notes[i - 1];
        const auto& current = reese.notes[i];
        CHECK(current.startTick < previous.endTick());
    }

    // Held, not plucked: nothing shorter than a beat.
    for (const auto& note : reese.notes)
        CHECK(note.lengthTick >= reese.ppq);

    // And it stays under the plucked bass, where a Reese belongs.
    const auto bass = engine.generate(cleanOptions(PartType::Bass));
    const auto average = [](const NoteSequence& part)
    {
        int total = 0;
        for (const auto& note : part.notes)
            total += note.pitch;
        return part.notes.empty() ? 0 : total / static_cast<int>(part.notes.size());
    };
    CHECK(average(reese) < average(bass));
}

TEST(ReeseMovesWithoutWandering)
{
    auto engine = engineFromBassLine();

    // It has to answer a new seed like everything else...
    auto options = cleanOptions(PartType::Reese);
    options.density = 0.8f;
    options.seed = 7;
    const auto first = engine.generate(options);
    options.seed = 8;
    const auto second = engine.generate(options);
    CHECK(! first.empty() && ! second.empty());

    // ...but stay a bed: every note is a chord tone of the chord under it.
    const auto& analysis = engine.analysis();
    for (const auto& part : { first, second })
    {
        for (const auto& note : part.notes)
        {
            const ChordSegment* segment = nullptr;
            for (const auto& candidate : analysis.progression)
                if (note.startTick >= candidate.startTick && note.startTick < candidate.endTick())
                    segment = &candidate;
            if (segment == nullptr)
                continue;
            CHECK(segment->chord.containsPitchClass(note.pitch)
                  || analysis.key.contains(mod12(note.pitch)));
        }
    }
}

TEST(ReeseWithoutGlideNeverOverlaps)
{
    auto engine = engineFromBassLine();
    auto options = cleanOptions(PartType::Reese);
    options.glide = false;
    const auto reese = engine.generate(options);
    CHECK(! reese.empty());

    // The whole reason the switch exists: on a polyphonic patch two low notes
    // sounding together sum and fight in the low end. With glide off nothing
    // may ever overlap - not by one tick.
    for (size_t i = 1; i < reese.notes.size(); ++i)
        CHECK(reese.notes[i].startTick >= reese.notes[i - 1].endTick());

    // Still a Reese and not a plucked bass: held notes only.
    for (const auto& note : reese.notes)
        CHECK(note.lengthTick >= reese.ppq);
}

TEST(ReeseVariesItsNoteLengths)
{
    // One held note a bar forever is what this used to be. Across a handful of
    // seeds the part has to produce more than a single note length, or the
    // "shorter and longer notes" it claims are not there.
    auto engine = engineFromBassLine();
    std::set<int64_t> lengths;
    for (uint32_t seed = 1; seed <= 8; ++seed)
    {
        auto options = cleanOptions(PartType::Reese);
        options.seed = seed;
        options.density = 0.7f;
        for (const auto& note : engine.generate(options).notes)
            lengths.insert(note.lengthTick);
    }
    CHECK(lengths.size() >= 3);
}

TEST(LockedChordsSurviveAReharmonisation)
{
    auto engine = engineFromBassLine();
    const auto before = engine.analysis().progression;
    CHECK(before.size() >= 4);

    // Lock the first and the third; everything is allowed to move.
    std::vector<bool> locked(before.size(), false);
    locked[0] = true;
    locked[2] = true;

    engine.applyReharmonization(7u, 1.0f, locked);
    const auto after = engine.analysis().progression;
    CHECK(after.size() == before.size());

    CHECK(after[0].chord.root == before[0].chord.root);
    CHECK(after[0].chord.type == before[0].chord.type);
    CHECK(after[2].chord.root == before[2].chord.root);
    CHECK(after[2].chord.type == before[2].chord.type);

    // And it still did the job on the ones that were free.
    const bool movedSomething = after[1].chord.root != before[1].chord.root
                             || after[1].chord.type != before[1].chord.type
                             || after[3].chord.root != before[3].chord.root
                             || after[3].chord.type != before[3].chord.type;
    CHECK(movedSomething);
}

TEST(ReeseNeverStacksAPitchOnItself)
{
    // Two overlapping note-ons for the same pitch is one voice with two ons and
    // two offs: the first off cuts it short, and the glide it was meant to make
    // is a glide from a note to itself. Held longer is the only right answer.
    auto engine = engineFromBassLine();
    for (uint32_t seed = 1; seed <= 24; ++seed)
    {
        auto options = cleanOptions(PartType::Reese);
        options.seed = seed;
        options.density = 0.85f;
        options.complexity = 0.85f;
        const auto reese = engine.generate(options);

        for (size_t i = 0; i < reese.notes.size(); ++i)
            for (size_t j = i + 1; j < reese.notes.size(); ++j)
                if (reese.notes[i].pitch == reese.notes[j].pitch)
                    CHECK(reese.notes[j].startTick >= reese.notes[i].endTick()
                          || reese.notes[i].startTick >= reese.notes[j].endTick());
    }
}

TEST(SubIsOneVoiceOnTheRoot)
{
    auto engine = engineFromBassLine();

    for (uint32_t seed = 1; seed <= 12; ++seed)
    {
        auto options = cleanOptions(PartType::Sub);
        options.seed = seed;
        options.density = 0.9f;      // the busiest it will ever be
        options.complexity = 1.0f;
        const auto subPart = engine.generate(options);
        CHECK(! subPart.empty());

        const auto& analysis = engine.analysis();
        for (size_t i = 0; i < subPart.notes.size(); ++i)
        {
            const auto& note = subPart.notes[i];

            // The root of whatever chord is sounding, never a third or a fifth.
            const auto* segment = analysis.chordAt(note.startTick);
            CHECK(segment != nullptr);
            if (segment != nullptr)
                CHECK(note.pitch % 12 == segment->chord.root % 12);

            // Never a kick: nothing shorter than a beat.
            CHECK(note.lengthTick >= subPart.ppq);

            // Never two at once. This is the rule the whole part exists for -
            // two subs sounding together sum into a low end nobody can mix.
            if (i > 0)
                CHECK(note.startTick >= subPart.notes[i - 1].endTick());
        }

        // One octave and no more: a sub that ranges has stopped being a sub.
        int low = 127, high = 0;
        for (const auto& note : subPart.notes)
        {
            low = std::min(low, note.pitch);
            high = std::max(high, note.pitch);
        }
        CHECK(high - low <= 12);
    }
}

TEST(SubSitsUnderTheBassAndTheReese)
{
    auto engine = engineFromBassLine();
    const auto average = [](const NoteSequence& part)
    {
        int total = 0;
        for (const auto& note : part.notes)
            total += note.pitch;
        return part.notes.empty() ? 0 : total / static_cast<int>(part.notes.size());
    };

    const auto subPart = engine.generate(cleanOptions(PartType::Sub));
    CHECK(average(subPart) < average(engine.generate(cleanOptions(PartType::Reese))));
    CHECK(average(subPart) < average(engine.generate(cleanOptions(PartType::Bass))));
}

TEST(SubHoldsWhenItIsToldTo)
{
    // Density is the only knob that does anything here: at the bottom it is one
    // held note per chord, and turned up it strikes the root again on the bar.
    auto engine = engineFromBassLine();

    auto still = cleanOptions(PartType::Sub);
    still.density = 0.0f;
    const auto held = engine.generate(still);

    size_t busiest = held.notes.size();
    for (uint32_t seed = 1; seed <= 8; ++seed)
    {
        auto moving = cleanOptions(PartType::Sub);
        moving.seed = seed;
        moving.density = 1.0f;
        busiest = std::max(busiest, engine.generate(moving).notes.size());
    }

    CHECK(held.notes.size() == engine.analysis().progression.size());
    CHECK(busiest > held.notes.size());
}
