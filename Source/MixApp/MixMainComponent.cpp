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
                owner.tracks[(size_t) row].type = (StemType) (category.getSelectedId() - 1);
                owner.result = {};
                owner.refreshResult();
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
    loadRefButton.setTooltip ("Carica il brano di riferimento (oppure trascinalo nella finestra)");
    analyseButton.setTooltip ("Separa gli strumenti del reference con l'IA e misura come sono mixati");
    openAnalysisButton.setTooltip ("Riapri un'analisi salvata (.amref) senza rifare la separazione");
    saveAnalysisButton.setTooltip ("Salva l'analisi del reference per usarla di nuovo");
    analyseButton.getProperties().set ("style", "big");
    addAndMakeVisible (refName);
    refName.fontHeight = 15.0f;
    addAndMakeVisible (modelLed);

    // reference cards
    addAndMakeVisible (mixCard);
    mixCard.onListen = [this] { play (1); };
    for (int i = 0; i < 6; ++i)
    {
        addAndMakeVisible (stemCards[(size_t) i]);
        stemCards[(size_t) i].setStem ((StemType) i, nullptr);
        stemCards[(size_t) i].onListen = [this, i] { play (2 + i); };
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
    modeTracks.setTooltip ("Hai le tracce separate del tuo brano (voce, batteria, basso...): il mix piu' preciso");
    modeStereo.setTooltip ("Hai solo il mix stereo: Alpha Monster lo separa con l'IA e lo rimixa");
    addButton.onClick = [this] { chooseTracks(); };
    removeButton.onClick = [this]
    {
        auto rows = trackList.getSelectedRows();
        for (int i = rows.size(); --i >= 0;)
            tracks.erase (tracks.begin() + rows[i]);
        result = {};
        refreshTracks();
        refreshResult();
    };
    clearButton.onClick = [this] { tracks.clear(); result = {}; refreshTracks(); refreshResult(); };
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
    knob (levelKnob, 0, 100, 100, "%", "Quanto seguire i volumi relativi del reference");
    knob (eqKnob, 0, 100, 70, "%", "Quanto avvicinare il timbro (EQ) di ogni strumento");
    knob (dynKnob, 0, 100, 60, "%", "Quanto avvicinare la dinamica (compressione)");
    knob (spaceKnob, 0, 100, 100, "%", "Quanto seguire pan e larghezza stereo");
    knob (reverbKnob, 0, 100, 80, "%", "Quanto seguire profondita' e riverbero");
    knob (targetKnob, -24, -8, -16, " LUFS", "Loudness del mix (pre-master). Lascia spazio al mastering: -16..-14 LUFS");
    addAndMakeVisible (mixButton);
    mixButton.getProperties().set ("style", "big");
    mixButton.onClick = [this] { runMix(); };
    addAndMakeVisible (progressBar);
    progressBar.setPercentageDisplay (false);

    // result
    addAndMakeVisible (resultPanel);
    addAndMakeVisible (spectrum);
    spectrum.caption = "REFERENCE (crema)  -  PRIMA (rosso)  -  DOPO (ciano)";
    spectrum.setRange (-42.0f, 12.0f);
    addAndMakeVisible (resultInfo);
    resultInfo.fontHeight = 13.0f;
    for (auto* b : { &exportButton, &playButton, &stopButton })
        addAndMakeVisible (b);
    exportButton.getProperties().set ("style", "big");
    exportButton.onClick = [this] { exportResults(); };
    exportButton.setTooltip ("Salva il mix (WAV 24 bit), le tracce processate e il report con tutti i parametri");
    playButton.onClick = [this] { play (playSource.getSelectedId()); };
    stopButton.onClick = [this] { stopPlayback(); };
    addAndMakeVisible (playSource);

    addAndMakeVisible (statusLine);
    statusLine.fontHeight = 14.0f;

    // audio
    sourcePlayer.setSource (&transport);
    const auto err = deviceManager.initialiseWithDefaultDevices (0, 2);
    audioReady = err.isEmpty() && deviceManager.getCurrentAudioDevice() != nullptr;
    if (audioReady) deviceManager.addAudioCallback (&sourcePlayer);

    const auto model = StemSeparator::findModelFile();
    modelLed.setOn (model.existsAsFile());
    modelLed.setLabel (model.existsAsFile() ? "MODELLO IA PRONTO" : "MODELLO IA DA SCARICARE");

    status ("> ALPHA MONSTER MIX v" AM_VERSION_STRING);
    status ("> 1) CARICA REFERENCE  2) ANALIZZA  3) aggiungi le tue tracce  4) MIXA");
    if (! audioReady) status ("!Nessuna uscita audio disponibile: l'ascolto e' disattivato.");

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
    transport.setSource (nullptr);
    sourcePlayer.setSource (nullptr);
    deviceManager.removeAudioCallback (&sourcePlayer);
    download.reset();
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
    legend (levelKnob, "LIVELLI");
    legend (eqKnob, "EQ");
    legend (dynKnob, "DINAMICA");
    legend (spaceKnob, "SPAZIO");
    legend (reverbKnob, "RIVERBERO");
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
        songHint.setBounds (c.removeFromBottom (44));
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
        auto bottom = c.removeFromBottom (34);
        exportButton.setBounds (bottom.removeFromLeft (120).reduced (2));
        bottom.removeFromLeft (8);
        stopButton.setBounds (bottom.removeFromRight (64).reduced (2));
        playButton.setBounds (bottom.removeFromRight (64).reduced (2));
        playSource.setBounds (bottom.reduced (4, 4));
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
        status ("!Attendi: c'e' gia' un'operazione in corso.");
        return false;
    }
    stopPlayback();
    juce::Component::SafePointer<MixMainComponent> safe (this);
    job = std::make_unique<BackgroundJob> (std::move (work), [safe, done]
    {
        if (safe == nullptr) return;
        safe->job.reset();
        safe->setBusy (false);
        if (done) done();
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
        progressBar.setTextToDisplay ("Download modello IA: " + juce::String ((int) (progressValue * 100.0)) + "%");
    }
    else
    {
        progressBar.setTextToDisplay ({});
    }
    playButton.setToggleState (transport.isPlaying(), juce::dontSendNotification);
}

