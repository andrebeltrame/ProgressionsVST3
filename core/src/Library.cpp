#include "harmonia/Library.h"

#include "harmonia/Json.h"
#include "harmonia/MidiFile.h"

#include <algorithm>
#include <cctype>
#include <iterator>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <map>
#include <atomic>
#include <mutex>
#include <set>
#include <thread>

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

/** macOS scatters these across any drive it touches, and on exFAT every copied
    file gets a "._" twin that looks like a MIDI file but is a resource fork. */
bool isSystemName(const std::string& name)
{
    return name.rfind("._", 0) == 0
        || name.rfind('.', 0) == 0
        || name == "__MACOSX"
        || name == "System Volume Information"
        || name == "$RECYCLE.BIN";
}

/** Genre and pack names are not roles. "Melodic House & Techno" must not make
    every clip in the pack a lead, and "Deep House" is not a bassline. */
bool looksLikeAPackName(const std::string& lowerComponent)
{
    static const char* markers[] = {
        "house", "techno", "trance", "garage", "disco", "progressive", "tech ",
        "vol", "pack", "midi", "sample", "loop", "collection", "edition", "bundle",
        ".com", "presets"
    };

    for (const char* marker : markers)
        if (contains(lowerComponent, marker))
            return true;

    // Role folders are called "Bass" or "Leads"; packs have long titles.
    return lowerComponent.size() > 20;
}

/** Role words as they appear in folder names. */
const std::pair<const char*, const char*>* roleKeywords(size_t& count)
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
    count = std::size(table);
    return table;
}

/** The short prefixes MIDI packs put on filenames: "BS Sheliak Em.mid". */
std::string roleFromFilenamePrefix(const std::string& lowerStem)
{
    static const std::pair<const char*, const char*> table[] = {
        { "bs", "bass" },   { "bl", "bass" },   { "sub", "bass" },
        { "ld", "lead" },   { "mel", "lead" },  { "top", "lead" },
        { "ch", "chords" }, { "chd", "chords" }, { "crd", "chords" }, { "kb", "chords" },
        { "pd", "pad" },    { "pad", "pad" },
        { "pl", "pluck" },  { "plk", "pluck" },
        { "arp", "arp" },   { "seq", "arp" },
        { "dr", "drums" },  { "pc", "drums" },
    };

    const auto end = lowerStem.find_first_of(" _-.");
    if (end == std::string::npos || end == 0)
        return {};

    const auto prefix = lowerStem.substr(0, end);
    for (const auto& [token, role] : table)
        if (prefix == token)
            return role;
    return {};
}

