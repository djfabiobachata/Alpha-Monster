#pragma once

#include <juce_dsp/juce_dsp.h>
#include <array>
#include <atomic>

namespace am
{

//==============================================================================
/** Stereo-linked RMS "glue" compressor with soft knee. */
class GlueCompressor
{
public:
    void prepare (double sampleRate);
    void reset();
    void setParameters (float thresholdDb, float ratio, float attackMs, float releaseMs, float kneeDb = 6.0f);
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    float getGainReductionDb() const noexcept { return grDb.load(); }

private:
    double fs = 44100.0;
    float threshold = 0.0f, ratio = 1.0f, knee = 6.0f;
    float attackCoef = 0.0f, releaseCoef = 0.0f, rmsCoef = 0.0f;
    float rmsState = 0.0f, gainState = 0.0f;   // gainState in dB (<= 0)
    std::atomic<float> grDb { 0.0f };
};

//==============================================================================
/** 3-band M/S width (crossovers 200 Hz / 4 kHz, phase-aligned LR4). */
class MultibandWidth
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();
    /** Side gains (linear) for low / mid / high. */
    void setSideGains (float low, float mid, float high) noexcept;
    void process (juce::AudioBuffer<float>& buffer) noexcept;

private:
    juce::dsp::LinkwitzRileyFilter<float> lp1, hp1, lp2, hp2, apLow;
    juce::AudioBuffer<float> low, mid, high;
    std::array<juce::SmoothedValue<float>, 3> gains;
};

//==============================================================================
/** Look-ahead brickwall limiter, 2x oversampled for true-peak control. */
class TruePeakLimiter
{
public:
    void prepare (double sampleRate, int blockSize);
    void reset();
    void setCeilingDb (float c) noexcept { ceiling = juce::Decibels::decibelsToGain (c); }
    void setReleaseMs (float ms) noexcept { releaseMs = ms; updateCoefs(); }
    void process (juce::AudioBuffer<float>& buffer) noexcept;
    int getLatencySamples() const noexcept;
    float getGainReductionDb() const noexcept { return grDb.load(); }

private:
    void updateCoefs();
    void processOversampled (juce::dsp::AudioBlock<float>& block) noexcept;

    double fs = 44100.0, osFs = 88200.0;
    float ceiling = 0.89f, releaseMs = 80.0f;
    float releaseCoef = 0.0f, attackCoef = 0.0f;
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampling;

    int lookahead = 128;
    std::vector<float> delayL, delayR, reqGain;
    int writePos = 0;
    // monotonic deque for sliding minimum of required gain
    std::vector<int64_t> dqIdx;
    std::vector<float> dqVal;
    int dqHead = 0, dqCount = 0;
    int64_t sampleCounter = 0;
    float smoothGain = 1.0f;
    std::atomic<float> grDb { 0.0f };
};

} // namespace am
