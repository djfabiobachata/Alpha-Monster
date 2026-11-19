#include "MixEngine.h"
#include "Analysis/AudioAnalyzer.h"
#include "Dsp/Dynamics.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <map>

namespace am
{

namespace
{
    using Buffer = juce::AudioBuffer<float>;

    /** Up to four evenly spaced 10 s windows: fast, representative measurements. */
    Buffer excerpt (const Buffer& b, double sr, double maxSec = 40.0)
    {
        const int n = b.getNumSamples();
        const int maxLen = (int) (maxSec * sr);
        if (n <= maxLen) return b;
        const int seg = maxLen / 4;
        Buffer out (2, seg * 4);
        for (int k = 0; k < 4; ++k)
        {
            const int start = (int) ((double) (n - seg) * (k + 0.5) / 4.0);
            for (int ch = 0; ch < 2; ++ch)
                out.copyFrom (ch, k * seg, b, ch, juce::jlimit (0, n - seg, start), seg);
        }
        return out;
    }

    void applyEq (Buffer& b, const BandCurve& curve, double sr)
    {
        GraphicEq eq;
        eq.prepare (sr, 2);
        eq.setTargetCurve (curve);
        eq.setAmount (1.0f);
        eq.process (b);
    }

    void applyComp (Buffer& b, float thr, float ratio, float att, float rel, double sr)
    {
        GlueCompressor c;
        c.prepare (sr);
        c.setParameters (thr, ratio, att, rel, 6.0f);
        // process in blocks (the compressor is block based for meters only)
        const int n = b.getNumSamples();
        for (int s = 0; s < n; s += 4096)
        {
            const int len = juce::jmin (4096, n - s);
            Buffer view (b.getArrayOfWritePointers(), 2, s, len);
            c.process (view);
        }
    }

    float panToTheta (float pan) { return (juce::jlimit (-1.0f, 1.0f, pan) + 1.0f) * juce::MathConstants<float>::pi * 0.25f; }

    /** Collapse to mono and place with a constant-power pan law. */
    void panMono (Buffer& b, float pan)
    {
        const float th = panToTheta (pan);
        const float gl = std::sqrt (2.0f) * std::cos (th), gr = std::sqrt (2.0f) * std::sin (th);
        float* l = b.getWritePointer (0);
        float* r = b.getWritePointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float m = 0.5f * (l[i] + r[i]);
            l[i] = m * gl;
            r[i] = m * gr;
        }
    }

    /** Moves the energy balance of a stereo track from one pan position to another. */
    void balanceStereo (Buffer& b, float from, float to)
    {
        const float bs = std::sin (juce::jlimit (-1.0f, 1.0f, from) * juce::MathConstants<float>::halfPi);
        const float bt = std::sin (juce::jlimit (-1.0f, 1.0f, to) * juce::MathConstants<float>::halfPi);
        float gl = std::sqrt (juce::jmax (0.02f, 1.0f - bt) / juce::jmax (0.02f, 1.0f - bs));
        float gr = std::sqrt (juce::jmax (0.02f, 1.0f + bt) / juce::jmax (0.02f, 1.0f + bs));
        const float norm = std::sqrt (2.0f / (gl * gl + gr * gr));
        b.applyGain (0, 0, b.getNumSamples(), gl * norm);
        b.applyGain (1, 0, b.getNumSamples(), gr * norm);
    }

