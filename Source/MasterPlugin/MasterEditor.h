#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "MasterProcessor.h"
#include "UI/AlphaLookAndFeel.h"
#include "UI/Widgets.h"

namespace am
{

/** Fixed-size (1180 x 720) control surface, scaled to the editor size. */
class MasterSurface : public juce::Component,
                      public juce::FileDragAndDropTarget,
                      private juce::Timer
{
public:
    explicit MasterSurface (MasterProcessor&);
    ~MasterSurface() override;

    static constexpr int baseWidth = 1180, baseHeight = 720;

    void paint (juce::Graphics&) override;
    void paintOverChildren (juce::Graphics&) override;
    void resized() override;

    bool isInterestedInFileDrag (const juce::StringArray&) override { return true; }
    void filesDropped (const juce::StringArray& files, int x, int y) override;

    /** Forces all readouts to refresh (used by the snapshot tool). */
    void refreshAll();

private:
    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;

    void timerCallback() override;
    void setupKnob (juce::Slider&, const juce::String& paramId, const juce::String& tooltip, bool bipolar = false);
    void setupToggle (juce::ToggleButton&, const juce::String& text, const juce::String& paramId);
    void chooseFile (bool reference);
    void updateProfiles();
    void updateSpectrum();
    void status (const juce::String& line);

    MasterProcessor& proc;

    // header
    juce::TextButton loadRefButton { "CARICA REF" }, learnButton { "LEARN" }, analyseButton { "ANALIZZA FILE" },
                     abButton { "ASCOLTA REF" };
    TerminalScreen refName;
    juce::Slider inKnob, outKnob, mixKnob;

    // reference panel
    ModulePanel refPanel { "Reference" };
    SevenSegmentDisplay refLufs { 5 }, refTp { 4 }, refLra { 4 }, refPlr { 4 };
    TerminalScreen refInfo;

    // spectrum
    SpectrumScreen spectrum;

    // loudness panel
    ModulePanel loudPanel { "Loudness" };
    NeedleMeter needle { "LU vs REF", -12.0f, 3.0f, 0.0f };
    SevenSegmentDisplay outShort { 5 }, outInt { 5 }, targetLufs { 5 };
    juce::TextButton matchButton { "MATCH" }, resetButton { "RESET" };

    // processing modules
    ModulePanel eqPanel { "Match EQ" }, compPanel { "Glue Comp" }, widthPanel { "Stereo Width" },
                limPanel { "Limiter" }, outPanel { "Output" };
    juce::Slider eqKnob, compKnob, widthKnob, driveKnob, ceilingKnob;
    juce::ToggleButton eqOn, compOn, widthOn, limOn, bassMono;
    TerminalScreen eqMoves, compInfo, widthInfo;
    BarMeter compGr { 0.0f, 12.0f, true }, limGr { 0.0f, 12.0f, true };
    VectorScope scope;
    BarMeter outL { -48.0f, 3.0f }, outR { -48.0f, 3.0f };
    SevenSegmentDisplay driveDisplay { 5 };
    LedLamp corrLed { "FASE OK", theme::ledGreen };

    TerminalScreen statusLine;
    juce::TooltipWindow tooltips { this, 600 };

    std::vector<std::unique_ptr<SliderAttachment>> sliderAttachments;
    std::vector<std::unique_ptr<ButtonAttachment>> buttonAttachments;
    std::unique_ptr<juce::FileChooser> chooser;

    // live spectrum
    std::vector<float> fifoL, fifoR, ring;
    int ringPos = 0;
    juce::dsp::FFT fft { 13 };
    std::vector<float> fftData;
    std::array<float, kNumBands> liveBands {};
    bool wasBusy = false;
    juce::String lastErrorShown;
};

class MasterEditor : public juce::AudioProcessorEditor
{
public:
    explicit MasterEditor (MasterProcessor&);
    ~MasterEditor() override;
    void resized() override;
    void paint (juce::Graphics& g) override { g.fillAll (theme::bodyDark); }

private:
    AlphaLookAndFeel lnf;
    MasterSurface surface;
};

} // namespace am
