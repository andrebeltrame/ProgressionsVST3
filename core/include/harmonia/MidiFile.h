#pragma once

#include "harmonia/Types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace harmonia
{

/** Minimal, dependency-free Standard MIDI File reader/writer (format 0 and 1).
    Everything read is merged into a single NoteSequence and rescaled to kPPQ.
*/
namespace midi
{

bool readFromMemory(const uint8_t* data, size_t size, NoteSequence& out, std::string& error);
bool readFromFile(const std::string& path, NoteSequence& out, std::string& error);

/** Writes a format-1 file: track 0 holds tempo/time-signature, track 1 the notes. */
std::vector<uint8_t> writeToMemory(const NoteSequence& seq);
bool writeToFile(const std::string& path, const NoteSequence& seq, std::string& error);

} // namespace midi
} // namespace harmonia
