#include "AudioAnalyzer.h"
#include <algorithm>
#include <cmath>
#include <numeric>

namespace am
{

namespace
{
    float norm01 (float v, float lo, float hi)
    {
        return juce::jlimit (0.0f, 1.0f, (v - lo) / (hi - lo));
    }

    float median (std::vector<float> v)
    {
        if (v.empty()) return 0.0f;
        const auto mid = v.begin() + (long) (v.size() / 2);
        std::nth_element (v.begin(), mid, v.end());
        return *mid;
    }

    inline double toDb (double powerValue) { return 10.0 * std::log10 (powerValue + 1.0e-20); }
}

//==============================================================================
void StreamingAnalyzer::prepare (double sampleRate, bool collectEnvelope)
{
    fs = sampleRate;
    collectEnv = collectEnvelope;
    fft = std::make_unique<juce::dsp::FFT> (fftOrder);

    window.resize (fftSize);
    windowPower = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        window[(size_t) i] = 0.5f - 0.5f * std::cos (2.0f * juce::MathConstants<float>::pi * (float) i / (float) fftSize);
        windowPower += (double) window[(size_t) i] * window[(size_t) i];
    }

    inL.assign (fftSize, 0.0f);
    inR.assign (fftSize, 0.0f);
    fftL.assign (2 * fftSize, 0.0f);
    fftR.assign (2 * fftSize, 0.0f);

    const size_t nBins = fftSize / 2 + 1;
    sxx.assign (nBins, 0.0);
    syy.assign (nBins, 0.0);
    sxy.assign (nBins, {});
    psdSum.assign (nBins, 0.0);

    loudness.prepare (fs, 2);
    truePeak.prepare (2);

    if (collectEnv)
    {
        envDb.reserve ((size_t) (fs * 600.0 / envHop));
        envCorr.reserve (envDb.capacity());
    }
    reset();
}

void StreamingAnalyzer::reset()
{
    std::fill (inL.begin(), inL.end(), 0.0f);
    std::fill (inR.begin(), inR.end(), 0.0f);
    inPos = samplesSinceFrame = filled = 0;
    std::fill (sxx.begin(), sxx.end(), 0.0);
    std::fill (syy.begin(), syy.end(), 0.0);
    std::fill (sxy.begin(), sxy.end(), std::complex<double>());
    std::fill (psdSum.begin(), psdSum.end(), 0.0);
    activeFrames = totalFrames = 0;
    energyL = energyR = crossLR = 0.0;
    envDb.clear(); envCorr.clear();
    blkLL = blkRR = blkLR = 0.0; blkPos = 0;
    loudness.reset();
    truePeak.reset();
    activeSquares = 0.0; activeSamples = 0; totalSamples = 0;
}

void StreamingAnalyzer::process (const juce::AudioBuffer<float>& b)
{
    if (b.getNumChannels() <= 0) return;
    const float* l = b.getReadPointer (0);
    const float* r = b.getNumChannels() > 1 ? b.getReadPointer (1) : l;
    process (l, r, b.getNumSamples());
}

void StreamingAnalyzer::process (const float* left, const float* right, int numSamples)
{
    const float* chans[2] = { left, right };
    loudness.process (chans, 2, numSamples);
    truePeak.process (chans, 2, numSamples);
    totalSamples += numSamples;

    for (int i = 0; i < numSamples; ++i)
    {
        const float l = left[i], r = right[i];
        inL[(size_t) inPos] = l;
        inR[(size_t) inPos] = r;
        inPos = (inPos + 1) % fftSize;
        filled = juce::jmin (filled + 1, fftSize);

        blkLL += (double) l * l;
        blkRR += (double) r * r;
        blkLR += (double) l * r;
        if (++blkPos >= envHop)
            processEnvelopeBlock();

        if (++samplesSinceFrame >= hop && filled >= fftSize / 2)
        {
            samplesSinceFrame = 0;
            processFftFrame();
        }
    }
}

