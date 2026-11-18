#include "PluginEditor.h"

#include <algorithm>
#include <cmath>

namespace
{
    struct KnobSpec
    {
        const char* id;
        const char* label;
    };

    const KnobSpec topRow[] = {
        { "harmonics", "HARMONICS" }, { "click", "CLICK" }, { "tilt", "TILT" },
        { "morph", "MORPH" },         { "tune", "TUNE" },   { "glide", "GLIDE" }
    };

    const KnobSpec bottomRow[] = {
        { "attack", "ATTACK" },   { "decay", "DECAY" },     { "sustain", "SUSTAIN" }, { "release", "RELEASE" },
        { "hp", "HIGH-PASS" },    { "lp", "LOW-PASS" },     { "reverb", "REVERB" },   { "room", "ROOM" }
    };

    void drawPartials (juce::Graphics& g, const ResynthModel* model, juce::Rectangle<float> area, juce::Colour colour)
    {
        if (model == nullptr || model->partialHz.empty() || model->envelopes.empty())
            return;

        std::vector<float> maxima;
        float peakAmp = 0.0f;
        for (const auto& env : model->envelopes)
        {
            const float m = *std::max_element (env.begin(), env.end());
            maxima.push_back (m);
            peakAmp = std::max (peakAmp, m);
        }
        if (peakAmp <= 0.0f)
            return;

        // Logarithmic frequency axis, 30 Hz to 12 kHz.
        const float lo = std::log2 (30.0f);
        const float hi = std::log2 (12000.0f);

        g.setColour (colour);
        for (size_t k = 0; k < model->partialHz.size() && k < maxima.size(); ++k)
        {
            const float f = (float) juce::jmax (30.0, model->partialHz[k]);
            const float x = area.getX() + area.getWidth() * (std::log2 (f) - lo) / (hi - lo);
            const float h = area.getHeight() * maxima[k] / peakAmp;
            g.fillRect (x - 1.5f, area.getBottom() - h, 3.0f, h);
        }
    }
}

void ModelView::setModels (std::shared_ptr<const ResynthModel> a, std::shared_ptr<const ResynthModel> b)
{
    modelA = std::move (a);
    modelB = std::move (b);
    repaint();
}

void ModelView::paint (juce::Graphics& g)
{
    using namespace KamustColours;
    const auto area = getLocalBounds().toFloat().reduced (3.0f);

    g.setColour (juce::Colour (0xff0c1514));
    g.fillEllipse (area);
    g.setColour (brass);
    g.drawEllipse (area, 4.0f);

    const auto plot = area.reduced (18.0f, 30.0f);
    drawPartials (g, modelA.get(), plot, brass);
    drawPartials (g, modelB.get(), plot, teal.brighter (0.6f));

    if (modelA == nullptr)
    {
        g.setColour (cream.withAlpha (0.5f));
        g.setFont (juce::Font (12.0f, juce::Font::bold));
        g.drawText ("NO SOURCE", area, juce::Justification::centred, false);
    }
}

KamustIAEditor::KamustIAEditor (KamustIAProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p)
{
    setLookAndFeel (&lookAndFeel);
    setSize (760, 490);

    addAndMakeVisible (modelView);

    for (const auto& spec : topRow)
        addKnob (spec.id, spec.label);
    for (const auto& spec : bottomRow)
        addKnob (spec.id, spec.label);

    slotALabel.setFont (juce::Font (14.0f, juce::Font::bold));
    slotBLabel.setFont (juce::Font (14.0f, juce::Font::bold));
    statusLabel.setFont (juce::Font (12.0f));
    addAndMakeVisible (slotALabel);
    addAndMakeVisible (slotBLabel);
    addAndMakeVisible (statusLabel);

    loadAButton.setButtonText ("LOAD A");
    loadBButton.setButtonText ("LOAD B");
    clearBButton.setButtonText ("CLEAR B");
    loadAButton.onClick = [this] { openChooser (0); };
    loadBButton.onClick = [this] { openChooser (1); };
    clearBButton.onClick = [this]
    {
        processor.clearSample (1);
        refreshSlots();
    };
    addAndMakeVisible (loadAButton);
    addAndMakeVisible (loadBButton);
    addAndMakeVisible (clearBButton);

    statusLabel.setText ("Drop a one-shot onto the left half (A) or right half (B), or use LOAD.",
                         juce::dontSendNotification);
    refreshSlots();
}

KamustIAEditor::~KamustIAEditor()
{
    setLookAndFeel (nullptr);
}

void KamustIAEditor::addKnob (const char* paramId, const juce::String& text)
{
    auto knob = std::make_unique<Knob>();

    knob->slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    knob->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 14);

    knob->label.setText (text, juce::dontSendNotification);
    knob->label.setJustificationType (juce::Justification::centred);
    knob->label.setFont (juce::Font (11.0f, juce::Font::bold));

    knob->attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.apvts, paramId, knob->slider);

    addAndMakeVisible (knob->slider);
    addAndMakeVisible (knob->label);
    knobs.push_back (std::move (knob));
}

