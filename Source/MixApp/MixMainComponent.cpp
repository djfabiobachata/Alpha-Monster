#include "MixMainComponent.h"
#include "Analysis/AudioAnalyzer.h"

namespace am
{

//==============================================================================
/** One row of the track list: name, category, result. */
class TrackRow : public juce::Component
{
public:
    explicit TrackRow (MixMainComponent& o) : owner (o)
    {
        for (int i = 0; i < kNumStemTypes; ++i)
            category.addItem (stemName ((StemType) i), i + 1);
        category.onChange = [this]
        {
            if (row >= 0 && row < (int) owner.tracks.size())
            {
                owner.invalidatePlayer();
                owner.tracks[(size_t) row].type = (StemType) (category.getSelectedId() - 1);
                owner.result = {};
                owner.refreshResult();
                owner.rebuildPlayer();
            }
        };
        addAndMakeVisible (category);
    }

    void update (int r, bool sel)
    {
        row = r;
        selected = sel;
        if (row >= 0 && row < (int) owner.tracks.size())
        {
            const auto& t = owner.tracks[(size_t) row];
            category.setSelectedId ((int) t.type + 1, juce::dontSendNotification);
            name = t.name;
            juce::String res;
            if (t.after.valid)
            {
                const auto& s = t.settings;
                res = (s.gainDb >= 0 ? "+" : "") + juce::String (s.gainDb, 1) + "dB  " + t.after.panText()
                      + (s.reverbSendDb > -99.0f ? "  REV" : "") + (s.compressed ? "  COMP" : "");
            }
            resultText = res;
            colour = theme::stemColour ((int) t.type);
        }
        category.setEnabled (owner.job == nullptr);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat().reduced (1.0f);
        g.setColour (selected ? theme::panel.withAlpha (0.7f) : juce::Colours::white.withAlpha (row % 2 == 0 ? 0.03f : 0.0f));
        g.fillRect (r);
        g.setColour (colour);
        g.fillEllipse (r.getX() + 6.0f, r.getCentreY() - 4.0f, 8.0f, 8.0f);
        g.setFont (fonts::mono (14.0f));
        g.setColour (theme::cream);
        g.drawText (name, r.withTrimmedLeft (20.0f).withWidth (210.0f), juce::Justification::centredLeft, true);
        g.setColour (theme::crtTrace);
        g.drawText (resultText, r.withTrimmedLeft (380.0f), juce::Justification::centredLeft, true);
    }

    void resized() override { category.setBounds (234, 3, 140, getHeight() - 6); }
    void mouseDown (const juce::MouseEvent& e) override { owner.trackList.selectRowsBasedOnModifierKeys (row, e.mods, false); }

private:
    MixMainComponent& owner;
    juce::ComboBox category;
    int row = -1;
    bool selected = false;
    juce::String name, resultText;
    juce::Colour colour;
};

//==============================================================================
MixMainComponent::MixMainComponent()
{
    setSize (defaultWidth, defaultHeight);

    // header
    for (auto* b : { &loadRefButton, &analyseButton, &openAnalysisButton, &saveAnalysisButton })
        addAndMakeVisible (b);
    loadRefButton.onClick = [this] { chooseReference(); };
    analyseButton.onClick = [this] { analyseReference(); };
    openAnalysisButton.onClick = [this] { loadAnalysis(); };
    saveAnalysisButton.onClick = [this] { saveAnalysis(); };
    loadRefButton.setTooltip ("Load the reference song (or drag it into the window)");
    analyseButton.setTooltip ("Separate the reference instruments with AI and measure how they are mixed");
    openAnalysisButton.setTooltip ("Reopen a saved analysis (.amref) without separating again");
    saveAnalysisButton.setTooltip ("Save the reference analysis to reuse it later");
    analyseButton.getProperties().set ("style", "big");
    addAndMakeVisible (refName);
    refName.fontHeight = 15.0f;
    addAndMakeVisible (modelLed);

    // reference cards
    addAndMakeVisible (mixCard);
    mixCard.onListen = [this] { openPlayer(); player.showOnlySection (PlayerLane::reference); engine.play(); };
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible (stemCards[(size_t) i]);
        stemCards[(size_t) i].setStem ((StemType) i, nullptr);
        stemCards[(size_t) i].onListen = [this, i]
        {
            openPlayer();
            player.auditionLane (juce::String ((int) PlayerLane::reference) + "/" + stemName ((StemType) i));
        };
    }
    mixCard.setMixSummary ({}, {});

    // your song
    addAndMakeVisible (songPanel);
    for (auto* b : { &modeTracks, &modeStereo, &addButton, &removeButton, &clearButton, &loadMixButton })
        addAndMakeVisible (b);
    modeTracks.setClickingTogglesState (false);
    modeStereo.setClickingTogglesState (false);
    modeTracks.onClick = [this] { setMode (Mode::tracks); };
    modeStereo.onClick = [this] { setMode (Mode::stereoMix); };
    modeTracks.setTooltip ("You have the separate tracks of your song (vocals, drums, bass...): the most accurate mix");
    modeStereo.setTooltip ("You only have the stereo mix: Alpha Monster separates it with AI and remixes it");
    addButton.onClick = [this] { chooseTracks(); };
    removeButton.onClick = [this]
    {
        invalidatePlayer();
        auto rows = trackList.getSelectedRows();
        for (int i = rows.size(); --i >= 0;)
            tracks.erase (tracks.begin() + rows[i]);
        result = {};
        updateOriginalMix();
        refreshTracks();
        refreshResult();
        rebuildPlayer();
    };
    clearButton.onClick = [this]
    {
        invalidatePlayer();
        tracks.clear();
        result = {};
        updateOriginalMix();
        refreshTracks();
        refreshResult();
        rebuildPlayer();
    };
    loadMixButton.onClick = [this] { chooseStereoMix(); };
    trackList.setRowHeight (28);
    trackList.setMultipleSelectionEnabled (true);
    trackList.setColour (juce::ListBox::backgroundColourId, theme::displayBg);
    addAndMakeVisible (trackList);
    addAndMakeVisible (songHint);
    songHint.fontHeight = 13.0f;

