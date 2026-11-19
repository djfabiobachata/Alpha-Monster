#include "MasterEditor.h"
#include "Analysis/AudioFileUtils.h"

namespace am
{

namespace
{
    std::array<float, kNumBands> displayNormalise (const std::array<float, kNumBands>& bands)
    {
        float m = 0.0f;
        for (int b = 10; b <= 24; ++b) m += bands[(size_t) b];
        m /= 15.0f;
        auto out = bands;
        for (auto& v : out) v -= m;
        return out;
    }

    juce::String fmt (float v, int decimals = 1)
    {
        if (v <= -69.0f) return "---";
        return juce::String (v, decimals);
    }
}

//==============================================================================
MasterSurface::MasterSurface (MasterProcessor& p) : proc (p)
{
    setSize (baseWidth, baseHeight);

    for (auto* b : { &loadRefButton, &learnButton, &analyseButton, &abButton, &matchButton, &resetButton })
        addAndMakeVisible (b);

    loadRefButton.setTooltip ("Carica il brano di riferimento (WAV, AIFF, FLAC, MP3...). Puoi anche trascinarlo qui.");
    analyseButton.setTooltip ("Analizza un bounce del TUO brano invece di usare LEARN.");
    learnButton.setTooltip ("Premi LEARN e fai suonare il tuo brano nella DAW (almeno 20-30 s, meglio tutto). Premi di nuovo per finire.");
    abButton.setTooltip ("Ascolta il reference al posto dell'uscita del plugin, per il confronto A/B.");
    matchButton.setTooltip ("Durante la riproduzione: regola DRIVE perche' la loudness integrata in uscita sia uguale al reference.");
    resetButton.setTooltip ("Azzera la misura della loudness integrata in uscita.");
    learnButton.setClickingTogglesState (false);
    abButton.setClickingTogglesState (true);
    matchButton.getProperties().set ("style", "big");

    loadRefButton.onClick = [this] { chooseFile (true); };
    analyseButton.onClick = [this] { chooseFile (false); };
    learnButton.onClick = [this]
    {
        const bool start = ! proc.isLearning();
        proc.setLearning (start);
        learnButton.setToggleState (start, juce::dontSendNotification);
        status (start ? "> LEARN attivo: fai suonare il tuo brano nella DAW..." : "> LEARN completato.");
        if (! start && proc.lastError.isNotEmpty()) { status ("!" + proc.lastError); proc.lastError.clear(); }
    };
    abButton.onClick = [this]
    {
        if (abButton.getToggleState() && ! proc.hasReferenceAudio())
        {
            abButton.setToggleState (false, juce::dontSendNotification);
            status ("!Carica prima un reference.");
            return;
        }
        proc.setListenToReference (abButton.getToggleState());
    };
    matchButton.onClick = [this]
    {
        if (proc.matchLoudnessNow()) status ("> DRIVE regolato sulla loudness del reference. Continua a riprodurre per verificare.");
        else status ("!Serve un reference e qualche secondo di riproduzione prima di MATCH.");
    };
    resetButton.onClick = [this] { proc.resetOutputMeter(); };

    addAndMakeVisible (refName);
    refName.fontHeight = 15.0f;
    refName.setLines ({ "NESSUN REFERENCE - premi CARICA REF o trascina un file" });

    setupKnob (inKnob, "input", "Guadagno d'ingresso");
    setupKnob (outKnob, "output", "Livello d'uscita (dopo il limiter)");
    setupKnob (mixKnob, "mix", "Dry/Wet");

    addAndMakeVisible (refPanel);
    for (auto* d : { &refLufs, &refTp, &refLra, &refPlr })
        addAndMakeVisible (d);
    refLufs.setCaption ("LUFS INTEGRATI");
    refTp.setCaption ("TRUE PEAK");
    refLra.setCaption ("LRA");
    refPlr.setCaption ("PLR");
    addAndMakeVisible (refInfo);
    refInfo.fontHeight = 13.0f;

    addAndMakeVisible (spectrum);
    spectrum.caption = "REF (crema)  -  TUO BRANO (rosso)  -  USCITA (ciano)  -  EQ (arancio)";
    spectrum.setRange (-36.0f, 12.0f);
    spectrum.showZeroLine = true;

    addAndMakeVisible (loudPanel);
    addAndMakeVisible (needle);
    for (auto* d : { &outShort, &outInt, &targetLufs })
        addAndMakeVisible (d);
    outShort.setCaption ("SHORT-TERM");
    outInt.setCaption ("INTEGRATI");
    targetLufs.setCaption ("TARGET");
    targetLufs.setColour (theme::amber);

    for (auto* pnl : { &eqPanel, &compPanel, &widthPanel, &limPanel, &outPanel })
        addAndMakeVisible (pnl);

    setupKnob (eqKnob, "eqAmount", "Quanto applicare la curva di match EQ");
    setupKnob (compKnob, "compAmount", "Compressione 'glue' (soglia e rapporto automatici dal tuo brano)");
    setupKnob (widthKnob, "widthAmount", "Quanto avvicinare l'immagine stereo (3 bande) al reference");
    setupKnob (driveKnob, "drive", "Guadagno nel limiter: e' cio' che porta la loudness al livello del reference");
    setupKnob (ceilingKnob, "ceiling", "Tetto true-peak del limiter (dBTP)");

    setupToggle (eqOn, "ON", "eqOn");
    setupToggle (compOn, "ON", "compOn");
    setupToggle (widthOn, "ON", "widthOn");
    setupToggle (limOn, "ON", "limOn");
    setupToggle (bassMono, "BASS MONO", "bassMono");
    bassMono.setTooltip ("Rende mono le frequenze sotto i 200 Hz");

    for (auto* t : { &eqMoves, &compInfo, &widthInfo })
    {
        addAndMakeVisible (t);
        t->fontHeight = 12.5f;
    }
    addAndMakeVisible (compGr);
    addAndMakeVisible (limGr);
    compGr.setCaption ("GR");
    limGr.setCaption ("GR");
    addAndMakeVisible (scope);
    addAndMakeVisible (outL);
    addAndMakeVisible (outR);
    outL.setCaption ("L");
    outR.setCaption ("R");
    addAndMakeVisible (driveDisplay);
    driveDisplay.setCaption ("DRIVE dB");
    driveDisplay.setColour (theme::amber);
    addAndMakeVisible (corrLed);

    addAndMakeVisible (statusLine);
    statusLine.fontHeight = 14.0f;
    status ("> ALPHA MONSTER MASTER v" AM_VERSION_STRING " pronto.");
    status ("> 1) CARICA REF   2) LEARN o ANALIZZA FILE   3) MATCH");

    fifoL.resize (8192);
    fifoR.resize (8192);
    ring.assign (8192, 0.0f);
    fftData.assign (16384, 0.0f);
    liveBands.fill (-100.0f);

    proc.onProfilesChanged = [this] { updateProfiles(); };
    updateProfiles();
    startTimerHz (30);
}

MasterSurface::~MasterSurface()
{
    proc.onProfilesChanged = nullptr;
}

void MasterSurface::setupKnob (juce::Slider& s, const juce::String& id, const juce::String& tooltip, bool bipolar)
{
    s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    s.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    s.setTooltip (tooltip);
    s.getProperties().set ("bipolar", bipolar);
    s.setDoubleClickReturnValue (true, (double) proc.params.getParameter (id)->convertFrom0to1 (proc.params.getParameter (id)->getDefaultValue()));
    addAndMakeVisible (s);
    sliderAttachments.push_back (std::make_unique<SliderAttachment> (proc.params, id, s));
}

void MasterSurface::setupToggle (juce::ToggleButton& b, const juce::String& text, const juce::String& id)
{
    b.setButtonText (text);
    addAndMakeVisible (b);
    buttonAttachments.push_back (std::make_unique<ButtonAttachment> (proc.params, id, b));
}

void MasterSurface::status (const juce::String& line)
{
    statusLine.addLine (line, 50);
}

//==============================================================================
void MasterSurface::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    AlphaLookAndFeel::drawBodyBackground (g, r);

