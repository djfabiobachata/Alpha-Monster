#include "FilterDesign.h"
#include <cmath>
#include <algorithm>
#include <vector>
#include <complex>

namespace am
{

BandCurve computeMatchCurve (const SignalProfile& source, const SignalProfile& target,
                             const MatchCurveOptions& o)
{
    BandCurve diff {};
    if (! source.valid || ! target.valid)
        return diff;

    const auto src = source.normalisedBands();
    const auto tgt = target.normalisedBands();

    float srcMax = -200.0f, tgtMax = -200.0f;
    for (int b = 0; b < kNumBands; ++b)
    {
        srcMax = juce::jmax (srcMax, src[(size_t) b]);
        tgtMax = juce::jmax (tgtMax, tgt[(size_t) b]);
    }

    for (int b = 0; b < kNumBands; ++b)
    {
        float d = tgt[(size_t) b] - src[(size_t) b];
        // never pull up bands where the source has (almost) nothing: that only raises noise
        if (src[(size_t) b] < srcMax - 45.0f) d = juce::jmin (d, 2.0f);
        // nor carve out bands that are empty in both
        if (src[(size_t) b] < srcMax - 60.0f && tgt[(size_t) b] < tgtMax - 60.0f) d = 0.0f;
        diff[(size_t) b] = d;
    }

    for (int pass = 0; pass < o.smoothingPasses; ++pass)
    {
        BandCurve s = diff;
        for (int b = 0; b < kNumBands; ++b)
        {
            const int a = juce::jmax (0, b - 1), c = juce::jmin (kNumBands - 1, b + 1);
            s[(size_t) b] = (diff[(size_t) a] + diff[(size_t) b] + diff[(size_t) c]) / 3.0f;
        }
        diff = s;
    }

    // anchor the mid range at 0 dB
    float mean = 0.0f; int n = 0;
    for (int b = 10; b <= 24; ++b) { mean += diff[(size_t) b]; ++n; }
    mean /= (float) n;

    for (auto& d : diff)
        d = juce::jlimit (-o.maxCutDb, o.maxBoostDb, (d - mean)) * o.amount;

    // gentle fade at the extremes (sub-sonic and air)
    diff[0] *= 0.4f; diff[1] *= 0.7f;
    diff[kNumBands - 1] *= 0.5f;
    return diff;
}

juce::StringArray describeCurve (const BandCurve& c, float minDb, int maxItems)
{
    struct Move { int band; float db; };
    std::vector<Move> moves;
    const auto& f = bandCentres();

    for (int b = 0; b < kNumBands; ++b)
    {
        const float v = c[(size_t) b];
        if (std::abs (v) < minDb) continue;
        const float l = b > 0 ? c[(size_t) b - 1] : (v > 0 ? -100.0f : 100.0f);
        const float r = b < kNumBands - 1 ? c[(size_t) b + 1] : (v > 0 ? -100.0f : 100.0f);
        const bool isPeak = v > 0 && v >= l && v >= r;
        const bool isDip  = v < 0 && v <= l && v <= r;
        if (isPeak || isDip) moves.push_back ({ b, v });
    }

    std::sort (moves.begin(), moves.end(), [] (const Move& a, const Move& b) { return std::abs (a.db) > std::abs (b.db); });

    juce::StringArray out;
    for (auto& m : moves)
    {
        if (out.size() >= maxItems) break;
        // estimate bandwidth: how many bands stay above half the gain
        int w = 1;
        for (int k = m.band - 1; k >= 0 && std::abs (c[(size_t) k]) >= std::abs (m.db) * 0.5f && (c[(size_t) k] > 0) == (m.db > 0); --k) ++w;
        for (int k = m.band + 1; k < kNumBands && std::abs (c[(size_t) k]) >= std::abs (m.db) * 0.5f && (c[(size_t) k] > 0) == (m.db > 0); ++k) ++w;
        const float octaves = (float) w / 3.0f;
        const float q = juce::jlimit (0.3f, 8.0f, std::sqrt (std::pow (2.0f, octaves)) / (std::pow (2.0f, octaves) - 1.0f));

        juce::String kind = "Peak";
        if (m.band <= 4 && w >= 4)                    kind = "Low shelf";
        else if (m.band >= kNumBands - 4 && w >= 4)   kind = "High shelf";

        out.add (kind + " " + (m.db > 0 ? "+" : "") + juce::String (m.db, 1) + " dB @ "
                 + formatFrequency (f[(size_t) m.band]) + "Hz" + (kind == "Peak" ? "  Q " + juce::String (q, 1) : juce::String()));
    }
    return out;
}

//==============================================================================
GraphicEq::GraphicEq()
{
    for (auto& t : targetDb) t.store (0.0f);
    for (auto& g : fittedGains) g.store (0.0f);
}

GraphicEq::Coeffs GraphicEq::peaking (double fs, double f, double q, double gainDb)
{
    Coeffs c;
    const double A = std::pow (10.0, gainDb / 40.0);
    const double w0 = 2.0 * juce::MathConstants<double>::pi * f / fs;
    const double alpha = std::sin (w0) / (2.0 * q);
    const double cw = std::cos (w0);
    const double a0 = 1.0 + alpha / A;
    c.b0 = (1.0 + alpha * A) / a0;
    c.b1 = (-2.0 * cw) / a0;
    c.b2 = (1.0 - alpha * A) / a0;
    c.a1 = (-2.0 * cw) / a0;
    c.a2 = (1.0 - alpha / A) / a0;
    return c;
}

double GraphicEq::magnitudeDb (const Coeffs& c, double fs, double f)
{
    const double w = 2.0 * juce::MathConstants<double>::pi * f / fs;
    const std::complex<double> z1 = std::polar (1.0, -w), z2 = std::polar (1.0, -2.0 * w);
    const auto num = c.b0 + c.b1 * z1 + c.b2 * z2;
    const auto den = 1.0 + c.a1 * z1 + c.a2 * z2;
    return 20.0 * std::log10 (std::abs (num / den) + 1.0e-12);
}

BandCurve GraphicEq::fitGains (const BandCurve& target, double fs, float q)
{
    const auto& f = bandCentres();
    BandCurve g {};
    const double nyq = fs * 0.45;

    for (int b = 0; b < kNumBands; ++b)
        g[(size_t) b] = f[(size_t) b] < nyq ? target[(size_t) b] * 0.6f : 0.0f;

    for (int iter = 0; iter < 12; ++iter)
    {
        std::array<Coeffs, kNumBands> cs;
        for (int j = 0; j < kNumBands; ++j)
            cs[(size_t) j] = peaking (fs, f[(size_t) j], q, g[(size_t) j]);

        for (int b = 0; b < kNumBands; ++b)
        {
            if (f[(size_t) b] >= nyq) continue;
            double resp = 0.0;
            for (int j = 0; j < kNumBands; ++j)
                if (f[(size_t) j] < nyq && std::abs (g[(size_t) j]) > 1.0e-4f)
                    resp += magnitudeDb (cs[(size_t) j], fs, f[(size_t) b]);
            g[(size_t) b] = juce::jlimit (-24.0f, 24.0f, g[(size_t) b] + 0.6f * (float) (target[(size_t) b] - resp));
        }
    }
    return g;
}

void GraphicEq::prepare (double sampleRate, int numChannels)
{
    fs = sampleRate;
    numCh = juce::jlimit (1, 2, numChannels);
    // refit for the new sample rate
    BandCurve t;
    for (int b = 0; b < kNumBands; ++b) t[(size_t) b] = targetDb[(size_t) b].load();
    setTargetCurve (t);
    reset();
}

void GraphicEq::reset()
{
    for (auto& ch : state)
        for (auto& s : ch) s = { 0.0, 0.0 };
    appliedVersion = 0;
    appliedAmount = -1.0f;
}

void GraphicEq::setTargetCurve (const BandCurve& curveDb)
{
    const auto g = fitGains (curveDb, fs, kQ);
    for (int b = 0; b < kNumBands; ++b)
    {
        targetDb[(size_t) b].store (curveDb[(size_t) b]);
        fittedGains[(size_t) b].store (g[(size_t) b]);
    }
    version.fetch_add (1);
}

BandCurve GraphicEq::getTargetCurve() const
{
    BandCurve t;
    for (int b = 0; b < kNumBands; ++b) t[(size_t) b] = targetDb[(size_t) b].load();
    return t;
}

void GraphicEq::setAmount (float a) noexcept
{
    amount.store (juce::jlimit (0.0f, 2.0f, a));
}

void GraphicEq::updateCoefficients()
{
    const float amt = amount.load();
    const auto& f = bandCentres();
    for (int b = 0; b < kNumBands; ++b)
    {
        const float g = fittedGains[(size_t) b].load() * amt;
        active[(size_t) b] = std::abs (g) > 0.01f && f[(size_t) b] < fs * 0.45;
        if (active[(size_t) b])
            coeffs[(size_t) b] = peaking (fs, f[(size_t) b], kQ, g);
    }
    appliedAmount = amt;
}

void GraphicEq::process (float* const* channels, int numChannels, int numSamples) noexcept
{
    const int v = version.load();
    if (v != appliedVersion || std::abs (amount.load() - appliedAmount) > 0.002f)
    {
        appliedVersion = v;
        updateCoefficients();
    }

    const int nCh = juce::jmin (numChannels, 2);
    for (int b = 0; b < kNumBands; ++b)
    {
        if (! active[(size_t) b]) continue;
        const auto& c = coeffs[(size_t) b];
        for (int ch = 0; ch < nCh; ++ch)
        {
            auto& s = state[(size_t) ch][(size_t) b];
            double z1 = s[0], z2 = s[1];
            float* d = channels[ch];
            for (int i = 0; i < numSamples; ++i)
            {
                const double x = d[i];
                const double y = c.b0 * x + z1;
                z1 = c.b1 * x - c.a1 * y + z2;
                z2 = c.b2 * x - c.a2 * y;
                d[i] = (float) y;
            }
            s[0] = z1; s[1] = z2;
        }
    }
}

float GraphicEq::responseDb (float hz) const
{
    const auto& f = bandCentres();
    const float amt = amount.load();
    double r = 0.0;
    for (int b = 0; b < kNumBands; ++b)
    {
        const float g = fittedGains[(size_t) b].load() * amt;
        if (std::abs (g) > 0.01f && f[(size_t) b] < fs * 0.45)
            r += magnitudeDb (peaking (fs, f[(size_t) b], kQ, g), fs, hz);
    }
    return (float) r;
}

} // namespace am
