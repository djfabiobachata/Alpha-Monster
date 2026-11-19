// am_snapshot: renders the Alpha Monster user interfaces to PNG files without
// opening any window. It drives the real processor with real audio so the
// screenshots show live data. Used for visual checks during development / CI.
//
// usage: am_snapshot <outDir> <reference.wav> <mytrack.wav>

#include <juce_gui_extra/juce_gui_extra.h>
#include "MasterProcessor.h"
#include "MasterEditor.h"
#include "Analysis/AudioFileUtils.h"
#if AM_SNAPSHOT_MIX
 #include "MixMainComponent.h"
#endif

static void pump (int ms)
{
    juce::MessageManager::getInstance()->runDispatchLoopUntil (ms);
}

static void save (juce::Component& c, const juce::File& f, float scale = 1.0f)
{
    auto img = c.createComponentSnapshot (c.getLocalBounds(), true, scale);
    f.deleteFile();
    juce::FileOutputStream os (f);
    juce::PNGImageFormat png;
    png.writeImageToStream (img, os);
    std::cout << "wrote " << f.getFullPathName() << std::endl;
}

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 4)
    {
        std::cout << "usage: am_snapshot <outDir> <reference> <mytrack>" << std::endl;
        return 1;
    }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto outDir = cwd.getChildFile (argv[1]);
    outDir.createDirectory();
    const auto refFile = cwd.getChildFile (argv[2]);
    const auto ownFile = cwd.getChildFile (argv[3]);

    // ---------------------------------------------------------------- Master
    {
        am::MasterProcessor proc;
        proc.setPlayConfigDetails (2, 2, 44100.0, 512);
        proc.prepareToPlay (44100.0, 512);

        proc.loadReferenceFile (refFile);
        while (proc.isBusy()) pump (50);
        pump (100);
        proc.analyseOwnTrackFile (ownFile);
        while (proc.isBusy()) pump (50);
        pump (100);

        auto own = am::loadAudioFile (ownFile, 44100.0);
        am::AlphaLookAndFeel lnf;
        am::MasterSurface surface (proc);
        surface.setLookAndFeel (&lnf);

        // play 12 s of the track through the plugin
        juce::MidiBuffer midi;
        juce::AudioBuffer<float> block (2, 512);
        const int total = juce::jmin (own.buffer.getNumSamples(), 44100 * 12);
        for (int pos = 0; pos + 512 <= total; pos += 512)
        {
            block.copyFrom (0, 0, own.buffer, 0, pos, 512);
            block.copyFrom (1, 0, own.buffer, 1, pos, 512);
            proc.processBlock (block, midi);
            if ((pos / 512) % 8 == 0) pump (1);
        }
        proc.matchLoudnessNow();
        if (auto* b = proc.params.getParameter ("boost")) b->setValueNotifyingHost (0.3f);
        proc.setRefPositionSec (11.0);
        for (int pos = 0; pos + 512 <= total; pos += 512)
        {
            block.copyFrom (0, 0, own.buffer, 0, pos, 512);
            block.copyFrom (1, 0, own.buffer, 1, pos, 512);
            proc.processBlock (block, midi);
            if ((pos / 512) % 8 == 0) pump (1);
        }
        pump (300);
        surface.refreshAll();
        pump (300);
        save (surface, outDir.getChildFile ("master.png"));
        std::cout << "out integrated " << proc.getOutputIntegratedLufs() << " ref " << proc.getReference().integratedLufs
                  << " drive " << proc.params.getRawParameterValue ("drive")->load() << std::endl;
        surface.setLookAndFeel (nullptr);
    }

    // ---------------------------------------------------------------- Master DSP self-test
    {
        auto run = [] (bool bypass, float boost, double& rmsIn, double& rmsOut, double& maxErr)
        {
            am::MasterProcessor p;
            p.setPlayConfigDetails (2, 2, 44100.0, 512);
            p.prepareToPlay (44100.0, 512);
            p.params.getParameter ("bypass")->setValueNotifyingHost (bypass ? 1.0f : 0.0f);
            p.params.getParameter ("boost")->setValueNotifyingHost (boost);
            const int lat = p.getLatencySamples();
            juce::Random rnd (3);
            std::vector<float> inHist;
            juce::MidiBuffer midi;
            juce::AudioBuffer<float> b (2, 512);
            double si = 0, so = 0; maxErr = 0; int cnt = 0;
            for (int k = 0; k < 200; ++k)
            {
                for (int i = 0; i < 512; ++i)
                {
                    const float x = 0.03f * (rnd.nextFloat() * 2.0f - 1.0f);
                    b.setSample (0, i, x); b.setSample (1, i, x * 0.8f);
                    inHist.push_back (x);
                }
                p.processBlock (b, midi);
                if (k < 20) continue;
                for (int i = 0; i < 512; ++i)
                {
                    const size_t idx = (size_t) (k * 512 + i);
                    const float ref = idx >= (size_t) lat ? inHist[idx - (size_t) lat] : 0.0f;
                    si += ref * ref; so += b.getSample (0, i) * b.getSample (0, i); ++cnt;
                    maxErr = juce::jmax (maxErr, (double) std::abs (b.getSample (0, i) - ref));
                }
            }
            rmsIn = std::sqrt (si / cnt); rmsOut = std::sqrt (so / cnt);
        };
        double i0, o0, e0, i1, o1, e1, i2, o2, e2;
        run (true, 0.0f, i0, o0, e0);      // bypass: output == delayed input
        run (false, 0.0f, i1, o1, e1);     // processing, no boost
        run (false, 1.0f, i2, o2, e2);     // full boost
        const double boostDb = 20.0 * std::log10 (o2 / o1);
        const bool ok = e0 < 1.0e-5 && boostDb > 3.0 && boostDb < 7.0;
        std::cout << "master dsp test: " << (ok ? "OK" : "FAILED") << "  bypass max error " << e0
                  << "  boost gain " << boostDb << " dB" << std::endl;
        if (! ok) return 6;
    }