    // header strip
    auto header = r.removeFromTop (78.0f);
    g.setColour (theme::headerBg);
    g.fillRoundedRectangle (header.reduced (6.0f, 6.0f), 8.0f);
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.drawRoundedRectangle (header.reduced (6.0f, 6.0f), 8.0f, 1.5f);
    AlphaLookAndFeel::drawLogo (g, juce::Rectangle<float> (18.0f, 14.0f, 380.0f, 50.0f), "Master  -  reference mastering");

    // knob legends in header
    AlphaLookAndFeel::drawLegend (g, "IN", juce::Rectangle<float> (950, 8, 60, 14), juce::Justification::centred, 13.0f);
    AlphaLookAndFeel::drawLegend (g, "OUT", juce::Rectangle<float> (1020, 8, 60, 14), juce::Justification::centred, 13.0f);
    AlphaLookAndFeel::drawLegend (g, "MIX", juce::Rectangle<float> (1090, 8, 60, 14), juce::Justification::centred, 13.0f);

    // screws
    for (auto pt : { juce::Point<float> (8, 8), juce::Point<float> ((float) baseWidth - 8, 8),
                     juce::Point<float> (8, (float) baseHeight - 8), juce::Point<float> ((float) baseWidth - 8, (float) baseHeight - 8) })
        AlphaLookAndFeel::drawScrew (g, pt, 4.5f);

}

