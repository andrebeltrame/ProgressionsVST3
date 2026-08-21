#include "harmonia/Engine.h"

namespace harmonia
{

bool Engine::loadSourceFromFile(const std::string& path, std::string& error)
{
    NoteSequence loaded;
    if (! midi::readFromFile(path, loaded, error))
        return false;
    setSource(loaded);
    return true;
}

bool Engine::loadSourceFromMemory(const uint8_t* data, size_t size, std::string& error)
{
    NoteSequence loaded;
    if (! midi::readFromMemory(data, size, loaded, error))
        return false;
    setSource(loaded);
    return true;
}

void Engine::setSource(const NoteSequence& sequence)
{
    sourceSequence = sequence;
    sourceSequence.sort();
    reanalyse();
}

void Engine::clear()
{
    sourceSequence.clear();
    currentAnalysis = Analysis {};
    detectedProgression.clear();
}

void Engine::setAnalysisOptions(const AnalysisOptions& newOptions)
{
    options = newOptions;
    reanalyse();
}

void Engine::reanalyse()
{
    currentAnalysis = harmonia::analyze(sourceSequence, options);
    detectedProgression = currentAnalysis.progression;
}

NoteSequence Engine::generate(const GenerateOptions& generateOptions) const
{
    return harmonia::generate(currentAnalysis, sourceSequence, generateOptions);
}

std::vector<NoteSequence> Engine::generateVariations(const GenerateOptions& generateOptions, int count) const
{
    return harmonia::generateVariations(currentAnalysis, sourceSequence, generateOptions, count);
}

void Engine::applyReharmonization(uint32_t seed, float amount)
{
    if (! currentAnalysis.valid)
        return;
    currentAnalysis.progression = harmonia::reharmonize(currentAnalysis, seed, amount);
}

void Engine::resetProgression()
{
    currentAnalysis.progression = detectedProgression;
}

void Engine::setChord(size_t index, const Chord& chord)
{
    if (index < currentAnalysis.progression.size())
        currentAnalysis.progression[index].chord = chord;
}

} // namespace harmonia
