#include "PluginProcessor.h"
#include "PluginEditor.h"

#include <cstring>

KamustIAProcessor::KamustIAProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "Parameters", createParameterLayout())
{
    formatManager.registerBasicFormats();

    synth.addSound (new ResynthSound());
    for (int i = 0; i < 8; ++i)
        synth.addVoice (new ResynthVoice (shared));
    synth.setNoteStealingEnabled (true);
}

KamustIAProcessor::~KamustIAProcessor() = default;

juce::AudioProcessorValueTreeState::ParameterLayout KamustIAProcessor::createParameterLayout()
{
    using FloatParam = juce::AudioParameterFloat;
    using Range = juce::NormalisableRange<float>;

    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    auto add = [&layout] (const char* id, const char* name, Range range, float defaultValue)
    {
        layout.add (std::make_unique<FloatParam> (juce::ParameterID { id, 1 }, name, range, defaultValue));
    };

    add ("harmonics", "Harmonics", Range (0.0f, 1.0f), 1.0f);
    add ("click",     "Click",     Range (0.0f, 1.0f), 0.3f);
    add ("tilt",      "Tilt",      Range (-1.0f, 1.0f), 0.0f);
    add ("morph",     "Morph",     Range (0.0f, 1.0f), 0.0f);
    add ("tune",      "Tune",      Range (-12.0f, 12.0f, 0.01f), 0.0f);
    add ("glide",     "Glide",     Range (0.0f, 1000.0f, 1.0f, 0.4f), 0.0f);

    add ("attack",  "Attack",  Range (0.001f, 2.0f, 0.0f, 0.4f), 0.005f);
    add ("decay",   "Decay",   Range (0.01f, 4.0f, 0.0f, 0.4f), 0.5f);
    add ("sustain", "Sustain", Range (0.0f, 1.0f), 1.0f);
    add ("release", "Release", Range (0.01f, 4.0f, 0.0f, 0.4f), 0.4f);

    add ("hp",     "High-pass", Range (20.0f, 1000.0f, 0.0f, 0.3f), 20.0f);
    add ("lp",     "Low-pass",  Range (500.0f, 20000.0f, 0.0f, 0.3f), 20000.0f);
    add ("reverb", "Reverb",    Range (0.0f, 1.0f), 0.15f);
    add ("room",   "Room",      Range (0.0f, 1.0f), 0.5f);

    return layout;
}

void KamustIAProcessor::prepareToPlay (double newSampleRate, int)
{
    sampleRate = newSampleRate;
    synth.setCurrentPlaybackSampleRate (newSampleRate);
    reverb.setSampleRate (newSampleRate);
    reverb.reset();
    lowState.fill (0.0f);
    highState.fill (0.0f);
}

bool KamustIAProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

float KamustIAProcessor::param (const char* id) const
{
    return apvts.getRawParameterValue (id)->load();
}

void KamustIAProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    buffer.clear();

    const int numSamples = buffer.getNumSamples();

    {
        const juce::SpinLock::ScopedLockType lock (modelLock);

        shared.modelA = models[0].get();
        shared.modelB = models[1].get();
        shared.harmonics = param ("harmonics");
        shared.click = param ("click");
        shared.tilt = param ("tilt");
        shared.morph = param ("morph");
        shared.transpose = (float) std::pow (2.0, param ("tune") / 12.0);

        const double glideSeconds = param ("glide") / 1000.0;
        shared.glideCoeff = glideSeconds <= 0.001
                                ? 1.0f
                                : (float) (1.0 - std::exp (-1.0 / (glideSeconds * sampleRate)));

        shared.adsr.attack = param ("attack");
        shared.adsr.decay = param ("decay");
        shared.adsr.sustain = param ("sustain");
        shared.adsr.release = param ("release");

        synth.renderNextBlock (buffer, midi, 0, numSamples);
    }

    // Output filters: one-pole low-pass, then one-pole high-pass.
    const double twoPiOverRate = juce::MathConstants<double>::twoPi / sampleRate;
    const float lpCoef = (float) (1.0 - std::exp (-twoPiOverRate * param ("lp")));
    const float hpCoef = (float) (1.0 - std::exp (-twoPiOverRate * param ("hp")));
    const int channels = juce::jmin (buffer.getNumChannels(), 2);

    for (int ch = 0; ch < channels; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        auto& lo = lowState[(size_t) ch];
        auto& hi = highState[(size_t) ch];

        for (int i = 0; i < numSamples; ++i)
        {
            lo += lpCoef * (data[i] - lo);
            hi += hpCoef * (lo - hi);
            data[i] = lo - hi;
        }
    }

    // Reverb (stereo only).
    const float reverbMix = param ("reverb");
    if (reverbMix > 0.001f && channels == 2)
    {
        juce::Reverb::Parameters rp;
        rp.roomSize = param ("room");
        rp.damping = 0.5f;
        rp.wetLevel = reverbMix;
        rp.dryLevel = 1.0f - 0.5f * reverbMix;
        rp.width = 1.0f;
        rp.freezeMode = 0.0f;
        reverb.setParameters (rp);
        reverb.processStereo (buffer.getWritePointer (0), buffer.getWritePointer (1), numSamples);
    }

    // Safety clip so the output never exceeds full scale.
    for (int ch = 0; ch < channels; ++ch)
    {
        float* data = buffer.getWritePointer (ch);
        for (int i = 0; i < numSamples; ++i)
            data[i] = juce::jlimit (-1.0f, 1.0f, data[i]);
    }
}

