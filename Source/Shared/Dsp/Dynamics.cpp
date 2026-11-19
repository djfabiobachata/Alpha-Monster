#include "Dsp/Dynamics.h"

namespace am
{

//==============================================================================
void GlueCompressor::prepare (double sampleRate)
{
    fs = sampleRate;
    rmsCoef = std::exp (-1.0f / (float) (0.010 * fs));
    setParameters (threshold, ratio, 15.0f, 150.0f, knee);
    reset();
}

void GlueCompressor::reset()
{
    rmsState = 0.0f;
    gainState = 0.0f;
    grDb.store (0.0f);
}

void GlueCompressor::setParameters (float thresholdDb, float r, float attackMs, float releaseMs, float kneeDb)
{
    threshold = thresholdDb;
    ratio = juce::jmax (1.0f, r);
    knee = juce::jmax (0.0f, kneeDb);
    attackCoef = std::exp (-1.0f / (float) (juce::jmax (0.1f, attackMs) * 0.001 * fs));
    releaseCoef = std::exp (-1.0f / (float) (juce::jmax (1.0f, releaseMs) * 0.001 * fs));
}

void GlueCompressor::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    const int nCh = juce::jmin (2, buffer.getNumChannels());
    if (nCh == 0) return;
    float* l = buffer.getWritePointer (0);
    float* r = nCh > 1 ? buffer.getWritePointer (1) : l;

    float maxGr = 0.0f;
    for (int i = 0; i < n; ++i)
    {
        const float sq = 0.5f * (l[i] * l[i] + r[i] * r[i]);
        rmsState = rmsCoef * rmsState + (1.0f - rmsCoef) * sq;
        const float levelDb = 10.0f * std::log10 (rmsState + 1.0e-12f);

        // soft-knee static curve
        const float over = levelDb - threshold;
        float target = 0.0f;
        if (2.0f * over <= -knee)       target = 0.0f;
        else if (2.0f * over >= knee)   target = over * (1.0f / ratio - 1.0f);
        else
        {
            const float x = over + knee * 0.5f;
            target = (1.0f / ratio - 1.0f) * x * x / (2.0f * knee);
        }

        const float coef = target < gainState ? attackCoef : releaseCoef;
        gainState = coef * gainState + (1.0f - coef) * target;

        const float g = juce::Decibels::decibelsToGain (gainState);
        l[i] *= g;
        if (nCh > 1) r[i] *= g;
        maxGr = juce::jmin (maxGr, gainState);
    }
    grDb.store (-maxGr);
}

//==============================================================================
void MultibandWidth::prepare (double sampleRate, int blockSize)
{
    juce::dsp::ProcessSpec spec { sampleRate, (juce::uint32) blockSize, 2 };
    for (auto* f : { &lp1, &hp1, &lp2, &hp2, &apLow })
        f->prepare (spec);
    lp1.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    hp1.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    lp2.setType (juce::dsp::LinkwitzRileyFilterType::lowpass);
    hp2.setType (juce::dsp::LinkwitzRileyFilterType::highpass);
    apLow.setType (juce::dsp::LinkwitzRileyFilterType::allpass);
    lp1.setCutoffFrequency (200.0f);
    hp1.setCutoffFrequency (200.0f);
    lp2.setCutoffFrequency (4000.0f);
    hp2.setCutoffFrequency (4000.0f);
    apLow.setCutoffFrequency (4000.0f);

    low.setSize (2, blockSize);
    mid.setSize (2, blockSize);
    high.setSize (2, blockSize);
    for (auto& g : gains) g.reset (sampleRate, 0.05);
    reset();
}

void MultibandWidth::reset()
{
    for (auto* f : { &lp1, &hp1, &lp2, &hp2, &apLow })
        f->reset();
}

void MultibandWidth::setSideGains (float lo, float mi, float hi) noexcept
{
    gains[0].setTargetValue (lo);
    gains[1].setTargetValue (mi);
    gains[2].setTargetValue (hi);
}

void MultibandWidth::process (juce::AudioBuffer<float>& buffer) noexcept
{
    const int n = buffer.getNumSamples();
    if (buffer.getNumChannels() < 2 || n > low.getNumSamples()) return;

    for (int ch = 0; ch < 2; ++ch)
    {
        const float* in = buffer.getReadPointer (ch);
        float* lo = low.getWritePointer (ch);
        float* mi = mid.getWritePointer (ch);
        float* hi = high.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            const float x = in[i];
            const float a = lp1.processSample (ch, x);
            const float rest = hp1.processSample (ch, x);
            lo[i] = apLow.processSample (ch, a);
            mi[i] = lp2.processSample (ch, rest);
            hi[i] = hp2.processSample (ch, rest);
        }
    }

    float* l = buffer.getWritePointer (0);
    float* r = buffer.getWritePointer (1);
    juce::AudioBuffer<float>* bands[3] = { &low, &mid, &high };
    for (int i = 0; i < n; ++i)
    {
        float outL = 0.0f, outR = 0.0f;
        for (int b = 0; b < 3; ++b)
        {
            const float g = gains[(size_t) b].getNextValue();
            const float bl = bands[b]->getSample (0, i), br = bands[b]->getSample (1, i);
            const float m = 0.5f * (bl + br), s = 0.5f * (bl - br) * g;
            outL += m + s;
            outR += m - s;
        }
        l[i] = outL;
        r[i] = outR;
    }
}

