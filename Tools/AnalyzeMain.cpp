// am_analyze: prints the Alpha Monster analysis of one or more audio files as JSON.
// Used for testing the analysis engine and in CI.

#include <juce_events/juce_events.h>
#include "Analysis/AudioAnalyzer.h"
#include "Analysis/AudioFileUtils.h"
#include "Analysis/FilterDesign.h"

int main (int argc, char* argv[])
{
    juce::ScopedJuceInitialiser_GUI init;

    if (argc < 2)
    {
        std::cout << "usage: am_analyze <file> [<file> ...] [--match <target>]" << std::endl;
        return 1;
    }

    juce::StringArray args;
    for (int i = 1; i < argc; ++i) args.add (argv[i]);

    juce::String matchTarget;
    if (int idx = args.indexOf ("--match"); idx >= 0 && idx + 1 < args.size())
    {
        matchTarget = args[idx + 1];
        args.removeRange (idx, 2);
    }

    am::SignalProfile last;
    for (auto& a : args)
    {
        auto f = juce::File::getCurrentWorkingDirectory().getChildFile (a);
        auto loaded = am::loadAudioFile (f);
        if (! loaded.ok())
        {
            std::cerr << loaded.error << std::endl;
            return 2;
        }

        const auto t0 = juce::Time::getMillisecondCounterHiRes();
        auto p = am::AudioAnalyzer::analyse (loaded.buffer, loaded.sampleRate);
        const auto ms = juce::Time::getMillisecondCounterHiRes() - t0;

        std::cout << "== " << f.getFileName() << "  (" << juce::String (ms, 0) << " ms)" << std::endl;
        std::cout << "LUFS " << p.integratedLufs << "  TP " << p.truePeakDb << "  LRA " << p.loudnessRange
                  << "  crest " << p.crestDb << "  active " << p.activeFraction << std::endl;
        std::cout << "pan " << p.pan << " (" << p.panText() << ")  width " << p.width << " [" << p.regionWidth[0] << ", "
                  << p.regionWidth[1] << ", " << p.regionWidth[2] << "]  corr " << p.correlation
                  << "  " << p.widthText() << std::endl;
        std::cout << "reverb " << p.reverbAmount << "  rt60 " << p.rt60 << "  transient " << p.transientSharpness
                  << "  depth " << p.depth << "  centroid " << p.centroidHz << "  tilt " << p.tiltDbPerOct << std::endl;
        last = p;
    }

    if (matchTarget.isNotEmpty())
    {
        auto t = am::loadAudioFile (juce::File::getCurrentWorkingDirectory().getChildFile (matchTarget));
        auto tp = am::AudioAnalyzer::analyse (t.buffer, t.sampleRate);
        am::MatchCurveOptions o;
        auto curve = am::computeMatchCurve (last, tp, o);
        std::cout << "match curve:";
        for (auto c : curve) std::cout << " " << juce::String (c, 1);
        std::cout << std::endl << am::describeCurve (curve).joinIntoString ("\n") << std::endl;

        auto fitted = am::GraphicEq::fitGains (curve, 44100.0, 2.2f);
        am::GraphicEq eq; eq.prepare (44100.0, 2); eq.setTargetCurve (curve);
        float maxErr = 0;
        for (int b = 2; b < am::kNumBands - 1; ++b)
            maxErr = juce::jmax (maxErr, std::abs (eq.responseDb (am::bandCentres()[(size_t) b]) - curve[(size_t) b]));
        std::cout << "graphic EQ fit max error: " << maxErr << " dB  (gain[17]=" << fitted[17] << ")" << std::endl;
    }
    return 0;
}
