#pragma once

#include "UI/Widgets.h"
#include "Analysis/Profiles.h"

namespace am
{

/** One reference instrument: level, position, width, depth, reverb, tone. */
class StemCard : public ModulePanel
{
public:
    StemCard();

    void setStem (StemType type, const StemProfile* stem);   // nullptr = not available
    void setMixSummary (const SignalProfile& mix, const juce::String& name);
    /** Shows the result of your track for comparison (cyan markers). */
    void setYourResult (const SignalProfile* yours, float yourLevelRel);
    void clearYourResult() { setYourResult (nullptr, 0.0f); }

    std::function<void()> onListen;
    void resized() override;
    void paint (juce::Graphics&) override;

private:
    bool isMix = false;
    bool available = false;
    juce::Colour accent = theme::amber;
    SevenSegmentDisplay level { 5 };
    juce::Slider pan;
    ScaleMeter width { "MONO", "WIDE" }, depth { "FRONT", "BACK" }, reverb { "DRY", "WET" };
    LedLamp monoLed { "MONO", theme::ledAmber }, stereoLed { "STEREO", theme::ledGreen };
    TerminalScreen info;
    juce::TextButton listen { "LISTEN" };
    juce::String panText;
};

} // namespace am
