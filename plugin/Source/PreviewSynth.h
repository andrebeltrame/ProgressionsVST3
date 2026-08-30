#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

#include <array>
#include <atomic>

/** A deliberately plain built-in sound so you can hear an idea without wiring up
    an instrument first. Two detuned saw-ish oscillators through an ADSR. */
struct PreviewSound : public juce::SynthesiserSound
{
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

/** How the preview should sound for one part. Parts play together now, each on
    its own MIDI channel, and seven parts through one pad tone is mud - a bass
    needs to be a bass to tell you anything. */
struct PreviewCharacter
{
    float attack = 0.012f;
    float decay = 0.18f;
    float sustain = 0.65f;
    float release = 0.35f;
    float detune = 1.0015f; // 1.0 = a single oscillator
    float tone = 1.0f;      // scales the low-pass cutoff
    float gain = 1.0f;
};

/** Which character each MIDI channel plays with. Written from the message thread
    when the parts change, read by the voices; a torn read is one note in the
    wrong timbre, and nothing worse. */
using PreviewCharacterMap = std::array<std::atomic<int>, 17>;

/** Indexed by the value stored in the map above. */
inline const PreviewCharacter& previewCharacter(int index)
{
    static const PreviewCharacter characters[] = {
        { 0.055f, 0.30f, 0.80f, 0.60f, 1.0015f, 1.00f, 1.00f }, // Pad
        { 0.008f, 0.22f, 0.55f, 0.28f, 1.0020f, 1.25f, 0.95f }, // Chords
        { 0.010f, 0.20f, 0.70f, 0.30f, 1.0010f, 1.45f, 1.00f }, // Melody
        { 0.014f, 0.22f, 0.62f, 0.32f, 1.0010f, 1.30f, 0.85f }, // Counter melody
        { 0.004f, 0.16f, 0.85f, 0.12f, 1.0000f, 0.35f, 1.30f }, // Bass
        { 0.030f, 0.40f, 0.95f, 0.45f, 1.0060f, 0.28f, 1.25f }, // Reese - held, detuned, dark
        { 0.004f, 0.12f, 0.35f, 0.16f, 1.0015f, 1.40f, 0.90f }, // Arp
        { 0.003f, 0.09f, 0.20f, 0.12f, 1.0025f, 1.60f, 0.95f }, // Pluck
        // Sub - one oscillator, no detune, as close to a sine as this gets, and
        // a short release so two roots never ring into each other down there.
        { 0.006f, 0.20f, 0.98f, 0.10f, 1.0000f, 0.12f, 1.35f }, // Sub
    };
    constexpr int count = static_cast<int>(std::size(characters));
    return characters[juce::jlimit(0, count - 1, index)];
}

class PreviewVoice : public juce::SynthesiserVoice
{
public:
    explicit PreviewVoice(const PreviewCharacterMap& map) : characterMap(map) {}

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<PreviewSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        // JUCE sets the playing channel before it calls this, but only exposes
        // it as a predicate, so ask each channel in turn.
        int character = 0;
        for (int channel = 1; channel <= 16; ++channel)
        {
            if (isPlayingChannel(channel))
            {
                character = characterMap[static_cast<size_t>(channel)].load(std::memory_order_relaxed);
                break;
            }
        }
        const auto& voiceCharacter = previewCharacter(character);

        const double frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        phaseDelta = frequency / getSampleRate();
        detunedDelta = phaseDelta * static_cast<double>(voiceCharacter.detune);
        level = 0.16f * velocity * voiceCharacter.gain;
        // Roll the top end off for high notes so chords do not turn harsh.
        brightness = juce::jlimit(0.05f, 0.95f,
                                  (1.0f - static_cast<float>(midiNoteNumber - 40) / 90.0f)
                                      * voiceCharacter.tone);
        envelope.setSampleRate(getSampleRate());
        envelope.setParameters({ voiceCharacter.attack, voiceCharacter.decay,
                                 voiceCharacter.sustain, voiceCharacter.release });
        envelope.noteOn();
    }

    void stopNote(float, bool allowTailOff) override
    {
        if (allowTailOff)
        {
            envelope.noteOff();
        }
        else
        {
            envelope.reset();
            clearCurrentNote();
        }
    }

    void pitchWheelMoved(int) override {}
    void controllerMoved(int, int) override {}

    void renderNextBlock(juce::AudioBuffer<float>& output, int startSample, int numSamples) override
    {
        if (! envelope.isActive())
            return;

        for (int i = 0; i < numSamples; ++i)
        {
            const auto saw = static_cast<float>(2.0 * phase - 1.0);
            const auto detuned = static_cast<float>(2.0 * detunedPhase - 1.0);
            const float raw = 0.5f * (saw + detuned);

            lowpass += brightness * (raw - lowpass);
            const float sample = lowpass * level * envelope.getNextSample();

            for (int channel = output.getNumChannels(); --channel >= 0;)
                output.addSample(channel, startSample + i, sample);

            phase += phaseDelta;
            if (phase >= 1.0)
                phase -= 1.0;
            detunedPhase += detunedDelta;
            if (detunedPhase >= 1.0)
                detunedPhase -= 1.0;
        }

        if (! envelope.isActive())
            clearCurrentNote();
    }

private:
    const PreviewCharacterMap& characterMap;
    juce::ADSR envelope;
    double phase = 0.0, detunedPhase = 0.0;
    double phaseDelta = 0.0, detunedDelta = 0.0;
    float level = 0.0f;
    float brightness = 0.5f;
    float lowpass = 0.0f;
};
