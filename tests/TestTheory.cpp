#include "TestHarness.h"

#include "harmonia/Theory.h"

#include <algorithm>
#include <cmath>

using namespace harmonia;

TEST(NoteAndChordNames)
{
    CHECK_EQ(pitchClassName(1), std::string("C#"));
    CHECK_EQ(pitchClassName(1, true), std::string("Db"));
    CHECK_EQ(noteName(60), std::string("C4"));
    CHECK_EQ(noteName(21), std::string("A0"));

    CHECK_EQ((Chord { 0, ChordType::Major }).name(), std::string("C"));
    CHECK_EQ((Chord { 9, ChordType::Minor7 }).name(), std::string("Am7"));
    CHECK_EQ((Chord { 7, ChordType::Dominant7 }).name(), std::string("G7"));
    CHECK_EQ((Chord { 5, ChordType::Major7, 0 }).name(), std::string("Fmaj7/C"));
}

TEST(ScaleMembership)
{
    const Key cMajor { 0, ScaleType::Major };
    CHECK(cMajor.contains(0));
    CHECK(cMajor.contains(11));
    CHECK(! cMajor.contains(1));
    CHECK_EQ(cMajor.degreeOf(4), 2);
    CHECK_EQ(cMajor.degreeOf(6), -1);
    CHECK_EQ(cMajor.name(), std::string("C Major"));

    const Key aMinor { 9, ScaleType::NaturalMinor };
    CHECK(aMinor.isMinorMode());
    CHECK(aMinor.contains(9));
    CHECK(! aMinor.contains(8));

    // Snapping a non-diatonic pitch lands on a scale tone.
    CHECK(cMajor.contains(mod12(cMajor.snapToScale(61))));
    CHECK_EQ(cMajor.snapToScale(60), 60);
}

TEST(ScaleDegreeToPitch)
{
    const Key cMajor { 0, ScaleType::Major };
    CHECK_EQ(cMajor.pitchForDegree(0, 60), 60);
    CHECK_EQ(cMajor.pitchForDegree(4, 60), 67);
    CHECK_EQ(cMajor.pitchForDegree(7, 60), 72);
    CHECK_EQ(cMajor.pitchForDegree(-1, 60), 59);
    CHECK_EQ(cMajor.pitchForDegree(-7, 60), 48);
}

TEST(DiatonicChordsOfAKey)
{
    const Key cMajor { 0, ScaleType::Major };
    const auto triads = diatonicTriads(cMajor);
    CHECK_EQ(triads.size(), size_t(7));
    CHECK_EQ(triads[0].name(), std::string("C"));
    CHECK_EQ(triads[1].name(), std::string("Dm"));
    CHECK_EQ(triads[4].name(), std::string("G"));
    CHECK_EQ(triads[5].name(), std::string("Am"));
    CHECK_EQ(triads[6].name(), std::string("Bdim"));

    const auto sevenths = diatonicSevenths(cMajor);
    CHECK_EQ(sevenths[0].name(), std::string("Cmaj7"));
    CHECK_EQ(sevenths[4].name(), std::string("G7"));
    CHECK_EQ(sevenths[6].name(), std::string("Bm7b5"));
}

TEST(RomanNumerals)
{
    const Key cMajor { 0, ScaleType::Major };
    CHECK_EQ((Chord { 0, ChordType::Major }).romanNumeral(cMajor), std::string("I"));
    CHECK_EQ((Chord { 9, ChordType::Minor }).romanNumeral(cMajor), std::string("vi"));
    CHECK_EQ((Chord { 7, ChordType::Dominant7 }).romanNumeral(cMajor), std::string("V7"));
    CHECK_EQ((Chord { 10, ChordType::Major }).romanNumeral(cMajor), std::string("bVII"));
    CHECK_EQ((Chord { 2, ChordType::Minor7 }).romanNumeral(cMajor), std::string("ii7"));
}

TEST(ChordExtensionRespectsComplexity)
{
    const Key cMajor { 0, ScaleType::Major };
    CHECK(extendChord({ 0, ChordType::Major7 }, cMajor, 0.0f).type == ChordType::Major);
    CHECK(extendChord({ 0, ChordType::Major }, cMajor, 0.5f).type == ChordType::Major7);
    CHECK(extendChord({ 7, ChordType::Major }, cMajor, 0.5f).type == ChordType::Dominant7);
    CHECK(extendChord({ 2, ChordType::Minor }, cMajor, 0.9f).type == ChordType::Minor9);
}

TEST(VoicingStaysInRangeAndUsesChordTones)
{
    const Chord chord { 5, ChordType::Major7 }; // Fmaj7
    const auto voicing = voiceChord(chord, {}, 55, 84, 4);
    CHECK(! voicing.empty());
    for (int pitch : voicing)
    {
        CHECK(pitch >= 55);
        CHECK(pitch <= 84);
        CHECK(chord.containsPitchClass(pitch));
    }
    CHECK(std::is_sorted(voicing.begin(), voicing.end()));
}

TEST(VoiceLeadingKeepsMovementSmall)
{
    const auto first = voiceChord({ 0, ChordType::Major }, {}, 55, 84, 4);
    const auto second = voiceChord({ 5, ChordType::Major }, first, 55, 84, 4);

    CHECK_EQ(first.size(), second.size());
    int totalMovement = 0;
    for (size_t i = 0; i < std::min(first.size(), second.size()); ++i)
        totalMovement += std::abs(second[i] - first[i]);
    // C -> F in close position should move by a handful of semitones, not an octave.
    CHECK(totalMovement <= 9);
}

TEST(NearestPitchWithClass)
{
    CHECK_EQ(nearestPitchWithClass(0, 60), 60);
    CHECK_EQ(nearestPitchWithClass(11, 60), 59);
    CHECK_EQ(nearestPitchWithClass(1, 60), 61);
    CHECK_EQ(mod12(nearestPitchWithClass(7, 43)), 7);
}
