#pragma once

#include "harmonia/Theory.h"
#include "harmonia/Types.h"

#include <array>
#include <string>
#include <vector>

namespace harmonia
{

/** What the incoming MIDI clip sounds like it is doing. */
enum class SourceRole
{
    Unknown,
    Bass,
    Chords,  // pad / keys / anything sustained and polyphonic
    Lead,    // monophonic line in a melodic register
    Arp
};

const char* toString(SourceRole role);
bool sourceRoleFromString(const std::string& text, SourceRole& out);

struct ChordSegment
{
    int64_t startTick = 0;
    int64_t lengthTick = 0;
    Chord chord;
    float confidence = 0.0f;

    int64_t endTick() const noexcept { return startTick + lengthTick; }
};

struct RhythmProfile
{
    /** Onset weight for each 16th-note slot of a bar, normalised to a 0..1 peak. */
    std::array<float, 16> grid { };
    float notesPerBar = 0.0f;
    float averageLengthBeats = 0.5f;
    float averageVelocity = 90.0f;
    float syncopation = 0.0f;  // 0 = everything on downbeats, 1 = everything off-grid
    float polyphony = 1.0f;    // mean number of simultaneously sounding notes
    int lowestPitch = 60;
    int highestPitch = 72;
    bool monophonic = true;

    /** Highest-weighted 16th slots, most prominent first. */
    std::vector<int> strongestSlots(int count) const;
};

struct Analysis
{
    bool valid = false;
    std::string message;

    Key key;
    float keyConfidence = 0.0f;
    std::vector<ChordSegment> progression;
    RhythmProfile rhythm;
    SourceRole role = SourceRole::Unknown;

    double bpm = 120.0;
    TimeSignature timeSignature;
    int ppq = kPPQ;
    int bars = 0;
    int64_t lengthTicks = 0;

    int64_t ticksPerBar() const noexcept { return harmonia::ticksPerBar(timeSignature, ppq); }
    int64_t ticksPerBeat() const noexcept { return ppq * 4 / (timeSignature.denominator > 0 ? timeSignature.denominator : 4); }

    const ChordSegment* chordAt(int64_t tick) const;
    /** "Dm7 | G7 | Cmaj7 | Cmaj7", spelled the way the key is written. */
    std::string progressionString() const;
    std::string progressionString(bool useFlats) const;
    /** "ii7 | V7 | Imaj7 | Imaj7" */
    std::string romanNumeralString() const;
};

struct AnalysisOptions
{
    /** Force the harmonic grid instead of letting the detector choose. 0 = auto. */
    int chordsPerBar = 0;
    /** Higher values make the detector hold on to a chord for longer. */
    float chordChangePenalty = 0.55f;
    /** Weight of the "this chord belongs to the detected key" bonus. */
    float diatonicBias = 0.22f;
    /** Override the detected key. */
    bool forceKey = false;
    Key key;
    /** Allow seventh chords in the detected progression. */
    bool allowSevenths = true;
};

Analysis analyze(const NoteSequence& sequence, const AnalysisOptions& options = {});

/** Duration+velocity weighted pitch class histogram, normalised so the peak is 1. */
std::array<float, 12> pitchClassHistogram(const NoteSequence& sequence);

/** Krumhansl-Schmuckler key detection with a light modal refinement pass. */
Key detectKey(const NoteSequence& sequence, float* confidenceOut = nullptr);

SourceRole detectRole(const NoteSequence& sequence, const RhythmProfile& rhythm);
RhythmProfile buildRhythmProfile(const NoteSequence& sequence, const TimeSignature& ts, int ppq);

} // namespace harmonia
