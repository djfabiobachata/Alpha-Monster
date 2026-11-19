// am_separate: AI stem separation from the command line.
// usage: am_separate <model.bin> <input audio> <outDir> [threads]

#include <juce_events/juce_events.h>
#include "StemSeparator.h"
#include "Analysis/AudioAnalyzer.h"
#include "Analysis/AudioFileUtils.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;
    if (argc < 4)
    {
        std::cout << "usage: am_separate <model.bin> <input> <outDir> [threads]" << std::endl;
        return 1;
    }
    const auto cwd = juce::File::getCurrentWorkingDirectory();
    const auto modelFile = cwd.getChildFile (argv[1]);
    const auto inFile = cwd.getChildFile (argv[2]);
    const auto outDir = cwd.getChildFile (argv[3]);
    const int threads = argc > 4 ? juce::String (argv[4]).getIntValue() : am::StemSeparator::recommendedThreads();

    am::StemSeparator sep;
    juce::String err;
    auto t0 = juce::Time::getMillisecondCounterHiRes();
    if (! sep.loadModel (modelFile, err))
    {
        std::cerr << err << std::endl;
        return 2;
    }
    std::cout << "model loaded (" << sep.numSources() << " sources) in "
              << (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0 << " s" << std::endl;

    auto audio = am::loadAudioFile (inFile, am::StemSeparator::sampleRate);
    if (! audio.ok())
    {
        std::cerr << audio.error << std::endl;
        return 3;
    }

    t0 = juce::Time::getMillisecondCounterHiRes();
    int lastPct = -10;
    auto stems = sep.separate (audio.buffer, threads, [&] (float p, const juce::String& msg)
    {
        const int pct = (int) (p * 100.0f);
        if (pct >= lastPct + 10) { std::cout << msg << std::endl; lastPct = pct; }
        return true;
    });
    const double secs = (juce::Time::getMillisecondCounterHiRes() - t0) / 1000.0;
    if (stems.empty())
    {
        std::cerr << "separation failed" << std::endl;
        return 4;
    }
    std::cout << "separated " << audio.buffer.getNumSamples() / 44100.0 << " s of audio in " << secs << " s with "
              << threads << " threads" << std::endl;

    outDir.createDirectory();
    const float mixLufs = am::AudioAnalyzer::measureLufs (audio.buffer, 44100.0);
    for (auto& s : stems)
    {
        const auto f = outDir.getChildFile (am::stemKey (s.type) + ".wav");
        am::writeWav (f, s.audio, 44100.0, 24);
        std::cout << am::stemKey (s.type) << ": " << am::AudioAnalyzer::measureLufs (s.audio, 44100.0) - mixLufs
                  << " dB rel. mix" << std::endl;
    }
    return 0;
}
