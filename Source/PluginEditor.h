#pragma once

#include <JuceHeader.h>
#include <memory>
#include <vector>
#include "PluginProcessor.h"
#include "RetroLookAndFeel.h"

// Porthole-style display showing the partials of model A (brass) and B (teal).
class ModelView : public juce::Component
{
public:
    void setModels (std::shared_ptr<const ResynthModel> a, std::shared_ptr<const ResynthModel> b);
    void paint (juce::Graphics& g) override;

private:
    std::shared_ptr<const ResynthModel> modelA;
    std::shared_ptr<const ResynthModel> modelB;
};

class KamustIAEditor : public juce::AudioProcessorEditor,
                       public juce::FileDragAndDropTarget
{
public:
    explicit KamustIAEditor (KamustIAProcessor&);
    ~KamustIAEditor() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray& files) override;
    void filesDropped (const juce::StringArray& files, int x, int y) override;

private:
    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;
    };

    void addKnob (const char* paramId, const juce::String& text);
    void loadFileIntoSlot (const juce::File& file, int slot);
    void openChooser (int slot);
    void refreshSlots();

    KamustIAProcessor& processor;
    RetroLookAndFeel lookAndFeel;
    ModelView modelView;
    std::vector<std::unique_ptr<Knob>> knobs;
    juce::Label slotALabel;
    juce::Label slotBLabel;
    juce::Label statusLabel;
    juce::TextButton loadAButton;
    juce::TextButton loadBButton;
    juce::TextButton clearBButton;
    std::unique_ptr<juce::FileChooser> chooser;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KamustIAEditor)
};
