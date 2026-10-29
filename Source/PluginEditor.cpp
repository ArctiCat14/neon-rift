#include "PluginEditor.h"

#include <cmath>

using namespace neonrift;

//==============================================================================
NeonRiftAudioProcessorEditor::Knob::Knob (juce::AudioProcessorValueTreeState& state,
                                          const char* paramID, const juce::String& caption,
                                          int decimals)
    : label ("caption", caption),
      attachment (state, paramID, slider)
{
    slider.setSliderStyle (juce::Slider::RotaryVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 60, 16);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f, true);
    slider.setPopupDisplayEnabled (true, false, nullptr);
    slider.setNumDecimalPlacesToDisplay (decimals);
    slider.textFromValueFunction = [decimals] (double v) { return juce::String (v, decimals); };
    slider.updateText();

    if (auto* param = state.getParameter (paramID))
        slider.setTooltip (param->getName (64));

    addAndMakeVisible (slider);

    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    label.setColour (juce::Label::textColourId, NeonRiftLookAndFeel::dim);
    label.setMinimumHorizontalScale (0.6f);
    addAndMakeVisible (label);
}

void NeonRiftAudioProcessorEditor::Knob::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    slider.setBounds (r);
    slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, juce::jmax (24, getWidth() - 8), 16);
}

//==============================================================================
NeonRiftAudioProcessorEditor::Box::Box (juce::AudioProcessorValueTreeState& state,
                                        const char* paramID, const juce::String& caption,
                                        const juce::StringArray& items)
    : label ("caption", caption)
{
    // Items must exist before the attachment tries to select one.
    combo.addItemList (items, 1);
    combo.setJustificationType (juce::Justification::centred);
    addAndMakeVisible (combo);

    if (auto* param = state.getParameter (paramID))
        combo.setTooltip (param->getName (64));

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (state, paramID, combo);

    label.setJustificationType (juce::Justification::centred);
    label.setFont (juce::Font (juce::FontOptions (9.5f).withStyle ("Bold")));
    label.setColour (juce::Label::textColourId, NeonRiftLookAndFeel::dim);
    addAndMakeVisible (label);
}

void NeonRiftAudioProcessorEditor::Box::resized()
{
    auto r = getLocalBounds();
    label.setBounds (r.removeFromTop (14));
    combo.setBounds (r.reduced (0, 2));
}

//==============================================================================
NeonRiftAudioProcessorEditor::NeonRiftAudioProcessorEditor (NeonRiftAudioProcessor& p)
    : AudioProcessorEditor (&p),
      processor (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lnf);

    osc1Knobs = { addKnob (pid::osc1Oct,    "OCTAVE", 0),
                  addKnob (pid::osc1Detune, "DETUNE", 0),
                  addKnob (pid::osc1Level,  "LEVEL",  2) };

    osc2Knobs = { addKnob (pid::osc2Oct,    "OCTAVE", 0),
                  addKnob (pid::osc2Detune, "DETUNE", 0),
                  addKnob (pid::osc2Level,  "LEVEL",  2) };

    filterKnobs = { addKnob (pid::cutoff,       "CUTOFF",  0),
                    addKnob (pid::resonance,    "RESONANCE", 2),
                    addKnob (pid::filterEnvAmt, "ENV AMT", 2),
                    addKnob (pid::noiseLevel,   "NOISE",   2) };

    lfoKnobs = { addKnob (pid::lfoRate,  "RATE",  2),
                 addKnob (pid::lfoDepth, "DEPTH", 2) };

    ampKnobs = { addKnob (pid::ampAttack,  "ATTACK",  2),
                 addKnob (pid::ampDecay,   "DECAY",   2),
                 addKnob (pid::ampSustain, "SUSTAIN", 2),
                 addKnob (pid::ampRelease, "RELEASE", 2) };

    filtKnobs = { addKnob (pid::filtAttack,  "ATTACK",  2),
                  addKnob (pid::filtDecay,   "DECAY",   2),
                  addKnob (pid::filtSustain, "SUSTAIN", 2),
                  addKnob (pid::filtRelease, "RELEASE", 2) };

    fxKnobs = { addKnob (pid::drive,      "DRIVE",    2),
                addKnob (pid::chorusMix,  "CHORUS",   2),
                addKnob (pid::delayTime,  "DLY TIME", 2),
                addKnob (pid::delayFb,    "DLY FB",   2),
                addKnob (pid::delayMix,   "DLY MIX",  2),
                addKnob (pid::reverbSize, "REV SIZE", 2),
                addKnob (pid::reverbMix,  "REV MIX",  2) };

    masterKnob = addKnob (pid::masterGain, "MASTER", 1);

    osc1Wave     = addBox (pid::osc1Wave,  "WAVE",   oscWaveNames());
    osc2Wave     = addBox (pid::osc2Wave,  "WAVE",   oscWaveNames());
    lfoTargetBox = addBox (pid::lfoTarget, "TARGET", lfoTargetNames());

    keyboard.setAvailableRange (36, 96);
    keyboard.setKeyWidth (24.0f);
    keyboard.setScrollButtonsVisible (false);
    keyboard.setLowestVisibleKey (36);
    keyboard.setMouseClickGrabsKeyboardFocus (false);
    addAndMakeVisible (keyboard);

    voiceLabel.setText ("VOICES 0 / 16", juce::dontSendNotification);
    voiceLabel.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    voiceLabel.setColour (juce::Label::textColourId, NeonRiftLookAndFeel::cyan.withAlpha (0.85f));
    voiceLabel.setJustificationType (juce::Justification::centredRight);
    addAndMakeVisible (voiceLabel);

    startTimerHz (20);

    // Resize only once every child exists: setSize() runs resized() immediately.
    setSize (1000, 700);
}