std::string roleFromPathImpl(const fs::path& relative)
{
    const auto stem = toLower(relative.stem().string());

    size_t keywordCount = 0;
    const auto* keywords = roleKeywords(keywordCount);

    // The file name is the most specific clue there is.
    if (auto role = roleFromFilenamePrefix(stem); ! role.empty())
        return role;
    for (size_t i = 0; i < keywordCount; ++i)
        if (contains(stem, keywords[i].first))
            return keywords[i].second;

    // Then the folders, deepest first, ignoring anything that reads as a pack
    // title rather than a role.
    std::vector<std::string> components;
    for (const auto& part : relative.parent_path())
        components.push_back(toLower(part.string()));

    for (auto it = components.rbegin(); it != components.rend(); ++it)
    {
        if (looksLikeAPackName(*it))
            continue;
        for (size_t i = 0; i < keywordCount; ++i)
            if (contains(*it, keywords[i].first))
                return keywords[i].second;
    }
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

std::string roleForPath(const std::string& relativePath)
{
    return roleFromPathImpl(fs::path(relativePath));
}

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
                           std::vector<std::string>* errors,
                           StyleModel* styleModel)
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

    // Collect first so the progress callback can report a total, and count
    // everything we walk past - a scan that silently skipped nine tenths of a
    // drive should not look like a scan of a small drive.
    std::vector<fs::path> files;
    auto& stats = index.stats;

    // The top-level folder each file belongs to, carried down the walk so a
    // per-file relative() call is not needed.
    std::string currentTopFolder;

    const auto collect = [&files, &stats, &currentTopFolder](const fs::directory_entry& entry)
    {
        std::error_code entryEc;
        if (! entry.is_regular_file(entryEc) || entryEc)
            return;

        if (isSystemName(entry.path().filename().string()))
        {
            ++stats.systemFilesSkipped;
            return;
        }

        ++stats.filesSeen;
        const auto extension = toLower(entry.path().extension().string());
        if (extension == ".mid" || extension == ".midi")
        {
            ++stats.midiFilesFound;
            ++stats.midiByTopFolder[currentTopFolder.empty() ? "(root)" : currentTopFolder];
            files.push_back(entry.path());
        }
        else
        {
            ++stats.otherExtensions[extension.empty() ? "(no extension)" : extension];
        }
    };

    // Walk with an explicit stack of folders rather than
    // recursive_directory_iterator: that iterator becomes end() as soon as any
    // increment fails, so one folder macOS refuses (.Spotlight-V100 returns
    // EPERM, which skip_permission_denied does not cover) silently ends the
    // scan of the whole drive. Here a folder we cannot read costs that folder
    // and nothing else.
    // Each folder remembers which top-level folder it came from.
    std::vector<std::pair<fs::path, std::string>> pending { { rootPath, std::string {} } };

    // Only needed when chasing symlinks, where the same folder can be reached
    // by more than one path - including a link that points back up the tree.
    std::set<std::string> visited;
    if (options.followSymlinks)
    {
        std::error_code rootEc;
        const auto resolvedRoot = fs::canonical(rootPath, rootEc);
        if (! rootEc)
            visited.insert(resolvedRoot.string());
    }

    while (! pending.empty())
    {
        const auto [directory, topFolder] = pending.back();
        pending.pop_back();
        currentTopFolder = topFolder;

        std::error_code dirEc;
        fs::directory_iterator it(directory, fs::directory_options::skip_permission_denied, dirEc);
        if (dirEc)
        {
            ++stats.walkErrors;
            continue;
        }

        for (; it != fs::directory_iterator(); it.increment(dirEc))
        {
            if (dirEc)
            {
                // Give up on this folder, keep the rest of the drive.
                ++stats.walkErrors;
                break;
            }

            const auto& entry = *it;
            const auto name = entry.path().filename().string();

            std::error_code kindEc;
            if (entry.is_directory(kindEc) && ! kindEc)
            {
                ++stats.directoriesVisited;
                if (! options.recursive || isSystemName(name))
                    continue;

                std::error_code linkEc;
                if (options.followSymlinks)
                {
                    // Dedupe by real path, so a link pointing back up the tree
                    // is walked once instead of forever.
                    const auto resolved = fs::canonical(entry.path(), linkEc);
                    if (linkEc || ! visited.insert(resolved.string()).second)
                        continue;
                }
                else if (entry.is_symlink(linkEc) && ! linkEc)
                {
                    continue;
                }

                pending.emplace_back(entry.path(), topFolder.empty() ? name : topFolder);
                continue;
            }

            collect(entry);
        }
    }

    std::sort(files.begin(), files.end());
    if (options.maxFiles > 0 && files.size() > options.maxFiles)
    {
        stats.droppedByLimit = files.size() - options.maxFiles;
        files.resize(options.maxFiles);
    }

    if (options.dryRun)
        return index;

    // Reading and analysing files is the slow part of a big drive and every
    // file is independent, so split the list into one fixed block per thread.
    // Fixed blocks rather than a shared queue keeps the output identical from
    // run to run.
    const unsigned threadCount = std::max(1u, options.threads > 0
                                                  ? options.threads
                                                  : std::thread::hardware_concurrency());

    struct Slot
    {
        LibraryEntry entry;
        bool valid = false;
    };

    struct Partial
    {
        StyleModel model;
        std::vector<std::string> errors;
        size_t unreadable = 0;
        size_t skippedDrums = 0;
    };

    std::vector<Slot> slots(files.size());
    std::vector<Partial> partials(threadCount);
    std::atomic<size_t> completed { 0 };
    std::mutex progressMutex;

    const auto processRange = [&](unsigned threadIndex, size_t from, size_t to)
    {
        auto& partial = partials[threadIndex];

        for (size_t i = from; i < to; ++i)
        {
            const auto& file = files[i];
            std::error_code relativeEc;
            const auto relative = fs::relative(file, rootPath, relativeEc).string();

            if (progress)
            {
                const size_t at = completed.fetch_add(1) + 1;
                const std::lock_guard<std::mutex> lock(progressMutex);
                progress(relative, at, files.size());
            }
            else
            {
                completed.fetch_add(1);
            }

            NoteSequence sequence;
            std::string error;
            if (! midi::readFromFile(file.string(), sequence, error))
            {
                ++partial.unreadable;
                partial.errors.push_back(relative + ": " + error);
                continue;
            }

            LibraryEntry entry;
            entry.path = file.string();
            entry.relativePath = relative;
            entry.name = file.stem().string();
            entry.tags = tagsFromRelativePath(fs::path(relative));
            entry.drums = looksLikeDrums(sequence);
            entry.folderRole = roleForPath(relative);
            if (entry.folderRole == "drums")
                entry.drums = true;

            if (options.skipDrums && entry.drums)
            {
                ++partial.skippedDrums;
                continue;
            }

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

            if (styleModel != nullptr && ! entry.drums)
            {
                learnFromClip(partial.model, sequence, analysis, entry.folderRole);
                if (((i - from) % 500) == 499)
                    partial.model.prune(1024, 512, 800);
            }

            slots[i].entry = std::move(entry);
            slots[i].valid = true;
        }
    };

    if (threadCount <= 1 || files.size() < 32)
    {
        processRange(0, 0, files.size());
    }
    else
    {
        std::vector<std::thread> workers;
        workers.reserve(threadCount);
        for (unsigned t = 0; t < threadCount; ++t)
        {
            const size_t from = files.size() * t / threadCount;
            const size_t to = files.size() * (t + 1) / threadCount;
            workers.emplace_back(processRange, t, from, to);
        }
        for (auto& worker : workers)
            worker.join();
    }

    // Join the blocks back together in file order.
    index.entries.reserve(files.size());
    for (auto& slot : slots)
    {
        if (! slot.valid)
            continue;
        index.entries.push_back(std::move(slot.entry));
        ++stats.indexed;
    }

    for (auto& partial : partials)
    {
        stats.unreadable += partial.unreadable;
        stats.skippedDrums += partial.skippedDrums;
        if (errors != nullptr)
            errors->insert(errors->end(), partial.errors.begin(), partial.errors.end());
        if (styleModel != nullptr)
            styleModel->merge(partial.model);
    }
    if (errors != nullptr)
        std::sort(errors->begin(), errors->end());

    if (styleModel != nullptr)
        styleModel->prune();

    return index;
}

