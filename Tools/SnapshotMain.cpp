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

#if AM_SNAPSHOT_MIX
    {
        am::AlphaLookAndFeel lnf;
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
        am::MixMainComponent mix;
        mix.setSize (am::MixMainComponent::defaultWidth, am::MixMainComponent::defaultHeight);
        mix.runDemoForSnapshot (refFile, ownFile, [] (int ms) { pump (ms); });
        save (mix, outDir.getChildFile ("mix.png"));
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }
#endif
    return 0;
}
