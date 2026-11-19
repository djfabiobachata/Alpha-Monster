#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "AlphaLookAndFeel.h"

namespace am
{

//==============================================================================
/** Oxblood module plate with a title strip; children go in getContentBounds(). */
class ModulePanel : public juce::Component
{
public:
    explicit ModulePanel (juce::String titleText = {}) : title (std::move (titleText)) {}
    void setTitle (const juce::String& t) { title = t; resized(); repaint(); }
    juce::Rectangle<int> getContentBounds() const;
    void paint (juce::Graphics&) override;
    float titleHeight = 24.0f;

private:
    juce::String title;
};

//==============================================================================
/** Red seven-segment readout with ghosted unlit segments. */
class SevenSegmentDisplay : public juce::Component
{
public:
    explicit SevenSegmentDisplay (int numCells = 5) : cells (numCells) {}
    void setText (const juce::String& t);
    void setNumCells (int n) { cells = n; repaint(); }
    void setColour (juce::Colour on) { onColour = on; repaint(); }
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void paint (juce::Graphics&) override;

private:
    static uint8_t segmentsFor (juce::juce_wchar c);
    void drawDigit (juce::Graphics&, juce::Rectangle<float> cell, uint8_t segs, bool dot);

    juce::String text;
    juce::String caption;
    int cells = 5;
    juce::Colour onColour = theme::segOn;
};

//==============================================================================
/** Black terminal screen with amber text (status, reports). */
class TerminalScreen : public juce::Component
{
public:
    void setLines (const juce::StringArray& l) { lines = l; repaint(); }
    void addLine (const juce::String& l, int maxLines = 200);
    void clear() { lines.clear(); repaint(); }
    void paint (juce::Graphics&) override;
    float fontHeight = 14.0f;
    juce::Colour textColour { 0xffffc98a };

private:
    juce::StringArray lines;
};

//==============================================================================
/** Analogue needle meter (cream face). */
class NeedleMeter : public juce::Component, private juce::Timer
{
public:
    NeedleMeter (juce::String label, float minDb = -20.0f, float maxDb = 3.0f, float redFromDb = 0.0f);
    void setValue (float db) { target = db; }
    void setValueImmediate (float db) { target = current = db; repaint(); }
    void setLabel (const juce::String& l) { label = l; repaint(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float angleFor (float db) const;
    juce::String label;
    float lo, hi, red;
    float target, current;
};

//==============================================================================
/** Vertical LED-bar meter (level or gain reduction). */
class BarMeter : public juce::Component, private juce::Timer
{
public:
    BarMeter (float minDb = -48.0f, float maxDb = 3.0f, bool inverted = false);
    void setLevel (float db);
    void setCaption (const juce::String& c) { caption = c; repaint(); }
    void paint (juce::Graphics&) override;

private:
    void timerCallback() override;
    float lo, hi;
    bool inverted;
    float level, peak;
    int peakHold = 0;
    juce::String caption;
};

//==============================================================================
/** Horizontal scale with a pointer (width, depth, reverb readouts). */
class ScaleMeter : public juce::Component
{
public:
    ScaleMeter (juce::String leftText, juce::String rightText) : left (std::move (leftText)), right (std::move (rightText)) {}
    void setValue (float v01, const juce::String& valueText = {}) { value = juce::jlimit (0.0f, 1.0f, v01); text = valueText; repaint(); }
    void setSecondary (float v01) { secondary = v01; repaint(); }   // e.g. "your track" marker, <0 = hidden
    void paint (juce::Graphics&) override;
    juce::Colour pointerColour = theme::amber;

private:
    juce::String left, right, text;
    float value = 0.0f, secondary = -1.0f;
};

//==============================================================================
/** Small LED lamp with a legend. */
class LedLamp : public juce::Component
{
public:
    explicit LedLamp (juce::String l, juce::Colour c = theme::ledRed) : label (std::move (l)), colour (c) {}
    void setOn (bool o) { if (on != o) { on = o; repaint(); } }
    void setLabel (const juce::String& l) { label = l; repaint(); }
    void paint (juce::Graphics&) override;

private:
    juce::String label;
    juce::Colour colour;
    bool on = false;
};

//==============================================================================
/** Cream-bezel CRT screen. Subclasses paint the phosphor content. */
class CrtScreen : public juce::Component
{
public:
    void paint (juce::Graphics&) override;
    juce::String caption;

protected:
    virtual void paintScreen (juce::Graphics&, juce::Rectangle<float> glass) = 0;
};

/** Log-frequency spectrum curves on a CRT (reference, your track, EQ...). */
class SpectrumScreen : public CrtScreen
{
public:
    struct Curve
    {
        std::vector<float> freqs, db;
        juce::Colour colour = theme::crtTrace;
        float thickness = 1.6f;
        bool filled = false;
        juce::String name;
    };

    void setCurves (std::vector<Curve> c) { curves = std::move (c); repaint(); }
    void setRange (float minDb, float maxDb) { lo = minDb; hi = maxDb; repaint(); }
    bool showZeroLine = false;

protected:
    void paintScreen (juce::Graphics&, juce::Rectangle<float> glass) override;

private:
    float xFor (float hz, juce::Rectangle<float> r) const;
    std::vector<Curve> curves;
    float lo = -60.0f, hi = 0.0f;
};

/** Goniometer / vector scope fed with L/R samples (lock-free from audio thread). */
class VectorScope : public CrtScreen, private juce::Timer
{
public:
    VectorScope();
    void pushSamples (const float* l, const float* r, int n) noexcept;  // audio thread
    void setCorrelation (float c) { correlation = c; }

protected:
    void paintScreen (juce::Graphics&, juce::Rectangle<float> glass) override;

private:
    void timerCallback() override;
    static constexpr int capacity = 4096;
    juce::AbstractFifo fifo { capacity };
    std::vector<float> bufL, bufR;
    std::vector<juce::Point<float>> points;
    std::atomic<float> correlation { 1.0f };
};

} // namespace am
