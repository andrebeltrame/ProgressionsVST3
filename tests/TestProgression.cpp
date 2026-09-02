#include "Fixtures.h"
#include "TestHarness.h"

#include "harmonia/Engine.h"
#include "harmonia/Presets.h"
#include "harmonia/Progression.h"

#include <set>

using namespace harmonia;

namespace
{
std::vector<int> roots(const std::vector<Chord>& chords)
{
    std::vector<int> out;
    for (const auto& chord : chords)
        out.push_back(chord.root);
    return out;
}

std::vector<Chord> chordsOf(const Analysis& analysis)
{
    std::vector<Chord> out;
    for (const auto& segment : analysis.progression)
        out.push_back(segment.chord);
    return out;
}

std::vector<int> progressionRoots(const Analysis& analysis)
{
    std::vector<int> out;
    for (const auto& segment : analysis.progression)
        out.push_back(segment.chord.root);
    return out;
}
} // namespace

TEST(ChordSymbolParsing)
{
    Chord chord;

    CHECK(parseChordSymbol("C", chord));
    CHECK_EQ(chord.name(), std::string("C"));

    CHECK(parseChordSymbol("Am", chord));
    CHECK_EQ(chord.name(), std::string("Am"));

    CHECK(parseChordSymbol("F#m7b5", chord));
    CHECK_EQ(chord.name(), std::string("F#m7b5"));

    CHECK(parseChordSymbol("Bbmaj9", chord));
    CHECK_EQ(chord.root, 10);
    CHECK(chord.type == ChordType::Major9);

    CHECK(parseChordSymbol("Gsus4", chord));
    CHECK(chord.type == ChordType::Sus4);

    CHECK(parseChordSymbol("Cmaj7/G", chord));
    CHECK_EQ(chord.name(), std::string("Cmaj7/G"));

    CHECK(parseChordSymbol("dm7", chord)); // lower-case root letters are fine
    CHECK_EQ(chord.name(), std::string("Dm7"));

    CHECK(! parseChordSymbol("H", chord));
    CHECK(! parseChordSymbol("Cwobble", chord));
    CHECK(! parseChordSymbol("", chord));
}

TEST(RomanNumeralParsingUsesTheKeyScale)
{
    const Key aMinor { 9, ScaleType::NaturalMinor };
    Chord chord;

    CHECK(parseRomanNumeral("i", aMinor, chord));
    CHECK_EQ(chord.name(), std::string("Am"));

    CHECK(parseRomanNumeral("VI", aMinor, chord));
    CHECK_EQ(chord.name(), std::string("F"));

    CHECK(parseRomanNumeral("III", aMinor, chord));
    CHECK_EQ(chord.name(), std::string("C"));

    CHECK(parseRomanNumeral("VII", aMinor, chord));
    CHECK_EQ(chord.name(), std::string("G"));

    // An uppercase numeral means a major chord even where the key says otherwise.
    CHECK(parseRomanNumeral("V", aMinor, chord));
    CHECK_EQ(chord.name(), std::string("E"));

    CHECK(parseRomanNumeral("iv7", aMinor, chord));
    CHECK_EQ(chord.name(), std::string("Dm7"));

    const Key cMajor { 0, ScaleType::Major };
    CHECK(parseRomanNumeral("bVII", cMajor, chord));
    CHECK_EQ(chord.name(), std::string("A#"));
    CHECK(parseRomanNumeral("V7", cMajor, chord));
    CHECK_EQ(chord.name(), std::string("G7"));
    CHECK(parseRomanNumeral("ii7", cMajor, chord));
    CHECK_EQ(chord.name(), std::string("Dm7"));

    CHECK(! parseRomanNumeral("iV", cMajor, chord)); // mixed case is a typo
    CHECK(! parseRomanNumeral("VIII", cMajor, chord));
}

TEST(NumeralsRoundTripThroughTheirPrinter)
{
    const Key aMinor { 9, ScaleType::NaturalMinor };
    for (const char* text : { "i", "VI", "III", "VII", "iv7", "v7", "VImaj7", "isus2" })
    {
        Chord chord;
        CHECK(parseRomanNumeral(text, aMinor, chord));
        CHECK_EQ(chord.romanNumeral(aMinor), std::string(text));
    }
}

TEST(ProgressionParsingAcceptsTheUsualSeparators)
{
    const Key aMinor { 9, ScaleType::NaturalMinor };
    std::vector<Chord> chords;
    std::string error;

    CHECK(parseProgression("Am | F | C | G", aMinor, chords, error));
    CHECK_EQ(roots(chords), (std::vector<int> { 9, 5, 0, 7 }));

    CHECK(parseProgression("Am F C G", aMinor, chords, error));
    CHECK_EQ(roots(chords), (std::vector<int> { 9, 5, 0, 7 }));

    CHECK(parseProgression("i - VI - III - VII", aMinor, chords, error));
    CHECK_EQ(roots(chords), (std::vector<int> { 9, 5, 0, 7 }));

    CHECK(parseProgression("Am,F,C,G", aMinor, chords, error));
    CHECK_EQ(chords.size(), size_t(4));

    // Symbols and numerals can be mixed.
    CHECK(parseProgression("i | F | III | G7", aMinor, chords, error));
    CHECK_EQ(progressionToText(chords), std::string("Am | F | C | G7"));

    CHECK(! parseProgression("Am | wat | C", aMinor, chords, error));
    CHECK(error.find("wat") != std::string::npos);
    CHECK(chords.empty());

    CHECK(! parseProgression("   ", aMinor, chords, error));
}

