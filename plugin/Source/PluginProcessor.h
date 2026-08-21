#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

#include "PreviewSynth.h"
#include "RenderedPart.h"

#include "harmonia/Engine.h"

namespace ParamID
{
constexpr const char* part        = "part";
constexpr const char* density     = "density";
constexpr const char* complexity  = "complexity";
constexpr const char* humanize    = "humanize";
constexpr const char* develop     = "develop";
constexpr const char* swing       = "swing";
constexpr const char* octave      = "octave";
constexpr const char* voices      = "voices";
constexpr const char* bars        = "bars";
constexpr const char* follow      = "followRhythm";
constexpr const char* avoid       = "avoidSource";
constexpr const char* arpPattern  = "arpPattern";
constexpr const char* preview     = "preview";
constexpr const char* level       = "level";
constexpr const char* hostSync    = "hostSync";
constexpr const char* midiChannel = "midiChannel";
constexpr const char* harmonicRhythm = "harmonicRhythm";
constexpr const char* useStyle = "useStyle";
constexpr const char* styleAmount = "styleAmount";
} // namespace ParamID

/** The plugin: holds the analysis engine, renders the generated part and plays
    it back through MIDI out and a built-in preview voice. */
class ProgressionsProcessor : public juce::AudioProcessor,
                          public juce::ChangeBroadcaster,
                          private juce::AudioProcessorValueTreeState::Listener,
                          private juce::AsyncUpdater
{
public:
    ProgressionsProcessor();
    ~ProgressionsProcessor() override;

    // --- AudioProcessor -----------------------------------------------------
    void prepareToPlay(double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported(const BusesLayout& layouts) const override;
    void processBlock(juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Progressions"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 1.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram(int) override {}
    const juce::String getProgramName(int) override { return "Default"; }
    void changeProgramName(int, const juce::String&) override {}

    void getStateInformation(juce::MemoryBlock& destData) override;
    void setStateInformation(const void* data, int sizeInBytes) override;

    // --- Message-thread API used by the editor ------------------------------
    bool loadMidiFile(const juce::File& file);
    bool loadMidiFromData(const void* data, size_t size, const juce::String& displayName);
    void clearSource();

    void regenerate();
    void rollNewSeed();
    juce::uint32 getSeed() const noexcept { return seed; }
    void setSeed(juce::uint32 newSeed);

    void reharmonize(float amount);
    void resetProgression();
    void nudgeChord(int index, int direction);

    /** Types a progression in over whatever is loaded: "Am | F | C | G" or
        "i VI III VII". Returns false and sets the error text if it will not parse. */
    bool setProgressionText(const juce::String& text);
    bool applyPreset(const juce::String& presetId);
    juce::String getProgressionText() const;
    bool hasWrittenProgression() const noexcept { return engine.hasWrittenProgression(); }

    /** Where the model currently in use came from. */
    enum class StyleSource
    {
        None,
        BuiltIn,   // baked into this build with -DHARMONIA_STYLE_MODEL
        Installed, // living in the plugin's own folder, shared by every instance
        Session    // a file loaded into this instance only
    };

    /** Load the style model that `harmonia-cli learn` built from your own
        collection, so the generators write with your habits. With `install`
        set it also becomes the plugin's own model, which every new instance
        picks up without needing this path again. */
    bool loadStyleModelFile(const juce::File& file, bool install = true);
    /** Loads whatever the plugin carries: the installed model if there is one,
        otherwise the one baked into the binary. */
    void loadDefaultStyleModel();
    /** Drops the installed copy and falls back to the built-in model. */
    void forgetInstalledStyleModel();
    void clearStyleModel();
    bool hasStyleModel() const noexcept { return ! styleModel.empty(); }
    juce::String styleSummary() const;
    int styleClipCount() const noexcept { return styleModel.clipsLearned; }
    juce::File styleModelFile() const { return styleFile; }
    StyleSource styleSource() const noexcept { return styleOrigin; }
    /** One short line for the UI: where this brain came from. */
    juce::String styleSourceText() const;

    /** Pin the key instead of letting the detector choose. */
    void setForcedKey(bool forced, int tonic, harmonia::ScaleType scale);
    bool isKeyForced() const noexcept { return keyForced; }
    int forcedTonic() const noexcept { return keyTonic; }
    harmonia::ScaleType forcedScale() const noexcept { return keyScale; }

    const harmonia::Engine& getEngine() const noexcept { return engine; }
    const harmonia::NoteSequence& getGeneratedSequence() const noexcept { return generated; }
    juce::String getSourceName() const { return sourceName; }
    juce::String getLastError() const { return lastError; }

    /** Writes the generated part next to the given file, or to a temp file for
        a drag-and-drop into the host. */
    bool exportGenerated(const juce::File& destination);
    juce::File writeDragFile();

    // --- Internal transport --------------------------------------------------
    void startPlayback();
    void stopPlayback();
    bool isPlayingInternally() const noexcept { return internalPlaying.load(); }
    /** 0..1 through the generated loop, for the playhead in the piano roll. */
    double getPlayPositionNormalised() const;

    juce::AudioProcessorValueTreeState apvts;

private:
    void parameterChanged(const juce::String& parameterID, float newValue) override;
    void handleAsyncUpdate() override;

    harmonia::GenerateOptions currentOptions() const;
    void applyAnalysisOptions();
    void publish(RenderedPart::Ptr part);
    void renderEvents(juce::MidiBuffer& midi, int numSamples, double bpm, double startPPQ);
    void panic(juce::MidiBuffer& midi);

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    harmonia::Engine engine;
    harmonia::StyleModel styleModel;
    juce::File styleFile;
    StyleSource styleOrigin = StyleSource::None;
    harmonia::NoteSequence generated;
    juce::String sourceName;
    juce::String lastError;
    juce::MemoryBlock sourceMidiData;
    juce::uint32 seed = 1;

    bool keyForced = false;
    int keyTonic = 9;
    harmonia::ScaleType keyScale = harmonia::ScaleType::NaturalMinor;

    juce::SpinLock partLock;
    RenderedPart::Ptr activePart;

    juce::Synthesiser previewSynth;
    juce::MidiBuffer scratchMidi;
    juce::File dragFile;

    double currentSampleRate = 44100.0;
    double internalPPQ = 0.0;
    std::atomic<bool> internalPlaying { false };
    std::atomic<double> reportedPosition { 0.0 };
    std::atomic<double> clipBpm { 120.0 };
    std::atomic<double> hostBpm { 122.0 };
    bool wasPlaying = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR(ProgressionsProcessor)
};
