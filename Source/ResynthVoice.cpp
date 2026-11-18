#include "ResynthVoice.h"

#include <cmath>

namespace
{
    constexpr int sineSize = 4096;

    struct SineTable
    {
        std::array<float, sineSize> data {};

        SineTable()
        {
            for (int i = 0; i < sineSize; ++i)
                data[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * i / sineSize);
        }
    };

    const SineTable sineTable;

    inline float lookupSine (double phase) noexcept
    {
        return sineTable.data[(size_t) ((int) (phase * sineSize) & (sineSize - 1))];
    }
}

ResynthVoice::ResynthVoice (const SharedVoiceState& state)
    : shared (state)
{
}

bool ResynthVoice::canPlaySound (juce::SynthesiserSound* sound)
{
    return dynamic_cast<ResynthSound*> (sound) != nullptr;
}

void ResynthVoice::setCurrentPlaybackSampleRate (double newRate)
{
    juce::SynthesiserVoice::setCurrentPlaybackSampleRate (newRate);
    sampleRate = newRate;
    adsr.setSampleRate (newRate);
}

void ResynthVoice::startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int)
{
    targetHz = juce::MidiMessage::getMidiNoteInHertz (midiNoteNumber) * shared.transpose;

    // With glide off, or on the very first note, jump straight to the pitch.
    if (! hasPlayed || shared.glideCoeff >= 1.0f)
        currentHz = targetHz;
    hasPlayed = true;

    velocityGain = velocity;
    time = 0.0;

    // Deterministic phase offsets avoid the partials all starting in step.
    for (int k = 0; k < ResynthModel::maxPartials; ++k)
    {
        const double offset = std::fmod (k * 0.618033988, 1.0);
        stateA.phase[(size_t) k] = offset;
        stateB.phase[(size_t) k] = offset;
    }
    stateA.clickPos = 0.0;
    stateB.clickPos = 0.0;

    adsr.setParameters (shared.adsr);
    adsr.noteOn();
}

void ResynthVoice::stopNote (float, bool allowTailOff)
{
    adsr.noteOff();
    if (! allowTailOff)
        clearCurrentNote();
}

void ResynthVoice::computeGains (const ResynthModel& model, float tilt, float harmonics, ModelState& state)
{
    const int count = (int) model.partialHz.size();
    const double reference = model.rootHz > 0.0
                                 ? model.rootHz
                                 : (count > 0 ? model.partialHz.front() : 1.0);

    for (int k = 0; k < ResynthModel::maxPartials; ++k)
    {
        if (k >= count)
        {
            state.gain[(size_t) k] = 0.0f;
            continue;
        }
        const double rel = juce::jlimit (0.1, 8.0, model.partialHz[(size_t) k] / reference);
        state.gain[(size_t) k] = (float) (std::pow (rel, (double) tilt) * harmonics);
    }
}

float ResynthVoice::renderPartials (const ResynthModel& model, ModelState& state, float modelGain, double noteHz, double t)
{
    if (modelGain <= 0.0f || model.envelopes.empty())
        return 0.0f;

    const double ratio = model.rootHz > 0.0 ? noteHz / model.rootHz : 1.0;
    const double framePos = t / model.frameSeconds;
    const int count = (int) model.partialHz.size();
    float out = 0.0f;

    for (int k = 0; k < count; ++k)
    {
        const double freq = model.partialHz[(size_t) k] * ratio;
        if (freq >= 0.45 * sampleRate)
            continue;

        const auto& env = model.envelopes[(size_t) k];
        const int last = (int) env.size() - 1;
        float amp = 0.0f;
        if (framePos >= last)
        {
            amp = env[(size_t) last];
        }
        else
        {
            const int i = (int) framePos;
            const float frac = (float) (framePos - i);
            amp = env[(size_t) i] + (env[(size_t) i + 1] - env[(size_t) i]) * frac;
        }

        auto& phase = state.phase[(size_t) k];
        out += amp * state.gain[(size_t) k] * lookupSine (phase);

        phase += freq / sampleRate;
        phase -= std::floor (phase);
    }

    return out * modelGain;
}

float ResynthVoice::renderClick (const ResynthModel& model, double& position, float gain, double noteHz) const
{
    const auto& data = model.click;
    const int len = (int) data.size();
    const int i = (int) position;
    float out = 0.0f;

    if (gain > 0.0f && i >= 0 && i + 1 < len)
    {
        const float frac = (float) (position - i);
        out = (data[(size_t) i] + (data[(size_t) i + 1] - data[(size_t) i]) * frac) * gain;
    }

    const double ratio = model.rootHz > 0.0 ? noteHz / model.rootHz : 1.0;
    position += ratio * model.sampleRate / sampleRate;
    return out;
}

void ResynthVoice::renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples)
{
    const auto* a = shared.modelA;
    if (a == nullptr || ! adsr.isActive())
    {
        clearCurrentNote();
        return;
    }

    adsr.setParameters (shared.adsr);

    const auto* b = shared.modelB;
    const bool useB = b != nullptr && shared.morph > 0.0f;
    const float gainA = useB ? 1.0f - shared.morph : 1.0f;
    const float gainB = useB ? shared.morph : 0.0f;

    computeGains (*a, shared.tilt, shared.harmonics, stateA);
    if (useB)
        computeGains (*b, shared.tilt, shared.harmonics, stateB);

    const double glide = shared.glideCoeff;

    for (int i = 0; i < numSamples; ++i)
    {
        currentHz += (targetHz - currentHz) * glide;

        float s = renderPartials (*a, stateA, gainA, currentHz, time);
        s += renderClick (*a, stateA.clickPos, shared.click * gainA, currentHz);

        if (useB)
        {
            s += renderPartials (*b, stateB, gainB, currentHz, time);
            s += renderClick (*b, stateB.clickPos, shared.click * gainB, currentHz);
        }

        const float env = adsr.getNextSample();
        s *= env * velocityGain * shared.masterGain;

        for (int ch = 0; ch < output.getNumChannels(); ++ch)
            output.addSample (ch, startSample + i, s);

        time += 1.0 / sampleRate;

        if (! adsr.isActive())
        {
            clearCurrentNote();
            break;
        }
    }
}
