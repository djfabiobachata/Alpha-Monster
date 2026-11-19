#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <functional>
#include "Analysis/Profiles.h"
#include "Analysis/FilterDesign.h"

namespace am
{

/** What Alpha Monster did to one track (also what the user can copy in a DAW). */
struct TrackSettings
{
    float gainDb = 0.0f;
    float panBefore = 0.0f, panAfter = 0.0f;
    bool  pannedAsMono = false;
    float sideGainDb = 0.0f;          // M/S side gain (width)
    float stereoize = 0.0f;           // pseudo-stereo amount for mono sources (0 = none)
    BandCurve eqCurve {};
    bool  compressed = false;
    float compThresholdDb = 0.0f, compRatio = 1.0f, compAttackMs = 10.0f, compReleaseMs = 120.0f;
    float reverbSendDb = -100.0f;     // wet level relative to the dry track
    float reverbDecaySec = 0.0f, reverbRoom = 0.5f, reverbDamping = 0.5f;
    StemType targetStem = StemType::other;
    bool  hasTarget = false;
};

struct MixTrack
{
    juce::String name;
    juce::File file;
    StemType type = StemType::other;
    bool fromSeparation = false;
    juce::AudioBuffer<float> audio;     // stereo, project rate
    juce::AudioBuffer<float> processed; // after auto-mix (incl. gain)
    SignalProfile before, after;
    TrackSettings settings;
};

struct MixAmounts
{
    float level = 1.0f;      // 0..1 how much to follow the reference balance
    float eq = 0.7f;         // tonal matching
    float dynamics = 0.6f;   // compression matching
    float space = 1.0f;      // pan & width
    float reverb = 0.8f;     // depth / reverb
    float targetLufs = -16.0f; // loudness of the (pre-master) mix
};

struct MixResult
{
    bool ok = false;
    juce::String error;
    juce::AudioBuffer<float> original, mix;
    SignalProfile mixBefore, mixAfter;
    double sampleRate = 44100.0;
    juce::StringArray warnings;
};

class AutoMixer
{
public:
    using Progress = std::function<bool (float, const juce::String&)>;

    /** Processes `tracks` in place (fills processed / settings / after) and returns the mix. */
    static MixResult run (std::vector<MixTrack>& tracks, const ReferenceMixProfile& reference,
                          double sampleRate, const MixAmounts& amounts, Progress progress);

    static juce::String buildReport (const std::vector<MixTrack>& tracks, const ReferenceMixProfile& reference,
                                     const MixResult& result, const MixAmounts& amounts);
    static juce::var buildJson (const std::vector<MixTrack>& tracks, const ReferenceMixProfile& reference,
                                const MixResult& result);

    /** Human description of one stem of the reference (used in UI and report). */
    static juce::StringArray describeReferenceStem (const StemProfile& s);

    /** Which reference stem a user category should follow. */
    static const StemProfile* targetFor (StemType userType, const ReferenceMixProfile& ref);
};

} // namespace am
