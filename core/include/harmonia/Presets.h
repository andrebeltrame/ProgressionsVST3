#pragma once

#include "harmonia/Theory.h"

#include <string>
#include <vector>

namespace harmonia
{

/** A progression you can drop in without typing anything. The chords are stored
    as roman numerals so a preset works in any key. */
struct ProgressionPreset
{
    std::string id;
    std::string name;
    std::string style;     // "House", "Deep House", "Melodic House", ...
    std::string numerals;  // "i | VI | III | VII"
    ScaleType mode = ScaleType::NaturalMinor;
    std::string note;      // one line on where it is normally used
};

const std::vector<ProgressionPreset>& progressionPresets();
const ProgressionPreset* findProgressionPreset(const std::string& id);

/** The distinct style names, in the order the presets are listed. */
std::vector<std::string> progressionStyles();

} // namespace harmonia
