#include "MasterProcessor.h"
#include "MasterEditor.h"
#include "Analysis/AudioFileUtils.h"

namespace am
{

namespace ids
{
    static const juce::String input = "input", eqAmount = "eqAmount", compAmount = "compAmount",
                              widthAmount = "widthAmount", drive = "drive", ceiling = "ceiling",
                              output = "output", mix = "mix", eqOn = "eqOn", compOn = "compOn",
                              widthOn = "widthOn", limOn = "limOn", bassMono = "bassMono";
}

juce::AudioProcessorValueTreeState::ParameterLayout MasterProcessor::createLayout()
{
    using namespace juce;
    AudioProcessorValueTreeState::ParameterLayout l;
    auto db = [] (const String& id, const String& name, float lo, float hi, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (lo, hi, 0.1f), def,
                                                      AudioParameterFloatAttributes().withLabel ("dB"));
    };
    auto pct = [] (const String& id, const String& name, float def)
    {
        return std::make_unique<AudioParameterFloat> (ParameterID { id, 1 }, name, NormalisableRange<float> (0.0f, 100.0f, 1.0f), def,
                                                      AudioParameterFloatAttributes().withLabel ("%"));
    };
    auto onOff = [] (const String& id, const String& name, bool def)
    {
        return std::make_unique<AudioParameterBool> (ParameterID { id, 1 }, name, def);
    };

    l.add (db (ids::input, "Input", -12.0f, 12.0f, 0.0f));
    l.add (pct (ids::eqAmount, "Match EQ", 80.0f));
    l.add (pct (ids::compAmount, "Glue", 40.0f));
    l.add (pct (ids::widthAmount, "Width Match", 100.0f));
    l.add (db (ids::drive, "Drive", -12.0f, 24.0f, 0.0f));
    l.add (db (ids::ceiling, "Ceiling", -3.0f, 0.0f, -1.0f));
    l.add (db (ids::output, "Output", -12.0f, 0.0f, 0.0f));
    l.add (pct (ids::mix, "Mix", 100.0f));
    l.add (onOff (ids::eqOn, "EQ On", true));
    l.add (onOff (ids::compOn, "Glue On", true));
    l.add (onOff (ids::widthOn, "Width On", true));
    l.add (onOff (ids::limOn, "Limiter On", true));
    l.add (onOff (ids::bassMono, "Bass Mono", false));
    return l;
}

MasterProcessor::MasterProcessor()
    : AudioProcessor (BusesProperties().withInput ("Input", juce::AudioChannelSet::stereo(), true)
                                       .withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      params (*this, nullptr, "AlphaMonsterMaster", createLayout())
{
    for (auto& s : sideBaseDb) s.store (0.0f);
    learnL.resize ((size_t) learnFifo.getTotalSize());
    learnR.resize ((size_t) learnFifo.getTotalSize());
    scopeL.resize ((size_t) scopeFifo.getTotalSize());
    scopeR.resize ((size_t) scopeFifo.getTotalSize());
}

MasterProcessor::~MasterProcessor()
{
    learnThreadRun.store (false);
    if (learnThread && learnThread->joinable()) learnThread->join();
    pool.removeAllJobs (true, 20000);
    cancelPendingUpdate();
}

bool MasterProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // mastering is a stereo job: stereo in, stereo out
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo()
        && layouts.getMainInputChannelSet() == juce::AudioChannelSet::stereo();
}

void MasterProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    fs = sampleRate;
    blockSize = juce::jmax (1, samplesPerBlock);

    eq.prepare (fs, 2);
    comp.prepare (fs);
    width.prepare (fs, blockSize);
    limiter.prepare (fs, blockSize);
    setLatencySamples (limiter.getLatencySamples());

    dryDelay.prepare ({ fs, (juce::uint32) blockSize, 2 });
    dryDelay.setMaximumDelayInSamples (juce::jmax (64, limiter.getLatencySamples() + 8));
    dryDelay.setDelay ((float) limiter.getLatencySamples());
    dryBuffer.setSize (2, blockSize);

    for (auto* s : { &inGain, &driveGain, &outGain, &mixAmt })
        s->reset (fs, 0.03);

    outLoudness.prepare (fs, 2);
    learnAnalyzer.prepare (fs, false);
    learnAnalyzer.setGateDb (-60.0f);

    // resample reference audio for A/B if the host rate changed
    {
        const juce::SpinLock::ScopedLockType sl (refAudioLock);
        if (refAudio.getNumSamples() > 0 && std::abs (refAudioRate - fs) > 0.5)
        {
            refAudio = resample (refAudio, refAudioRate, fs);
            refAudioRate = fs;
            refPlayPos = 0;
        }
    }
}

//==============================================================================
void MasterProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    juce::ScopedNoDenormals noDenormals;
    // some hosts send blocks larger than announced: work in slices
    const int total = buffer.getNumSamples();
    for (int start = 0; start < total; start += blockSize)
    {
        const int len = juce::jmin (blockSize, total - start);
        juce::AudioBuffer<float> slice (buffer.getArrayOfWritePointers(), buffer.getNumChannels(), start, len);
        processSlice (slice);
    }
}

