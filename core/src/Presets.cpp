#include "harmonia/Presets.h"

#include <algorithm>

namespace harmonia
{

const std::vector<ProgressionPreset>& progressionPresets()
{
    static const std::vector<ProgressionPreset> presets {
        // ---- House -----------------------------------------------------------
        { "house-anthem", "Anthem loop", "House", "VI | VII | i | III", ScaleType::NaturalMinor,
          "Starts on the relative major and lifts into the tonic. Big and obvious." },
        { "house-classic", "Classic vamp", "House", "i | VI | III | VII", ScaleType::NaturalMinor,
          "The four chords behind half the genre." },
        { "house-organ", "Organ stab", "House", "i | iv | VII | III", ScaleType::NaturalMinor,
          "Sits well under short organ or piano stabs." },
        { "house-pump", "Two-chord pump", "House", "i7 | IV7", ScaleType::Dorian,
          "Dorian two-chord loop - the raised sixth is the whole point." },
        { "house-disco", "Disco turnaround", "House", "Imaj7 | vi7 | ii7 | V7", ScaleType::Major,
          "Filtered-disco loop, needs seventh chords to sound right." },

        // ---- Deep House --------------------------------------------------------
        { "deep-warm", "Warm sevenths", "Deep House", "i7 | VImaj7 | VII7 | v7", ScaleType::NaturalMinor,
          "Pads with sevenths, slow harmonic rhythm, lots of space." },
        { "deep-rhodes", "Rhodes vamp", "Deep House", "i9 | IV9", ScaleType::Dorian,
          "Two chords, ninths on both, Rhodes or electric piano." },
        { "deep-night", "Night loop", "Deep House", "i7 | iv7 | VII7 | IIImaj7", ScaleType::NaturalMinor,
          "Darker four-bar loop, good for late-night sets." },
        { "deep-soul", "Soulful turnaround", "Deep House", "Imaj7 | iii7 | vi7 | ii7", ScaleType::Major,
          "Descending soulful cycle; put the bass on the roots." },
        { "deep-ii-v", "ii-V loop", "Deep House", "ii7 | V7 | Imaj7 | vi7", ScaleType::Major,
          "Jazz turnaround at house tempo." },

        // ---- Melodic House -----------------------------------------------------
        { "melodic-lift", "Melodic lift", "Melodic House", "i | III | VII | VI", ScaleType::NaturalMinor,
          "The Anjuna-style four - lands well under long arpeggios." },
        { "melodic-drive", "Driving minor", "Melodic House", "i | VII | VI | VII", ScaleType::NaturalMinor,
          "Keeps moving without resolving; good under a rolling bass." },
        { "melodic-air", "Suspended air", "Melodic House", "isus2 | VIsus2 | IIIsus2 | VIIsus2", ScaleType::NaturalMinor,
          "No thirds anywhere, so the lead decides the mood." },
        { "melodic-emotive", "Emotive four", "Melodic House", "VI | VII | i | v", ScaleType::NaturalMinor,
          "Starts away from home and falls back into it." },
        { "melodic-progressive", "Progressive build", "Melodic House", "i | VI | iv | VII", ScaleType::NaturalMinor,
          "Long-form build; each chord can hold two bars." },

        // ---- Afro / Organic ----------------------------------------------------
        { "afro-cycle", "Afro cycle", "Afro House", "i | VII | VI | v", ScaleType::NaturalMinor,
          "Descending cycle over a percussive groove." },
        { "afro-dorian", "Dorian groove", "Afro House", "i7 | IV | i7 | VII", ScaleType::Dorian,
          "Modal and repetitive - let the percussion carry it." },

        // ---- Progressive House -------------------------------------------------
        { "prog-plateau", "Long plateau", "Progressive House", "i | VII | III | VI", ScaleType::NaturalMinor,
          "Rotates without ever cadencing. Give each chord two bars and let it ride." },
        { "prog-suspended", "Suspended build", "Progressive House", "isus4 | i | VIIsus4 | VII", ScaleType::NaturalMinor,
          "Each suspension resolves late; that delay is where the lift comes from." },
        { "prog-lydian", "Lydian float", "Progressive House", "Imaj7 | II7 | Imaj7 | vi7", ScaleType::Lydian,
          "The raised fourth keeps it weightless - good under a long, slow arpeggio." },

        // ---- Melodic Techno ------------------------------------------------------
        { "mtechno-two", "Two-chord dark", "Melodic Techno", "i | VI", ScaleType::NaturalMinor,
          "Two chords, eight bars each. The tension is in the arrangement, not the harmony." },
        { "mtechno-phrygian", "Phrygian pull", "Melodic Techno", "i | II", ScaleType::Phrygian,
          "The flat second is the whole sound. Keep the bass nailed to the tonic." },
        { "mtechno-held", "Held suspension", "Melodic Techno", "isus2 | isus4", ScaleType::NaturalMinor,
          "One root, two colours - no third anywhere, so nothing commits." },

        // ---- Techno --------------------------------------------------------------
        { "techno-roll", "Rolling minor", "Techno", "i | VII | i | VI", ScaleType::NaturalMinor,
          "Comes home every other bar, so it hypnotises instead of going anywhere." },
        { "techno-stab", "Minor stab", "Techno", "i | iv", ScaleType::NaturalMinor,
          "Two stabs, no resolution. Short notes, hard gate." },

        // ---- Trance ---------------------------------------------------------------
        { "trance-uplift", "Uplifting four", "Trance", "vi | IV | I | V", ScaleType::Major,
          "Minor start, major landing - the hands-in-the-air shape." },
        { "trance-emotional", "Emotional roll", "Trance", "VI | VII | III | i", ScaleType::NaturalMinor,
          "Three steps up and then home. Works with the chords rolling in eighths." },

        // ---- Drum & Bass -----------------------------------------------------------
        { "dnb-liquid", "Liquid sevenths", "Drum & Bass", "i7 | VImaj7 | IIImaj7 | VII7", ScaleType::NaturalMinor,
          "The diatonic minor sevenths in a row - warm without going jazzy." },
        { "dnb-jazzy", "Minor two-five", "Drum & Bass", "iim7b5 | V7 | i7 | VImaj7", ScaleType::HarmonicMinor,
          "The minor turnaround liquid leans on; the V wants a raised third." },

        // ---- UK Garage --------------------------------------------------------------
        { "garage-two", "Two-step", "UK Garage", "i9 | IV7", ScaleType::Dorian,
          "Ninth on the tonic, dominant on the four. Swing the sixteenths." },
        { "garage-soul", "Soulful garage", "UK Garage", "Imaj7 | vi9 | ii9 | V7", ScaleType::Major,
          "Chopped vocal harmony; the ninths keep it from sounding like a jazz standard." },

        // ---- Downtempo ----------------------------------------------------------------
        { "ambient-drift", "Slow drift", "Downtempo", "Imaj7 | IVmaj7", ScaleType::Lydian,
          "Two major sevenths a fourth apart. Four bars each, no hurry." },
        { "ambient-open", "Open suspension", "Downtempo", "isus2 | VIIsus2", ScaleType::NaturalMinor,
          "No thirds, so it reads as texture rather than harmony." },

        // ---- Classics ----------------------------------------------------------
        { "pop-four", "Four chords", "Classic", "I | V | vi | IV", ScaleType::Major,
          "The one everybody knows." },
        { "andalusian", "Andalusian cadence", "Classic", "i | VII | VI | V", ScaleType::NaturalMinor,
          "Ends on a major V, which pulls hard back to the tonic." },
        { "minor-cycle", "Minor cycle", "Classic", "i | iv | v | i", ScaleType::NaturalMinor,
          "Plainest minor cadence there is; a good blank canvas." },
    };
    return presets;
}

const ProgressionPreset* findProgressionPreset(const std::string& id)
{
    const auto& presets = progressionPresets();
    const auto it = std::find_if(presets.begin(), presets.end(),
                                 [&id](const ProgressionPreset& preset) { return preset.id == id; });
    return it != presets.end() ? &*it : nullptr;
}

std::vector<std::string> progressionStyles()
{
    std::vector<std::string> styles;
    for (const auto& preset : progressionPresets())
        if (std::find(styles.begin(), styles.end(), preset.style) == styles.end())
            styles.push_back(preset.style);
    return styles;
}

} // namespace harmonia