void MasterSurface::resized()
{
    // header
    loadRefButton.setBounds (420, 16, 110, 26);
    learnButton.setBounds (536, 16, 80, 26);
    analyseButton.setBounds (622, 16, 120, 26);
    abButton.setBounds (748, 16, 110, 26);
    refName.setBounds (420, 45, 506, 24);
    inKnob.setBounds (950, 20, 60, 56);
    outKnob.setBounds (1020, 20, 60, 56);
    mixKnob.setBounds (1090, 20, 60, 56);

    // row 1
    refPanel.setBounds (12, 86, 290, 300);
    {
        auto c = refPanel.getBounds().reduced (12, 8).withTrimmedTop (26);
        refLufs.setBounds (c.removeFromTop (78).reduced (30, 0));
        c.removeFromTop (8);
        auto row = c.removeFromTop (60);
        const int w = row.getWidth() / 3;
        refTp.setBounds (row.removeFromLeft (w).reduced (4, 0));
        refLra.setBounds (row.removeFromLeft (w).reduced (4, 0));
        refPlr.setBounds (row.reduced (4, 0));
        c.removeFromTop (8);
        refInfo.setBounds (c);
    }

    spectrum.setBounds (312, 86, 540, 300);

    loudPanel.setBounds (862, 86, 306, 300);
    {
        auto c = loudPanel.getBounds().reduced (12, 8).withTrimmedTop (26);
        needle.setBounds (c.removeFromTop (110).reduced (20, 0));
        c.removeFromTop (8);
        auto row = c.removeFromTop (58);
        const int w = row.getWidth() / 3;
        outShort.setBounds (row.removeFromLeft (w).reduced (3, 0));
        outInt.setBounds (row.removeFromLeft (w).reduced (3, 0));
        targetLufs.setBounds (row.reduced (3, 0));
        c.removeFromTop (12);
        auto buttons = c.removeFromTop (44);
        matchButton.setBounds (buttons.removeFromLeft (180).reduced (2));
        resetButton.setBounds (buttons.reduced (6, 6));
    }

    // row 2: modules
    const int y = 396, h = 252, gap = 10, w = (baseWidth - 24 - 4 * gap) / 5;
    int x = 12;
    for (auto* pnl : { &eqPanel, &compPanel, &widthPanel, &limPanel, &outPanel })
    {
        pnl->setBounds (x, y, w, h);
        x += w + gap;
    }

    {
        auto c = eqPanel.getBounds().reduced (10, 8).withTrimmedTop (26);
        auto top = c.removeFromTop (110);
        eqOn.setBounds (top.removeFromLeft (52).withSizeKeepingCentre (52, 22));
        eqKnob.setBounds (top.reduced (8, 0));
        c.removeFromTop (6);
        eqMoves.setBounds (c);
    }
    {
        auto c = compPanel.getBounds().reduced (10, 8).withTrimmedTop (26);
        auto top = c.removeFromTop (110);
        compOn.setBounds (top.removeFromLeft (52).withSizeKeepingCentre (52, 22));
        compGr.setBounds (top.removeFromRight (22));
        compKnob.setBounds (top.reduced (6, 0));
        c.removeFromTop (6);
        compInfo.setBounds (c);
    }
    {
        auto c = widthPanel.getBounds().reduced (10, 8).withTrimmedTop (26);
        auto top = c.removeFromTop (110);
        auto toggles = top.removeFromLeft (92);
        toggles.removeFromTop (18);
        widthOn.setBounds (toggles.removeFromTop (30).withSizeKeepingCentre (92, 22));
        bassMono.setBounds (toggles.removeFromTop (30).withSizeKeepingCentre (92, 22));
        widthKnob.setBounds (top.reduced (2, 0));
        c.removeFromTop (6);
        widthInfo.setBounds (c);
    }
    {
        auto c = limPanel.getBounds().reduced (10, 8).withTrimmedTop (26);
        auto top = c.removeFromTop (48);
        limOn.setBounds (top.removeFromLeft (60).withSizeKeepingCentre (60, 22));
        driveDisplay.setBounds (top.removeFromRight (110));
        c.removeFromTop (6);
        limGr.setBounds (c.removeFromRight (22));
        auto knobs = c;
        driveKnob.setBounds (knobs.removeFromTop (knobs.getHeight() / 2).reduced (10, 0));
        ceilingKnob.setBounds (knobs.reduced (16, 2));
    }
    {
        auto c = outPanel.getBounds().reduced (10, 8).withTrimmedTop (28);
        corrLed.setBounds (c.removeFromBottom (20));
        c.removeFromBottom (4);
        scope.setBounds (c.removeFromBottom (112).withSizeKeepingCentre (112, 112));
        c.removeFromBottom (4);
        outL.setBounds (c.removeFromLeft (c.getWidth() / 2).withSizeKeepingCentre (26, c.getHeight()));
        outR.setBounds (c.withSizeKeepingCentre (26, c.getHeight()));
    }

    statusLine.setBounds (12, 658, baseWidth - 24, 52);
}