//==============================================================================
void TruePeakLimiter::prepare (double sampleRate, int blockSize)
{
    fs = sampleRate;
    oversampling = std::make_unique<juce::dsp::Oversampling<float>> (2, 1, juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR, true, true);
    oversampling->initProcessing ((size_t) blockSize);
    osFs = fs * 2.0;

    lookahead = juce::jmax (8, (int) std::round (0.0015 * osFs));
    delayL.assign ((size_t) lookahead + 1, 0.0f);
    delayR.assign ((size_t) lookahead + 1, 0.0f);
    reqGain.assign ((size_t) lookahead + 1, 1.0f);
    dqIdx.assign ((size_t) lookahead + 3, 0);
    dqVal.assign ((size_t) lookahead + 3, 1.0f);
    updateCoefs();
    reset();
}

void TruePeakLimiter::updateCoefs()
{
    releaseCoef = std::exp (-1.0f / (float) (releaseMs * 0.001 * osFs));
    attackCoef = std::exp (-1.0f / (float) (lookahead / 4.0));
}

void TruePeakLimiter::reset()
{
    if (oversampling) oversampling->reset();
    std::fill (delayL.begin(), delayL.end(), 0.0f);
    std::fill (delayR.begin(), delayR.end(), 0.0f);
    std::fill (reqGain.begin(), reqGain.end(), 1.0f);
    writePos = 0;
    dqHead = dqCount = 0;
    sampleCounter = 0;
    smoothGain = 1.0f;
    grDb.store (0.0f);
}

int TruePeakLimiter::getLatencySamples() const noexcept
{
    const float osLatency = oversampling ? oversampling->getLatencyInSamples() : 0.0f;
    return (int) std::round (osLatency + (float) lookahead / 2.0f);  // look-ahead is counted at 2x rate
}

void TruePeakLimiter::process (juce::AudioBuffer<float>& buffer) noexcept
{
    if (buffer.getNumChannels() < 2 || oversampling == nullptr) return;
    juce::dsp::AudioBlock<float> block (buffer.getArrayOfWritePointers(), 2, (size_t) buffer.getNumSamples());
    auto up = oversampling->processSamplesUp (block);
    processOversampled (up);
    oversampling->processSamplesDown (block);

    // final safety clip at the ceiling (inter-sample overs after downsampling are tiny)
    for (int ch = 0; ch < 2; ++ch)
    {
        float* d = buffer.getWritePointer (ch);
        for (int i = 0; i < buffer.getNumSamples(); ++i)
            d[i] = juce::jlimit (-ceiling, ceiling, d[i]);
    }
}

void TruePeakLimiter::processOversampled (juce::dsp::AudioBlock<float>& block) noexcept
{
    const int n = (int) block.getNumSamples();
    float* l = block.getChannelPointer (0);
    float* r = block.getChannelPointer (1);
    const int len = lookahead + 1;
    const int cap = (int) dqIdx.size();
    float minGain = 1.0f;

    for (int i = 0; i < n; ++i)
    {
        const float peak = juce::jmax (std::abs (l[i]), std::abs (r[i]));
        const float req = peak > ceiling ? ceiling / peak : 1.0f;

        // sliding minimum of the required gain over the look-ahead window
        while (dqCount > 0 && dqVal[(size_t) ((dqHead + dqCount - 1) % cap)] >= req)
            --dqCount;
        const int slot = (dqHead + dqCount) % cap;
        dqIdx[(size_t) slot] = sampleCounter;
        dqVal[(size_t) slot] = req;
        ++dqCount;
        while (dqCount > 0 && dqIdx[(size_t) dqHead] <= sampleCounter - len)
        {
            dqHead = (dqHead + 1) % cap;
            --dqCount;
        }
        const float target = dqVal[(size_t) dqHead];
        ++sampleCounter;

        // fast attack that completes inside the look-ahead, program-dependent release
        const float coef = target < smoothGain ? attackCoef : releaseCoef;
        smoothGain = coef * smoothGain + (1.0f - coef) * target;

        const float dl = delayL[(size_t) writePos], dr = delayR[(size_t) writePos];
        delayL[(size_t) writePos] = l[i];
        delayR[(size_t) writePos] = r[i];
        writePos = (writePos + 1) % len;

        l[i] = dl * smoothGain;
        r[i] = dr * smoothGain;
        minGain = juce::jmin (minGain, smoothGain);
    }
    grDb.store (-juce::Decibels::gainToDecibels (minGain, -60.0f));
}

} // namespace am
