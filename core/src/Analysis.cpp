#include "harmonia/Analysis.h"

#include <algorithm>
#include <cmath>
#include <cctype>
#include <numeric>

namespace harmonia
{

namespace
{

// Temperley's revised key profiles. They behave far better than the original
// Krumhansl-Kessler weights on short loops, which is what people drop in here.
const float kMajorProfile[12] = { 5.0f, 2.0f, 3.5f, 2.0f, 4.5f, 4.0f,
                                  2.0f, 4.5f, 2.0f, 3.5f, 1.5f, 4.0f };
// The minor profile is nudged towards natural minor: the classical weights
// expect a leading tone that pop, rock and electronic material rarely has.
const float kMinorProfile[12] = { 5.0f, 2.0f, 3.5f, 4.5f, 2.0f, 4.0f,
                                  2.0f, 4.5f, 3.5f, 2.0f, 4.0f, 2.5f };

/** Bonus applied to a tonic candidate that opens the clip. */
constexpr float kFirstBassBonus = 0.14f;
/** ...and a smaller one for the note the clip lands on. */
constexpr float kLastBassBonus = 0.06f;

float correlation(const std::array<float, 12>& x, const float* y)
{
    float meanX = 0.0f, meanY = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        meanX += x[i];
        meanY += y[i];
    }
    meanX /= 12.0f;
    meanY /= 12.0f;

    float num = 0.0f, denX = 0.0f, denY = 0.0f;
    for (int i = 0; i < 12; ++i)
    {
        const float dx = x[i] - meanX;
        const float dy = y[i] - meanY;
        num += dx * dy;
        denX += dx * dx;
        denY += dy * dy;
    }
    const float den = std::sqrt(denX * denY);
    return den > 1e-9f ? num / den : 0.0f;
}

struct Candidate
{
    Chord chord;
    std::vector<int> pcs;
    int third = -1;
    int seventh = -1;
    float sizeCost = 0.0f;
    bool diatonic = false;
};

std::vector<Candidate> buildCandidates(const Key& key, bool allowSevenths)
{
    std::vector<ChordType> types {
        ChordType::Major, ChordType::Minor, ChordType::Diminished,
        ChordType::Augmented, ChordType::Sus4, ChordType::Sus2
    };
    if (allowSevenths)
    {
        types.insert(types.end(), { ChordType::Major7, ChordType::Minor7,
                                    ChordType::Dominant7, ChordType::HalfDiminished7 });
    }

    const auto keyPcs = key.pitchClasses();
    const auto inKey = [&keyPcs](int pc)
    {
        return std::find(keyPcs.begin(), keyPcs.end(), mod12(pc)) != keyPcs.end();
    };

    std::vector<Candidate> out;
    out.reserve(12 * types.size());
    for (int root = 0; root < 12; ++root)
    {
        for (ChordType type : types)
        {
            Candidate c;
            c.chord = { root, type, -1 };
            c.pcs = c.chord.pitchClasses();
            const auto& intervals = chordIntervals(type);
            for (int interval : intervals)
            {
                if (interval == 3 || interval == 4)
                    c.third = mod12(root + interval);
                if (interval == 10 || interval == 11)
                    c.seventh = mod12(root + interval);
            }
            // Bigger chords soak up more of the frame, so they need to earn it,
            // and colour chords should not win a close call against a plain triad.
            c.sizeCost = 0.07f * static_cast<float>(std::max<size_t>(3, c.pcs.size()) - 3);
            switch (type)
            {
                case ChordType::Sus2:            c.sizeCost += 0.13f; break;
                case ChordType::Sus4:            c.sizeCost += 0.11f; break;
                case ChordType::Augmented:       c.sizeCost += 0.18f; break;
                case ChordType::Diminished:      c.sizeCost += 0.07f; break;
                case ChordType::HalfDiminished7: c.sizeCost += 0.07f; break;
                default: break;
            }
            c.diatonic = std::all_of(c.pcs.begin(), c.pcs.end(), inKey);
            out.push_back(std::move(c));
        }
    }
    return out;
}

} // namespace

const char* toString(SourceRole role)
{
    switch (role)
    {
        case SourceRole::Bass:   return "Bass";
        case SourceRole::Chords: return "Chords / Pad";
        case SourceRole::Lead:   return "Lead / Melody";
        case SourceRole::Arp:    return "Arp";
        case SourceRole::Unknown:
        default:                 return "Unknown";
    }
}