void KamustIAEditor::refreshSlots()
{
    const auto nameA = processor.getSampleName (0);
    const auto nameB = processor.getSampleName (1);

    slotALabel.setText ("A - SOURCE:  " + (nameA.isEmpty() ? juce::String ("empty") : nameA),
                        juce::dontSendNotification);
    slotBLabel.setText ("B - MORPH TARGET:  " + (nameB.isEmpty() ? juce::String ("optional") : nameB),
                        juce::dontSendNotification);

    modelView.setModels (processor.getModel (0), processor.getModel (1));
    clearBButton.setEnabled (processor.getModel (1) != nullptr);
}

void KamustIAEditor::loadFileIntoSlot (const juce::File& file, int slot)
{
    const juce::String slotName = slot == 0 ? "A" : "B";

    if (processor.loadSample (file, slot))
        statusLabel.setText ("Loaded " + file.getFileName() + " into " + slotName + ".", juce::dontSendNotification);
    else
        statusLabel.setText ("Could not analyse " + file.getFileName() + " (unsupported or silent file).",
                             juce::dontSendNotification);

    refreshSlots();
}

void KamustIAEditor::openChooser (int slot)
{
    chooser = std::make_unique<juce::FileChooser> ("Choose a one-shot sample", juce::File(),
                                                   "*.wav;*.aif;*.aiff;*.flac;*.ogg;*.mp3");

    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, slot] (const juce::FileChooser& fc)
                          {
                              const auto result = fc.getResult();
                              if (result.existsAsFile())
                                  loadFileIntoSlot (result, slot);
                          });
}

bool KamustIAEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    for (const auto& f : files)
        if (juce::File (f).hasFileExtension ("wav;aif;aiff;flac;ogg;mp3"))
            return true;
    return false;
}

void KamustIAEditor::filesDropped (const juce::StringArray& files, int x, int y)
{
    (void) y;
    if (files.isEmpty())
        return;

    const int slot = x < getWidth() / 2 ? 0 : 1;
    loadFileIntoSlot (juce::File (files[0]), slot);
}

void KamustIAEditor::paint (juce::Graphics& g)
{
    using namespace KamustColours;
    const auto bounds = getLocalBounds().toFloat();

    g.setGradientFill (juce::ColourGradient (teal.darker (0.6f), 0.0f, 0.0f,
                                             rust, bounds.getWidth(), bounds.getHeight(), false));
    g.fillRect (bounds);

    // Title plate
    const juce::Rectangle<float> title (20.0f, 14.0f, 720.0f, 40.0f);
    g.setColour (face);
    g.fillRoundedRectangle (title, 8.0f);
    g.setColour (brass);
    g.drawRoundedRectangle (title, 8.0f, 2.0f);
    g.setFont (juce::Font (26.0f, juce::Font::bold));
    g.drawText ("KAMUST IA", title, juce::Justification::centred, false);

    // Sample slot panel
    const juce::Rectangle<float> slotPanel (214.0f, 66.0f, 522.0f, 170.0f);
    g.setColour (face.withAlpha (0.85f));
    g.fillRoundedRectangle (slotPanel, 10.0f);
    g.setColour (brass);
    g.drawRoundedRectangle (slotPanel, 10.0f, 1.5f);

    // Knob panels
    const juce::Rectangle<float> panelTop (12.0f, 246.0f, 736.0f, 118.0f);
    const juce::Rectangle<float> panelBottom (12.0f, 368.0f, 736.0f, 116.0f);
    for (auto panel : { panelTop, panelBottom })
    {
        g.setColour (face.withAlpha (0.7f));
        g.fillRoundedRectangle (panel, 10.0f);
        g.setColour (brass.withAlpha (0.6f));
        g.drawRoundedRectangle (panel, 10.0f, 1.0f);
    }

    // Corner rivets
    g.setColour (brass.darker (0.2f));
    for (auto corner : { juce::Point<float> (8.0f, 8.0f), juce::Point<float> (752.0f, 8.0f),
                         juce::Point<float> (8.0f, 482.0f), juce::Point<float> (752.0f, 482.0f) })
        g.fillEllipse (corner.x - 3.0f, corner.y - 3.0f, 6.0f, 6.0f);
}

void KamustIAEditor::resized()
{
    modelView.setBounds (24, 66, 170, 170);

    slotALabel.setBounds (234, 74, 500, 22);
    slotBLabel.setBounds (234, 100, 500, 22);
    loadAButton.setBounds (234, 136, 110, 28);
    loadBButton.setBounds (354, 136, 110, 28);
    clearBButton.setBounds (474, 136, 110, 28);
    statusLabel.setBounds (234, 176, 500, 52);

    for (size_t i = 0; i < knobs.size(); ++i)
    {
        juce::Rectangle<int> cell;
        if (i < 6)
            cell = juce::Rectangle<int> (20 + (int) i * 120, 256, 120, 110);
        else
            cell = juce::Rectangle<int> (20 + (int) (i - 6) * 90, 374, 90, 110);

        auto& knob = *knobs[i];
        knob.label.setBounds (cell.removeFromTop (16));
        knob.slider.setBounds (cell);
    }
}
