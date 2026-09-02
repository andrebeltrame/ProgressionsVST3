#pragma once

#include "harmonia/Analysis.h"
#include "harmonia/Theory.h"
#include "harmonia/Types.h"

#include <string>
#include <vector>

namespace harmonia
{

/** Reads a chord symbol: "Am", "F#m7b5", "Bbmaj9", "Cmaj7/G", "Gsus4". */
bool parseChordSymbol(const std::string& token, Chord& out);

/** Reads a roman numeral in the given key: "i", "VI", "bVII7", "V7", "ivm7".
    Degrees are counted in the key's own scale, so in A minor VI is F. */
bool parseRomanNumeral(const std::string& token, const Key& key, Chord& out);

/** Reads a whole progression. Tokens may be separated by "|", "-", "," or
    spaces, and chord symbols and roman numerals may be mixed. */
bool parseProgression(const std::string& text, const Key& key,
                      std::vector<Chord>& out, std::string& error);

std::string progressionToText(const std::vector<Chord>& chords, bool useFlats = false);
std::string progressionToText(const std::vector<Chord>& chords, const Key& key);
std::string progressionToRoman(const std::vector<Chord>& chords, const Key& key);

/** Guesses the key a written progression is in, using the first chord as the
    tonic candidate and the chord tones as the scale. */
Key guessKeyForProgression(const std::vector<Chord>& chords);

struct StyleModel;

/** Picks a key to write in out of nothing - the tonic uniformly, the mode
    weighted towards the ones this plugin is actually used in. */
Key inventKey(uint32_t seed);

/** Invents a progression with no clip and nothing typed: the harmony behind
    "Surprise me".

    With a style model it draws on the roman-numeral loops learned from your own
    collection, weighted by how often each turned up, and reads them in `key` -
    so a surprise still sounds like your records. Those loops are numerals, not
    anyone's notes: nothing of the original material comes across. Without a
    model, or when the learned loop will not fit, it walks the functional
    harmony of the key instead, which is what keeps this working with no library
    at all.

    Deterministic: the same seed and key always invent the same progression. */
std::vector<Chord> inventProgression(const Key& key, uint32_t seed, int length,
                                     const StyleModel* style = nullptr);

/** Builds a playable Analysis from chords alone - no source clip needed. */
Analysis analysisFromProgression(const std::vector<Chord>& chords,
                                 const Key& key,
                                 double bpm = 122.0,
                                 TimeSignature timeSignature = {},
                                 int beatsPerChord = 0, // 0 = one bar per chord
                                 int repeats = 1,
                                 int ppq = kPPQ);

/** Swaps the chords of an existing analysis, keeping its tempo, meter, length,
    groove and detected role. The new chords are spread evenly over the clip and
    snapped to the beat grid. */
void applyProgressionTo(Analysis& analysis, const std::vector<Chord>& chords);

/** Holds each chord for `ticksPerChord`, stretching the analysis to fit.

    This is the difference between a progression that *repeats* to fill more
    bars and one that is *expanded* to fill them: the same four chords over
    eight bars, each held twice as long, which is what a slow pad wants. Tempo,
    meter, groove and detected role are untouched - only how long each chord is
    held changes, so the parts written over it keep their feel while the harmony
    moves underneath them more slowly.

    Does nothing when `ticksPerChord` is zero, which is how "leave the clip's
    own harmonic rhythm alone" is expressed. */
void stretchHarmony(Analysis& analysis, int64_t ticksPerChord);

} // namespace harmonia