void MasterProcessor::processSlice (juce::AudioBuffer<float>& buffer)
{
    const int n = buffer.getNumSamples();
    const int nCh = buffer.getNumChannels();
    if (n == 0 || nCh == 0) return;

    auto p = [this] (const juce::String& id) { return params.getRawParameterValue (id)->load(); };

    inGain.setTargetValue (juce::Decibels::decibelsToGain (p (ids::input)));
    driveGain.setTargetValue (juce::Decibels::decibelsToGain (p (ids::drive)));
    outGain.setTargetValue (juce::Decibels::decibelsToGain (p (ids::output)));
    mixAmt.setTargetValue (p (ids::mix) * 0.01f);

    // input gain
    {
        auto start = inGain.getCurrentValue();
        inGain.skip (n);
        buffer.applyGainRamp (0, n, start, inGain.getCurrentValue());
    }
    float inPk = 0.0f;
    for (int ch = 0; ch < nCh; ++ch) inPk = juce::jmax (inPk, buffer.getMagnitude (ch, 0, n));
    meters.inPeakDb.store (juce::Decibels::gainToDecibels (inPk, -100.0f));

    const float* inL = buffer.getReadPointer (0);
    const float* inR = buffer.getReadPointer (nCh > 1 ? 1 : 0);

    // learn: hand the input to the analysis thread
    if (learning.load())
    {
        int s1, n1, s2, n2;
        const int count = juce::jmin (n, learnFifo.getFreeSpace());
        learnFifo.prepareToWrite (count, s1, n1, s2, n2);
        if (n1 > 0) { std::copy (inL, inL + n1, learnL.begin() + s1); std::copy (inR, inR + n1, learnR.begin() + s1); }
        if (n2 > 0) { std::copy (inL + n1, inL + n1 + n2, learnL.begin() + s2); std::copy (inR + n1, inR + n1 + n2, learnR.begin() + s2); }
        learnFifo.finishedWrite (n1 + n2);
    }

    // dry path (latency aligned)
    for (int ch = 0; ch < 2; ++ch)
    {
        const float* src = buffer.getReadPointer (juce::jmin (ch, nCh - 1));
        float* d = dryBuffer.getWritePointer (ch);
        for (int i = 0; i < n; ++i)
        {
            dryDelay.pushSample (ch, src[i]);
            d[i] = dryDelay.popSample (ch);
        }
    }

    // work in stereo
    juce::AudioBuffer<float> work (buffer.getArrayOfWritePointers(), juce::jmin (nCh, 2), n);
    if (nCh == 1)
    {
        // mono host bus: process a temporary stereo copy
        dryBuffer.copyFrom (1, 0, dryBuffer, 0, 0, n);
    }

    if (p (ids::eqOn) > 0.5f)
    {
        eq.setAmount (p (ids::eqAmount) * 0.01f);
        eq.process (work);
    }

    if (p (ids::compOn) > 0.5f)
    {
        const float amt = p (ids::compAmount) * 0.01f;
        const float thr = compBaseThreshold.load() + 6.0f - 12.0f * amt;
        const float ratio = 1.2f + 2.8f * amt;
        comp.setParameters (thr, ratio, 20.0f - 10.0f * amt, 180.0f, 8.0f);
        comp.process (work);
        meters.compGr.store (comp.getGainReductionDb());
    }
    else meters.compGr.store (0.0f);

    if (nCh > 1 && p (ids::widthOn) > 0.5f)
    {
        const float amt = p (ids::widthAmount) * 0.01f;
        float g[3];
        for (int b = 0; b < 3; ++b)
            g[b] = juce::Decibels::decibelsToGain (sideBaseDb[(size_t) b].load() * amt, -60.0f);
        if (p (ids::bassMono) > 0.5f) g[0] = 0.0f;
        width.setSideGains (g[0], g[1], g[2]);
        width.process (work);
    }
    else if (nCh > 1 && p (ids::bassMono) > 0.5f)
    {
        width.setSideGains (0.0f, 1.0f, 1.0f);
        width.process (work);
    }

    {
        auto start = driveGain.getCurrentValue();
        driveGain.skip (n);
        work.applyGainRamp (0, n, start, driveGain.getCurrentValue());
    }

    if (nCh > 1)
    {
        limiter.setCeilingDb (p (ids::limOn) > 0.5f ? p (ids::ceiling) : 24.0f);
        limiter.process (work);
        meters.limGr.store (p (ids::limOn) > 0.5f ? limiter.getGainReductionDb() : 0.0f);
    }

    {
        auto start = outGain.getCurrentValue();
        outGain.skip (n);
        work.applyGainRamp (0, n, start, outGain.getCurrentValue());
    }

    // dry / wet
    for (int ch = 0; ch < juce::jmin (nCh, 2); ++ch)
    {
        float* d = work.getWritePointer (ch);
        const float* dry = dryBuffer.getReadPointer (ch);
        auto m = mixAmt;
        for (int i = 0; i < n; ++i)
        {
            const float w = m.getNextValue();
            d[i] = dry[i] + (d[i] - dry[i]) * w;
        }
    }
    mixAmt.skip (n);

    // A/B: reference playback replaces the output
    if (listenRef.load())
    {
        const juce::SpinLock::ScopedTryLockType sl (refAudioLock);
        if (sl.isLocked() && refAudio.getNumSamples() > 0)
        {
            const int len = refAudio.getNumSamples();
            for (int i = 0; i < n; ++i)
            {
                for (int ch = 0; ch < juce::jmin (nCh, 2); ++ch)
                    work.setSample (ch, i, refAudio.getSample (ch, refPlayPos));
                refPlayPos = (refPlayPos + 1) % len;
            }
        }
    }

    // meters
    if (resetMeterRequested.exchange (false))
        outLoudness.reset();
    outLoudness.process (work);
    meters.outMomentary.store (outLoudness.getMomentaryLufs());
    meters.outShortTerm.store (outLoudness.getShortTermLufs());
    meters.outPeakLDb.store (juce::Decibels::gainToDecibels (work.getMagnitude (0, 0, n), -100.0f));
    meters.outPeakRDb.store (juce::Decibels::gainToDecibels (work.getMagnitude (juce::jmin (1, nCh - 1), 0, n), -100.0f));

    {
        const float* l = work.getReadPointer (0);
        const float* r = work.getReadPointer (juce::jmin (1, nCh - 1));
        double lr = 0, ll = 0, rr = 0;
        for (int i = 0; i < n; ++i) { lr += l[i] * r[i]; ll += l[i] * l[i]; rr += r[i] * r[i]; }
        const double d = std::sqrt (ll * rr);
        const float c = d > 1.0e-9 ? (float) (lr / d) : 1.0f;
        meters.correlation.store (0.9f * meters.correlation.load() + 0.1f * c);

        int s1, n1, s2, n2;
        const int count = juce::jmin (n, scopeFifo.getFreeSpace());
        scopeFifo.prepareToWrite (count, s1, n1, s2, n2);
        if (n1 > 0) { std::copy (l, l + n1, scopeL.begin() + s1); std::copy (r, r + n1, scopeR.begin() + s1); }
        if (n2 > 0) { std::copy (l + n1, l + n1 + n2, scopeL.begin() + s2); std::copy (r + n1, r + n1 + n2, scopeR.begin() + s2); }
        scopeFifo.finishedWrite (n1 + n2);
    }
}

