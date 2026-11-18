#pragma once

#include <JuceHeader.h>
#include <array>
#include <memory>
#include "ResynthModel.h"
#include "ResynthVoice.h"

class KamustIAProcessor : public juce::AudioProcessor
{
public:
    static constexpr int numSlots = 2;  // slot 0 = source A, slot 1 = morph target B

    KamustIAProcessor();
    ~KamustIAProcessor() override;

    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;
    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Kamust IA"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    bool isMidiEffect() const override { return false; }
    double getTailLengthSeconds() const override { return 4.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

    // Sample management. Call from the message thread only.
    bool loadSample (const juce::File& file, int slot);
    void clearSample (int slot);
    std::shared_ptr<const ResynthModel> getModel (int slot) const;
    juce::String getSampleName (int slot) const;

    juce::AudioProcessorValueTreeState apvts;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout();
    bool storeSample (juce::AudioBuffer<float> source, double sampleRate, int slot, const juce::String& name);
    float param (const char* id) const;

    juce::AudioFormatManager formatManager;
    SharedVoiceState shared;
    juce::Synthesiser synth;
    juce::Reverb reverb;

    // Written only on the message thread. The audio thread reads it under modelLock.
    juce::SpinLock modelLock;
    std::array<std::shared_ptr<const ResynthModel>, numSlots> models;
    std::array<juce::AudioBuffer<float>, numSlots> sources;   // kept so the project can be saved
    std::array<double, numSlots> sourceRates {};
    std::array<juce::String, numSlots> sourceNames;

    double sampleRate = 44100.0;
    std::array<float, 2> lowState {};
    std::array<float, 2> highState {};

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KamustIAProcessor)
};
