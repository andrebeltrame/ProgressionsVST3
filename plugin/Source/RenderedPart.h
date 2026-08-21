#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include "harmonia/Types.h"

#include <algorithm>
#include <vector>

/** A generated part flattened into note on/off events measured in quarter notes,
    ready for the audio thread. Reference counted so the message thread can swap
    a new one in while the audio thread still holds the old one. */
struct RenderedPart : public juce::ReferenceCountedObject
{
    using Ptr = juce::ReferenceCountedObjectPtr<RenderedPart>;

    struct Event
    {
        double ppq = 0.0;
        bool noteOn = false;
        int pitch = 60;
        int velocity = 100;
    };

    std::vector<Event> events;
    double lengthPPQ = 4.0;

    static Ptr fromSequence(const harmonia::NoteSequence& sequence, int channel)
    {
        Ptr part = new RenderedPart();
        const auto ppq = static_cast<double>(std::max(1, sequence.ppq));

        for (const auto& note : sequence.notes)
        {
            part->events.push_back({ static_cast<double>(note.startTick) / ppq, true, note.pitch, note.velocity });
            part->events.push_back({ static_cast<double>(note.endTick()) / ppq, false, note.pitch, 0 });
        }

        std::sort(part->events.begin(), part->events.end(), [](const Event& a, const Event& b)
        {
            if (a.ppq != b.ppq)
                return a.ppq < b.ppq;
            return static_cast<int>(a.noteOn) < static_cast<int>(b.noteOn); // offs first
        });

        const auto barTicks = static_cast<double>(std::max<int64_t>(1, sequence.ticksPerBar()));
        const auto lengthTicks = static_cast<double>(sequence.lengthTicks());
        const double bars = std::max(1.0, std::ceil(lengthTicks / barTicks));
        part->lengthPPQ = std::max(1.0, bars * barTicks / ppq);

        juce::ignoreUnused(channel);
        return part;
    }
};