//==============================================================================
void MixMainComponent::setMode (Mode m)
{
    mode = m;
    modeTracks.setToggleState (m == Mode::tracks, juce::dontSendNotification);
    modeStereo.setToggleState (m == Mode::stereoMix, juce::dontSendNotification);
    songHint.setLines (m == Mode::tracks
        ? juce::StringArray { "Aggiungi le tracce/stem del tuo brano (o trascinale qui).",
                              "La categoria viene proposta dal nome: correggila se serve." }
        : juce::StringArray { "Carica il tuo mix stereo: verra' separato con l'IA in",
                              "voce, batteria, basso, chitarre, piano e altro, poi rimixato." });
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
    chooser = std::make_unique<juce::FileChooser> ("Scegli il brano di riferimento",
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
    status ("> Carico il reference: " + file.getFileName());
    startJob ([file, loaded, prof] (BackgroundJob& j)
    {
        j.update (-1.0f, "Caricamento reference...");
        *loaded = loadAudioFile (file, StemSeparator::sampleRate);
        if (loaded->ok())
        {
            j.update (0.5f, "Analisi del mix...");
            *prof = AudioAnalyzer::analyse (loaded->buffer, StemSeparator::sampleRate,
                                            [&j] (float p) { return j.update (0.5f + 0.5f * p, "Analisi del mix..."); });
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
        status ("> Reference caricato. Premi ANALIZZA per separare gli strumenti.");
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
                                          .withTitle ("Modello IA mancante")
                                          .withMessage ("Per separare gli strumenti serve il modello Demucs v4 (circa 55 MB).\n"
                                                        "Vuoi scaricarlo adesso?")
                                          .withButton ("Scarica")
                                          .withButton ("Annulla"),
                                      [safe, then] (int r)
        {
            if (safe != nullptr && r == 1) safe->downloadModel (then);
        });
        return false;
    }

    status ("> Carico il modello IA: " + model.getFileName());
    juce::String err;
    if (! separator.loadModel (model, err))
    {
        status ("!" + err);
        return false;
    }
    modelLed.setOn (true);
    modelLed.setLabel ("IA PRONTA (" + juce::String (separator.numSources()) + " STRUMENTI)");
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
    status ("> Download del modello IA in corso...");
    setBusy (true);
    download = juce::URL (StemSeparator::modelDownloadUrl (StemSeparator::defaultModelFileName()))
                   .downloadToFile (downloadTarget.withFileExtension ("part"),
                                    juce::URL::DownloadTaskOptions().withListener (this));
    if (download == nullptr)
    {
        setBusy (false);
        status ("!Download non riuscito: controlla la connessione.");
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
            status ("> Modello IA scaricato.");
            modelLed.setOn (true);
            if (afterDownload) { auto f = afterDownload; afterDownload = nullptr; f(); }
        }
        else
        {
            downloadTarget.withFileExtension ("part").deleteFile();
            status ("!Download del modello non riuscito. Riprova o copia il file in: " + StemSeparator::userModelFolder().getFullPathName());
        }
    });
}