void StreamingAnalyzer::processEnvelopeBlock()
{
    const double ms = (blkLL + blkRR) / (2.0 * envHop);
    const float db = (float) toDb (ms);

    if (collectEnv && envDb.size() < envDb.capacity())
    {
        envDb.push_back (db);
        const double d = std::sqrt (blkLL * blkRR);
        envCorr.push_back (d > 1.0e-12 ? (float) (blkLR / d) : 1.0f);
    }

    if (db > gateDb)
    {
        energyL += blkLL;
        energyR += blkRR;
        crossLR += blkLR;
        activeSquares += blkLL + blkRR;
        activeSamples += 2 * envHop;
    }

    blkLL = blkRR = blkLR = 0.0;
    blkPos = 0;
}

void StreamingAnalyzer::processFftFrame()
{
    ++totalFrames;

    double e = 0.0;
    for (int i = 0; i < fftSize; ++i)
    {
        const int idx = (inPos + i) % fftSize;
        const float w = window[(size_t) i];
        fftL[(size_t) i] = inL[(size_t) idx] * w;
        fftR[(size_t) i] = inR[(size_t) idx] * w;
        e += (double) fftL[(size_t) i] * fftL[(size_t) i] + (double) fftR[(size_t) i] * fftR[(size_t) i];
    }

    const double frameMs = e / (2.0 * windowPower);
    if (toDb (frameMs) < gateDb)
        return;

    ++activeFrames;
    std::fill (fftL.begin() + fftSize, fftL.end(), 0.0f);
    std::fill (fftR.begin() + fftSize, fftR.end(), 0.0f);
    fft->performRealOnlyForwardTransform (fftL.data(), true);
    fft->performRealOnlyForwardTransform (fftR.data(), true);

    const int nBins = fftSize / 2 + 1;
    for (int k = 1; k < nBins; ++k)
    {
        const std::complex<double> L (fftL[(size_t) (2 * k)], fftL[(size_t) (2 * k + 1)]);
        const std::complex<double> R (fftR[(size_t) (2 * k)], fftR[(size_t) (2 * k + 1)]);
        const double pl = std::norm (L), pr = std::norm (R);
        sxx[(size_t) k] += pl;
        syy[(size_t) k] += pr;
        sxy[(size_t) k] += L * std::conj (R);
        psdSum[(size_t) k] += 0.5 * (pl + pr);
    }
}

float StreamingAnalyzer::getCurrentWidth() const
{
    double num = 0.0, den = 0.0;
    const double binHz = fs / fftSize;
    for (size_t k = 1; k < sxx.size(); ++k)
    {
        const double f = (double) k * binHz;
        if (f < 40.0 || f > 16000.0) continue;
        const double w = sxx[k] + syy[k];
        if (w <= 0.0) continue;
        const double lo = juce::jmin (sxx[k], syy[k]), hi = juce::jmax (sxx[k], syy[k]);
        double g = std::abs (sxy[k]) / std::sqrt (sxx[k] * syy[k] + 1.0e-30);
        const double imbalanceDb = 10.0 * std::log10 ((hi + 1.0e-30) / (lo + 1.0e-30));
        const double a = juce::jlimit (0.0, 1.0, (imbalanceDb - 20.0) / 10.0);
        g = g + (1.0 - g) * a;
        num += w * (1.0 - g);
        den += w;
    }
    return den > 0.0 ? (float) (num / den) : 0.0f;
}

