#include "PluginProcessor.h"
#include "PluginEditor.h"

using namespace neonrift;

//==============================================================================
NeonRiftAudioProcessor::NeonRiftAudioProcessor()
    : AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      apvts (*this, nullptr, "NeonRift", createParameterLayout())
{
}

//==============================================================================
void NeonRiftAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    synth.prepare (sampleRate, samplesPerBlock, 2);
    monoScratch.setSize (2, juce::jmax (32, samplesPerBlock), false, true, true);
}

void NeonRiftAudioProcessor::releaseResources()
{
    synth.reset();
}

bool NeonRiftAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    const auto& out = layouts.getMainOutputChannelSet();

    if (out == juce::AudioChannelSet::stereo())
        return true;

    return out == juce::AudioChannelSet::mono();
}

//==============================================================================
SynthParams NeonRiftAudioProcessor::readParams() const
{
    auto value = [this] (const char* id) -> float
    {
        if (auto* p = apvts.getRawParameterValue (id))
            return p->load (std::memory_order_relaxed);

        return 0.0f;
    };

    SynthParams p;
    p.osc1Wave   = juce::roundToInt (value (pid::osc1Wave));
    p.osc2Wave   = juce::roundToInt (value (pid::osc2Wave));
    p.lfoTarget  = juce::roundToInt (value (pid::lfoTarget));

    p.osc1Oct    = value (pid::osc1Oct);
    p.osc2Oct    = value (pid::osc2Oct);
    p.osc1Detune = value (pid::osc1Detune);
    p.osc2Detune = value (pid::osc2Detune);
    p.osc1Level  = value (pid::osc1Level);
    p.osc2Level  = value (pid::osc2Level);
    p.noiseLevel = value (pid::noiseLevel);

    p.cutoff       = value (pid::cutoff);
    p.resonance    = value (pid::resonance);
    p.filterEnvAmt = value (pid::filterEnvAmt);

    p.ampA = value (pid::ampAttack);
    p.ampD = value (pid::ampDecay);
    p.ampS = value (pid::ampSustain);
    p.ampR = value (pid::ampRelease);

    p.fA = value (pid::filtAttack);
    p.fD = value (pid::filtDecay);
    p.fS = value (pid::filtSustain);
    p.fR = value (pid::filtRelease);

    p.lfoRate  = value (pid::lfoRate);
    p.lfoDepth = value (pid::lfoDepth);

    p.drive          = value (pid::drive);
    p.chorusMix      = value (pid::chorusMix);
    p.delayTime      = value (pid::delayTime);
    p.delayFeedback  = value (pid::delayFb);
    p.delayMix       = value (pid::delayMix);
    p.reverbSize     = value (pid::reverbSize);
    p.reverbMix      = value (pid::reverbMix);

    p.masterGainDb = value (pid::masterGain);

    return p;
}

void NeonRiftAudioProcessor::handleMidi (const juce::MidiBuffer& midi)
{
    for (const auto metadata : midi)
    {
        const auto message = metadata.getMessage();

        if (message.isNoteOn())
            synth.noteOn (message.getNoteNumber(), message.getFloatVelocity());
        else if (message.isNoteOff())
            synth.noteOff (message.getNoteNumber());
        else if (message.isAllNotesOff() || message.isAllSoundOff())
            synth.allNotesOff (message.isAllSoundOff());
        else if (message.isSustainPedalOn())
            synth.sustainPedal (true);
        else if (message.isSustainPedalOff())
            synth.sustainPedal (false);
    }
}

void NeonRiftAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    buffer.clear();
    synth.setParams (readParams());
    keyboardState.processNextMidiBuffer (midi, 0, numSamples, true);
    handleMidi (midi);

    if (buffer.getNumChannels() >= 2)
    {
        synth.render (buffer, 0, numSamples);
    }
    else
    {
        monoScratch.setSize (2, juce::jmax (32, numSamples), false, false, true);
        monoScratch.clear();
        synth.render (monoScratch, 0, numSamples);

        const float* l = monoScratch.getReadPointer (0);
        const float* r = monoScratch.getReadPointer (1);
        auto* out = buffer.getWritePointer (0);

        for (int i = 0; i < numSamples; ++i)
            out[i] = 0.5f * (l[i] + r[i]);
    }
}

//==============================================================================
juce::AudioProcessorEditor* NeonRiftAudioProcessor::createEditor()
{
    return new NeonRiftAudioProcessorEditor (*this);
}

//==============================================================================
void NeonRiftAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    if (auto xml = apvts.copyState().createXml())
        copyXmlToBinary (*xml, destData);
}

void NeonRiftAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    if (auto xml = getXmlFromBinary (data, sizeInBytes))
    {
        auto tree = juce::ValueTree::fromXml (*xml);

        if (tree.hasType (apvts.state.getType()))
        {
            apvts.replaceState (tree);
            synth.allNotesOff (true);
        }
    }
}

//==============================================================================
juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new NeonRiftAudioProcessor();
}
