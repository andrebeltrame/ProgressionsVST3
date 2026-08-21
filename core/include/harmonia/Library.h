#pragma once

#include "harmonia/Analysis.h"
#include "harmonia/StyleModel.h"
#include "harmonia/Theory.h"

#include <functional>
#include <map>
#include <string>
#include <vector>

namespace harmonia
{

/** One analysed MIDI file from your collection. */
struct LibraryEntry
{
    std::string path;         // absolute
    std::string relativePath; // relative to the scanned root
    std::vector<std::string> tags; // folder names, lower-cased
    std::string folderRole;   // bass / lead / pad / pluck / arp / drums, from the path
    std::string name;

    SourceRole role = SourceRole::Unknown;
    bool drums = false;       // percussion, so no use for harmony

    Key key;
    float keyConfidence = 0.0f;
    double bpm = 120.0;
    int bars = 0;
    int noteCount = 0;

    std::string progression;
    std::string roman;
    RhythmProfile rhythm;

    /** How the entry reads in one line, for listings. */
    std::string summary() const;
};

/** What the walk actually saw. Without this a scan that quietly skipped most of
    a drive looks exactly like a scan of a small drive. */
struct ScanStats
{
    size_t directoriesVisited = 0;
    size_t filesSeen = 0;        // every regular file, whatever the extension
    size_t midiFilesFound = 0;   // .mid / .midi
    size_t indexed = 0;
    size_t unreadable = 0;
    size_t skippedDrums = 0;
    size_t droppedByLimit = 0;   // left out because of ScanOptions::maxFiles
    size_t walkErrors = 0;       // folders the walk could not enter
    /** AppleDouble twins and hidden system files, skipped before they can be
        mistaken for clips. A drive formatted exFAT is full of them. */
    size_t systemFilesSkipped = 0;
    /** Extensions that were not MIDI, most common first, for spotting a
        collection that is all .rmi or still inside .zip files. */
    std::map<std::string, int> otherExtensions;
    /** MIDI files per top-level folder, so a huge drive can be split into the
        parts worth learning from. */
    std::map<std::string, int> midiByTopFolder;
};

struct LibraryIndex
{
    std::string root;
    std::vector<LibraryEntry> entries;
    ScanStats stats;

    const LibraryEntry* find(const std::string& relativePath) const;
};

struct ScanOptions
{
    bool recursive = true;
    size_t maxFiles = 0;         // 0 = no limit
    bool skipDrums = false;      // leave percussion out of the index entirely
    /** Follow folder symlinks and aliases. Off by default because a loop in the
        tree would make the walk run forever. */
    bool followSymlinks = false;
    /** Count what is there without opening anything. */
    bool dryRun = false;
};

/** Walks a folder tree, analyses every .mid it finds and returns the index.
    `progress` is called with (relative path, done, total) as it goes.
    Pass a `styleModel` to also learn from every clip in the same pass - reading
    a large drive twice is the slow part. */
LibraryIndex scanDirectory(const std::string& root,
                           const ScanOptions& options,
                           const std::function<void(const std::string&, size_t, size_t)>& progress = {},
                           std::vector<std::string>* errors = nullptr,
                           StyleModel* styleModel = nullptr);

bool saveIndex(const std::string& path, const LibraryIndex& index, std::string& error);
bool loadIndex(const std::string& path, LibraryIndex& index, std::string& error);

struct LibraryQuery
{
    std::string tag;          // substring match against tags and the relative path
    std::string role;         // bass / lead / chords / arp / pluck / drums
    std::string containing;   // substring of the detected progression
    int tonic = -1;           // -1 = any
    bool matchMode = false;
    ScaleType mode = ScaleType::NaturalMinor;
    double minBpm = 0.0;
    double maxBpm = 0.0;      // 0 = no upper bound
    int minBars = 0;
    bool excludeDrums = true;
    size_t limit = 0;         // 0 = everything
};

std::vector<const LibraryEntry*> queryLibrary(const LibraryIndex& index, const LibraryQuery& query);

/** What a clip is for, worked out from its path: the file name first (including
    the "BS "/"LD " prefixes packs use), then the folders it sits in, deepest
    first. Pack and genre titles are ignored - "Melodic House" is not a lead.
    Returns an empty string when the path says nothing. */
std::string roleForPath(const std::string& relativePath);

/** Counts entries per tag, most common first - a quick map of a collection. */
std::vector<std::pair<std::string, int>> tagHistogram(const LibraryIndex& index, size_t limit = 0);

} // namespace harmonia
