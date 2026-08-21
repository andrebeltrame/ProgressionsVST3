#pragma once

#include "harmonia/Analysis.h"
#include "harmonia/Generators.h"
#include "harmonia/MidiFile.h"
#include "harmonia/Types.h"

#include <string>
#include <vector>

namespace harmonia
{

/** Everything the plugin and the CLI need, in one object:
    load a clip -> analyse it -> write new parts over its harmony. */
class Engine
{
public:
    Engine() = default;

    bool loadSourceFromFile(const std::string& path, std::string& error);
    bool loadSourceFromMemory(const uint8_t* data, size_t size, std::string& error);
    void setSource(const NoteSequence& sequence);
    void clear();

    const NoteSequence& source() const noexcept { return sourceSequence; }
    bool hasSource() const noexcept { return ! sourceSequence.empty(); }

    const AnalysisOptions& analysisOptions() const noexcept { return options; }
    void setAnalysisOptions(const AnalysisOptions& newOptions);

    const Analysis& analysis() const noexcept { return currentAnalysis; }

    NoteSequence generate(const GenerateOptions& generateOptions) const;
    std::vector<NoteSequence> generateVariations(const GenerateOptions& generateOptions, int count) const;

    /** Replaces the detected progression with a reharmonised version. */
    void applyReharmonization(uint32_t seed, float amount);
    /** Restores the progression that was detected from the source clip. */
    void resetProgression();
    /** Hand-edit a single chord of the progression. */
    void setChord(size_t index, const Chord& chord);

private:
    void reanalyse();

    NoteSequence sourceSequence;
    AnalysisOptions options;
    Analysis currentAnalysis;
    std::vector<ChordSegment> detectedProgression;
};

} // namespace harmonia