void MixMainComponent::analyseReference()
{
    if (! refAudio.ok()) { status ("!Carica prima un reference."); return; }
    if (! ensureModel ([this] { analyseReference(); })) return;
    startSeparation();
}

void MixMainComponent::startSeparation()
{
    auto stems = std::make_shared<std::vector<StemSeparator::Stem>>();
    auto profile = std::make_shared<ReferenceMixProfile> (refProfile);
    const int threads = StemSeparator::recommendedThreads();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    status ("> Separazione del reference con l'IA (" + juce::String (threads) + " thread). Puo' richiedere qualche minuto...");

    startJob ([this, stems, profile, threads] (BackgroundJob& j)
    {
        *stems = separator.separate (refAudio.buffer, threads, [&j] (float p, const juce::String& m) { return j.update (0.85f * p, m); });
        if (stems->empty()) return;

        profile->stems.clear();
        for (size_t i = 0; i < stems->size(); ++i)
        {
            auto& s = (*stems)[i];
            if (! j.update (0.85f + 0.15f * (float) i / (float) stems->size(), "Analisi: " + stemName (s.type))) return;
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
        if (stems->empty()) { status ("!Separazione annullata o non riuscita."); return; }
        refStems = std::move (*stems);
        refProfile = *profile;
        result = {};
        refreshReference();
        refreshResult();
        status ("> Analisi completata in " + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 0) + " s. Ora aggiungi il tuo brano.");
    });
}

//==============================================================================
void MixMainComponent::chooseTracks()
{
    chooser = std::make_unique<juce::FileChooser> ("Scegli le tracce del tuo brano",
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
            if (! j.update ((float) i / (float) files.size(), "Caricamento: " + files[i].getFileName())) return;
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
        refreshTracks();
        refreshResult();
        status ("> " + juce::String ((int) loaded->size()) + " tracce aggiunte (" + juce::String (projectRate / 1000.0, 1) + " kHz).");
    });
}