bool saveIndex(const std::string& path, const LibraryIndex& index, std::string& error)
{
    // Streamed rather than built as one document: a drive with a few hundred
    // thousand clips would need gigabytes to hold the tree and the string.
    std::ofstream stream(path);
    if (! stream)
    {
        error = "Could not write '" + path + "'.";
        return false;
    }

    const auto quote = [](const std::string& text) { return json::quoted(text); };

    stream << "{\n \"version\": 1,\n \"root\": " << quote(index.root) << ",\n \"entries\": [";

    bool first = true;
    for (const auto& entry : index.entries)
    {
        stream << (first ? "\n  " : ",\n  ") << "{";
        first = false;

        stream << "\"path\": " << quote(entry.path)
               << ", \"relative\": " << quote(entry.relativePath)
               << ", \"name\": " << quote(entry.name)
               << ", \"tags\": [";
        for (size_t i = 0; i < entry.tags.size(); ++i)
            stream << (i > 0 ? ", " : "") << quote(entry.tags[i]);
        stream << "]"
               << ", \"folderRole\": " << quote(entry.folderRole)
               << ", \"role\": " << quote(toString(entry.role))
               << ", \"drums\": " << (entry.drums ? "true" : "false")
               << ", \"tonic\": " << entry.key.tonic
               << ", \"scale\": " << quote(toString(entry.key.scale))
               << ", \"keyConfidence\": " << std::setprecision(3) << entry.keyConfidence
               << ", \"bpm\": " << std::setprecision(5) << entry.bpm
               << ", \"bars\": " << entry.bars
               << ", \"notes\": " << entry.noteCount
               << ", \"progression\": " << quote(entry.progression)
               << ", \"roman\": " << quote(entry.roman);

        const auto& rhythm = entry.rhythm;
        stream << ", \"rhythm\": {"
               << "\"notesPerBar\": " << std::setprecision(4) << rhythm.notesPerBar
               << ", \"averageLengthBeats\": " << rhythm.averageLengthBeats
               << ", \"averageVelocity\": " << rhythm.averageVelocity
               << ", \"syncopation\": " << rhythm.syncopation
               << ", \"polyphony\": " << rhythm.polyphony
               << ", \"lowestPitch\": " << rhythm.lowestPitch
               << ", \"highestPitch\": " << rhythm.highestPitch
               << ", \"monophonic\": " << (rhythm.monophonic ? "true" : "false")
               << ", \"grid\": [";
        for (size_t i = 0; i < rhythm.grid.size(); ++i)
            stream << (i > 0 ? ", " : "") << std::setprecision(3)
                   << std::round(rhythm.grid[i] * 1000.0f) / 1000.0f;
        stream << "]}}";

        if (! stream)
        {
            error = "Failed while writing '" + path + "'.";
            return false;
        }
    }

    stream << (index.entries.empty() ? "" : "\n ") << "]\n}\n";
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
