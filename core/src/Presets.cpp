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
