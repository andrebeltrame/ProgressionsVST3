#include <cmath>
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
    if (writtenProgression)
        rebuildFromWrittenChords();
}

void Engine::setAnalysisOptions(const AnalysisOptions& newOptions)
{
    options = newOptions;
    reanalyse();
}

void Engine::reanalyse()
{
    if (sourceSequence.empty())
    {
        // No clip: either a written progression or nothing at all.
        currentAnalysis = Analysis {};
        detectedProgression.clear();
        if (writtenProgression)
            rebuildFromWrittenChords();
        return;
    }

    currentAnalysis = harmonia::analyze(sourceSequence, options);
    detectedProgression = currentAnalysis.progression;

    if (writtenProgression)
        rebuildFromWrittenChords();
    else
        applyHarmonyHold();
}

void Engine::applyHarmonyHold()
{
    if (harmonyHold <= 0.0f)
        return;

    // Absolute rather than a multiplier, so applying it twice is the same as
    // applying it once - which matters because rebuildFromWrittenChords is
    // reached both directly and through reanalyse.
    stretchHarmony(currentAnalysis,
                   static_cast<int64_t>(std::llround(static_cast<double>(harmonyHold)
                                                     * static_cast<double>(currentAnalysis.ticksPerBar()))));
    detectedProgression = currentAnalysis.progression;
}

void Engine::setBarsPerChord(float bars)
{
    const float wanted = bars > 0.0f ? bars : 0.0f;
    if (harmonyHold == wanted)
        return;
    harmonyHold = wanted;
    reanalyse();
}

void Engine::rebuildFromWrittenChords()
{
    if (writtenChords.empty())
        return;

    if (sourceSequence.empty())
    {
        const Key key = options.forceKey ? options.key : guessKeyForProgression(writtenChords);
        currentAnalysis = analysisFromProgression(writtenChords, key, canvasBpm, canvasTimeSignature,
                                                  canvasBarsPerChord > 0
                                                      ? canvasBarsPerChord * canvasTimeSignature.numerator
                                                      : 0,
                                                  1, kPPQ);
    }
    else
    {
        applyProgressionTo(currentAnalysis, writtenChords);
    }
    applyHarmonyHold();
    detectedProgression = currentAnalysis.progression;
}

bool Engine::setProgressionText(const std::string& text, std::string& error)
{
    Key key = currentAnalysis.valid ? currentAnalysis.key : (options.forceKey ? options.key : Key { 9, ScaleType::NaturalMinor });

    std::vector<Chord> chords;
    if (! parseProgression(text, key, chords, error))
        return false;

    writtenChords = std::move(chords);
    writtenProgression = true;
    writtenText = text;

    if (sourceSequence.empty() && ! options.forceKey)
        key = guessKeyForProgression(writtenChords);

    rebuildFromWrittenChords();
    if (sourceSequence.empty())
        currentAnalysis.key = key;

    return true;
}

bool Engine::applyPreset(const std::string& presetId, std::string& error)
{
    const auto* preset = findProgressionPreset(presetId);
    if (preset == nullptr)
    {
        error = "No preset called '" + presetId + "'.";
        return false;
    }

    // Keep whatever tonic we are already in, but switch to the mode the preset
    // was written for - "vi IV I V" only means anything in a major key.
    const int tonic = currentAnalysis.valid ? currentAnalysis.key.tonic
                                            : (options.forceKey ? options.key.tonic : 9);
    const Key key { tonic, preset->mode };

    std::vector<Chord> chords;
    if (! parseProgression(preset->numerals, key, chords, error))
        return false;

    // A pinned key has to follow the preset's mode, or the analysis ends up
    // saying one thing and the options another: the next reanalyse would read
    // the same numerals in the old mode and quietly hand back different chords.
    if (options.forceKey)
        options.key = key;

    writtenChords = std::move(chords);
    writtenProgression = true;
    writtenText = progressionToText(writtenChords);

    if (currentAnalysis.valid)
        currentAnalysis.key = key;

    rebuildFromWrittenChords();
    if (! currentAnalysis.valid && ! writtenChords.empty())
        currentAnalysis.key = key;
    else
        currentAnalysis.key = key;

    return true;
}

void Engine::setKey(const Key& key)
{
    options.forceKey = true;
    options.key = key;
    reanalyse();
    if (! sourceSequence.empty() || writtenProgression)
        currentAnalysis.key = key;
}

void Engine::setBlankCanvas(double bpm, int barsPerChord, const TimeSignature& timeSignature)
{
    canvasBpm = bpm > 0.0 ? bpm : 122.0;
    canvasBarsPerChord = barsPerChord > 0 ? barsPerChord : 1;
    canvasTimeSignature = timeSignature;
    if (sourceSequence.empty() && writtenProgression)
        rebuildFromWrittenChords();
}

NoteSequence Engine::generate(const GenerateOptions& generateOptions) const
{
    return harmonia::generate(currentAnalysis, sourceSequence, generateOptions);
}

std::vector<NoteSequence> Engine::generateVariations(const GenerateOptions& generateOptions, int count) const
{
    return harmonia::generateVariations(currentAnalysis, sourceSequence, generateOptions, count);
}

void Engine::applyReharmonization(uint32_t seed, float amount, const std::vector<bool>& locked)
{
    if (! currentAnalysis.valid)
        return;
    currentAnalysis.progression = harmonia::reharmonize(currentAnalysis, seed, amount, locked);
}

void Engine::resetProgression()
{
    // Back to what the clip actually said, discarding anything written by hand.
    writtenProgression = false;
    writtenChords.clear();
    writtenText.clear();
    reanalyse();
}

void Engine::setChord(size_t index, const Chord& chord)
{
    if (index < currentAnalysis.progression.size())
        currentAnalysis.progression[index].chord = chord;
}

} // namespace harmonia
