#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include "UI/AlphaLookAndFeel.h"
#include "UI/Widgets.h"

namespace am
{

//==============================================================================
/** One audio lane of the player (reference stem, your track, result track...). */
struct PlayerLane
{
    enum Section { reference = 0, original = 1, result = 2, numSections = 3 };

    juce::String name;
    Section section = reference;
    bool isMix = false;
    juce::Colour colour;
    const juce::AudioBuffer<float>* audio = nullptr;   // owned by the app, never by the player
    double sampleRate = 44100.0;
    float levelMatchGain = 1.0f;                       // used when "level match" is on

    std::vector<float> peaks;                          // max |x| per bucket, for the waveform
    double firstSoundSec = 0.0;                        // where the lane starts to make sound

    std::atomic<bool> mute { false }, solo { false };
    float currentGain = 0.0f;                          // audio thread only (click-free ramps)

    juce::String key() const { return juce::String ((int) section) + "/" + name; }
    double lengthSec() const { return audio != nullptr ? audio->getNumSamples() / sampleRate : 0.0; }
    void computePeaks (int buckets = 3000);
};
using LanePtr = std::shared_ptr<PlayerLane>;

//==============================================================================
/** Plays any number of lanes in sync, each with mute / solo, from any position.
    Lanes may have different sample rates (interpolated on the fly). */
class PlaybackEngine : public juce::AudioSource
{
public:
    /** Keeps mute / solo of lanes that exist both before and after. */
    void setLanes (std::vector<LanePtr> newLanes);
    /** Must be called before the audio buffers referenced by the lanes change. */
    void clearLanes();
    std::vector<LanePtr> getLanes() const;

    void play();
    void pause();
    void togglePlay() { if (isPlaying()) pause(); else play(); }
    bool isPlaying() const noexcept { return playing.load(); }
    void setPositionSec (double s);
    double getPositionSec() const noexcept { return (double) position.load() / deviceRate.load(); }
    double getLengthSec() const noexcept { return length.load(); }

    void setSectionEnabled (int section, bool on);
    bool isSectionEnabled (int section) const noexcept { return sectionOn[(size_t) section].load(); }
    void setLevelMatch (bool b) noexcept { levelMatch.store (b); }
    bool isLevelMatch() const noexcept { return levelMatch.load(); }
    bool isAudible (const PlayerLane&) const;

    std::function<void()> onStateChange;   // message thread, after mute/solo/section changes

    // AudioSource
    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void releaseResources() override {}
    void getNextAudioBlock (const juce::AudioSourceChannelInfo&) override;

private:
    mutable juce::SpinLock lock;
    std::vector<LanePtr> lanes;
    std::atomic<double> deviceRate { 44100.0 }, length { 0.0 };
    std::atomic<int64_t> position { 0 };
    std::atomic<bool> playing { false }, levelMatch { true };
    std::array<std::atomic<bool>, 3> sectionOn { { true, true, true } };
};

//==============================================================================
/** The player surface: transport, A/B section switches, waveform lanes. */
class PlayerComponent : public juce::Component,
                        private juce::Timer
{
public:
    explicit PlayerComponent (PlaybackEngine&);
    ~PlayerComponent() override;

    /** Re-reads the lanes from the engine. */
    void refresh();
    /** Solo one lane only, move to where it starts and play. */
    void auditionLane (const juce::String& key);
    /** Make only one section audible (A/B). */
    void showOnlySection (int section);

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

    static constexpr int headerWidth = 230;

private:
    class LaneRow;
    class SectionRow;
    class Ruler;
    class LaneList;

    void timerCallback() override;
    void updateButtons();
    juce::String formatTime (double sec) const;

    PlaybackEngine& engine;
    juce::TextButton rewindButton { "|<" }, playButton { "PLAY" }, unmuteButton { "RESET MUTES" };
    std::array<juce::TextButton, 3> sectionButtons;
    juce::ToggleButton levelMatchToggle;
    SevenSegmentDisplay timeDisplay { 7 };
    TerminalScreen hint;
    std::unique_ptr<Ruler> ruler;
    std::unique_ptr<LaneList> list;
    juce::Viewport viewport;
};

//==============================================================================
class PlayerWindow : public juce::DocumentWindow
{
public:
    explicit PlayerWindow (PlayerComponent& content);
    void closeButtonPressed() override { setVisible (false); }
};

} // namespace am