    // auto-mix
    addAndMakeVisible (mixPanel);
    auto knob = [this] (juce::Slider& s, double lo, double hi, double def, const juce::String& suffix, const juce::String& tip)
    {
        s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
        s.setRange (lo, hi, 1.0);
        s.setValue (def, juce::dontSendNotification);
        s.setDoubleClickReturnValue (true, def);
        s.setTextValueSuffix (suffix);
        s.setTooltip (tip);
        addAndMakeVisible (s);
    };
    knob (levelKnob, 0, 100, 100, "%", "How closely to follow the reference level balance");
    knob (eqKnob, 0, 100, 70, "%", "How closely to match the tone (EQ) of each instrument");
    knob (dynKnob, 0, 100, 60, "%", "How closely to match the dynamics (compression)");
    knob (spaceKnob, 0, 100, 100, "%", "How closely to follow pan and stereo width");
    knob (reverbKnob, 0, 100, 80, "%", "How closely to follow depth and reverb");
    knob (targetKnob, -24, -8, -16, " LUFS", "Loudness of the mix (pre-master). Leave room for mastering: -16..-14 LUFS");
    addAndMakeVisible (mixButton);
    mixButton.getProperties().set ("style", "big");
    mixButton.onClick = [this] { runMix(); };
    addAndMakeVisible (progressBar);
    progressBar.setPercentageDisplay (false);

    // result
    addAndMakeVisible (resultPanel);
    addAndMakeVisible (spectrum);
    spectrum.caption = "REFERENCE (cream)  -  BEFORE (red)  -  AFTER (cyan)";
    spectrum.setRange (-42.0f, 12.0f);
    addAndMakeVisible (resultInfo);
    resultInfo.fontHeight = 13.0f;
    for (auto* b : { &exportButton, &playerButton, &playButton, &rewindButton })
        addAndMakeVisible (b);
    exportButton.getProperties().set ("style", "big");
    playerButton.getProperties().set ("style", "big");
    exportButton.onClick = [this] { exportResults(); };
    exportButton.setTooltip ("Save the mix (24-bit WAV), the processed tracks and the report with every parameter");
    playerButton.onClick = [this] { openPlayer(); };
    playerButton.setTooltip ("Open the player: waveforms of the reference, your song and the result, with mute / solo");
    playButton.onClick = [this] { engine.togglePlay(); };
    playButton.setTooltip ("Play / pause (what is enabled in the player)");
    rewindButton.onClick = [this] { engine.setPositionSec (0.0); };

    addAndMakeVisible (statusLine);
    statusLine.fontHeight = 14.0f;

    // audio
    sourcePlayer.setSource (&engine);
    const auto err = deviceManager.initialiseWithDefaultDevices (0, 2);
    audioReady = err.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr;
    if (audioReady) deviceManager.addAudioCallback (&sourcePlayer);

    const auto model = StemSeparator::findModelFile();
    modelLed.setOn (model.existsAsFile());
    modelLed.setLabel (model.existsAsFile() ? "AI MODEL READY" : "AI MODEL NOT INSTALLED");

    status ("> ALPHA MONSTER MIX v" AM_VERSION_STRING);
    status ("> 1) LOAD REFERENCE  2) ANALYSE  3) add your tracks  4) MIX  5) PLAYER to listen and compare");
    if (! audioReady) status ("!No audio output available: playback is disabled.");

    setMode (Mode::tracks);
    refreshReference();
    refreshTracks();
    refreshResult();
    startTimerHz (15);
}

MixMainComponent::~MixMainComponent()
{
    stopTimer();
    job.reset();
    engine.clearLanes();
    sourcePlayer.setSource (nullptr);
    deviceManager.removeAudioCallback (&sourcePlayer);
    download.reset();
    playerWindow.reset();
}

//==============================================================================
void MixMainComponent::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    AlphaLookAndFeel::drawBodyBackground (g, r);
    auto header = r.removeFromTop (78.0f);
    g.setColour (theme::headerBg);
    g.fillRoundedRectangle (header.reduced (6.0f, 6.0f), 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (header.reduced (6.0f, 6.0f), 8.0f, 1.5f);
    AlphaLookAndFeel::drawLogo (g, juce::Rectangle<float> (18.0f, 14.0f, 380.0f, 50.0f), "Mix  -  reference mixing");

    for (auto pt : { juce::Point<float> (8, 8), juce::Point<float> ((float) getWidth() - 8, 8),
                     juce::Point<float> (8, (float) getHeight() - 8), juce::Point<float> ((float) getWidth() - 8, (float) getHeight() - 8) })
        AlphaLookAndFeel::drawScrew (g, pt, 4.5f);
}