NeonRiftAudioProcessorEditor::~NeonRiftAudioProcessorEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

//==============================================================================
NeonRiftAudioProcessorEditor::Knob* NeonRiftAudioProcessorEditor::addKnob (const char* paramID,
                                                                          const juce::String& caption,
                                                                          int decimals)
{
    ownedKnobs.push_back (std::make_unique<Knob> (processor.apvts, paramID, caption, decimals));
    addAndMakeVisible (*ownedKnobs.back());
    return ownedKnobs.back().get();
}

NeonRiftAudioProcessorEditor::Box* NeonRiftAudioProcessorEditor::addBox (const char* paramID,
                                                                        const juce::String& caption,
                                                                        const juce::StringArray& items)
{
    ownedBoxes.push_back (std::make_unique<Box> (processor.apvts, paramID, caption, items));
    addAndMakeVisible (*ownedBoxes.back());
    return ownedBoxes.back().get();
}

void NeonRiftAudioProcessorEditor::layoutRow (juce::Rectangle<int> area,
                                              const std::vector<Knob*>& knobs) const
{
    if (knobs.empty())
        return;

    const int w = area.getWidth() / (int) knobs.size();

    for (auto* k : knobs)
    {
        k->setBounds (area.removeFromLeft (w));
        area.removeFromLeft (2);
    }
}

