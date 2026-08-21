#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace harmonia
{

/** Internal tick resolution used by every sequence produced by the engine. */
constexpr int kPPQ = 960;

struct Note
{
    int64_t startTick = 0;
    int64_t lengthTick = 0;
    int pitch = 60;     // 0..127
    int velocity = 100; // 1..127
    int channel = 0;    // 0..15

    int64_t endTick() const noexcept { return startTick + lengthTick; }
};

struct TimeSignature
{
    int numerator = 4;
    int denominator = 4;
    int64_t startTick = 0;
};

struct TempoEvent
{
    double bpm = 120.0;
    int64_t startTick = 0;
};

/** A bag of notes plus the meta events needed to render it as a standard MIDI file. */
struct NoteSequence
{
    std::vector<Note> notes;
    std::vector<TimeSignature> timeSignatures;
    std::vector<TempoEvent> tempos;
    int ppq = kPPQ;
    std::string name;

    bool empty() const noexcept { return notes.empty(); }
    void clear();

    /** Sorts by start tick, then pitch. */
    void sort();

    int64_t lengthTicks() const noexcept;
    int64_t firstTick() const noexcept;

    double bpm() const noexcept;
    TimeSignature timeSignatureAt(int64_t tick) const noexcept;

    int64_t ticksPerBeat() const noexcept { return ppq; }
    int64_t ticksPerBar() const noexcept;
    int barCount() const noexcept;

    void transpose(int semitones);
    /** Rewrites every tick so the sequence uses the given resolution. */
    void changePPQ(int newPPQ);
};

/** Ticks per bar for an arbitrary time signature at a given resolution. */
int64_t ticksPerBar(const TimeSignature& ts, int ppq) noexcept;

} // namespace harmonia