//==============================================================================
void MasterSurface::chooseFile (bool reference)
{
    chooser = std::make_unique<juce::FileChooser> (reference ? "Scegli il brano di riferimento" : "Scegli il tuo brano (bounce)",
                                                   juce::File::getSpecialLocation (juce::File::userMusicDirectory),
                                                   supportedFileWildcard());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this, reference] (const juce::FileChooser& fc)
    {
        const auto f = fc.getResult();
        if (! f.existsAsFile()) return;
        if (reference) proc.loadReferenceFile (f);
        else           proc.analyseOwnTrackFile (f);
        status ("> Analisi in corso: " + f.getFileName());
    });
}

void MasterSurface::filesDropped (const juce::StringArray& files, int, int)
{
    if (files.isEmpty()) return;
    const juce::File f (files[0]);
    const bool own = juce::ModifierKeys::getCurrentModifiers().isShiftDown();
    if (own) proc.analyseOwnTrackFile (f);
    else     proc.loadReferenceFile (f);
    status ("> Analisi in corso: " + f.getFileName() + (own ? " (tuo brano)" : " (reference)"));
}

//==============================================================================
void MasterSurface::updateProfiles()
{
    const auto& ref = proc.getReference();
    const auto& own = proc.getLearned();

    if (ref.valid)
    {
        refName.setLines ({ "REF: " + proc.getReferenceName() + "   (" + juce::String (ref.durationSec / 60.0f, 1) + " min)" });
        refLufs.setText (juce::String (ref.integratedLufs, 1));
        refTp.setText (juce::String (ref.truePeakDb, 1));
        refLra.setText (juce::String (ref.loudnessRange, 1));
        refPlr.setText (juce::String (ref.plrDb, 1));
        targetLufs.setText (juce::String (ref.integratedLufs, 1));

        juce::StringArray info;
        info.add ("Max short-term " + juce::String (ref.maxShortTermLufs, 1) + " LUFS");
        info.add ("Crest " + juce::String (ref.crestDb, 1) + " dB   Tilt " + juce::String (ref.tiltDbPerOct, 1) + " dB/oct");
        info.add ("Immagine: " + ref.widthText() + "  (" + juce::String (juce::roundToInt (ref.width * 100)) + "%)");
        info.add ("Larghezza L/M/H " + juce::String (juce::roundToInt (ref.regionWidth[0] * 100)) + "/"
                  + juce::String (juce::roundToInt (ref.regionWidth[1] * 100)) + "/" + juce::String (juce::roundToInt (ref.regionWidth[2] * 100)) + " %");
        info.add ("Energia L/M/H " + juce::String (juce::roundToInt (ref.regionEnergy[0] * 100)) + "/"
                  + juce::String (juce::roundToInt (ref.regionEnergy[1] * 100)) + "/" + juce::String (juce::roundToInt (ref.regionEnergy[2] * 100)) + " %");
        info.add ("Correlazione " + juce::String (ref.correlation, 2));
        refInfo.setLines (info);
    }
    else
    {
        for (auto* d : { &refLufs, &refTp, &refLra, &refPlr, &targetLufs }) d->setText ("----");
        refInfo.setLines ({ "Carica un reference:", "il brano che vuoi", "far 'suonare' come", "il tuo master." });
    }

    // EQ moves
    if (ref.valid && own.valid)
    {
        auto moves = describeCurve (proc.getEqCurve(), 1.0f, 6);
        if (moves.isEmpty()) moves.add ("Bilanciamento gia' simile");
        moves.insert (0, "Correzioni (100%):");
        eqMoves.setLines (moves);

        const float crestDiff = (own.truePeakDb - own.integratedLufs) - ref.plrDb;
        compInfo.setLines ({ "Tuo PLR  " + juce::String (own.truePeakDb - own.integratedLufs, 1) + " dB",
                             "Ref PLR  " + juce::String (ref.plrDb, 1) + " dB",
                             crestDiff > 2.0f ? "Il tuo brano e' piu'" : "Dinamica simile:",
                             crestDiff > 2.0f ? "dinamico: alza GLUE" : "GLUE leggero basta" });

        const auto side = proc.getSideGainsDb();
        auto pct = [] (float w) { return juce::String (juce::roundToInt (w * 100)); };
        widthInfo.setLines ({ "LARGHEZZA   REF  TUO",
                              "LOW         " + pct (ref.regionWidth[0]).paddedLeft (' ', 3) + "  " + pct (own.regionWidth[0]).paddedLeft (' ', 3),
                              "MID         " + pct (ref.regionWidth[1]).paddedLeft (' ', 3) + "  " + pct (own.regionWidth[1]).paddedLeft (' ', 3),
                              "HIGH        " + pct (ref.regionWidth[2]).paddedLeft (' ', 3) + "  " + pct (own.regionWidth[2]).paddedLeft (' ', 3),
                              "Side dB  " + juce::String (side[0], 1) + " / " + juce::String (side[1], 1) + " / " + juce::String (side[2], 1) });
    }
    else
    {
        eqMoves.setLines ({ ref.valid ? "Ora LEARN o" : "Serve un reference", ref.valid ? "ANALIZZA FILE" : "e il tuo brano", "per calcolare", "la curva." });
        compInfo.setLines ({ "Soglia e rapporto", "si adattano al", "tuo brano." });
        widthInfo.setLines ({ "Immagine stereo", "per 3 bande:", "200 Hz / 4 kHz" });
    }

    updateSpectrum();
}