//==============================================================================
void NeonRiftAudioProcessorEditor::resized()
{
    // Hosts (and our own constructor) can call this before the children exist.
    if (masterKnob == nullptr || osc1Wave == nullptr || osc2Wave == nullptr || lfoTargetBox == nullptr)
        return;

    auto r = getLocalBounds();
    r.removeFromTop (70);

    auto footer = r.removeFromBottom (124);
    r.removeFromBottom (6);
    r.reduce (14, 0);

    auto topRow = r.removeFromTop (210);
    r.removeFromTop (10);
    auto bottomRow = r;

    juce::Rectangle<int> pOsc1, pOsc2, pFilter, pLfo, pAmp, pFilt, pFx;

    {
        auto t = topRow;
        pOsc1   = t.removeFromLeft (222); t.removeFromLeft (10);
        pOsc2   = t.removeFromLeft (222); t.removeFromLeft (10);
        pFilter = t.removeFromLeft (300); t.removeFromLeft (10);
        pLfo    = t;
    }
    {
        auto t = bottomRow;
        pAmp  = t.removeFromLeft (300); t.removeFromLeft (10);
        pFilt = t.removeFromLeft (300); t.removeFromLeft (10);
        pFx   = t;
    }

    panels = { { pOsc1,   "OSC 1",           NeonRiftLookAndFeel::cyan },
               { pOsc2,   "OSC 2",           NeonRiftLookAndFeel::magenta },
               { pFilter, "FILTER",          NeonRiftLookAndFeel::cyan },
               { pLfo,    "LFO",             NeonRiftLookAndFeel::magenta },
               { pAmp,    "AMP ENVELOPE",    NeonRiftLookAndFeel::cyan },
               { pFilt,   "FILTER ENVELOPE", NeonRiftLookAndFeel::magenta },
               { pFx,     "EFFECTS",         NeonRiftLookAndFeel::cyan } };

    auto content = [] (juce::Rectangle<int> panel)
    {
        return panel.reduced (12).withTrimmedTop (28);
    };

    {
        auto c = content (pOsc1);
        layoutRow (c.removeFromTop (108), osc1Knobs);
        osc1Wave->setBounds (c.getX() + (c.getWidth() - 130) / 2, c.getBottom() - 34, 130, 34);
    }
    {
        auto c = content (pOsc2);
        layoutRow (c.removeFromTop (108), osc2Knobs);
        osc2Wave->setBounds (c.getX() + (c.getWidth() - 130) / 2, c.getBottom() - 34, 130, 34);
    }
    {
        auto c = content (pFilter);
        layoutRow (c.removeFromTop (100), filterKnobs);
    }
    {
        auto c = content (pLfo);
        layoutRow (c.removeFromTop (108), lfoKnobs);
        lfoTargetBox->setBounds (c.getX() + (c.getWidth() - 130) / 2, c.getBottom() - 34, 130, 34);
    }
    {
        auto c = content (pAmp);
        layoutRow (c.removeFromTop (112), ampKnobs);
        ampCurveArea = c.reduced (2, 2);
    }
    {
        auto c = content (pFilt);
        layoutRow (c.removeFromTop (112), filtKnobs);
        filtCurveArea = c.reduced (2, 2);
    }
    {
        auto c = content (pFx);
        auto row1 = c.removeFromTop (102);
        c.removeFromTop (4);
        auto row2 = c.removeFromTop (102);

        layoutRow (row1, { fxKnobs[0], fxKnobs[1], fxKnobs[2], fxKnobs[3] });
        layoutRow (row2, { fxKnobs[4], fxKnobs[5], fxKnobs[6] });
    }

    masterKnob->setBounds (footer.removeFromRight (100).withTrimmedTop (2).withTrimmedBottom (8));
    keyboard.setBounds (footer.reduced (0, 2).withTrimmedRight (8));

    keyboard.setOctaveForMiddleC (4);

    voiceLabel.setBounds (getWidth() - 270, 26, 250, 18);
}

//==============================================================================
void NeonRiftAudioProcessorEditor::drawPanel (juce::Graphics& g, const Panel& panel) const
{
    const auto bounds = panel.bounds.toFloat();

    g.setColour (NeonRiftLookAndFeel::panel.withAlpha (0.92f));
    g.fillRoundedRectangle (bounds, 10.0f);

    g.setColour (panel.accent.withAlpha (0.10f));
    g.drawRoundedRectangle (bounds.reduced (1.0f), 10.0f, 1.6f);

    g.setColour (NeonRiftLookAndFeel::panelEdge);
    g.drawRoundedRectangle (bounds.reduced (0.5f), 10.0f, 1.0f);

    auto titleArea = panel.bounds.reduced (14, 0).removeFromTop (36).withTrimmedTop (11);

    g.setColour (panel.accent);
    g.setFont (juce::Font (juce::FontOptions (10.5f).withStyle ("Bold")));
    g.drawText (panel.title, titleArea, juce::Justification::centredLeft);

    g.setColour (panel.accent.withAlpha (0.65f));
    g.fillRect (titleArea.getX(), titleArea.getBottom() - 2, 22, 2);

    const float cx = (float) bounds.getRight() - 18.0f;
    const float cy = (float) bounds.getY() + 18.0f;
    g.setColour (panel.accent.withAlpha (0.5f));
    g.fillEllipse (cx, cy, 3.0f, 3.0f);
}

