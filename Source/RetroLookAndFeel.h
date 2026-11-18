#pragma once

#include <JuceHeader.h>

// Palette for a worn brass-and-rust look: aged brass, dark bronze face,
// cream lettering, amber value arcs and a verdigris-teal accent.
namespace KamustColours
{
    const juce::Colour brass { 0xffc89b4a };
    const juce::Colour cream { 0xfff1e4c3 };
    const juce::Colour face  { 0xff2a1d14 };
    const juce::Colour amber { 0xffe8a33d };
    const juce::Colour teal  { 0xff2f5f5a };
    const juce::Colour rust  { 0xff6e3a22 };
}

class RetroLookAndFeel : public juce::LookAndFeel_V4
{
public:
    RetroLookAndFeel()
    {
        using namespace KamustColours;
        setColour (juce::Label::textColourId, cream);
        setColour (juce::Slider::textBoxTextColourId, cream);
        setColour (juce::Slider::textBoxBackgroundColourId, face);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::TextButton::buttonColourId, brass);
        setColour (juce::TextButton::textColourOffId, face);
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float startAngle, float endAngle, juce::Slider&) override
    {
        using namespace KamustColours;

        const auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height).reduced (4.0f);
        const float diameter = juce::jmin (bounds.getWidth(), bounds.getHeight());
        const auto centre = bounds.getCentre();
        const float radius = diameter * 0.5f;
        const float angle = startAngle + sliderPos * (endAngle - startAngle);

        // Drop shadow
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        g.fillEllipse (centre.x - radius + 2.0f, centre.y - radius + 4.0f, diameter, diameter);

        // Brass rim
        juce::ColourGradient rim (brass.brighter (0.4f), centre.x - radius, centre.y - radius,
                                  brass.darker (0.6f), centre.x + radius, centre.y + radius, true);
        g.setGradientFill (rim);
        g.fillEllipse (centre.x - radius, centre.y - radius, diameter, diameter);

        // Dark bronze face
        const float faceRadius = radius * 0.78f;
        g.setColour (face);
        g.fillEllipse (centre.x - faceRadius, centre.y - faceRadius, faceRadius * 2.0f, faceRadius * 2.0f);

        // Track and value arc
        const float arcRadius = radius * 0.9f;
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.strokePath (track, juce::PathStrokeType (3.0f));

        juce::Path value;
        value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, angle, true);
        g.setColour (amber);
        g.strokePath (value, juce::PathStrokeType (3.0f));

        // Pointer
        juce::Path pointer;
        pointer.addRoundedRectangle (-1.5f, -faceRadius * 0.85f, 3.0f, faceRadius * 0.6f, 1.0f);
        g.setColour (cream);
        g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (centre.x, centre.y));

        // Centre rivet
        g.setColour (brass.darker (0.3f));
        g.fillEllipse (centre.x - 2.5f, centre.y - 2.5f, 5.0f, 5.0f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour&,
                               bool highlighted, bool down) override
    {
        using namespace KamustColours;
        const auto r = button.getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (down ? brass.darker (0.3f) : (highlighted ? brass.brighter (0.1f) : brass));
        g.fillRoundedRectangle (r, 6.0f);
        g.setColour (face);
        g.drawRoundedRectangle (r, 6.0f, 1.5f);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool) override
    {
        g.setColour (KamustColours::face);
        g.setFont (juce::Font (13.0f, juce::Font::bold));
        g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
    }
};