void MixMainComponent::paintOverChildren (juce::Graphics& g)
{
    auto legend = [&g] (juce::Component& c, const juce::String& t)
    {
        auto b = c.getBounds().toFloat();
        AlphaLookAndFeel::drawLegend (g, t, b.withY (b.getY() - 15.0f).withHeight (14.0f), juce::Justification::centred, 13.0f);
    };
    legend (levelKnob, "LEVELS");
    legend (eqKnob, "EQ");
    legend (dynKnob, "DYNAMICS");
    legend (spaceKnob, "SPACE");
    legend (reverbKnob, "REVERB");
    legend (targetKnob, "TARGET");

    if (job != nullptr)
    {
        // dim the work area while a job runs
        g.setColour (juce::Colours::black.withAlpha (0.08f));
        g.fillRect (getLocalBounds().withTrimmedTop (78).withTrimmedBottom (70));
    }
}

void MixMainComponent::resized()
{
    // header
    loadRefButton.setBounds (420, 14, 150, 28);
    analyseButton.setBounds (576, 12, 150, 32);
    openAnalysisButton.setBounds (732, 14, 120, 28);
    saveAnalysisButton.setBounds (858, 14, 120, 28);
    refName.setBounds (420, 46, 760, 24);
    modelLed.setBounds (1196, 46, 230, 22);

    // reference row
    const int y1 = 86, h1 = 352, gap = 8;
    const int cardW = (getWidth() - 24 - 6 * gap) / 7;
    mixCard.setBounds (12, y1, cardW, h1);
    for (int i = 0; i < 6; ++i)
        stemCards[(size_t) i].setBounds (12 + (i + 1) * (cardW + gap), y1, cardW, h1);

    // bottom row
    const int y2 = y1 + h1 + 10, h2 = getHeight() - y2 - 76;
    songPanel.setBounds (12, y2, 600, h2);
    {
        auto c = songPanel.getContentBounds() + songPanel.getPosition();
        auto row = c.removeFromTop (30);
        modeTracks.setBounds (row.removeFromLeft (150).reduced (2));
        modeStereo.setBounds (row.removeFromLeft (130).reduced (2));
        row.removeFromLeft (16);
        if (mode == Mode::tracks)
        {
            addButton.setBounds (row.removeFromLeft (140).reduced (2));
            removeButton.setBounds (row.removeFromLeft (80).reduced (2));
            clearButton.setBounds (row.removeFromLeft (70).reduced (2));
            loadMixButton.setBounds ({});
        }
        else
        {
            loadMixButton.setBounds (row.removeFromLeft (170).reduced (2));
            addButton.setBounds ({});
            removeButton.setBounds ({});
            clearButton.setBounds ({});
        }
        c.removeFromTop (6);
        songHint.setBounds (c.removeFromBottom (54));
        c.removeFromBottom (4);
        trackList.setBounds (c);
    }

    mixPanel.setBounds (622, y2, 292, h2);
    {
        auto c = mixPanel.getContentBounds() + mixPanel.getPosition();
        c.removeFromTop (16);
        auto r1 = c.removeFromTop (92);
        const int kw = r1.getWidth() / 3;
        levelKnob.setBounds (r1.removeFromLeft (kw).reduced (4, 0));
        eqKnob.setBounds (r1.removeFromLeft (kw).reduced (4, 0));
        dynKnob.setBounds (r1.reduced (4, 0));
        c.removeFromTop (20);
        auto r2 = c.removeFromTop (92);
        spaceKnob.setBounds (r2.removeFromLeft (kw).reduced (4, 0));
        reverbKnob.setBounds (r2.removeFromLeft (kw).reduced (4, 0));
        targetKnob.setBounds (r2.reduced (4, 0));
        c.removeFromTop (10);
        mixButton.setBounds (c.removeFromTop (48).reduced (6, 2));
        c.removeFromTop (6);
        progressBar.setBounds (c.removeFromTop (22).reduced (6, 0));
    }

    resultPanel.setBounds (924, y2, getWidth() - 936, h2);
    {
        auto c = resultPanel.getContentBounds() + resultPanel.getPosition();
        auto bottom = c.removeFromBottom (36);
        exportButton.setBounds (bottom.removeFromLeft (120).reduced (2));
        bottom.removeFromLeft (8);
        playerButton.setBounds (bottom.removeFromLeft (120).reduced (2));
        bottom.removeFromLeft (8);
        rewindButton.setBounds (bottom.removeFromLeft (44).reduced (2));
        playButton.setBounds (bottom.removeFromLeft (90).reduced (2));
        c.removeFromBottom (6);
        spectrum.setBounds (c.removeFromLeft (c.getWidth() * 55 / 100));
        c.removeFromLeft (6);
        resultInfo.setBounds (c);
    }

    statusLine.setBounds (12, getHeight() - 68, getWidth() - 24, 58);
}

//==============================================================================
void MixMainComponent::status (const juce::String& line)
{
    statusLine.addLine (line, 100);
}

bool MixMainComponent::startJob (BackgroundJob::Work work, std::function<void()> done)
{
    if (job != nullptr)
    {
        status ("!Please wait: another operation is running.");
        return false;
    }
    invalidatePlayer();   // buffers may change while the job runs
    juce::Component::SafePointer<MixMainComponent> safe (this);
    job = std::make_unique<BackgroundJob> (std::move (work), [safe, done]
    {
        if (safe == nullptr) return;
        safe->job.reset();
        safe->setBusy (false);
        if (done) done();
        safe->rebuildPlayer();
    });
    setBusy (true);
    job->startThread (juce::Thread::Priority::normal);
    return true;
}

