#include "SynthEngine.h"

namespace neonrift
{

//==============================================================================
void NeonVoice::prepare (double sr)
{
    sampleRate = sr;
    ampEnv.setSampleRate (sr);
    filterEnv.setSampleRate (sr);
    reset();
}

void NeonVoice::reset()
{
    active = false;
    note = -1;
    velocity = 0.0f;
    osc1.reset();
    osc2.reset();
    filter.reset();
    ampEnv.reset();
    filterEnv.reset();
}

void NeonVoice::start (int midiNote, float vel, juce::uint64 order)
{
    note = midiNote;
    velocity = juce::jlimit (0.0f, 1.0f, vel);
    startOrder = order;
    active = true;

    baseFreq = juce::MidiMessage::getMidiNoteInHertz (midiNote);

    osc1.reset();
    osc2.reset();
    filter.reset();

    ampEnv.reset();
    filterEnv.reset();
    ampEnv.noteOn();
    filterEnv.noteOn();
}

void NeonVoice::release()
{
    ampEnv.noteOff();
    filterEnv.noteOff();
}

void NeonVoice::kill()
{
    active = false;
    ampEnv.reset();
    filterEnv.reset();
    filter.reset();
}

void NeonVoice::render (float* left, float* right, int numSamples, const SynthParams& p, const float* lfo)
{
    if (! active)
        return;

    ampEnv.setParameters ({ p.ampA, p.ampD, p.ampS, p.ampR });
    filterEnv.setParameters ({ p.fA, p.fD, p.fS, p.fR });

    const float q = 0.5f + p.resonance * 11.5f;
    const float maxHz = (float) sampleRate * 0.45f;
    float lastFilterEnv = 0.0f;

    for (int i = 0; i < numSamples; ++i)
    {
        if ((i & 15) == 0)
        {
            const float l = lfo[i];

            const float cutoffOct = (p.lfoTarget == 1 ? l * p.lfoDepth * 3.0f : 0.0f);
            const float octaves = p.filterEnvAmt * lastFilterEnv * 5.0f + cutoffOct;
            filter.setCutoff (juce::jmin (maxHz, p.cutoff * std::exp2 (octaves)), q, sampleRate);

            const float pitchMod = (p.lfoTarget == 2 ? l * p.lfoDepth * 2.0f : 0.0f);
            const double inc1 = baseFreq * std::exp2 ((p.osc1Oct * 12.0f + p.osc1Detune * 0.01f + pitchMod) / 12.0f) / sampleRate;
            const double inc2 = baseFreq * std::exp2 ((p.osc2Oct * 12.0f + p.osc2Detune * 0.01f + pitchMod) / 12.0f) / sampleRate;
            osc1.setIncrement (inc1);
            osc2.setIncrement (inc2);
        }

        const float filterEnvValue = filterEnv.getNextSample();
        lastFilterEnv = filterEnvValue;

        float mix = osc1.next (p.osc1Wave) * p.osc1Level
                  + osc2.next (p.osc2Wave) * p.osc2Level;

        if (p.noiseLevel > 0.0001f)
            mix += (random.nextFloat() * 2.0f - 1.0f) * p.noiseLevel;

        const float filtered = filter.process (mix);

        const float ampLfo = (p.lfoTarget == 3 ? juce::jlimit (0.0f, 2.0f, 1.0f + lfo[i] * p.lfoDepth) : 1.0f);
        const float amp = ampEnv.getNextSample();

        const float out = filtered * amp * velocity * ampLfo * 0.22f;

        left[i]  += out;
        right[i] += out;
    }

    if (! ampEnv.isActive())
        active = false;
}

//==============================================================================
void NeonSynth::StereoDelay::prepare (double sr, double maxSeconds, int maxBlockSize)
{
    const auto maxSamples = (int) (sr * maxSeconds) + 8;
    juce::dsp::ProcessSpec spec { sr, (juce::uint32) maxBlockSize, 1 };
    lineL.setMaximumDelayInSamples (maxSamples);
    lineR.setMaximumDelayInSamples (maxSamples);
    lineL.prepare (spec);
    lineR.prepare (spec);
    reset();
}

void NeonSynth::StereoDelay::reset()
{
    lineL.reset();
    lineR.reset();
}

void NeonSynth::StereoDelay::process (juce::AudioBuffer<float>& buffer, float delaySeconds,
                                      float feedback, float mix, double sr)
{
    if (mix <= 0.0001f || buffer.getNumSamples() <= 0)
        return;

    const int maxDelay = lineL.getMaximumDelayInSamples();
    const int d = juce::jlimit (1, juce::jmax (1, maxDelay - 2), (int) (delaySeconds * sr));
    const int dR = juce::jlimit (1, juce::jmax (1, maxDelay - 2), (int) (delaySeconds * 1.031 * sr));

    lineL.setDelay ((float) d);
    lineR.setDelay ((float) dR);

    auto* l = buffer.getWritePointer (0);
    auto* r = buffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float dl = lineL.popSample (0);
        const float dr = lineR.popSample (0);

        const float inL = l[i];
        const float inR = r[i];

        lineL.pushSample (0, inL + dr * feedback);
        lineR.pushSample (0, inR + dl * feedback);

        l[i] = inL * (1.0f - mix) + dl * mix;
        r[i] = inR * (1.0f - mix) + dr * mix;
    }
}

