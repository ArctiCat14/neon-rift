#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

/** Dark cyberpunk styling: neon arcs, violet panels, glowing accents. */
class NeonRiftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    static inline const juce::Colour background { 0xff07060f };
    static inline const juce::Colour panel      { 0xff120f1e };
    static inline const juce::Colour panelEdge  { 0xff2a2340 };
    static inline const juce::Colour cyan       { 0xff2ee6ff };
    static inline const juce::Colour magenta    { 0xffff3fa4 };
    static inline const juce::Colour text       { 0xffe8e6ff };
    static inline const juce::Colour dim        { 0xff8d87b0 };

    NeonRiftLookAndFeel()
    {
        setColour (juce::ResizableWindow::backgroundColourId, background);

        setColour (juce::Slider::rotarySliderFillColourId,    cyan);
        setColour (juce::Slider::rotarySliderOutlineColourId, panelEdge);
        setColour (juce::Slider::textBoxTextColourId,         text);
        setColour (juce::Slider::textBoxBackgroundColourId,   juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId,      juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId,    cyan.withAlpha (0.25f));

        setColour (juce::Label::textColourId, text);

        setColour (juce::ComboBox::backgroundColourId, panel.brighter (0.06f));
        setColour (juce::ComboBox::textColourId,       text);
        setColour (juce::ComboBox::outlineColourId,    panelEdge);
        setColour (juce::ComboBox::arrowColourId,      cyan);

        setColour (juce::PopupMenu::backgroundColourId,            panel.brighter (0.05f));
        setColour (juce::PopupMenu::textColourId,                  text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, cyan.withAlpha (0.25f));
        setColour (juce::PopupMenu::highlightedTextColourId,       juce::Colours::white);

        setColour (juce::MidiKeyboardComponent::whiteNoteColourId,         juce::Colour (0xffd9d6ef));
        setColour (juce::MidiKeyboardComponent::blackNoteColourId,         juce::Colour (0xff15121f));
        setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId,  juce::Colour (0xff0a0812));
        setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, cyan.withAlpha (0.35f));
        setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId,    magenta.withAlpha (0.65f));
    }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override
    {
        juce::ignoreUnused (slider);

        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (5.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);
        const float lineW = juce::jmax (2.0f, radius * 0.13f);

        // recessed knob body
        const float bodyR = radius - lineW * 1.9f;
        g.setColour (juce::Colour (0xff0d0a18));
        g.fillEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);
        g.setColour (panel.brighter (0.10f));
        g.drawEllipse (centre.x - bodyR, centre.y - bodyR, bodyR * 2.0f, bodyR * 2.0f, 1.0f);

        // outer value track
        juce::Path track;
        track.addCentredArc (centre.x, centre.y, radius - lineW, radius - lineW,
                             0.0f, rotaryStartAngle, rotaryEndAngle, true);
        g.setColour (panelEdge);
        g.strokePath (track, juce::PathStrokeType (lineW * 0.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (sliderPos > 0.0001f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, radius - lineW, radius - lineW,
                                 0.0f, rotaryStartAngle, angle, true);

            // glow pass, then the crisp arc
            g.setColour (cyan.withAlpha (0.18f));
            g.strokePath (value, juce::PathStrokeType (lineW * 2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

            juce::ColourGradient grad (cyan, centre.x - radius, centre.y + radius,
                                       magenta, centre.x + radius, centre.y - radius, false);
            g.setGradientFill (grad);
            g.strokePath (value, juce::PathStrokeType (lineW, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        // pointer
        const juce::Point<float> tip (centre.x + std::sin (angle) * (bodyR - 1.5f),
                                      centre.y - std::cos (angle) * (bodyR - 1.5f));
        g.setColour (cyan.interpolatedWith (magenta, sliderPos * 0.5f));
        g.drawLine ({ centre, tip }, juce::jmax (1.6f, lineW * 0.65f));
    }
};
