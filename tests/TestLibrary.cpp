#include "Fixtures.h"
#include "TestHarness.h"

#include "harmonia/Json.h"
#include "harmonia/Library.h"
#include "harmonia/MidiFile.h"

#include "harmonia/Engine.h"

#include <algorithm>
#include <filesystem>
#include <fstream>

using namespace harmonia;
namespace fs = std::filesystem;

namespace
{

/** Builds a throwaway folder tree that looks like a producer's sample drive. */
struct TemporaryLibrary
{
    fs::path root;

    TemporaryLibrary()
    {
        root = fs::temp_directory_path() / "harmonia_test_library";
        fs::remove_all(root);

        write("Deep House/Bass/deep_bass_01.mid", fixtures::bassClip({ 36, 33, 29, 31 }));
        write("Deep House/Pads/warm_pad.mid", fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 } }));
        write("Melodic House/Leads/lead_hook.mid", fixtures::melodyClip({ 72, 74, 76, 77, 79, 77, 76, 74 }));
        write("Melodic House/Plucks/pluck_seq.mid", fixtures::melodyClip({ 72, 79, 76, 84, 72, 79, 76, 84 }));
        write("Drums/Percussion/groove.mid", drumClip());
        write("notes.txt", std::string("not a midi file"));
        write("Deep House/Bass/broken.mid", std::string("MThd but not really"));
    }

    ~TemporaryLibrary()
    {
        std::error_code ec;
        fs::remove_all(root, ec);
    }

    static NoteSequence drumClip()
    {
        auto sequence = fixtures::emptySequence();
        for (int i = 0; i < 8; ++i)
            sequence.notes.push_back({ static_cast<int64_t>(i) * (kPPQ / 2), kPPQ / 4, 36 + (i % 2) * 6, 100, 9 });
        sequence.sort();
        return sequence;
    }

    void write(const std::string& relative, const NoteSequence& sequence) const
    {
        const auto path = root / relative;
        fs::create_directories(path.parent_path());
        std::string error;
        midi::writeToFile(path.string(), sequence, error);
    }

    void write(const std::string& relative, const std::string& contents) const
    {
        const auto path = root / relative;
        fs::create_directories(path.parent_path());
        std::ofstream(path.string(), std::ios::binary) << contents;
    }
};

const LibraryEntry* entryEndingWith(const LibraryIndex& index, const std::string& suffix)
{
    for (const auto& entry : index.entries)
        if (entry.relativePath.size() >= suffix.size()
            && entry.relativePath.compare(entry.relativePath.size() - suffix.size(), suffix.size(), suffix) == 0)
            return &entry;
    return nullptr;
}

} // namespace

TEST(JsonRoundTrip)
{
    auto root = json::Value::object();
    root.set("name", json::Value("deep \"house\"\n"));
    root.set("count", json::Value(42));
    root.set("ratio", json::Value(0.5));
    root.set("on", json::Value(true));

    auto list = json::Value::array();
    list.push(json::Value(1));
    list.push(json::Value("two"));
    root.set("list", std::move(list));

    json::Value parsed;
    std::string error;
    CHECK(json::parse(root.toString(), parsed, error));
    CHECK(error.empty());
    CHECK_EQ(parsed["name"].asString(), std::string("deep \"house\"\n"));
    CHECK_EQ(parsed["count"].asInt(), 42);
    CHECK_NEAR(parsed["ratio"].asNumber(), 0.5, 1e-9);
    CHECK(parsed["on"].asBool());
    CHECK_EQ(parsed["list"].items().size(), size_t(2));
    CHECK_EQ(parsed["list"].items()[1].asString(), std::string("two"));
    CHECK(parsed["missing"].isNull());
}

TEST(JsonRejectsGarbage)
{
    json::Value value;
    std::string error;
    CHECK(! json::parse("{\"a\": }", value, error));
    CHECK(! error.empty());
    CHECK(! json::parse("[1, 2", value, error));
    CHECK(! json::parse("{} trailing", value, error));
}

TEST(ScanningAFolderTree)
{
    const TemporaryLibrary library;
    std::vector<std::string> errors;
    const auto index = scanDirectory(library.root.string(), {}, {}, &errors);

    // Five readable MIDI files; the .txt is ignored and the broken one is reported.
    CHECK_EQ(index.entries.size(), size_t(5));
    CHECK_EQ(errors.size(), size_t(1));
    CHECK(errors.front().find("broken.mid") != std::string::npos);

    const auto* bass = entryEndingWith(index, "deep_bass_01.mid");
    CHECK(bass != nullptr);
    if (bass != nullptr)
    {
        CHECK_EQ(bass->folderRole, std::string("bass"));
        CHECK(bass->role == SourceRole::Bass);
        CHECK_EQ(bass->progression, std::string("C | Am | F | G"));
        CHECK_EQ(bass->key.tonic, 0);
        CHECK(! bass->drums);
        CHECK(bass->rhythm.notesPerBar > 0.0f);

        const bool taggedByStyle = std::find(bass->tags.begin(), bass->tags.end(), "deep house") != bass->tags.end();
        CHECK(taggedByStyle);
    }

    const auto* pluck = entryEndingWith(index, "pluck_seq.mid");
    CHECK(pluck != nullptr);
    if (pluck != nullptr)
        CHECK_EQ(pluck->folderRole, std::string("pluck"));

    const auto* drums = entryEndingWith(index, "groove.mid");
    CHECK(drums != nullptr);
    if (drums != nullptr)
    {
        CHECK(drums->drums);
        CHECK(drums->progression.empty()); // percussion has no harmony worth storing
    }
}