int MasterProcessor::readScopeSamples (float* l, float* r, int maxSamples)
{
    int s1, n1, s2, n2;
    scopeFifo.prepareToRead (juce::jmin (maxSamples, scopeFifo.getNumReady()), s1, n1, s2, n2);
    std::copy (scopeL.begin() + s1, scopeL.begin() + s1 + n1, l);
    std::copy (scopeR.begin() + s1, scopeR.begin() + s1 + n1, r);
    std::copy (scopeL.begin() + s2, scopeL.begin() + s2 + n2, l + n1);
    std::copy (scopeR.begin() + s2, scopeR.begin() + s2 + n2, r + n1);
    scopeFifo.finishedRead (n1 + n2);
    return n1 + n2;
}

//==============================================================================
void MasterProcessor::setLearning (bool shouldLearn)
{
    if (shouldLearn == learning.load()) return;

    if (shouldLearn)
    {
        {
            std::lock_guard<std::mutex> lk (learnMutex);
            learnAnalyzer.prepare (fs, false);
            learnAnalyzer.setGateDb (-60.0f);
        }
        learnFifo.reset();
        learnSamples.store (0);
        learnThreadRun.store (true);
        learnThread = std::make_unique<std::thread> ([this] { learnThreadLoop(); });
        learning.store (true);
    }
    else
    {
        learning.store (false);
        learnThreadRun.store (false);
        if (learnThread && learnThread->joinable()) learnThread->join();
        learnThread.reset();

        SignalProfile p;
        {
            std::lock_guard<std::mutex> lk (learnMutex);
            p = learnAnalyzer.finish();
        }
        if (p.valid && p.durationSec > 2.0f)
        {
            {
                const juce::ScopedLock sl (profileLock);
                learned = p;
            }
            if (reference.valid)
                if (auto* d = params.getParameter (ids::drive))
                    d->setValueNotifyingHost (d->convertTo0to1 (juce::jlimit (-12.0f, 24.0f, reference.integratedLufs - p.integratedLufs)));
        }
        else
        {
            lastError = "Learn troppo breve o segnale assente: riproduci almeno 10-20 secondi del brano.";
        }
        recomputeTargets();
    }
}

void MasterProcessor::learnThreadLoop()
{
    std::vector<float> l (8192), r (8192);
    while (learnThreadRun.load() || learnFifo.getNumReady() > 0)
    {
        const int ready = juce::jmin (8192, learnFifo.getNumReady());
        if (ready == 0)
        {
            std::this_thread::sleep_for (std::chrono::milliseconds (20));
            continue;
        }
        int s1, n1, s2, n2;
        learnFifo.prepareToRead (ready, s1, n1, s2, n2);
        std::copy (learnL.begin() + s1, learnL.begin() + s1 + n1, l.begin());
        std::copy (learnR.begin() + s1, learnR.begin() + s1 + n1, r.begin());
        std::copy (learnL.begin() + s2, learnL.begin() + s2 + n2, l.begin() + n1);
        std::copy (learnR.begin() + s2, learnR.begin() + s2 + n2, r.begin() + n1);
        learnFifo.finishedRead (n1 + n2);

        std::lock_guard<std::mutex> lk (learnMutex);
        learnAnalyzer.process (l.data(), r.data(), n1 + n2);
        learnSamples.store (learnAnalyzer.getSamplesProcessed());
    }
}

