#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include "Analysis/AudioAnalyzer.h"
#include "Analysis/FilterDesign.h"
#include "Analysis/Loudness.h"
#include "Dsp/Dynamics.h"

namespace am
{

class MasterProcessor : public juce::AudioProcessor,
                        private juce::AsyncUpdater
{
public:
    MasterProcessor();
    ~MasterProcessor() override;

    //==========================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using AudioProcessor::processBlock;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "Alpha Monster Master"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Default"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    //==========================================================================
    juce::AudioProcessorValueTreeState params;

    // reference & learning (message thread)
    void loadReferenceFile (const juce::File&);
    void analyseOwnTrackFile (const juce::File&);
    void setLearning (bool shouldLearn);
    bool isLearning() const noexcept { return learning.load(); }
    bool isBusy() const noexcept { return busy.load(); }
    juce::String getBusyText() const;
    float getLearnSeconds() const noexcept;

    const SignalProfile& getReference() const noexcept { return reference; }
    const SignalProfile& getLearned() const noexcept { return learned; }
    juce::String getReferenceName() const { return referenceName; }
    BandCurve getEqCurve() const { return eq.getTargetCurve(); }
    float getEqResponseDb (float hz) const { return eq.responseDb (hz); }
    std::array<float, 3> getSideGainsDb() const;

    /** Adjusts DRIVE so the measured output loudness hits the reference. */
    bool matchLoudnessNow();
    void resetOutputMeter() { resetMeterRequested.store (true); }
    void setListenToReference (bool b) { listenRef.store (b); }
    bool isListeningToReference() const noexcept { return listenRef.load(); }
    bool hasReferenceAudio() const;

    // meters
    struct Meters
    {
        std::atomic<float> inPeakDb { -100.0f }, outPeakLDb { -100.0f }, outPeakRDb { -100.0f };
        std::atomic<float> outMomentary { -100.0f }, outShortTerm { -100.0f };
        std::atomic<float> compGr { 0.0f }, limGr { 0.0f }, correlation { 1.0f };
    } meters;
    float getOutputIntegratedLufs() const { return outLoudness.getIntegratedLufs(); }

    /** Output samples for the scope / live spectrum (read on the message thread). */
    int readScopeSamples (float* l, float* r, int maxSamples);

    std::function<void()> onProfilesChanged;   // called on the message thread
    juce::String lastError;

private:
    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();
    void handleAsyncUpdate() override;
    void processSlice (juce::AudioBuffer<float>&);
    void recomputeTargets();
    void learnThreadLoop();

    // DSP
    double fs = 44100.0;
    int blockSize = 512;
    GraphicEq eq;
    GlueCompressor comp;
    MultibandWidth width;
    TruePeakLimiter limiter;
    juce::dsp::DelayLine<float, juce::dsp::DelayLineInterpolationTypes::None> dryDelay { 1 << 15 };
    juce::AudioBuffer<float> dryBuffer;
    juce::SmoothedValue<float> inGain, driveGain, outGain, mixAmt;
    LoudnessMeter outLoudness;
    std::atomic<bool> resetMeterRequested { false };

    std::atomic<float> compBaseThreshold { -18.0f };
    std::array<std::atomic<float>, 3> sideBaseDb {};

    // learn
    std::atomic<bool> learning { false };
    juce::AbstractFifo learnFifo { 1 << 18 };
    std::vector<float> learnL, learnR;
    std::unique_ptr<std::thread> learnThread;
    std::atomic<bool> learnThreadRun { false };
    StreamingAnalyzer learnAnalyzer;
    std::mutex learnMutex;
    std::atomic<int64_t> learnSamples { 0 };

    // background jobs
    juce::ThreadPool pool { 1 };
    std::atomic<bool> busy { false };
    juce::String busyText;
    mutable juce::CriticalSection profileLock;
    SignalProfile reference, learned;
    SignalProfile pendingReference, pendingLearned;
    bool hasPendingReference = false, hasPendingLearned = false;
    juce::String referenceName, referencePath, pendingReferenceName;

    // A/B reference playback
    juce::SpinLock refAudioLock;
    juce::AudioBuffer<float> refAudio, pendingRefAudio;
    double refAudioRate = 0.0;
    int refPlayPos = 0;
    std::atomic<bool> listenRef { false };

    // scope
    juce::AbstractFifo scopeFifo { 1 << 14 };
    std::vector<float> scopeL, scopeR;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MasterProcessor)
};

} // namespace am
