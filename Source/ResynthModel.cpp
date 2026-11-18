#include "ResynthModel.h"

#include <algorithm>
#include <cmath>

namespace
{
    constexpr int fftOrder = 12;                 // 4096-point analysis window
    constexpr int fftSize = 1 << fftOrder;
    constexpr int hopSize = 1024;
    constexpr double maxAnalysisSeconds = 12.0;
    constexpr int maxEnvelopeFrames = 512;

    struct Peak
    {
        int bin;
        float amp;
    };
}

ResynthModel analyseSample (const juce::AudioBuffer<float>& source, double sampleRate)
{
    ResynthModel model;
    model.sampleRate = sampleRate;

    const int channels = source.getNumChannels();
    const int numSamples = juce::jmin (source.getNumSamples(), (int) (maxAnalysisSeconds * sampleRate));
    if (channels <= 0 || numSamples <= 0 || sampleRate <= 0.0)
        return model;

    // 1. Mono mix, peak-normalised so the analysis does not depend on the input level.
    std::vector<float> x ((size_t) numSamples, 0.0f);
    for (int ch = 0; ch < channels; ++ch)
    {
        const float* in = source.getReadPointer (ch);
        for (int i = 0; i < numSamples; ++i)
            x[(size_t) i] += in[i] / (float) channels;
    }

    float peak = 0.0f;
    for (float v : x)
        peak = std::max (peak, std::abs (v));
    if (peak < 1e-6f)
        return model;
    for (float& v : x)
        v /= peak;
    model.durationSeconds = numSamples / sampleRate;

    // 2. Click layer: first 60 ms, differentiated to emphasise the attack, faded out.
    {
        const int len = juce::jmin (numSamples, juce::roundToInt (0.06 * sampleRate));
        model.click.resize ((size_t) len);
        for (int i = 0; i < len; ++i)
        {
            const float prev = i > 0 ? x[(size_t) i - 1] : 0.0f;
            const float fade = std::exp (-4.0f * (float) i / (float) len);
            model.click[(size_t) i] = (x[(size_t) i] - prev) * fade;
        }
    }

    // 3. Short-time Fourier transform, magnitudes only.
    juce::dsp::FFT fft (fftOrder);
    std::vector<float> window ((size_t) fftSize);
    for (int i = 0; i < fftSize; ++i)
        window[(size_t) i] = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) (fftSize - 1));

    const int half = fftSize / 2;
    const int numFrames = numSamples / hopSize + 1;
    std::vector<std::vector<float>> mags ((size_t) numFrames, std::vector<float> ((size_t) half + 1, 0.0f));
    std::vector<float> buf ((size_t) fftSize * 2, 0.0f);

    for (int f = 0; f < numFrames; ++f)
    {
        std::fill (buf.begin(), buf.end(), 0.0f);
        const int start = f * hopSize - fftSize / 2;
        for (int i = 0; i < fftSize; ++i)
        {
            const int idx = start + i;
            if (idx >= 0 && idx < numSamples)
                buf[(size_t) i] = x[(size_t) idx] * window[(size_t) i];
        }
        fft.performFrequencyOnlyForwardTransform (buf.data());
        for (int k = 0; k <= half; ++k)
            mags[(size_t) f][(size_t) k] = buf[(size_t) k];
    }

    // 4. Average spectrum and its strongest local maxima = the partials.
    std::vector<float> avg ((size_t) half + 1, 0.0f);
    for (const auto& frame : mags)
        for (int k = 0; k <= half; ++k)
            avg[(size_t) k] += frame[(size_t) k] / (float) numFrames;

    const float avgMax = *std::max_element (avg.begin(), avg.end());
    if (avgMax <= 1e-9f)
        return model;

    std::vector<Peak> peaks;
    for (int k = 2; k < half - 2; ++k)
    {
        const float a = avg[(size_t) k - 1];
        const float b = avg[(size_t) k];
        const float c = avg[(size_t) k + 1];
        if (b > a && b >= c && b > 0.01f * avgMax)
            peaks.push_back ({ k, b });
    }
    if (peaks.empty())
        return model;

    std::sort (peaks.begin(), peaks.end(), [] (const Peak& p, const Peak& q) { return p.amp > q.amp; });
    if ((int) peaks.size() > ResynthModel::maxPartials)
        peaks.resize ((size_t) ResynthModel::maxPartials);
    std::sort (peaks.begin(), peaks.end(), [] (const Peak& p, const Peak& q) { return p.bin < q.bin; });

    float topAmp = 0.0f;
    for (const auto& p : peaks)
        topAmp = std::max (topAmp, p.amp);

    // Refine each peak frequency with parabolic interpolation.
    const double binHz = sampleRate / fftSize;
    for (const auto& p : peaks)
    {
        const float a = avg[(size_t) p.bin - 1];
        const float b = avg[(size_t) p.bin];
        const float c = avg[(size_t) p.bin + 1];
        const float denom = a - 2.0f * b + c;
        const float delta = std::abs (denom) > 1e-12f ? 0.5f * (a - c) / denom : 0.0f;
        model.partialHz.push_back ((p.bin + delta) * binHz);
    }

    // 5. Pitch: the lowest strong partial, accepted only if the partials look harmonic.
    for (size_t i = 0; i < peaks.size(); ++i)
    {
        if (peaks[i].amp >= 0.25f * topAmp && model.partialHz[i] >= 40.0)
        {
            model.rootHz = model.partialHz[i];
            break;
        }
    }
    if (model.rootHz > 0.0)
    {
        int harmonic = 0;
        for (double f : model.partialHz)
        {
            const double r = f / model.rootHz;
            const double n = std::round (r);
            if (n >= 1.0 && std::abs (r - n) < 0.03 * n)
                ++harmonic;
        }
        if (harmonic < 0.5 * (double) model.partialHz.size())
            model.rootHz = 0.0; // unpitched: play back at the sample's own pitch
    }

    // 6. Amplitude envelope of every partial over time (max of the peak bin and its neighbours).
    const int stride = juce::jmax (1, (numFrames + maxEnvelopeFrames - 1) / maxEnvelopeFrames);
    const int envFrames = (numFrames + stride - 1) / stride;
    model.frameSeconds = hopSize * (double) stride / sampleRate;
    model.envelopes.assign (peaks.size(), std::vector<float> ((size_t) envFrames, 0.0f));

    double sumOfMaxima = 0.0;
    for (size_t pi = 0; pi < peaks.size(); ++pi)
    {
        const int bin = peaks[pi].bin;
        auto& env = model.envelopes[pi];
        float envMax = 0.0f;
        for (int e = 0; e < envFrames; ++e)
        {
            const auto& fr = mags[(size_t) (e * stride)];
            env[(size_t) e] = juce::jmax (fr[(size_t) bin - 1], fr[(size_t) bin], fr[(size_t) bin + 1]);
            envMax = std::max (envMax, env[(size_t) e]);
        }
        sumOfMaxima += envMax;
    }

    // Keep the relative balance between partials, normalise the total to 1.
    if (sumOfMaxima > 0.0)
        for (auto& env : model.envelopes)
            for (auto& v : env)
                v = (float) (v / sumOfMaxima);

    return model;
}