#if AM_SNAPSHOT_MIX
    {
        // playback engine self-test: rates, seek, mute, solo
        juce::AudioBuffer<float> tone (2, 48000 * 4), noise (2, 44100 * 4);
        for (int i = 0; i < tone.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch) tone.setSample (ch, i, i < 48000 * 2 ? 0.0f : 0.5f * std::sin (6.2831853f * 440.0f * i / 48000.0f));
        juce::Random rnd (1);
        for (int i = 0; i < noise.getNumSamples(); ++i)
            for (int ch = 0; ch < 2; ++ch) noise.setSample (ch, i, 0.2f * (rnd.nextFloat() * 2.0f - 1.0f));

        am::PlaybackEngine e;
        auto a = std::make_shared<am::PlayerLane>(); a->name = "tone"; a->audio = &tone; a->sampleRate = 48000; a->computePeaks();
        auto b = std::make_shared<am::PlayerLane>(); b->name = "noise"; b->audio = &noise; b->sampleRate = 44100; b->section = am::PlayerLane::result; b->computePeaks();
        e.setLanes ({ a, b });
        e.setLevelMatch (false);
        e.prepareToPlay (512, 44100.0);
        juce::AudioBuffer<float> out (2, 512);
        auto rms = [&] (int blocks)
        {
            double sum = 0; int n = 0;
            for (int k = 0; k < blocks; ++k)
            {
                juce::AudioSourceChannelInfo info (&out, 0, 512);
                e.getNextAudioBlock (info);
                for (int i = 0; i < 512; ++i) { sum += out.getSample (0, i) * out.getSample (0, i); ++n; }
            }
            return std::sqrt (sum / n);
        };
        bool ok = true;
        e.play();
        const double both = rms (20);
        a->mute = true; rms (2); const double noiseOnly = rms (20);
        a->mute = false; b->solo = true; rms (2); const double soloNoise = rms (20);
        b->solo = false; a->solo = true; e.setPositionSec (0.5); rms (2); const double silentPart = rms (20);
        e.setPositionSec (2.5); rms (2); const double tonePart = rms (20);
        e.setPositionSec (3.99); rms (10);
        ok &= both > 0.05 && noiseOnly > 0.05 && std::abs (noiseOnly - soloNoise) < 0.02 && silentPart < 1.0e-4 && tonePart > 0.2 && ! e.isPlaying();
        ok &= std::abs (a->firstSoundSec - 2.0) < 0.05 && b->firstSoundSec < 0.01;
        std::cout << "player engine test: " << (ok ? "OK" : "FAILED") << "  both " << both << " noise " << noiseOnly << " solo " << soloNoise
                  << " silent " << silentPart << " tone " << tonePart << " first " << a->firstSoundSec << std::endl;
        if (! ok) return 5;
    }
    {
        am::setEnglish (true);
        am::theme::useMixPalette();
        am::AlphaLookAndFeel lnf;
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
        am::MixMainComponent mix;
        mix.setSize (am::MixMainComponent::defaultWidth, am::MixMainComponent::defaultHeight);
        mix.runDemoForSnapshot (refFile, ownFile, [] (int ms) { pump (ms); });
        save (mix, outDir.getChildFile ("mix.png"));

        // player: vocals of the reference soloed, playhead moved where they start
        auto& player = mix.getPlayerComponent();
        player.setSize (1280, 820);
        player.auditionLane ("0/Vocals");
        pump (400);
        save (player, outDir.getChildFile ("player.png"));
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }
#endif
    return 0;
}
