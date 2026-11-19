#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <array>
#include <vector>

namespace am
{

//==============================================================================
/** ITU-R BS.1770-4 / EBU R128 loudness meter.

    Works both offline (feed a whole file) and in real time (feed blocks from the
    audio thread; it never allocates after prepare()).
    Integrated loudness and LRA use fixed-size histograms, so memory is bounded
    regardless of program length.
*/
class LoudnessMeter
{
public:
    void prepare (double sampleRate, int numChannels);
    void reset();

    /** Accepts any number of channels; channels beyond 2 are ignored. */
    void process (const float* const* channels, int numChannels, int numSamples);
    void process (const juce::AudioBuffer<float>& buffer)
    {
        process (buffer.getArrayOfReadPointers(), buffer.getNumChannels(), buffer.getNumSamples());
    }

    float getMomentaryLufs() const noexcept   { return momentary; }
    float getShortTermLufs() const noexcept   { return shortTerm; }
    float getMaxShortTermLufs() const noexcept { return maxShortTerm; }
    float getMaxMomentaryLufs() const noexcept { return maxMomentary; }
    float getIntegratedLufs() const;
    float getLoudnessRange() const;

    static float energyToLufs (double e) noexcept;
    static double lufsToEnergy (double l) noexcept;

private:
    struct Biquad
    {
        double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
        double z1 = 0, z2 = 0;
        inline double process (double x) noexcept
        {
            const double y = b0 * x + z1;
            z1 = b1 * x - a1 * y + z2;
            z2 = b2 * x - a2 * y;
            return y;
        }
    };

    void finishSubBlock();

    static constexpr int histBins = 1000;   // 0.1 dB bins from -70 to +30 LUFS
    static constexpr float histMin = -70.0f;
    static constexpr float histStep = 0.1f;
    static int binFor (float lufs) noexcept;
    static float binCentre (int bin) noexcept;

    double fs = 48000.0;
    int channels = 2;
    std::array<Biquad, 2> pre, rlb;

    int subBlockLen = 4800, subBlockPos = 0;
    double subBlockEnergy = 0.0;
    std::array<double, 30> ring {};
    int ringWrite = 0, ringCount = 0;

    float momentary = -100.0f, shortTerm = -100.0f;
    float maxShortTerm = -100.0f, maxMomentary = -100.0f;
    std::vector<uint32_t> integratedHist, shortTermHist;
};

//==============================================================================
/** 4x oversampled true-peak detector (BS.1770 Annex 2 style). */
class TruePeakMeter
{
public:
    void prepare (int numChannels);
    void reset();
    void process (const float* const* channels, int numChannels, int numSamples);
    void process (const juce::AudioBuffer<float>& b) { process (b.getArrayOfReadPointers(), b.getNumChannels(), b.getNumSamples()); }

    /** Linear peak since last reset / getAndResetPeak. */
    float getPeak() const noexcept { return peak; }
    float getAndResetPeak() noexcept { auto p = peak; peak = 0.0f; return p; }

private:
    static constexpr int phases = 4, tapsPerPhase = 12;
    std::array<std::array<float, tapsPerPhase>, phases> coeffs {};
    std::vector<std::array<float, tapsPerPhase>> history;
    std::vector<int> historyPos;
    float peak = 0.0f;
};

} // namespace am
