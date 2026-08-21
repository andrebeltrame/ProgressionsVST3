#include "harmonia/StyleModel.h"

#include "harmonia/Json.h"
#include "harmonia/Library.h"
#include "harmonia/MidiFile.h"

#include <algorithm>
#include <cctype>
#include <cmath>
#include <fstream>
#include <limits>

namespace harmonia
{

namespace
{

constexpr int kNoDegree = std::numeric_limits<int>::min();

/** Scale degree counted across octaves, so a step of 7 is one octave. */
int absoluteDegree(const Key& key, int pitch)
{
    const int degree = key.degreeOf(mod12(pitch));
    if (degree < 0)
        return kNoDegree;

    const auto& steps = scaleIntervals(key.scale);
    if (steps.empty())
        return kNoDegree;

    const int semitonesAboveTonic = pitch - key.tonic;
    const int octave = static_cast<int>(std::floor(static_cast<double>(semitonesAboveTonic) / 12.0));
    return octave * static_cast<int>(steps.size()) + degree;
}

StyleRole roleFor(const Analysis& analysis, const std::string& roleHint)
{
    StyleRole role = StyleRole::Melody;
    if (styleRoleFromString(roleHint, role))
        return role;

    switch (analysis.role)
    {
        case SourceRole::Bass:   return StyleRole::Bass;
        case SourceRole::Chords: return StyleRole::Chords;
        case SourceRole::Arp:    return StyleRole::Arp;
        case SourceRole::Lead:
        case SourceRole::Unknown:
        default:                 return StyleRole::Melody;
    }
}

void addPattern(std::map<uint16_t, RhythmPattern>& bank, const RhythmPattern& pattern)
{
    auto it = bank.find(pattern.onsets);
    if (it == bank.end())
    {
        bank.emplace(pattern.onsets, pattern);
        return;
    }

    // Average the velocities in, so a bank entry reflects every bar that used it.
    auto& existing = it->second;
    const float weight = 1.0f / static_cast<float>(existing.count + 1);
    for (size_t slot = 0; slot < existing.velocity.size(); ++slot)
    {
        existing.velocity[slot] = static_cast<uint8_t>(
            existing.velocity[slot] + (pattern.velocity[slot] - existing.velocity[slot]) * weight);
        existing.lengthSlots[slot] = static_cast<uint8_t>(
            existing.lengthSlots[slot] + (pattern.lengthSlots[slot] - existing.lengthSlots[slot]) * weight);
    }
    ++existing.count;
}

template <typename Key_, typename Compare>
void trimToTop(std::map<Key_, int>& counts, size_t limit, Compare)
{
    if (counts.size() <= limit)
        return;

    std::vector<std::pair<Key_, int>> flat(counts.begin(), counts.end());
    std::nth_element(flat.begin(), flat.begin() + static_cast<long>(limit), flat.end(),
                     [](const auto& a, const auto& b) { return a.second > b.second; });
    flat.resize(limit);

    counts.clear();
    for (auto& entry : flat)
        counts.emplace(std::move(entry.first), entry.second);
}

int sampleFromCounts(const std::map<int, int>& counts, Rng& rng, int fallback)
{
    if (counts.empty())
        return fallback;

    int total = 0;
    for (const auto& [value, count] : counts)
        total += count;
    if (total <= 0)
        return fallback;

    int pick = rng.range(0, total - 1);
    for (const auto& [value, count] : counts)
    {
        pick -= count;
        if (pick < 0)
            return value;
    }
    return counts.begin()->first;
}

} // namespace

// ---------------------------------------------------------------------------

const char* toString(StyleRole role)
{
    switch (role)
    {
        case StyleRole::Melody: return "melody";
        case StyleRole::Bass:   return "bass";
        case StyleRole::Pluck:  return "pluck";
        case StyleRole::Chords: return "chords";
        case StyleRole::Arp:    return "arp";
    }
    return "melody";
}

bool styleRoleFromString(const std::string& text, StyleRole& out)
{
    std::string lowered;
    for (char c : text)
        lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lowered == "bass")                       { out = StyleRole::Bass; return true; }
    if (lowered == "pluck")                      { out = StyleRole::Pluck; return true; }
    if (lowered == "arp")                        { out = StyleRole::Arp; return true; }
    if (lowered == "chords" || lowered == "pad") { out = StyleRole::Chords; return true; }
    if (lowered == "lead" || lowered == "melody"){ out = StyleRole::Melody; return true; }
    return false;
}