TEST(IndexSurvivesSaveAndLoad)
{
    const TemporaryLibrary library;
    const auto index = scanDirectory(library.root.string(), {}, {}, nullptr);

    const auto indexPath = (fs::temp_directory_path() / "harmonia_test_index.json").string();
    std::string error;
    CHECK(saveIndex(indexPath, index, error));
    CHECK(error.empty());

    LibraryIndex loaded;
    CHECK(loadIndex(indexPath, loaded, error));
    CHECK_EQ(loaded.entries.size(), index.entries.size());
    CHECK_EQ(loaded.root, index.root);

    for (size_t i = 0; i < loaded.entries.size(); ++i)
    {
        const auto& a = index.entries[i];
        const auto& b = loaded.entries[i];
        CHECK_EQ(b.relativePath, a.relativePath);
        CHECK_EQ(b.folderRole, a.folderRole);
        CHECK_EQ(b.progression, a.progression);
        CHECK_EQ(b.key.tonic, a.key.tonic);
        CHECK(b.key.scale == a.key.scale);
        CHECK(b.role == a.role);
        CHECK_EQ(b.drums, a.drums);
        CHECK_EQ(b.bars, a.bars);
        CHECK_NEAR(b.bpm, a.bpm, 0.01);
        CHECK_NEAR(b.rhythm.notesPerBar, a.rhythm.notesPerBar, 0.01);
        for (size_t slot = 0; slot < b.rhythm.grid.size(); ++slot)
            CHECK_NEAR(b.rhythm.grid[slot], a.rhythm.grid[slot], 0.01);
    }

    fs::remove(indexPath);
}

TEST(QueryingTheLibrary)
{
    const TemporaryLibrary library;
    const auto index = scanDirectory(library.root.string(), {}, {}, nullptr);

    LibraryQuery byRole;
    byRole.role = "bass";
    CHECK_EQ(queryLibrary(index, byRole).size(), size_t(1));

    LibraryQuery byTag;
    byTag.tag = "melodic house";
    CHECK_EQ(queryLibrary(index, byTag).size(), size_t(2));

    LibraryQuery byKey;
    byKey.tonic = 0;
    const auto inC = queryLibrary(index, byKey);
    CHECK(! inC.empty());
    for (const auto* entry : inC)
        CHECK_EQ(entry->key.tonic, 0);

    LibraryQuery drumsExcluded;
    CHECK_EQ(queryLibrary(index, drumsExcluded).size(), size_t(4));
    drumsExcluded.excludeDrums = false;
    CHECK_EQ(queryLibrary(index, drumsExcluded).size(), size_t(5));

    LibraryQuery limited;
    limited.excludeDrums = false;
    limited.limit = 2;
    CHECK_EQ(queryLibrary(index, limited).size(), size_t(2));

    LibraryQuery byProgression;
    byProgression.containing = "Am";
    CHECK(! queryLibrary(index, byProgression).empty());

    const auto tags = tagHistogram(index);
    CHECK(! tags.empty());
}

TEST(ScanOptionsAreRespected)
{
    const TemporaryLibrary library;

    ScanOptions shallow;
    shallow.recursive = false;
    CHECK(scanDirectory(library.root.string(), shallow, {}, nullptr).entries.empty());

    ScanOptions capped;
    capped.maxFiles = 2;
    CHECK(scanDirectory(library.root.string(), capped, {}, nullptr).entries.size() <= size_t(2));

    ScanOptions noDrums;
    noDrums.skipDrums = true;
    const auto index = scanDirectory(library.root.string(), noDrums, {}, nullptr);
    for (const auto& entry : index.entries)
        CHECK(! entry.drums);
}

TEST(ALibraryClipCanDonateItsGroove)
{
    const TemporaryLibrary library;
    const auto index = scanDirectory(library.root.string(), {}, {}, nullptr);
    const auto* donor = entryEndingWith(index, "deep_bass_01.mid");
    CHECK(donor != nullptr);
    if (donor == nullptr)
        return;

    // A pad clip has no groove of its own worth borrowing; the bass does.
    Engine engine;
    engine.setSource(fixtures::chordClip({ { 60, 64, 67 }, { 57, 60, 64 } }));

    GenerateOptions plain;
    plain.part = PartType::Melody;
    plain.humanize = 0.0f;
    plain.seed = 5;

    GenerateOptions donated = plain;
    donated.useGrooveDonor = true;
    donated.grooveDonor = donor->rhythm;

    const auto withoutDonor = engine.generate(plain);
    const auto withDonor = engine.generate(donated);
    CHECK(! withDonor.empty());

    std::vector<int64_t> plainStarts, donorStarts;
    for (const auto& note : withoutDonor.notes)
        plainStarts.push_back(note.startTick);
    for (const auto& note : withDonor.notes)
        donorStarts.push_back(note.startTick);
    CHECK(plainStarts != donorStarts);

    // The donor grid dominates: nearly every onset lands on a 16th it plays.
    const int64_t slot = kPPQ / 4;
    int onDonorSlots = 0;
    for (const auto& note : withDonor.notes)
    {
        const auto position = static_cast<size_t>((note.startTick % (kPPQ * 4)) / slot);
        if (donor->rhythm.grid[position] > 0.0f)
            ++onDonorSlots;
    }
    CHECK(! withDonor.notes.empty());
    CHECK(onDonorSlots * 10 >= static_cast<int>(withDonor.notes.size()) * 8);
}
