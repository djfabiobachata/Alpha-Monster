#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_dsp/juce_dsp.h>
#include <functional>
#include <complex>
#include "Profiles.h"
#include "Loudness.h"

namespace am
{

//==============================================================================
/** Incremental analyser: feed stereo audio in chunks, then call finish().

    - STFT (8192 / hop 2048) for 1/3-octave PSD, centroid, tilt, L/R coherence
      (stereo width), and L/R energy (pan).
    - 512-sample envelope for transients, decays (RT60) and wet/dry estimate.
    - BS.1770 loudness and 4x true peak.

    Used offline by the Mix app (whole files) and on a background thread by the
    Master plugin ("learn" mode).  It allocates only in prepare().
*/
class StreamingAnalyzer
{
public:
    void prepare (double sampleRate, bool collectEnvelope = true);
    void reset();

    /** Frames whose RMS is below this level (dBFS) are ignored for tone & image. */
    void setGateDb (float g) noexcept { gateDb = g; }

    void process (const float* left, const float* right, int numSamples);
    void process (const juce::AudioBuffer<float>& b);

    SignalProfile finish() const;

    /** Cheap live readouts (thread-safe enough for display). */
    float getCurrentWidth() const;
    float getShortTermLufs() const { return loudness.getShortTermLufs(); }
    int64_t getSamplesProcessed() const noexcept { return totalSamples; }

private:
    void processFftFrame();
    void processEnvelopeBlock();
    void analyseSpace (SignalProfile& p) const;

    double fs = 44100.0;
    bool collectEnv = true;
    float gateDb = -60.0f;

    static constexpr int fftOrder = 13, fftSize = 1 << fftOrder, hop = 2048, envHop = 512;
    std::unique_ptr<juce::dsp::FFT> fft;
    std::vector<float> window;
    double windowPower = 1.0;

    std::vector<float> inL, inR;     // circular input for FFT frames
    int inPos = 0, samplesSinceFrame = 0, filled = 0;
    std::vector<float> fftL, fftR;   // 2*fftSize work buffers

    // spectral accumulators (per bin)
    std::vector<double> sxx, syy;
    std::vector<std::complex<double>> sxy;
    std::vector<double> psdSum;
    int64_t activeFrames = 0, totalFrames = 0;

    // pan / correlation (time domain, gated)
    double energyL = 0, energyR = 0, crossLR = 0;

    // envelope (dB per envHop) + per-block correlation
    std::vector<float> envDb, envCorr;
    double blkLL = 0, blkRR = 0, blkLR = 0;
    int blkPos = 0;

    LoudnessMeter loudness;
    TruePeakMeter truePeak;
    double activeSquares = 0; int64_t activeSamples = 0;
    int64_t totalSamples = 0;
};

//==============================================================================
struct AudioAnalyzer
{
    /** Progress callback: receives 0..1, return false to cancel. */
    using Progress = std::function<bool (float)>;

    /** Full analysis of a (mono or stereo) buffer. Two passes: the first
        finds the level distribution so silent parts of stems are gated out. */
    static SignalProfile analyse (const juce::AudioBuffer<float>& audio, double sampleRate,
                                  Progress progress = {});

    /** Integrated loudness only (fast). */
    static float measureLufs (const juce::AudioBuffer<float>& audio, double sampleRate);

    /** Stereo width only (fast, analyses at most maxSeconds evenly spread). */
    static float measureWidth (const juce::AudioBuffer<float>& audio, double sampleRate);

    static float measureTruePeakDb (const juce::AudioBuffer<float>& audio);
};

} // namespace am
