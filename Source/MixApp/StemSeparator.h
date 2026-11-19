#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>
#include <functional>
#include <memory>
#include "Analysis/Profiles.h"

namespace am
{

/** AI source separation (Demucs v4 hybrid transformer, via demucs.cpp).

    Input must be stereo at 44.1 kHz. The song is cut into ~30 s chunks with
    overlap and the chunks run in parallel on several threads; the results are
    cross-faded back together. */
class StemSeparator
{
public:
    StemSeparator();
    ~StemSeparator();

    static constexpr double sampleRate = 44100.0;

    static juce::String defaultModelFileName();       // 6-source model
    static juce::String modelDownloadUrl (const juce::String& fileName);
    static juce::File userModelFolder();
    /** Looks next to the app, in the bundle resources and in the user folder. */
    static juce::File findModelFile();

    bool loadModel (const juce::File& modelFile, juce::String& error);
    bool isLoaded() const noexcept;
    int numSources() const noexcept;                   // 4 or 6
    StemType sourceType (int index) const noexcept;

    struct Stem
    {
        StemType type;
        juce::AudioBuffer<float> audio;
    };

    /** progress(0..1, message) -> return false to cancel. Returns empty on cancel/error. */
    using Progress = std::function<bool (float, const juce::String&)>;
    std::vector<Stem> separate (const juce::AudioBuffer<float>& stereo44k, int numThreads, Progress progress);

    static int recommendedThreads();

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

} // namespace am
