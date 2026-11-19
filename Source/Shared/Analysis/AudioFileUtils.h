#pragma once

#include <juce_audio_formats/juce_audio_formats.h>

namespace am
{

struct LoadedAudio
{
    juce::AudioBuffer<float> buffer;   // always stereo
    double sampleRate = 0.0;
    juce::String name;
    juce::String error;
    bool sourceWasMono = false;

    bool ok() const noexcept { return error.isEmpty() && buffer.getNumSamples() > 0; }
};

/** Shared format manager with WAV, AIFF, FLAC, OGG, MP3 (+ CoreAudio / Media Foundation). */
juce::AudioFormatManager& formatManager();

juce::String supportedFileWildcard();

/** Loads any supported file as stereo float audio. If targetRate > 0 the audio
    is resampled (windowed-sinc) to that rate. Files longer than maxSeconds are
    rejected to keep memory sane. */
LoadedAudio loadAudioFile (const juce::File& file, double targetRate = 0.0, double maxSeconds = 20.0 * 60.0);

/** High quality sample-rate conversion. */
juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double inRate, double outRate);

/** Writes a WAV file (24-bit by default, 32 = float). */
bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate, int bits = 24);

} // namespace am
