#include "Fixtures.h"
#include "TestHarness.h"

#include "harmonia/MidiFile.h"

using namespace harmonia;

TEST(MidiRoundTripPreservesNotes)
{
    auto original = fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 } });
    original.name = "test clip";

    const auto bytes = midi::writeToMemory(original);
    CHECK(bytes.size() > 22);

    NoteSequence loaded;
    std::string error;
    CHECK(midi::readFromMemory(bytes.data(), bytes.size(), loaded, error));
    CHECK(error.empty());

    CHECK_EQ(loaded.notes.size(), original.notes.size());
    CHECK_EQ(loaded.ppq, kPPQ);
    for (size_t i = 0; i < loaded.notes.size(); ++i)
    {
        CHECK_EQ(loaded.notes[i].pitch, original.notes[i].pitch);
        CHECK_EQ(loaded.notes[i].startTick, original.notes[i].startTick);
        CHECK_EQ(loaded.notes[i].lengthTick, original.notes[i].lengthTick);
        CHECK_EQ(loaded.notes[i].velocity, original.notes[i].velocity);
    }
}

TEST(MidiRoundTripPreservesTempoAndMeter)
{
    auto original = fixtures::emptySequence(93.5);
    original.timeSignatures = { { 3, 4, 0 } };
    original.notes.push_back({ 0, 480, 60, 100, 0 });

    const auto bytes = midi::writeToMemory(original);
    NoteSequence loaded;
    std::string error;
    CHECK(midi::readFromMemory(bytes.data(), bytes.size(), loaded, error));

    CHECK_NEAR(loaded.bpm(), 93.5, 0.2);
    CHECK_EQ(loaded.timeSignatureAt(0).numerator, 3);
    CHECK_EQ(loaded.timeSignatureAt(0).denominator, 4);
    CHECK_EQ(loaded.ticksPerBar(), static_cast<int64_t>(kPPQ) * 3);
}

TEST(MidiReadRescalesToInternalResolution)
{
    // Hand-built format 0 file at 96 PPQ: one quarter note C4 at tick 0.
    const std::vector<uint8_t> file {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,12,
        0x00, 0x90, 60, 100,
        0x60, 0x80, 60, 0,
        0x00, 0xff, 0x2f, 0x00
    };

    NoteSequence loaded;
    std::string error;
    CHECK(midi::readFromMemory(file.data(), file.size(), loaded, error));
    CHECK_EQ(loaded.notes.size(), size_t(1));
    CHECK_EQ(loaded.ppq, kPPQ);
    CHECK_EQ(loaded.notes[0].lengthTick, static_cast<int64_t>(kPPQ));
}

TEST(MidiRejectsGarbage)
{
    const std::vector<uint8_t> junk { 'n', 'o', 'p', 'e', 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11 };
    NoteSequence loaded;
    std::string error;
    CHECK(! midi::readFromMemory(junk.data(), junk.size(), loaded, error));
    CHECK(! error.empty());
}

TEST(MidiHandlesRunningStatus)
{
    // Two note-ons using running status, then two note-offs (velocity 0 form).
    const std::vector<uint8_t> file {
        'M','T','h','d', 0,0,0,6, 0,0, 0,1, 0,96,
        'M','T','r','k', 0,0,0,17,
        0x00, 0x90, 60, 100,
        0x00,       64, 100,
        0x60,       60, 0,
        0x00,       64, 0,
        0x00, 0xff, 0x2f, 0x00
    };

    NoteSequence loaded;
    std::string error;
    CHECK(midi::readFromMemory(file.data(), file.size(), loaded, error));
    CHECK_EQ(loaded.notes.size(), size_t(2));
}
