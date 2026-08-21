#pragma once

#include "harmonia/Analysis.h"
#include "harmonia/Theory.h"

#include <functional>
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

struct LibraryIndex
{
    std::string root;
    std::vector<LibraryEntry> entries;

    const LibraryEntry* find(const std::string& relativePath) const;
};

struct ScanOptions
{
    bool recursive = true;
    size_t maxFiles = 0;        // 0 = no limit
    bool skipDrums = false;     // leave percussion out of the index entirely
};

/** Walks a folder tree, analyses every .mid it finds and returns the index.
    `progress` is called with (relative path, done, total) as it goes. */
LibraryIndex scanDirectory(const std::string& root,
                           const ScanOptions& options,
                           const std::function<void(const std::string&, size_t, size_t)>& progress = {},
                           std::vector<std::string>* errors = nullptr);

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

/** Counts entries per tag, most common first - a quick map of a collection. */
std::vector<std::pair<std::string, int>> tagHistogram(const LibraryIndex& index, size_t limit = 0);

} // namespace harmonia
