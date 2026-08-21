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

} // namespace harmonia
