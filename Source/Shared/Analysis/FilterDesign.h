#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <atomic>
#include "Profiles.h"

namespace am
{

using BandCurve = std::array<float, kNumBands>;

struct MatchCurveOptions
{
    float amount = 1.0f;        // 0..1 scales the whole correction
    float maxBoostDb = 9.0f;
    float maxCutDb = 12.0f;
    int   smoothingPasses = 2;  // 3-band moving average passes (~1 octave)
};

/** Tonal-balance correction (dB per 1/3-octave band) that moves `source`
    towards `target`. The 200 Hz - 5 kHz region is anchored at 0 dB on average
    so the correction changes balance, not loudness. */
BandCurve computeMatchCurve (const SignalProfile& source, const SignalProfile& target,
                             const MatchCurveOptions& options);

/** Human readable summary of the main moves in a curve ("+2.5 dB @ 3.2k"). */
juce::StringArray describeCurve (const BandCurve& curve, float minDb = 1.5f, int maxItems = 5);

//==============================================================================
/** 31-band constant-Q graphic EQ that is fitted to a target curve.
    Minimum phase, zero latency. The target can be changed from any thread;
    the audio thread picks it up at the next process() call. */
class GraphicEq
{
public:
    GraphicEq();

    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Sets the response to approximate (dB per band). Thread-safe. */
    void setTargetCurve (const BandCurve& curveDb);
    void setAmount (float amount01) noexcept;   // scales the fitted gains
    BandCurve getTargetCurve() const;

    void process (float* const* channels, int numChannels, int numSamples) noexcept;
    void process (juce::AudioBuffer<float>& b) noexcept { process (b.getArrayOfWritePointers(), b.getNumChannels(), b.getNumSamples()); }

    /** Response of the fitted bank (incl. amount) at any frequency. */
    float responseDb (float hz) const;

    static BandCurve fitGains (const BandCurve& target, double sampleRate, float q);

private:
    struct Coeffs { double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0; };
    static Coeffs peaking (double fs, double f, double q, double gainDb);
    static double magnitudeDb (const Coeffs& c, double fs, double f);
    void updateCoefficients();

    static constexpr float kQ = 2.2f;
    double fs = 44100.0;
    int numCh = 2;

    std::array<std::atomic<float>, kNumBands> targetDb;
    std::array<std::atomic<float>, kNumBands> fittedGains;
    std::atomic<float> amount { 1.0f };
    std::atomic<int> version { 1 };
    int appliedVersion = 0;
    float appliedAmount = -1.0f;

    std::array<Coeffs, kNumBands> coeffs {};
    std::array<bool, kNumBands> active {};
    std::array<std::array<std::array<double, 2>, kNumBands>, 2> state {};
};

} // namespace am
