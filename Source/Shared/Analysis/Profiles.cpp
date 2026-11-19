#include "Profiles.h"
#include <cmath>
#include <atomic>

namespace am
{

static std::atomic<bool> englishUi { false };
void setEnglish (bool e) { englishUi.store (e); }
bool isEnglish()         { return englishUi.load(); }

const std::array<float, kNumBands>& bandCentres()
{
    static const std::array<float, kNumBands> c {
        20.0f, 25.0f, 31.5f, 40.0f, 50.0f, 63.0f, 80.0f, 100.0f, 125.0f, 160.0f,
        200.0f, 250.0f, 315.0f, 400.0f, 500.0f, 630.0f, 800.0f, 1000.0f, 1250.0f, 1600.0f,
        2000.0f, 2500.0f, 3150.0f, 4000.0f, 5000.0f, 6300.0f, 8000.0f, 10000.0f, 12500.0f, 16000.0f,
        20000.0f };
    return c;
}

juce::String formatFrequency (float hz)
{
    if (hz >= 1000.0f)
    {
        const float k = hz / 1000.0f;
        return (std::abs (k - std::round (k)) < 0.05f ? juce::String ((int) std::round (k))
                                                       : juce::String (k, 1)) + "k";
    }
    return juce::String ((int) std::round (hz));
}

//==============================================================================
std::array<float, kNumBands> SignalProfile::normalisedBands() const
{
    double total = 0.0;
    for (auto b : bandDb) total += std::pow (10.0, b / 10.0);
    const float ref = total > 0.0 ? (float) (10.0 * std::log10 (total)) : 0.0f;
    std::array<float, kNumBands> out {};
    for (size_t i = 0; i < out.size(); ++i)
        out[i] = bandDb[i] - ref;
    return out;
}

static juce::Array<juce::var> toArray (const float* data, size_t n)
{
    juce::Array<juce::var> a;
    for (size_t i = 0; i < n; ++i) a.add (juce::var ((double) data[i]));
    return a;
}

template <size_t N>
static void fromArray (const juce::var& v, std::array<float, N>& out)
{
    if (auto* a = v.getArray())
        for (size_t i = 0; i < N && (int) i < a->size(); ++i)
            out[i] = (float) (double) a->getReference ((int) i);
}

juce::var SignalProfile::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("valid", valid);
    o->setProperty ("duration", durationSec);
    o->setProperty ("lufs", integratedLufs);
    o->setProperty ("maxShortTerm", maxShortTermLufs);
    o->setProperty ("maxMomentary", maxMomentaryLufs);
    o->setProperty ("lra", loudnessRange);
    o->setProperty ("truePeak", truePeakDb);
    o->setProperty ("rms", rmsDb);
    o->setProperty ("crest", crestDb);
    o->setProperty ("plr", plrDb);
    o->setProperty ("active", activeFraction);
    o->setProperty ("pan", pan);
    o->setProperty ("width", width);
    o->setProperty ("correlation", correlation);
    o->setProperty ("mono", isMono);
    o->setProperty ("regionWidth", toArray (regionWidth.data(), regionWidth.size()));
    o->setProperty ("regionEnergy", toArray (regionEnergy.data(), regionEnergy.size()));
    o->setProperty ("reverb", reverbAmount);
    o->setProperty ("rt60", rt60);
    o->setProperty ("depth", depth);
    o->setProperty ("transient", transientSharpness);
    o->setProperty ("bands", toArray (bandDb.data(), bandDb.size()));
    o->setProperty ("centroid", centroidHz);
    o->setProperty ("tilt", tiltDbPerOct);
    return juce::var (o);
}

SignalProfile SignalProfile::fromVar (const juce::var& v)
{
    SignalProfile p;
    auto f = [&] (const char* key, float def) { return v.hasProperty (key) ? (float) (double) v[key] : def; };
    p.valid = (bool) v.getProperty ("valid", false);
    p.durationSec = f ("duration", 0);
    p.integratedLufs = f ("lufs", -70);
    p.maxShortTermLufs = f ("maxShortTerm", -70);
    p.maxMomentaryLufs = f ("maxMomentary", -70);
    p.loudnessRange = f ("lra", 0);
    p.truePeakDb = f ("truePeak", -100);
    p.rmsDb = f ("rms", -100);
    p.crestDb = f ("crest", 0);
    p.plrDb = f ("plr", 0);
    p.activeFraction = f ("active", 0);
    p.pan = f ("pan", 0);
    p.width = f ("width", 0);
    p.correlation = f ("correlation", 1);
    p.isMono = (bool) v.getProperty ("mono", true);
    fromArray (v["regionWidth"], p.regionWidth);
    fromArray (v["regionEnergy"], p.regionEnergy);
    p.reverbAmount = f ("reverb", 0);
    p.rt60 = f ("rt60", 0);
    p.depth = f ("depth", 0);
    p.transientSharpness = f ("transient", 0);
    fromArray (v["bands"], p.bandDb);
    p.centroidHz = f ("centroid", 0);
    p.tiltDbPerOct = f ("tilt", 0);
    return p;
}

juce::String SignalProfile::panText() const
{
    const int v = (int) std::round (pan * 100.0f);
    if (std::abs (v) < 5) return "C";
    return (v < 0 ? "L " : "R ") + juce::String (std::abs (v));
}

