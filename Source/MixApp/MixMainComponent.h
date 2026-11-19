#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "UI/AlphaLookAndFeel.h"
#include "UI/Widgets.h"
#include "Analysis/AudioFileUtils.h"
#include "StemCard.h"
#include "StemSeparator.h"
#include "MixEngine.h"

namespace am
{

//==============================================================================
/** Small worker thread with progress for long jobs (separation, mixing...). */
class BackgroundJob : public juce::Thread
{
public:
    using Work = std::function<void (BackgroundJob&)>;
    BackgroundJob (Work w, std::function<void()> done)
        : juce::Thread ("Alpha Monster job"), work (std::move (w)), onDone (std::move (done)) {}
    ~BackgroundJob() override { stopThread (60000); }

    void run() override
    {
        work (*this);
        auto cb = onDone;
        juce::MessageManager::callAsync ([cb] { if (cb) cb(); });
    }

    bool update (float p, const juce::String& msg)
    {
        progress.store (p);
        { const juce::ScopedLock sl (lock); message = msg; }
        return ! threadShouldExit();
    }
    juce::String getMessage() const { const juce::ScopedLock sl (lock); return message; }
    std::atomic<float> progress { 0.0f };

private:
    Work work;
    std::function<void()> onDone;
    juce::CriticalSection lock;
    juce::String message;
};

//==============================================================================
class MixMainComponent : public juce::Component,
                         public juce::FileDragAndDropTarget,
                         private juce::ListBoxModel,
                         private juce::Timer,
                         private juce::URL::DownloadTaskListener
{
public:
    MixMainComponent();
    ~MixMainComponent() override;

    static constexpr int defaultWidth = 1440, defaultHeight = 900;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray&, int x, int y) override;

    /** Test hook for am_snapshot: runs the whole workflow synchronously. Uses
        sibling files ref_<stem>.wav / my_<stem>.wav when no AI model exists. */
    void runDemoForSnapshot (const juce::File& reference, const juce::File& myMix, std::function<void (int)> pump);

private:
    enum class Mode { tracks, stereoMix };

    // actions
    void chooseReference();
    void loadReference (const juce::File&);
    void analyseReference();
    void startSeparation();
    void addTracks (const juce::Array<juce::File>&);
    void chooseTracks();
    void chooseStereoMix();
    void separateStereoMix (const juce::File&);
    void runMix();
    void exportResults();
    void saveAnalysis();
    void loadAnalysis();
    bool ensureModel (std::function<void()> then);
    void downloadModel (std::function<void()> then);
    void setMode (Mode m);

    // UI refresh
    void refreshReference();
    void refreshTracks();
    void refreshResult();
    void refreshPlayerSources();
    void setBusy (bool b);
    void status (const juce::String& line);
    bool startJob (BackgroundJob::Work, std::function<void()> done);

    // player
    void play (int sourceId);
    void stopPlayback();

    // ListBoxModel
    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics&, int w, int h, bool selected) override;
    juce::Component* refreshComponentForRow (int row, bool selected, juce::Component* existing) override;
    void deleteKeyPressed (int row) override;

    void timerCallback() override;
    void finished (juce::URL::DownloadTask*, bool success) override;
    void progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total) override;

    friend class TrackRow;

    // ---------------------------------------------------------------- state
    LoadedAudio refAudio;                 // 44.1 kHz stereo
    ReferenceMixProfile refProfile;
    std::vector<StemSeparator::Stem> refStems;
    StemSeparator separator;
    std::vector<MixTrack> tracks;
    double projectRate = 44100.0;
    MixResult result;
    MixAmounts amounts;
    Mode mode = Mode::tracks;
    juce::String userMixName;
    std::unique_ptr<BackgroundJob> job;
    std::unique_ptr<juce::FileChooser> chooser;
    std::unique_ptr<juce::URL::DownloadTask> download;
    std::function<void()> afterDownload;
    std::atomic<float> downloadProgress { 0.0f };
    juce::File downloadTarget;
    juce::File lastExportFolder;

    // ---------------------------------------------------------------- header
    juce::TextButton loadRefButton { "CARICA REFERENCE" }, analyseButton { "ANALIZZA" },
                     openAnalysisButton { "APRI ANALISI" }, saveAnalysisButton { "SALVA ANALISI" };
    TerminalScreen refName;
    LedLamp modelLed { "MODELLO IA", theme::ledGreen };

    // ---------------------------------------------------------------- reference
    StemCard mixCard;
    std::array<StemCard, 6> stemCards;

    // ---------------------------------------------------------------- your song
    ModulePanel songPanel { "Il tuo brano" };
    juce::TextButton modeTracks { "TRACCE SEPARATE" }, modeStereo { "MIX STEREO" };
    juce::TextButton addButton { "AGGIUNGI TRACCE" }, removeButton { "RIMUOVI" }, clearButton { "SVUOTA" },
                     loadMixButton { "CARICA MIX STEREO" };
    juce::ListBox trackList { "tracks", this };
    TerminalScreen songHint;

    // ---------------------------------------------------------------- auto-mix
    ModulePanel mixPanel { "Auto-Mix" };
    juce::Slider levelKnob, eqKnob, dynKnob, spaceKnob, reverbKnob, targetKnob;
    juce::TextButton mixButton { "MIXA COME IL REFERENCE" };
    double progressValue = 0.0;
    juce::ProgressBar progressBar { progressValue };

    // ---------------------------------------------------------------- result
    ModulePanel resultPanel { "Risultato" };
    SpectrumScreen spectrum;
    TerminalScreen resultInfo;
    juce::TextButton exportButton { "ESPORTA" }, playButton { "PLAY" }, stopButton { "STOP" };
    juce::ComboBox playSource;

    TerminalScreen statusLine;
    juce::TooltipWindow tooltips { this, 600 };

    // ---------------------------------------------------------------- audio
    juce::AudioDeviceManager deviceManager;
    juce::AudioSourcePlayer sourcePlayer;
    juce::AudioTransportSource transport;
    std::unique_ptr<juce::MemoryAudioSource> memorySource;
    juce::AudioBuffer<float> playBuffer;
    bool audioReady = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixMainComponent)
};

} // namespace am
