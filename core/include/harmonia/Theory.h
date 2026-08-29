#pragma once

#include <string>
#include <vector>

namespace harmonia
{

/** Musical scales the analyser can report and the generators can write in. */
enum class ScaleType
{
    Major,
    NaturalMinor,
    HarmonicMinor,
    MelodicMinor,
    Dorian,
    Phrygian,
    Lydian,
    Mixolydian,
    Locrian,
    MajorPentatonic,
    MinorPentatonic,
    Blues
};

enum class ChordType
{
    Major,
    Minor,
    Diminished,
    Augmented,
    Sus2,
    Sus4,
    Major6,
    Minor6,
    Major7,
    Minor7,
    Dominant7,
    HalfDiminished7,
    Diminished7,
    MinorMajor7,
    Dominant7Sus4,
    Add9,
    Major9,
    Minor9,
    Dominant9,
    Power // root + fifth, used when a source is too sparse to imply a third
};

const char* toString(ScaleType type);
const char* toString(ChordType type);

/** Reads back what toString(ScaleType) wrote, plus the obvious aliases. */
bool scaleTypeFromString(const std::string& text, ScaleType& out);

/** Chord tones as semitone offsets from the root. */
const std::vector<int>& chordIntervals(ChordType type);

/** Short symbol appended to a root name, e.g. "m7", "maj7", "sus4". */
const char* chordSymbol(ChordType type);

/** Suffix appended to a roman numeral, e.g. "7", "maj7", "sus4". The case of
    the numeral itself already carries major/minor. */
const char* numeralSuffix(ChordType type);

/** Scale steps as semitone offsets from the tonic. */
const std::vector<int>& scaleIntervals(ScaleType type);

/** How the user wants notes spelled. Auto follows the key, which is what reads
    right musically; the other two are the b/# switch, for when someone is
    copying a progression written one way and wants to see it that way. */
enum class AccidentalStyle
{
    Auto,
    Sharps,
    Flats
};

struct Key;
/** The single place anything asks "sharps or flats here?". */
bool useFlatsFor(const Key& key, AccidentalStyle style);

std::string pitchClassName(int pitchClass, bool preferFlats = false);
std::string noteName(int midiPitch, bool preferFlats = false);

/** Wraps any integer into 0..11. */
int mod12(int value);

struct Key
{
    int tonic = 0; // pitch class 0..11
    ScaleType scale = ScaleType::Major;

    /** "Eb Minor". Uses the key's own accidentals unless told otherwise. */
    std::string name() const;
    std::string name(bool useFlats) const;
    std::vector<int> pitchClasses() const;
    bool contains(int pitchClass) const;
    /** Index of the pitch class in the scale, or -1 if it is not diatonic. */
    int degreeOf(int pitchClass) const;
    bool isMinorMode() const;
    /** True for keys that are normally written with flats, so Eb does not come
        out as D#. */
    bool preferFlats() const;
    /** Nearest scale tone to the given MIDI pitch. */
    int snapToScale(int midiPitch, int direction = 0) const;
    /** MIDI pitch for scale degree `degree` (may be negative or beyond the octave). */
    int pitchForDegree(int degree, int octaveBase) const;
};

struct Chord
{
    int root = 0; // pitch class
    ChordType type = ChordType::Major;
    int bass = -1; // pitch class of an inversion, -1 for root position

    /** "Am7", "Cmaj7/G". Sharps unless you ask for flats - Analysis and Key
        spell chords with the accidentals the key actually uses. */
    std::string name() const;
    std::string name(bool useFlats) const;
    std::vector<int> pitchClasses() const;
    bool containsPitchClass(int pitchClass) const;
    /** Roman numeral relative to a key, e.g. "vi", "V7", "bVII". */
    std::string romanNumeral(const Key& key) const;
};

/** The seven diatonic seventh chords of a key, degree 0 = tonic. */
std::vector<Chord> diatonicSevenths(const Key& key);
std::vector<Chord> diatonicTriads(const Key& key);

/** Simplifies a chord to a triad (used when the user asks for less complexity). */
Chord simplifyToTriad(const Chord& chord);
/** Adds a diatonically sensible seventh/ninth to a triad. */
Chord extendChord(const Chord& chord, const Key& key, float complexity);

/** Voices a chord inside [lowPitch, highPitch], moving as little as possible
    from `previous`. Returns MIDI pitches, low to high. */
std::vector<int> voiceChord(const Chord& chord,
                            const std::vector<int>& previous,
                            int lowPitch,
                            int highPitch,
                            int maxVoices);

/** Nearest MIDI pitch with the given pitch class to `reference`. */
int nearestPitchWithClass(int pitchClass, int reference);

} // namespace harmonia