juce::String SignalProfile::widthText() const
{
    if (isMono)          return "Mono";
    if (width < 0.25f)   return tr ("Stereo stretto", "Narrow stereo");
    if (width < 0.45f)   return tr ("Stereo medio", "Medium stereo");
    if (width < 0.70f)   return tr ("Stereo ampio", "Wide stereo");
    return tr ("Molto ampio", "Very wide");
}

juce::String SignalProfile::depthText() const
{
    if (depth < 0.25f) return tr ("In primo piano", "Upfront");
    if (depth < 0.45f) return tr ("Vicino", "Close");
    if (depth < 0.65f) return tr ("Medio", "Middle");
    if (depth < 0.80f) return tr ("Lontano", "Far");
    return tr ("Molto lontano", "Very far");
}

juce::String SignalProfile::reverbText() const
{
    if (reverbAmount < 0.15f) return tr ("Secco", "Dry");
    if (reverbAmount < 0.35f) return tr ("Poco riverbero", "Little reverb");
    if (reverbAmount < 0.60f) return tr ("Riverbero medio", "Medium reverb");
    return tr ("Molto riverbero", "Lots of reverb");
}

//==============================================================================
juce::String stemName (StemType t)
{
    switch (t)
    {
        case StemType::drums:  return tr ("Batteria", "Drums");
        case StemType::bass:   return tr ("Basso", "Bass");
        case StemType::other:  return tr ("Altro", "Other");
        case StemType::vocals: return tr ("Voce", "Vocals");
        case StemType::guitar: return tr ("Chitarre", "Guitars");
        case StemType::piano:  return "Piano/Keys";
        case StemType::numTypes: break;
    }
    return tr ("Altro", "Other");
}

juce::String stemKey (StemType t)
{
    switch (t)
    {
        case StemType::drums:  return "drums";
        case StemType::bass:   return "bass";
        case StemType::other:  return "other";
        case StemType::vocals: return "vocals";
        case StemType::guitar: return "guitar";
        case StemType::piano:  return "piano";
        case StemType::numTypes: break;
    }
    return "other";
}

StemType stemFromKey (const juce::String& key)
{
    for (int i = 0; i < kNumStemTypes; ++i)
        if (stemKey ((StemType) i) == key)
            return (StemType) i;
    return StemType::other;
}

StemType guessStemFromFileName (const juce::String& fileName)
{
    const auto n = fileName.toLowerCase();
    auto has = [&n] (std::initializer_list<const char*> words)
    {
        for (auto* w : words) if (n.contains (w)) return true;
        return false;
    };

    if (has ({ "vox", "vocal", "voce", "voci", "chorus", "coro", "backing", "adlib", "sing" }))                      return StemType::vocals;
    if (has ({ "bass", "basso", "808", "sub " , "subbass" }))                                                        return StemType::bass;
    if (has ({ "drum", "batt", "kick", "cassa", "snare", "rullante", "hihat", "hi-hat", "hi hat", "tom ", "perc", "clap", "cymb", "piatt", "overhead", "shaker" })) return StemType::drums;
    if (has ({ "gtr", "guit", "chitar", "acoustic", "strat", "tele" }))                                              return StemType::guitar;
    if (has ({ "piano", "keys", "rhodes", "organ", "organo", "wurli", "tastier", "epiano" }))                         return StemType::piano;
    return StemType::other;
}

float typicalCentroidHz (StemType t)
{
    switch (t)
    {
        case StemType::drums:  return 3200.0f;
        case StemType::bass:   return 250.0f;
        case StemType::vocals: return 2000.0f;
        case StemType::guitar: return 1600.0f;
        case StemType::piano:  return 1300.0f;
        case StemType::other:
        case StemType::numTypes: break;
    }
    return 1500.0f;
}

void finaliseDepth (SignalProfile& p, StemType t)
{
    const float typical = typicalCentroidHz (t);
    const float darkness = p.centroidHz > 1.0f
                               ? juce::jlimit (0.0f, 1.0f, (float) std::log2 (typical / p.centroidHz) * 0.5f + 0.25f)
                               : 0.5f;
    p.depth = juce::jlimit (0.0f, 1.0f, 0.55f * p.reverbAmount
                                            + 0.25f * (1.0f - p.transientSharpness)
                                            + 0.20f * darkness);
}

//==============================================================================
const StemProfile* ReferenceMixProfile::find (StemType t) const
{
    for (auto& s : stems)
        if (s.type == t) return &s;
    return nullptr;
}

juce::var ReferenceMixProfile::toVar() const
{
    auto* o = new juce::DynamicObject();
    o->setProperty ("source", sourceName);
    o->setProperty ("mix", mix.toVar());
    juce::Array<juce::var> arr;
    for (auto& s : stems)
    {
        auto* so = new juce::DynamicObject();
        so->setProperty ("type", stemKey (s.type));
        so->setProperty ("levelRelMix", s.levelRelMixDb);
        so->setProperty ("profile", s.profile.toVar());
        arr.add (juce::var (so));
    }
    o->setProperty ("stems", arr);
    return juce::var (o);
}

ReferenceMixProfile ReferenceMixProfile::fromVar (const juce::var& v)
{
    ReferenceMixProfile r;
    r.sourceName = v["source"].toString();
    r.mix = SignalProfile::fromVar (v["mix"]);
    if (auto* a = v["stems"].getArray())
        for (auto& s : *a)
        {
            StemProfile sp;
            sp.type = stemFromKey (s["type"].toString());
            sp.levelRelMixDb = (float) (double) s["levelRelMix"];
            sp.profile = SignalProfile::fromVar (s["profile"]);
            r.stems.push_back (sp);
        }
    return r;
}

} // namespace am