int RhythmPattern::onsetCount() const
{
    int count = 0;
    for (int slot = 0; slot < 16; ++slot)
        if (hasOnset(slot))
            ++count;
    return count;
}

int slotClassFor(int slot)
{
    const int position = ((slot % 16) + 16) % 16;
    if (position == 0)
        return 0;
    if (position % 4 == 0)
        return 1;
    if (position % 2 == 0)
        return 2;
    return 3;
}

bool StyleModel::hasPatternsFor(StyleRole role) const
{
    const auto it = patterns.find(role);
    return it != patterns.end() && ! it->second.empty();
}

std::string StyleModel::summary() const
{
    if (empty())
        return "nothing learned yet";

    std::string out = std::to_string(clipsLearned) + " clips, " + std::to_string(barsLearned) + " bars";
    for (const auto& [role, bank] : patterns)
        if (! bank.empty())
            out += ", " + std::to_string(bank.size()) + " " + toString(role) + " bars";
    if (! voicings.empty())
        out += ", " + std::to_string(voicings.size()) + " voicings";
    if (! progressions.empty())
        out += ", " + std::to_string(progressions.size()) + " progressions";
    return out;
}

std::vector<std::pair<std::string, int>> StyleModel::topProgressions(size_t limit) const
{
    std::vector<std::pair<std::string, int>> out(progressions.begin(), progressions.end());
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

void StyleModel::prune(size_t maxPatternsPerRole, size_t maxVoicings, size_t maxProgressions)
{
    for (auto& [role, bank] : patterns)
    {
        if (bank.size() <= maxPatternsPerRole)
            continue;

        std::vector<const RhythmPattern*> flat;
        flat.reserve(bank.size());
        for (const auto& [mask, pattern] : bank)
            flat.push_back(&pattern);
        std::nth_element(flat.begin(), flat.begin() + static_cast<long>(maxPatternsPerRole), flat.end(),
                         [](const RhythmPattern* a, const RhythmPattern* b) { return a->count > b->count; });

        std::map<uint16_t, RhythmPattern> kept;
        for (size_t i = 0; i < maxPatternsPerRole; ++i)
            kept.emplace(flat[i]->onsets, *flat[i]);
        bank = std::move(kept);
    }

    trimToTop(voicings, maxVoicings, std::less<>());
    trimToTop(progressions, maxProgressions, std::less<>());
}

// ---------------------------------------------------------------------------

void learnFromClip(StyleModel& model,
                   const NoteSequence& sequence,
                   const Analysis& analysis,
                   const std::string& roleHint)
{
    if (! analysis.valid || sequence.notes.empty() || analysis.progression.empty())
        return;

    const StyleRole role = roleFor(analysis, roleHint);
    const int64_t barTicks = std::max<int64_t>(1, analysis.ticksPerBar());
    const int64_t slotTicks = std::max<int64_t>(1, barTicks / 16);
    const int bars = std::max(1, analysis.bars);

    ++model.clipsLearned;

    // ---- One rhythm pattern per bar -----------------------------------------
    // Bucket the notes once instead of rescanning the whole clip for every bar;
    // a long arrangement would otherwise cost bars x notes.
    std::vector<std::vector<const Note*>> notesByBar(static_cast<size_t>(bars));
    for (const auto& note : sequence.notes)
    {
        const auto bar = note.startTick / barTicks;
        if (bar >= 0 && bar < bars)
            notesByBar[static_cast<size_t>(bar)].push_back(&note);
    }

    for (int bar = 0; bar < bars; ++bar)
    {
        const int64_t barStart = static_cast<int64_t>(bar) * barTicks;
        RhythmPattern pattern;

        for (const auto* notePtr : notesByBar[static_cast<size_t>(bar)])
        {
            const auto& note = *notePtr;
            const int slot = static_cast<int>(std::clamp<int64_t>((note.startTick - barStart + slotTicks / 2) / slotTicks,
                                                                  0, 15));
            const auto velocity = static_cast<uint8_t>(std::clamp(note.velocity, 1, 127));
            if (velocity <= pattern.velocity[static_cast<size_t>(slot)])
                continue; // keep the loudest note of a chord as the representative

            pattern.onsets = static_cast<uint16_t>(pattern.onsets | (1u << slot));
            pattern.velocity[static_cast<size_t>(slot)] = velocity;
            pattern.lengthSlots[static_cast<size_t>(slot)] =
                static_cast<uint8_t>(std::clamp<int64_t>((note.lengthTick + slotTicks / 2) / slotTicks, 1, 16));
        }

        if (pattern.onsets != 0)
        {
            addPattern(model.patterns[role], pattern);
            ++model.barsLearned;
        }
    }

    // ---- Melodic movement, in scale steps -------------------------------------
    if (role != StyleRole::Chords)
    {
        auto notes = sequence.notes;
        std::sort(notes.begin(), notes.end(), [](const Note& a, const Note& b)
        {
            if (a.startTick != b.startTick)
                return a.startTick < b.startTick;
            return a.pitch > b.pitch; // a chord contributes its top note
        });

        int previousDegree = kNoDegree;
        int previousStep = std::numeric_limits<int>::min();
        int64_t previousTick = -1;

        for (const auto& note : notes)
        {
            if (note.startTick == previousTick)
                continue; // already took the top note of this chord
            previousTick = note.startTick;

            const int degree = absoluteDegree(analysis.key, note.pitch);
            if (degree == kNoDegree)
            {
                previousDegree = kNoDegree; // a chromatic note breaks the chain
                continue;
            }

            if (previousDegree != kNoDegree)
            {
                const int step = std::clamp(degree - previousDegree, -9, 9);
                ++model.stepMarginal[step];
                if (previousStep != std::numeric_limits<int>::min())
                    ++model.stepTransitions[previousStep][step];
                previousStep = step;
            }
            previousDegree = degree;
        }
    }

    // ---- Intervals above the chord root ---------------------------------------
    for (const auto& note : sequence.notes)
    {
        const auto* segment = analysis.chordAt(note.startTick);
        if (segment == nullptr)
            continue;

        const int interval = mod12(note.pitch - segment->chord.root);
        const int slot = static_cast<int>(((note.startTick % barTicks) + slotTicks / 2) / slotTicks) % 16;

        if (slot % 4 == 0)
            ++model.strongBeatInterval[interval];

        if (role == StyleRole::Bass)
            ++model.bassIntervalBySlotClass[static_cast<size_t>(slotClassFor(slot))][interval];
    }

    // ---- Voicings --------------------------------------------------------------
    if (! analysis.rhythm.monophonic)
    {
        // A handful of voicings says everything a whole arrangement would, and
        // sampling the lot costs segments x notes.
        constexpr size_t kMaxVoicingSamples = 64;
        size_t sampled = 0;

        for (const auto& segment : analysis.progression)
        {
            if (++sampled > kMaxVoicingSamples)
                break;

            std::vector<int> sounding;
            for (const auto& note : sequence.notes)
            {
                if (note.startTick > segment.startTick + slotTicks)
                    break; // notes are sorted, nothing later can overlap the start
                if (note.endTick() > segment.startTick)
                    sounding.push_back(note.pitch);
            }

            std::sort(sounding.begin(), sounding.end());
            sounding.erase(std::unique(sounding.begin(), sounding.end()), sounding.end());
            if (sounding.size() < 2 || sounding.size() > 6)
                continue;

            // Measure from the root below the lowest voice, so inversions keep
            // their shape and the voicing can be rebuilt on any root.
            int reference = nearestPitchWithClass(segment.chord.root, sounding.front());
            if (reference > sounding.front())
                reference -= 12;

            std::vector<int> intervals;
            bool usable = true;
            for (int pitch : sounding)
            {
                const int interval = pitch - reference;
                if (interval < 0 || interval > 36)
                {
                    usable = false;
                    break;
                }
                intervals.push_back(interval);
            }

            if (usable && intervals.size() >= 2)
                ++model.voicings[intervals];
        }
    }

    // ---- Progressions -----------------------------------------------------------
    if (analysis.progression.size() >= 2 && analysis.progression.size() <= 8)
        ++model.progressions[analysis.romanNumeralString()];
}

void StyleModel::merge(const StyleModel& other)
{
    clipsLearned += other.clipsLearned;
    barsLearned += other.barsLearned;

    for (const auto& [step, count] : other.stepMarginal)
        stepMarginal[step] += count;
    for (const auto& [previous, counts] : other.stepTransitions)
        for (const auto& [step, count] : counts)
            stepTransitions[previous][step] += count;
    for (const auto& [interval, count] : other.strongBeatInterval)
        strongBeatInterval[interval] += count;

    for (size_t slotClass = 0; slotClass < bassIntervalBySlotClass.size(); ++slotClass)
        for (const auto& [interval, count] : other.bassIntervalBySlotClass[slotClass])
            bassIntervalBySlotClass[slotClass][interval] += count;

    for (const auto& [role, bank] : other.patterns)
    {
        auto& mine = patterns[role];
        for (const auto& [mask, pattern] : bank)
        {
            const auto existing = mine.find(mask);
            if (existing == mine.end())
            {
                mine.emplace(mask, pattern);
                continue;
            }

            // Weighted average of the two, so a merge matches what one pass
            // over the same clips would have produced.
            auto& into = existing->second;
            const float total = static_cast<float>(into.count + pattern.count);
            for (size_t slot = 0; slot < into.velocity.size(); ++slot)
            {
                into.velocity[slot] = static_cast<uint8_t>(
                    (into.velocity[slot] * into.count + pattern.velocity[slot] * pattern.count) / total);
                into.lengthSlots[slot] = static_cast<uint8_t>(
                    (into.lengthSlots[slot] * into.count + pattern.lengthSlots[slot] * pattern.count) / total);
            }
            into.count += pattern.count;
        }
    }

    for (const auto& [intervals, count] : other.voicings)
        voicings[intervals] += count;
    for (const auto& [progression, count] : other.progressions)
        progressions[progression] += count;
}

StyleModel buildStyleModel(const std::vector<const LibraryEntry*>& entries,
                           const std::function<void(size_t, size_t)>& progress,
                           std::vector<std::string>* errors)
{
    StyleModel model;
    size_t done = 0;

    for (const auto* entry : entries)
    {
        ++done;
        if (progress)
            progress(done, entries.size());

        if (entry == nullptr || entry->drums)
            continue;

        NoteSequence sequence;
        std::string error;
        if (! midi::readFromFile(entry->path, sequence, error))
        {
            if (errors != nullptr)
                errors->push_back(entry->relativePath + ": " + error);
            continue;
        }

        learnFromClip(model, sequence, analyze(sequence), entry->folderRole);

        // Keep memory bounded while working through a very large collection.
        if ((done % 500) == 0)
            model.prune(1024, 512, 800);
    }

    model.prune();
    return model;
}

StyleModel buildStyleModel(const LibraryIndex& index,
                           const std::function<void(size_t, size_t)>& progress,
                           std::vector<std::string>* errors)
{
    std::vector<const LibraryEntry*> entries;
    entries.reserve(index.entries.size());
    for (const auto& entry : index.entries)
        entries.push_back(&entry);
    return buildStyleModel(entries, progress, errors);
}

// ---------------------------------------------------------------------------
// Serialisation
// ---------------------------------------------------------------------------

namespace
{

json::Value countsToJson(const std::map<int, int>& counts)
{
    auto object = json::Value::object();
    for (const auto& [value, count] : counts)
        object.set(std::to_string(value), json::Value(count));
    return object;
}

void countsFromJson(const json::Value& value, std::map<int, int>& out)
{
    out.clear();
    for (const auto& [key, count] : value.fields())
        out[std::stoi(key)] = count.asInt(0);
}

json::Value bytesToJson(const std::array<uint8_t, 16>& values)
{
    auto array = json::Value::array();
    for (uint8_t value : values)
        array.push(json::Value(static_cast<int>(value)));
    return array;
}

void bytesFromJson(const json::Value& value, std::array<uint8_t, 16>& out)
{
    out.fill(0);
    const auto& items = value.items();
    for (size_t i = 0; i < items.size() && i < out.size(); ++i)
        out[i] = static_cast<uint8_t>(std::clamp(items[i].asInt(0), 0, 255));
}

} // namespace

bool saveStyleModel(const std::string& path, const StyleModel& model, std::string& error)
{
    auto root = json::Value::object();
    root.set("version", json::Value(1));
    root.set("clips", json::Value(model.clipsLearned));
    root.set("bars", json::Value(model.barsLearned));
    root.set("stepMarginal", countsToJson(model.stepMarginal));
    root.set("strongBeatInterval", countsToJson(model.strongBeatInterval));

    auto transitions = json::Value::object();
    for (const auto& [previous, counts] : model.stepTransitions)
        transitions.set(std::to_string(previous), countsToJson(counts));
    root.set("stepTransitions", std::move(transitions));

    auto bassIntervals = json::Value::array();
    for (const auto& counts : model.bassIntervalBySlotClass)
        bassIntervals.push(countsToJson(counts));
    root.set("bassIntervals", std::move(bassIntervals));

    auto patterns = json::Value::object();
    for (const auto& [role, bank] : model.patterns)
    {
        auto array = json::Value::array();
        for (const auto& [mask, pattern] : bank)
        {
            auto item = json::Value::object();
            item.set("mask", json::Value(static_cast<int>(pattern.onsets)));
            item.set("n", json::Value(pattern.count));
            item.set("vel", bytesToJson(pattern.velocity));
            item.set("len", bytesToJson(pattern.lengthSlots));
            array.push(std::move(item));
        }
        patterns.set(toString(role), std::move(array));
    }
    root.set("patterns", std::move(patterns));

    auto voicings = json::Value::array();
    for (const auto& [intervals, count] : model.voicings)
    {
        auto item = json::Value::object();
        auto array = json::Value::array();
        for (int interval : intervals)
            array.push(json::Value(interval));
        item.set("i", std::move(array));
        item.set("n", json::Value(count));
        voicings.push(std::move(item));
    }
    root.set("voicings", std::move(voicings));

    auto progressions = json::Value::array();
    for (const auto& [text, count] : model.progressions)
    {
        auto item = json::Value::object();
        item.set("p", json::Value(text));
        item.set("n", json::Value(count));
        progressions.push(std::move(item));
    }
    root.set("progressions", std::move(progressions));

    std::ofstream stream(path);
    if (! stream)
    {
        error = "Could not write '" + path + "'.";
        return false;
    }
    stream << root.toString(0);
    if (! stream.good())
    {
        error = "Failed while writing '" + path + "'.";
        return false;
    }
    error.clear();
    return true;
}

bool loadStyleModel(const std::string& path, StyleModel& model, std::string& error)
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

    if (! root.isObject() || ! root.has("clips"))
    {
        error = "'" + path + "' is not a Harmonia style model.";
        return false;
    }

    model = StyleModel {};
    model.clipsLearned = root["clips"].asInt(0);
    model.barsLearned = root["bars"].asInt(0);
    countsFromJson(root["stepMarginal"], model.stepMarginal);
    countsFromJson(root["strongBeatInterval"], model.strongBeatInterval);

    for (const auto& [previous, counts] : root["stepTransitions"].fields())
        countsFromJson(counts, model.stepTransitions[std::stoi(previous)]);

    const auto& bassIntervals = root["bassIntervals"].items();
    for (size_t i = 0; i < bassIntervals.size() && i < model.bassIntervalBySlotClass.size(); ++i)
        countsFromJson(bassIntervals[i], model.bassIntervalBySlotClass[i]);

    for (const auto& [roleName, array] : root["patterns"].fields())
    {
        StyleRole role = StyleRole::Melody;
        if (! styleRoleFromString(roleName, role))
            continue;

        for (const auto& item : array.items())
        {
            RhythmPattern pattern;
            pattern.onsets = static_cast<uint16_t>(item["mask"].asInt(0));
            pattern.count = std::max(1, item["n"].asInt(1));
            bytesFromJson(item["vel"], pattern.velocity);
            bytesFromJson(item["len"], pattern.lengthSlots);
            if (pattern.onsets != 0)
                model.patterns[role].emplace(pattern.onsets, pattern);
        }
    }

    for (const auto& item : root["voicings"].items())
    {
        std::vector<int> intervals;
        for (const auto& interval : item["i"].items())
            intervals.push_back(interval.asInt(0));
        if (! intervals.empty())
            model.voicings[intervals] = std::max(1, item["n"].asInt(1));
    }

    for (const auto& item : root["progressions"].items())
    {
        const auto progression = item["p"].asString("");
        if (! progression.empty())
            model.progressions[progression] = std::max(1, item["n"].asInt(1));
    }

    error.clear();
    return true;
}

