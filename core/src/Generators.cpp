#include "harmonia/Generators.h"

#include "harmonia/Rng.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace harmonia
{

const char* toString(PartType part)
{
    switch (part)
    {
        case PartType::Pad:           return "Pad";
        case PartType::Chords:        return "Chords";
        case PartType::Melody:        return "Melody";
        case PartType::CounterMelody: return "Counter Melody";
        case PartType::Bass:          return "Bass";
        case PartType::Arp:           return "Arp";
        case PartType::Pluck:         return "Pluck";
    }
    return "Pad";
}

bool partTypeFromString(const std::string& text, PartType& out)
{
    std::string lower;
    lower.reserve(text.size());
    for (char c : text)
        lower.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));

    if (lower == "pad")                                    { out = PartType::Pad; return true; }
    if (lower == "chords" || lower == "comp")              { out = PartType::Chords; return true; }
    if (lower == "melody" || lower == "lead")              { out = PartType::Melody; return true; }
    if (lower == "counter" || lower == "countermelody")    { out = PartType::CounterMelody; return true; }
    if (lower == "bass")                                   { out = PartType::Bass; return true; }
    if (lower == "arp" || lower == "arpeggio")             { out = PartType::Arp; return true; }
    if (lower == "pluck" || lower == "pluk" || lower == "stab") { out = PartType::Pluck; return true; }
    return false;
}

void defaultRange(PartType part, int& lowPitch, int& highPitch)
{
    switch (part)
    {
        case PartType::Pad:           lowPitch = 55; highPitch = 84; break;
        case PartType::Chords:        lowPitch = 52; highPitch = 81; break;
        case PartType::Melody:        lowPitch = 64; highPitch = 88; break;
        case PartType::CounterMelody: lowPitch = 59; highPitch = 81; break;
        case PartType::Bass:          lowPitch = 28; highPitch = 55; break;
        case PartType::Arp:           lowPitch = 60; highPitch = 91; break;
        case PartType::Pluck:         lowPitch = 60; highPitch = 91; break;
    }
}

namespace
{

struct Onset
{
    int64_t tick = 0;
    int64_t length = 0;
    float accent = 0.5f;
    int velocity = 0; // 0 = derive it from the accent
};

struct Context
{
    const Analysis* analysis = nullptr;
    const NoteSequence* source = nullptr;
    const GenerateOptions* options = nullptr;
    Rng rng { 1u };

    int ppq = kPPQ;
    int64_t barTicks = kPPQ * 4;
    int64_t beatTicks = kPPQ;
    int64_t slotTicks = kPPQ / 4;
    int64_t totalTicks = kPPQ * 4;
    int bars = 1;

    std::vector<ChordSegment> progression;
    /** The groove the generators write against: the source clip's, or a donor. */
    RhythmProfile rhythm;
    /** Weight per 16th slot of a bar where the anchor part starts notes, empty
        when there is no anchor. Parts lean towards entering with it. */
    std::array<float, 16> anchorSlots { };
    bool hasAnchor = false;
    Key key;

    /** True when the corpus has material for this role and the dice agree. */
    bool useStyle(StyleRole role)
    {
        const auto* style = options->style;
        if (style == nullptr || style->empty() || ! style->hasPatternsFor(role))
            return false;
        return rng.chance(std::clamp(options->styleAmount, 0.0f, 1.0f));
    }

    const StyleModel* style() const { return options->style; }
    int lowPitch = 48;
    int highPitch = 84;