SignalProfile StreamingAnalyzer::finish() const
{
    SignalProfile p;
    p.durationSec = (float) ((double) totalSamples / fs);
    p.integratedLufs = loudness.getIntegratedLufs();
    p.maxShortTermLufs = loudness.getMaxShortTermLufs();
    p.maxMomentaryLufs = loudness.getMaxMomentaryLufs();
    p.loudnessRange = loudness.getLoudnessRange();
    p.truePeakDb = juce::Decibels::gainToDecibels (truePeak.getPeak(), -100.0f);
    p.activeFraction = totalFrames > 0 ? (float) activeFrames / (float) totalFrames : 0.0f;

    if (activeFrames == 0 || activeSamples == 0)
        return p;

    p.valid = true;
    p.rmsDb = (float) toDb (activeSquares / (double) activeSamples);
    p.crestDb = p.truePeakDb - p.rmsDb;
    p.plrDb = p.truePeakDb - p.integratedLufs;

    // ---- tone ---------------------------------------------------------------
    const double binHz = fs / fftSize;
    const double scale = 2.0 / (fs * windowPower * (double) activeFrames);
    const auto& centres = bandCentres();
    const int nBins = (int) psdSum.size();

    for (int b = 0; b < kNumBands; ++b)
    {
        const double fc = centres[(size_t) b];
        const double lo = fc * std::pow (2.0, -1.0 / 6.0);
        const double hi = juce::jmin (fc * std::pow (2.0, 1.0 / 6.0), fs * 0.5);
        if (lo >= fs * 0.5) { p.bandDb[(size_t) b] = -140.0f; continue; }

        double sum = 0.0; int count = 0;
        for (int k = juce::jmax (1, (int) std::ceil (lo / binHz)); k < nBins && (double) k * binHz < hi; ++k)
        {
            sum += psdSum[(size_t) k] * scale;
            ++count;
        }
        if (count == 0)
        {
            const int k = juce::jlimit (1, nBins - 1, (int) std::round (fc / binHz));
            sum = psdSum[(size_t) k] * scale;
            count = 1;
        }
        p.bandDb[(size_t) b] = (float) toDb ((sum / count) * (hi - lo));
    }

    {
        double num = 0.0, den = 0.0;
        for (int k = 1; k < nBins; ++k)
        {
            const double f = k * binHz;
            if (f < 20.0 || f > 20000.0) continue;
            num += f * psdSum[(size_t) k];
            den += psdSum[(size_t) k];
        }
        p.centroidHz = den > 0.0 ? (float) (num / den) : 0.0f;
    }

    {
        // regression of band level against octave number, 63 Hz .. 12.5 kHz
        double sx = 0, sy = 0, sxx2 = 0, sxy2 = 0; int n = 0;
        for (int b = 5; b <= 28; ++b)
        {
            if (centres[(size_t) b] >= fs * 0.5) break;
            const double x = std::log2 (centres[(size_t) b] / 1000.0);
            const double y = p.bandDb[(size_t) b];
            sx += x; sy += y; sxx2 += x * x; sxy2 += x * y; ++n;
        }
        const double d = n * sxx2 - sx * sx;
        p.tiltDbPerOct = (n > 2 && std::abs (d) > 1.0e-9) ? (float) ((n * sxy2 - sx * sy) / d) : 0.0f;
    }

    {
        std::array<double, 3> e {};
        for (int b = 0; b < kNumBands; ++b)
        {
            const float fc = centres[(size_t) b];
            const int r = fc < kLowSplitHz ? 0 : (fc < kHighSplitHz ? 1 : 2);
            e[(size_t) r] += std::pow (10.0, p.bandDb[(size_t) b] / 10.0);
        }
        const double t = e[0] + e[1] + e[2];
        for (size_t r = 0; r < 3; ++r)
            p.regionEnergy[r] = t > 0.0 ? (float) (e[r] / t) : 0.0f;
    }

    // ---- stereo image -------------------------------------------------------
    {
        std::array<double, 3> num {}, den {};
        double tNum = 0.0, tDen = 0.0;
        for (int k = 1; k < nBins; ++k)
        {
            const double f = k * binHz;
            if (f < 30.0 || f > 18000.0) continue;
            const double w = sxx[(size_t) k] + syy[(size_t) k];
            if (w <= 0.0) continue;
            const double lo = juce::jmin (sxx[(size_t) k], syy[(size_t) k]);
            const double hi = juce::jmax (sxx[(size_t) k], syy[(size_t) k]);
            double g = std::abs (sxy[(size_t) k]) / std::sqrt (sxx[(size_t) k] * syy[(size_t) k] + 1.0e-30);
            // a source panned hard to one side is mono even though coherence is undefined
            const double imbalanceDb = 10.0 * std::log10 ((hi + 1.0e-30) / (lo + 1.0e-30));
            g += (1.0 - g) * juce::jlimit (0.0, 1.0, (imbalanceDb - 20.0) / 10.0);
            g = juce::jlimit (0.0, 1.0, g);

            const int r = f < kLowSplitHz ? 0 : (f < kHighSplitHz ? 1 : 2);
            num[(size_t) r] += w * (1.0 - g);
            den[(size_t) r] += w;
            tNum += w * (1.0 - g);
            tDen += w;
        }
        for (size_t r = 0; r < 3; ++r)
            p.regionWidth[r] = den[r] > 0.0 ? (float) (num[r] / den[r]) : 0.0f;
        p.width = tDen > 0.0 ? (float) (tNum / tDen) : 0.0f;

        const double eSum = energyL + energyR;
        const double bal = eSum > 0.0 ? (energyR - energyL) / eSum : 0.0;
        p.pan = (float) (2.0 / juce::MathConstants<double>::pi * std::asin (juce::jlimit (-1.0, 1.0, bal)));
        const double d = std::sqrt (energyL * energyR);
        p.correlation = d > 1.0e-12 ? (float) (crossLR / d) : 1.0f;
        p.isMono = p.width < 0.10f;
    }

    analyseSpace (p);
    finaliseDepth (p, StemType::other);
    return p;
}