// ---------------------------------------------------------------------------
// Sampling
// ---------------------------------------------------------------------------

const RhythmPattern* pickPattern(const StyleModel& model, StyleRole role, float targetOnsets, Rng& rng)
{
    const auto it = model.patterns.find(role);
    if (it == model.patterns.end() || it->second.empty())
        return nullptr;

    std::vector<const RhythmPattern*> candidates;
    std::vector<float> weights;
    candidates.reserve(it->second.size());
    weights.reserve(it->second.size());

    for (const auto& [mask, pattern] : it->second)
    {
        // Common bars are more likely, and bars close to the density the user
        // asked for are much more likely.
        const float distance = std::abs(static_cast<float>(pattern.onsetCount()) - targetOnsets);
        const float fit = std::exp(-distance * distance / 8.0f);
        candidates.push_back(&pattern);
        weights.push_back(std::sqrt(static_cast<float>(pattern.count)) * fit + 0.001f);
    }

    const int index = rng.weighted(weights);
    return index >= 0 ? candidates[static_cast<size_t>(index)] : nullptr;
}

int sampleStep(const StyleModel& model, int previousStep, Rng& rng, int fallback)
{
    const auto context = model.stepTransitions.find(previousStep);
    if (context != model.stepTransitions.end() && ! context->second.empty())
        return sampleFromCounts(context->second, rng, fallback);
    return sampleFromCounts(model.stepMarginal, rng, fallback);
}

