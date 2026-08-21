#include "harmonia/MidiFile.h"

#include <algorithm>
#include <cstring>
#include <fstream>
#include <map>

namespace harmonia::midi
{

namespace
{

struct Reader
{
    const uint8_t* data;
    size_t size;
    size_t pos = 0;

    bool ok(size_t need) const { return pos + need <= size; }
    uint8_t u8() { return ok(1) ? data[pos++] : uint8_t(0); }

    uint32_t u16()
    {
        const uint32_t a = u8();
        return (a << 8) | u8();
    }

    uint32_t u32()
    {
        const uint32_t a = u16();
        return (a << 16) | u16();
    }

    /** MIDI variable-length quantity. */
    uint32_t vlq()
    {
        uint32_t value = 0;
        for (int i = 0; i < 4; ++i)
        {
            const uint8_t b = u8();
            value = (value << 7) | (b & 0x7f);
            if ((b & 0x80) == 0)
                break;
        }
        return value;
    }
};

void putVlq(std::vector<uint8_t>& out, uint32_t value)
{
    uint8_t buffer[5];
    int count = 0;
    buffer[count++] = static_cast<uint8_t>(value & 0x7f);
    value >>= 7;
    while (value > 0 && count < 5)
    {
        buffer[count++] = static_cast<uint8_t>((value & 0x7f) | 0x80);
        value >>= 7;
    }
    for (int i = count - 1; i >= 0; --i)
        out.push_back(buffer[i]);
}

void putU16(std::vector<uint8_t>& out, uint16_t v)
{
    out.push_back(static_cast<uint8_t>(v >> 8));
    out.push_back(static_cast<uint8_t>(v & 0xff));
}

void putU32(std::vector<uint8_t>& out, uint32_t v)
{
    out.push_back(static_cast<uint8_t>(v >> 24));
    out.push_back(static_cast<uint8_t>((v >> 16) & 0xff));
    out.push_back(static_cast<uint8_t>((v >> 8) & 0xff));
    out.push_back(static_cast<uint8_t>(v & 0xff));
}

struct PendingNote
{
    int64_t startTick;
    int velocity;
};

} // namespace

bool readFromMemory(const uint8_t* data, size_t size, NoteSequence& out, std::string& error)
{
    out.clear();
    error.clear();

    if (data == nullptr || size < 14)
    {
        error = "File is too small to be a MIDI file.";
        return false;
    }

    Reader r { data, size, 0 };
    if (std::memcmp(data, "MThd", 4) != 0)
    {
        error = "Missing MThd header - this is not a Standard MIDI File.";
        return false;
    }
    r.pos = 4;

    const uint32_t headerLength = r.u32();
    const uint32_t format = r.u16();
    const uint32_t trackCount = r.u16();
    const int32_t division = static_cast<int16_t>(r.u16());
    r.pos = 8 + headerLength;

    if (format > 2)
    {
        error = "Unsupported MIDI format.";
        return false;
    }
    if (division <= 0)
    {
        error = "SMPTE time division is not supported.";
        return false;
    }

    const int sourcePPQ = division;
    out.ppq = sourcePPQ;

    for (uint32_t track = 0; track < trackCount && r.pos + 8 <= size; ++track)
    {
        if (std::memcmp(data + r.pos, "MTrk", 4) != 0)
        {
            // Skip unknown chunk.
            r.pos += 4;
            const uint32_t len = r.u32();
            r.pos += len;
            continue;
        }

        r.pos += 4;
        const uint32_t trackLength = r.u32();
        const size_t trackEnd = std::min(size, r.pos + trackLength);

        int64_t tick = 0;
        uint8_t runningStatus = 0;
        // key = (channel << 8) | pitch, so overlapping notes on different channels behave.
        std::map<int, std::vector<PendingNote>> sounding;

        while (r.pos < trackEnd)
        {
            tick += r.vlq();

            uint8_t status = r.data[r.pos];
            if (status & 0x80)
            {
                ++r.pos;
                if (status < 0xf0)
                    runningStatus = status;
            }
            else
            {
                status = runningStatus;
                if (status == 0)
                    break; // corrupt stream
            }

            const uint8_t type = status & 0xf0;
            const int channel = status & 0x0f;

            if (status == 0xff)
            {
                const uint8_t metaType = r.u8();
                const uint32_t length = r.vlq();
                const size_t metaStart = r.pos;

                if (metaType == 0x51 && length >= 3)
                {
                    const uint32_t usPerQuarter = (static_cast<uint32_t>(r.data[metaStart]) << 16)
                                                | (static_cast<uint32_t>(r.data[metaStart + 1]) << 8)
                                                | static_cast<uint32_t>(r.data[metaStart + 2]);
                    if (usPerQuarter > 0)
                        out.tempos.push_back({ 60000000.0 / static_cast<double>(usPerQuarter), tick });
                }
                else if (metaType == 0x58 && length >= 2)
                {
                    TimeSignature ts;
                    ts.numerator = r.data[metaStart];
                    ts.denominator = 1 << r.data[metaStart + 1];
                    ts.startTick = tick;
                    out.timeSignatures.push_back(ts);
                }
                else if (metaType == 0x03 && length > 0 && out.name.empty())
                {
                    out.name.assign(reinterpret_cast<const char*>(r.data + metaStart), length);
                }

                r.pos = metaStart + length;
                continue;
            }

            if (status == 0xf0 || status == 0xf7)
            {
                const uint32_t length = r.vlq();
                r.pos += length;
                continue;
            }

            switch (type)
            {
                case 0x80: // note off
                case 0x90: // note on
                {
                    const int pitch = r.u8() & 0x7f;
                    const int velocity = r.u8() & 0x7f;
                    const int key = (channel << 8) | pitch;

                    if (type == 0x90 && velocity > 0)
                    {
                        sounding[key].push_back({ tick, velocity });
                    }
                    else
                    {
                        auto it = sounding.find(key);
                        if (it != sounding.end() && ! it->second.empty())
                        {
                            const PendingNote pending = it->second.front();
                            it->second.erase(it->second.begin());

                            Note n;
                            n.startTick = pending.startTick;
                            n.lengthTick = std::max<int64_t>(1, tick - pending.startTick);
                            n.pitch = pitch;
                            n.velocity = pending.velocity;
                            n.channel = channel;
                            out.notes.push_back(n);
                        }
                    }
                    break;
                }
                case 0xa0:
                case 0xb0:
                case 0xe0:
                    r.pos += 2;
                    break;
                case 0xc0:
                case 0xd0:
                    r.pos += 1;
                    break;
                default:
                    r.pos = trackEnd;
                    break;
            }
        }

        // Any note left hanging gets closed at the end of the track.
        for (auto& [key, pendings] : sounding)
        {
            for (const auto& pending : pendings)
            {
                Note n;
                n.startTick = pending.startTick;
                n.lengthTick = std::max<int64_t>(1, tick - pending.startTick);
                n.pitch = key & 0xff;
                n.velocity = pending.velocity;
                n.channel = (key >> 8) & 0x0f;
                out.notes.push_back(n);
            }
        }

        r.pos = trackEnd;
    }

    if (out.notes.empty())
    {
        error = "No notes in this file (" + std::to_string(trackCount) + " track"
              + (trackCount == 1 ? "" : "s") + ", " + std::to_string(size) + " bytes).";
        return false;
    }

    out.sort();
    out.changePPQ(kPPQ);
    return true;
}

bool readFromFile(const std::string& path, NoteSequence& out, std::string& error)
{
    std::ifstream stream(path, std::ios::binary);
    if (! stream)
    {
        error = "Could not open '" + path + "'.";
        return false;
    }

    std::vector<uint8_t> bytes((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());
    return readFromMemory(bytes.data(), bytes.size(), out, error);
}

std::vector<uint8_t> writeToMemory(const NoteSequence& seq)
{
    NoteSequence copy = seq;
    copy.sort();

    std::vector<uint8_t> out;
    out.insert(out.end(), { 'M', 'T', 'h', 'd' });
    putU32(out, 6);
    putU16(out, 1);
    putU16(out, 2);
    putU16(out, static_cast<uint16_t>(copy.ppq));

    const auto appendTrack = [&out](const std::vector<uint8_t>& events)
    {
        out.insert(out.end(), { 'M', 'T', 'r', 'k' });
        putU32(out, static_cast<uint32_t>(events.size()));
        out.insert(out.end(), events.begin(), events.end());
    };

    // ---- Track 0: tempo map -------------------------------------------------
    {
        std::vector<uint8_t> track;
        int64_t last = 0;

        if (! copy.name.empty())
        {
            putVlq(track, 0);
            track.insert(track.end(), { 0xff, 0x03 });
            const auto name = copy.name.substr(0, 127);
            putVlq(track, static_cast<uint32_t>(name.size()));
            track.insert(track.end(), name.begin(), name.end());
        }

        auto tempos = copy.tempos;
        if (tempos.empty())
            tempos.push_back({ 120.0, 0 });
        for (const auto& t : tempos)
        {
            putVlq(track, static_cast<uint32_t>(t.startTick - last));
            last = t.startTick;
            const uint32_t usPerQuarter = static_cast<uint32_t>(60000000.0 / std::max(1.0, t.bpm));
            track.insert(track.end(), { 0xff, 0x51, 0x03 });
            track.push_back(static_cast<uint8_t>((usPerQuarter >> 16) & 0xff));
            track.push_back(static_cast<uint8_t>((usPerQuarter >> 8) & 0xff));
            track.push_back(static_cast<uint8_t>(usPerQuarter & 0xff));
        }

        auto signatures = copy.timeSignatures;
        if (signatures.empty())
            signatures.push_back({ 4, 4, 0 });
        for (const auto& ts : signatures)
        {
            putVlq(track, static_cast<uint32_t>(std::max<int64_t>(0, ts.startTick - last)));
            last = std::max(last, ts.startTick);
            int denominatorPower = 0;
            for (int d = ts.denominator; d > 1; d >>= 1)
                ++denominatorPower;
            track.insert(track.end(), { 0xff, 0x58, 0x04 });
            track.push_back(static_cast<uint8_t>(ts.numerator));
            track.push_back(static_cast<uint8_t>(denominatorPower));
            track.push_back(24);
            track.push_back(8);
        }

        putVlq(track, 0);
        track.insert(track.end(), { 0xff, 0x2f, 0x00 });
        appendTrack(track);
    }

    // ---- Track 1: the notes -------------------------------------------------
    {
        struct Event
        {
            int64_t tick;
            uint8_t status;
            uint8_t data1;
            uint8_t data2;
            int order; // note-offs before note-ons at the same tick
        };

        std::vector<Event> events;
        events.reserve(copy.notes.size() * 2);
        for (const auto& n : copy.notes)
        {
            const uint8_t channel = static_cast<uint8_t>(n.channel & 0x0f);
            events.push_back({ n.startTick, static_cast<uint8_t>(0x90 | channel),
                               static_cast<uint8_t>(std::clamp(n.pitch, 0, 127)),
                               static_cast<uint8_t>(std::clamp(n.velocity, 1, 127)), 1 });
            events.push_back({ n.endTick(), static_cast<uint8_t>(0x80 | channel),
                               static_cast<uint8_t>(std::clamp(n.pitch, 0, 127)), 0, 0 });
        }

        std::stable_sort(events.begin(), events.end(), [](const Event& a, const Event& b)
        {
            if (a.tick != b.tick)
                return a.tick < b.tick;
            return a.order < b.order;
        });

        std::vector<uint8_t> track;
        int64_t last = 0;
        for (const auto& e : events)
        {
            putVlq(track, static_cast<uint32_t>(std::max<int64_t>(0, e.tick - last)));
            last = e.tick;
            track.push_back(e.status);
            track.push_back(e.data1);
            track.push_back(e.data2);
        }

        putVlq(track, 0);
        track.insert(track.end(), { 0xff, 0x2f, 0x00 });
        appendTrack(track);
    }

    return out;
}

bool writeToFile(const std::string& path, const NoteSequence& seq, std::string& error)
{
    const auto bytes = writeToMemory(seq);
    std::ofstream stream(path, std::ios::binary);
    if (! stream)
    {
        error = "Could not write to '" + path + "'.";
        return false;
    }
    stream.write(reinterpret_cast<const char*>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
    return stream.good();
}

} // namespace harmonia::midi