void MixMainComponent::setBusy (bool b)
{
    for (auto* c : std::initializer_list<juce::Component*> { &loadRefButton, &analyseButton, &openAnalysisButton, &saveAnalysisButton,
                                                             &modeTracks, &modeStereo, &addButton, &removeButton, &clearButton,
                                                             &loadMixButton, &mixButton, &exportButton })
        c->setEnabled (! b);
    if (! b) progressValue = 0.0;
    trackList.updateContent();
    repaint();
}

void MixMainComponent::timerCallback()
{
    if (job != nullptr)
    {
        progressValue = job->progress.load();
        progressBar.setTextToDisplay (job->getMessage());
    }
    else if (download != nullptr)
    {
        progressValue = downloadProgress.load();
        progressBar.setTextToDisplay ("Downloading AI model: " + juce::String ((int) (progressValue * 100.0)) + "%");
    }
    else
    {
        progressBar.setTextToDisplay ({});
    }
    playButton.setToggleState (engine.isPlaying(), juce::dontSendNotification);
    playButton.setButtonText (engine.isPlaying() ? "PAUSE" : "PLAY");
}

//==============================================================================
void MixMainComponent::setMode (Mode m)
{
    mode = m;
    modeTracks.setToggleState (m == Mode::tracks, juce::dontSendNotification);
    modeStereo.setToggleState (m == Mode::stereoMix, juce::dontSendNotification);
    songHint.setLines (m == Mode::tracks
        ? juce::StringArray { "Add the tracks / stems of your song (or drag them here).",
                              "The category is guessed from the file name: correct it if needed." }
        : juce::StringArray { "Load your stereo mix: it will be separated with AI into",
                              "vocals, drums, bass, guitars, piano and other, then remixed." });
    resized();
    repaint();
}

void MixMainComponent::filesDropped (const juce::StringArray& files, int x, int y)
{
    juce::Array<juce::File> list;
    for (auto& f : files) list.add (juce::File (f));
    if (list.isEmpty()) return;

    const juce::Point<int> p (x, y);
    if (songPanel.getBounds().contains (p))
    {
        if (mode == Mode::tracks) addTracks (list);
        else separateStereoMix (list[0]);
    }
    else
    {
        loadReference (list[0]);
    }
}

//==============================================================================
void MixMainComponent::chooseReference()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the reference song",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), supportedFileWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile()) loadReference (fc.getResult());
    });
}

void MixMainComponent::loadReference (const juce::File& file)
{
    auto loaded = std::make_shared<LoadedAudio>();
    auto prof = std::make_shared<SignalProfile>();
    status ("> Loading the reference: " + file.getFileName());
    startJob ([file, loaded, prof] (BackgroundJob& j)
    {
        j.update (-1.0f, "Loading reference...");
        *loaded = loadAudioFile (file, StemSeparator::sampleRate);
        if (loaded->ok())
        {
            j.update (0.5f, "Analysing the mix...");
            *prof = AudioAnalyzer::analyse (loaded->buffer, StemSeparator::sampleRate,
                                            [&j] (float p) { return j.update (0.5f + 0.5f * p, "Analysing the mix..."); });
        }
    },
    [this, loaded, prof, file]
    {
        if (! loaded->ok()) { status ("!" + loaded->error); return; }
        refAudio = std::move (*loaded);
        refProfile = {};
        refProfile.sourceName = file.getFileNameWithoutExtension();
        refProfile.mix = *prof;
        refStems.clear();
        result = {};
        refreshReference();
        refreshResult();
        status ("> Reference loaded. Press ANALYSE to separate the instruments.");
    });
}

bool MixMainComponent::ensureModel (std::function<void()> then)
{
    if (separator.isLoaded()) return true;

    auto model = StemSeparator::findModelFile();
    if (! model.existsAsFile())
    {
        juce::Component::SafePointer<MixMainComponent> safe (this);
        juce::AlertWindow::showAsync (juce::MessageBoxOptions()
                                          .withIconType (juce::MessageBoxIconType::QuestionIcon)
                                          .withTitle ("AI model missing")
                                          .withMessage ("Separating the instruments needs the Demucs v4 model (about 55 MB).\n"
                                                        "Download it now?")
                                          .withButton ("Download")
                                          .withButton ("Cancel"),
                                      [safe, then] (int r)
        {
            if (safe != nullptr && r == 1) safe->downloadModel (then);
        });
        return false;
    }

    status ("> Loading the AI model: " + model.getFileName());
    juce::String err;
    if (! separator.loadModel (model, err))
    {
        status ("!" + err);
        return false;
    }
    modelLed.setOn (true);
    modelLed.setLabel ("AI READY (" + juce::String (separator.numSources()) + " INSTRUMENTS)");
    juce::ignoreUnused (then);
    return true;
}

void MixMainComponent::downloadModel (std::function<void()> then)
{
    const auto folder = StemSeparator::userModelFolder();
    folder.createDirectory();
    downloadTarget = folder.getChildFile (StemSeparator::defaultModelFileName());
    afterDownload = std::move (then);
    downloadProgress.store (0.0f);
    status ("> Downloading the AI model...");
    setBusy (true);
    download = juce::URL (StemSeparator::modelDownloadUrl (StemSeparator::defaultModelFileName()))
                   .downloadToFile (downloadTarget.withFileExtension ("part"),
                                    juce::URL::DownloadTaskOptions().withListener (this));
    if (download == nullptr)
    {
        setBusy (false);
        status ("!Download failed: check your connection.");
    }
}