    void applySideGain (Buffer& b, float g)
    {
        float* l = b.getWritePointer (0);
        float* r = b.getWritePointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            const float m = 0.5f * (l[i] + r[i]), s = 0.5f * (l[i] - r[i]) * g;
            l[i] = m + s;
            r[i] = m - s;
        }
    }

    /** Decorrelated copy of the mid signal (Schroeder all-pass chain, high-passed). */
    Buffer decorrelatedSide (const Buffer& b, double sr, float hpHz)
    {
        const int n = b.getNumSamples();
        Buffer d (1, n);
        float* out = d.getWritePointer (0);
        const float* l = b.getReadPointer (0);
        const float* r = b.getReadPointer (1);
        for (int i = 0; i < n; ++i) out[i] = 0.5f * (l[i] + r[i]);

        for (double ms : { 5.3, 8.9, 13.7, 19.1 })
        {
            const int D = juce::jmax (1, (int) (ms * 0.001 * sr));
            const float g = 0.55f;
            std::vector<float> xBuf ((size_t) D, 0.0f), yBuf ((size_t) D, 0.0f);
            int pos = 0;
            for (int i = 0; i < n; ++i)
            {
                const float x = out[i];
                const float y = -g * x + xBuf[(size_t) pos] + g * yBuf[(size_t) pos];
                xBuf[(size_t) pos] = x;
                yBuf[(size_t) pos] = y;
                pos = (pos + 1) % D;
                out[i] = y;
            }
        }

        // 2nd-order high-pass so the low end stays mono
        juce::IIRFilter hp1, hp2;
        hp1.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, hpHz));
        hp2.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, hpHz));
        hp1.processSamples (out, n);
        hp2.processSamples (out, n);
        return d;
    }

    void addSide (Buffer& b, const Buffer& side, float k)
    {
        const float* s = side.getReadPointer (0);
        float* l = b.getWritePointer (0);
        float* r = b.getWritePointer (1);
        for (int i = 0; i < b.getNumSamples(); ++i)
        {
            l[i] += k * s[i];
            r[i] -= k * s[i];
        }
    }

    Buffer makeReverbWet (const Buffer& dry, double sr, float decaySec, float damping)
    {
        juce::Reverb rv;
        rv.setSampleRate (sr);
        juce::Reverb::Parameters p;
        p.roomSize = juce::jlimit (0.30f, 0.96f, 0.30f + (decaySec - 0.3f) / 3.0f * 0.66f);
        p.damping = damping;
        p.wetLevel = 1.0f;
        p.dryLevel = 0.0f;
        p.width = 1.0f;
        rv.setParameters (p);

        // short pre-delay (20 ms) keeps the source clear
        const int pre = (int) (0.02 * sr);
        const int n = dry.getNumSamples();
        const int tail = (int) (decaySec * sr);
        Buffer wet (2, n);
        Buffer in (2, n + tail);
        in.clear();
        for (int ch = 0; ch < 2; ++ch)
            if (n - pre > 0) in.copyFrom (ch, pre, dry, ch, 0, n - pre);

        // high-pass the send
        juce::IIRFilter hpL, hpR;
        hpL.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 180.0));
        hpR.setCoefficients (juce::IIRCoefficients::makeHighPass (sr, 180.0));
        hpL.processSamples (in.getWritePointer (0), n);
        hpR.processSamples (in.getWritePointer (1), n);

        for (int s = 0; s < n; s += 4096)
        {
            const int len = juce::jmin (4096, n - s);
            rv.processStereo (in.getWritePointer (0, s), in.getWritePointer (1, s), len);
        }
        wet.copyFrom (0, 0, in, 0, 0, n);
        wet.copyFrom (1, 0, in, 1, 0, n);
        return wet;
    }

    Buffer sumOf (const std::vector<MixTrack>& tracks, const std::vector<int>& idx, bool processed, int length)
    {
        Buffer sum (2, length);
        sum.clear();
        for (int i : idx)
        {
            const auto& src = processed ? tracks[(size_t) i].processed : tracks[(size_t) i].audio;
            for (int ch = 0; ch < 2; ++ch)
                sum.addFrom (ch, 0, src, ch, 0, juce::jmin (length, src.getNumSamples()));
        }
        return sum;
    }

    template <typename Fn>
    float bisect (float lo, float hi, float goal, int iterations, Fn measure)
    {
        // measure() must increase with x
        for (int i = 0; i < iterations; ++i)
        {
            const float mid = 0.5f * (lo + hi);
            if (measure (mid) < goal) lo = mid; else hi = mid;
        }
        return 0.5f * (lo + hi);
    }

    juce::String db (float v, int dec = 1) { return (v >= 0.0f ? "+" : "") + juce::String (v, dec) + " dB"; }
    juce::String pct (float v) { return juce::String (juce::roundToInt (v * 100.0f)) + "%"; }
    juce::String panStr (float p)
    {
        const int v = juce::roundToInt (p * 100.0f);
        if (std::abs (v) < 5) return "C";
        return (v < 0 ? "L " : "R ") + juce::String (std::abs (v));
    }
}

