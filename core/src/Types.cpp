#include "harmonia/Types.h"

#include <algorithm>

namespace harmonia
{

int64_t ticksPerBar(const TimeSignature& ts, int ppq) noexcept
{
    const int num = ts.numerator > 0 ? ts.numerator : 4;
    const int den = ts.denominator > 0 ? ts.denominator : 4;
    return static_cast<int64_t>(ppq) * 4 * num / den;
}

void NoteSequence::clear()
{
    notes.clear();
    timeSignatures.clear();
    tempos.clear();
    name.clear();
    loopLengthTicks = 0;
}

void NoteSequence::sort()
{
    std::stable_sort(notes.begin(), notes.end(), [](const Note& a, const Note& b)
    {
        if (a.startTick != b.startTick)
            return a.startTick < b.startTick;
        return a.pitch < b.pitch;
    });
    std::stable_sort(timeSignatures.begin(), timeSignatures.end(),
                     [](const TimeSignature& a, const TimeSignature& b) { return a.startTick < b.startTick; });
    std::stable_sort(tempos.begin(), tempos.end(),
                     [](const TempoEvent& a, const TempoEvent& b) { return a.startTick < b.startTick; });
}

int64_t NoteSequence::lengthTicks() const noexcept
{
    int64_t end = 0;
    for (const auto& n : notes)
        end = std::max(end, n.endTick());
    return end;
}

int64_t NoteSequence::playbackLengthTicks() const noexcept
{
    return loopLengthTicks > 0 ? loopLengthTicks : lengthTicks();
}

void NoteSequence::trimToLoop()
{
    if (loopLengthTicks <= 0)
        return;

    std::vector<Note> kept;
    kept.reserve(notes.size());
    for (auto& n : notes)
    {
        if (n.startTick >= loopLengthTicks)
            continue;
        n.lengthTick = std::min(n.lengthTick, loopLengthTicks - n.startTick);
        if (n.lengthTick > 0)
            kept.push_back(n);
    }
    notes.swap(kept);
}

int64_t NoteSequence::firstTick() const noexcept
{
    if (notes.empty())
        return 0;
    int64_t first = notes.front().startTick;
    for (const auto& n : notes)
        first = std::min(first, n.startTick);
    return first;
}

double NoteSequence::bpm() const noexcept
{
    return tempos.empty() ? 120.0 : tempos.front().bpm;
}

TimeSignature NoteSequence::timeSignatureAt(int64_t tick) const noexcept
{
    TimeSignature current;
    for (const auto& ts : timeSignatures)
    {
        if (ts.startTick <= tick)
            current = ts;
        else
            break;
    }
    return current;
}

int64_t NoteSequence::ticksPerBar() const noexcept
{
    return harmonia::ticksPerBar(timeSignatureAt(0), ppq);
}

int NoteSequence::barCount() const noexcept
{
    const int64_t tpb = ticksPerBar();
    if (tpb <= 0)
        return 0;
    return static_cast<int>((lengthTicks() + tpb - 1) / tpb);
}

void NoteSequence::transpose(int semitones)
{
    for (auto& n : notes)
        n.pitch = std::clamp(n.pitch + semitones, 0, 127);
}

void NoteSequence::changePPQ(int newPPQ)
{
    if (newPPQ <= 0 || newPPQ == ppq)
        return;

    const double scale = static_cast<double>(newPPQ) / static_cast<double>(ppq);
    const auto rescale = [scale](int64_t t) { return static_cast<int64_t>(static_cast<double>(t) * scale + 0.5); };

    for (auto& n : notes)
    {
        const int64_t start = rescale(n.startTick);
        const int64_t end = rescale(n.endTick());
        n.startTick = start;
        n.lengthTick = std::max<int64_t>(1, end - start);
    }
    for (auto& ts : timeSignatures)
        ts.startTick = rescale(ts.startTick);
    for (auto& t : tempos)
        t.startTick = rescale(t.startTick);
    loopLengthTicks = rescale(loopLengthTicks);

    ppq = newPPQ;
}

} // namespace harmonia
