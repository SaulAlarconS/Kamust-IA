#pragma once

#include <JuceHeader.h>
#include <array>
#include "ResynthModel.h"

// Parameters and models shared by all voices. The processor writes it once per block.
struct SharedVoiceState
{
    const ResynthModel* modelA = nullptr;
    const ResynthModel* modelB = nullptr;
    float harmonics = 1.0f;
    float click = 0.3f;
    float tilt = 0.0f;
    float morph = 0.0f;
    float transpose = 1.0f;     // frequency multiplier from the Tune knob
    float glideCoeff = 1.0f;    // per-sample smoothing coefficient, 1 = no glide
    float masterGain = 0.6f;
    juce::ADSR::Parameters adsr;
};

class ResynthSound : public juce::SynthesiserSound
{
public:
    bool appliesToNote (int) override { return true; }
    bool appliesToChannel (int) override { return true; }
};

// Additive resynthesis voice. It plays the partials of model A (and blends
// towards model B when Morph is above zero) at the pitch of the MIDI note.
class ResynthVoice : public juce::SynthesiserVoice
{
public:
    explicit ResynthVoice (const SharedVoiceState& state);

    bool canPlaySound (juce::SynthesiserSound* sound) override;
    void startNote (int midiNoteNumber, float velocity, juce::SynthesiserSound*, int) override;
    void stopNote (float velocity, bool allowTailOff) override;
    void pitchWheelMoved (int) override {}
    void controllerMoved (int, int) override {}
    void renderNextBlock (juce::AudioBuffer<float>& output, int startSample, int numSamples) override;
    void setCurrentPlaybackSampleRate (double newRate) override;

private:
    struct ModelState
    {
        std::array<double, ResynthModel::maxPartials> phase {};
        std::array<float, ResynthModel::maxPartials> gain {};
        double clickPos = 0.0;
    };

    static void computeGains (const ResynthModel& model, float tilt, float harmonics, ModelState& state);
    float renderPartials (const ResynthModel& model, ModelState& state, float modelGain, double noteHz, double t);
    float renderClick (const ResynthModel& model, double& position, float gain, double noteHz) const;

    const SharedVoiceState& shared;
    juce::ADSR adsr;
    double sampleRate = 44100.0;
    double currentHz = 440.0;
    double targetHz = 440.0;
    double time = 0.0;
    float velocityGain = 1.0f;
    bool hasPlayed = false;
    ModelState stateA;
    ModelState stateB;
};
