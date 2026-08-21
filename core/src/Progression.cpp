#include "harmonia/Progression.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <iterator>
#include <map>

namespace harmonia
{

namespace
{

struct SuffixEntry
{
    const char* text;
    ChordType type;
    bool caseSensitive;
};

/** Longest first: "maj7" has to win over "m", "m7b5" over "m7". */
const SuffixEntry kChordSuffixes[] = {
    { "maj9",   ChordType::Major9,          false },
    { "maj7",   ChordType::Major7,          false },
    { "M9",     ChordType::Major9,          true  },
    { "M7",     ChordType::Major7,          true  },
    { "min7b5", ChordType::HalfDiminished7, false },
    { "m7b5",   ChordType::HalfDiminished7, false },
    { "7sus4",  ChordType::Dominant7Sus4,   false },
    { "7sus",   ChordType::Dominant7Sus4,   false },
    { "min9",   ChordType::Minor9,          false },
    { "min7",   ChordType::Minor7,          false },
    { "min6",   ChordType::Minor6,          false },
    { "mmaj7",  ChordType::MinorMajor7,     false },
    { "dim7",   ChordType::Diminished7,     false },
    { "add9",   ChordType::Add9,            false },
    { "sus2",   ChordType::Sus2,            false },
    { "sus4",   ChordType::Sus4,            false },
    { "min",    ChordType::Minor,           false },
    { "dim",    ChordType::Diminished,      false },
    { "aug",    ChordType::Augmented,       false },
    { "sus",    ChordType::Sus4,            false },
    { "m9",     ChordType::Minor9,          false },
    { "m7",     ChordType::Minor7,          false },
    { "m6",     ChordType::Minor6,          false },
    { "m",      ChordType::Minor,           false },
    { "9",      ChordType::Dominant9,       false },
    { "7",      ChordType::Dominant7,       false },
    { "6",      ChordType::Major6,          false },
    { "5",      ChordType::Power,           false },
    { "+",      ChordType::Augmented,       false },
    { "o7",     ChordType::Diminished7,     false },
    { "o",      ChordType::Diminished,      false },
    { "",       ChordType::Major,           false },
};

const SuffixEntry kNumeralSuffixesUpper[] = {
    { "maj9",  ChordType::Major9,        false },
    { "maj7",  ChordType::Major7,        false },
    { "7sus4", ChordType::Dominant7Sus4, false },
    { "add9",  ChordType::Add9,          false },
    { "sus2",  ChordType::Sus2,          false },
    { "sus4",  ChordType::Sus4,          false },
    { "aug",   ChordType::Augmented,     false },
    { "9",     ChordType::Dominant9,     false },
    { "7",     ChordType::Dominant7,     false },
    { "6",     ChordType::Major6,        false },
    { "5",     ChordType::Power,         false },
    { "+",     ChordType::Augmented,     false },
    { "",      ChordType::Major,         false },
};

const SuffixEntry kNumeralSuffixesLower[] = {
    { "m7b5", ChordType::HalfDiminished7, false },
    { "maj7", ChordType::MinorMajor7,     false },
    { "dim7", ChordType::Diminished7,     false },
    { "add9", ChordType::Add9,            false },
    { "sus2", ChordType::Sus2,            false },
    { "sus4", ChordType::Sus4,            false },
    { "dim",  ChordType::Diminished,      false },
    { "o7",   ChordType::Diminished7,     false },
    { "9",    ChordType::Minor9,          false },
    { "7",    ChordType::Minor7,          false },
    { "6",    ChordType::Minor6,          false },
    { "o",    ChordType::Diminished,      false },
    { "",     ChordType::Minor,           false },
};

std::string toLower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

/** Replaces the typographic symbols people paste in with plain ASCII. */
std::string normalise(const std::string& token)
{
    static const std::pair<const char*, const char*> replacements[] = {
        { "\xE2\x99\xAF", "#" },   // sharp sign
        { "\xE2\x99\xAD", "b" },   // flat sign
        { "\xC2\xB0",     "dim" }, // degree sign
        { "\xC3\xB8",     "m7b5" },// slashed o
        { "\xE2\x88\x86", "maj7" },// increment (delta)
        { "\xCE\x94",     "maj7" },// greek delta
    };

    std::string out = token;
    for (const auto& [from, to] : replacements)
    {
        size_t at = 0;
        while ((at = out.find(from, at)) != std::string::npos)
        {
            out.replace(at, std::strlen(from), to);
            at += std::strlen(to);
        }
    }
    return out;
}

bool matchSuffix(const std::string& suffix, const SuffixEntry* table, size_t count, ChordType& out)
{
    const std::string lowered = toLower(suffix);
    for (size_t i = 0; i < count; ++i)
    {
        const auto& entry = table[i];
        if (entry.caseSensitive ? (suffix == entry.text) : (lowered == toLower(entry.text)))
        {
            out = entry.type;
            return true;
        }
    }
    return false;
}

/** Reads a note letter plus optional accidental. Returns the characters used. */
int readRoot(const std::string& token, int& pitchClass)
{
    if (token.empty())
        return 0;

    static const int letters[7] = { 9, 11, 0, 2, 4, 5, 7 }; // A B C D E F G
    const char letter = static_cast<char>(std::toupper(static_cast<unsigned char>(token[0])));
    if (letter < 'A' || letter > 'G')
        return 0;

    pitchClass = letters[letter - 'A'];
    int used = 1;
    while (used < static_cast<int>(token.size()) && (token[static_cast<size_t>(used)] == '#'
                                                     || token[static_cast<size_t>(used)] == 'b'))
    {
        pitchClass = mod12(pitchClass + (token[static_cast<size_t>(used)] == '#' ? 1 : -1));
        ++used;
    }
    return used;
}

const std::vector<int>& referenceSteps(const Key& key)
{
    static const std::vector<int> majorSteps { 0, 2, 4, 5, 7, 9, 11 };
    const auto& steps = scaleIntervals(key.scale);
    return steps.size() == 7 ? steps : majorSteps;
}

} // namespace

bool parseChordSymbol(const std::string& rawToken, Chord& out)
{
    const std::string token = normalise(rawToken);
    if (token.empty())
        return false;

    int root = 0;
    const int used = readRoot(token, root);
    if (used == 0)
        return false;

    std::string rest = token.substr(static_cast<size_t>(used));

    int bass = -1;
    const auto slash = rest.find('/');
    if (slash != std::string::npos)
    {
        int bassPitch = 0;
        const std::string bassToken = rest.substr(slash + 1);
        if (readRoot(bassToken, bassPitch) == 0)
            return false;
        bass = bassPitch;
        rest = rest.substr(0, slash);
    }

    ChordType type = ChordType::Major;
    if (! matchSuffix(rest, kChordSuffixes, std::size(kChordSuffixes), type))
        return false;

    out = { root, type, bass };
    return true;
}

bool parseRomanNumeral(const std::string& rawToken, const Key& key, Chord& out)
{
    std::string token = normalise(rawToken);
    if (token.empty())
        return false;

    int accidental = 0;
    size_t pos = 0;
    while (pos < token.size() && (token[pos] == '#' || token[pos] == 'b'))
    {
        accidental += token[pos] == '#' ? 1 : -1;
        ++pos;
    }

    const size_t numeralStart = pos;
    while (pos < token.size())
    {
        const char c = static_cast<char>(std::tolower(static_cast<unsigned char>(token[pos])));
        if (c != 'i' && c != 'v')
            break;
        ++pos;
    }

    const std::string numeral = token.substr(numeralStart, pos - numeralStart);
    if (numeral.empty())
        return false;

    // The numeral has to be written in one case; that is what carries the quality.
    const bool upper = std::isupper(static_cast<unsigned char>(numeral[0])) != 0;
    for (char c : numeral)
        if ((std::isupper(static_cast<unsigned char>(c)) != 0) != upper)
            return false;

    static const std::map<std::string, int> degrees {
        { "i", 0 }, { "ii", 1 }, { "iii", 2 }, { "iv", 3 }, { "v", 4 }, { "vi", 5 }, { "vii", 6 }
    };
    const auto degree = degrees.find(toLower(numeral));
    if (degree == degrees.end())
        return false;

    ChordType type = ChordType::Major;
    const std::string suffix = token.substr(pos);
    const bool matched = upper
                             ? matchSuffix(suffix, kNumeralSuffixesUpper, std::size(kNumeralSuffixesUpper), type)
                             : matchSuffix(suffix, kNumeralSuffixesLower, std::size(kNumeralSuffixesLower), type);
    if (! matched)
        return false;

    const auto& steps = referenceSteps(key);
    const int root = mod12(key.tonic + steps[static_cast<size_t>(degree->second)] + accidental);
    out = { root, type, -1 };
    return true;
}

bool parseProgression(const std::string& text, const Key& key,
                      std::vector<Chord>& out, std::string& error)
{
    out.clear();
    error.clear();

    // "|", ",", "-", ">" and whitespace all separate chords, so "Am | F | C | G",
    // "Am F C G" and "i - VI - III - VII" all work.
    const auto isSeparator = [](char c)
    {
        return c == '|' || c == ',' || c == '-' || c == '>' || c == ';'
            || c == ' ' || c == '\t' || c == '\n' || c == '\r';
    };

    std::vector<std::string> tokens;
    std::string current;
    for (char c : text)
    {
        if (isSeparator(c))
        {
            if (! current.empty())
                tokens.push_back(current);
            current.clear();
            continue;
        }
        current += c;
    }
    if (! current.empty())
        tokens.push_back(current);

    if (tokens.empty())
    {
        error = "Type a progression, for example: Am | F | C | G";
        return false;
    }

    for (const auto& token : tokens)
    {
        Chord chord;
        if (parseChordSymbol(token, chord) || parseRomanNumeral(token, key, chord))
        {
            out.push_back(chord);
            continue;
        }
        error = "'" + token + "' is not a chord I recognise.";
        out.clear();
        return false;
    }

    if (out.size() > 64)
    {
        error = "That is more than 64 chords - trim it down.";
        out.clear();
        return false;
    }
    return true;
}

std::string progressionToText(const std::vector<Chord>& chords, const Key& key)
{
    return progressionToText(chords, key.preferFlats());
}

std::string progressionToText(const std::vector<Chord>& chords, bool useFlats)
{
    std::string out;
    for (size_t i = 0; i < chords.size(); ++i)
    {
        if (i > 0)
            out += " | ";
        out += chords[i].name(useFlats);
    }
    return out;
}

std::string progressionToRoman(const std::vector<Chord>& chords, const Key& key)
{
    std::string out;
    for (size_t i = 0; i < chords.size(); ++i)
    {
        if (i > 0)
            out += " | ";
        out += chords[i].romanNumeral(key);
    }
    return out;
}

Key guessKeyForProgression(const std::vector<Chord>& chords)
{
    if (chords.empty())
        return { 0, ScaleType::Major };

    // Score every key by how many chord tones it contains, with the first chord
    // treated as the tonic candidate.
    Key best { chords.front().root, ScaleType::Major };
    float bestScore = -1e9f;

    for (int tonic = 0; tonic < 12; ++tonic)
    {
        for (ScaleType scale : { ScaleType::Major, ScaleType::NaturalMinor,
                                 ScaleType::Dorian, ScaleType::Mixolydian })
        {
            const Key candidate { tonic, scale };
            float score = 0.0f;
            for (size_t i = 0; i < chords.size(); ++i)
            {
                const float weight = (i == 0 ? 1.6f : 1.0f);
                for (int pc : chords[i].pitchClasses())
                    score += candidate.contains(pc) ? weight : -weight;
            }
            if (tonic == chords.front().root)
                score += 2.5f;
            if (tonic == chords.back().root)
                score += 0.8f;

            // A minor first chord points at a minor key, and the other way round.
            const bool firstIsMinor = chords.front().containsPitchClass(mod12(chords.front().root + 3));
            if (tonic == chords.front().root && firstIsMinor == candidate.isMinorMode())
                score += 1.5f;

            if (score > bestScore)
            {
                bestScore = score;
                best = candidate;
            }
        }
    }
    return best;
}

Analysis analysisFromProgression(const std::vector<Chord>& chords,
                                 const Key& key,
                                 double bpm,
                                 TimeSignature timeSignature,
                                 int beatsPerChord,
                                 int repeats,
                                 int ppq)
{
    Analysis analysis;
    if (chords.empty())
    {
        analysis.message = "No chords to build a progression from.";
        return analysis;
    }

    analysis.ppq = ppq;
    analysis.bpm = bpm;
    analysis.timeSignature = timeSignature;
    analysis.key = key;
    analysis.keyConfidence = 1.0f;
    analysis.role = SourceRole::Unknown;

    const int64_t barTicks = harmonia::ticksPerBar(timeSignature, ppq);
    const int64_t beatTicks = static_cast<int64_t>(ppq) * 4 / std::max(1, timeSignature.denominator);
    const int64_t chordTicks = beatsPerChord > 0 ? beatTicks * beatsPerChord : barTicks;

    int64_t tick = 0;
    for (int repeat = 0; repeat < std::max(1, repeats); ++repeat)
    {
        for (const auto& chord : chords)
        {
            ChordSegment segment;
            segment.startTick = tick;
            segment.lengthTick = chordTicks;
            segment.chord = chord;
            segment.confidence = 1.0f;
            analysis.progression.push_back(segment);
            tick += chordTicks;
        }
    }

    // Round the clip out to whole bars.
    analysis.lengthTicks = ((tick + barTicks - 1) / barTicks) * barTicks;
    analysis.bars = static_cast<int>(analysis.lengthTicks / std::max<int64_t>(1, barTicks));
    if (! analysis.progression.empty() && tick < analysis.lengthTicks)
        analysis.progression.back().lengthTick += analysis.lengthTicks - tick;

    // No source clip means no groove to borrow; an empty grid makes the
    // generators fall back to their own feel.
    analysis.rhythm = RhythmProfile {};
    analysis.rhythm.notesPerBar = 0.0f;
    analysis.rhythm.lowestPitch = 60;
    analysis.rhythm.highestPitch = 60;

    analysis.valid = true;
    return analysis;
}

void applyProgressionTo(Analysis& analysis, const std::vector<Chord>& chords)
{
    if (chords.empty() || ! analysis.valid)
        return;

    const int64_t total = std::max<int64_t>(1, analysis.lengthTicks);
    const int64_t beatTicks = std::max<int64_t>(1, analysis.ticksPerBeat());
    const auto count = static_cast<int64_t>(chords.size());

    analysis.progression.clear();
    analysis.progression.reserve(chords.size());

    for (int64_t i = 0; i < count; ++i)
    {
        int64_t start = total * i / count;
        int64_t end = total * (i + 1) / count;

        // Land the chord changes on the beat grid when the chords are long
        // enough for that to make sense.
        if (total / count >= beatTicks)
        {
            start = (start + beatTicks / 2) / beatTicks * beatTicks;
            end = (end + beatTicks / 2) / beatTicks * beatTicks;
        }
        if (i == 0)
            start = 0;
        if (i == count - 1)
            end = total;

        ChordSegment segment;
        segment.startTick = start;
        segment.lengthTick = std::max<int64_t>(1, end - start);
        segment.chord = chords[static_cast<size_t>(i)];
        segment.confidence = 1.0f;
        analysis.progression.push_back(segment);
    }
}

} // namespace harmonia