float MasterProcessor::getLearnSeconds() const noexcept
{
    return (float) ((double) learnSamples.load() / fs);
}

juce::String MasterProcessor::getBusyText() const
{
    const juce::ScopedLock sl (profileLock);
    return busyText;
}

void MasterProcessor::loadReferenceFile (const juce::File& file)
{
    if (busy.load()) return;
    busy.store (true);
    {
        const juce::ScopedLock sl (profileLock);
        busyText = "Analisi reference: " + file.getFileName();
    }
    const double hostRate = fs;

    pool.addJob ([this, file, hostRate]
    {
        auto loaded = loadAudioFile (file);
        SignalProfile p;
        juce::AudioBuffer<float> playback;
        if (loaded.ok())
        {
            p = AudioAnalyzer::analyse (loaded.buffer, loaded.sampleRate);
            playback = resample (loaded.buffer, loaded.sampleRate, hostRate);
        }

        {
            const juce::ScopedLock sl (profileLock);
            if (p.valid)
            {
                pendingReference = p;
                pendingReferenceName = file.getFileNameWithoutExtension();
                referencePath = file.getFullPathName();
                hasPendingReference = true;
            }
            else lastError = loaded.ok() ? "Il file reference sembra silenzioso." : loaded.error;
        }
        if (playback.getNumSamples() > 0)
        {
            const juce::SpinLock::ScopedLockType sl (refAudioLock);
            refAudio = std::move (playback);
            refAudioRate = hostRate;
            refPlayPos = 0;
        }
        busy.store (false);
        triggerAsyncUpdate();
    });
}

void MasterProcessor::analyseOwnTrackFile (const juce::File& file)
{
    if (busy.load()) return;
    busy.store (true);
    {
        const juce::ScopedLock sl (profileLock);
        busyText = "Analisi del tuo brano: " + file.getFileName();
    }

    pool.addJob ([this, file]
    {
        auto loaded = loadAudioFile (file);
        SignalProfile p;
        if (loaded.ok())
            p = AudioAnalyzer::analyse (loaded.buffer, loaded.sampleRate);
        {
            const juce::ScopedLock sl (profileLock);
            if (p.valid) { pendingLearned = p; hasPendingLearned = true; }
            else lastError = loaded.ok() ? "Il file sembra silenzioso." : loaded.error;
        }
        busy.store (false);
        triggerAsyncUpdate();
    });
}

bool MasterProcessor::hasReferenceAudio() const
{
    return refAudio.getNumSamples() > 0;
}

void MasterProcessor::handleAsyncUpdate()
{
    bool learnedChanged = false;
    {
        const juce::ScopedLock sl (profileLock);
        if (hasPendingReference)
        {
            reference = pendingReference;
            referenceName = pendingReferenceName;
            hasPendingReference = false;
        }
        if (hasPendingLearned)
        {
            learned = pendingLearned;
            hasPendingLearned = false;
            learnedChanged = true;
        }
    }
    recomputeTargets();

    // first time both profiles exist: propose a drive that matches loudness
    if (learnedChanged && reference.valid && learned.valid)
        if (auto* d = params.getParameter (ids::drive))
            d->setValueNotifyingHost (d->convertTo0to1 (juce::jlimit (-12.0f, 24.0f, reference.integratedLufs - learned.integratedLufs)));

    if (onProfilesChanged) onProfilesChanged();
}

