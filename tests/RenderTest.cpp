/*
    Offline verification for the NeonRift engine.

    No DAW required: this renders audio through the exact same DSP that the
    plugin uses, checks it for silence / NaNs / sane spectral behaviour, and
    writes a demo WAV next to the executable so it can be listened to.
*/
#include <juce_audio_formats/juce_audio_formats.h>
#include "SynthEngine.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <vector>

using namespace neonrift;

namespace
{
constexpr double kSampleRate = 48000.0;
constexpr int    kBlockSize  = 512;

int gFailures = 0;

void check (bool condition, const char* what)
{
    std::printf ("[%s] %s\n", condition ? "PASS" : "FAIL", what);
    if (! condition)
        ++gFailures;
}

struct Event
{
    int sample;
    bool noteOn;
    int note;
    float velocity;
};

SynthParams makePatch()
{
    SynthParams p;
    p.osc1Wave = 0; p.osc1Level = 0.55f;
    p.osc2Wave = 1; p.osc2Level = 0.30f; p.osc2Detune = 9.0f; p.osc2Oct = -1.0f;
    p.noiseLevel = 0.04f;

    p.cutoff = 1600.0f;
    p.resonance = 0.30f;
    p.filterEnvAmt = 0.45f;

    p.ampA = 0.008f; p.ampD = 0.35f; p.ampS = 0.72f; p.ampR = 0.55f;
    p.fA = 0.005f;  p.fD = 0.45f;   p.fS = 0.22f;  p.fR = 0.45f;

    p.lfoTarget = 0; p.lfoRate = 1.2f; p.lfoDepth = 0.0f;

    p.drive = 0.18f;
    p.chorusMix = 0.35f;
    p.delayTime = 0.28f; p.delayFeedback = 0.32f; p.delayMix = 0.22f;
    p.reverbSize = 0.55f; p.reverbMix = 0.22f;
    p.masterGainDb = -3.0f;
    return p;
}

/** Renders `seconds` of audio, applying events at their sample positions. */
juce::AudioBuffer<float> render (NeonSynth& synth, const SynthParams& patch, double seconds,
                                 const std::vector<Event>& events)
{
    const int total = (int) (kSampleRate * seconds);
    juce::AudioBuffer<float> output (2, total);
    output.clear();

    juce::AudioBuffer<float> block (2, kBlockSize);
    std::vector<Event> sorted = events;

    std::sort (sorted.begin(), sorted.end(), [] (const Event& a, const Event& b)
    {
        return a.sample < b.sample;
    });

    synth.setParams (patch);
    size_t nextEvent = 0;

    for (int pos = 0; pos < total; pos += kBlockSize)
    {
        const int n = juce::jmin (kBlockSize, total - pos);

        while (nextEvent < sorted.size() && sorted[nextEvent].sample <= pos)
        {
            const auto& e = sorted[nextEvent];

            if (e.noteOn)
                synth.noteOn (e.note, e.velocity);
            else
                synth.noteOff (e.note);

            ++nextEvent;
        }

        block.setSize (2, n, false, false, true);
        block.clear();
        synth.render (block, 0, n);

        for (int ch = 0; ch < 2; ++ch)
            output.copyFrom (ch, pos, block, ch, 0, n);
    }

    return output;
}

float peakOf (const juce::AudioBuffer<float>& b, int startSample = 0, int numSamples = -1)
{
    float peak = 0.0f;
    const int end = numSamples < 0 ? b.getNumSamples() : juce::jmin (b.getNumSamples(), startSample + numSamples);

    for (int ch = 0; ch < b.getNumChannels(); ++ch)
        peak = juce::jmax (peak, b.getMagnitude (ch, startSample, end - startSample));

    return peak;
}

float rmsOf (const juce::AudioBuffer<float>& b, int startSample, int numSamples)
{
    return b.getRMSLevel (0, startSample, numSamples);
}

bool hasInvalidSamples (const juce::AudioBuffer<float>& b)
{
    for (int ch = 0; ch < b.getNumChannels(); ++ch)
    {
        const auto* d = b.getReadPointer (ch);

        for (int i = 0; i < b.getNumSamples(); ++i)
            if (! std::isfinite (d[i]) || std::abs (d[i]) > 12.0f)
                return true;
    }

    return false;
}

/** Crude brightness measure: zero crossings per second. */
double zeroCrossingRate (const juce::AudioBuffer<float>& b, int startSample, int numSamples)
{
    const auto* d = b.getReadPointer (0);
    int crossings = 0;

    for (int i = startSample + 1; i < startSample + numSamples; ++i)
        if ((d[i - 1] <= 0.0f) != (d[i] <= 0.0f))
            ++crossings;

    return (double) crossings / (numSamples / kSampleRate);
}

/**
    Brightness proxy that is robust for saw waves: mean absolute first
    difference divided by mean absolute level. High harmonics cost more
    per-sample change, so a more open filter raises the ratio.
*/
double highFrequencyRatio (const juce::AudioBuffer<float>& b, int startSample, int numSamples)
{
    const auto* d = b.getReadPointer (0);
    double diff = 0.0, level = 0.0;

    for (int i = startSample + 1; i < startSample + numSamples; ++i)
    {
        diff  += std::abs (d[i] - d[i - 1]);
        level += std::abs (d[i]);
    }

    return diff / juce::jmax (1.0e-9, level);
}

void writeWav (const juce::File& file, const juce::AudioBuffer<float>& buffer)
{
    file.deleteFile();

    if (auto stream = std::unique_ptr<juce::FileOutputStream> (file.createOutputStream()))
    {
        juce::WavAudioFormat format;
        std::unique_ptr<juce::AudioFormatWriter> writer (
            format.createWriterFor (stream.get(), kSampleRate,
                                    (unsigned int) buffer.getNumChannels(), 24, {}, 0));

        if (writer != nullptr)
        {
            stream.release();
            writer->writeFromAudioSampleBuffer (buffer, 0, buffer.getNumSamples());
            std::printf ("[wav ] wrote %s\n", file.getFullPathName().toRawUTF8());
            return;
        }
    }

    std::printf ("[wav ] FAILED to write %s\n", file.getFullPathName().toRawUTF8());
    ++gFailures;
}
} // namespace

