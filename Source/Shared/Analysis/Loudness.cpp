#include "Loudness.h"
#include <cmath>

namespace am
{

//==============================================================================
float LoudnessMeter::energyToLufs (double e) noexcept
{
    if (e <= 1.0e-12)
        return -120.0f;
    return (float) (-0.691 + 10.0 * std::log10 (e));
}

double LoudnessMeter::lufsToEnergy (double l) noexcept
{
    return std::pow (10.0, (l + 0.691) / 10.0);
}

int LoudnessMeter::binFor (float lufs) noexcept
{
    return juce::jlimit (0, histBins - 1, (int) std::floor ((lufs - histMin) / histStep));
}

float LoudnessMeter::binCentre (int bin) noexcept
{
    return histMin + ((float) bin + 0.5f) * histStep;
}

void LoudnessMeter::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    channels = juce::jlimit (1, 2, numChannels);

    // K-weighting coefficients for any sample rate (same derivation as libebur128)
    {
        const double f0 = 1681.974450955533, G = 3.999843853973347, Q = 0.7071752369554196;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double Vh = std::pow (10.0, G / 20.0);
        const double Vb = std::pow (Vh, 0.4996667741545416);
        const double a0 = 1.0 + K / Q + K * K;

        for (auto& f : pre)
        {
            f.b0 = (Vh + Vb * K / Q + K * K) / a0;
            f.b1 = 2.0 * (K * K - Vh) / a0;
            f.b2 = (Vh - Vb * K / Q + K * K) / a0;
            f.a1 = 2.0 * (K * K - 1.0) / a0;
            f.a2 = (1.0 - K / Q + K * K) / a0;
        }
    }
    {
        const double f0 = 38.13547087602444, Q = 0.5003270373238773;
        const double K = std::tan (juce::MathConstants<double>::pi * f0 / fs);
        const double den = 1.0 + K / Q + K * K;

        for (auto& f : rlb)
        {
            f.b0 = 1.0; f.b1 = -2.0; f.b2 = 1.0;
            f.a1 = 2.0 * (K * K - 1.0) / den;
            f.a2 = (1.0 - K / Q + K * K) / den;
        }
    }

    subBlockLen = juce::jmax (1, (int) std::round (fs / 10.0));
    integratedHist.assign (histBins, 0);
    shortTermHist.assign (histBins, 0);
    reset();
}

void LoudnessMeter::reset()
{
    for (auto& f : pre) f.z1 = f.z2 = 0.0;
    for (auto& f : rlb) f.z1 = f.z2 = 0.0;
    subBlockPos = 0;
    subBlockEnergy = 0.0;
    ring.fill (0.0);
    ringWrite = ringCount = 0;
    momentary = shortTerm = maxShortTerm = maxMomentary = -100.0f;
    std::fill (integratedHist.begin(), integratedHist.end(), 0u);
    std::fill (shortTermHist.begin(), shortTermHist.end(), 0u);
}

void LoudnessMeter::process (const float* const* data, int numChannels, int numSamples)
{
    const int nCh = juce::jmin (numChannels, 2);
    if (nCh <= 0) return;

    for (int i = 0; i < numSamples; ++i)
    {
        double e = 0.0;
        for (int ch = 0; ch < nCh; ++ch)
        {
            const double y = rlb[(size_t) ch].process (pre[(size_t) ch].process ((double) data[ch][i]));
            e += y * y;
        }
        // a mono source counts as both speakers would carry it only once (BS.1770 sums channels)
        subBlockEnergy += e;

        if (++subBlockPos >= subBlockLen)
            finishSubBlock();
    }
}

void LoudnessMeter::finishSubBlock()
{
    ring[(size_t) ringWrite] = subBlockEnergy / (double) subBlockLen;
    ringWrite = (ringWrite + 1) % (int) ring.size();
    ringCount = juce::jmin (ringCount + 1, (int) ring.size());
    subBlockPos = 0;
    subBlockEnergy = 0.0;

    auto meanOfLast = [this] (int n)
    {
        double s = 0.0;
        for (int k = 1; k <= n; ++k)
            s += ring[(size_t) ((ringWrite - k + (int) ring.size()) % (int) ring.size())];
        return s / (double) n;
    };

    if (ringCount >= 4)
    {
        const double eM = meanOfLast (4);
        momentary = energyToLufs (eM);
        maxMomentary = juce::jmax (maxMomentary, momentary);
        if (momentary > -70.0f)
            ++integratedHist[(size_t) binFor (momentary)];
    }

    if (ringCount >= 30)
    {
        shortTerm = energyToLufs (meanOfLast (30));
        maxShortTerm = juce::jmax (maxShortTerm, shortTerm);
        if (shortTerm > -70.0f)
            ++shortTermHist[(size_t) binFor (shortTerm)];
    }
    else if (ringCount >= 4)
    {
        shortTerm = energyToLufs (meanOfLast (ringCount));
    }
}