void MasterSurface::updateSpectrum()
{
    std::vector<SpectrumScreen::Curve> curves;
    const auto& centres = bandCentres();
    std::vector<float> freqs (centres.begin(), centres.end());

    const auto& ref = proc.getReference();
    const auto& own = proc.getLearned();

    if (ref.valid)
    {
        auto d = displayNormalise (ref.bandDb);
        curves.push_back ({ freqs, std::vector<float> (d.begin(), d.end()), theme::cream, 2.0f, true, "REFERENCE" });
    }
    if (own.valid)
    {
        auto d = displayNormalise (own.bandDb);
        curves.push_back ({ freqs, std::vector<float> (d.begin(), d.end()), theme::ledRed, 1.6f, false, "TUO BRANO" });
    }
    if (liveBands[17] > -99.0f)
    {
        auto d = displayNormalise (liveBands);
        curves.push_back ({ freqs, std::vector<float> (d.begin(), d.end()), theme::crtTrace, 1.4f, false, "USCITA" });
    }
    if (ref.valid && own.valid)
    {
        std::vector<float> f, db;
        for (float hz = 20.0f; hz <= 20000.0f; hz *= 1.06f)
        {
            f.push_back (hz);
            db.push_back (proc.getEqResponseDb (hz));
        }
        curves.push_back ({ f, db, theme::orange, 2.2f, false, "EQ" });
    }
    spectrum.setCurves (std::move (curves));
}

void MasterSurface::refreshAll()
{
    updateProfiles();
    timerCallback();
}