//==============================================================================
const StemProfile* AutoMixer::targetFor (StemType t, const ReferenceMixProfile& ref)
{
    auto usable = [] (const StemProfile* s)
    {
        return s != nullptr && s->profile.valid && s->levelRelMixDb > -35.0f && s->profile.activeFraction > 0.03f;
    };
    if (auto* s = ref.find (t); usable (s)) return s;
    // guitar / piano missing (4-stem model or not in the song): follow "other"
    if (t == StemType::guitar || t == StemType::piano)
        if (auto* s = ref.find (StemType::other); usable (s)) return s;
    if (t == StemType::other)
    {
        for (auto alt : { StemType::guitar, StemType::piano })
            if (auto* s = ref.find (alt); usable (s)) return s;
    }
    return nullptr;
}

juce::StringArray AutoMixer::describeReferenceStem (const StemProfile& s)
{
    const auto& p = s.profile;
    juce::StringArray l;
    l.add ("Volume: " + db (s.levelRelMixDb) + " rispetto al mix");
    l.add ("Posizione: " + panStr (p.pan) + "   " + p.widthText() + " (" + pct (p.width) + ")");
    l.add ("Profondita': " + p.depthText() + " (" + pct (p.depth) + ")");
    l.add ("Riverbero: " + p.reverbText() + (p.rt60 > 0.0f ? "  RT60 ~" + juce::String (p.rt60, 1) + " s" : juce::String()));
    l.add ("Tono: centro " + formatFrequency (p.centroidHz) + "Hz, tilt " + juce::String (p.tiltDbPerOct, 1) + " dB/oct");
    l.add ("Dinamica: crest " + juce::String (p.crestDb, 1) + " dB, LRA " + juce::String (p.loudnessRange, 1) + " LU");
    return l;
}