float LoudnessMeter::getIntegratedLufs() const
{
    double sum = 0.0; uint64_t n = 0;
    for (int b = 0; b < histBins; ++b)
        if (integratedHist[(size_t) b] > 0)
        {
            sum += (double) integratedHist[(size_t) b] * lufsToEnergy (binCentre (b));
            n += integratedHist[(size_t) b];
        }

    if (n == 0) return -70.0f;

    const float relGate = energyToLufs (sum / (double) n) - 10.0f;
    sum = 0.0; n = 0;
    for (int b = binFor (relGate); b < histBins; ++b)
        if (integratedHist[(size_t) b] > 0 && binCentre (b) >= relGate)
        {
            sum += (double) integratedHist[(size_t) b] * lufsToEnergy (binCentre (b));
            n += integratedHist[(size_t) b];
        }

    return n == 0 ? -70.0f : energyToLufs (sum / (double) n);
}

float LoudnessMeter::getLoudnessRange() const
{
    double sum = 0.0; uint64_t n = 0;
    for (int b = 0; b < histBins; ++b)
        if (shortTermHist[(size_t) b] > 0)
        {
            sum += (double) shortTermHist[(size_t) b] * lufsToEnergy (binCentre (b));
            n += shortTermHist[(size_t) b];
        }
    if (n == 0) return 0.0f;

    const float relGate = energyToLufs (sum / (double) n) - 20.0f;
    uint64_t total = 0;
    for (int b = 0; b < histBins; ++b)
        if (binCentre (b) >= relGate) total += shortTermHist[(size_t) b];
    if (total == 0) return 0.0f;

    auto percentile = [&] (double p)
    {
        const uint64_t target = (uint64_t) std::ceil (p * (double) total);
        uint64_t acc = 0;
        for (int b = 0; b < histBins; ++b)
        {
            if (binCentre (b) < relGate) continue;
            acc += shortTermHist[(size_t) b];
            if (acc >= juce::jmax<uint64_t> (1, target)) return binCentre (b);
        }
        return binCentre (histBins - 1);
    };

    return juce::jmax (0.0f, percentile (0.95) - percentile (0.10));
}

//==============================================================================
void TruePeakMeter::prepare (int numChannels)
{
    // windowed-sinc interpolation filter, 48 taps split into 4 phases
    const int total = phases * tapsPerPhase;
    const double centre = (total - 1) / 2.0;
    for (int n = 0; n < total; ++n)
    {
        const double x = ((double) n - centre) / (double) phases;
        const double sinc = std::abs (x) < 1.0e-9 ? 1.0 : std::sin (juce::MathConstants<double>::pi * x) / (juce::MathConstants<double>::pi * x);
        const double w = 0.5 * (1.0 - std::cos (2.0 * juce::MathConstants<double>::pi * (n + 0.5) / (double) total));
        coeffs[(size_t) (n % phases)][(size_t) (n / phases)] = (float) (sinc * w);
    }
    // normalise each phase to unity DC gain
    for (auto& ph : coeffs)
    {
        float s = 0.0f;
        for (auto c : ph) s += c;
        if (std::abs (s) > 1.0e-6f)
            for (auto& c : ph) c /= s;
    }

    history.assign ((size_t) juce::jmax (1, numChannels), {});
    historyPos.assign ((size_t) juce::jmax (1, numChannels), 0);
    reset();
}

void TruePeakMeter::reset()
{
    for (auto& h : history) h.fill (0.0f);
    for (auto& p : historyPos) p = 0;
    peak = 0.0f;
}

void TruePeakMeter::process (const float* const* data, int numChannels, int numSamples)
{
    const int nCh = juce::jmin (numChannels, (int) history.size());
    for (int ch = 0; ch < nCh; ++ch)
    {
        auto& h = history[(size_t) ch];
        int& pos = historyPos[(size_t) ch];
        float pk = peak;

        for (int i = 0; i < numSamples; ++i)
        {
            h[(size_t) pos] = data[ch][i];
            pos = (pos + 1) % tapsPerPhase;

            for (int p = 0; p < phases; ++p)
            {
                float acc = 0.0f;
                int idx = pos;
                for (int t = tapsPerPhase - 1; t >= 0; --t)
                {
                    acc += coeffs[(size_t) p][(size_t) t] * h[(size_t) idx];
                    idx = (idx + 1) % tapsPerPhase;
                }
                pk = juce::jmax (pk, std::abs (acc));
            }
            pk = juce::jmax (pk, std::abs (data[ch][i]));
        }
        peak = pk;
    }
}

} // namespace am