void MasterSurface::timerCallback()
{
    // meters
    const auto& m = proc.meters;
    const auto& ref = proc.getReference();
    const float st = m.outShortTerm.load();
    outShort.setText (fmt (st));
    const float integ = proc.getOutputIntegratedLufs();
    outInt.setText (fmt (integ));
    needle.setValue (ref.valid && st > -69.0f ? st - ref.integratedLufs : -12.0f);
    compGr.setLevel (m.compGr.load());
    limGr.setLevel (m.limGr.load());
    outL.setLevel (m.outPeakLDb.load());
    outR.setLevel (m.outPeakRDb.load());
    const float corr = m.correlation.load();
    scope.setCorrelation (corr);
    corrLed.setOn (corr > -0.1f);
    corrLed.setLabel (corr > -0.1f ? "FASE OK  " + juce::String (corr, 2) : "FASE!  " + juce::String (corr, 2));
    driveDisplay.setText (juce::String (proc.params.getRawParameterValue ("drive")->load(), 1));

    // learn status
    if (proc.isLearning())
    {
        learnButton.setButtonText ("STOP " + juce::String ((int) proc.getLearnSeconds()) + "s");
        learnButton.setToggleState (true, juce::dontSendNotification);
    }
    else
    {
        learnButton.setButtonText ("LEARN");
        learnButton.setToggleState (false, juce::dontSendNotification);
    }

    const bool busyNow = proc.isBusy();
    if (busyNow && ! wasBusy) status ("> " + proc.getBusyText() + "...");
    if (! busyNow && wasBusy) status ("> Analisi completata.");
    wasBusy = busyNow;
    if (proc.lastError.isNotEmpty() && proc.lastError != lastErrorShown)
    {
        status ("!" + proc.lastError);
        lastErrorShown = proc.lastError;
        proc.lastError.clear();
    }
    abButton.setToggleState (proc.isListeningToReference(), juce::dontSendNotification);

    // scope & live spectrum
    const int got = proc.readScopeSamples (fifoL.data(), fifoR.data(), (int) fifoL.size());
    if (got > 0)
    {
        scope.pushSamples (fifoL.data(), fifoR.data(), got);
        for (int i = 0; i < got; ++i)
        {
            ring[(size_t) ringPos] = 0.5f * (fifoL[(size_t) i] + fifoR[(size_t) i]);
            ringPos = (ringPos + 1) % (int) ring.size();
        }

        std::fill (fftData.begin(), fftData.end(), 0.0f);
        const int N = (int) ring.size();
        for (int i = 0; i < N; ++i)
        {
            const float w = 0.5f - 0.5f * std::cos (juce::MathConstants<float>::twoPi * (float) i / (float) N);
            fftData[(size_t) i] = ring[(size_t) ((ringPos + i) % N)] * w;
        }
        fft.performFrequencyOnlyForwardTransform (fftData.data(), true);

        const double fs = proc.getSampleRate() > 0 ? proc.getSampleRate() : 44100.0;
        const double binHz = fs / N;
        const auto& centres = bandCentres();
        for (int b = 0; b < kNumBands; ++b)
        {
            const double lo = centres[(size_t) b] * std::pow (2.0, -1.0 / 6.0), hi = centres[(size_t) b] * std::pow (2.0, 1.0 / 6.0);
            double sum = 0.0; int cnt = 0;
            for (int k = juce::jmax (1, (int) std::ceil (lo / binHz)); k < N / 2 && k * binHz < hi; ++k)
            {
                sum += (double) fftData[(size_t) k] * fftData[(size_t) k];
                ++cnt;
            }
            if (cnt == 0)
            {
                const int k = juce::jlimit (1, N / 2 - 1, (int) std::round (centres[(size_t) b] / binHz));
                sum = (double) fftData[(size_t) k] * fftData[(size_t) k];
                cnt = 1;
            }
            const float db = (float) (10.0 * std::log10 ((sum / cnt) * (hi - lo) + 1.0e-20));
            liveBands[(size_t) b] = liveBands[(size_t) b] < -99.0f ? db : 0.85f * liveBands[(size_t) b] + 0.15f * db;
        }
        updateSpectrum();
    }
}

//==============================================================================
MasterEditor::MasterEditor (MasterProcessor& p)
    : AudioProcessorEditor (p), surface (p)
{
    surface.setLookAndFeel (&lnf);
    addAndMakeVisible (surface);
    setResizable (true, true);
    setResizeLimits (MasterSurface::baseWidth * 6 / 10, MasterSurface::baseHeight * 6 / 10,
                     MasterSurface::baseWidth * 2, MasterSurface::baseHeight * 2);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) MasterSurface::baseWidth / MasterSurface::baseHeight);
    setSize (MasterSurface::baseWidth, MasterSurface::baseHeight);
}

MasterEditor::~MasterEditor()
{
    surface.setLookAndFeel (nullptr);
}

void MasterEditor::resized()
{
    const float scale = juce::jmin ((float) getWidth() / MasterSurface::baseWidth, (float) getHeight() / MasterSurface::baseHeight);
    surface.setTransform (juce::AffineTransform::scale (scale));
    surface.setBounds (0, 0, MasterSurface::baseWidth, MasterSurface::baseHeight);
}

} // namespace am