void StreamingAnalyzer::analyseSpace (SignalProfile& p) const
{
    const int n = (int) envDb.size();
    const double hopSec = envHop / fs;
    p.transientSharpness = norm01 (p.crestDb, 8.0f, 22.0f);
    if (n < 16)
        return;

    const float maxEnv = *std::max_element (envDb.begin(), envDb.end());
    const float gate = juce::jmax (gateDb, maxEnv - 45.0f);

    // ---- onsets
    struct Onset { int start, peak; float rise; };
    std::vector<Onset> onsets;
    const int minSpacing = juce::jmax (2, (int) std::round (0.06 / hopSec));
    int last = -minSpacing;
    for (int k = 3; k < n; ++k)
    {
        const float before = juce::jmin (envDb[(size_t) k - 1], juce::jmin (envDb[(size_t) k - 2], envDb[(size_t) k - 3]));
        const float rise = envDb[(size_t) k] - before;
        if (rise > 6.0f && envDb[(size_t) k] > gate && k - last >= minSpacing)
        {
            int peak = k;
            for (int j = k + 1; j < juce::jmin (n, k + 5); ++j)
                if (envDb[(size_t) j] > envDb[(size_t) peak]) peak = j;
            onsets.push_back ({ k, peak, envDb[(size_t) peak] - before });
            last = peak;
            k = peak;
        }
    }

    const int early = juce::jmax (2, (int) std::round (0.08 / hopSec));
    const int lateMax = (int) std::round (1.0 / hopSec);
    const int decayMax = (int) std::round (2.5 / hopSec);
    const int minLate = (int) std::round (0.15 / hopSec);

    std::vector<float> ratios, rts, rises;
    double tailDecor = 0.0, tailW = 0.0, headDecor = 0.0, headW = 0.0;

    for (size_t i = 0; i < onsets.size(); ++i)
    {
        const int pk = onsets[i].peak;
        const int next = i + 1 < onsets.size() ? onsets[i + 1].start : n;
        const float lp = envDb[(size_t) pk];
        rises.push_back (onsets[i].rise);

        for (int j = pk; j < juce::jmin (next, pk + 2); ++j)
        {
            const double w = std::pow (10.0, envDb[(size_t) j] / 10.0);
            headDecor += w * (1.0 - envCorr[(size_t) j]);
            headW += w;
        }

        // early / late energy, only for percussive events (the sound itself decays
        // quickly): for sustained notes late energy says nothing about the room
        const int lateEnd = juce::jmin (next, juce::jmin (n, pk + lateMax));
        bool percussive = false;
        for (int j = pk; j < juce::jmin (lateEnd, pk + early * 4); ++j)
            if (envDb[(size_t) j] <= lp - 10.0f) { percussive = true; break; }
        if (percussive && lateEnd - (pk + early) >= minLate)
        {
            double eE = 0.0, eL = 0.0;
            for (int j = pk; j < pk + early; ++j) eE += std::pow (10.0, envDb[(size_t) j] / 10.0);
            for (int j = pk + early; j < lateEnd; ++j)
            {
                const double w = std::pow (10.0, envDb[(size_t) j] / 10.0);
                eL += w;
                const float rel = lp - envDb[(size_t) j];
                if (rel > 8.0f && rel < 40.0f)
                {
                    tailDecor += w * (1.0 - envCorr[(size_t) j]);
                    tailW += w;
                }
            }
            eE /= early;
            eL /= (lateEnd - pk - early);
            ratios.push_back ((float) toDb (eL / (eE + 1.0e-20)));
        }

        // decay slope -> RT60
        const int decayEnd = juce::jmin (next, juce::jmin (n, pk + decayMax));
        int t5 = -1, tEnd = -1;
        for (int j = pk; j < decayEnd; ++j)
        {
            if (t5 < 0 && envDb[(size_t) j] <= lp - 5.0f) t5 = j;
            if (t5 >= 0)
            {
                tEnd = j;
                if (envDb[(size_t) j] <= lp - 35.0f || envDb[(size_t) j] < gate) break;
            }
        }
        if (t5 >= 0 && tEnd - t5 >= 3 && lp - envDb[(size_t) tEnd] >= 15.0f)
        {
            double sx = 0, sy = 0, sxx2 = 0, sxy2 = 0;
            const int m = tEnd - t5 + 1;
            for (int j = t5; j <= tEnd; ++j)
            {
                const double x = (j - t5) * hopSec, y = envDb[(size_t) j];
                sx += x; sy += y; sxx2 += x * x; sxy2 += x * y;
            }
            const double d = m * sxx2 - sx * sx;
            if (std::abs (d) > 1.0e-12)
            {
                const double slope = (m * sxy2 - sx * sy) / d; // dB per second
                if (slope < -1.0)
                    rts.push_back ((float) juce::jlimit (0.05, 8.0, -60.0 / slope));
            }
        }
    }

    // ---- note endings: how much sound is left right after the direct sound stops,
    // and how it decays. Works for sustained instruments and vocals.
    std::vector<float> drops;
    {
        int k = 8;
        while (k < n - 7)
        {
            const float lo = envDb[(size_t) k - 1];
            float hiPrev = -200.0f, loPrev = 200.0f;
            for (int j = k - 7; j < k; ++j)
            {
                hiPrev = juce::jmax (hiPrev, envDb[(size_t) j]);
                loPrev = juce::jmin (loPrev, envDb[(size_t) j]);
            }
            const bool sustainedBefore = hiPrev - loPrev < 4.5f;
            if (sustainedBefore && k + 6 < n && lo > gate + 12.0f && lo - envDb[(size_t) k + 5] > 6.0f
                && envDb[(size_t) k] < lo)
            {
                // follow the steep part (direct sound dying)
                int ts = k + 4;
                while (ts < n - 2 && (envDb[(size_t) ts] - envDb[(size_t) ts + 2]) > 3.0f) ++ts;
                const float drop = lo - envDb[(size_t) ts];
                drops.push_back (drop);

                // tail decay until something new starts
                int te = ts;
                const int maxEnd = juce::jmin (n - 1, ts + decayMax);
                while (te < maxEnd && envDb[(size_t) te + 1] <= envDb[(size_t) te] + 1.5f && envDb[(size_t) te + 1] > gate) ++te;
                if (te - ts >= 5 && envDb[(size_t) ts] - envDb[(size_t) te] >= 6.0f)
                {
                    double sx = 0, sy = 0, sxx2 = 0, sxy2 = 0;
                    const int m = te - ts + 1;
                    for (int j = ts; j <= te; ++j)
                    {
                        const double x = (j - ts) * hopSec, y = envDb[(size_t) j];
                        sx += x; sy += y; sxx2 += x * x; sxy2 += x * y;
                    }
                    const double d = m * sxx2 - sx * sx;
                    if (std::abs (d) > 1.0e-12)
                    {
                        const double slope = (m * sxy2 - sx * sy) / d;
                        if (slope < -1.0) rts.push_back ((float) juce::jlimit (0.05, 8.0, -60.0 / slope));
                    }
                }
                else if (envDb[(size_t) ts] < gate + 3.0f)
                {
                    rts.push_back (0.08f);   // dropped straight to silence: dry
                }
                k = juce::jmax (k + 1, te);
                continue;
            }
            ++k;
        }
    }

    const float riseMed = median (rises);
    p.transientSharpness = 0.5f * norm01 (riseMed, 6.0f, 30.0f) + 0.5f * norm01 (p.crestDb, 8.0f, 22.0f);

    // combine the available cues: decay time (most reliable), tail decorrelation
    // (stereo reverbs) and late/early energy of percussive hits
    float sum = 0.0f, weights = 0.0f;
    if (tailW > 0.0 && headW > 0.0)
    {
        const float decor = (float) juce::jmax (0.0, tailDecor / tailW - headDecor / headW);
        sum += 0.30f * norm01 (decor, 0.02f, 0.35f);
        weights += 0.30f;
    }
    if (rts.size() >= 3)
    {
        p.rt60 = median (rts);
        sum += 0.45f * norm01 (p.rt60, 0.3f, 2.2f);
        weights += 0.45f;
    }
    if (ratios.size() >= 3)
    {
        sum += 0.25f * norm01 (median (ratios), -24.0f, -6.0f);
        weights += 0.25f;
    }
    if (drops.size() >= 2)
    {
        // small drop after the note ends = lots of room sound
        sum += 0.35f * (1.0f - norm01 (median (drops), 10.0f, 35.0f));
        weights += 0.35f;
    }
    p.reverbAmount = weights > 0.0f ? juce::jlimit (0.0f, 1.0f, sum / weights) : 0.0f;
}

