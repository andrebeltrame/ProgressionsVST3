#include "Fixtures.h"
#include "TestHarness.h"

#include "harmonia/Engine.h"
#include "harmonia/Presets.h"
#include "harmonia/Progression.h"

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
