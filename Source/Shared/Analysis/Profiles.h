#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>

namespace am
{

/** UI language of the shared texts (the Mix app is English, Master Italian). */
void setEnglish (bool english);
bool isEnglish();
inline juce::String tr (const char* italian, const char* english) { return juce::String::fromUTF8 (isEnglish() ? english : italian); }

//==============================================================================
/** 1/3-octave analysis bands, 20 Hz .. 20 kHz (ISO 266 nominal centres). */
constexpr int kNumBands = 31;
const std::array<float, kNumBands>& bandCentres();
juce::String formatFrequency (float hz);

/** Three broad bands used for width / balance readouts. */
enum class Region { low = 0, mid = 1, high = 2 };
constexpr float kLowSplitHz = 200.0f, kHighSplitHz = 4000.0f;

//==============================================================================
/** Everything Alpha Monster measures on one stereo signal. */
struct SignalProfile
{
    bool  valid = false;
    float durationSec = 0.0f;

    // level & dynamics
    float integratedLufs = -70.0f;
    float maxShortTermLufs = -70.0f;
    float maxMomentaryLufs = -70.0f;
    float loudnessRange = 0.0f;      // LU
    float truePeakDb = -100.0f;      // dBTP
    float rmsDb = -100.0f;           // active-part RMS, dBFS
    float crestDb = 0.0f;            // true peak - active RMS
    float plrDb = 0.0f;              // true peak - integrated loudness
    float activeFraction = 0.0f;     // share of the timeline where the signal plays

    // stereo image
    float pan = 0.0f;                // -1 hard left .. +1 hard right
    float width = 0.0f;              // 0 = mono .. 1 = fully decorrelated
    float correlation = 1.0f;        // Pearson L/R correlation (active parts)
    bool  isMono = true;
    std::array<float, 3> regionWidth {};   // low / mid / high width
    std::array<float, 3> regionEnergy {};  // low / mid / high share (sum = 1)

    // space
    float reverbAmount = 0.0f;       // 0 dry .. 1 very wet (estimate)
    float rt60 = 0.0f;               // seconds (estimate, 0 if not measurable)
    float depth = 0.0f;              // 0 upfront .. 1 far back (estimate)
    float transientSharpness = 0.0f; // 0 soft .. 1 very punchy

    // tone
    std::array<float, kNumBands> bandDb {};  // mean band power, dB (absolute)
    float centroidHz = 0.0f;
    float tiltDbPerOct = 0.0f;

    /** Band levels normalised to the total power (sum of bands = 0 dB). */
    std::array<float, kNumBands> normalisedBands() const;

    juce::var toVar() const;
    static SignalProfile fromVar (const juce::var&);

    juce::String panText() const;       // "C", "L 23", "R 70"
    juce::String widthText() const;     // "Mono", "Stretto", "Medio", "Ampio", "Molto ampio"
    juce::String depthText() const;     // "In primo piano" ...
    juce::String reverbText() const;
};

//==============================================================================
/** Demucs output order is drums, bass, other, vocals (+ guitar, piano for 6s). */
enum class StemType { drums = 0, bass, other, vocals, guitar, piano, numTypes };

constexpr int kNumStemTypes = (int) StemType::numTypes;
juce::String stemName (StemType t);           // Italian UI name
juce::String stemKey (StemType t);            // stable id for files / JSON
StemType stemFromKey (const juce::String& key);
StemType guessStemFromFileName (const juce::String& name);
float typicalCentroidHz (StemType t);

/** Applies stem-specific heuristics (depth needs to know what "bright" means). */
void finaliseDepth (SignalProfile& p, StemType t);

struct StemProfile
{
    StemType type = StemType::other;
    SignalProfile profile;
    float levelRelMixDb = 0.0f;   // stem loudness minus full-mix loudness
};

/** Result of analysing a reference song for mixing. */
struct ReferenceMixProfile
{
    juce::String sourceName;
    SignalProfile mix;
    std::vector<StemProfile> stems;

    const StemProfile* find (StemType t) const;
    juce::var toVar() const;
    static ReferenceMixProfile fromVar (const juce::var&);
};

} // namespace am