//==============================================================================
SignalProfile AudioAnalyzer::analyse (const juce::AudioBuffer<float>& audio, double sampleRate, Progress progress)
{
    const int n = audio.getNumSamples();
    if (n <= 0 || audio.getNumChannels() <= 0)
        return {};

    const float* l = audio.getReadPointer (0);
    const float* r = audio.getNumChannels() > 1 ? audio.getReadPointer (1) : l;

    // pass 1: level distribution -> gate
    std::vector<float> levels;
    levels.reserve ((size_t) (n / 4096 + 1));
    for (int s = 0; s < n; s += 4096)
    {
        const int len = juce::jmin (4096, n - s);
        double e = 0.0;
        for (int i = 0; i < len; ++i) e += (double) l[s + i] * l[s + i] + (double) r[s + i] * r[s + i];
        levels.push_back ((float) toDb (e / (2.0 * len)));
    }
    std::vector<float> sorted = levels;
    std::sort (sorted.begin(), sorted.end());
    const float p95 = sorted[(size_t) ((sorted.size() - 1) * 95 / 100)];
    const float gate = juce::jmax (-60.0f, p95 - 30.0f);

    // pass 2
    StreamingAnalyzer sa;
    sa.prepare (sampleRate, true);
    sa.setGateDb (gate);

    constexpr int chunk = 1 << 16;
    for (int s = 0; s < n; s += chunk)
    {
        const int len = juce::jmin (chunk, n - s);
        sa.process (l + s, r + s, len);
        if (progress && ! progress ((float) (s + len) / (float) n))
            return {};
    }
    return sa.finish();
}

float AudioAnalyzer::measureLufs (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    LoudnessMeter m;
    m.prepare (sampleRate, audio.getNumChannels());
    if (audio.getNumChannels() == 1)
    {
        const float* chans[2] = { audio.getReadPointer (0), audio.getReadPointer (0) };
        m.process (chans, 2, audio.getNumSamples());
    }
    else
    {
        m.process (audio);
    }
    return m.getIntegratedLufs();
}

float AudioAnalyzer::measureWidth (const juce::AudioBuffer<float>& audio, double sampleRate)
{
    if (audio.getNumChannels() < 2) return 0.0f;
    StreamingAnalyzer sa;
    sa.prepare (sampleRate, false);
    sa.setGateDb (-60.0f);
    sa.process (audio);
    return sa.getCurrentWidth();
}

float AudioAnalyzer::measureTruePeakDb (const juce::AudioBuffer<float>& audio)
{
    TruePeakMeter tp;
    tp.prepare (audio.getNumChannels());
    tp.process (audio);
    return juce::Decibels::gainToDecibels (tp.getPeak(), -100.0f);
}

} // namespace am
