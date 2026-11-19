#include "AudioFileUtils.h"
#include "Profiles.h"

namespace am
{

juce::AudioFormatManager& formatManager()
{
    static juce::AudioFormatManager fm;
    static bool init = [] { fm.registerBasicFormats(); return true; }();
    juce::ignoreUnused (init);
    return fm;
}

juce::String supportedFileWildcard()
{
    auto w = formatManager().getWildcardForAllFormats();
    return w.isNotEmpty() ? w : juce::String ("*.wav;*.aif;*.aiff;*.flac;*.mp3;*.ogg");
}

juce::AudioBuffer<float> resample (const juce::AudioBuffer<float>& in, double inRate, double outRate)
{
    if (std::abs (inRate - outRate) < 0.5 || in.getNumSamples() == 0)
        return in;

    const double ratio = inRate / outRate;
    const int outLen = (int) std::ceil (in.getNumSamples() / ratio);
    juce::AudioBuffer<float> out (in.getNumChannels(), outLen);

    for (int ch = 0; ch < in.getNumChannels(); ++ch)
    {
        juce::WindowedSincInterpolator interp;
        // feed with zero padding at the end so the tail is fully produced
        juce::HeapBlock<float> padded ((size_t) in.getNumSamples() + 256, true);
        std::memcpy (padded.get(), in.getReadPointer (ch), sizeof (float) * (size_t) in.getNumSamples());
        interp.process (ratio, padded.get(), out.getWritePointer (ch), outLen);
    }
    return out;
}

LoadedAudio loadAudioFile (const juce::File& file, double targetRate, double maxSeconds)
{
    LoadedAudio r;
    r.name = file.getFileNameWithoutExtension();

    std::unique_ptr<juce::AudioFormatReader> reader (formatManager().createReaderFor (file));
    if (reader == nullptr)
    {
        r.error = tr ("Formato non supportato o file illeggibile: ", "Unsupported format or unreadable file: ") + file.getFileName();
        return r;
    }

    const auto len = reader->lengthInSamples;
    if (len <= 0 || (double) len / reader->sampleRate > maxSeconds)
    {
        r.error = tr ("File vuoto o troppo lungo: ", "Empty or too long file: ") + file.getFileName();
        return r;
    }

    const int n = (int) len;
    const int srcCh = (int) reader->numChannels;
    juce::AudioBuffer<float> tmp (juce::jmax (1, srcCh), n);
    reader->read (&tmp, 0, n, 0, true, true);

    r.buffer.setSize (2, n);
    if (srcCh == 1)
    {
        r.sourceWasMono = true;
        r.buffer.copyFrom (0, 0, tmp, 0, 0, n);
        r.buffer.copyFrom (1, 0, tmp, 0, 0, n);
    }
    else
    {
        r.buffer.copyFrom (0, 0, tmp, 0, 0, n);
        r.buffer.copyFrom (1, 0, tmp, 1, 0, n);
        // fold extra channels (e.g. 5.1) into L/R
        for (int ch = 2; ch < srcCh; ++ch)
        {
            r.buffer.addFrom (0, 0, tmp, ch, 0, n, 0.5f);
            r.buffer.addFrom (1, 0, tmp, ch, 0, n, 0.5f);
        }
    }

    r.sampleRate = reader->sampleRate;
    if (targetRate > 0.0 && std::abs (targetRate - r.sampleRate) > 0.5)
    {
        r.buffer = resample (r.buffer, r.sampleRate, targetRate);
        r.sampleRate = targetRate;
    }
    return r;
}

bool writeWav (const juce::File& file, const juce::AudioBuffer<float>& audio, double sampleRate, int bits)
{
    file.deleteFile();
    file.getParentDirectory().createDirectory();
    juce::WavAudioFormat wav;
    std::unique_ptr<juce::OutputStream> os (file.createOutputStream().release());
    if (os == nullptr) return false;

    std::unique_ptr<juce::AudioFormatWriter> writer (
        wav.createWriterFor (os.get(), sampleRate, (unsigned int) audio.getNumChannels(), bits, {}, 0));
    if (writer == nullptr) return false;
    os.release(); // writer owns the stream now

    return writer->writeFromAudioSampleBuffer (audio, 0, audio.getNumSamples());
}

} // namespace am