void MixMainComponent::progress (juce::URL::DownloadTask*, juce::int64 downloaded, juce::int64 total)
{
    if (total > 0) downloadProgress.store ((float) downloaded / (float) total);
}

void MixMainComponent::finished (juce::URL::DownloadTask* task, bool success)
{
    const bool ok = success && task != nullptr && ! task->hadError() && task->getTargetLocation().getSize() > 1000000;
    juce::MessageManager::callAsync ([this, ok]
    {
        download.reset();
        setBusy (false);
        if (ok && downloadTarget.withFileExtension ("part").moveFileTo (downloadTarget))
        {
            status ("> AI model downloaded.");
            modelLed.setOn (true);
            if (afterDownload) { auto f = afterDownload; afterDownload = nullptr; f(); }
        }
        else
        {
            downloadTarget.withFileExtension ("part").deleteFile();
            status ("!Model download failed. Try again or copy the file to: " + StemSeparator::userModelFolder().getFullPathName());
        }
    });
}

void MixMainComponent::analyseReference()
{
    if (! refAudio.ok()) { status ("!Load a reference first."); return; }
    if (! ensureModel ([this] { analyseReference(); })) return;
    startSeparation();
}

void MixMainComponent::startSeparation()
{
    auto stems = std::make_shared<std::vector<StemSeparator::Stem>>();
    auto profile = std::make_shared<ReferenceMixProfile> (refProfile);
    const int threads = StemSeparator::recommendedThreads();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    status ("> Separating the reference with AI (" + juce::String (threads) + " threads). This can take a few minutes...");

    startJob ([this, stems, profile, threads] (BackgroundJob& j)
    {
        *stems = separator.separate (refAudio.buffer, threads, [&j] (float p, const juce::String& m) { return j.update (0.85f * p, m); });
        if (stems->empty()) return;

        profile->stems.clear();
        for (size_t i = 0; i < stems->size(); ++i)
        {
            auto& s = (*stems)[i];
            if (! j.update (0.85f + 0.15f * (float) i / (float) stems->size(), "Analysing: " + stemName (s.type))) return;
            StemProfile sp;
            sp.type = s.type;
            sp.profile = AudioAnalyzer::analyse (s.audio, StemSeparator::sampleRate);
            finaliseDepth (sp.profile, s.type);
            sp.levelRelMixDb = sp.profile.integratedLufs - profile->mix.integratedLufs;
            profile->stems.push_back (sp);
        }
    },
    [this, stems, profile, t0]
    {
        if (stems->empty()) { status ("!Separation cancelled or failed."); return; }
        refStems = std::move (*stems);
        refProfile = *profile;
        result = {};
        refreshReference();
        refreshResult();
        status ("> Analysis completed in " + juce::String (juce::roundToInt ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0)) + " s. Now add your song.");
    });
}

//==============================================================================
void MixMainComponent::chooseTracks()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose the tracks of your song",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), supportedFileWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles
                              | juce::FileBrowserComponent::canSelectMultipleItems,
                          [this] (const juce::FileChooser& fc)
    {
        if (! fc.getResults().isEmpty()) addTracks (fc.getResults());
    });
}

void MixMainComponent::addTracks (const juce::Array<juce::File>& files)
{
    if (mode != Mode::tracks)
    {
        setMode (Mode::tracks);
        tracks.clear();
    }

    auto loaded = std::make_shared<std::vector<MixTrack>>();
    auto errors = std::make_shared<juce::StringArray>();
    double rate = tracks.empty() ? 0.0 : projectRate;
    auto rateOut = std::make_shared<double> (rate);

    startJob ([files, loaded, errors, rateOut] (BackgroundJob& j)
    {
        for (int i = 0; i < files.size(); ++i)
        {
            if (! j.update ((float) i / (float) files.size(), "Loading: " + files[i].getFileName())) return;
            auto a = loadAudioFile (files[i], *rateOut);
            if (! a.ok()) { errors->add (a.error); continue; }
            if (*rateOut <= 0.0) *rateOut = a.sampleRate;
            MixTrack t;
            t.name = files[i].getFileNameWithoutExtension();
            t.file = files[i];
            t.type = guessStemFromFileName (t.name);
            t.audio = std::move (a.buffer);
            loaded->push_back (std::move (t));
        }
    },
    [this, loaded, errors, rateOut]
    {
        for (auto& e : *errors) status ("!" + e);
        if (*rateOut > 0.0) projectRate = *rateOut;
        for (auto& t : *loaded) tracks.push_back (std::move (t));
        result = {};
        updateOriginalMix();
        refreshTracks();
        refreshResult();
        status ("> " + juce::String ((int) loaded->size()) + " tracks added (" + juce::String (projectRate / 1000.0, 1) + " kHz).");
    });
}

void MixMainComponent::chooseStereoMix()
{
    chooser = std::make_unique<juce::FileChooser> ("Choose your stereo mix",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), supportedFileWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        if (fc.getResult().existsAsFile()) separateStereoMix (fc.getResult());
    });
}