TEST(KeyGuessFromAWrittenProgression)
{
    const Key aMinor { 9, ScaleType::NaturalMinor };
    std::vector<Chord> chords;
    std::string error;

    CHECK(parseProgression("Am F C G", aMinor, chords, error));
    const Key guessed = guessKeyForProgression(chords);
    CHECK_EQ(guessed.tonic, 9);
    CHECK(guessed.isMinorMode());

    CHECK(parseProgression("C Am F G", aMinor, chords, error));
    CHECK_EQ(guessKeyForProgression(chords).tonic, 0);
    CHECK(! guessKeyForProgression(chords).isMinorMode());
}

TEST(AnalysisBuiltFromChordsAlone)
{
    const Key aMinor { 9, ScaleType::NaturalMinor };
    std::vector<Chord> chords;
    std::string error;
    CHECK(parseProgression("Am | F | C | G", aMinor, chords, error));

    const auto analysis = analysisFromProgression(chords, aMinor, 124.0);
    CHECK(analysis.valid);
    CHECK_EQ(analysis.bars, 4);
    CHECK_NEAR(analysis.bpm, 124.0, 0.001);
    CHECK_EQ(analysis.progression.size(), size_t(4));
    CHECK_EQ(analysis.lengthTicks, static_cast<int64_t>(kPPQ) * 16);
    for (const auto& segment : analysis.progression)
        CHECK_EQ(segment.lengthTick, static_cast<int64_t>(kPPQ) * 4);

    // Generators work off it with no source clip at all.
    GenerateOptions options;
    options.part = PartType::Pad;
    options.humanize = 0.0f;
    const auto pad = generate(analysis, NoteSequence {}, options);
    CHECK(! pad.empty());
    CHECK_EQ(pad.barCount(), 4);
}

TEST(WrittenProgressionKeepsTheClipGroove)
{
    Engine engine;
    engine.setSource(fixtures::bassClip({ 36, 33, 29, 31 }));
    const auto detectedBpm = engine.analysis().bpm;
    const auto detectedBars = engine.analysis().bars;

    std::string error;
    CHECK(engine.setProgressionText("Dm7 | G7 | Cmaj7 | Cmaj7", error));
    CHECK(error.empty());
    CHECK(engine.hasWrittenProgression());

    const auto& analysis = engine.analysis();
    CHECK_EQ(progressionRoots(analysis), (std::vector<int> { 2, 7, 0, 0 }));
    CHECK_NEAR(analysis.bpm, detectedBpm, 0.001);
    CHECK_EQ(analysis.bars, detectedBars);
    CHECK(analysis.role == SourceRole::Bass); // the clip is still a bass line

    // Chord changes land on bar lines for a four-chord, four-bar clip.
    for (size_t i = 0; i < analysis.progression.size(); ++i)
        CHECK_EQ(analysis.progression[i].startTick, static_cast<int64_t>(i) * kPPQ * 4);

    const auto pad = engine.generate({});
    CHECK(! pad.empty());

    engine.resetProgression();
    CHECK(! engine.hasWrittenProgression());
    CHECK_EQ(progressionRoots(engine.analysis()), (std::vector<int> { 0, 9, 5, 7 }));
}

TEST(WrittenProgressionWithNoClip)
{
    Engine engine;
    std::string error;
    CHECK(engine.setProgressionText("Fm | Db | Ab | Eb", error));
    CHECK(engine.analysis().valid);
    CHECK_EQ(engine.analysis().key.tonic, 5);
    CHECK(engine.analysis().key.isMinorMode());
    CHECK_EQ(engine.analysis().bars, 4);

    GenerateOptions options;
    options.part = PartType::Arp;
    options.complexity = 0.0f; // triads only, so the check below is exact
    options.humanize = 0.0f;   // and no jitter across the chord boundaries
    const auto arp = engine.generate(options);
    CHECK(! arp.empty());
    for (const auto& note : arp.notes)
    {
        const auto* segment = engine.analysis().chordAt(note.startTick);
        CHECK(segment->chord.containsPitchClass(note.pitch));
    }
}

TEST(PresetsAreAllParsable)
{
    CHECK(progressionPresets().size() >= 15u);

    for (const auto& preset : progressionPresets())
    {
        const Key key { 9, preset.mode };
        std::vector<Chord> chords;
        std::string error;
        const bool parsed = parseProgression(preset.numerals, key, chords, error);
        if (! parsed)
            ::testing::reportFailure(__FILE__, __LINE__, preset.id + ": " + error);
        CHECK(chords.size() >= 2u);
        CHECK(! preset.name.empty());
        CHECK(! preset.style.empty());
    }
}

