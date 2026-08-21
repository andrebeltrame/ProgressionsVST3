#include "harmonia/Theory.h"

#include <algorithm>
#include <cmath>
#include <map>

namespace harmonia
{

namespace
{

const std::vector<int> kMajor { 0, 2, 4, 5, 7, 9, 11 };
const std::vector<int> kNaturalMinor { 0, 2, 3, 5, 7, 8, 10 };
const std::vector<int> kHarmonicMinor { 0, 2, 3, 5, 7, 8, 11 };
const std::vector<int> kMelodicMinor { 0, 2, 3, 5, 7, 9, 11 };
const std::vector<int> kDorian { 0, 2, 3, 5, 7, 9, 10 };
const std::vector<int> kPhrygian { 0, 1, 3, 5, 7, 8, 10 };
const std::vector<int> kLydian { 0, 2, 4, 6, 7, 9, 11 };
const std::vector<int> kMixolydian { 0, 2, 4, 5, 7, 9, 10 };
const std::vector<int> kLocrian { 0, 1, 3, 5, 6, 8, 10 };
const std::vector<int> kMajorPentatonic { 0, 2, 4, 7, 9 };
const std::vector<int> kMinorPentatonic { 0, 3, 5, 7, 10 };
const std::vector<int> kBlues { 0, 3, 5, 6, 7, 10 };

const char* kSharpNames[12] = { "C", "C#", "D", "D#", "E", "F", "F#", "G", "G#", "A", "A#", "B" };
const char* kFlatNames[12]  = { "C", "Db", "D", "Eb", "E", "F", "Gb", "G", "Ab", "A", "Bb", "B" };

} // namespace

int mod12(int value)
{
    const int m = value % 12;
    return m < 0 ? m + 12 : m;
}

const char* toString(ScaleType type)
{
    switch (type)
    {
        case ScaleType::Major:           return "Major";
        case ScaleType::NaturalMinor:    return "Minor";
        case ScaleType::HarmonicMinor:   return "Harmonic Minor";
        case ScaleType::MelodicMinor:    return "Melodic Minor";
        case ScaleType::Dorian:          return "Dorian";
        case ScaleType::Phrygian:        return "Phrygian";
        case ScaleType::Lydian:          return "Lydian";
        case ScaleType::Mixolydian:      return "Mixolydian";
        case ScaleType::Locrian:         return "Locrian";
        case ScaleType::MajorPentatonic: return "Major Pentatonic";
        case ScaleType::MinorPentatonic: return "Minor Pentatonic";
        case ScaleType::Blues:           return "Blues";
    }
    return "Major";
}

const char* toString(ChordType type)
{
    switch (type)
    {
        case ChordType::Major:            return "Major";
        case ChordType::Minor:            return "Minor";
        case ChordType::Diminished:       return "Diminished";
        case ChordType::Augmented:        return "Augmented";
        case ChordType::Sus2:             return "Sus2";
        case ChordType::Sus4:             return "Sus4";
        case ChordType::Major6:           return "Major 6";
        case ChordType::Minor6:           return "Minor 6";
        case ChordType::Major7:           return "Major 7";
        case ChordType::Minor7:           return "Minor 7";
        case ChordType::Dominant7:        return "Dominant 7";
        case ChordType::HalfDiminished7:  return "Half Diminished";
        case ChordType::Diminished7:      return "Diminished 7";
        case ChordType::MinorMajor7:      return "Minor Major 7";
        case ChordType::Dominant7Sus4:    return "7sus4";
        case ChordType::Add9:             return "Add 9";
        case ChordType::Major9:           return "Major 9";
        case ChordType::Minor9:           return "Minor 9";
        case ChordType::Dominant9:        return "Dominant 9";
        case ChordType::Power:            return "Power";
    }
    return "Major";
}

const std::vector<int>& chordIntervals(ChordType type)
{
    static const std::map<ChordType, std::vector<int>> table {
        { ChordType::Major,           { 0, 4, 7 } },
        { ChordType::Minor,           { 0, 3, 7 } },
        { ChordType::Diminished,      { 0, 3, 6 } },
        { ChordType::Augmented,       { 0, 4, 8 } },
        { ChordType::Sus2,            { 0, 2, 7 } },
        { ChordType::Sus4,            { 0, 5, 7 } },
        { ChordType::Major6,          { 0, 4, 7, 9 } },
        { ChordType::Minor6,          { 0, 3, 7, 9 } },
        { ChordType::Major7,          { 0, 4, 7, 11 } },
        { ChordType::Minor7,          { 0, 3, 7, 10 } },
        { ChordType::Dominant7,       { 0, 4, 7, 10 } },
        { ChordType::HalfDiminished7, { 0, 3, 6, 10 } },
        { ChordType::Diminished7,     { 0, 3, 6, 9 } },
        { ChordType::MinorMajor7,     { 0, 3, 7, 11 } },
        { ChordType::Dominant7Sus4,   { 0, 5, 7, 10 } },
        { ChordType::Add9,            { 0, 4, 7, 14 } },
        { ChordType::Major9,          { 0, 4, 7, 11, 14 } },
        { ChordType::Minor9,          { 0, 3, 7, 10, 14 } },
        { ChordType::Dominant9,       { 0, 4, 7, 10, 14 } },
        { ChordType::Power,           { 0, 7 } },
    };
    static const std::vector<int> fallback { 0, 4, 7 };
    const auto it = table.find(type);
    return it != table.end() ? it->second : fallback;
}

const char* chordSymbol(ChordType type)
{
    switch (type)
    {
        case ChordType::Major:            return "";
        case ChordType::Minor:            return "m";
        case ChordType::Diminished:       return "dim";
        case ChordType::Augmented:        return "aug";
        case ChordType::Sus2:             return "sus2";
        case ChordType::Sus4:             return "sus4";
        case ChordType::Major6:           return "6";
        case ChordType::Minor6:           return "m6";
        case ChordType::Major7:           return "maj7";
        case ChordType::Minor7:           return "m7";
        case ChordType::Dominant7:        return "7";
        case ChordType::HalfDiminished7:  return "m7b5";
        case ChordType::Diminished7:      return "dim7";
        case ChordType::MinorMajor7:      return "mMaj7";
        case ChordType::Dominant7Sus4:    return "7sus4";
        case ChordType::Add9:             return "add9";
        case ChordType::Major9:           return "maj9";
        case ChordType::Minor9:           return "m9";
        case ChordType::Dominant9:        return "9";
        case ChordType::Power:            return "5";
    }
    return "";
}

const std::vector<int>& scaleIntervals(ScaleType type)
{
    switch (type)
    {
        case ScaleType::Major:           return kMajor;
        case ScaleType::NaturalMinor:    return kNaturalMinor;
        case ScaleType::HarmonicMinor:   return kHarmonicMinor;
        case ScaleType::MelodicMinor:    return kMelodicMinor;
        case ScaleType::Dorian:          return kDorian;
        case ScaleType::Phrygian:        return kPhrygian;
        case ScaleType::Lydian:          return kLydian;
        case ScaleType::Mixolydian:      return kMixolydian;
        case ScaleType::Locrian:         return kLocrian;
        case ScaleType::MajorPentatonic: return kMajorPentatonic;
        case ScaleType::MinorPentatonic: return kMinorPentatonic;
        case ScaleType::Blues:           return kBlues;
    }
    return kMajor;
}

std::string pitchClassName(int pitchClass, bool preferFlats)
{
    const int pc = mod12(pitchClass);
    return preferFlats ? kFlatNames[pc] : kSharpNames[pc];
}

std::string noteName(int midiPitch, bool preferFlats)
{
    const int octave = midiPitch / 12 - 1;
    return pitchClassName(midiPitch, preferFlats) + std::to_string(octave);
}

// ---------------------------------------------------------------------------
// Key
// ---------------------------------------------------------------------------

bool Key::isMinorMode() const
{
    switch (scale)
    {
        case ScaleType::NaturalMinor:
        case ScaleType::HarmonicMinor:
        case ScaleType::MelodicMinor:
        case ScaleType::Dorian:
        case ScaleType::Phrygian:
        case ScaleType::Locrian:
        case ScaleType::MinorPentatonic:
        case ScaleType::Blues:
            return true;
        default:
            return false;
    }
}

std::string Key::name(bool preferFlats) const
{
    return pitchClassName(tonic, preferFlats) + std::string(" ") + toString(scale);
}

std::vector<int> Key::pitchClasses() const
{
    std::vector<int> out;
    for (int step : scaleIntervals(scale))
        out.push_back(mod12(tonic + step));
    return out;
}

bool Key::contains(int pitchClass) const
{
    return degreeOf(pitchClass) >= 0;
}

int Key::degreeOf(int pitchClass) const
{
    const auto& steps = scaleIntervals(scale);
    const int offset = mod12(pitchClass - tonic);
    for (size_t i = 0; i < steps.size(); ++i)
        if (steps[i] == offset)
            return static_cast<int>(i);
    return -1;
}

int Key::pitchForDegree(int degree, int octaveBase) const
{
    const auto& steps = scaleIntervals(scale);
    const int size = static_cast<int>(steps.size());
    int octaveShift = 0;
    int index = degree;
    while (index < 0)
    {
        index += size;
        --octaveShift;
    }
    octaveShift += index / size;
    index %= size;
    return octaveBase + tonic + steps[index] + 12 * octaveShift;
}

int Key::snapToScale(int midiPitch, int direction) const
{
    if (contains(mod12(midiPitch)))
        return midiPitch;

    for (int distance = 1; distance <= 6; ++distance)
    {
        if (direction >= 0 && contains(mod12(midiPitch + distance)))
            return midiPitch + distance;
        if (direction <= 0 && contains(mod12(midiPitch - distance)))
            return midiPitch - distance;
    }
    return midiPitch;
}

// ---------------------------------------------------------------------------
// Chord
// ---------------------------------------------------------------------------

std::vector<int> Chord::pitchClasses() const
{
    std::vector<int> out;
    for (int interval : chordIntervals(type))
    {
        const int pc = mod12(root + interval);
        if (std::find(out.begin(), out.end(), pc) == out.end())
            out.push_back(pc);
    }
    return out;
}

bool Chord::containsPitchClass(int pitchClass) const
{
    const auto pcs = pitchClasses();
    return std::find(pcs.begin(), pcs.end(), mod12(pitchClass)) != pcs.end();
}

std::string Chord::name(bool preferFlats) const
{
    std::string out = pitchClassName(root, preferFlats) + std::string(chordSymbol(type));
    if (bass >= 0 && mod12(bass) != mod12(root))
        out += "/" + pitchClassName(bass, preferFlats);
    return out;
}

std::string Chord::romanNumeral(const Key& key) const
{
    static const char* upper[7] = { "I", "II", "III", "IV", "V", "VI", "VII" };
    static const char* lower[7] = { "i", "ii", "iii", "iv", "v", "vi", "vii" };
    // Degrees measured against the major scale, so chromatic roots get an accidental.
    static const int majorSteps[7] = { 0, 2, 4, 5, 7, 9, 11 };

    const int offset = mod12(root - key.tonic);
    int degree = 0;
    std::string accidental;
    bool found = false;
    for (int i = 0; i < 7; ++i)
    {
        if (majorSteps[i] == offset)
        {
            degree = i;
            found = true;
            break;
        }
    }
    if (! found)
    {
        for (int i = 0; i < 7; ++i)
        {
            if (majorSteps[i] == mod12(offset + 1))
            {
                degree = i;
                accidental = "b";
                break;
            }
        }
    }

    const bool minorish = type == ChordType::Minor || type == ChordType::Minor7
                       || type == ChordType::Minor9 || type == ChordType::Minor6
                       || type == ChordType::MinorMajor7 || type == ChordType::Diminished
                       || type == ChordType::Diminished7 || type == ChordType::HalfDiminished7;

    std::string numeral = accidental + (minorish ? lower[degree] : upper[degree]);

    switch (type)
    {
        case ChordType::Dominant7:       numeral += "7"; break;
        case ChordType::Dominant9:       numeral += "9"; break;
        case ChordType::Major7:          numeral += "maj7"; break;
        case ChordType::Major9:          numeral += "maj9"; break;
        case ChordType::Minor7:          numeral += "7"; break;
        case ChordType::Minor9:          numeral += "9"; break;
        case ChordType::Minor6:          numeral += "6"; break;
        case ChordType::Major6:          numeral += "6"; break;
        case ChordType::HalfDiminished7: numeral += "\xC3\xB8""7"; break;
        case ChordType::Diminished:      numeral += "\xC2\xB0"; break;
        case ChordType::Diminished7:     numeral += "\xC2\xB0""7"; break;
        case ChordType::Augmented:       numeral += "+"; break;
        case ChordType::Sus2:            numeral += "sus2"; break;
        case ChordType::Sus4:            numeral += "sus4"; break;
        case ChordType::Dominant7Sus4:   numeral += "7sus4"; break;
        case ChordType::Add9:            numeral += "add9"; break;
        case ChordType::Power:           numeral += "5"; break;
        default: break;
    }
    return numeral;
}

std::vector<Chord> diatonicTriads(const Key& key)
{
    std::vector<Chord> out;
    const auto pcs = key.pitchClasses();
    const int size = static_cast<int>(pcs.size());
    if (size < 7)
        return out;

    for (int degree = 0; degree < 7; ++degree)
    {
        const int root = pcs[degree];
        const int third = mod12(pcs[(degree + 2) % 7] - root);
        const int fifth = mod12(pcs[(degree + 4) % 7] - root);

        ChordType type = ChordType::Major;
        if (third == 4 && fifth == 7)      type = ChordType::Major;
        else if (third == 3 && fifth == 7) type = ChordType::Minor;
        else if (third == 3 && fifth == 6) type = ChordType::Diminished;
        else if (third == 4 && fifth == 8) type = ChordType::Augmented;
        else if (third == 2 || third == 5) type = ChordType::Sus4;

        out.push_back({ root, type, -1 });
    }
    return out;
}

std::vector<Chord> diatonicSevenths(const Key& key)
{
    std::vector<Chord> out;
    const auto pcs = key.pitchClasses();
    if (pcs.size() < 7)
        return out;

    for (int degree = 0; degree < 7; ++degree)
    {
        const int root = pcs[degree];
        const int third = mod12(pcs[(degree + 2) % 7] - root);
        const int fifth = mod12(pcs[(degree + 4) % 7] - root);
        const int seventh = mod12(pcs[(degree + 6) % 7] - root);

        ChordType type = ChordType::Major7;
        if (third == 4 && fifth == 7 && seventh == 11)      type = ChordType::Major7;
        else if (third == 4 && fifth == 7 && seventh == 10) type = ChordType::Dominant7;
        else if (third == 3 && fifth == 7 && seventh == 10) type = ChordType::Minor7;
        else if (third == 3 && fifth == 7 && seventh == 11) type = ChordType::MinorMajor7;
        else if (third == 3 && fifth == 6 && seventh == 10) type = ChordType::HalfDiminished7;
        else if (third == 3 && fifth == 6 && seventh == 9)  type = ChordType::Diminished7;
        else if (third == 4 && fifth == 8)                  type = ChordType::Augmented;

        out.push_back({ root, type, -1 });
    }
    return out;
}

Chord simplifyToTriad(const Chord& chord)
{
    Chord out = chord;
    out.bass = -1;
    switch (chord.type)
    {
        case ChordType::Major7:
        case ChordType::Major9:
        case ChordType::Major6:
        case ChordType::Add9:
        case ChordType::Dominant7:
        case ChordType::Dominant9:
            out.type = ChordType::Major;
            break;
        case ChordType::Minor7:
        case ChordType::Minor9:
        case ChordType::Minor6:
        case ChordType::MinorMajor7:
            out.type = ChordType::Minor;
            break;
        case ChordType::HalfDiminished7:
        case ChordType::Diminished7:
            out.type = ChordType::Diminished;
            break;
        case ChordType::Dominant7Sus4:
            out.type = ChordType::Sus4;
            break;
        default:
            break;
    }
    return out;
}

Chord extendChord(const Chord& chord, const Key& key, float complexity)
{
    if (complexity < 0.34f)
        return simplifyToTriad(chord);

    Chord out = chord;
    const int degree = key.degreeOf(chord.root);
    const auto sevenths = diatonicSevenths(key);

    // Prefer the seventh the key already contains, so an extension never drags
    // in a note that is out of key.
    bool extended = false;
    if (degree >= 0 && degree < static_cast<int>(sevenths.size()))
    {
        const Chord diatonic = sevenths[static_cast<size_t>(degree)];
        if (simplifyToTriad(diatonic).type == simplifyToTriad(chord).type)
        {
            out.type = diatonic.type;
            extended = true;
        }
    }

    if (! extended)
    {
        const bool isDominantDegree = mod12(chord.root - key.tonic) == 7;
        switch (chord.type)
        {
            case ChordType::Major:      out.type = isDominantDegree ? ChordType::Dominant7 : ChordType::Major7; break;
            case ChordType::Minor:      out.type = ChordType::Minor7; break;
            case ChordType::Diminished: out.type = ChordType::HalfDiminished7; break;
            default: break;
        }
    }

    // The ninth only goes on when it is diatonic too.
    if (complexity >= 0.67f && key.contains(mod12(chord.root + 2)))
    {
        switch (out.type)
        {
            case ChordType::Major7:    out.type = ChordType::Major9; break;
            case ChordType::Minor7:    out.type = ChordType::Minor9; break;
            case ChordType::Dominant7: out.type = ChordType::Dominant9; break;
            case ChordType::Major:     out.type = ChordType::Add9; break;
            default: break;
        }
    }

    return out;
}

int nearestPitchWithClass(int pitchClass, int reference)
{
    const int pc = mod12(pitchClass);
    int candidate = reference - mod12(reference - pc);
    if (std::abs(candidate + 12 - reference) < std::abs(candidate - reference))
        candidate += 12;
    return candidate;
}

std::vector<int> voiceChord(const Chord& chord,
                            const std::vector<int>& previous,
                            int lowPitch,
                            int highPitch,
                            int maxVoices)
{
    auto pcs = chord.pitchClasses();
    if (pcs.empty())
        return {};

    if (maxVoices > 0 && static_cast<int>(pcs.size()) > maxVoices)
    {
        // Drop the fifth first - it carries the least information.
        const int fifth = mod12(chord.root + 7);
        auto it = std::find(pcs.begin(), pcs.end(), fifth);
        if (it != pcs.end() && pcs.size() > 3)
            pcs.erase(it);
        while (static_cast<int>(pcs.size()) > maxVoices)
            pcs.pop_back();
    }

    const int voiceCount = static_cast<int>(pcs.size());
    const double centre = 0.5 * (lowPitch + highPitch);

    double previousCentre = centre;
    if (! previous.empty())
    {
        double sum = 0.0;
        for (int pitch : previous)
            sum += pitch;
        previousCentre = sum / static_cast<double>(previous.size());
    }

    std::vector<int> best;
    double bestCost = 1e18;

    // Try every inversion at every octave that fits, and keep whichever one
    // moves the least from the chord before it.
    for (int inversion = 0; inversion < voiceCount; ++inversion)
    {
        std::vector<int> order;
        order.reserve(static_cast<size_t>(voiceCount));
        for (int i = 0; i < voiceCount; ++i)
            order.push_back(pcs[static_cast<size_t>((inversion + i) % voiceCount)]);

        for (int bottom = lowPitch; bottom <= lowPitch + 11; ++bottom)
        {
            if (mod12(bottom) != order[0])
                continue;

            for (int octave = 0; bottom + octave * 12 <= highPitch; ++octave)
            {
                std::vector<int> candidate;
                candidate.push_back(bottom + octave * 12);
                bool fits = true;
                for (int i = 1; i < voiceCount; ++i)
                {
                    int next = candidate.back() + mod12(order[static_cast<size_t>(i)] - mod12(candidate.back()));
                    if (next == candidate.back())
                        next += 12;
                    if (next > highPitch)
                    {
                        fits = false;
                        break;
                    }
                    candidate.push_back(next);
                }
                if (! fits)
                    continue;

                double cost = 0.0;
                double sum = 0.0;
                for (int pitch : candidate)
                {
                    sum += pitch;
                    if (! previous.empty())
                    {
                        int nearest = 127;
                        for (int reference : previous)
                            nearest = std::min(nearest, std::abs(pitch - reference));
                        cost += nearest;
                    }
                }

                const double candidateCentre = sum / static_cast<double>(candidate.size());
                cost += 0.6 * std::abs(candidateCentre - (previous.empty() ? centre : previousCentre));
                // Discourage voicings that sprawl over more than two octaves.
                cost += 0.25 * std::max(0, candidate.back() - candidate.front() - 24);

                if (cost < bestCost)
                {
                    bestCost = cost;
                    best = candidate;
                }
            }
        }
    }

    if (best.empty())
    {
        // Nothing fitted the register - fall back to folding the tones into range.
        for (int pc : pcs)
        {
            int pitch = nearestPitchWithClass(pc, static_cast<int>(centre));
            while (pitch < lowPitch)
                pitch += 12;
            while (pitch > highPitch)
                pitch -= 12;
            best.push_back(std::clamp(pitch, 0, 127));
        }
    }

    std::sort(best.begin(), best.end());
    best.erase(std::unique(best.begin(), best.end()), best.end());
    return best;
}

} // namespace harmonia
