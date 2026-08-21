#include "harmonia/Library.h"

#include "harmonia/Json.h"
#include "harmonia/MidiFile.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

namespace fs = std::filesystem;

namespace harmonia
{

namespace
{

std::string toLower(std::string text)
{
    for (char& c : text)
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    return text;
}

bool contains(const std::string& haystack, const std::string& needle)
{
    return ! needle.empty() && haystack.find(needle) != std::string::npos;
}

/** Recognises what a folder is for from its name: ".../Deep House/Bass/..." */
std::string roleFromPath(const std::string& lowerPath)
{
    static const std::pair<const char*, const char*> table[] = {
        { "drum", "drums" }, { "perc", "drums" }, { "kick", "drums" },
        { "hihat", "drums" }, { "hat", "drums" }, { "snare", "drums" }, { "clap", "drums" },
        { "808", "bass" }, { "bass", "bass" }, { "sub", "bass" },
        { "pluck", "pluck" }, { "pluk", "pluck" },
        { "arp", "arp" }, { "sequence", "arp" }, { "seq", "arp" },
        { "lead", "lead" }, { "melod", "lead" }, { "top", "lead" }, { "hook", "lead" },
        { "pad", "pad" }, { "chord", "chords" }, { "keys", "chords" },
        { "piano", "chords" }, { "rhodes", "chords" }, { "stab", "chords" },
    };

    for (const auto& [needle, role] : table)
        if (contains(lowerPath, needle))
            return role;
    return {};
}

std::vector<std::string> tagsFromRelativePath(const fs::path& relative)
{
    std::vector<std::string> tags;
    for (const auto& part : relative.parent_path())
    {
        const auto tag = toLower(part.string());
        if (! tag.empty() && tag != "." && std::find(tags.begin(), tags.end(), tag) == tags.end())
            tags.push_back(tag);
    }
    return tags;
}

bool looksLikeDrums(const NoteSequence& sequence)
{
    if (sequence.notes.empty())
        return false;

    size_t onChannel10 = 0;
    for (const auto& note : sequence.notes)
        if (note.channel == 9)
            ++onChannel10;

    return static_cast<double>(onChannel10) / static_cast<double>(sequence.notes.size()) > 0.7;
}

json::Value gridToJson(const RhythmProfile& rhythm)
{
    auto array = json::Value::array();
    for (float value : rhythm.grid)
        array.push(json::Value(static_cast<double>(std::round(value * 1000.0f) / 1000.0f)));
    return array;
}

} // namespace

std::string LibraryEntry::summary() const
{
    std::string out = relativePath;
    out += "  [" + std::string(toString(role));
    if (! folderRole.empty())
        out += "/" + folderRole;
    out += "]  " + key.name();
    out += "  " + std::to_string(static_cast<int>(bpm + 0.5)) + " BPM";
    out += "  " + std::to_string(bars) + " bars";
    if (! progression.empty())
        out += "  " + progression;
    return out;
}

const LibraryEntry* LibraryIndex::find(const std::string& relative) const
{
    const auto it = std::find_if(entries.begin(), entries.end(), [&relative](const LibraryEntry& entry)
    {
        return entry.relativePath == relative || entry.path == relative;
    });
    return it != entries.end() ? &*it : nullptr;
}

LibraryIndex scanDirectory(const std::string& root,
                           const ScanOptions& options,
                           const std::function<void(const std::string&, size_t, size_t)>& progress,
                           std::vector<std::string>* errors)
{
    LibraryIndex index;

    std::error_code ec;
    const fs::path rootPath = fs::absolute(root, ec);
    index.root = rootPath.string();

    if (! fs::is_directory(rootPath, ec))
    {
        if (errors != nullptr)
            errors->push_back("'" + root + "' is not a folder.");
        return index;
    }

    // Collect first so the progress callback can report a total.
    std::vector<fs::path> files;
    const auto collect = [&files](const fs::directory_entry& entry)
    {
        if (! entry.is_regular_file())
            return;
        const auto extension = toLower(entry.path().extension().string());
        if (extension == ".mid" || extension == ".midi")
            files.push_back(entry.path());
    };

    if (options.recursive)
    {
        for (auto it = fs::recursive_directory_iterator(rootPath, fs::directory_options::skip_permission_denied, ec);
             it != fs::recursive_directory_iterator(); it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            collect(*it);
        }
    }
    else
    {
        for (auto it = fs::directory_iterator(rootPath, fs::directory_options::skip_permission_denied, ec);
             it != fs::directory_iterator(); it.increment(ec))
        {
            if (ec)
            {
                ec.clear();
                continue;
            }
            collect(*it);
        }
    }

    std::sort(files.begin(), files.end());
    if (options.maxFiles > 0 && files.size() > options.maxFiles)
        files.resize(options.maxFiles);

    size_t done = 0;
    for (const auto& file : files)
    {
        const auto relative = fs::relative(file, rootPath, ec).string();
        ++done;
        if (progress)
            progress(relative, done, files.size());

        NoteSequence sequence;
        std::string error;
        if (! midi::readFromFile(file.string(), sequence, error))
        {
            if (errors != nullptr)
                errors->push_back(relative + ": " + error);
            continue;
        }

        LibraryEntry entry;
        entry.path = file.string();
        entry.relativePath = relative;
        entry.name = file.stem().string();
        entry.tags = tagsFromRelativePath(fs::path(relative));
        entry.drums = looksLikeDrums(sequence);
        entry.folderRole = roleFromPath(toLower(relative));
        if (entry.folderRole == "drums")
            entry.drums = true;

        if (options.skipDrums && entry.drums)
            continue;

        const auto analysis = analyze(sequence);
        entry.role = analysis.role;
        entry.key = analysis.key;
        entry.keyConfidence = analysis.keyConfidence;
        entry.bpm = analysis.bpm;
        entry.bars = analysis.bars;
        entry.noteCount = static_cast<int>(sequence.notes.size());
        entry.rhythm = analysis.rhythm;
        if (! entry.drums && analysis.valid)
        {
            entry.progression = analysis.progressionString();
            entry.roman = analysis.romanNumeralString();
        }

        index.entries.push_back(std::move(entry));
    }

    return index;
}

bool saveIndex(const std::string& path, const LibraryIndex& index, std::string& error)
{
    auto root = json::Value::object();
    root.set("version", json::Value(1));
    root.set("root", json::Value(index.root));

    auto entries = json::Value::array();
    for (const auto& entry : index.entries)
    {
        auto item = json::Value::object();
        item.set("path", json::Value(entry.path));
        item.set("relative", json::Value(entry.relativePath));
        item.set("name", json::Value(entry.name));

        auto tags = json::Value::array();
        for (const auto& tag : entry.tags)
            tags.push(json::Value(tag));
        item.set("tags", tags);

        item.set("folderRole", json::Value(entry.folderRole));
        item.set("role", json::Value(std::string(toString(entry.role))));
        item.set("drums", json::Value(entry.drums));
        item.set("tonic", json::Value(entry.key.tonic));
        item.set("scale", json::Value(std::string(toString(entry.key.scale))));
        item.set("keyConfidence", json::Value(static_cast<double>(entry.keyConfidence)));
        item.set("bpm", json::Value(entry.bpm));
        item.set("bars", json::Value(entry.bars));
        item.set("notes", json::Value(entry.noteCount));
        item.set("progression", json::Value(entry.progression));
        item.set("roman", json::Value(entry.roman));

        auto rhythm = json::Value::object();
        rhythm.set("notesPerBar", json::Value(static_cast<double>(entry.rhythm.notesPerBar)));
        rhythm.set("averageLengthBeats", json::Value(static_cast<double>(entry.rhythm.averageLengthBeats)));
        rhythm.set("averageVelocity", json::Value(static_cast<double>(entry.rhythm.averageVelocity)));
        rhythm.set("syncopation", json::Value(static_cast<double>(entry.rhythm.syncopation)));
        rhythm.set("polyphony", json::Value(static_cast<double>(entry.rhythm.polyphony)));
        rhythm.set("lowestPitch", json::Value(entry.rhythm.lowestPitch));
        rhythm.set("highestPitch", json::Value(entry.rhythm.highestPitch));
        rhythm.set("monophonic", json::Value(entry.rhythm.monophonic));
        rhythm.set("grid", gridToJson(entry.rhythm));
        item.set("rhythm", rhythm);

        entries.push(std::move(item));
    }
    root.set("entries", std::move(entries));

    std::ofstream stream(path);
    if (! stream)
    {
        error = "Could not write '" + path + "'.";
        return false;
    }
    stream << root.toString(1);
    if (! stream.good())
    {
        error = "Failed while writing '" + path + "'.";
        return false;
    }
    error.clear();
    return true;
}

bool loadIndex(const std::string& path, LibraryIndex& index, std::string& error)
{
    std::ifstream stream(path);
    if (! stream)
    {
        error = "Could not open '" + path + "'.";
        return false;
    }

    const std::string text((std::istreambuf_iterator<char>(stream)), std::istreambuf_iterator<char>());

    json::Value root;
    if (! json::parse(text, root, error))
        return false;

    if (! root.isObject() || ! root["entries"].isArray())
    {
        error = "'" + path + "' is not a Harmonia library index.";
        return false;
    }

    index = LibraryIndex {};
    index.root = root["root"].asString("");

    for (const auto& item : root["entries"].items())
    {
        LibraryEntry entry;
        entry.path = item["path"].asString("");
        entry.relativePath = item["relative"].asString("");
        entry.name = item["name"].asString("");
        for (const auto& tag : item["tags"].items())
            entry.tags.push_back(tag.asString(""));
        entry.folderRole = item["folderRole"].asString("");

        SourceRole role = SourceRole::Unknown;
        sourceRoleFromString(item["role"].asString("Unknown"), role);
        entry.role = role;

        entry.drums = item["drums"].asBool(false);
        entry.key.tonic = item["tonic"].asInt(0);
        ScaleType scale = ScaleType::Major;
        scaleTypeFromString(item["scale"].asString("Major"), scale);
        entry.key.scale = scale;
        entry.keyConfidence = static_cast<float>(item["keyConfidence"].asNumber(0.0));
        entry.bpm = item["bpm"].asNumber(120.0);
        entry.bars = item["bars"].asInt(0);
        entry.noteCount = item["notes"].asInt(0);
        entry.progression = item["progression"].asString("");
        entry.roman = item["roman"].asString("");

        const auto& rhythm = item["rhythm"];
        entry.rhythm.notesPerBar = static_cast<float>(rhythm["notesPerBar"].asNumber(0.0));
        entry.rhythm.averageLengthBeats = static_cast<float>(rhythm["averageLengthBeats"].asNumber(0.5));
        entry.rhythm.averageVelocity = static_cast<float>(rhythm["averageVelocity"].asNumber(90.0));
        entry.rhythm.syncopation = static_cast<float>(rhythm["syncopation"].asNumber(0.0));
        entry.rhythm.polyphony = static_cast<float>(rhythm["polyphony"].asNumber(1.0));
        entry.rhythm.lowestPitch = rhythm["lowestPitch"].asInt(60);
        entry.rhythm.highestPitch = rhythm["highestPitch"].asInt(72);
        entry.rhythm.monophonic = rhythm["monophonic"].asBool(true);

        const auto& grid = rhythm["grid"];
        for (size_t i = 0; i < grid.items().size() && i < entry.rhythm.grid.size(); ++i)
            entry.rhythm.grid[i] = static_cast<float>(grid.items()[i].asNumber(0.0));

        index.entries.push_back(std::move(entry));
    }

    error.clear();
    return true;
}

std::vector<const LibraryEntry*> queryLibrary(const LibraryIndex& index, const LibraryQuery& query)
{
    std::vector<const LibraryEntry*> results;
    const auto wantedTag = toLower(query.tag);
    const auto wantedRole = toLower(query.role);
    const auto wantedText = toLower(query.containing);

    for (const auto& entry : index.entries)
    {
        if (query.excludeDrums && entry.drums)
            continue;

        if (! wantedTag.empty())
        {
            const bool inTags = std::any_of(entry.tags.begin(), entry.tags.end(),
                                            [&wantedTag](const std::string& tag) { return contains(tag, wantedTag); });
            if (! inTags && ! contains(toLower(entry.relativePath), wantedTag))
                continue;
        }

        if (! wantedRole.empty())
        {
            const auto detected = toLower(toString(entry.role));
            if (! contains(detected, wantedRole) && entry.folderRole != wantedRole)
                continue;
        }

        if (query.tonic >= 0 && entry.key.tonic != query.tonic)
            continue;
        if (query.matchMode && entry.key.scale != query.mode)
            continue;
        if (query.minBpm > 0.0 && entry.bpm < query.minBpm)
            continue;
        if (query.maxBpm > 0.0 && entry.bpm > query.maxBpm)
            continue;
        if (query.minBars > 0 && entry.bars < query.minBars)
            continue;
        if (! wantedText.empty() && ! contains(toLower(entry.progression), wantedText)
            && ! contains(toLower(entry.roman), wantedText))
            continue;

        results.push_back(&entry);
        if (query.limit > 0 && results.size() >= query.limit)
            break;
    }
    return results;
}

std::vector<std::pair<std::string, int>> tagHistogram(const LibraryIndex& index, size_t limit)
{
    std::map<std::string, int> counts;
    for (const auto& entry : index.entries)
        for (const auto& tag : entry.tags)
            ++counts[tag];

    std::vector<std::pair<std::string, int>> out(counts.begin(), counts.end());
    std::sort(out.begin(), out.end(), [](const auto& a, const auto& b)
    {
        if (a.second != b.second)
            return a.second > b.second;
        return a.first < b.first;
    });
    if (limit > 0 && out.size() > limit)
        out.resize(limit);
    return out;
}

} // namespace harmonia
