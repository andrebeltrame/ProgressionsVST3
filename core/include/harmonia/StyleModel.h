#pragma once

#include "harmonia/Analysis.h"
#include "harmonia/Rng.h"
#include "harmonia/Theory.h"
#include "harmonia/Types.h"

#include <array>
#include <cstdint>
#include <functional>
#include <map>
#include <string>
#include <vector>

namespace harmonia
{

struct LibraryIndex;

/** Which bank of learned material a clip contributes to. */
enum class StyleRole
{
    Melody,
    Bass,
    Pluck,
    Chords,
    Arp
};

const char* toString(StyleRole role);
bool styleRoleFromString(const std::string& text, StyleRole& out);

/** One bar of rhythm lifted from a real clip, on a 16th grid. */
struct RhythmPattern
{
    uint16_t onsets = 0;                     // bit n = a note starts on 16th n
    std::array<uint8_t, 16> velocity { };    // 0 where nothing starts
    std::array<uint8_t, 16> lengthSlots { }; // note length in 16ths, 0 where nothing starts
    int count = 1;                           // how often this bar appeared in the corpus

    int onsetCount() const;
    bool hasOnset(int slot) const { return (onsets & (1u << slot)) != 0; }
};

/** Everything the engine has learned from a collection of MIDI files. Purely
    statistical: counts of movements, bars and voicings, never copies of whole
    phrases. */
struct StyleModel
{
    /** Melodic movement in scale steps, so it transposes to any key.
        prevStep -> nextStep -> count. */
    std::map<int, std::map<int, int>> stepTransitions;
    std::map<int, int> stepMarginal;
    /** Interval above the chord root for notes landing on a beat. */
    std::map<int, int> strongBeatInterval;
    /** Interval above the chord root for a bass note, by how strong its 16th is
        (0 = bar downbeat, 1 = other beats, 2 = 8th offbeats, 3 = 16th offbeats). */
    std::array<std::map<int, int>, 4> bassIntervalBySlotClass;

    /** One bank of bars per role, keyed by the onset mask. */
    std::map<StyleRole, std::map<uint16_t, RhythmPattern>> patterns;

    /** Chord voicings as semitone intervals above the root, lowest first. */
    std::map<std::vector<int>, int> voicings;
    /** Roman numeral loops seen in the corpus, and how often each turned up. */
    std::map<std::string, int> progressions;

    int clipsLearned = 0;
    int barsLearned = 0;

    bool empty() const { return clipsLearned == 0; }
    bool hasPatternsFor(StyleRole role) const;
    /** Human-readable one-liner for the UI. */
    std::string summary() const;
    /** The most common progressions in the corpus, most frequent first. */
    std::vector<std::pair<std::string, int>> topProgressions(size_t limit = 20) const;

    /** Keeps the most common material and drops the long tail, so the model
        stays small no matter how big the drive was. */
    void prune(size_t maxPatternsPerRole = 256, size_t maxVoicings = 128, size_t maxProgressions = 200);
};

/** Feeds one analysed clip into the model. `roleHint` comes from the folder name
    ("bass", "pluck", ...) and wins over the detected role when it is set. */
void learnFromClip(StyleModel& model,
                   const NoteSequence& sequence,
                   const Analysis& analysis,
                   const std::string& roleHint = {});

/** Re-reads every file in an index and learns from all of them. */
StyleModel buildStyleModel(const LibraryIndex& index,
                           const std::function<void(size_t, size_t)>& progress = {},
                           std::vector<std::string>* errors = nullptr);

bool saveStyleModel(const std::string& path, const StyleModel& model, std::string& error);
bool loadStyleModel(const std::string& path, StyleModel& model, std::string& error);

// --- Sampling, used by the generators ---------------------------------------

/** Picks a bar of rhythm, preferring patterns that are common in the corpus and
    close to the requested number of onsets. Returns nullptr if that role has
    nothing learned. */
const RhythmPattern* pickPattern(const StyleModel& model, StyleRole role, float targetOnsets, Rng& rng);

/** Next melodic step given the previous one, falling back to the marginal
    distribution for unseen contexts. Returns `fallback` if nothing was learned. */
int sampleStep(const StyleModel& model, int previousStep, Rng& rng, int fallback = 1);

/** Interval above the chord root for a bass note on a 16th of that strength. */
int sampleBassInterval(const StyleModel& model, int slotClass, Rng& rng, int fallback = 0);

/** A learned voicing as intervals above the root, or an empty vector. */
std::vector<int> sampleVoicing(const StyleModel& model, size_t voiceCount, Rng& rng);

/** 0 = bar downbeat, 1 = other beats, 2 = 8th offbeats, 3 = 16th offbeats. */
int slotClassFor(int slot);

} // namespace harmonia