//==============================================================================
MixResult AutoMixer::run (std::vector<MixTrack>& tracks, const ReferenceMixProfile& ref, double sr,
                          const MixAmounts& amt, Progress progress)
{
    MixResult res;
    res.sampleRate = sr;
    if (tracks.empty()) { res.error = "Nessuna traccia da mixare."; return res; }
    if (ref.stems.empty()) { res.error = "Analizza prima il reference (separazione degli strumenti)."; return res; }

    int length = 0;
    for (auto& t : tracks) length = juce::jmax (length, t.audio.getNumSamples());

    auto report = [&] (float p, const juce::String& msg) { return ! progress || progress (juce::jlimit (0.0f, 1.0f, p), msg); };

    // ---- 1. analyse every track as it is ----------------------------------------
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        if (! report (0.25f * (float) i / (float) tracks.size(), "Analisi traccia: " + tracks[i].name)) return res;
        tracks[i].before = AudioAnalyzer::analyse (tracks[i].audio, sr);
        tracks[i].processed = tracks[i].audio;
        tracks[i].settings = {};
        tracks[i].settings.panBefore = tracks[i].before.pan;
        tracks[i].settings.panAfter = tracks[i].before.pan;
    }

    std::vector<int> all;
    for (int i = 0; i < (int) tracks.size(); ++i) all.push_back (i);
    res.original = sumOf (tracks, all, false, length);
    const float origMixLufs = AudioAnalyzer::measureLufs (res.original, sr);

    // ---- 2. per instrument group -------------------------------------------------
    std::map<int, std::vector<int>> groups;
    for (int i = 0; i < (int) tracks.size(); ++i) groups[(int) tracks[(size_t) i].type].push_back (i);

    std::map<int, float> groupOrigLufs, groupProcLufs;
    int gi = 0;
    for (auto& [typeInt, idx] : groups)
    {
        const auto type = (StemType) typeInt;
        const float p0 = 0.25f + 0.6f * (float) gi / (float) groups.size();
        const float pw = 0.6f / (float) groups.size();
        ++gi;

        const auto* target = targetFor (type, ref);
        auto groupSrc = sumOf (tracks, idx, false, length);
        const auto srcProf = AudioAnalyzer::analyse (groupSrc, sr);
        groupOrigLufs[typeInt] = srcProf.integratedLufs;

        if (target == nullptr || ! srcProf.valid)
        {
            groupProcLufs[typeInt] = srcProf.integratedLufs;
            if (target == nullptr)
                res.warnings.add ("Nessun riferimento per '" + stemName (type) + "': tracce lasciate col bilanciamento originale.");
            continue;
        }

        const auto& tp = target->profile;
        for (int i : idx)
        {
            tracks[(size_t) i].settings.hasTarget = true;
            tracks[(size_t) i].settings.targetStem = target->type;
        }

        // -- EQ
        if (! report (p0, stemName (type) + ": EQ di matching")) return res;
        if (amt.eq > 0.0f)
        {
            MatchCurveOptions o;
            o.amount = amt.eq;
            o.maxBoostDb = 8.0f;
            o.maxCutDb = 10.0f;
            const auto curve = computeMatchCurve (srcProf, tp, o);
            for (int i : idx)
            {
                applyEq (tracks[(size_t) i].processed, curve, sr);
                tracks[(size_t) i].settings.eqCurve = curve;
            }
        }

        // -- dynamics
        const float crestDiff = srcProf.crestDb - tp.crestDb;
        if (amt.dynamics > 0.0f && crestDiff > 1.5f)
        {
            if (! report (p0 + pw * 0.15f, stemName (type) + ": compressione")) return res;
            const float ratio = juce::jlimit (1.2f, 6.0f, 1.0f + juce::jmin (crestDiff, 12.0f) / 3.0f * amt.dynamics);
            float att = 10.0f, rel = 140.0f;
            switch (type)
            {
                case StemType::drums:  att = 22.0f; rel = 110.0f; break;
                case StemType::bass:   att = 12.0f; rel = 160.0f; break;
                case StemType::vocals: att = 5.0f;  rel = 120.0f; break;
                default: break;
            }
            for (int i : idx)
            {
                auto& t = tracks[(size_t) i];
                if (! t.before.valid) continue;
                const float thr = t.before.rmsDb + 3.0f - juce::jmin (crestDiff, 10.0f) * 0.4f * amt.dynamics;
                applyComp (t.processed, thr, ratio, att, rel, sr);
                t.settings.compressed = true;
                t.settings.compThresholdDb = thr;
                t.settings.compRatio = ratio;
                t.settings.compAttackMs = att;
                t.settings.compReleaseMs = rel;
            }
        }
        else if (crestDiff < -3.0f)
        {
            res.warnings.add (stemName (type) + ": nel reference e' piu' dinamico del tuo (" + juce::String (-crestDiff, 1)
                              + " dB di crest in piu'). Riduci la compressione su queste tracce.");
        }

        // -- pan
        if (! report (p0 + pw * 0.3f, stemName (type) + ": posizione nello stereo")) return res;
        const float panGoal = srcProf.pan + (tp.pan - srcProf.pan) * amt.space;
        {
            std::vector<int> monoTracks;
            for (int i : idx)
                if (tracks[(size_t) i].before.width < 0.08f) monoTracks.push_back (i);

            if (idx.size() == 1 || monoTracks.size() < 2 || tp.width < 0.2f)
            {
                for (int i : idx)
                {
                    auto& t = tracks[(size_t) i];
                    if (t.before.width < 0.08f)
                    {
                        const float trackGoal = idx.size() == 1 ? panGoal : t.before.pan + (panGoal - srcProf.pan);
                        panMono (t.processed, trackGoal);
                        t.settings.pannedAsMono = true;
                        t.settings.panAfter = trackGoal;
                    }
                    else
                    {
                        const float trackGoal = juce::jlimit (-1.0f, 1.0f, t.before.pan + (panGoal - srcProf.pan));
                        balanceStereo (t.processed, t.before.pan, trackGoal);
                        t.settings.panAfter = trackGoal;
                    }
                }
            }
            else
            {
                // several mono parts and a wide target: spread them across the field
                const float spread = juce::jlimit (0.2f, 0.85f, tp.width * 1.3f) * amt.space;
                const int m = (int) monoTracks.size();
                for (int k = 0; k < m; ++k)
                {
                    auto& t = tracks[(size_t) monoTracks[(size_t) k]];
                    const float pos = m == 1 ? 0.0f : -1.0f + 2.0f * (float) k / (float) (m - 1);
                    const float goal = juce::jlimit (-1.0f, 1.0f, panGoal + spread * pos);
                    panMono (t.processed, goal);
                    t.settings.pannedAsMono = true;
                    t.settings.panAfter = goal;
                }
                for (int i : idx)
                {
                    auto& t = tracks[(size_t) i];
                    if (t.before.width >= 0.08f)
                    {
                        const float goal = juce::jlimit (-1.0f, 1.0f, t.before.pan + (panGoal - srcProf.pan));
                        balanceStereo (t.processed, t.before.pan, goal);
                        t.settings.panAfter = goal;
                    }
                }
            }
        }

        // -- reverb / depth
        if (amt.reverb > 0.0f)
        {
            if (! report (p0 + pw * 0.45f, stemName (type) + ": profondita' e riverbero")) return res;
            auto dryGroup = sumOf (tracks, idx, true, length);
            const auto dryEx = excerpt (dryGroup, sr);
            const float rNow = AudioAnalyzer::analyse (dryEx, sr).reverbAmount;
            const float rGoal = rNow + (tp.reverbAmount - rNow) * amt.reverb;

            if (rGoal > rNow + 0.05f)
            {
                const float decay = tp.rt60 > 0.1f ? juce::jlimit (0.4f, 4.0f, tp.rt60) : 1.4f;
                const float damping = juce::jlimit (0.2f, 0.8f, 0.35f + 0.4f * tp.depth);
                std::vector<Buffer> wets;
                Buffer wetGroup (2, length);
                wetGroup.clear();
                for (int i : idx)
                {
                    wets.push_back (makeReverbWet (tracks[(size_t) i].processed, sr, decay, damping));
                    for (int ch = 0; ch < 2; ++ch) wetGroup.addFrom (ch, 0, wets.back(), ch, 0, length);
                }
                const auto wetEx = excerpt (wetGroup, sr);

                const float sendDb = bisect (-40.0f, 0.0f, rGoal, 6, [&] (float dbv)
                {
                    Buffer test (dryEx);
                    const float g = juce::Decibels::decibelsToGain (dbv);
                    for (int ch = 0; ch < 2; ++ch) test.addFrom (ch, 0, wetEx, ch, 0, test.getNumSamples(), g);
                    return AudioAnalyzer::analyse (test, sr).reverbAmount;
                });

                const float g = juce::Decibels::decibelsToGain (sendDb);
                for (size_t k = 0; k < idx.size(); ++k)
                {
                    auto& t = tracks[(size_t) idx[k]];
                    for (int ch = 0; ch < 2; ++ch) t.processed.addFrom (ch, 0, wets[k], ch, 0, length, g);
                    t.settings.reverbSendDb = sendDb;
                    t.settings.reverbDecaySec = decay;
                    t.settings.reverbDamping = damping;
                    t.settings.reverbRoom = juce::jlimit (0.30f, 0.96f, 0.30f + (decay - 0.3f) / 3.0f * 0.66f);
                }
            }
        }

        // -- width
        if (amt.space > 0.0f)
        {
            if (! report (p0 + pw * 0.75f, stemName (type) + ": larghezza stereo")) return res;
            auto grp = sumOf (tracks, idx, true, length);
            const auto ex = excerpt (grp, sr);
            const float wNow = AudioAnalyzer::measureWidth (ex, sr);
            const float wGoal = juce::jlimit (0.0f, 0.95f, wNow + (tp.width - wNow) * amt.space);

            if (std::abs (wGoal - wNow) > 0.03f)
            {
                if (wNow < 0.05f && wGoal > wNow)
                {
                    const float hpHz = (type == StemType::bass || type == StemType::drums) ? 220.0f : 140.0f;
                    const auto sideEx = decorrelatedSide (ex, sr, hpHz);
                    const float k = bisect (0.0f, 1.5f, wGoal, 7, [&] (float kk)
                    {
                        Buffer t (ex);
                        addSide (t, sideEx, kk);
                        return AudioAnalyzer::measureWidth (t, sr);
                    });
                    for (int i : idx)
                    {
                        auto& t = tracks[(size_t) i];
                        const auto side = decorrelatedSide (t.processed, sr, hpHz);
                        addSide (t.processed, side, k);
                        t.settings.stereoize = k;
                    }
                }
                else
                {
                    const float sDb = bisect (-40.0f, 12.0f, wGoal, 8, [&] (float dbv)
                    {
                        Buffer t (ex);
                        applySideGain (t, juce::Decibels::decibelsToGain (dbv, -100.0f));
                        return AudioAnalyzer::measureWidth (t, sr);
                    });
                    for (int i : idx)
                    {
                        applySideGain (tracks[(size_t) i].processed, juce::Decibels::decibelsToGain (sDb, -100.0f));
                        tracks[(size_t) i].settings.sideGainDb = sDb;
                    }
                }
            }
        }

        // -- final position check: reverb returns and width changes move the balance
        if (amt.space > 0.0f)
        {
            auto grp = sumOf (tracks, idx, true, length);
            const auto prof = AudioAnalyzer::analyse (excerpt (grp, sr), sr);
            if (prof.valid && std::abs (prof.pan - panGoal) > 0.04f)
                for (int i : idx)
                    balanceStereo (tracks[(size_t) i].processed, prof.pan, panGoal);
        }

        auto grp = sumOf (tracks, idx, true, length);
        groupProcLufs[typeInt] = AudioAnalyzer::measureLufs (grp, sr);
    }

    // ---- 3. levels ----------------------------------------------------------------
    if (! report (0.87f, "Bilanciamento dei volumi")) return res;
    const float g0 = amt.targetLufs - origMixLufs;
    std::map<int, float> groupGain;
    for (auto& [typeInt, idx] : groups)
    {
        const float keepBalance = g0 - (groupProcLufs[typeInt] - groupOrigLufs[typeInt]);
        const auto* target = targetFor ((StemType) typeInt, ref);
        if (target == nullptr || groupProcLufs[typeInt] <= -69.0f)
        {
            groupGain[typeInt] = keepBalance;
            continue;
        }
        const float matched = (amt.targetLufs + target->levelRelMixDb) - groupProcLufs[typeInt];
        groupGain[typeInt] = (1.0f - amt.level) * keepBalance + amt.level * matched;
    }

    auto buildMix = [&] (float globalDb)
    {
        Buffer mix (2, length);
        mix.clear();
        for (auto& [typeInt, idx] : groups)
            for (int i : idx)
            {
                const float g = juce::Decibels::decibelsToGain (groupGain[typeInt] + globalDb);
                for (int ch = 0; ch < 2; ++ch)
                    mix.addFrom (ch, 0, tracks[(size_t) i].processed, ch, 0, juce::jmin (length, tracks[(size_t) i].processed.getNumSamples()), g);
            }
        return mix;
    };

    auto mix = buildMix (0.0f);
    float global = amt.targetLufs - AudioAnalyzer::measureLufs (mix, sr);
    mix = buildMix (global);
    const float tpDb = AudioAnalyzer::measureTruePeakDb (mix);
    if (tpDb > -1.0f)
    {
        global -= (tpDb + 1.0f);
        mix = buildMix (global);
        res.warnings.add ("Mix abbassato di " + juce::String (tpDb + 1.0f, 1) + " dB per lasciare headroom (picchi a -1 dBTP): il volume finale lo dara' il mastering.");
    }

    for (auto& [typeInt, idx] : groups)
        for (int i : idx)
        {
            auto& t = tracks[(size_t) i];
            t.settings.gainDb = groupGain[typeInt] + global;
            t.processed.applyGain (juce::Decibels::decibelsToGain (t.settings.gainDb));
        }

    // ---- 4. verification ------------------------------------------------------------
    for (size_t i = 0; i < tracks.size(); ++i)
    {
        if (! report (0.9f + 0.08f * (float) i / (float) tracks.size(), "Verifica: " + tracks[i].name)) return res;
        tracks[i].after = AudioAnalyzer::analyse (tracks[i].processed, sr);
    }
    res.mix = std::move (mix);
    res.mixBefore = AudioAnalyzer::analyse (res.original, sr);
    res.mixAfter = AudioAnalyzer::analyse (res.mix, sr);
    res.ok = true;
    report (1.0f, "Mix completato");
    return res;
}

