#pragma once

#include "harmonia/Analysis.h"
#include "harmonia/StyleModel.h"
#include "harmonia/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace harmonia
{

/** The kind of part the engine should write over the analysed harmony. */
enum class PartType
{
    Pad,           // sustained chord voicings
    Chords,        // rhythmic stabs / comping
    Melody,        // singable lead line
    CounterMelody, // second line designed to sit against the source
    Bass,          // root-driven low line
    Reese,         // sustained low line, notes tied so a synth can glide between them
    Arp,           // broken chord pattern
    Pluck          // short chord tones on the groove, house-style
};

const char* toString(PartType part);
/** Parses "pad", "chords", "melody", "counter", "bass", "reese", "arp". Returns false if unknown. */
bool partTypeFromString(const std::string& text, PartType& out);

enum class ArpPattern
{
    Up,
    Down,
    UpDown,
    DownUp,
    Converge,
    Random
};

struct GenerateOptions
{
    PartType part = PartType::Pad;
    uint32_t seed = 1;

    int octaveShift = 0;      // whole octaves up/down from the part's default register
    float density = 0.5f;     // how busy the part is, 0..1
    float complexity = 0.5f;  // extensions, chromatic approaches, inversions, 0..1
    float humanize = 0.15f;   // timing/velocity jitter, 0..1
    /** How hard a melodic line works its motif: at 0 the phrase just repeats
        and transposes, at 1 it inverts, runs backwards, fragments and fills the
        gaps with passing notes. */
    float motifDevelopment = 0.4f;
    float swing = 0.0f;       // 0 = straight, 1 = full triplet feel on off-16ths

    /** Borrow the onset grid of the source clip instead of using the part default. */
    bool followSourceRhythm = true;
    /** Take the groove from another clip entirely - typically one picked out of
        your MIDI library. Overrides the source clip's groove when set. */
    bool useGrooveDonor = false;
    RhythmProfile grooveDonor {};

    /** A part generated just before this one - typically the bass under a
        melody. The new part borrows its groove and keeps out of its way
        harmonically. Borrowed pointer; must outlive the generate() call. */
    const NoteSequence* companion = nullptr;

    /** The part everything else is written around - in the plugin, the pad. The
        new part enters where the anchor moves and keeps out of the way of the
        voices it is holding. Deliberately not a groove donor: a pad holding one
        chord a bar says nothing useful about a melody's rhythm, so the anchor
        shapes where a line lands and which notes it avoids, never its feel.
        Borrowed pointer; must outlive the generate() call. */
    const NoteSequence* anchor = nullptr;

    /** Everything learned from your own collection: which bars you play, how
        your lines move, what you put over a root. Borrowed pointer - it only has
        to outlive the generate() call. */
    const StyleModel* style = nullptr;
    /** How often the learned material is used instead of the built-in feel. */
    float styleAmount = 1.0f;
    /** Keep the generated line out of the source's way (register + unisons). */
    bool avoidSourceCollisions = true;
    /** Resolve the final note/chord to something stable. */
    bool cadenceAtEnd = true;

    int maxVoices = 4;
    int lowPitch = -1;   // -1 = part default
    int highPitch = -1;  // -1 = part default
    int channel = 0;
    int baseVelocity = 96;

    ArpPattern arpPattern = ArpPattern::UpDown;
    /** Repeat the analysed progression until this many bars are filled. 0 = source length. */
    int bars = 0;
};

/** Writes a new part over the analysed harmony. */
NoteSequence generate(const Analysis& analysis, const NoteSequence& source, const GenerateOptions& options);

/** Convenience: `count` takes on the same settings, each with its own seed. */
std::vector<NoteSequence> generateVariations(const Analysis& analysis,
                                             const NoteSequence& source,
                                             const GenerateOptions& options,
                                             int count);

/** Suggests an alternative progression over the same bars (secondary dominants,
    relative substitutions, modal interchange, tritone subs). */
std::vector<ChordSegment> reharmonize(const Analysis& analysis, uint32_t seed, float amount);

/** Default register for a part, used when lowPitch/highPitch are left at -1. */
void defaultRange(PartType part, int& lowPitch, int& highPitch);

} // namespace harmonia