bool sourceRoleFromString(const std::string& text, SourceRole& out)
{
    std::string lowered;
    for (char c : text)
        lowered += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    if (lowered == "bass")                                        { out = SourceRole::Bass; return true; }
    if (lowered == "chords" || lowered == "chords / pad"
        || lowered == "pad" || lowered == "keys")                 { out = SourceRole::Chords; return true; }
    if (lowered == "lead" || lowered == "melody"
        || lowered == "lead / melody")                            { out = SourceRole::Lead; return true; }
    if (lowered == "arp")                                         { out = SourceRole::Arp; return true; }
    if (lowered == "unknown")                                     { out = SourceRole::Unknown; return true; }
    return false;
}

std::vector<int> RhythmProfile::strongestSlots(int count) const
{
    std::vector<int> indices(16);
    std::iota(indices.begin(), indices.end(), 0);
    std::stable_sort(indices.begin(), indices.end(),
                     [this](int a, int b) { return grid[a] > grid[b]; });
    if (count < static_cast<int>(indices.size()))
        indices.resize(std::max(0, count));
    return indices;
}

const ChordSegment* Analysis::chordAt(int64_t tick) const
{
    for (const auto& segment : progression)
        if (tick >= segment.startTick && tick < segment.endTick())
            return &segment;
    return progression.empty() ? nullptr : &progression.back();
}

std::string Analysis::progressionString() const
{
    return progressionString(key.preferFlats());
}

std::string Analysis::progressionString(bool useFlats) const
{
    std::string out;
    for (size_t i = 0; i < progression.size(); ++i)
    {
        if (i > 0)
            out += " | ";
        out += progression[i].chord.name(useFlats);
    }
    return out;
}

std::string Analysis::romanNumeralString() const
{
    std::string out;
    for (size_t i = 0; i < progression.size(); ++i)
    {
        if (i > 0)
            out += " | ";
        out += progression[i].chord.romanNumeral(key);
    }
    return out;
}

std::array<float, 12> pitchClassHistogram(const NoteSequence& sequence)
{
    std::array<float, 12> histogram { };
    for (const auto& n : sequence.notes)
    {
        const float weight = static_cast<float>(n.lengthTick) / static_cast<float>(std::max(1, sequence.ppq))
                           * (0.4f + 0.6f * static_cast<float>(n.velocity) / 127.0f);
        histogram[mod12(n.pitch)] += weight;
    }

    const float peak = *std::max_element(histogram.begin(), histogram.end());
    if (peak > 0.0f)
        for (auto& v : histogram)
            v /= peak;
    return histogram;
}