void MasterProcessor::recomputeTargets()
{
    SignalProfile ref, own;
    {
        const juce::ScopedLock sl (profileLock);
        ref = reference;
        own = learned;
    }

    if (ref.valid && own.valid)
    {
        MatchCurveOptions o;
        o.amount = 1.0f;
        eq.setTargetCurve (computeMatchCurve (own, ref, o));

        compBaseThreshold.store (own.rmsDb);

        for (size_t b = 0; b < 3; ++b)
        {
            auto rho = [] (float w) { w = juce::jlimit (0.0f, 0.98f, w); return w / (2.0f - w); };
            const float rs = juce::jmax (1.0e-4f, rho (own.regionWidth[b]));
            const float rt = rho (ref.regionWidth[b]);
            const float s = juce::jlimit (0.0f, 2.5f, std::sqrt (rt / rs));
            sideBaseDb[b].store (juce::jlimit (-24.0f, 8.0f, juce::Decibels::gainToDecibels (s, -60.0f)));
        }
    }
    else
    {
        eq.setTargetCurve ({});
        for (auto& s : sideBaseDb) s.store (0.0f);
        if (own.valid) compBaseThreshold.store (own.rmsDb);
    }

    if (! juce::MessageManager::getInstance()->isThisTheMessageThread())
        triggerAsyncUpdate();
    else if (onProfilesChanged)
        onProfilesChanged();
}

std::array<float, 3> MasterProcessor::getSideGainsDb() const
{
    return { sideBaseDb[0].load(), sideBaseDb[1].load(), sideBaseDb[2].load() };
}

bool MasterProcessor::matchLoudnessNow()
{
    if (! reference.valid) return false;
    const float measured = outLoudness.getIntegratedLufs();
    if (measured <= -69.0f) return false;

    if (auto* d = params.getParameter (ids::drive))
    {
        const float current = params.getRawParameterValue (ids::drive)->load();
        const float next = juce::jlimit (-12.0f, 24.0f, current + (reference.integratedLufs - measured));
        d->setValueNotifyingHost (d->convertTo0to1 (next));
    }
    resetOutputMeter();
    return true;
}

//==============================================================================
void MasterProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = params.copyState();
    juce::ValueTree extra ("AlphaMonster");
    {
        const juce::ScopedLock sl (profileLock);
        extra.setProperty ("reference", juce::JSON::toString (reference.toVar(), true), nullptr);
        extra.setProperty ("learned", juce::JSON::toString (learned.toVar(), true), nullptr);
        extra.setProperty ("referenceName", referenceName, nullptr);
        extra.setProperty ("referencePath", referencePath, nullptr);
    }
    state.removeChild (state.getChildWithName ("AlphaMonster"), nullptr);
    state.appendChild (extra, nullptr);
    if (auto xml = state.createXml())
        copyXmlToBinary (*xml, dest);
}

void MasterProcessor::setStateInformation (const void* data, int size)
{
    auto xml = getXmlFromBinary (data, size);
    if (xml == nullptr) return;
    auto state = juce::ValueTree::fromXml (*xml);
    if (! state.isValid()) return;

    auto extra = state.getChildWithName ("AlphaMonster");
    if (extra.isValid())
    {
        const juce::ScopedLock sl (profileLock);
        reference = SignalProfile::fromVar (juce::JSON::parse (extra["reference"].toString()));
        learned = SignalProfile::fromVar (juce::JSON::parse (extra["learned"].toString()));
        referenceName = extra["referenceName"].toString();
        referencePath = extra["referencePath"].toString();
    }
    state.removeChild (extra, nullptr);
    params.replaceState (state);
    recomputeTargets();

    // reload reference audio for A/B listening if the file is still there
    const juce::File f (referencePath);
    if (referencePath.isNotEmpty() && f.existsAsFile())
    {
        const double hostRate = fs;
        pool.addJob ([this, f, hostRate]
        {
            auto loaded = loadAudioFile (f, hostRate);
            if (loaded.ok())
            {
                const juce::SpinLock::ScopedLockType sl (refAudioLock);
                refAudio = std::move (loaded.buffer);
                refAudioRate = hostRate;
                refPlayPos = 0;
            }
        });
    }
}

juce::AudioProcessorEditor* MasterProcessor::createEditor()
{
    return new MasterEditor (*this);
}

} // namespace am

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new am::MasterProcessor();
}
