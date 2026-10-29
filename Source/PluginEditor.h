#pragma once

#include "PluginProcessor.h"
#include "NeonLookAndFeel.h"

class NeonRiftAudioProcessorEditor : public juce::AudioProcessorEditor,
                                     private juce::Timer
{
public:
    explicit NeonRiftAudioProcessorEditor (NeonRiftAudioProcessor&);
    ~NeonRiftAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void timerCallback() override;

private:
    struct Knob : juce::Component
    {
        Knob (juce::AudioProcessorValueTreeState& state, const char* paramID,
              const juce::String& caption, int decimals = 2);
        void resized() override;

        juce::Slider slider;
        juce::Label label;
        juce::AudioProcessorValueTreeState::SliderAttachment attachment;
    };

    struct Box : juce::Component
    {
        Box (juce::AudioProcessorValueTreeState& state, const char* paramID,
             const juce::String& caption, const juce::StringArray& items);
        void resized() override;

        juce::ComboBox combo;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment> attachment;
    };

    struct Panel
    {
        juce::Rectangle<int> bounds;
        juce::String title;
        juce::Colour accent;
    };

    Knob* addKnob (const char* paramID, const juce::String& caption, int decimals = 2);
    Box*  addBox  (const char* paramID, const juce::String& caption, const juce::StringArray& items);
    void  drawPanel (juce::Graphics&, const Panel&) const;
    void  drawEnvelope (juce::Graphics&, juce::Rectangle<int>, float a, float d, float s, float r,
                        juce::Colour) const;
    void  layoutRow (juce::Rectangle<int>, const std::vector<Knob*>&) const;

    NeonRiftAudioProcessor& processor;
    NeonRiftLookAndFeel lnf;
    juce::TooltipWindow tooltipWindow { this, 700 };

    std::vector<std::unique_ptr<Knob>> ownedKnobs;
    std::vector<std::unique_ptr<Box>> ownedBoxes;
    std::vector<Panel> panels;

    std::vector<Knob*> osc1Knobs, osc2Knobs, filterKnobs, lfoKnobs, ampKnobs, filtKnobs, fxKnobs;
    Box* osc1Wave = nullptr;
    Box* osc2Wave = nullptr;
    Box* lfoTargetBox = nullptr;
    Knob* masterKnob = nullptr;

    juce::Rectangle<int> ampCurveArea, filtCurveArea;

    juce::MidiKeyboardComponent keyboard;
    juce::Label voiceLabel;
    int lastVoiceCount = -1;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (NeonRiftAudioProcessorEditor)
};
