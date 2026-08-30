#pragma once

#include "harmonia/Analysis.h"
#include "harmonia/Generators.h"
#include "harmonia/MidiFile.h"
#include "harmonia/Presets.h"
#include "harmonia/Progression.h"
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

    /** Replaces the detected progression with a reharmonised version. Chords
        whose entry in `locked` is true are left exactly as they are. */
    void applyReharmonization(uint32_t seed, float amount, const std::vector<bool>& locked = {});
    /** Restores the progression that was detected from the source clip, or the
        last one that was typed in if there is no clip. */
    void resetProgression();
    /** Hand-edit a single chord of the progression. */
    void setChord(size_t index, const Chord& chord);

    // --- Written progressions -------------------------------------------------

    /** Sets the progression from text: "Am | F | C | G" or "i VI III VII".
        With a clip loaded, its tempo, meter and groove are kept and only the
        chords change. Without one, a bare progression is built from scratch. */
    bool setProgressionText(const std::string& text, std::string& error);

    /** Applies a style preset, keeping the current tonic but taking the
        preset's mode. */
    bool applyPreset(const std::string& presetId, std::string& error);

    /** True when the progression came from the user rather than from a clip. */
    bool hasWrittenProgression() const noexcept { return writtenProgression; }
    const std::string& progressionText() const noexcept { return writtenText; }

    /** Key used for reading roman numerals and for generating without a clip. */
    void setKey(const Key& key);

    /** Tempo, meter and length used when there is no source clip. */
    void setBlankCanvas(double bpm, int barsPerChord, const TimeSignature& timeSignature = {});
    double blankBpm() const noexcept { return canvasBpm; }

private:
    void reanalyse();

    void rebuildFromWrittenChords();

    NoteSequence sourceSequence;
    AnalysisOptions options;
    Analysis currentAnalysis;
    std::vector<ChordSegment> detectedProgression;

    bool writtenProgression = false;
    std::string writtenText;
    std::vector<Chord> writtenChords;
    double canvasBpm = 122.0;
    int canvasBarsPerChord = 1;
    TimeSignature canvasTimeSignature;
};

} // namespace harmonia
