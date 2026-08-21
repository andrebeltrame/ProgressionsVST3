#pragma once

#include <juce_audio_basics/juce_audio_basics.h>

/** A deliberately plain built-in sound so you can hear an idea without wiring up
    an instrument first. Two detuned saw-ish oscillators through an ADSR. */
struct PreviewSound : public juce::SynthesiserSound
{
    bool appliesToNote(int) override { return true; }
    bool appliesToChannel(int) override { return true; }
};

class PreviewVoice : public juce::SynthesiserVoice
{
public:
    PreviewVoice()
    {
        envelope.setParameters({ 0.012f, 0.18f, 0.65f, 0.35f });
    }

    bool canPlaySound(juce::SynthesiserSound* sound) override
    {
        return dynamic_cast<PreviewSound*>(sound) != nullptr;
    }

    void startNote(int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override
    {
        const double frequency = juce::MidiMessage::getMidiNoteInHertz(midiNoteNumber);
        phaseDelta = frequency / getSampleRate();
        detunedDelta = phaseDelta * 1.0015;
        level = 0.16f * velocity;
        // Roll the top end off for high notes so chords do not turn harsh.
        brightness = juce::jlimit(0.15f, 0.9f, 1.0f - static_cast<float>(midiNoteNumber - 40) / 90.0f);
        envelope.setSampleRate(getSampleRate());
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
    juce::ADSR envelope;
    double phase = 0.0, detunedPhase = 0.0;
    double phaseDelta = 0.0, detunedDelta = 0.0;
    float level = 0.0f;
    float brightness = 0.5f;
    float lowpass = 0.0f;
};
