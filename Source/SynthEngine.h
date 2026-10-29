#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>

#include <array>
#include <cmath>
#include <set>
#include <vector>

namespace neonrift
{

/** Everything the DSP needs for one block, snapshotted from the parameter tree. */
struct SynthParams
{
    int   osc1Wave = 0, osc2Wave = 1, lfoTarget = 0;
    float osc1Oct = 0.0f, osc2Oct = 0.0f, osc1Detune = 0.0f, osc2Detune = 0.0f;
    float osc1Level = 0.6f, osc2Level = 0.35f, noiseLevel = 0.0f;

    float cutoff = 2200.0f, resonance = 0.25f, filterEnvAmt = 0.35f;

    float ampA = 0.01f, ampD = 0.35f, ampS = 0.75f, ampR = 0.45f;
    float fA = 0.01f, fD = 0.5f, fS = 0.3f, fR = 0.5f;

    float lfoRate = 1.0f, lfoDepth = 0.0f;

    float drive = 0.0f, chorusMix = 0.0f;
    float delayTime = 0.3f, delayFeedback = 0.3f, delayMix = 0.0f;
    float reverbSize = 0.4f, reverbMix = 0.0f;

    float masterGainDb = 0.0f;
};

//==============================================================================
/** Zero-delay-feedback (TPT) state variable filter, 12 dB/oct lowpass. */
class SvfLowpass
{
public:
    void reset() { ic1 = ic2 = 0.0f; }

    void setCutoff (float hz, float q, double sampleRate)
    {
        hz = juce::jlimit (20.0f, (float) sampleRate * 0.45f, hz);
        q  = juce::jmax (0.5f, q);

        const float g = std::tan (juce::MathConstants<float>::pi * hz / (float) sampleRate);
        const float k = 1.0f / q;

        a1 = 1.0f / (1.0f + g * (g + k));
        a2 = g * a1;
        a3 = g * a2;
    }

    inline float process (float x) noexcept
    {
        const float v3 = x - ic2;
        const float v1 = a1 * ic1 + a2 * v3;
        const float v2 = ic2 + a2 * ic1 + a3 * v3;
        ic1 = 2.0f * v1 - ic1;
        ic2 = 2.0f * v2 - ic2;
        return v2;
    }

private:
    float a1 = 0.0f, a2 = 0.0f, a3 = 0.0f;
    float ic1 = 0.0f, ic2 = 0.0f;
};

//==============================================================================
/** Band-limited oscillator using PolyBLEP for saw and square. */
class PolyBlepOsc
{
public:
    void reset (double startPhase = 0.0) { phase = startPhase; }

    void setIncrement (double inc) { increment = juce::jlimit (0.0, 0.49, inc); }

    inline float next (int wave) noexcept
    {
        const double dt = increment;
        double t = phase;

        float out = 0.0f;

        switch (wave)
        {
            case 0: // saw
                out = (float) (2.0 * t - 1.0 - polyBlep (t, dt));
                break;

            case 1: // square
            {
                out = t < 0.5 ? 1.0f : -1.0f;
                out += (float) polyBlep (t, dt);
                double t2 = t + 0.5;
                if (t2 >= 1.0) t2 -= 1.0;
                out -= (float) polyBlep (t2, dt);
                break;
            }

            case 2: // triangle (mild aliasing, far below the saw's)
                out = (float) (1.0 - 4.0 * std::abs (t - 0.5));
                break;

            case 3: // sine
            default:
                out = (float) std::sin (juce::MathConstants<double>::twoPi * t);
                break;
        }

        phase += dt;
        if (phase >= 1.0)
            phase -= 1.0;

        return out;
    }

private:
    static double polyBlep (double t, double dt) noexcept
    {
        if (dt <= 0.0)
            return 0.0;

        if (t < dt)
        {
            t /= dt;
            return t + t - t * t - 1.0;
        }

        if (t > 1.0 - dt)
        {
            t = (t - 1.0) / dt;
            return t * t + t + t + 1.0;
        }

        return 0.0;
    }

    double phase = 0.0;
    double increment = 0.0;
};

//==============================================================================
class NeonVoice
{
public:
    void prepare (double sampleRate);
    void reset();

    void start (int midiNote, float velocity, juce::uint64 order);
    void release();
    void kill();

    bool isActive() const noexcept { return active; }
    int  getNote() const noexcept { return note; }
    juce::uint64 getStartOrder() const noexcept { return startOrder; }

    void render (float* left, float* right, int numSamples, const SynthParams& p, const float* lfo);

private:
    double sampleRate = 44100.0;
    int    note = -1;
    float  velocity = 0.0f;
    juce::uint64 startOrder = 0;
    bool   active = false;

    double baseFreq = 440.0;
    PolyBlepOsc osc1, osc2;
    SvfLowpass filter;
    juce::ADSR ampEnv, filterEnv;
    juce::Random random;
};

//==============================================================================
class NeonSynth
{
public:
    static constexpr int numVoices = 16;

    void prepare (double sampleRate, int maximumBlockSize, int numChannels);
    void reset();

    void setParams (const SynthParams& p) { params = p; }
    const SynthParams& getParams() const noexcept { return params; }

    void noteOn (int midiNote, float velocity);
    void noteOff (int midiNote);
    void sustainPedal (bool down);
    void allNotesOff (bool immediate = false);

    void render (juce::AudioBuffer<float>& buffer, int startSample, int numSamples);

    int getActiveVoiceCount() const;

    // Exposed for the editor's little voice LED meter.
    std::atomic<int> activeVoices { 0 };

private:
    NeonVoice& findFreeVoice (int midiNote);
    void stopNote (int midiNote, bool allowTailOff);

    std::array<NeonVoice, numVoices> voices;
    std::set<int> heldNotes, sustainedNotes;

    SynthParams params;
    double sampleRate = 44100.0;
    juce::uint64 voiceCounter = 0;

    bool sustainDown = false;

    double lfoPhase = 0.0;
    std::vector<float> lfoBuffer;

    juce::dsp::Chorus<float> chorus;
    juce::dsp::Reverb reverb;

    struct StereoDelay
    {
        juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::Linear> lineL, lineR;
        void prepare (double sr, double maxSeconds, int maxBlockSize);
        void reset();
        void process (juce::AudioBuffer<float>& buffer, float delaySeconds, float feedback, float mix, double sr);
    } delay;

    juce::SmoothedValue<float> masterGain;
};
} // namespace neonrift