//==============================================================================
void NeonSynth::prepare (double sr, int maximumBlockSize, int numChannels)
{
    juce::ignoreUnused (numChannels);
    sampleRate = sr;
    voiceCounter = 0;

    for (auto& v : voices)
        v.prepare (sr);

    lfoBuffer.assign ((size_t) juce::jmax (1, maximumBlockSize), 0.0f);

    juce::dsp::ProcessSpec spec { sr, (juce::uint32) juce::jmax (32, maximumBlockSize), 2 };
    chorus.prepare (spec);
    chorus.reset();
    chorus.setRate (0.6f);
    chorus.setDepth (0.25f);
    chorus.setCentreDelay (12.0f);
    chorus.setFeedback (0.12f);
    chorus.setMix (0.0f);

    reverb.prepare (spec);
    reverb.reset();

    delay.prepare (sr, 2.0, juce::jmax (32, maximumBlockSize));

    masterGain.reset (sr, 0.02);
    masterGain.setCurrentAndTargetValue (juce::Decibels::decibelsToGain (params.masterGainDb));

    reset();
}

void NeonSynth::reset()
{
    for (auto& v : voices)
        v.reset();

    heldNotes.clear();
    sustainedNotes.clear();
    sustainDown = false;
    lfoPhase = 0.0;

    chorus.reset();
    reverb.reset();
    delay.reset();

    activeVoices.store (0);
}

NeonVoice& NeonSynth::findFreeVoice (int midiNote)
{
    for (auto& v : voices)
        if (v.isActive() && v.getNote() == midiNote)
            return v;

    for (auto& v : voices)
        if (! v.isActive())
            return v;

    auto* oldest = &voices[0];
    for (auto& v : voices)
        if (v.getStartOrder() < oldest->getStartOrder())
            oldest = &v;

    return *oldest;
}

void NeonSynth::noteOn (int midiNote, float velocity)
{
    if (midiNote < 0 || midiNote > 127)
        return;

    heldNotes.insert (midiNote);
    sustainedNotes.erase (midiNote);

    auto& v = findFreeVoice (midiNote);
    if (v.isActive())
        v.kill();

    v.start (midiNote, velocity, ++voiceCounter);
}

void NeonSynth::noteOff (int midiNote)
{
    heldNotes.erase (midiNote);

    if (sustainDown)
    {
        sustainedNotes.insert (midiNote);
        return;
    }

    stopNote (midiNote, true);
}

void NeonSynth::stopNote (int midiNote, bool allowTailOff)
{
    for (auto& v : voices)
    {
        if (v.isActive() && v.getNote() == midiNote)
        {
            if (allowTailOff)
                v.release();
            else
                v.kill();
        }
    }
}

void NeonSynth::sustainPedal (bool down)
{
    if (down == sustainDown)
        return;

    sustainDown = down;

    if (! down)
    {
        for (int n : sustainedNotes)
            if (heldNotes.find (n) == heldNotes.end())
                stopNote (n, true);

        sustainedNotes.clear();
    }
}

void NeonSynth::allNotesOff (bool immediate)
{
    heldNotes.clear();
    sustainedNotes.clear();

    for (auto& v : voices)
    {
        if (! v.isActive())
            continue;

        if (immediate)
            v.kill();
        else
            v.release();
    }
}

int NeonSynth::getActiveVoiceCount() const
{
    int n = 0;
    for (auto& v : voices)
        if (v.isActive())
            ++n;

    return n;
}

void NeonSynth::render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples)
{
    if (numSamples <= 0)
        return;

    auto* left  = buffer.getWritePointer (0, startSample);
    auto* right = buffer.getNumChannels() > 1 ? buffer.getWritePointer (1, startSample) : left;

    if ((int) lfoBuffer.size() < numSamples)
        lfoBuffer.assign ((size_t) numSamples, 0.0f);

    const double lfoInc = juce::jlimit (0.0, 0.49, (double) params.lfoRate / sampleRate);
    for (int i = 0; i < numSamples; ++i)
    {
        lfoBuffer[(size_t) i] = (float) std::sin (juce::MathConstants<double>::twoPi * lfoPhase);
        lfoPhase += lfoInc;
        if (lfoPhase >= 1.0)
            lfoPhase -= 1.0;
    }

    for (auto& v : voices)
        if (v.isActive())
            v.render (left, right, numSamples, params, lfoBuffer.data());

    activeVoices.store (getActiveVoiceCount());

    // ---- FX chain -----------------------------------------------------------
    if (params.drive > 0.0001f)
    {
        const float gain = 1.0f + params.drive * 9.0f;
        const float norm = 1.0f / std::tanh (gain);
        const float amount = params.drive;

        for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        {
            auto* d = buffer.getWritePointer (ch, startSample);
            for (int i = 0; i < numSamples; ++i)
            {
                const float x = d[i];
                const float shaped = std::tanh (x * gain) * norm;
                d[i] = x + (shaped - x) * amount;
            }
        }
    }

    if (params.chorusMix > 0.0001f)
    {
        chorus.setMix (params.chorusMix);
        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubBlock ((size_t) startSample, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        chorus.process (ctx);
    }

    delay.process (buffer, params.delayTime, params.delayFeedback, params.delayMix, sampleRate);

    if (params.reverbMix > 0.0001f)
    {
        reverb.setParameters ({ 0.15f + params.reverbSize * 0.8f, 0.45f, params.reverbMix, 1.0f - params.reverbMix * 0.5f, 1.0f, false });

        juce::dsp::AudioBlock<float> block (buffer);
        auto sub = block.getSubBlock ((size_t) startSample, (size_t) numSamples);
        juce::dsp::ProcessContextReplacing<float> ctx (sub);
        reverb.setEnabled (true);
        reverb.process (ctx);
    }
    else
    {
        reverb.setEnabled (false);
    }

    masterGain.setTargetValue (juce::Decibels::decibelsToGain (params.masterGainDb));

    for (int i = 0; i < numSamples; ++i)
    {
        const float g = masterGain.getNextValue();
        left[i] *= g;
        if (right != left)
            right[i] *= g;
    }
}
} // namespace neonrift
