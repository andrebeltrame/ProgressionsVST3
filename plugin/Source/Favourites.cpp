#include "Favourites.h"

#include "StyleStore.h"

namespace favourites
{

namespace
{
const char* kListTag = "PROGRESSIONS_FAVOURITES";
const char* kEntryTag = "FAVOURITE";
} // namespace

juce::File file()
{
    return styleStore::directory().getChildFile("favourites.xml");
}

juce::File midiFolder()
{
    // When the plug-in's folder has been redirected - the smoke test does this
    // so it never touches a real installation - the MIDI follows it. Writing
    // into someone's actual Music folder from a test is not acceptable.
    if (juce::SystemStats::getEnvironmentVariable("HARMONIA_STYLE_DIR", {}).isNotEmpty())
        return styleStore::directory().getChildFile("Favourites");

    const auto music = juce::File::getSpecialLocation(juce::File::userMusicDirectory);
    if (music.isDirectory())
        return music.getChildFile("Progressions");
    return styleStore::directory().getChildFile("Favourites");
}

std::vector<Entry> load()
{
    std::vector<Entry> out;

    const auto xml = juce::XmlDocument::parse(file());
    if (xml == nullptr || ! xml->hasTagName(kListTag))
        return out;

    for (auto* element : xml->getChildWithTagNameIterator(kEntryTag))
    {
        Entry entry;
        entry.name = element->getStringAttribute("name");
        entry.created = element->getStringAttribute("created");
        entry.folder = element->getStringAttribute("folder");

        if (auto* stateXml = element->getFirstChildElement())
            entry.state = juce::ValueTree::fromXml(*stateXml);

        // A favourite whose state will not parse is not a favourite - it would
        // sit in the list looking recallable and do nothing when clicked.
        if (entry.name.isNotEmpty() && entry.state.isValid())
            out.push_back(std::move(entry));
    }

    return out;
}

bool save(const std::vector<Entry>& entries, juce::String& error)
{
    const auto target = file();
    const auto folder = target.getParentDirectory();

    const auto created = folder.createDirectory();
    if (created.failed())
    {
        error = created.getErrorMessage();
        return false;
    }

    juce::XmlElement list(kListTag);
    for (const auto& entry : entries)
    {
        auto* element = list.createNewChildElement(kEntryTag);
        element->setAttribute("name", entry.name);
        element->setAttribute("created", entry.created);
        element->setAttribute("folder", entry.folder);
        if (auto stateXml = entry.state.createXml())
            element->addChildElement(stateXml.release());
    }

    if (! list.writeTo(target))
    {
        error = "Could not write " + target.getFullPathName();
        return false;
    }

    return true;
}

} // namespace favourites