bool KamustIAProcessor::loadSample (const juce::File& file, int slot)
{
    std::unique_ptr<juce::AudioFormatReader> reader (formatManager.createReaderFor (file));
    if (reader == nullptr || reader->lengthInSamples <= 0 || reader->sampleRate <= 0.0)
        return false;

    const int numSamples = (int) juce::jmin (reader->lengthInSamples,
                                             (juce::int64) (12.0 * reader->sampleRate));
    juce::AudioBuffer<float> buffer ((int) reader->numChannels, numSamples);
    reader->read (&buffer, 0, numSamples, 0, true, true);

    return storeSample (std::move (buffer), reader->sampleRate, slot, file.getFileNameWithoutExtension());
}

bool KamustIAProcessor::storeSample (juce::AudioBuffer<float> source, double rate, int slot, const juce::String& name)
{
    auto model = std::make_shared<const ResynthModel> (analyseSample (source, rate));
    if (model->partialHz.empty())
        return false;

    const juce::SpinLock::ScopedLockType lock (modelLock);
    models[(size_t) slot] = std::move (model);
    sources[(size_t) slot] = std::move (source);
    sourceRates[(size_t) slot] = rate;
    sourceNames[(size_t) slot] = name;
    return true;
}

void KamustIAProcessor::clearSample (int slot)
{
    const juce::SpinLock::ScopedLockType lock (modelLock);
    models[(size_t) slot].reset();
    sources[(size_t) slot].setSize (0, 0);
    sourceRates[(size_t) slot] = 0.0;
    sourceNames[(size_t) slot] = {};
}

std::shared_ptr<const ResynthModel> KamustIAProcessor::getModel (int slot) const
{
    return models[(size_t) slot];
}

juce::String KamustIAProcessor::getSampleName (int slot) const
{
    return sourceNames[(size_t) slot];
}

void KamustIAProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();

    // Samples are stored inside the project so it opens on any machine.
    for (int slot = 0; slot < numSlots; ++slot)
    {
        if (models[(size_t) slot] == nullptr)
            continue;

        const auto& src = sources[(size_t) slot];
        const int channels = src.getNumChannels();
        const int n = src.getNumSamples();
        const auto key = juce::String (slot);

        juce::MemoryBlock raw ((size_t) channels * (size_t) n * sizeof (float));
        for (int ch = 0; ch < channels; ++ch)
            std::memcpy (static_cast<char*> (raw.getData()) + (size_t) ch * (size_t) n * sizeof (float),
                         src.getReadPointer (ch),
                         (size_t) n * sizeof (float));

        state.setProperty ("sample" + key, raw.toBase64Encoding(), nullptr);
        state.setProperty ("channels" + key, channels, nullptr);
        state.setProperty ("rate" + key, sourceRates[(size_t) slot], nullptr);
        state.setProperty ("name" + key, sourceNames[(size_t) slot], nullptr);
    }

    std::unique_ptr<juce::XmlElement> xml (state.createXml());
    if (xml != nullptr)
        copyXmlToBinary (*xml, destData);
}

void KamustIAProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr)
        return;

    const auto tree = juce::ValueTree::fromXml (*xml);
    if (! tree.isValid() || ! tree.hasType (apvts.state.getType()))
        return;

    apvts.replaceState (tree);

    for (int slot = 0; slot < numSlots; ++slot)
    {
        const auto key = juce::String (slot);
        const auto encoded = tree.getProperty ("sample" + key).toString();
        const int channels = (int) tree.getProperty ("channels" + key, 1);
        const double rate = (double) tree.getProperty ("rate" + key, 44100.0);

        if (encoded.isEmpty() || channels <= 0 || rate <= 0.0)
            continue;

        juce::MemoryBlock raw;
        raw.fromBase64Encoding (encoded);

        const int n = (int) (raw.getSize() / ((size_t) channels * sizeof (float)));
        if (n <= 0)
            continue;

        juce::AudioBuffer<float> buffer (channels, n);
        for (int ch = 0; ch < channels; ++ch)
            std::memcpy (buffer.getWritePointer (ch),
                         static_cast<const char*> (raw.getData()) + (size_t) ch * (size_t) n * sizeof (float),
                         (size_t) n * sizeof (float));

        storeSample (std::move (buffer), rate, slot, tree.getProperty ("name" + key).toString());
    }
}

juce::AudioProcessorEditor* KamustIAProcessor::createEditor()
{
    return new KamustIAEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KamustIAProcessor();
}