//==============================================================================
juce::String AutoMixer::buildReport (const std::vector<MixTrack>& tracks, const ReferenceMixProfile& ref,
                                     const MixResult& res, const MixAmounts& amt)
{
    juce::String s;
    auto line = [&s] (const juce::String& l = {}) { s << l << juce::newLine; };

    line ("ALPHA MONSTER MIX - REPORT");
    line ("Generato: " + juce::Time::getCurrentTime().toString (true, true));
    line ("Reference: " + ref.sourceName);
    line();
    line ("=== REFERENCE: COME E' MIXATO ===");
    line ("Mix: " + juce::String (ref.mix.integratedLufs, 1) + " LUFS, true peak " + juce::String (ref.mix.truePeakDb, 1)
          + " dBTP, LRA " + juce::String (ref.mix.loudnessRange, 1) + " LU, " + ref.mix.widthText() + " (" + pct (ref.mix.width) + ")");
    for (auto& st : ref.stems)
    {
        line();
        line ("[" + stemName (st.type).toUpperCase() + "]");
        for (auto& l : describeReferenceStem (st)) line ("  " + l);
    }

    line();
    line ("=== IL TUO MIX: IMPOSTAZIONI APPLICATE ===");
    line ("Quantita': livelli " + pct (amt.level) + ", EQ " + pct (amt.eq) + ", dinamica " + pct (amt.dynamics)
          + ", spazio " + pct (amt.space) + ", riverbero " + pct (amt.reverb) + ", target " + juce::String (amt.targetLufs, 1) + " LUFS");
    for (auto& t : tracks)
    {
        const auto& st = t.settings;
        line();
        line ("Traccia: " + t.name + "   (" + stemName (t.type) + (st.hasTarget ? " -> segue " + stemName (st.targetStem) : juce::String (", nessun riferimento")) + ")");
        line ("  Fader:        " + db (st.gainDb));
        line ("  Pan:          " + panStr (st.panBefore) + " -> " + panStr (st.panAfter) + (st.pannedAsMono ? "  (sorgente mono)" : "  (bilanciamento stereo)"));
        if (st.stereoize > 0.0f)
            line ("  Larghezza:    pseudo-stereo " + pct (st.stereoize / 1.5f) + " (sopra 140-220 Hz)");
        else if (std::abs (st.sideGainDb) > 0.2f)
            line ("  Larghezza:    side " + db (st.sideGainDb) + " (M/S)");
        else
            line ("  Larghezza:    invariata");
        auto eq = describeCurve (st.eqCurve, 1.0f, 6);
        line ("  EQ:           " + (eq.isEmpty() ? juce::String ("nessuna correzione importante") : eq.joinIntoString ("; ")));
        if (st.compressed)
            line ("  Compressore:  soglia " + juce::String (st.compThresholdDb, 1) + " dBFS, ratio " + juce::String (st.compRatio, 1)
                  + ":1, attack " + juce::String ((int) st.compAttackMs) + " ms, release " + juce::String ((int) st.compReleaseMs) + " ms (RMS, knee 6 dB)");
        else
            line ("  Compressore:  non necessario");
        if (st.reverbSendDb > -99.0f)
            line ("  Riverbero:    send " + db (st.reverbSendDb) + ", decay ~" + juce::String (st.reverbDecaySec, 1) + " s, pre-delay 20 ms, HPF 180 Hz, damping "
                  + juce::String (st.reverbDamping, 2));
        else
            line ("  Riverbero:    nessuno aggiunto");
        if (t.after.valid)
            line ("  Risultato:    pan " + t.after.panText() + ", " + t.after.widthText() + " (" + pct (t.after.width) + "), riverbero "
                  + pct (t.after.reverbAmount) + ", profondita' " + t.after.depthText());
    }

    line();
    line ("=== MIX FINALE ===");
    line ("Prima: " + juce::String (res.mixBefore.integratedLufs, 1) + " LUFS, " + res.mixBefore.widthText() + " (" + pct (res.mixBefore.width) + ")");
    line ("Dopo:  " + juce::String (res.mixAfter.integratedLufs, 1) + " LUFS, true peak " + juce::String (res.mixAfter.truePeakDb, 1)
          + " dBTP, " + res.mixAfter.widthText() + " (" + pct (res.mixAfter.width) + ")");
    if (! res.warnings.isEmpty())
    {
        line();
        line ("Note:");
        for (auto& w : res.warnings) line ("  - " + w);
    }
    line();
    line ("Suggerimento: per il volume finale usa Alpha Monster Master con lo stesso reference.");
    return s;
}

