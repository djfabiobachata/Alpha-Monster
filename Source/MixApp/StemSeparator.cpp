#include "StemSeparator.h"

#if defined(_MSC_VER)
 #pragma warning (push, 0)
#elif defined(__clang__)
 #pragma clang diagnostic push
 #pragma clang diagnostic ignored "-Weverything"
#elif defined(__GNUC__)
 #pragma GCC diagnostic push
 #pragma GCC diagnostic ignored "-Wall"
 #pragma GCC diagnostic ignored "-Wextra"
 #pragma GCC diagnostic ignored "-Wshadow"
 #pragma GCC diagnostic ignored "-Wconversion"
 #pragma GCC diagnostic ignored "-Wsign-conversion"
#endif
#include <Eigen/Dense>
#include "model.hpp"
#include "tensor.hpp"
#if defined(_MSC_VER)
 #pragma warning (pop)
#elif defined(__clang__)
 #pragma clang diagnostic pop
#elif defined(__GNUC__)
 #pragma GCC diagnostic pop
#endif

#include <atomic>
#include <thread>
#include <mutex>

namespace am
{

struct StemSeparator::Impl
{
    std::unique_ptr<demucscpp::demucs_model> model;
};

StemSeparator::StemSeparator() : impl (std::make_unique<Impl>()) {}
StemSeparator::~StemSeparator() = default;

juce::String StemSeparator::defaultModelFileName()
{
    return "ggml-model-htdemucs-6s-f16.bin";
}

juce::String StemSeparator::modelDownloadUrl (const juce::String& fileName)
{
    return "https://huggingface.co/datasets/Retrobear/demucs.cpp/resolve/main/" + fileName;
}

juce::File StemSeparator::userModelFolder()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Alpha Monster").getChildFile ("Models");
}

juce::File StemSeparator::findModelFile()
{
    const auto exe = juce::File::getSpecialLocation (juce::File::currentExecutableFile);
    juce::Array<juce::File> dirs;
    dirs.add (exe.getParentDirectory().getChildFile ("Models"));                       // Windows / Linux
    dirs.add (exe.getParentDirectory().getParentDirectory().getChildFile ("Resources").getChildFile ("Models")); // macOS bundle
    dirs.add (userModelFolder());
   #if JUCE_MAC
    dirs.add (juce::File ("/Library/Application Support/Alpha Monster/Models"));
   #elif JUCE_WINDOWS
    dirs.add (juce::File::getSpecialLocation (juce::File::commonApplicationDataDirectory).getChildFile ("Alpha Monster").getChildFile ("Models"));
   #endif

    for (const auto& name : { defaultModelFileName(), juce::String ("ggml-model-htdemucs-4s-f16.bin") })
        for (auto& d : dirs)
        {
            auto f = d.getChildFile (name);
            if (f.existsAsFile() && f.getSize() > 1000000)
                return f;
        }
    return {};
}

bool StemSeparator::loadModel (const juce::File& modelFile, juce::String& error)
{
    if (! modelFile.existsAsFile())
    {
        error = "Modello IA non trovato: " + modelFile.getFullPathName();
        return false;
    }

    auto m = std::make_unique<demucscpp::demucs_model>();
    bool ok = false;
    try
    {
        // demucs.cpp prints progress on stdout; that's fine for a GUI app
        ok = demucscpp::load_demucs_model (modelFile.getFullPathName().toStdString(), m.get());
    }
    catch (const std::exception& e)
    {
        error = juce::String ("Errore nel caricamento del modello: ") + e.what();
        return false;
    }

    if (! ok)
    {
        error = "Il file del modello IA non e' valido: " + modelFile.getFileName();
        return false;
    }
    impl->model = std::move (m);
    return true;
}

bool StemSeparator::isLoaded() const noexcept  { return impl->model != nullptr; }
int StemSeparator::numSources() const noexcept { return impl->model == nullptr ? 0 : (impl->model->is_4sources ? 4 : 6); }

StemType StemSeparator::sourceType (int index) const noexcept
{
    // demucs order: drums, bass, other, vocals, guitar, piano
    return (StemType) juce::jlimit (0, kNumStemTypes - 1, index);
}

int StemSeparator::recommendedThreads()
{
    const int hw = (int) std::thread::hardware_concurrency();
    return juce::jlimit (1, 6, hw > 2 ? hw / 2 : 1);
}

