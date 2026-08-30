#pragma once

#include <juce_core/juce_core.h>
#include <juce_data_structures/juce_data_structures.h>

#include <vector>

/** The combinations you decided to keep.

    Global, not per project: a favourite is something you found, and finding it
    again should not depend on which set you had open at the time. So it lives
    next to the style model, in the plugin's own folder, and every instance in
    every project sees the same list.

    A favourite is the plug-in's whole state - and because generation is
    deterministic, that state *is* the music: the same seed and the same
    settings write the same notes back, to the tick. So a favourite costs a few
    hundred bytes rather than a file of notes, and recalling one gives you the
    parts back live and still adjustable, not a rendered clip.

    The MIDI is written out too, one file per part, because the point of a
    favourite is often to use it somewhere this plug-in is not. */
namespace favourites
{

struct Entry
{
    juce::String name;     // what the list shows: key, chords, parts
    juce::String created;  // ISO date, for the tooltip
    juce::String folder;   // where its MIDI was written, may be empty
    juce::ValueTree state; // the plug-in state that reproduces it
};

/** The file the list is kept in. */
juce::File file();

/** Where the exported MIDI goes: a Progressions folder in the user's music
    folder, or beside the list itself when there is no music folder to use. */
juce::File midiFolder();

std::vector<Entry> load();
bool save(const std::vector<Entry>& entries, juce::String& error);

} // namespace favourites