void MixMainComponent::separateStereoMix (const juce::File& file)
{
    setMode (Mode::stereoMix);
    if (! ensureModel ([this, file] { separateStereoMix (file); })) return;

    auto out = std::make_shared<std::vector<MixTrack>>();
    auto err = std::make_shared<juce::String>();
    auto original = std::make_shared<juce::AudioBuffer<float>>();
    const int threads = StemSeparator::recommendedThreads();
    status ("> Separating your mix with AI: " + file.getFileName());

    startJob ([this, file, out, err, original, threads] (BackgroundJob& j)
    {
        j.update (-1.0f, "Loading your mix...");
        auto a = loadAudioFile (file, StemSeparator::sampleRate);
        if (! a.ok()) { *err = a.error; return; }
        *original = a.buffer;
        auto stems = separator.separate (a.buffer, threads, [&j] (float p, const juce::String& m) { return j.update (0.95f * p, m); });
        if (stems.empty()) { *err = "Separation cancelled."; return; }
        for (auto& s : stems)
        {
            const float lufs = AudioAnalyzer::measureLufs (s.audio, StemSeparator::sampleRate);
            if (lufs < -50.0f) continue;   // instrument not present in the song
            MixTrack t;
            t.name = stemName (s.type) + " (separated)";
            t.type = s.type;
            t.fromSeparation = true;
            t.audio = std::move (s.audio);
            out->push_back (std::move (t));
        }
    },
    [this, out, err, file, original]
    {
        if (err->isNotEmpty()) { status ("!" + *err); return; }
        tracks = std::move (*out);
        projectRate = StemSeparator::sampleRate;
        originalMix = std::move (*original);
        originalRate = projectRate;
        originalLufs = AudioAnalyzer::measureLufs (originalMix, originalRate);
        userMixName = file.getFileNameWithoutExtension();
        result = {};
        refreshTracks();
        refreshResult();
        status ("> Mix separated into " + juce::String ((int) tracks.size()) + " instruments. Now press MIX.");
    });
}

//==============================================================================
void MixMainComponent::runMix()
{
    if (refProfile.stems.empty()) { status ("!Analyse the reference first (ANALYSE) or open a saved analysis."); return; }
    if (tracks.empty()) { status ("!Add the tracks of your song or load your stereo mix."); return; }

    amounts.level = (float) levelKnob.getValue() / 100.0f;
    amounts.eq = (float) eqKnob.getValue() / 100.0f;
    amounts.dynamics = (float) dynKnob.getValue() / 100.0f;
    amounts.space = (float) spaceKnob.getValue() / 100.0f;
    amounts.reverb = (float) reverbKnob.getValue() / 100.0f;
    amounts.targetLufs = (float) targetKnob.getValue();

    auto res = std::make_shared<MixResult>();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    status ("> Automatic mix running...");
    startJob ([this, res] (BackgroundJob& j)
    {
        *res = AutoMixer::run (tracks, refProfile, projectRate, amounts,
                               [&j] (float p, const juce::String& m) { return j.update (p, m); });
    },
    [this, res, t0]
    {
        if (! res->ok) { status ("!" + (res->error.isNotEmpty() ? res->error : juce::String ("Mix cancelled."))); return; }
        result = std::move (*res);
        refreshTracks();
        refreshResult();
        status ("> Mix completed in " + juce::String (juce::roundToInt ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0))
                + " s. Open the PLAYER to compare, then press EXPORT.");
        for (auto& w : result.warnings) status ("!" + w);
    });
}

void MixMainComponent::exportResults()
{
    if (! result.ok) { status ("!Nothing to export: run the mix first."); return; }
    chooser = std::make_unique<juce::FileChooser> ("Choose the export folder",
                                                   lastExportFolder.isDirectory() ? lastExportFolder : juce::File::getSpecialLocation (juce::File::userMusicDirectory));
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                          [this] (const juce::FileChooser& fc)
    {
        auto dir = fc.getResult();
        if (! dir.isDirectory()) return;
        lastExportFolder = dir;
        const auto base = juce::File::createLegalFileName ((userMixName.isNotEmpty() ? userMixName : juce::String ("Mix")) + " - Alpha Monster");
        auto folder = dir.getChildFile (base);
        folder = folder.getNonexistentSibling();
        folder.createDirectory();

        bool ok = writeWav (folder.getChildFile ("Mix Alpha Monster.wav"), result.mix, result.sampleRate, 24);
        auto stemsDir = folder.getChildFile ("Processed tracks");
        stemsDir.createDirectory();
        for (size_t i = 0; i < tracks.size(); ++i)
            ok &= writeWav (stemsDir.getChildFile (juce::String ((int) i + 1).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (tracks[i].name) + ".wav"),
                            tracks[i].processed, result.sampleRate, 24);
        ok &= folder.getChildFile ("Report mix.txt").replaceWithText (AutoMixer::buildReport (tracks, refProfile, result, amounts));
        ok &= folder.getChildFile ("Mix settings.json").replaceWithText (juce::JSON::toString (AutoMixer::buildJson (tracks, refProfile, result)));
        status (ok ? "> Exported to: " + folder.getFullPathName() : "!Error while exporting to " + folder.getFullPathName());
        if (ok) folder.revealToUser();
    });
}

void MixMainComponent::saveAnalysis()
{
    if (refProfile.stems.empty()) { status ("!No analysis to save."); return; }
    chooser = std::make_unique<juce::FileChooser> ("Save the reference analysis",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory)
                                                       .getChildFile (juce::File::createLegalFileName (refProfile.sourceName) + ".amref"),
                                                   "*.amref");
    chooser->launchAsync (juce::FileBrowserComponent::saveMode | juce::FileBrowserComponent::warnAboutOverwriting,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (f == juce::File()) return;
        f = f.withFileExtension ("amref");
        const bool ok = f.replaceWithText (juce::JSON::toString (refProfile.toVar()));
        status (ok ? "> Analysis saved: " + f.getFileName() : "!Could not save " + f.getFullPathName());
    });
}