std::vector<StemSeparator::Stem> StemSeparator::separate (const juce::AudioBuffer<float>& audio, int numThreads, Progress progress)
{
    std::vector<Stem> result;
    if (! isLoaded() || audio.getNumSamples() == 0)
        return result;

    const auto& model = *impl->model;
    const int nSrc = numSources();
    const int total = audio.getNumSamples();
    const float* inL = audio.getReadPointer (0);
    const float* inR = audio.getReadPointer (audio.getNumChannels() > 1 ? 1 : 0);

    // chunking: ~30 s cores with 1 s overlap on each side
    const int core = (int) (30.0 * sampleRate);
    const int overlap = (int) (1.0 * sampleRate);
    struct Chunk { int start, end, padStart, padEnd; };
    std::vector<Chunk> chunks;
    for (int s = 0; s < total; s += core)
    {
        const int e = juce::jmin (total, s + core);
        chunks.push_back ({ s, e, juce::jmax (0, s - overlap), juce::jmin (total, e + overlap) });
    }
    // merge a very short last chunk into the previous one
    if (chunks.size() > 1 && chunks.back().end - chunks.back().start < (int) (5.0 * sampleRate))
    {
        auto last = chunks.back();
        chunks.pop_back();
        chunks.back().end = last.end;
        chunks.back().padEnd = last.padEnd;
    }

    result.resize ((size_t) nSrc);
    for (int t = 0; t < nSrc; ++t)
    {
        result[(size_t) t].type = sourceType (t);
        result[(size_t) t].audio.setSize (2, total);
        result[(size_t) t].audio.clear();
    }
    juce::AudioBuffer<float> weight (1, total);
    weight.clear();

    std::atomic<int> nextChunk { 0 };
    std::atomic<bool> cancelled { false };
    std::vector<float> chunkProgress (chunks.size(), 0.0f);
    std::mutex resultMutex, progressMutex;
    std::string firstError;

    auto worker = [&]
    {
        for (;;)
        {
            const int ci = nextChunk.fetch_add (1);
            if (ci >= (int) chunks.size() || cancelled.load()) return;
            const auto& c = chunks[(size_t) ci];
            const int len = c.padEnd - c.padStart;

            Eigen::MatrixXf in (2, len);
            for (int i = 0; i < len; ++i)
            {
                in (0, i) = inL[c.padStart + i];
                in (1, i) = inR[c.padStart + i];
            }

            demucscpp::ProgressCallback cb = [&, ci] (float p, const std::string&)
            {
                std::lock_guard<std::mutex> lk (progressMutex);
                chunkProgress[(size_t) ci] = juce::jlimit (0.0f, 1.0f, p);
            };

            Eigen::Tensor3dXf out;
            try
            {
                out = demucscpp::demucs_inference (model, in, cb);
            }
            catch (const std::exception& e)
            {
                std::lock_guard<std::mutex> lk (resultMutex);
                firstError = e.what();
                cancelled.store (true);
                return;
            }

            // cross-fade weights: linear ramps inside the overlaps
            std::lock_guard<std::mutex> lk (resultMutex);
            float* w = weight.getWritePointer (0);
            for (int i = 0; i < len; ++i)
            {
                const int g = c.padStart + i;
                float wt = 1.0f;
                if (g < c.start)           wt = (float) (g - c.padStart + 1) / (float) (c.start - c.padStart + 1);
                else if (g >= c.end)       wt = (float) (c.padEnd - g) / (float) (c.padEnd - c.end + 1);
                w[g] += wt;
                for (int t = 0; t < nSrc; ++t)
                {
                    result[(size_t) t].audio.getWritePointer (0)[g] += wt * out (t, 0, i);
                    result[(size_t) t].audio.getWritePointer (1)[g] += wt * out (t, 1, i);
                }
            }
            {
                std::lock_guard<std::mutex> lk2 (progressMutex);
                chunkProgress[(size_t) ci] = 1.0f;
            }
        }
    };

    const int threads = juce::jlimit (1, (int) chunks.size(), numThreads);
    std::vector<std::thread> pool;
    for (int i = 0; i < threads; ++i)
        pool.emplace_back (worker);

    // report progress from the calling thread while workers run
    for (;;)
    {
        bool allDone = true;
        float sum = 0.0f;
        {
            std::lock_guard<std::mutex> lk (progressMutex);
            for (auto p : chunkProgress)
            {
                sum += p;
                if (p < 1.0f) allDone = false;
            }
        }
        if (cancelled.load()) allDone = true;
        const float frac = sum / (float) chunks.size();
        if (progress && ! progress (frac, "Separazione IA: " + juce::String ((int) (frac * 100.0f)) + "%"))
            cancelled.store (true);
        if (allDone) break;
        std::this_thread::sleep_for (std::chrono::milliseconds (250));
    }

    for (auto& t : pool) t.join();

    if (cancelled.load())
    {
        if (! firstError.empty() && progress)
            progress (0.0f, "Errore separazione: " + juce::String (firstError));
        return {};
    }

    const float* w = weight.getReadPointer (0);
    for (auto& s : result)
        for (int ch = 0; ch < 2; ++ch)
        {
            float* d = s.audio.getWritePointer (ch);
            for (int i = 0; i < total; ++i)
                d[i] = w[i] > 0.0f ? d[i] / w[i] : 0.0f;
        }
    return result;
}

} // namespace am