void MixMainComponent::chooseStereoMix()
{
    chooser = std::make_unique<juce::FileChooser> ("Scegli il tuo mix stereo",
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
    const int threads = StemSeparator::recommendedThreads();
    status ("> Separo il tuo mix con l'IA: " + file.getFileName());

    startJob ([this, file, out, err, threads] (BackgroundJob& j)
    {
        j.update (-1.0f, "Caricamento del tuo mix...");
        auto a = loadAudioFile (file, StemSeparator::sampleRate);
        if (! a.ok()) { *err = a.error; return; }
        auto stems = separator.separate (a.buffer, threads, [&j] (float p, const juce::String& m) { return j.update (0.95f * p, m); });
        if (stems.empty()) { *err = "Separazione annullata."; return; }
        for (auto& s : stems)
        {
            const float lufs = AudioAnalyzer::measureLufs (s.audio, StemSeparator::sampleRate);
            if (lufs < -50.0f) continue;   // instrument not present in the song
            MixTrack t;
            t.name = stemName (s.type) + " (separata)";
            t.type = s.type;
            t.fromSeparation = true;
            t.audio = std::move (s.audio);
            out->push_back (std::move (t));
        }
    },
    [this, out, err, file]
    {
        if (err->isNotEmpty()) { status ("!" + *err); return; }
        tracks = std::move (*out);
        projectRate = StemSeparator::sampleRate;
        userMixName = file.getFileNameWithoutExtension();
        result = {};
        refreshTracks();
        refreshResult();
        status ("> Mix separato in " + juce::String ((int) tracks.size()) + " strumenti. Ora premi MIXA.");
    });
}

//==============================================================================
void MixMainComponent::runMix()
{
    if (refProfile.stems.empty()) { status ("!Prima analizza il reference (ANALIZZA) o apri un'analisi salvata."); return; }
    if (tracks.empty()) { status ("!Aggiungi le tracce del tuo brano o carica il tuo mix stereo."); return; }

    amounts.level = (float) levelKnob.getValue() / 100.0f;
    amounts.eq = (float) eqKnob.getValue() / 100.0f;
    amounts.dynamics = (float) dynKnob.getValue() / 100.0f;
    amounts.space = (float) spaceKnob.getValue() / 100.0f;
    amounts.reverb = (float) reverbKnob.getValue() / 100.0f;
    amounts.targetLufs = (float) targetKnob.getValue();

    auto res = std::make_shared<MixResult>();
    const auto t0 = juce::Time::getMillisecondCounterHiRes();
    status ("> Mix automatico in corso...");
    startJob ([this, res] (BackgroundJob& j)
    {
        *res = AutoMixer::run (tracks, refProfile, projectRate, amounts,
                               [&j] (float p, const juce::String& m) { return j.update (p, m); });
    },
    [this, res, t0]
    {
        if (! res->ok) { status ("!" + (res->error.isNotEmpty() ? res->error : juce::String ("Mix annullato."))); return; }
        result = std::move (*res);
        refreshTracks();
        refreshResult();
        status ("> Mix completato in " + juce::String ((juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0, 0)
                + " s. Ascolta 'Risultato' e premi ESPORTA.");
        for (auto& w : result.warnings) status ("!" + w);
    });
}

void MixMainComponent::exportResults()
{
    if (! result.ok) { status ("!Niente da esportare: esegui prima il mix."); return; }
    chooser = std::make_unique<juce::FileChooser> ("Scegli la cartella di esportazione",
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
        auto stemsDir = folder.getChildFile ("Tracce processate");
        stemsDir.createDirectory();
        for (size_t i = 0; i < tracks.size(); ++i)
            ok &= writeWav (stemsDir.getChildFile (juce::String ((int) i + 1).paddedLeft ('0', 2) + " " + juce::File::createLegalFileName (tracks[i].name) + ".wav"),
                            tracks[i].processed, result.sampleRate, 24);
        ok &= folder.getChildFile ("Report mix.txt").replaceWithText (AutoMixer::buildReport (tracks, refProfile, result, amounts));
        ok &= folder.getChildFile ("Parametri mix.json").replaceWithText (juce::JSON::toString (AutoMixer::buildJson (tracks, refProfile, result)));
        status (ok ? "> Esportato in: " + folder.getFullPathName() : "!Errore durante l'esportazione in " + folder.getFullPathName());
        if (ok) folder.revealToUser();
    });
}

void MixMainComponent::saveAnalysis()
{
    if (refProfile.stems.empty()) { status ("!Nessuna analisi da salvare."); return; }
    chooser = std::make_unique<juce::FileChooser> ("Salva l'analisi del reference",
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
        status (ok ? "> Analisi salvata: " + f.getFileName() : "!Impossibile salvare " + f.getFullPathName());
    });
}

void MixMainComponent::loadAnalysis()
{
    chooser = std::make_unique<juce::FileChooser> ("Apri un'analisi (.amref)",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory), "*.amref;*.json");
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
    {
        auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        auto p = ReferenceMixProfile::fromVar (juce::JSON::parse (f));
        if (p.stems.empty() || ! p.mix.valid) { status ("!File di analisi non valido."); return; }
        refProfile = p;
        refAudio = {};
        refStems.clear();
        result = {};
        refreshReference();
        refreshResult();
        status ("> Analisi caricata: " + refProfile.sourceName + " (l'ascolto del reference non e' disponibile).");
    });
}