juce::var AutoMixer::buildJson (const std::vector<MixTrack>& tracks, const ReferenceMixProfile& ref, const MixResult& res)
{
    auto* root = new juce::DynamicObject();
    root->setProperty ("reference", ref.toVar());
    juce::Array<juce::var> arr;
    for (auto& t : tracks)
    {
        const auto& st = t.settings;
        auto* o = new juce::DynamicObject();
        o->setProperty ("name", t.name);
        o->setProperty ("category", stemKey (t.type));
        o->setProperty ("follows", st.hasTarget ? stemKey (st.targetStem) : juce::String());
        o->setProperty ("gainDb", st.gainDb);
        o->setProperty ("panBefore", st.panBefore);
        o->setProperty ("panAfter", st.panAfter);
        o->setProperty ("pannedAsMono", st.pannedAsMono);
        o->setProperty ("sideGainDb", st.sideGainDb);
        o->setProperty ("stereoize", st.stereoize);
        juce::Array<juce::var> eq;
        for (auto v : st.eqCurve) eq.add ((double) v);
        o->setProperty ("eqCurveDb", eq);
        o->setProperty ("eqSummary", describeCurve (st.eqCurve, 1.0f, 6).joinIntoString ("; "));
        o->setProperty ("compressed", st.compressed);
        o->setProperty ("compThresholdDb", st.compThresholdDb);
        o->setProperty ("compRatio", st.compRatio);
        o->setProperty ("compAttackMs", st.compAttackMs);
        o->setProperty ("compReleaseMs", st.compReleaseMs);
        o->setProperty ("reverbSendDb", st.reverbSendDb);
        o->setProperty ("reverbDecaySec", st.reverbDecaySec);
        o->setProperty ("before", t.before.toVar());
        o->setProperty ("after", t.after.toVar());
        arr.add (juce::var (o));
    }
    root->setProperty ("tracks", arr);
    root->setProperty ("mixBefore", res.mixBefore.toVar());
    root->setProperty ("mixAfter", res.mixAfter.toVar());
    juce::Array<juce::var> w;
    for (auto& s : res.warnings) w.add (s);
    root->setProperty ("warnings", w);
    return juce::var (root);
}

} // namespace am