Key detectKey(const NoteSequence& sequence, float* confidenceOut)
{
    const auto histogram = pitchClassHistogram(sequence);

    // Which pitch class the clip starts and ends on. Profile matching alone
    // cannot tell a key from its relative major/minor; the bass at the edges of
    // the phrase usually can.
    int firstBass = -1;
    int lastBass = -1;
    if (! sequence.notes.empty())
    {
        int64_t firstTick = sequence.notes.front().startTick;
        int64_t lastTick = 0;
        for (const auto& n : sequence.notes)
        {
            firstTick = std::min(firstTick, n.startTick);
            lastTick = std::max(lastTick, n.endTick());
        }
        const int64_t window = std::max<int64_t>(1, sequence.ppq);
        int lowestFirst = 128;
        int lowestLast = 128;
        for (const auto& n : sequence.notes)
        {
            if (n.startTick < firstTick + window)
                lowestFirst = std::min(lowestFirst, n.pitch);
            if (n.endTick() > lastTick - window)
                lowestLast = std::min(lowestLast, n.pitch);
        }
        if (lowestFirst < 128)
            firstBass = mod12(lowestFirst);
        if (lowestLast < 128)
            lastBass = mod12(lowestLast);
    }

    Key best { 0, ScaleType::Major };
    float bestScore = -2.0f;
    float secondScore = -2.0f;

    for (int tonic = 0; tonic < 12; ++tonic)
    {
        std::array<float, 12> rotated { };
        for (int i = 0; i < 12; ++i)
            rotated[i] = histogram[mod12(tonic + i)];

        const float bonus = (tonic == firstBass ? kFirstBassBonus : 0.0f)
                          + (tonic == lastBass ? kLastBassBonus : 0.0f);
        const float majorScore = correlation(rotated, kMajorProfile) + bonus;
        const float minorScore = correlation(rotated, kMinorProfile) + bonus;

        for (const auto& [score, scale] : { std::pair { majorScore, ScaleType::Major },
                                            std::pair { minorScore, ScaleType::NaturalMinor } })
        {
            if (score > bestScore)
            {
                secondScore = bestScore;
                bestScore = score;
                best = { tonic, scale };
            }
            else if (score > secondScore)
            {
                secondScore = score;
            }
        }
    }

    // Modal refinement: the raw profiles only know major and natural minor, but
    // a lot of modern material is really Dorian or Mixolydian.
    const auto weightOf = [&histogram, &best](int semitonesAboveTonic)
    {
        return histogram[mod12(best.tonic + semitonesAboveTonic)];
    };

    const auto clearlyStronger = [](float candidate, float rival)
    {
        // The modal note has to actually be present, not just less absent than
        // the note it replaces.
        return candidate > 0.18f && candidate > rival * 1.8f + 0.08f;
    };

    if (best.scale == ScaleType::NaturalMinor)
    {
        if (clearlyStronger(weightOf(9), weightOf(8)))
            best.scale = ScaleType::Dorian;
        else if (clearlyStronger(weightOf(11), weightOf(10)))
            best.scale = ScaleType::HarmonicMinor;
    }
    else if (best.scale == ScaleType::Major)
    {
        if (clearlyStronger(weightOf(10), weightOf(11)))
            best.scale = ScaleType::Mixolydian;
        else if (clearlyStronger(weightOf(6), weightOf(5)))
            best.scale = ScaleType::Lydian;
    }

    if (confidenceOut != nullptr)
    {
        const float margin = std::clamp((bestScore - secondScore) * 4.0f, 0.0f, 1.0f);
        *confidenceOut = std::clamp(0.55f * std::max(0.0f, bestScore) + 0.45f * margin, 0.0f, 1.0f);
    }
    return best;
}

RhythmProfile buildRhythmProfile(const NoteSequence& sequence, const TimeSignature& ts, int ppq)
{
    RhythmProfile profile;
    if (sequence.notes.empty())
        return profile;

    const int64_t barTicks = std::max<int64_t>(1, harmonia::ticksPerBar(ts, ppq));
    const int64_t slotTicks = std::max<int64_t>(1, barTicks / 16);

    float totalWeight = 0.0f;
    float offGridWeight = 0.0f;
    double lengthSum = 0.0;
    double velocitySum = 0.0;

    profile.lowestPitch = 127;
    profile.highestPitch = 0;

    for (const auto& n : sequence.notes)
    {
        const int64_t posInBar = ((n.startTick % barTicks) + barTicks) % barTicks;
        const int slot = static_cast<int>((posInBar + slotTicks / 2) / slotTicks) % 16;
        const float weight = 0.4f + 0.6f * static_cast<float>(n.velocity) / 127.0f;

        profile.grid[static_cast<size_t>(slot)] += weight;
        totalWeight += weight;
        if ((slot % 4) != 0)
            offGridWeight += weight;

        lengthSum += static_cast<double>(n.lengthTick) / static_cast<double>(ppq);
        velocitySum += n.velocity;
        profile.lowestPitch = std::min(profile.lowestPitch, n.pitch);
        profile.highestPitch = std::max(profile.highestPitch, n.pitch);
    }

    const float peak = *std::max_element(profile.grid.begin(), profile.grid.end());
    if (peak > 0.0f)
        for (auto& v : profile.grid)
            v /= peak;

    const auto noteCount = static_cast<double>(sequence.notes.size());
    const int64_t totalTicks = std::max<int64_t>(barTicks, sequence.lengthTicks());
    const double barCount = std::max(1.0, static_cast<double>(totalTicks) / static_cast<double>(barTicks));

    profile.notesPerBar = static_cast<float>(noteCount / barCount);
    profile.averageLengthBeats = static_cast<float>(lengthSum / noteCount);
    profile.averageVelocity = static_cast<float>(velocitySum / noteCount);
    profile.syncopation = totalWeight > 0.0f ? offGridWeight / totalWeight : 0.0f;

    // Mean polyphony, sampled on a 16th grid. Walking a sorted event list keeps
    // this linear; scanning every note at every slot does not survive a clip
    // with thousands of notes.
    std::vector<std::pair<int64_t, int>> events;
    events.reserve(sequence.notes.size() * 2);
    for (const auto& n : sequence.notes)
    {
        events.emplace_back(n.startTick, 1);
        events.emplace_back(n.endTick(), -1);
    }
    std::sort(events.begin(), events.end());

    int64_t sounding = 0;
    int64_t occupied = 0;
    int active = 0;
    size_t nextEvent = 0;

    for (int64_t t = 0; t < totalTicks; t += slotTicks)
    {
        while (nextEvent < events.size() && events[nextEvent].first <= t)
            active += events[nextEvent++].second;

        if (active > 0)
        {
            ++occupied;
            sounding += active;
        }
    }

    profile.polyphony = occupied > 0 ? static_cast<float>(sounding) / static_cast<float>(occupied) : 1.0f;
    profile.monophonic = profile.polyphony < 1.2f;

    return profile;
}

