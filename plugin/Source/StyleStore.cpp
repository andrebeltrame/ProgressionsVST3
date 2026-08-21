#include "StyleStore.h"

#include "harmonia/StyleModel.h"

#if HARMONIA_HAS_EMBEDDED_STYLE
 #include "HarmoniaStyleData.h"
#endif

namespace styleStore
{

namespace
{
const char* kFileName = "library.style.json";
}

juce::File directory()
{
    const auto override_ = juce::SystemStats::getEnvironmentVariable("HARMONIA_STYLE_DIR", {});
    if (override_.isNotEmpty())
        return juce::File(override_);

    // Spelled out per platform rather than taken from
    // userApplicationDataDirectory, which is ~/Library on the Mac and the bare
    // home folder on Linux. harmonia-cli's --install mirrors these paths.
   #if JUCE_MAC
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile("Library/Application Support/Harmonia");
   #elif JUCE_WINDOWS
    return juce::File::getSpecialLocation(juce::File::userApplicationDataDirectory)
        .getChildFile("Harmonia");
   #else
    return juce::File::getSpecialLocation(juce::File::userHomeDirectory)
        .getChildFile(".config/Harmonia");
   #endif
}

juce::File installedFile()
{
    return directory().getChildFile(kFileName);
}

bool hasInstalled()
{
    return installedFile().existsAsFile();
}

bool install(const juce::File& source, juce::String& error)
{
    // Parse before copying: an installed model that will not load would leave
    // every future instance starting with an error.
    harmonia::StyleModel probe;
    std::string parseError;
    if (! harmonia::loadStyleModel(source.getFullPathName().toStdString(), probe, parseError))
    {
        error = juce::String(parseError);
        return false;
    }

    const auto folder = directory();
    const auto result = folder.createDirectory();
    if (result.failed())
    {
        error = result.getErrorMessage();
        return false;
    }

    const auto destination = installedFile();
    if (source == destination)
        return true;

    if (! source.copyFileTo(destination))
    {
        error = "Could not write " + destination.getFullPathName();
        return false;
    }

    error.clear();
    return true;
}

bool forget()
{
    const auto file = installedFile();
    return ! file.existsAsFile() || file.deleteFile();
}

bool hasBuiltIn()
{
   #if HARMONIA_HAS_EMBEDDED_STYLE
    return HarmoniaStyleData::namedResourceListSize > 0;
   #else
    return false;
   #endif
}

std::string builtInJson()
{
   #if HARMONIA_HAS_EMBEDDED_STYLE
    if (HarmoniaStyleData::namedResourceListSize <= 0)
        return {};

    int size = 0;
    if (const char* data = HarmoniaStyleData::getNamedResource(HarmoniaStyleData::namedResourceList[0], size))
        return std::string(data, static_cast<size_t>(size));
   #endif
    return {};
}

juce::String builtInName()
{
   #if HARMONIA_HAS_EMBEDDED_STYLE
    if (HarmoniaStyleData::namedResourceListSize > 0)
        return juce::String(HarmoniaStyleData::originalFilenames[0]);
   #endif
    return {};
}

} // namespace styleStore
