#pragma once

#include <juce_core/juce_core.h>

#include <string>

/** Where the plugin keeps its brain.

    A style model can reach the generators three ways, in this order of
    precedence:

      1. a file loaded into one instance for this session only;
      2. the model installed next to the plugin, in the user's application
         data folder - picking one in the editor puts it there, so every new
         instance in every project starts with it;
      3. a model baked into the binary at build time with
         -DHARMONIA_STYLE_MODEL=<file>, which travels inside the .vst3.

    That way the plugin is never a dangling path to someone's drive. */
namespace styleStore
{

/** The folder the installed model lives in. Overridable with the
    HARMONIA_STYLE_DIR environment variable, which the tests use so they never
    touch a real installation. */
juce::File directory();

/** The installed model itself; may not exist. */
juce::File installedFile();
bool hasInstalled();

/** Copies `source` in, so it becomes the model every instance starts with.
    Returns false and fills `error` if the file will not parse or will not copy. */
bool install(const juce::File& source, juce::String& error);

/** Removes the installed copy. The plugin falls back to the built-in model. */
bool forget();

/** True when this build had a model baked in. */
bool hasBuiltIn();
/** The baked-in model's JSON, or an empty string. */
std::string builtInJson();
/** The name of the file it was baked from, for the UI. */
juce::String builtInName();

} // namespace styleStore