int sampleBassInterval(const StyleModel& model, int slotClass, Rng& rng, int fallback)
{
    const auto index = static_cast<size_t>(std::clamp(slotClass, 0, 3));
    const auto& counts = model.bassIntervalBySlotClass[index];
    if (! counts.empty())
        return sampleFromCounts(counts, rng, fallback);

    // Fall back to whatever the corpus does on stronger positions.
    for (size_t other = 0; other < model.bassIntervalBySlotClass.size(); ++other)
        if (! model.bassIntervalBySlotClass[other].empty())
            return sampleFromCounts(model.bassIntervalBySlotClass[other], rng, fallback);
    return fallback;
}

std::vector<int> sampleVoicing(const StyleModel& model, size_t voiceCount, Rng& rng)
{
    if (model.voicings.empty())
        return {};

    std::vector<const std::vector<int>*> candidates;
    std::vector<float> weights;
    for (const auto& [intervals, count] : model.voicings)
    {
        if (voiceCount > 0 && intervals.size() > voiceCount)
            continue;
        candidates.push_back(&intervals);
        weights.push_back(static_cast<float>(count));
    }
    if (candidates.empty())
        return {};

    const int index = rng.weighted(weights);
    return index >= 0 ? *candidates[static_cast<size_t>(index)] : std::vector<int> {};
}

} // namespace harmonia