//==============================================================================
void MixMainComponent::refreshReference()
{
    if (refProfile.mix.valid)
        refName.setLines ({ "REF: " + refProfile.sourceName + "   " + juce::String (refProfile.mix.durationSec / 60.0f, 1) + " min   "
                            + juce::String (refProfile.mix.integratedLufs, 1) + " LUFS"
                            + (refProfile.stems.empty() ? "   - premi ANALIZZA" : "   - " + juce::String ((int) refProfile.stems.size()) + " strumenti analizzati") });
    else
        refName.setLines ({ "NESSUN REFERENCE - premi CARICA REFERENCE o trascina un file nella finestra" });

    mixCard.setMixSummary (refProfile.mix, refProfile.sourceName);
    for (int i = 0; i < 6; ++i)
    {
        const auto* s = refProfile.find ((StemType) i);
        stemCards[(size_t) i].setStem ((StemType) i, s);
    }
    refreshPlayerSources();
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
        curves.push_back ({ freqs, norm (result.mixBefore), theme::ledRed, 1.5f, false, "PRIMA" });
        curves.push_back ({ freqs, norm (result.mixAfter), theme::crtTrace, 1.8f, false, "DOPO" });
    }
    spectrum.setCurves (std::move (curves));

    juce::StringArray lines;
    if (result.ok)
    {
        lines.add ("MIX: " + juce::String (result.mixAfter.integratedLufs, 1) + " LUFS  TP " + juce::String (result.mixAfter.truePeakDb, 1));
        lines.add ("Larghezza " + juce::String (juce::roundToInt (result.mixAfter.width * 100)) + "% (ref "
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
        lines.add ("Qui vedrai il confronto tra");
        lines.add ("il reference e il tuo mix,");
        lines.add ("prima e dopo Alpha Monster.");
        lines.add ("");
        lines.add ("Tutti i parametri (volume, pan,");
        lines.add ("EQ, compressore, riverbero)");
        lines.add ("finiscono nel report esportato.");
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
    refreshPlayerSources();
}

void MixMainComponent::refreshPlayerSources()
{
    const int previous = playSource.getSelectedId();
    playSource.clear (juce::dontSendNotification);
    if (refAudio.ok()) playSource.addItem ("Reference", 1);
    for (size_t i = 0; i < refStems.size(); ++i)
        playSource.addItem ("Reference: " + stemName (refStems[i].type), 2 + (int) refStems[i].type);
    if (! tracks.empty()) playSource.addItem ("Il tuo brano (originale)", 10);
    if (result.ok) playSource.addItem ("Risultato Alpha Monster", 11);
    if (playSource.indexOfItemId (previous) >= 0) playSource.setSelectedId (previous, juce::dontSendNotification);
    else if (playSource.getNumItems() > 0) playSource.setSelectedItemIndex (playSource.getNumItems() - 1, juce::dontSendNotification);
}

//==============================================================================
void MixMainComponent::play (int id)
{
    if (! audioReady) { status ("!Nessuna uscita audio disponibile."); return; }
    stopPlayback();

    double rate = StemSeparator::sampleRate;
    juce::AudioBuffer<float> buf;
    if (id == 1 && refAudio.ok()) buf = refAudio.buffer;
    else if (id >= 2 && id < 8)
    {
        for (auto& s : refStems) if ((int) s.type == id - 2) buf = s.audio;
    }
    else if (id == 10 && ! tracks.empty())
    {
        int len = 0;
        for (auto& t : tracks) len = juce::jmax (len, t.audio.getNumSamples());
        buf.setSize (2, len);
        buf.clear();
        for (auto& t : tracks)
            for (int ch = 0; ch < 2; ++ch) buf.addFrom (ch, 0, t.audio, ch, 0, t.audio.getNumSamples());
        rate = projectRate;
    }
    else if (id == 11 && result.ok)
    {
        buf = result.mix;
        rate = result.sampleRate;
    }
    if (buf.getNumSamples() == 0) return;

    playBuffer = std::move (buf);
    memorySource = std::make_unique<juce::MemoryAudioSource> (playBuffer, false, false);
    transport.setSource (memorySource.get(), 0, nullptr, rate, 2);
    transport.setPosition (0.0);
    transport.start();
    if (playSource.indexOfItemId (id) >= 0) playSource.setSelectedId (id, juce::dontSendNotification);
}

void MixMainComponent::stopPlayback()
{
    transport.stop();
    transport.setSource (nullptr);
    memorySource.reset();
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