    const ChordSegment& chordAt(int64_t tick) const
    {
        for (const auto& segment : progression)
            if (tick >= segment.startTick && tick < segment.endTick())
                return segment;
        return progression.back();
    }
};

int clampPitch(int pitch, int low, int high)
{
    while (pitch < low)
        pitch += 12;
    while (pitch > high)
        pitch -= 12;
    return std::clamp(pitch, 0, 127);
}

int nearestChordTone(int pitch, const Chord& chord)
{
    const auto pcs = chord.pitchClasses();
    int best = pitch;
    int bestDistance = 128;
    for (int pc : pcs)
    {
        const int candidate = nearestPitchWithClass(pc, pitch);
        for (int shift : { -12, 0, 12 })
        {
            const int option = candidate + shift;
            const int distance = std::abs(option - pitch);
            if (distance < bestDistance)
            {
                bestDistance = distance;
                best = option;
            }
        }
    }
    return best;
}

/** A voicing shape lifted from the corpus, snapped onto the chord that is
    actually playing. The corpus supplies the spacing and register; the harmony
    still comes from the progression, so a major shape over a minor chord does
    not drag a wrong third in. */
std::vector<int> learnedVoicing(struct Context& ctx, const Chord& chord,
                                const std::vector<int>& previous, int maxVoices);

/** Positional weight of a 16th slot inside a bar - downbeats are heaviest. */
float slotWeight(int slot)
{
    if (slot % 16 == 0) return 1.00f;
    if (slot % 8 == 0)  return 0.88f;
    if (slot % 4 == 0)  return 0.72f;
    if (slot % 2 == 0)  return 0.48f;
    return 0.28f;
}

std::vector<Onset> buildOnsets(Context& ctx, float density, float legato, bool useSourceGrid,
                               StyleRole role = StyleRole::Melody)
{
    const auto& rhythm = ctx.rhythm;
    const int slotsPerBar = 16;
    const int64_t slotTicks = std::max<int64_t>(1, ctx.barTicks / slotsPerBar);
    const float multiplier = 0.15f + 0.85f * std::clamp(density, 0.0f, 1.0f);

    // Only lean on the source groove when there is a groove to lean on: a pad
    // holding one chord per bar says nothing useful about a melody's rhythm.
    float sourceInfluence = 0.0f;
    if (useSourceGrid)
    {
        int activeSlots = 0;
        for (float value : rhythm.grid)
            if (value > 0.15f)
                ++activeSlots;
        // Asking for a donor groove is explicit, so it counts for more than
        // simply following whatever clip happens to be loaded.
        const float ceiling = ctx.options->useGrooveDonor ? 0.9f : 0.6f;
        sourceInfluence = activeSlots >= 4 ? ceiling : (activeSlots >= 2 ? ceiling * 0.5f : 0.0f);
    }

    // Roughly how many onsets a bar should have at this density, used both to
    // pick a learned bar and to shape a generated one.
    const float targetOnsets = 1.0f + multiplier * 9.0f;

    // Where the anchor moves, this part is encouraged to move too. It is a pull,
    // not a rule - a line that only ever lands with the pad stops being a line.
    const auto anchorPull = [&ctx](int slot)
    {
        if (! ctx.hasAnchor)
            return 0.0f;
        return ctx.anchorSlots[static_cast<size_t>(slot % 16)];
    };

    struct Hit
    {
        int64_t tick;
        int velocity;
        int64_t learnedLength;
    };

    // One bar of rhythm, as offsets inside the bar rather than absolute ticks,
    // so the same cell can be stamped across a whole phrase.
    struct BarCell
    {
        std::vector<Hit> hits; // tick field holds the offset within the bar
    };

    const auto makeCell = [&]() -> BarCell
    {
        BarCell cell;

        // A bar lifted from your own collection, when there is one.
        const RhythmPattern* learned = ctx.useStyle(role)
                                           ? pickPattern(*ctx.style(), role, targetOnsets, ctx.rng)
                                           : nullptr;

        if (learned != nullptr)
        {
            for (int slot = 0; slot < slotsPerBar; ++slot)
            {
                if (! learned->hasOnset(slot))
                    continue;
                const int64_t length = static_cast<int64_t>(std::max<uint8_t>(1, learned->lengthSlots[static_cast<size_t>(slot)]))
                                     * slotTicks;
                cell.hits.push_back({ slot * slotTicks,
                                      learned->velocity[static_cast<size_t>(slot)],
                                      length });
            }
        }
        else
        {
            for (int slot = 0; slot < slotsPerBar; ++slot)
            {
                const float weight = (1.0f - sourceInfluence) * slotWeight(slot)
                                   + sourceInfluence * rhythm.grid[static_cast<size_t>(slot)];

                // A bar lifted from the corpus is the user's own material and is
                // left exactly as learned; only a generated bar takes the pull.
                const float probability = std::clamp((weight + 0.35f * anchorPull(slot)) * multiplier,
                                                     0.0f, 0.96f);
                if (ctx.rng.chance(probability))
                    cell.hits.push_back({ slot * slotTicks, 0, 0 });
            }
        }

        if (cell.hits.empty())
            cell.hits.push_back({ 0, 0, 0 });
        return cell;
    };

    // Music in this space loops: one rhythmic cell carried across the phrase,
    // not a new shape every bar. Resampling per bar is what makes a part sound
    // scattered - nothing repeats, so nothing reads as a groove.
    constexpr int kPhraseBars = 4;
    const BarCell mainCell = makeCell();

    // The last bar of a phrase may answer with a different cell - a fill - and
    // phrase two onwards may swap the fill, but bars 1-3 always hold the cell.
    const BarCell fillCell = ctx.rng.chance(0.45f) ? makeCell() : mainCell;

    std::vector<Hit> hits;
    for (int bar = 0; bar < ctx.bars; ++bar)
    {
        const int64_t barStart = static_cast<int64_t>(bar) * ctx.barTicks;
        const bool lastOfPhrase = (bar % kPhraseBars) == kPhraseBars - 1;
        const BarCell& cell = lastOfPhrase ? fillCell : mainCell;

        for (const auto& hit : cell.hits)
            hits.push_back({ barStart + hit.tick, hit.velocity, hit.learnedLength });
    }
    std::sort(hits.begin(), hits.end(), [](const Hit& a, const Hit& b) { return a.tick < b.tick; });
    hits.erase(std::unique(hits.begin(), hits.end(),
                           [](const Hit& a, const Hit& b) { return a.tick == b.tick; }),
               hits.end());

    std::vector<Onset> onsets;
    onsets.reserve(hits.size());
    for (size_t i = 0; i < hits.size(); ++i)
    {
        const int64_t next = (i + 1 < hits.size()) ? hits[i + 1].tick : ctx.totalTicks;
        const int64_t gap = std::max<int64_t>(slotTicks, next - hits[i].tick);

        Onset onset;
        onset.tick = hits[i].tick;
        onset.velocity = hits[i].velocity;
        if (hits[i].learnedLength > 0)
        {
            onset.length = std::min(hits[i].learnedLength, gap);
            // A staccato eighth lifted from a pack loop leaves dead air when
            // the phrase is sparse. Sustained roles stretch toward the next
            // note; plucks (low legato) keep their bite.
            if (legato >= 0.6f)
                onset.length = std::max(onset.length,
                                        static_cast<int64_t>(static_cast<double>(gap) * legato * 0.85));
        }
        else
        {
            onset.length = std::max<int64_t>(slotTicks / 2, static_cast<int64_t>(static_cast<double>(gap) * legato));
        }
        const int slot = static_cast<int>((hits[i].tick % ctx.barTicks) / slotTicks);
        onset.accent = std::min(1.0f, slotWeight(slot) + 0.25f * anchorPull(slot));
        onsets.push_back(onset);
    }
    return onsets;
}

/** Velocity for an onset: what the corpus played, or what the accent implies. */
int velocityFor(const Onset& onset, int baseVelocity, float accentDepth)
{
    if (onset.velocity > 0)
        return std::clamp(onset.velocity, 1, 127);
    return std::clamp(static_cast<int>(static_cast<float>(baseVelocity)
                                       * (1.0f - accentDepth + accentDepth * onset.accent)),
                      1, 127);
}

std::vector<int> learnedVoicing(Context& ctx, const Chord& chord,
                                const std::vector<int>& previous, int maxVoices)
{
    const auto* style = ctx.style();
    if (style == nullptr || style->voicings.empty())
        return {};
    if (! ctx.rng.chance(std::clamp(ctx.options->styleAmount, 0.0f, 1.0f)))
        return {};

    const auto intervals = sampleVoicing(*style, static_cast<size_t>(std::max(2, maxVoices)), ctx.rng);
    if (intervals.size() < 2)
        return {};

    const int anchor = previous.empty() ? (ctx.lowPitch + ctx.highPitch) / 2 - 6 : previous.front();
    int root = nearestPitchWithClass(chord.root, anchor);

    std::vector<int> voices;
    voices.reserve(intervals.size());
    for (int interval : intervals)
        voices.push_back(root + interval);

    // Slide the whole stack by octaves until it sits in the register.
    for (int attempt = 0; attempt < 4 && voices.back() > ctx.highPitch; ++attempt)
        for (int& pitch : voices)
            pitch -= 12;
    for (int attempt = 0; attempt < 4 && voices.front() < ctx.lowPitch; ++attempt)
        for (int& pitch : voices)
            pitch += 12;
    if (voices.front() < ctx.lowPitch || voices.back() > ctx.highPitch)
        return {};

    for (int& pitch : voices)
        pitch = std::clamp(nearestChordTone(pitch, chord), ctx.lowPitch, ctx.highPitch);

    std::sort(voices.begin(), voices.end());
    voices.erase(std::unique(voices.begin(), voices.end()), voices.end());
    return voices.size() >= 2 ? voices : std::vector<int> {};
}

void applySwing(Context& ctx, std::vector<Onset>& onsets)
{
    const float swing = std::clamp(ctx.options->swing, 0.0f, 1.0f);
    if (swing <= 0.0f)
        return;

    const int64_t eighth = std::max<int64_t>(1, ctx.beatTicks / 2);
    const auto offset = static_cast<int64_t>(static_cast<double>(eighth) * 0.333 * swing);
    for (auto& onset : onsets)
    {
        if (((onset.tick / eighth) % 2) == 1)
        {
            onset.tick += offset;
            onset.length = std::max<int64_t>(1, onset.length - offset);
        }
    }
}

void humanise(Context& ctx, NoteSequence& sequence, bool jitterTiming)
{
    const float amount = std::clamp(ctx.options->humanize, 0.0f, 1.0f);
    if (amount <= 0.0f)
        return;

    const auto maxOffset = jitterTiming
                               ? static_cast<int64_t>(static_cast<double>(ctx.ppq) * 0.03 * amount)
                               : int64_t { 0 };
    const int velocitySpread = static_cast<int>(18.0f * amount);

    for (auto& note : sequence.notes)
    {
        if (maxOffset > 0)
        {
            const int64_t jitter = ctx.rng.range(static_cast<int>(-maxOffset), static_cast<int>(maxOffset));
            note.startTick = std::max<int64_t>(0, note.startTick + jitter);
        }
        if (velocitySpread > 0)
            note.velocity = std::clamp(note.velocity + ctx.rng.range(-velocitySpread, velocitySpread), 1, 127);
    }
}

/** Keeps the new part clear of the register the source clip already occupies. */
void adjustRangeForSource(Context& ctx, PartType part)
{
    if (! ctx.options->avoidSourceCollisions || ctx.source == nullptr || ctx.source->notes.empty())
        return;

    const auto& rhythm = ctx.analysis->rhythm;
    const SourceRole role = ctx.analysis->role;

    switch (part)
    {
        case PartType::Pad:
        case PartType::Chords:
            if (role == SourceRole::Bass)
                ctx.lowPitch = std::max(ctx.lowPitch, rhythm.highestPitch + 2);
            else if (role == SourceRole::Lead || role == SourceRole::Arp)
                ctx.highPitch = std::min(ctx.highPitch, std::max(ctx.lowPitch + 12, rhythm.lowestPitch - 1));
            break;

        case PartType::Melody:
        case PartType::CounterMelody:
            if (role == SourceRole::Chords || role == SourceRole::Arp)
                ctx.lowPitch = std::max(ctx.lowPitch, rhythm.highestPitch - 4);
            break;

        case PartType::Bass:
            if (role != SourceRole::Bass)
                ctx.highPitch = std::min(ctx.highPitch, std::max(ctx.lowPitch + 12, rhythm.lowestPitch - 1));
            break;

        case PartType::Arp:
        case PartType::Pluck:
            if (role == SourceRole::Bass)
                ctx.lowPitch = std::max(ctx.lowPitch, rhythm.highestPitch + 4);
            break;
    }

    if (ctx.highPitch - ctx.lowPitch < 12)
        ctx.highPitch = ctx.lowPitch + 12;
    ctx.lowPitch = std::clamp(ctx.lowPitch, 12, 108);
    ctx.highPitch = std::clamp(ctx.highPitch, ctx.lowPitch + 12, 120);
}

// ---------------------------------------------------------------------------
// Part writers
// ---------------------------------------------------------------------------

NoteSequence writePad(Context& ctx)
{
    NoteSequence out;
    const auto& options = *ctx.options;
    const bool retriggerPerBar = options.density > 0.62f;
    std::vector<int> previousVoicing;

    for (const auto& segment : ctx.progression)
    {
        Chord chord = extendChord(segment.chord, ctx.key, options.complexity);
        auto voicing = learnedVoicing(ctx, chord, previousVoicing, options.maxVoices);
        if (voicing.empty())
            voicing = voiceChord(chord, previousVoicing, ctx.lowPitch, ctx.highPitch, options.maxVoices);
        if (voicing.empty())
            continue;
        previousVoicing = voicing;

        // A root below the voicing gives the pad a foundation, unless the source
        // clip is already the bass part.
        if (ctx.analysis->role != SourceRole::Bass && options.maxVoices >= 4)
        {
            const int root = nearestPitchWithClass(chord.root, voicing.front() - 8);
            if (root >= ctx.lowPitch - 12 && root >= 24 && root < voicing.front())
                voicing.insert(voicing.begin(), root);
        }

        std::vector<std::pair<int64_t, int64_t>> spans;
        if (retriggerPerBar)
        {
            for (int64_t t = segment.startTick; t < segment.endTick(); t += ctx.barTicks)
                spans.emplace_back(t, std::min(ctx.barTicks, segment.endTick() - t));
        }
        else
        {
            spans.emplace_back(segment.startTick, segment.lengthTick);
        }

        for (const auto& [start, length] : spans)
        {
            const int64_t sounding = std::max<int64_t>(ctx.slotTicks,
                                                       static_cast<int64_t>(static_cast<double>(length) * 0.98));
            for (size_t voice = 0; voice < voicing.size(); ++voice)
            {
                Note note;
                note.startTick = start;
                note.lengthTick = sounding;
                note.pitch = clampPitch(voicing[voice], std::max(24, ctx.lowPitch - 12), ctx.highPitch);
                // Slightly softer as the voicing climbs, so the top does not stick out.
                note.velocity = std::clamp(options.baseVelocity - static_cast<int>(voice) * 3
                                               - ctx.rng.range(0, 5),
                                           1, 127);
                note.channel = options.channel;
                out.notes.push_back(note);
            }
        }
    }
    return out;
}

NoteSequence writeChords(Context& ctx)
{
    NoteSequence out;
    const auto& options = *ctx.options;

    auto onsets = buildOnsets(ctx, 0.25f + 0.5f * options.density, 0.62f,
                              options.followSourceRhythm || options.useGrooveDonor,
                              StyleRole::Chords);
    applySwing(ctx, onsets);

    std::vector<int> previousVoicing;
    for (const auto& onset : onsets)
    {
        const auto& segment = ctx.chordAt(onset.tick);
        Chord chord = extendChord(segment.chord, ctx.key, options.complexity);
        auto voicing = learnedVoicing(ctx, chord, previousVoicing, options.maxVoices);
        if (voicing.empty())
            voicing = voiceChord(chord, previousVoicing, ctx.lowPitch, ctx.highPitch, options.maxVoices);
        if (voicing.empty())
            continue;
        previousVoicing = voicing;

        for (size_t voice = 0; voice < voicing.size(); ++voice)
        {
            Note note;
            note.startTick = onset.tick;
            note.lengthTick = onset.length;
            note.pitch = clampPitch(voicing[voice], ctx.lowPitch, ctx.highPitch);
            note.velocity = std::clamp(velocityFor(onset, options.baseVelocity, 0.32f)
                                           - static_cast<int>(voice) * 2,
                                       1, 127);
            note.channel = options.channel;
            out.notes.push_back(note);
        }
    }
    return out;
}

/** Bass written from the corpus: your bars, and your intervals over the root. */
NoteSequence writeLearnedBass(Context& ctx)
{
    NoteSequence out;
    const auto& options = *ctx.options;

    auto onsets = buildOnsets(ctx, options.density, 0.85f, false, StyleRole::Bass);
    applySwing(ctx, onsets);

    for (const auto& onset : onsets)
    {
        const auto& segment = ctx.chordAt(onset.tick);
        const int slot = static_cast<int>((onset.tick % ctx.barTicks) / ctx.slotTicks);
        const int interval = sampleBassInterval(*ctx.style(), slotClassFor(slot), ctx.rng, 0);

        int pitch = clampPitch(nearestPitchWithClass(mod12(segment.chord.root + interval), ctx.lowPitch + 7),
                               ctx.lowPitch, ctx.highPitch);

        // Trust the corpus, but do not let it drop a note that belongs to
        // neither the chord nor the key onto a strong beat.
        if (! segment.chord.containsPitchClass(pitch) && ! ctx.key.contains(mod12(pitch))
            && ctx.rng.chance(0.7f))
            pitch = clampPitch(nearestPitchWithClass(segment.chord.root, pitch), ctx.lowPitch, ctx.highPitch);

        Note note;
        note.startTick = onset.tick;
        note.lengthTick = std::max<int64_t>(ctx.slotTicks / 2, onset.length);
        note.pitch = pitch;
        note.velocity = velocityFor(onset, options.baseVelocity, 0.25f);
        note.channel = options.channel;
        out.notes.push_back(note);
    }
    return out;
}

NoteSequence writeBass(Context& ctx)
{
    if (ctx.useStyle(StyleRole::Bass) && ! ctx.style()->bassIntervalBySlotClass[0].empty())
        return writeLearnedBass(ctx);

    NoteSequence out;
    const auto& options = *ctx.options;
    const float density = std::clamp(options.density, 0.0f, 1.0f);

    for (size_t index = 0; index < ctx.progression.size(); ++index)
    {
        const auto& segment = ctx.progression[index];
        const Chord& chord = segment.chord;
        const Chord& nextChord = ctx.progression[(index + 1) % ctx.progression.size()].chord;

        const int rootPitch = clampPitch(nearestPitchWithClass(chord.root, ctx.lowPitch + 7),
                                         ctx.lowPitch, ctx.highPitch);
        const int fifthPitch = clampPitch(nearestPitchWithClass(mod12(chord.root + 7), rootPitch + 5),
                                          ctx.lowPitch, ctx.highPitch);
        const int thirdInterval = chord.containsPitchClass(mod12(chord.root + 3)) ? 3 : 4;
        const int thirdPitch = clampPitch(nearestPitchWithClass(mod12(chord.root + thirdInterval), rootPitch + 4),
                                          ctx.lowPitch, ctx.highPitch);
        const int octavePitch = std::min(ctx.highPitch, rootPitch + 12);

        int64_t step = ctx.beatTicks;
        if (density < 0.28f)
            step = std::max(ctx.beatTicks, segment.lengthTick);
        else if (density > 0.66f)
            step = std::max<int64_t>(1, ctx.beatTicks / 2);

        const int64_t end = segment.endTick();
        int position = 0;
        for (int64_t t = segment.startTick; t < end; t += step, ++position)
        {
            const int64_t length = std::min(step, end - t);
            const bool last = (t + step) >= end;

            int pitch = rootPitch;
            if (position > 0)
            {
                if (last && density > 0.45f && options.complexity > 0.4f)
                {
                    // Chromatic or diatonic approach into the next root.
                    const int target = clampPitch(nearestPitchWithClass(nextChord.root, rootPitch),
                                                  ctx.lowPitch, ctx.highPitch);
                    const int direction = target >= rootPitch ? -1 : 1;
                    pitch = clampPitch(target + direction, ctx.lowPitch, ctx.highPitch);
                }
                else
                {
                    std::vector<float> weights { 3.0f,                       // root
                                                 1.6f * density,             // fifth
                                                 1.1f * options.complexity,  // third
                                                 1.0f * density };           // octave
                    switch (ctx.rng.weighted(weights))
                    {
                        case 1: pitch = fifthPitch; break;
                        case 2: pitch = thirdPitch; break;
                        case 3: pitch = octavePitch; break;
                        default: pitch = rootPitch; break;
                    }
                }
            }

            Note note;
            note.startTick = t;
            note.lengthTick = std::max<int64_t>(ctx.slotTicks / 2,
                                                static_cast<int64_t>(static_cast<double>(length) * (density > 0.66f ? 0.72 : 0.92)));
            note.pitch = pitch;
            note.velocity = std::clamp(options.baseVelocity - (position == 0 ? 0 : 8) - ctx.rng.range(0, 6), 1, 127);
            note.channel = options.channel;
            out.notes.push_back(note);

            // Thin out busy patterns so they breathe.
            if (density > 0.66f && position > 0 && ctx.rng.chance(0.22f * (1.0f - density)))
                out.notes.pop_back();
        }
    }
    return out;
}

NoteSequence writeArp(Context& ctx)
{
    NoteSequence out;
    const auto& options = *ctx.options;

    const int64_t step = options.density < 0.45f ? std::max<int64_t>(1, ctx.beatTicks / 2)
                                                 : std::max<int64_t>(1, ctx.beatTicks / 4);
    const double gate = 0.55 + 0.35 * (1.0 - static_cast<double>(options.density));
    const int slotsPerBar = 16;
    const int64_t gridSlot = std::max<int64_t>(1, ctx.barTicks / slotsPerBar);

    // An arp is a figure, not a phrase: the run itself has to stay regular or it
    // stops being an arp. So a new seed does not scramble the notes - it picks a
    // different figure over the same chords. Without these the pattern is fully
    // determined by the chord and the seed changes nothing at all, which is
    // exactly how it behaved before.
    const int rotation = ctx.rng.range(0, 7);              // where the run starts
    const int octaveSpan = ctx.rng.chance(0.4f) ? 2 : 1;   // how far it reaches
    const bool liftCycles = ctx.rng.chance(0.35f);         // octave jump each cycle
    const int cycleLength = ctx.rng.range(3, 8);
    const float restChance = 0.06f * ctx.rng.unit() * (1.0f - options.density);

    // How much the groove under the plugin - the source clip, a donor, or the
    // learned corpus - is allowed to shape the accents. The run stays even; only
    // its weight moves, which is what makes an arp sit in a track instead of on
    // top of it.
    float grooveInfluence = 0.0f;
    if (options.followSourceRhythm || options.useGrooveDonor || options.companion != nullptr)
    {
        int activeSlots = 0;
        for (float value : ctx.rhythm.grid)
            if (value > 0.15f)
                ++activeSlots;
        grooveInfluence = activeSlots >= 4 ? (options.useGrooveDonor ? 0.75f : 0.5f)
                                           : (activeSlots >= 2 ? 0.3f : 0.0f);
    }
    if (grooveInfluence <= 0.0f && ctx.useStyle(StyleRole::Arp))
    {
        const float targetOnsets = 4.0f + 8.0f * std::clamp(options.density, 0.0f, 1.0f);
        if (const auto* learned = pickPattern(*ctx.style(), StyleRole::Arp, targetOnsets, ctx.rng))
        {
            grooveInfluence = 0.55f;
            for (int slot = 0; slot < slotsPerBar; ++slot)
                ctx.rhythm.grid[static_cast<size_t>(slot)] =
                    learned->hasOnset(slot)
                        ? std::clamp(static_cast<float>(learned->velocity[static_cast<size_t>(slot)]) / 127.0f,
                                     0.2f, 1.0f)
                        : 0.0f;
        }
    }

    int index = rotation;
    bool ascending = options.arpPattern != ArpPattern::Down && options.arpPattern != ArpPattern::DownUp;

    for (int64_t t = 0; t < ctx.totalTicks; t += step, ++index)
    {
        const auto& segment = ctx.chordAt(t);
        Chord chord = extendChord(segment.chord, ctx.key, options.complexity);

        // Spread the chord tones across the available register.
        std::vector<int> tones;
        const int ceiling = octaveSpan >= 2
                                ? ctx.highPitch
                                : std::min(ctx.highPitch, ctx.lowPitch + 14);
        for (int pitch = ctx.lowPitch; pitch <= ceiling; ++pitch)
            if (chord.containsPitchClass(pitch))
                tones.push_back(pitch);
        if (tones.empty())
            continue;

        const int size = static_cast<int>(tones.size());
        int toneIndex = 0;
        switch (options.arpPattern)
        {
            case ArpPattern::Up:      toneIndex = index % size; break;
            case ArpPattern::Down:    toneIndex = size - 1 - (index % size); break;
            case ArpPattern::Random:  toneIndex = ctx.rng.range(0, size - 1); break;
            case ArpPattern::Converge:
            {
                const int half = index % size;
                toneIndex = (half % 2 == 0) ? half / 2 : size - 1 - half / 2;
                break;
            }
            case ArpPattern::UpDown:
            case ArpPattern::DownUp:
            default:
            {
                const int period = std::max(1, size * 2 - 2);
                const int position = index % period;
                toneIndex = position < size ? position : period - position;
                if (! ascending)
                    toneIndex = size - 1 - toneIndex;
                break;
            }
        }
        toneIndex = std::clamp(toneIndex, 0, size - 1);

        const int slot = static_cast<int>((t % ctx.barTicks) / gridSlot);
        const float positional = slotWeight(slot);
        const float groove = ctx.rhythm.grid.empty()
                                 ? positional
                                 : ctx.rhythm.grid[static_cast<size_t>(slot % slotsPerBar)];
        const float weight = positional * (1.0f - grooveInfluence) + groove * grooveInfluence;

        // Never drop a downbeat - a hole there reads as a mistake, not a figure.
        if (slot != 0 && restChance > 0.0f && ctx.rng.chance(restChance * (1.0f - weight)))
            continue;

        Note note;
        note.startTick = t;
        note.lengthTick = std::max<int64_t>(1, static_cast<int64_t>(static_cast<double>(step) * gate));
        note.pitch = tones[static_cast<size_t>(toneIndex)];
        if (liftCycles && (index / std::max(1, cycleLength)) % 2 == 1
            && note.pitch + 12 <= ctx.highPitch)
            note.pitch += 12;
        note.velocity = std::clamp(static_cast<int>(static_cast<float>(options.baseVelocity)
                                                    * (0.72f + 0.28f * weight)),
                                   1, 127);
        note.channel = options.channel;
        out.notes.push_back(note);
    }
    return out;
}

/** Short chord tones riding the groove - the house pluck / stab. */
NoteSequence writePluck(Context& ctx)
{
    NoteSequence out;
    const auto& options = *ctx.options;

    auto onsets = buildOnsets(ctx, 0.35f + 0.55f * options.density, 0.4f,
                              options.followSourceRhythm || options.useGrooveDonor
                                  || options.companion != nullptr,
                              StyleRole::Pluck);
    applySwing(ctx, onsets);

    const int64_t maxLength = std::max<int64_t>(ctx.slotTicks / 2, ctx.slotTicks * 3 / 2);
    int previous = (ctx.lowPitch + ctx.highPitch) / 2;

    for (const auto& onset : onsets)
    {
        const auto& segment = ctx.chordAt(onset.tick);
        const Chord chord = extendChord(segment.chord, ctx.key, options.complexity);

        std::vector<int> tones;
        for (int pitch = ctx.lowPitch; pitch <= ctx.highPitch; ++pitch)
            if (chord.containsPitchClass(pitch))
                tones.push_back(pitch);
        if (tones.empty())
            continue;

        // Plucks jump around far more than a melody would, but not at random:
        // the closest tone is still the most likely pick.
        std::vector<float> weights;
        weights.reserve(tones.size());
        for (int pitch : tones)
        {
            const float distance = static_cast<float>(std::abs(pitch - previous));
            weights.push_back(1.0f / (1.0f + distance * (1.4f - options.complexity)));
        }
        const int index = ctx.rng.weighted(weights);
        const int pitch = tones[static_cast<size_t>(std::max(0, index))];
        previous = pitch;

        Note note;
        note.startTick = onset.tick;
        note.lengthTick = std::min(maxLength, std::max<int64_t>(ctx.slotTicks / 3, onset.length));
        note.pitch = pitch;
        note.velocity = velocityFor(onset, options.baseVelocity, 0.3f);
        note.channel = options.channel;
        out.notes.push_back(note);

        // A doubled octave on the accents gives the part some weight.
        if (options.density > 0.55f && onset.accent > 0.7f && pitch + 12 <= ctx.highPitch
            && ctx.rng.chance(0.45f))
        {
            Note doubled = note;
            doubled.pitch = pitch + 12;
            doubled.velocity = std::max(1, note.velocity - 12);
            out.notes.push_back(doubled);
        }
    }
    return out;
}

/** Shared engine behind Melody and CounterMelody. */
NoteSequence writeMelodicLine(Context& ctx, bool counterMelody)
{
    NoteSequence out;
    const auto& options = *ctx.options;

    const float density = std::clamp(options.density * (counterMelody ? 0.85f : 1.0f), 0.05f, 1.0f);
    auto onsets = buildOnsets(ctx, density, 0.86f,
                              options.useGrooveDonor || options.companion != nullptr
                                  || (options.followSourceRhythm && ! counterMelody),
                              StyleRole::Melody);
    applySwing(ctx, onsets);
    if (onsets.empty())
        return out;

    const int centre = (ctx.lowPitch + ctx.highPitch) / 2;
    const int octaveBase = nearestPitchWithClass(ctx.key.tonic, centre) - ctx.key.tonic;

    // A motif is a short list of scale-degree moves; phrases reuse and vary it.
    const int motifLength = std::clamp(static_cast<int>(onsets.size() / std::max(1, ctx.bars)), 2, 8);
    const std::vector<float> stepWeights { 0.6f, 2.2f, 3.0f, 1.4f, 2.6f, 1.8f, 0.7f };
    const std::vector<int> stepValues { -3, -2, -1, 0, 1, 2, 3 };

    // When there is a corpus, the motif walks the way your own lines walk.
    const bool learnedSteps = ctx.style() != nullptr && ! ctx.style()->stepMarginal.empty()
                            && ctx.rng.chance(std::clamp(options.styleAmount, 0.0f, 1.0f));

    const auto makeMotif = [&](int length)
    {
        std::vector<int> motif;
        motif.reserve(static_cast<size_t>(length));
        int degree = 0;
        int previousStep = 0;

        for (int i = 0; i < length; ++i)
        {
            motif.push_back(degree);

            int step = 0;
            if (learnedSteps)
            {
                step = sampleStep(*ctx.style(), previousStep, ctx.rng, 1);
            }
            else
            {
                step = stepValues[static_cast<size_t>(ctx.rng.weighted(stepWeights))];
                if (ctx.rng.chance(0.12f + 0.25f * options.complexity))
                    step += ctx.rng.chance(0.5f) ? 2 : -2; // occasional leap
            }

            previousStep = step;
            degree = std::clamp(degree + step, -7, 8);
        }
        return motif;
    };

    // A motif that barely moves reads as a flat line whatever happens later,
    // so a cramped draw gets one more roll with wider steps.
    const auto motifRange = [](const std::vector<int>& motif)
    {
        const auto [lo, hi] = std::minmax_element(motif.begin(), motif.end());
        return *hi - *lo;
    };

    auto motifA = makeMotif(motifLength);
    if (motifRange(motifA) < 3)
        motifA = makeMotif(motifLength);
    auto motifB = makeMotif(motifLength);
    if (motifRange(motifB) < 3)
        motifB = makeMotif(motifLength);

    // The arc of the phrase: hold, lift, peak, settle. This is what keeps four
    // bars from sitting at one height even when the motif repeats exactly.
    const int peakLift = 2 + (ctx.rng.chance(0.4f) ? 1 : 0);
    const int phraseArc[4] = { 0, 1, peakLift, ctx.rng.chance(0.5f) ? -1 : 0 };

    // --- Motif development ---------------------------------------------------
    // Transposing the same shape four times is the thinnest thing a phrase can
    // do. These are the standard ways a motif is worked: turned upside down,
    // run backwards, or cut to its first half and repeated.
    const float development = std::clamp(options.motifDevelopment, 0.0f, 1.0f);

    const auto invert = [](const std::vector<int>& motif)
    {
        std::vector<int> out;
        out.reserve(motif.size());
        for (int degree : motif)
            out.push_back(motif.front() - (degree - motif.front()));
        return out;
    };

    const auto retrograde = [](const std::vector<int>& motif)
    {
        return std::vector<int>(motif.rbegin(), motif.rend());
    };

    const auto fragment = [](const std::vector<int>& motif)
    {
        if (motif.size() < 4)
            return motif;
        const std::vector<int> half(motif.begin(), motif.begin() + static_cast<long>(motif.size() / 2));
        std::vector<int> out = half;
        out.insert(out.end(), half.begin(), half.end());
        return out;
    };

    // One variant per bar of the phrase, drawn once so the phrase is coherent.
    std::vector<std::vector<int>> barMotifs { motifA, motifA, motifB, motifA };
    if (development > 0.05f)
    {
        // Bar 2 answers bar 1: the more development, the more it is a real
        // transformation rather than the same shape moved.
        if (ctx.rng.chance(development * 0.7f))
            barMotifs[1] = retrograde(motifA);

        // Bar 3 is the contrast, and inverting it against the opening is the
        // clearest way to make it sound answered rather than random.
        if (ctx.rng.chance(development * 0.8f))
            barMotifs[2] = invert(motifA);

        // Bar 4 drives home by fragmenting - the motif chases its own tail.
        if (ctx.rng.chance(development * 0.6f))
            barMotifs[3] = fragment(motifA);
    }

    // Where the source line is heading, so a counter melody can move against it.
    const auto sourcePitchAt = [&ctx](int64_t tick) -> int
    {
        int pitch = -1;
        int64_t bestStart = -1;
        for (const auto& note : ctx.source->notes)
        {
            if (note.startTick <= tick && tick < note.endTick() && note.startTick > bestStart)
            {
                bestStart = note.startTick;
                pitch = note.pitch;
            }
        }
        return pitch;
    };

    // What the companion part (usually the bass) is holding at a given tick.
    // An explicitly chained part, or - in the plugin, where you load a bass and
    // ask for a melody over it - the source clip itself.
    const NoteSequence* companion = ctx.options->companion;
    if (companion == nullptr && ctx.source != nullptr && ! ctx.source->empty()
        && ctx.analysis->role == SourceRole::Bass)
        companion = ctx.source;

    const auto soundingPitchAt = [](const NoteSequence* part, int64_t tick) -> int
    {
        if (part == nullptr)
            return -1;
        int pitch = -1;
        for (const auto& note : part->notes)
        {
            if (note.startTick > tick)
                break;
            if (tick < note.endTick())
                pitch = note.pitch;
        }
        return pitch;
    };

    const auto companionPitchAt = [&](int64_t tick) { return soundingPitchAt(companion, tick); };

    // The top voice of the pad. A melody that lands on it disappears into the
    // chord instead of sitting over it.
    const NoteSequence* anchor = ctx.options->anchor;
    const auto anchorTopAt = [anchor](int64_t tick) -> int
    {
        if (anchor == nullptr)
            return -1;
        int pitch = -1;
        for (const auto& note : anchor->notes)
        {
            if (note.startTick > tick)
                break;
            if (tick < note.endTick())
                pitch = std::max(pitch, note.pitch);
        }
        return pitch;
    };

    int previousPitch = -1;
    int repeatCount = 0;
    int previousSourcePitch = -1;

    for (size_t i = 0; i < onsets.size(); ++i)
    {
        const auto& onset = onsets[i];
        const int bar = static_cast<int>(onset.tick / ctx.barTicks);
        const int barInPhrase = bar % 4;
        const auto& segment = ctx.chordAt(onset.tick);

        const std::vector<int>& motif = barMotifs[static_cast<size_t>(barInPhrase & 3)];
        const int transposition = phraseArc[barInPhrase & 3];

        int degree = motif[i % motif.size()] + transposition;
        int pitch = ctx.key.pitchForDegree(degree, octaveBase);

        const bool strong = onset.accent >= 0.7f || onset.length >= ctx.beatTicks;
        if (strong || ctx.rng.chance(0.35f + 0.4f * (1.0f - options.complexity)))
            pitch = nearestChordTone(pitch, segment.chord);
        else
            pitch = ctx.key.snapToScale(pitch, ctx.rng.chance(0.5f) ? 1 : -1);

        // Chromatic passing tone into the next strong note.
        if (! strong && options.complexity > 0.7f && ctx.rng.chance(0.18f))
            pitch += ctx.rng.chance(0.5f) ? 1 : -1;

        const int sourcePitch = counterMelody ? sourcePitchAt(onset.tick) : -1;
        if (counterMelody)
        {
            if (sourcePitch >= 0)
            {
                if (previousSourcePitch >= 0 && previousPitch >= 0)
                {
                    // Contrary motion: if the source rises, lean downward.
                    const int sourceDirection = sourcePitch - previousSourcePitch;
                    const int ourDirection = pitch - previousPitch;
                    if (sourceDirection != 0 && ourDirection != 0
                        && ((sourceDirection > 0) == (ourDirection > 0))
                        && ctx.rng.chance(0.65f))
                    {
                        pitch = nearestChordTone(previousPitch - (sourceDirection > 0 ? 2 : -2), segment.chord);
                    }
                }
                if (mod12(pitch) == mod12(sourcePitch))
                    pitch = nearestChordTone(pitch + 3, segment.chord);
                previousSourcePitch = sourcePitch;
            }
        }

        pitch = clampPitch(pitch, ctx.lowPitch, ctx.highPitch);

        // Keep leaps singable - after clamping, since folding a note back into
        // the register is itself an octave jump. The phrase arc pushes notes
        // out of range on purpose, so this has to be the last word on interval.
        if (previousPitch >= 0 && std::abs(pitch - previousPitch) > 12)
            pitch = clampPitch(previousPitch + (pitch > previousPitch ? 12 : -12),
                               ctx.lowPitch, ctx.highPitch);

        if (pitch == previousPitch)
        {
            if (++repeatCount >= 2)
            {
                pitch = ctx.key.snapToScale(pitch + (ctx.rng.chance(0.5f) ? 2 : -2), 0);
                pitch = clampPitch(pitch, ctx.lowPitch, ctx.highPitch);
                repeatCount = 0;
            }
        }
        else
        {
            repeatCount = 0;
        }

        // A melody a semitone off the bass note is a minor ninth - the one
        // interval that reads as a mistake in this genre. On strong positions,
        // step off it onto a chord tone.
        if (onset.accent >= 0.7f)
        {
            const int bassPitch = companionPitchAt(onset.tick);
            if (bassPitch >= 0 && mod12(pitch - bassPitch) == 1)
            {
                pitch = clampPitch(nearestChordTone(pitch + 1, segment.chord), ctx.lowPitch, ctx.highPitch);
                if (previousPitch >= 0 && std::abs(pitch - previousPitch) > 12)
                    pitch = clampPitch(previousPitch + (pitch > previousPitch ? 12 : -12),
                                       ctx.lowPitch, ctx.highPitch);
            }
        }

        // Last word on collisions: after leap limiting and repeat breaking the
        // line can land back on the source note, so check once more at the end.
        if (counterMelody && sourcePitch >= 0)
        {
            for (int attempt = 0; attempt < 3 && pitch == sourcePitch; ++attempt)
            {
                pitch = clampPitch(nearestChordTone(pitch - 3, segment.chord), ctx.lowPitch, ctx.highPitch);
                if (pitch == sourcePitch)
                    pitch = clampPitch(ctx.key.snapToScale(pitch - 2, -1), ctx.lowPitch, ctx.highPitch);
            }
        }

        // Doubling the pad's top voice on a strong beat buries the line inside
        // the chord, and a semitone off it is the same minor ninth the bass
        // check guards against. Last of all, so nothing above can undo it.
        if (onset.accent >= 0.7f)
        {
            const int top = anchorTopAt(onset.tick);
            for (int attempt = 0; attempt < 3 && top >= 0
                                 && (pitch == top || mod12(pitch - top) == 1); ++attempt)
            {
                const int stepped = clampPitch(nearestChordTone(pitch + 2 + attempt, segment.chord),
                                               ctx.lowPitch, ctx.highPitch);
                pitch = (stepped == pitch) ? clampPitch(ctx.key.snapToScale(pitch + 2, 1),
                                                        ctx.lowPitch, ctx.highPitch)
                                           : stepped;
            }
        }

        Note note;
        note.startTick = onset.tick;
        note.lengthTick = onset.length;
        note.pitch = pitch;
        note.velocity = velocityFor(onset, options.baseVelocity, 0.34f);
        note.channel = options.channel;
        out.notes.push_back(note);
        previousPitch = pitch;
    }

    // --- Passing and neighbour notes ------------------------------------------
    // A line that only lands on the beat sounds sketched. Filling a third with
    // the step between it, and turning a repeat into a neighbour, is what makes
    // a melody sound written rather than outlined.
    if (development > 0.25f && out.notes.size() >= 2)
    {
        std::vector<Note> ornamented;
        ornamented.reserve(out.notes.size() * 3 / 2);

        for (size_t i = 0; i < out.notes.size(); ++i)
        {
            ornamented.push_back(out.notes[i]);
            if (i + 1 >= out.notes.size())
                continue;

            const auto& current = out.notes[i];
            const auto& next = out.notes[i + 1];
            const int64_t gap = next.startTick - current.startTick;
            const int interval = next.pitch - current.pitch;

            // Only where there is room for another note without crowding.
            if (gap < ctx.slotTicks * 2)
                continue;
            if (! ctx.rng.chance(development * 0.55f))
                continue;

            int passing = -1;
            if (std::abs(interval) == 3 || std::abs(interval) == 4)
            {
                // A third: step through the note in between.
                passing = ctx.key.snapToScale(current.pitch + interval / 2, interval > 0 ? 1 : -1);
            }
            else if (interval == 0)
            {
                // A repeat: lean off it and come back.
                passing = ctx.key.snapToScale(current.pitch + (ctx.rng.chance(0.5f) ? 2 : -1), 0);
            }

            if (passing < 0 || passing == current.pitch || passing == next.pitch)
                continue;

            Note fill;
            // Snap to the 16th grid: an ornament off the grid sounds like a
            // timing error rather than a decision.
            fill.startTick = ((current.startTick + gap / 2) / ctx.slotTicks) * ctx.slotTicks;
            if (fill.startTick <= current.startTick || fill.startTick >= next.startTick)
                continue;
            fill.lengthTick = std::max<int64_t>(ctx.slotTicks / 2, gap / 2);
            fill.pitch = clampPitch(passing, ctx.lowPitch, ctx.highPitch);
            fill.velocity = std::max(1, current.velocity - 14); // lighter than the notes it joins
            fill.channel = options.channel;

            // The note it steps off can no longer run into it.
            ornamented.back().lengthTick = std::min(ornamented.back().lengthTick,
                                                    fill.startTick - current.startTick);
            ornamented.push_back(fill);
        }

        out.notes = std::move(ornamented);
    }

    if (options.cadenceAtEnd && ! out.notes.empty())
    {
        auto& last = out.notes.back();
        const auto& segment = ctx.chordAt(last.startTick);
        const int target = ctx.rng.chance(0.6f) ? segment.chord.root
                                                : mod12(segment.chord.root
                                                        + (segment.chord.containsPitchClass(mod12(segment.chord.root + 3)) ? 3 : 4));
        last.pitch = clampPitch(nearestPitchWithClass(target, last.pitch), ctx.lowPitch, ctx.highPitch);

        // The cadence rewrites the final pitch after every other guard has run,
        // so it has to honour the leap limit itself or the phrase can end on a
        // jump no singer would make.
        if (out.notes.size() >= 2)
        {
            const int approach = out.notes[out.notes.size() - 2].pitch;
            if (std::abs(last.pitch - approach) > 12)
                last.pitch = clampPitch(approach + (last.pitch > approach ? 12 : -12),
                                        ctx.lowPitch, ctx.highPitch);
        }

        if (counterMelody)
        {
            const int sourcePitch = sourcePitchAt(last.startTick);
            if (last.pitch == sourcePitch)
            {
                const int lower = last.pitch - 12;
                last.pitch = lower >= ctx.lowPitch ? lower
                                                   : clampPitch(last.pitch + 12, ctx.lowPitch, ctx.highPitch);
            }
        }

        last.lengthTick = std::max(last.lengthTick, ctx.totalTicks - last.startTick);
    }

    return out;
}

} // namespace

