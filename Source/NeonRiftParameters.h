#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_audio_utils/juce_audio_utils.h>

namespace neonrift
{
namespace pid
{
inline constexpr const char* osc1Wave   = "osc1_wave";
inline constexpr const char* osc1Oct    = "osc1_oct";
inline constexpr const char* osc1Detune = "osc1_detune";
inline constexpr const char* osc1Level  = "osc1_level";

inline constexpr const char* osc2Wave   = "osc2_wave";
inline constexpr const char* osc2Oct    = "osc2_oct";
inline constexpr const char* osc2Detune = "osc2_detune";
inline constexpr const char* osc2Level  = "osc2_level";

inline constexpr const char* noiseLevel = "noise_level";

inline constexpr const char* cutoff       = "filter_cutoff";
inline constexpr const char* resonance    = "filter_res";
inline constexpr const char* filterEnvAmt = "filter_env_amt";

inline constexpr const char* ampAttack  = "amp_attack";
inline constexpr const char* ampDecay   = "amp_decay";
inline constexpr const char* ampSustain = "amp_sustain";
inline constexpr const char* ampRelease = "amp_release";

inline constexpr const char* filtAttack  = "filt_attack";
inline constexpr const char* filtDecay   = "filt_decay";
inline constexpr const char* filtSustain = "filt_sustain";
inline constexpr const char* filtRelease = "filt_release";

inline constexpr const char* lfoRate   = "lfo_rate";
inline constexpr const char* lfoDepth  = "lfo_depth";
inline constexpr const char* lfoTarget = "lfo_target";

inline constexpr const char* drive       = "fx_drive";
inline constexpr const char* chorusMix   = "fx_chorus";
inline constexpr const char* delayTime   = "fx_delay_time";
inline constexpr const char* delayFb     = "fx_delay_fb";
inline constexpr const char* delayMix    = "fx_delay_mix";
inline constexpr const char* reverbSize  = "fx_reverb_size";
inline constexpr const char* reverbMix   = "fx_reverb_mix";

inline constexpr const char* masterGain = "master_gain";
} // namespace pid

inline juce::StringArray oscWaveNames() { return { "Saw", "Square", "Triangle", "Sine" }; }
inline juce::StringArray lfoTargetNames() { return { "Off", "Cutoff", "Pitch", "Amp" }; }

inline juce::AudioProcessorValueTreeState::ParameterLayout createParameterLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout layout;

    auto addFloat = [&layout] (const char* id, const String& name, NormalisableRange<float> range,
                               float def, const String& suffix = {}, const String& text = {})
    {
        layout.add (std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, range, def,
                                                           AudioParameterFloatAttributes()
                                                               .withLabel (text)
                                                               .withStringFromValueFunction (
                                                                   [suffix] (float v, int)
                                                                   {
                                                                       return String (v, 2) + suffix;
                                                                   })));
    };

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::osc1Wave, 1 }, "Osc 1 Wave", oscWaveNames(), 0));
    addFloat (pid::osc1Oct,    "Osc 1 Octave", NormalisableRange<float> (-2.0f, 2.0f, 1.0f), 0.0f, "", "oct");
    addFloat (pid::osc1Detune, "Osc 1 Detune", NormalisableRange<float> (-50.0f, 50.0f, 0.01f), 0.0f, " ct");
    addFloat (pid::osc1Level,  "Osc 1 Level",  NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.6f);

    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::osc2Wave, 1 }, "Osc 2 Wave", oscWaveNames(), 1));
    addFloat (pid::osc2Oct,    "Osc 2 Octave", NormalisableRange<float> (-2.0f, 2.0f, 1.0f), 0.0f, "", "oct");
    addFloat (pid::osc2Detune, "Osc 2 Detune", NormalisableRange<float> (-50.0f, 50.0f, 0.01f), 7.0f, " ct");
    addFloat (pid::osc2Level,  "Osc 2 Level",  NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.35f);

    addFloat (pid::noiseLevel, "Noise", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);

    addFloat (pid::cutoff,       "Cutoff",     NormalisableRange<float> (20.0f, 20000.0f, 0.0f, 0.25f), 2200.0f, " Hz");
    addFloat (pid::resonance,    "Resonance",  NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.25f);
    addFloat (pid::filterEnvAmt, "Filter Env", NormalisableRange<float> (-1.0f, 1.0f, 0.001f), 0.35f);

    addFloat (pid::ampAttack,  "Amp Attack",  NormalisableRange<float> (0.001f, 4.0f, 0.0f, 0.35f), 0.01f, " s");
    addFloat (pid::ampDecay,   "Amp Decay",   NormalisableRange<float> (0.005f, 4.0f, 0.0f, 0.35f), 0.35f, " s");
    addFloat (pid::ampSustain, "Amp Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.75f);
    addFloat (pid::ampRelease, "Amp Release", NormalisableRange<float> (0.005f, 6.0f, 0.0f, 0.35f), 0.45f, " s");

    addFloat (pid::filtAttack,  "Filter Attack",  NormalisableRange<float> (0.001f, 4.0f, 0.0f, 0.35f), 0.01f, " s");
    addFloat (pid::filtDecay,   "Filter Decay",   NormalisableRange<float> (0.005f, 4.0f, 0.0f, 0.35f), 0.5f, " s");
    addFloat (pid::filtSustain, "Filter Sustain", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.3f);
    addFloat (pid::filtRelease, "Filter Release", NormalisableRange<float> (0.005f, 6.0f, 0.0f, 0.35f), 0.5f, " s");

    addFloat (pid::lfoRate,  "LFO Rate",  NormalisableRange<float> (0.05f, 20.0f, 0.0f, 0.3f), 1.0f, " Hz");
    addFloat (pid::lfoDepth, "LFO Depth", NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    layout.add (std::make_unique<AudioParameterChoice> (ParameterID { pid::lfoTarget, 1 }, "LFO Target", lfoTargetNames(), 0));

    addFloat (pid::drive,      "Drive",        NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat (pid::chorusMix,  "Chorus",       NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat (pid::delayTime,  "Delay Time",   NormalisableRange<float> (0.05f, 1.5f, 0.001f), 0.3f, " s");
    addFloat (pid::delayFb,    "Delay Feedb.", NormalisableRange<float> (0.0f, 0.9f, 0.001f), 0.3f);
    addFloat (pid::delayMix,   "Delay Mix",    NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);
    addFloat (pid::reverbSize, "Reverb Size",  NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.4f);
    addFloat (pid::reverbMix,  "Reverb Mix",   NormalisableRange<float> (0.0f, 1.0f, 0.001f), 0.0f);

    addFloat (pid::masterGain, "Master", NormalisableRange<float> (-24.0f, 12.0f, 0.1f), 0.0f, " dB");

    return layout;
}
} // namespace neonrift