void MixMainComponent::loadAnalysis()
{
    chooser = std::make_unique<juce::FileChooser> ("Open an analysis (.amref)",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.amref;*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        auto p = ReferenceMixProfile::fromVar (juce::JSON::parse (f));
        if (p.stems.empty() || ! p.mix.valid) { status ("!Invalid analysis file."); return; }
        invalidatePlayer();
        refProfile = p;
        refAudio = {};
        refStems.clear();
        result = {};
        refreshReference();
        refreshResult();
        rebuildPlayer();
        status ("> Analysis loaded: " + refProfile.sourceName + " (the reference audio is not available for listening).");
    });
}

//==============================================================================
void MixMainComponent::refreshReference()
{
    if (refProfile.mix.valid)
        refName.setLines ({ "REF: " + refProfile.sourceName + "   " + juce::String (refProfile.mix.durationSec / 60.0f, 1) + " min   "
                            + juce::String (refProfile.mix.integratedLufs, 1) + " LUFS"
                            + (refProfile.stems.empty() ? "   - press ANALYSE" : "   - " + juce::String ((int) refProfile.stems.size()) + " instruments analysed") });
    else
        refName.setLines ({ "NO REFERENCE - press LOAD REFERENCE or drag a file into the window" });

    mixCard.setMixSummary (refProfile.mix, refProfile.sourceName);
    for (int i = 0; i < 6; ++i)
    {
        const auto* s = refProfile.find ((StemType) i);
        stemCards[(size_t) i].setStem ((StemType) i, s);
    }
}

void MixMainComponent::refreshTracks()
{
    trackList.updateContent();
    trackList.repaint();
}

void MixMainComponent::refreshResult()
{
    std::vector<SpectrumScreen::Curve> curves;
    const auto& centres = bandCentres();
    std::vector<float> freqs (centres.begin(), centres.end());
    auto norm = [] (const SignalProfile& p)
    {
        auto b = p.bandDb;
        float m = 0.0f;
        for (int i = 10; i <= 24; ++i) m += b[(size_t) i];
        m /= 15.0f;
        std::vector<float> out;
        for (auto v : b) out.push_back (v - m);
        return out;
    };
    if (refProfile.mix.valid) curves.push_back ({ freqs, norm (refProfile.mix), theme::cream, 2.0f, true, "REFERENCE" });
    if (result.ok)
    {
        curves.push_back ({ freqs, norm (result.mixBefore), theme::ledRed, 1.5f, false, "BEFORE" });
        curves.push_back ({ freqs, norm (result.mixAfter), theme::crtTrace, 1.8f, false, "AFTER" });
    }
    spectrum.setCurves (std::move (curves));

    juce::StringArray lines;
    if (result.ok)
    {
        lines.add ("MIX: " + juce::String (result.mixAfter.integratedLufs, 1) + " LUFS  TP " + juce::String (result.mixAfter.truePeakDb, 1));
        lines.add ("Width " + juce::String (juce::roundToInt (result.mixAfter.width * 100)) + "% (ref "
                   + juce::String (juce::roundToInt (refProfile.mix.width * 100)) + "%)");
        lines.add ("Tilt: " + juce::String (result.mixAfter.tiltDbPerOct, 1) + " (ref " + juce::String (refProfile.mix.tiltDbPerOct, 1) + ") dB/oct");
        lines.add ("");
        for (auto& t : tracks)
        {
            const auto& s = t.settings;
            lines.add (t.name.substring (0, 16).paddedRight (' ', 16) + " " + (s.gainDb >= 0 ? "+" : "") + juce::String (s.gainDb, 1) + "dB "
                       + t.after.panText());
        }
    }
    else
    {
        lines.add ("Here you will see the reference");
        lines.add ("compared with your mix,");
        lines.add ("before and after Alpha Monster.");
        lines.add ("");
        lines.add ("Every parameter (level, pan,");
        lines.add ("EQ, compressor, reverb)");
        lines.add ("goes into the exported report.");
    }
    resultInfo.setLines (lines);

    // your results on the reference cards
    for (int i = 0; i < 6; ++i)
    {
        const SignalProfile* yours = nullptr;
        SignalProfile tmp;
        if (result.ok)
        {
            juce::AudioBuffer<float> sum;
            int count = 0;
            for (auto& t : tracks)
                if (t.settings.hasTarget && (int) t.settings.targetStem == i && t.after.valid)
                {
                    tmp = t.after;  // single track: exact; several: show the first (approximation)
                    ++count;
                }
            if (count > 0) yours = &tmp;
        }
        stemCards[(size_t) i].setYourResult (yours, 0.0f);
    }
}

void MixMainComponent::updateOriginalMix()
{
    if (mode == Mode::stereoMix) return;   // the loaded stereo file is the original
    int len = 0;
    for (auto& t : tracks) len = juce::jmax (len, t.audio.getNumSamples());
    originalMix.setSize (2, len);
    originalMix.clear();
    for (auto& t : tracks)
        for (int ch = 0; ch < 2; ++ch) originalMix.addFrom (ch, 0, t.audio, ch, 0, t.audio.getNumSamples());
    originalRate = projectRate;
    originalLufs = len > 0 ? AudioAnalyzer::measureLufs (originalMix, originalRate) : -70.0f;
}