// ---------------------------------------------------------------------------

NoteSequence generate(const Analysis& analysis, const NoteSequence& source, const GenerateOptions& options)
{
    NoteSequence out;
    out.ppq = analysis.ppq;
    out.tempos = { { analysis.bpm, 0 } };
    out.timeSignatures = { analysis.timeSignature };
    out.name = toString(options.part);

    if (! analysis.valid || analysis.progression.empty())
        return out;

    Context ctx;
    ctx.analysis = &analysis;
    ctx.source = &source;
    ctx.options = &options;
    ctx.rng.reseed(options.seed);
    ctx.ppq = analysis.ppq;
    ctx.barTicks = std::max<int64_t>(1, analysis.ticksPerBar());
    ctx.beatTicks = std::max<int64_t>(1, analysis.ticksPerBeat());
    ctx.slotTicks = std::max<int64_t>(1, ctx.barTicks / 16);
    ctx.key = analysis.key;
    if (options.useGrooveDonor)
    {
        ctx.rhythm = options.grooveDonor;
    }
    else if (options.companion != nullptr && ! options.companion->empty())
    {
        // A melody written over a bass should share its syncopation - that
        // lock is most of what makes two parts sound like one track.
        ctx.rhythm = buildRhythmProfile(*options.companion, analysis.timeSignature, analysis.ppq);
    }
    else
    {
        ctx.rhythm = analysis.rhythm;
    }

    ctx.bars = options.bars > 0 ? options.bars : std::max(1, analysis.bars);
    ctx.totalTicks = static_cast<int64_t>(ctx.bars) * ctx.barTicks;

    // Fold the anchor's note starts onto one bar, so every bar of the new part
    // knows where the pad moves.
    if (options.anchor != nullptr && ! options.anchor->empty())
    {
        const int64_t slotTicks = std::max<int64_t>(1, ctx.barTicks / 16);
        float peak = 0.0f;
        for (const auto& note : options.anchor->notes)
        {
            const int slot = static_cast<int>((note.startTick % ctx.barTicks) / slotTicks) % 16;
            ctx.anchorSlots[static_cast<size_t>(slot)] += 1.0f;
            peak = std::max(peak, ctx.anchorSlots[static_cast<size_t>(slot)]);
        }
        if (peak > 0.0f)
        {
            for (auto& value : ctx.anchorSlots)
                value /= peak;
            ctx.hasAnchor = true;
        }
    }

    // Loop the analysed progression until the requested length is filled.
    const int64_t sourceLoop = std::max<int64_t>(ctx.barTicks, analysis.lengthTicks);
    for (int64_t offset = 0; offset < ctx.totalTicks; offset += sourceLoop)
    {
        for (const auto& segment : analysis.progression)
        {
            ChordSegment copy = segment;
            copy.startTick += offset;
            if (copy.startTick >= ctx.totalTicks)
                break;
            copy.lengthTick = std::min(copy.lengthTick, ctx.totalTicks - copy.startTick);
            ctx.progression.push_back(copy);
        }
    }
    if (ctx.progression.empty())
        return out;

    defaultRange(options.part, ctx.lowPitch, ctx.highPitch);
    if (options.lowPitch >= 0)
        ctx.lowPitch = options.lowPitch;
    if (options.highPitch >= 0)
        ctx.highPitch = options.highPitch;
    adjustRangeForSource(ctx, options.part);

    const int shift = options.octaveShift * 12;
    ctx.lowPitch = std::clamp(ctx.lowPitch + shift, 0, 115);
    ctx.highPitch = std::clamp(ctx.highPitch + shift, ctx.lowPitch + 12, 127);

    switch (options.part)
    {
        case PartType::Pad:           out.notes = writePad(ctx).notes; break;
        case PartType::Chords:        out.notes = writeChords(ctx).notes; break;
        case PartType::Bass:          out.notes = writeBass(ctx).notes; break;
        case PartType::Arp:           out.notes = writeArp(ctx).notes; break;
        case PartType::Pluck:         out.notes = writePluck(ctx).notes; break;
        case PartType::Melody:        out.notes = writeMelodicLine(ctx, false).notes; break;
        case PartType::CounterMelody: out.notes = writeMelodicLine(ctx, true).notes; break;
    }

    // A pad that drifts off the bar line just sounds late, so it only gets the
    // velocity half of the humanisation.
    humanise(ctx, out, options.part != PartType::Pad);

    for (auto& note : out.notes)
    {
        note.pitch = std::clamp(note.pitch, 0, 127);
        note.lengthTick = std::max<int64_t>(ctx.slotTicks / 4, note.lengthTick);
        note.channel = options.channel;
    }

    // The part fills exactly the loop it was asked for. Anything ringing over
    // the final bar line is cut there rather than left to stretch the clip the
    // host imports, and the length is recorded so a part that stops early still
    // writes a file as long as the loop.
    out.loopLengthTicks = ctx.totalTicks;
    out.trimToLoop();

    out.sort();
    return out;
}

