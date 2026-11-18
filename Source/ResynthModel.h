#pragma once

#include <JuceHeader.h>
#include <vector>

// Result of analysing a one-shot sample. It is built once on the message
// thread and never modified afterwards, so the audio thread can read it safely.
struct ResynthModel
{
    static constexpr int maxPartials = 48;

    double sampleRate = 44100.0;                 // rate of the analysed material
    double durationSeconds = 0.0;
    double frameSeconds = 0.02;                  // time between envelope frames
    double rootHz = 0.0;                         // detected pitch, 0 = unpitched
    std::vector<double> partialHz;               // partial frequencies at the sample's own pitch
    std::vector<std::vector<float>> envelopes;   // [partial][frame], all partials sum to 1 at peak
    std::vector<float> click;                    // differentiated attack layer (first 60 ms)
};

// Builds a resynthesis model from a sample: spectral peak tracking over time
// (additive layer) plus a short differentiated attack (click layer).
ResynthModel analyseSample (const juce::AudioBuffer<float>& source, double sampleRate);