void MixMainComponent::rebuildPlayer()
{
    engine.clearLanes();
    std::vector<LanePtr> lanes;
    auto add = [&lanes] (const juce::String& name, PlayerLane::Section sec, bool isMix, juce::Colour c,
                         const juce::AudioBuffer<float>& audio, double rate)
    {
        if (audio.getNumSamples() == 0) return;
        auto l = std::make_shared<PlayerLane>();
        l->name = name;
        l->section = sec;
        l->isMix = isMix;
        l->colour = c;
        l->audio = &audio;
        l->sampleRate = rate;
        l->mute.store (! isMix);   // default: only the mixes play
        l->computePeaks();
        lanes.push_back (l);
    };

    if (refAudio.ok())
    {
        add ("Reference mix", PlayerLane::reference, true, theme::cream, refAudio.buffer, StemSeparator::sampleRate);
        for (auto& st : refStems)
            add (stemName (st.type), PlayerLane::reference, false, theme::stemColour ((int) st.type), st.audio, StemSeparator::sampleRate);
    }
    if (! tracks.empty())
    {
        add (mode == Mode::stereoMix ? "Your mix (stereo file)" : "Your mix (sum of tracks)", PlayerLane::original, true,
             theme::ledRed, originalMix, originalRate);
        for (auto& t : tracks)
            add (t.name, PlayerLane::original, false, theme::stemColour ((int) t.type), t.audio, projectRate);
    }
    if (result.ok)
    {
        add ("Alpha Monster mix", PlayerLane::result, true, theme::crtTrace, result.mix, result.sampleRate);
        for (auto& t : tracks)
            add (t.name, PlayerLane::result, false, theme::stemColour ((int) t.type), t.processed, result.sampleRate);
    }

    // level match: every section plays at the loudness of the quietest mix
    const float lufs[3] = { refAudio.ok() ? refProfile.mix.integratedLufs : 100.0f,
                            ! tracks.empty() ? originalLufs : 100.0f,
                            result.ok ? result.mixAfter.integratedLufs : 100.0f };
    float target = 100.0f;
    for (auto v : lufs) if (v > -69.0f) target = juce::jmin (target, v);
    for (auto& l : lanes)
    {
        const float v = lufs[(int) l->section];
        l->levelMatchGain = (v > -69.0f && v < 99.0f) ? juce::Decibels::decibelsToGain (target - v) : 1.0f;
    }

    // what plays by default: the newest section
    const int newest = result.ok ? 2 : (! tracks.empty() ? 1 : 0);
    engine.setLanes (std::move (lanes));
    if (lastNewestSection != newest)
        for (int i = 0; i < 3; ++i) engine.setSectionEnabled (i, i == newest);
    lastNewestSection = newest;
    player.refresh();
}

void MixMainComponent::openPlayer()
{
    if (! audioReady) status ("!No audio output available: playback is disabled.");
    if (playerWindow == nullptr)
        playerWindow = std::make_unique<PlayerWindow> (player);
    playerWindow->setVisible (true);
    playerWindow->toFront (true);
    player.grabKeyboardFocus();
}

//==============================================================================
int MixMainComponent::getNumRows() { return (int) tracks.size(); }

void MixMainComponent::paintListBoxItem (int, juce::Graphics&, int, int, bool) {}

juce::Component* MixMainComponent::refreshComponentForRow (int row, bool selected, juce::Component* existing)
{
    if (row >= (int) tracks.size())
    {
        delete existing;
        return nullptr;
    }
    auto* r = dynamic_cast<TrackRow*> (existing);
    if (r == nullptr)
    {
        delete existing;
        r = new TrackRow (*this);
    }
    r->update (row, selected);
    return r;
}

void MixMainComponent::deleteKeyPressed (int)
{
    removeButton.triggerClick();
}

//==============================================================================
void MixMainComponent::runDemoForSnapshot (const juce::File& reference, const juce::File& myMix, std::function<void (int)> pump)
{
    auto waitJob = [&] { while (job != nullptr) pump (50); pump (50); };

    loadReference (reference);
    waitJob();

    const auto dir = reference.getParentDirectory();
    if (StemSeparator::findModelFile().existsAsFile())
    {
        analyseReference();
        waitJob();
    }
    else
    {
        // stand-in for the AI: pre-rendered stems next to the reference
        for (int i = 0; i < kNumStemTypes; ++i)
        {
            auto f = dir.getChildFile ("ref_" + stemKey ((StemType) i) + ".wav");
            if (! f.existsAsFile()) continue;
            auto a = loadAudioFile (f, StemSeparator::sampleRate);
            StemSeparator::Stem s { (StemType) i, a.buffer };
            StemProfile sp;
            sp.type = s.type;
            sp.profile = AudioAnalyzer::analyse (s.audio, StemSeparator::sampleRate);
            finaliseDepth (sp.profile, s.type);
            sp.levelRelMixDb = sp.profile.integratedLufs - refProfile.mix.integratedLufs;
            refProfile.stems.push_back (sp);
            refStems.push_back (std::move (s));
        }
        refreshReference();
    }

    juce::Array<juce::File> mine;
    for (auto& f : myMix.getParentDirectory().findChildFiles (juce::File::findFiles, false, "my_*.wav"))
        if (f != myMix) mine.add (f);
    addTracks (mine);
    waitJob();

    runMix();
    waitJob();
    status ("> (snapshot) report:");
    for (auto& l : juce::StringArray::fromLines (AutoMixer::buildReport (tracks, refProfile, result, amounts)))
        std::cout << l << std::endl;
}

} // namespace am
