#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace am
{

/** Alpha Monster palette: slate hardware body, oxblood modules, cream legends,
    red seven-segment displays and a phosphor CRT. */
namespace theme
{
    // Colours are variables so each product can have its own palette:
    // Mix = slate + oxblood + red displays, Master = graphite + orange + cyan.
    inline juce::Colour body        { 0xff4a5262 };
    inline juce::Colour bodyDark    { 0xff363c49 };
    inline juce::Colour bodyLight   { 0xff5f6879 };
    inline juce::Colour headerBg    { 0xff2b303b };

    inline juce::Colour panel       { 0xff7b2225 };
    inline juce::Colour panelDark   { 0xff5a171a };
    inline juce::Colour panelLight  { 0xff983036 };
    inline juce::Colour panelEdge   { 0xff230f11 };
    inline juce::Colour panelTitle  { 0xff651b1e };

    inline juce::Colour cream       { 0xffeee3c9 };
    inline juce::Colour creamDark   { 0xffc5b99f };
    inline juce::Colour ink         { 0xff1b1d22 };
    inline juce::Colour steel       { 0xff9aa1ad };

    inline juce::Colour keycap      { 0xffd3ccbb };
    inline juce::Colour keycapDark  { 0xff9c9584 };
    inline juce::Colour orange      { 0xfff0883a };
    inline juce::Colour amber       { 0xffffb648 };

    inline juce::Colour segOn       { 0xffff3b2b };
    inline juce::Colour segOff      { 0xff3b0e0b };
    inline juce::Colour displayBg   { 0xff150606 };

    inline juce::Colour crtBg       { 0xff0d2326 };
    inline juce::Colour crtTrace    { 0xff62f3df };
    inline juce::Colour crtGrid     { 0xff1d4a4c };

    inline juce::Colour ledRed      { 0xffff3d2e };
    inline juce::Colour ledGreen    { 0xff86ff6a };
    inline juce::Colour ledAmber    { 0xffffc23d };
    inline juce::Colour pointer     { 0xffd93225 };   // knob pointer
    inline juce::Colour terminalText { 0xffffc98a };

    void useMixPalette();
    void useMasterPalette();

    /** Colours used for the stems in curves and lists. */
    juce::Colour stemColour (int index);
}

namespace fonts
{
    juce::Font title (float height);   // wide techno face for the logo and headers
    juce::Font label (float height, bool bold = false); // condensed legends
    juce::Font mono (float height);    // displays and readouts
}

//==============================================================================
class AlphaLookAndFeel : public juce::LookAndFeel_V4
{
public:
    AlphaLookAndFeel();

    // knobs & sliders
    void drawRotarySlider (juce::Graphics&, int x, int y, int w, int h, float pos,
                           float startAngle, float endAngle, juce::Slider&) override;
    void drawLinearSlider (juce::Graphics&, int x, int y, int w, int h, float pos, float minPos, float maxPos,
                           juce::Slider::SliderStyle, juce::Slider&) override;
    juce::Slider::SliderLayout getSliderLayout (juce::Slider&) override;
    juce::Label* createSliderTextBox (juce::Slider&) override;

    // buttons
    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour&,
                               bool highlighted, bool down) override;
    void drawButtonText (juce::Graphics&, juce::TextButton&, bool highlighted, bool down) override;
    void drawToggleButton (juce::Graphics&, juce::ToggleButton&, bool highlighted, bool down) override;
    juce::Font getTextButtonFont (juce::TextButton&, int buttonHeight) override;

    // combo boxes & menus
    void drawComboBox (juce::Graphics&, int w, int h, bool down, int bx, int by, int bw, int bh, juce::ComboBox&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
    void positionComboBoxText (juce::ComboBox&, juce::Label&) override;
    juce::Font getPopupMenuFont() override;
    void drawPopupMenuBackground (juce::Graphics&, int w, int h) override;

    // misc
    juce::Font getLabelFont (juce::Label&) override;
    void drawProgressBar (juce::Graphics&, juce::ProgressBar&, int w, int h, double progress, const juce::String& text) override;
    void drawScrollbar (juce::Graphics&, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                        int thumbStart, int thumbSize, bool over, bool down) override;
    void drawTooltip (juce::Graphics&, const juce::String& text, int w, int h) override;
    juce::Rectangle<int> getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parentArea) override;

    juce::Typeface::Ptr getTypefaceForFont (const juce::Font&) override;

    //==========================================================================
    // shared drawing helpers (also used by the custom widgets)
    static void drawBodyBackground (juce::Graphics&, juce::Rectangle<float> area);
    static void drawModulePanel (juce::Graphics&, juce::Rectangle<float> area, const juce::String& title, float titleHeight = 22.0f);
    static void drawScrew (juce::Graphics&, juce::Point<float> centre, float radius, float angle = 0.6f);
    static void drawInsetWell (juce::Graphics&, juce::Rectangle<float> area, float corner = 4.0f);
    static void drawLegend (juce::Graphics&, const juce::String& text, juce::Rectangle<float> area,
                            juce::Justification j = juce::Justification::centred, float height = 13.0f,
                            juce::Colour c = theme::cream);
    static void drawLogo (juce::Graphics&, juce::Rectangle<float> area, const juce::String& subtitle);
};

} // namespace am