SourceRole detectRole(const NoteSequence& sequence, const RhythmProfile& rhythm)
{
    if (sequence.notes.empty())
        return SourceRole::Unknown;

    double pitchSum = 0.0;
    for (const auto& n : sequence.notes)
        pitchSum += n.pitch;
    const double averagePitch = pitchSum / static_cast<double>(sequence.notes.size());

    if (! rhythm.monophonic)
        return SourceRole::Chords;

    if (averagePitch < 52.0 && rhythm.highestPitch < 64)
        return SourceRole::Bass;

    if (rhythm.notesPerBar >= 7.0f && rhythm.averageLengthBeats < 0.4f
        && (rhythm.highestPitch - rhythm.lowestPitch) >= 10)
        return SourceRole::Arp;

    return SourceRole::Lead;
}

Analysis analyze(const NoteSequence& sequence, const AnalysisOptions& options)
{
    Analysis result;
    result.ppq = sequence.ppq;
    result.timeSignature = sequence.timeSignatureAt(0);
    result.bpm = sequence.bpm();

    if (sequence.notes.empty())
    {
        result.message = "The clip has no notes to analyse.";
        return result;
    }

    const int64_t barTicks = std::max<int64_t>(1, result.ticksPerBar());
    const int64_t beatTicks = std::max<int64_t>(1, result.ticksPerBeat());

    result.lengthTicks = ((sequence.lengthTicks() + barTicks - 1) / barTicks) * barTicks;
    result.bars = static_cast<int>(result.lengthTicks / barTicks);

    result.key = options.forceKey ? options.key : detectKey(sequence, &result.keyConfidence);
    if (options.forceKey)
        result.keyConfidence = 1.0f;

    result.rhythm = buildRhythmProfile(sequence, result.timeSignature, sequence.ppq);
    result.role = detectRole(sequence, result.rhythm);

    // ---- Harmonic frames ---------------------------------------------------
    // A melodic line implies harmony far more loosely than a chord part, so it
    // gets a coarser grid and a stronger pull towards holding the same chord.
    const bool melodicSource = result.role == SourceRole::Lead;
    const int beatsPerBar = std::max(1, static_cast<int>(barTicks / beatTicks));
    const int framesPerBar = options.chordsPerBar > 0
                                 ? options.chordsPerBar
                                 : (melodicSource ? std::max(1, beatsPerBar / 2) : beatsPerBar);
    const int64_t frameTicks = std::max<int64_t>(1, barTicks / framesPerBar);
    const int frameCount = static_cast<int>(result.lengthTicks / frameTicks);
    if (frameCount <= 0)
    {
        result.message = "Clip is shorter than a single beat.";
        return result;
    }

    // How strong each frame's position in the bar is. A bass note on the
    // downbeat is a root; the same note on the last eighth is a passing tone.
    const auto metricWeight = [framesPerBar](int frame)
    {
        const int position = framesPerBar > 0 ? frame % framesPerBar : 0;
        if (position == 0)
            return 1.0f;
        if (framesPerBar >= 2 && position == framesPerBar / 2)
            return 0.7f;
        return 0.45f;
    };

    std::vector<std::array<float, 12>> frames(static_cast<size_t>(frameCount));
    std::vector<int> frameBass(static_cast<size_t>(frameCount), -1);
    std::vector<int> frameLowestPitch(static_cast<size_t>(frameCount), 128);

    for (const auto& n : sequence.notes)
    {
        const int first = static_cast<int>(n.startTick / frameTicks);
        const int last = static_cast<int>((n.endTick() - 1) / frameTicks);
        for (int f = std::max(0, first); f <= std::min(frameCount - 1, last); ++f)
        {
            const int64_t frameStart = static_cast<int64_t>(f) * frameTicks;
            const int64_t overlap = std::min(n.endTick(), frameStart + frameTicks)
                                  - std::max(n.startTick, frameStart);
            if (overlap <= 0)
                continue;

            const float weight = static_cast<float>(overlap) / static_cast<float>(frameTicks)
                               * (0.45f + 0.55f * static_cast<float>(n.velocity) / 127.0f)
                               * (f == first ? 1.25f : 1.0f); // onsets matter more than tails
            frames[static_cast<size_t>(f)][static_cast<size_t>(mod12(n.pitch))] += weight;

            if (n.pitch < frameLowestPitch[static_cast<size_t>(f)])
            {
                frameLowestPitch[static_cast<size_t>(f)] = n.pitch;
                frameBass[static_cast<size_t>(f)] = mod12(n.pitch);
            }
        }
    }

    // How much a frame is worth as evidence. One pitch class fits dozens of
    // chords equally well, so a single bass note must not be allowed to pull the
    // progression off course the way a full voicing can.
    std::vector<float> frameEvidence(static_cast<size_t>(frameCount), 1.0f);

    for (int f = 0; f < frameCount; ++f)
    {
        auto& frame = frames[static_cast<size_t>(f)];
        int distinct = 0;
        for (float value : frame)
            if (value > 0.0f)
                ++distinct;
        frameEvidence[static_cast<size_t>(f)] = std::clamp(0.35f + 0.35f * static_cast<float>(distinct - 1),
                                                           0.35f, 1.0f);

        const float sum = std::accumulate(frame.begin(), frame.end(), 0.0f);
        if (sum > 0.0f)
            for (auto& v : frame)
                v /= sum;
    }

    // ---- Viterbi over chord candidates -------------------------------------
    const auto candidates = buildCandidates(result.key, options.allowSevenths);
    const int stateCount = static_cast<int>(candidates.size());
    // How much the lowest note in a frame should be trusted as the chord root.
    const float bassBonus = result.role == SourceRole::Bass ? 0.45f
                          : (melodicSource || result.role == SourceRole::Arp) ? 0.12f
                                                                              : 0.25f;
    // A walking bass with passing notes still means one chord, so it gets a
    // stiffer penalty too - just not as stiff as a free melody.
    const float changePenaltyScale = melodicSource ? 1.7f
                                   : result.role == SourceRole::Bass ? 1.3f
                                                                     : 1.0f;

    std::vector<float> previousScores(static_cast<size_t>(stateCount), 0.0f);
    std::vector<float> currentScores(static_cast<size_t>(stateCount), 0.0f);
    std::vector<std::vector<int>> backPointers(static_cast<size_t>(frameCount),
                                               std::vector<int>(static_cast<size_t>(stateCount), 0));
    std::vector<std::vector<float>> emissions(static_cast<size_t>(frameCount),
                                              std::vector<float>(static_cast<size_t>(stateCount), 0.0f));

    for (int f = 0; f < frameCount; ++f)
    {
        const auto& frame = frames[static_cast<size_t>(f)];
        const bool silent = std::accumulate(frame.begin(), frame.end(), 0.0f) <= 0.0f;

        for (int s = 0; s < stateCount; ++s)
        {
            const auto& candidate = candidates[static_cast<size_t>(s)];
            float score = 0.0f;

            if (! silent)
            {
                for (int pc = 0; pc < 12; ++pc)
                {
                    const float w = frame[static_cast<size_t>(pc)];
                    if (w <= 0.0f)
                        continue;

                    if (std::find(candidate.pcs.begin(), candidate.pcs.end(), pc) != candidate.pcs.end())
                    {
                        float member = 1.0f;
                        if (pc == candidate.chord.root) member += 0.35f;
                        else if (pc == candidate.third) member += 0.18f;
                        else if (pc == candidate.seventh) member += 0.08f;
                        score += w * member;
                    }
                    else
                    {
                        score -= w * 0.85f;
                    }
                }

                const int bass = frameBass[static_cast<size_t>(f)];
                if (bass >= 0)
                {
                    const float strength = metricWeight(f);
                    if (bass == candidate.chord.root)
                        score += bassBonus * strength;
                    else if (candidate.chord.containsPitchClass(bass))
                        score -= 0.08f * strength;
                    else
                        score -= 0.22f * strength;
                }

                score -= candidate.sizeCost;
                if (candidate.diatonic)
                    score += options.diatonicBias;
            }

            emissions[static_cast<size_t>(f)][static_cast<size_t>(s)] = score * frameEvidence[static_cast<size_t>(f)];
        }
    }

    for (int f = 0; f < frameCount; ++f)
    {
        const bool onBarLine = (f % framesPerBar) == 0;
        const bool onHalfBar = (framesPerBar >= 2) && (f % std::max(1, framesPerBar / 2)) == 0;
        const float changeCost = options.chordChangePenalty * changePenaltyScale
                               * (onBarLine ? 0.45f : (onHalfBar ? 0.75f : 1.0f));

        // Switching costs the same from wherever you came, so the best
        // predecessor of any state is either that state itself or whichever
        // state led the previous frame. Finding that leader once per frame
        // turns an O(states^2) inner loop into O(states).
        int leader = 0;
        float leaderScore = -1e9f;
        if (f > 0)
        {
            for (int p = 0; p < stateCount; ++p)
            {
                if (previousScores[static_cast<size_t>(p)] > leaderScore)
                {
                    leaderScore = previousScores[static_cast<size_t>(p)];
                    leader = p;
                }
            }
        }

        for (int s = 0; s < stateCount; ++s)
        {
            float best = emissions[static_cast<size_t>(f)][static_cast<size_t>(s)];
            int bestFrom = s;

            if (f > 0)
            {
                const float stay = previousScores[static_cast<size_t>(s)];
                const float move = leaderScore - changeCost;

                if (move > stay || (move == stay && leader < s))
                {
                    best += move;
                    bestFrom = leader;
                }
                else
                {
                    best += stay;
                }
            }

            currentScores[static_cast<size_t>(s)] = best;
            backPointers[static_cast<size_t>(f)][static_cast<size_t>(s)] = bestFrom;
        }
        previousScores.swap(currentScores);
    }

    int state = static_cast<int>(std::distance(previousScores.begin(),
                                               std::max_element(previousScores.begin(), previousScores.end())));
    std::vector<int> path(static_cast<size_t>(frameCount), 0);
    for (int f = frameCount - 1; f >= 0; --f)
    {
        path[static_cast<size_t>(f)] = state;
        state = backPointers[static_cast<size_t>(f)][static_cast<size_t>(state)];
    }

    // ---- Frames to segments -------------------------------------------------
    for (int f = 0; f < frameCount; ++f)
    {
        const auto& candidate = candidates[static_cast<size_t>(path[static_cast<size_t>(f)])];
        const float confidence = std::clamp(emissions[static_cast<size_t>(f)][static_cast<size_t>(path[static_cast<size_t>(f)])], 0.0f, 1.0f);

        if (! result.progression.empty()
            && result.progression.back().chord.root == candidate.chord.root
            && result.progression.back().chord.type == candidate.chord.type)
        {
            auto& last = result.progression.back();
            const float weightedTotal = last.confidence * static_cast<float>(last.lengthTick)
                                      + confidence * static_cast<float>(frameTicks);
            last.lengthTick += frameTicks;
            last.confidence = weightedTotal / static_cast<float>(last.lengthTick);
        }
        else
        {
            ChordSegment segment;
            segment.startTick = static_cast<int64_t>(f) * frameTicks;
            segment.lengthTick = frameTicks;
            segment.chord = candidate.chord;
            segment.confidence = confidence;
            result.progression.push_back(segment);
        }
    }

    // Absorb single-frame blips sandwiched between two identical chords.
    for (size_t i = 1; i + 1 < result.progression.size();)
    {
        auto& previous = result.progression[i - 1];
        const auto& current = result.progression[i];
        const auto& next = result.progression[i + 1];

        if (current.lengthTick <= frameTicks
            && previous.chord.root == next.chord.root
            && previous.chord.type == next.chord.type)
        {
            previous.lengthTick += current.lengthTick + next.lengthTick;
            result.progression.erase(result.progression.begin() + static_cast<long>(i),
                                     result.progression.begin() + static_cast<long>(i) + 2);
            if (i > 1)
                --i;
        }
        else
        {
            ++i;
        }
    }

    result.valid = ! result.progression.empty();
    if (! result.valid)
        result.message = "Could not derive a progression from this clip.";
    return result;
}

} // namespace harmonia