std::vector<NoteSequence> generateVariations(const Analysis& analysis,
                                             const NoteSequence& source,
                                             const GenerateOptions& options,
                                             int count)
{
    std::vector<NoteSequence> out;
    out.reserve(static_cast<size_t>(std::max(0, count)));
    for (int i = 0; i < count; ++i)
    {
        GenerateOptions variation = options;
        variation.seed = options.seed + static_cast<uint32_t>(i) * 2654435761u + 1u;
        out.push_back(generate(analysis, source, variation));
    }
    return out;
}

std::vector<ChordSegment> reharmonize(const Analysis& analysis, uint32_t seed, float amount)
{
    std::vector<ChordSegment> out = analysis.progression;
    if (out.empty())
        return out;

    Rng rng(seed);
    const Key& key = analysis.key;
    const auto triads = diatonicTriads(key);
    const auto sevenths = diatonicSevenths(key);
    if (triads.size() < 7)
        return out;

    for (size_t i = 0; i < out.size(); ++i)
    {
        if (! rng.chance(std::clamp(amount, 0.0f, 1.0f)))
            continue;

        Chord& chord = out[i].chord;
        const int degree = key.degreeOf(chord.root);
        const bool hasNext = (i + 1) < out.size();

        std::vector<int> moves { 0, 1, 2, 3, 4 };
        switch (moves[static_cast<size_t>(rng.range(0, static_cast<int>(moves.size()) - 1))])
        {
            case 0: // relative substitution: I<->vi, IV<->ii, V<->iii
            {
                if (degree == 0)      chord = triads[5];
                else if (degree == 3) chord = triads[1];
                else if (degree == 4) chord = triads[2];
                else if (degree == 5) chord = triads[0];
                else if (degree == 1) chord = triads[3];
                break;
            }
            case 1: // secondary dominant of whatever comes next
            {
                if (hasNext)
                {
                    const int targetRoot = out[i + 1].chord.root;
                    chord = { mod12(targetRoot + 7), ChordType::Dominant7, -1 };
                }
                break;
            }
            case 2: // tritone substitution for a dominant
            {
                if (chord.type == ChordType::Dominant7 || chord.type == ChordType::Dominant9)
                    chord = { mod12(chord.root + 6), ChordType::Dominant7, -1 };
                else if (degree == 4)
                    chord = { mod12(chord.root + 6), ChordType::Dominant7, -1 };
                break;
            }
            case 3: // modal interchange
            {
                const int pick = rng.range(0, 2);
                const int borrowed = pick == 0 ? mod12(key.tonic + 10)   // bVII
                                   : pick == 1 ? mod12(key.tonic + 8)    // bVI
                                               : mod12(key.tonic + 5);   // iv
                chord = { borrowed, pick == 2 ? ChordType::Minor : ChordType::Major, -1 };
                break;
            }
            case 4: // richer voicing of the same function
            {
                if (degree >= 0 && degree < static_cast<int>(sevenths.size()))
                    chord = sevenths[static_cast<size_t>(degree)];
                break;
            }
            default:
                break;
        }
    }
    return out;
}

} // namespace harmonia