TEST(ApplyingAPresetKeepsTheTonic)
{
    Engine engine;
    engine.setSource(fixtures::bassClip({ 36, 33, 29, 31 })); // A minor
    CHECK_EQ(engine.analysis().key.tonic, 0);

    std::string error;
    CHECK(engine.applyPreset("melodic-lift", error));
    CHECK(error.empty());
    CHECK(engine.hasWrittenProgression());
    CHECK_EQ(engine.analysis().key.tonic, 0);
    CHECK_EQ(engine.analysis().progression.size(), size_t(4));
    CHECK_EQ(engine.analysis().romanNumeralString(), std::string("i | III | VII | VI"));

    CHECK(! engine.applyPreset("does-not-exist", error));
    CHECK(! error.empty());
}

TEST(AnInventedProgressionIsDiatonicAndRepeatable)
{
    const Key key { 9, ScaleType::NaturalMinor }; // A minor

    const auto first = inventProgression(key, 1234u, 4);
    CHECK(first.size() == 4);

    // With no library it walks the key's own harmony, so every chord has to be
    // one of the seven degrees - a surprise that leaves the key is a bug.
    const auto triads = diatonicTriads(key);
    for (const auto& chord : first)
    {
        bool found = false;
        for (const auto& triad : triads)
            found = found || (triad.root == chord.root && triad.type == chord.type);
        CHECK(found);
    }

    // Same seed, same idea - the whole reproducibility story depends on it.
    const auto again = inventProgression(key, 1234u, 4);
    CHECK(roots(again) == roots(first));

    // A different seed has to be a different idea at least most of the time.
    int differing = 0;
    for (uint32_t seed = 1; seed <= 12; ++seed)
        if (roots(inventProgression(key, seed, 4)) != roots(first))
            ++differing;
    CHECK(differing >= 8);
}

TEST(AnInventedKeyIsRepeatable)
{
    const auto key = inventKey(99u);
    const auto again = inventKey(99u);
    CHECK(key.tonic == again.tonic);
    CHECK(key.scale == again.scale);
    CHECK(key.tonic >= 0 && key.tonic <= 11);

    // And it does not always hand back the same key.
    std::set<int> tonics;
    for (uint32_t seed = 1; seed <= 40; ++seed)
        tonics.insert(inventKey(seed).tonic);
    CHECK(tonics.size() >= 5);
}

TEST(HarmonyIsStretchedNotRepeated)
{
    // The difference between a progression that repeats to fill more bars and
    // one that is expanded to fill them. Four chords over eight bars, each held
    // twice as long - which is what a slow pad wants, and what doubling a clip's
    // loop length does in a DAW.
    Engine engine;
    engine.setSource(fixtures::bassClip({ 36, 33, 29, 31 }));

    const auto before = engine.analysis();
    CHECK(before.progression.size() == 4);
    CHECK(before.bars == 4);

    engine.setBarsPerChord(2.0f);
    const auto after = engine.analysis();

    // The same four chords, in the same order - nothing was added.
    CHECK(after.progression.size() == 4);
    CHECK(roots(chordsOf(after)) == roots(chordsOf(before)));

    // Twice as long, and each chord holds for two bars.
    CHECK(after.bars == 8);
    const int64_t bar = after.ticksPerBar();
    for (size_t i = 0; i < after.progression.size(); ++i)
    {
        CHECK(after.progression[i].lengthTick == bar * 2);
        CHECK(after.progression[i].startTick == static_cast<int64_t>(i) * bar * 2);
    }

    // Tempo, meter and the clip's groove are untouched - only the harmony moved.
    CHECK(after.bpm == before.bpm);
    CHECK(after.timeSignature.numerator == before.timeSignature.numerator);
    CHECK(after.rhythm.notesPerBar == before.rhythm.notesPerBar);

    // And it goes back.
    engine.setBarsPerChord(0.0f);
    CHECK(engine.analysis().bars == 4);
}

TEST(OneChordCanFillTheWholeThing)
{
    Engine engine;
    std::string error;
    CHECK(engine.setProgressionText("Am", error));

    engine.setBarsPerChord(8.0f);
    const auto& analysis = engine.analysis();
    CHECK(analysis.progression.size() == 1);
    CHECK(analysis.bars == 8);
    CHECK(analysis.progression[0].lengthTick == analysis.ticksPerBar() * 8);

    // A part written over it stretches across all eight bars rather than being
    // eight copies of a one-bar idea. Checked against the key rather than the
    // triad: the pad extends chords as the Colour knob allows, so a seventh or
    // a ninth over Am is right and only looks wrong from the triad's side.
    GenerateOptions options;
    options.part = PartType::Pad;
    options.seed = 3;
    const auto pad = engine.generate(options);
    CHECK(! pad.empty());
    CHECK(pad.lengthTicks() > analysis.ticksPerBar() * 6);
    for (const auto& note : pad.notes)
        CHECK(analysis.key.contains(note.pitch % 12));
}