int main (int, char**)
{
    std::printf ("NeonRift engine test @ %.0f Hz, block %d\n\n", kSampleRate, kBlockSize);

    const auto patch = makePatch();

    // ---- 1. silence before any note ---------------------------------------
    {
        NeonSynth synth;
        synth.prepare (kSampleRate, kBlockSize, 2);
        synth.setParams (patch);
        auto silence = render (synth, patch, 0.2, {});
        check (peakOf (silence) < 1.0e-6f, "silent with no notes held");
    }

    // ---- 2. a note produces decent, finite audio ---------------------------
    {
        NeonSynth synth;
        synth.prepare (kSampleRate, kBlockSize, 2);
        auto audio = render (synth, patch, 1.5, { { 0, true, 57, 0.9f } });

        const float peak = peakOf (audio, 4800, 48000);
        check (peak > 0.05f, "note produces audio");
        check (peak < 1.0f, "output does not clip by default");
        check (! hasInvalidSamples (audio), "no NaNs / runaway samples");
        check (zeroCrossingRate (audio, 4800, 24000) > 40.0, "audible fundamental present");
    }

    // ---- 3. filter cutoff really changes brightness ------------------------
    {
        auto bright = patch; bright.cutoff = 9000.0f; bright.filterEnvAmt = 0.0f;
        auto dark   = patch; dark.cutoff   = 180.0f;  dark.filterEnvAmt = 0.0f;

        NeonSynth s1, s2;
        s1.prepare (kSampleRate, kBlockSize, 2);
        s2.prepare (kSampleRate, kBlockSize, 2);

        auto a = render (s1, bright, 0.6, { { 0, true, 45, 0.9f } });
        auto b = render (s2, dark,   0.6, { { 0, true, 45, 0.9f } });

        const double brightRatio = highFrequencyRatio (a, 9600, 14400);
        const double darkRatio   = highFrequencyRatio (b, 9600, 14400);

        std::printf ("       brightness ratio: bright=%.3f, dark=%.3f\n", brightRatio, darkRatio);
        check (brightRatio > darkRatio * 1.8, "cutoff sweeps brightness as expected");
    }

    // ---- 4. release actually fades out ------------------------------------
    {
        NeonSynth synth;
        synth.prepare (kSampleRate, kBlockSize, 2);
        auto audio = render (synth, patch, 3.0, { { 0, true, 60, 0.9f }, { (int) (kSampleRate * 1.2), false, 60, 0.0f } });

        const float held = rmsOf (audio, (int) (kSampleRate * 0.8), 8000);
        const float tail = rmsOf (audio, audio.getNumSamples() - 8000, 8000);

        std::printf ("       held rms=%.4f, tail rms=%.4f\n", held, tail);
        check (held > 0.01f, "sustain level is audible");
        check (tail < held * 0.1f, "release tail decays to near silence");
    }

    // ---- 5. 16-voice polyphony stays stable -------------------------------
    {
        NeonSynth synth;
        synth.prepare (kSampleRate, kBlockSize, 2);

        std::vector<Event> chord;

        for (int i = 0; i < 20; ++i) // more notes than voices -> stealing
            chord.push_back ({ i * 64, true, 48 + i, 0.8f });

        auto audio = render (synth, patch, 1.0, chord);
        check (! hasInvalidSamples (audio), "voice stealing stays stable");
        check (synth.getActiveVoiceCount() <= NeonSynth::numVoices, "voice count stays within 16");
    }

    // ---- 6. demo audio for human ears ------------------------------------
    {
        const double seconds = 6.0;
        std::vector<Event> demo;
        const int step = (int) (kSampleRate * 0.28);

        const int arp[] = { 60, 64, 67, 72, 67, 64, 60, 64, 67, 72, 76, 72, 67, 64 };
        for (int i = 0; i < 14; ++i)
        {
            demo.push_back ({ (int) (i * step * 0.95), true,  arp[i], 0.85f });
            demo.push_back ({ (int) (i * step * 0.95 + step * 0.8), false, arp[i], 0.0f });
        }

        const int chordAt = (int) (kSampleRate * 4.2);
        for (int n : { 36, 48, 55, 60, 64, 67 })
        {
            demo.push_back ({ chordAt, true, n, 0.75f });
            demo.push_back ({ chordAt + (int) (kSampleRate * 1.3), false, n, 0.0f });
        }

        NeonSynth synth;
        synth.prepare (kSampleRate, kBlockSize, 2);
        auto audio = render (synth, patch, seconds, demo);

        const auto file = juce::File::getCurrentWorkingDirectory().getChildFile ("NeonRift-demo.wav");
        writeWav (file, audio);
        check (! hasInvalidSamples (audio), "demo render is clean");
        std::printf ("       demo peak = %.3f\n", peakOf (audio));
    }

    std::printf ("\n%s (%d failure%s)\n",
                 gFailures == 0 ? "ALL CHECKS PASSED" : "CHECKS FAILED",
                 gFailures, gFailures == 1 ? "" : "s");

    return gFailures == 0 ? 0 : 1;
}