void NeonRiftAudioProcessorEditor::drawEnvelope (juce::Graphics& g, juce::Rectangle<int> area,
                                                 float a, float d, float s, float r,
                                                 juce::Colour colour) const
{
    if (area.getWidth() < 20 || area.getHeight() < 20)
        return;

    auto box = area.toFloat().reduced (6.0f, 4.0f);

    g.setColour (NeonRiftLookAndFeel::panelEdge.withAlpha (0.55f));
    for (int i = 0; i <= 3; ++i)
    {
        const float y = box.getY() + box.getHeight() * (float) i / 3.0f;
        g.drawHorizontalLine ((int) y, box.getX(), box.getRight());
    }

    auto norm = [] (float t) { return std::sqrt (juce::jlimit (0.001f, 6.0f, t) / 6.0f); };

    const float wA = 0.06f + 0.28f * norm (a);
    const float wD = 0.05f + 0.22f * norm (d);
    const float wS = 0.20f;
    const float wR = 0.05f + 0.24f * norm (r);
    const float total = wA + wD + wS + wR;
    const float scale = box.getWidth() / total;

    const float x0 = box.getX();
    const float yb = box.getBottom();
    const float yt = box.getY() + 4.0f;
    const float ys = yb - s * (yb - yt);

    juce::Path curve;
    curve.startNewSubPath (x0, yb);
    curve.lineTo (x0 + wA * scale, yt);
    curve.lineTo (x0 + (wA + wD) * scale, ys);
    curve.lineTo (x0 + (wA + wD + wS) * scale, ys);
    curve.lineTo (x0 + total * scale, yb);

    juce::Path filled (curve);
    filled.lineTo (box.getRight(), yb);
    filled.closeSubPath();

    g.setColour (colour.withAlpha (0.10f));
    g.fillPath (filled);

    g.setColour (colour.withAlpha (0.25f));
    g.strokePath (curve, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

    g.setColour (colour);
    g.strokePath (curve, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
}

//==============================================================================
void NeonRiftAudioProcessorEditor::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat();

    juce::ColourGradient bg (juce::Colour (0xff0a0817), 0.0f, 0.0f,
                             juce::Colour (0xff180a24), b.getWidth(), b.getHeight(), false);
    g.setGradientFill (bg);
    g.fillRect (b);

    g.setColour (juce::Colours::white.withAlpha (0.022f));
    for (int x = 0; x < getWidth(); x += 34)
        g.drawVerticalLine (x, 0.0f, (float) getHeight());
    for (int y = 0; y < getHeight(); y += 34)
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());

    juce::ColourGradient titleGrad (NeonRiftLookAndFeel::cyan, 20.0f, 12.0f,
                                    NeonRiftLookAndFeel::magenta, 330.0f, 50.0f, false);
    g.setGradientFill (titleGrad);
    g.setFont (juce::Font (juce::FontOptions (36.0f).withStyle ("Bold")));
    g.drawText ("NEONRIFT", 20, 6, 430, 44, juce::Justification::centredLeft);

    g.setColour (NeonRiftLookAndFeel::dim);
    g.setFont (juce::Font (juce::FontOptions (9.5f)));
    g.drawText ("POLYPHONIC SYNTHESIZER   //   V1.0", 22, 48, 430, 14,
                juce::Justification::centredLeft);

    g.setColour (NeonRiftLookAndFeel::cyan.withAlpha (0.35f));
    g.fillRect (20.0f, 68.0f, (float) getWidth() - 40.0f, 1.0f);

    for (const auto& p : panels)
        drawPanel (g, p);

    auto value = [this] (const char* id) -> float
    {
        if (auto* p = processor.apvts.getRawParameterValue (id))
            return p->load();

        return 0.0f;
    };

    drawEnvelope (g, ampCurveArea,
                  value (pid::ampAttack), value (pid::ampDecay),
                  value (pid::ampSustain), value (pid::ampRelease),
                  NeonRiftLookAndFeel::cyan);

    drawEnvelope (g, filtCurveArea,
                  value (pid::filtAttack), value (pid::filtDecay),
                  value (pid::filtSustain), value (pid::filtRelease),
                  NeonRiftLookAndFeel::magenta);
}

//==============================================================================
void NeonRiftAudioProcessorEditor::timerCallback()
{
    const int active = processor.getSynth().activeVoices.load();

    if (active != lastVoiceCount)
    {
        lastVoiceCount = active;
        voiceLabel.setText ("VOICES " + juce::String (active) + " / 16",
                            juce::dontSendNotification);
    }
}
