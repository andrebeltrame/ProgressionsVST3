#pragma once

#include "harmonia/Types.h"

#include <vector>

namespace fixtures
{

inline harmonia::NoteSequence emptySequence(double bpm = 120.0)
{
    harmonia::NoteSequence sequence;
    sequence.ppq = harmonia::kPPQ;
    sequence.tempos.push_back({ bpm, 0 });
    sequence.timeSignatures.push_back({ 4, 4, 0 });
    return sequence;
}

/** One sustained chord per bar. */
inline harmonia::NoteSequence chordClip(const std::vector<std::vector<int>>& chordsPerBar)
{
    auto sequence = emptySequence();
    const int64_t bar = harmonia::kPPQ * 4;
    for (size_t i = 0; i < chordsPerBar.size(); ++i)
    {
        for (int pitch : chordsPerBar[i])
        {
            harmonia::Note note;
            note.startTick = static_cast<int64_t>(i) * bar;
            note.lengthTick = bar - 20;
            note.pitch = pitch;
            note.velocity = 90;
            sequence.notes.push_back(note);
        }
    }
    sequence.sort();
    return sequence;
}

/** Four quarter notes per bar on the given bass pitch. */
inline harmonia::NoteSequence bassClip(const std::vector<int>& pitchPerBar)
{
    auto sequence = emptySequence();
    const int64_t beat = harmonia::kPPQ;
    for (size_t i = 0; i < pitchPerBar.size(); ++i)
    {
        for (int step = 0; step < 4; ++step)
        {
            harmonia::Note note;
            note.startTick = (static_cast<int64_t>(i) * 4 + step) * beat;
            note.lengthTick = beat - 30;
            note.pitch = pitchPerBar[i] + (step == 2 ? 12 : 0);
            note.velocity = step == 0 ? 110 : 92;
            sequence.notes.push_back(note);
        }
    }
    sequence.sort();
    return sequence;
}

/** Eighth-note monophonic line from a list of pitches. */
inline harmonia::NoteSequence melodyClip(const std::vector<int>& pitches)
{
    auto sequence = emptySequence();
    const int64_t eighth = harmonia::kPPQ / 2;
    for (size_t i = 0; i < pitches.size(); ++i)
    {
        harmonia::Note note;
        note.startTick = static_cast<int64_t>(i) * eighth;
        note.lengthTick = eighth - 20;
        note.pitch = pitches[i];
        note.velocity = 96;
        sequence.notes.push_back(note);
    }
    sequence.sort();
    return sequence;
}

} // namespace fixtures
