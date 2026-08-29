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
    /** The loop this sequence is meant to fill, in ticks. Zero means "as long as
        the notes happen to be". A generated part sets it so the written MIDI file
        declares the loop rather than ending on its last note-off - a host sizes an
        imported clip by the end of the file, so a note ringing past the final bar
        would otherwise stretch the clip past the loop. */
    int64_t loopLengthTicks = 0;

    bool empty() const noexcept { return notes.empty(); }
    void clear();

    /** Sorts by start tick, then pitch. */
    void sort();

    /** The end of the last note. */
    int64_t lengthTicks() const noexcept;
    /** loopLengthTicks when one was declared, the end of the last note otherwise. */
    int64_t playbackLengthTicks() const noexcept;
    /** Shortens anything crossing the loop so nothing sounds past it. Notes that
        start on or after the loop end are dropped. Does nothing without a loop. */
    void trimToLoop();
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
