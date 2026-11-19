#include "AlphaLookAndFeel.h"
#include "BinaryData.h"

namespace am
{

void theme::useMixPalette()
{
    body = juce::Colour (0xff4a5262); bodyDark = juce::Colour (0xff363c49); bodyLight = juce::Colour (0xff5f6879); headerBg = juce::Colour (0xff2b303b);
    panel = juce::Colour (0xff7b2225); panelDark = juce::Colour (0xff5a171a); panelLight = juce::Colour (0xff983036);
    panelEdge = juce::Colour (0xff230f11); panelTitle = juce::Colour (0xff651b1e);
    cream = juce::Colour (0xffeee3c9); creamDark = juce::Colour (0xffc5b99f);
    orange = juce::Colour (0xfff0883a); amber = juce::Colour (0xffffb648);
    segOn = juce::Colour (0xffff3b2b); segOff = juce::Colour (0xff3b0e0b); displayBg = juce::Colour (0xff150606);
    pointer = juce::Colour (0xffd93225); terminalText = juce::Colour (0xffffc98a);
}

void theme::useMasterPalette()
{
    body = juce::Colour (0xff3d4046); bodyDark = juce::Colour (0xff2a2c31); bodyLight = juce::Colour (0xff50545c); headerBg = juce::Colour (0xff1e2024);
    panel = juce::Colour (0xff5b6069); panelDark = juce::Colour (0xff42464d); panelLight = juce::Colour (0xff737882);
    panelEdge = juce::Colour (0xff17181b); panelTitle = juce::Colour (0xff30333a);
    cream = juce::Colour (0xffecebe6); creamDark = juce::Colour (0xffb7b6b0);
    orange = juce::Colour (0xffff8a24); amber = juce::Colour (0xffffa94a);
    segOn = juce::Colour (0xff35e8da); segOff = juce::Colour (0xff0a2a29); displayBg = juce::Colour (0xff041211);
    pointer = juce::Colour (0xffff8a24); terminalText = juce::Colour (0xff86ece2);
}

juce::Colour theme::stemColour (int index)
{
    static const juce::Colour c[] = {
        juce::Colour (0xffff7a45),  // drums
        juce::Colour (0xff6fb7ff),  // bass
        juce::Colour (0xffc7a6ff),  // other
        juce::Colour (0xfff5e26b),  // vocals
        juce::Colour (0xff7cf29a),  // guitar
        juce::Colour (0xffff8fc7),  // piano
    };
    return c[(size_t) juce::jlimit (0, 5, index)];
}

//==============================================================================
namespace
{
    struct Typefaces
    {
        juce::Typeface::Ptr title, label, labelBold, mono;

        Typefaces()
        {
            title     = juce::Typeface::createSystemTypefaceFor (AMBinary::MichromaRegular_ttf, AMBinary::MichromaRegular_ttfSize);
            label     = juce::Typeface::createSystemTypefaceFor (AMBinary::BarlowCondensedSemiBold_ttf, AMBinary::BarlowCondensedSemiBold_ttfSize);
            labelBold = juce::Typeface::createSystemTypefaceFor (AMBinary::BarlowCondensedBold_ttf, AMBinary::BarlowCondensedBold_ttfSize);
            mono      = juce::Typeface::createSystemTypefaceFor (AMBinary::ShareTechMonoRegular_ttf, AMBinary::ShareTechMonoRegular_ttfSize);
        }
    };

    Typefaces& typefaces()
    {
        static Typefaces t;
        return t;
    }
}

juce::Font fonts::title (float h)               { return juce::Font (juce::FontOptions (typefaces().title).withHeight (h)); }
juce::Font fonts::label (float h, bool bold)    { return juce::Font (juce::FontOptions (bold ? typefaces().labelBold : typefaces().label).withHeight (h)); }
juce::Font fonts::mono (float h)                { return juce::Font (juce::FontOptions (typefaces().mono).withHeight (h)); }

//==============================================================================
AlphaLookAndFeel::AlphaLookAndFeel()
{
    using namespace theme;
    setColourScheme ({ body, bodyDark, bodyLight, steel, cream, orange, ink, cream, cream });

    setColour (juce::ResizableWindow::backgroundColourId, body);
    setColour (juce::DocumentWindow::backgroundColourId, body);
    setColour (juce::Label::textColourId, cream);
    setColour (juce::Slider::textBoxTextColourId, cream);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::rotarySliderFillColourId, orange);
    setColour (juce::TextButton::buttonColourId, keycap);
    setColour (juce::TextButton::buttonOnColourId, orange);
    setColour (juce::TextButton::textColourOffId, ink);
    setColour (juce::TextButton::textColourOnId, ink);
    setColour (juce::ToggleButton::textColourId, cream);
    setColour (juce::ToggleButton::tickColourId, ledRed);
    setColour (juce::ComboBox::backgroundColourId, displayBg);
    setColour (juce::ComboBox::textColourId, cream);
    setColour (juce::ComboBox::outlineColourId, panelEdge);
    setColour (juce::ComboBox::arrowColourId, orange);
    setColour (juce::PopupMenu::backgroundColourId, headerBg);
    setColour (juce::PopupMenu::textColourId, cream);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, panel);
    setColour (juce::PopupMenu::highlightedTextColourId, cream);
    setColour (juce::ListBox::backgroundColourId, displayBg);
    setColour (juce::ListBox::outlineColourId, panelEdge);
    setColour (juce::TextEditor::backgroundColourId, displayBg);
    setColour (juce::TextEditor::textColourId, cream);
    setColour (juce::TextEditor::outlineColourId, panelEdge);
    setColour (juce::TextEditor::focusedOutlineColourId, orange);
    setColour (juce::ScrollBar::thumbColourId, steel);
    setColour (juce::ProgressBar::backgroundColourId, displayBg);
    setColour (juce::ProgressBar::foregroundColourId, segOn);
    setColour (juce::AlertWindow::backgroundColourId, headerBg);
    setColour (juce::AlertWindow::textColourId, cream);
    setColour (juce::AlertWindow::outlineColourId, panel);
    setColour (juce::TooltipWindow::backgroundColourId, headerBg);
    setColour (juce::TooltipWindow::textColourId, cream);
    setColour (juce::TooltipWindow::outlineColourId, orange);
    setColour (juce::CaretComponent::caretColourId, orange);
    setColour (juce::TableHeaderComponent::backgroundColourId, panelTitle);
    setColour (juce::TableHeaderComponent::textColourId, cream);
    setColour (juce::TableHeaderComponent::outlineColourId, panelEdge);
}

juce::Typeface::Ptr AlphaLookAndFeel::getTypefaceForFont (const juce::Font& f)
{
    if (f.getTypefaceName() == juce::Font::getDefaultSansSerifFontName())
        return f.isBold() ? typefaces().labelBold : typefaces().label;
    if (f.getTypefaceName() == juce::Font::getDefaultMonospacedFontName())
        return typefaces().mono;
    return LookAndFeel_V4::getTypefaceForFont (f);
}

//==============================================================================
void AlphaLookAndFeel::drawBodyBackground (juce::Graphics& g, juce::Rectangle<float> r)
{
    juce::ColourGradient grad (theme::bodyLight, r.getX(), r.getY(), theme::bodyDark, r.getX(), r.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRect (r);

    // fine diagonal brushing
    g.setColour (juce::Colours::white.withAlpha (0.025f));
    for (float x = r.getX() - r.getHeight(); x < r.getRight(); x += 5.0f)
        g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
    g.setColour (juce::Colours::black.withAlpha (0.035f));
    for (float x = r.getX() - r.getHeight() + 2.5f; x < r.getRight(); x += 5.0f)
        g.drawLine (x, r.getBottom(), x + r.getHeight(), r.getY(), 1.0f);
}

void AlphaLookAndFeel::drawScrew (juce::Graphics& g, juce::Point<float> c, float rad, float angle)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.fillEllipse (c.x - rad + 0.8f, c.y - rad + 1.2f, rad * 2.0f, rad * 2.0f);
    juce::ColourGradient grad (juce::Colour (0xffd9dde3), c.x - rad, c.y - rad, juce::Colour (0xff6d7380), c.x + rad, c.y + rad, false);
    g.setGradientFill (grad);
    g.fillEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f);
    g.setColour (juce::Colour (0xff3c414b));
    const juce::Point<float> d (std::cos (angle) * rad * 0.7f, std::sin (angle) * rad * 0.7f);
    g.drawLine ({ c - d, c + d }, juce::jmax (1.0f, rad * 0.28f));
}

void AlphaLookAndFeel::drawInsetWell (juce::Graphics& g, juce::Rectangle<float> r, float corner)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, -1.0f), corner);
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.0f), corner);
    g.setColour (theme::panelDark);
    g.fillRoundedRectangle (r, corner);
}

void AlphaLookAndFeel::drawModulePanel (juce::Graphics& g, juce::Rectangle<float> r, const juce::String& title, float titleH)
{
    const float corner = 7.0f;

    // drop shadow
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.5f), corner);

    g.setColour (theme::panelEdge);
    g.fillRoundedRectangle (r, corner);

    auto inner = r.reduced (2.0f);
    juce::ColourGradient grad (theme::panelLight, inner.getX(), inner.getY(), theme::panelDark, inner.getX(), inner.getBottom(), false);
    grad.addColour (0.15, theme::panel);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (inner, corner - 1.5f);

    // subtle grain
    juce::Random rnd ((juce::int64) (r.getX() * 31 + r.getY()));
    g.setColour (juce::Colours::black.withAlpha (0.06f));
    for (int i = 0; i < (int) (inner.getWidth() * inner.getHeight() / 60.0f); ++i)
        g.fillRect (inner.getX() + rnd.nextFloat() * inner.getWidth(), inner.getY() + rnd.nextFloat() * inner.getHeight(), 1.0f, 1.0f);

    if (title.isNotEmpty())
    {
        auto bar = inner.removeFromTop (titleH).reduced (6.0f, 3.0f);
        g.setColour (theme::panelTitle);
        g.fillRoundedRectangle (bar, 3.0f);
        g.setColour (juce::Colours::black.withAlpha (0.4f));
        g.drawRoundedRectangle (bar, 3.0f, 1.0f);
        g.setColour (theme::cream);
        g.setFont (fonts::title (juce::jmin (13.0f, titleH * 0.5f)));
        g.drawText (title.toUpperCase(), bar, juce::Justification::centred, false);
    }

    // highlight on top edge
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawLine (r.getX() + corner, r.getY() + 2.5f, r.getRight() - corner, r.getY() + 2.5f, 1.0f);
}

void AlphaLookAndFeel::drawLegend (juce::Graphics& g, const juce::String& text, juce::Rectangle<float> area,
                                   juce::Justification j, float height, juce::Colour c)
{
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.setFont (fonts::label (height, true));
    g.drawText (text, area.translated (0.0f, 1.0f), j, false);
    g.setColour (c);
    g.drawText (text, area, j, false);
}

void AlphaLookAndFeel::drawLogo (juce::Graphics& g, juce::Rectangle<float> area, const juce::String& subtitle)
{
    // emblem: a horned shield with an "A" cut-out (original Alpha Monster mark)
    auto emblem = area.removeFromLeft (area.getHeight()).reduced (area.getHeight() * 0.06f);
    {
        const float w = emblem.getWidth(), h = emblem.getHeight();
        const float x = emblem.getX(), y = emblem.getY();
        juce::Path p;
        p.startNewSubPath (x + w * 0.10f, y + h * 0.02f);              // left horn tip
        p.quadraticTo (x + w * 0.16f, y + h * 0.30f, x + w * 0.30f, y + h * 0.30f);
        p.lineTo (x + w * 0.70f, y + h * 0.30f);
        p.quadraticTo (x + w * 0.84f, y + h * 0.30f, x + w * 0.90f, y + h * 0.02f); // right horn tip
        p.quadraticTo (x + w * 1.00f, y + h * 0.45f, x + w * 0.86f, y + h * 0.62f);
        p.quadraticTo (x + w * 0.70f, y + h * 0.90f, x + w * 0.50f, y + h * 0.99f);
        p.quadraticTo (x + w * 0.30f, y + h * 0.90f, x + w * 0.14f, y + h * 0.62f);
        p.quadraticTo (x + w * 0.00f, y + h * 0.45f, x + w * 0.10f, y + h * 0.02f);
        p.closeSubPath();

        juce::Path a;   // stylised "A" with fangs
        a.startNewSubPath (x + w * 0.50f, y + h * 0.38f);
        a.lineTo (x + w * 0.68f, y + h * 0.80f);
        a.lineTo (x + w * 0.58f, y + h * 0.80f);
        a.lineTo (x + w * 0.54f, y + h * 0.70f);
        a.lineTo (x + w * 0.46f, y + h * 0.70f);
        a.lineTo (x + w * 0.42f, y + h * 0.80f);
        a.lineTo (x + w * 0.32f, y + h * 0.80f);
        a.closeSubPath();
        juce::Path hole;
        hole.addTriangle (x + w * 0.50f, y + h * 0.52f, x + w * 0.525f, y + h * 0.62f, x + w * 0.475f, y + h * 0.62f);

        g.setColour (juce::Colours::black.withAlpha (0.5f));
        g.fillPath (p, juce::AffineTransform::translation (1.5f, 2.0f));
        juce::ColourGradient grad (theme::panelLight, x, y, theme::panelDark, x, y + h, false);
        g.setGradientFill (grad);
        g.fillPath (p);
        g.setColour (theme::cream);
        g.strokePath (p, juce::PathStrokeType (juce::jmax (1.2f, w * 0.035f)));
        g.fillPath (a);
        g.setColour (theme::panelDark);
        g.fillPath (hole);
        // eyes
        g.setColour (theme::amber);
        g.fillEllipse (x + w * 0.30f, y + h * 0.42f, w * 0.08f, h * 0.06f);
        g.fillEllipse (x + w * 0.62f, y + h * 0.42f, w * 0.08f, h * 0.06f);
    }

    area.removeFromLeft (area.getHeight() * 0.18f);
    const float th = area.getHeight() * 0.62f;
    auto titleArea = area.removeFromTop (th);

    juce::GlyphArrangement ga;
    ga.addLineOfText (fonts::title (th * 0.92f), "ALPHA MONSTER", titleArea.getX(), titleArea.getBottom() - th * 0.18f);
    juce::Path text;
    ga.createPath (text);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillPath (text, juce::AffineTransform::translation (2.0f, 2.5f));
    g.setColour (theme::cream);
    g.fillPath (text);
    g.setColour (theme::ink.withAlpha (0.6f));
    g.strokePath (text, juce::PathStrokeType (1.0f));

    if (subtitle.isNotEmpty())
    {
        g.setFont (fonts::label (area.getHeight() * 0.9f, true));
        g.setColour (theme::orange);
        g.drawText (subtitle.toUpperCase(), area.withTrimmedLeft (2.0f), juce::Justification::topLeft, false);
    }
}

//==============================================================================
void AlphaLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos,
                                         float startAngle, float endAngle, juce::Slider& s)
{
    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const float size = juce::jmin (bounds.getWidth(), bounds.getHeight());
    auto r = bounds.withSizeKeepingCentre (size, size);
    const auto c = r.getCentre();
    const float outer = size * 0.5f;
    const bool readOnly = ! s.isEnabled() || s.getProperties().getWithDefault ("readOnly", false);
    const bool small = size < 46.0f;

    // tick ring
    const int ticks = small ? 11 : 21;
    g.setColour (theme::cream.withAlpha (0.75f));
    for (int i = 0; i < ticks; ++i)
    {
        const float a = startAngle + (endAngle - startAngle) * (float) i / (float) (ticks - 1);
        const bool major = (i % (small ? 5 : 5)) == 0;
        const float r1 = outer * (major ? 0.80f : 0.86f), r2 = outer * 0.97f;
        g.drawLine (c.x + std::sin (a) * r1, c.y - std::cos (a) * r1,
                    c.x + std::sin (a) * r2, c.y - std::cos (a) * r2, major ? 1.6f : 1.0f);
    }

    // value arc (for bipolar knobs start from centre)
    const bool bipolar = s.getProperties().getWithDefault ("bipolar", false);
    const float valueAngle = startAngle + pos * (endAngle - startAngle);
    {
        const float arcR = outer * 0.76f;
        juce::Path arc;
        const float from = bipolar ? (startAngle + endAngle) * 0.5f : startAngle;
        arc.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, from, valueAngle, true);
        g.setColour (juce::Colours::black.withAlpha (0.45f));
        juce::Path track;
        track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
        g.strokePath (track, juce::PathStrokeType (small ? 2.0f : 3.0f));
        g.setColour (readOnly ? theme::amber : theme::orange);
        g.strokePath (arc, juce::PathStrokeType (small ? 2.0f : 3.0f));
    }

    // body: shadow, dark skirt, knurled edge, cream cap
    const float bodyR = outer * 0.66f;
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (c.x - bodyR + 1.5f, c.y - bodyR + 3.0f, bodyR * 2.0f, bodyR * 2.0f);

    juce::ColourGradient skirt (juce::Colour (0xff4c4f56), c.x, c.y - bodyR, juce::Colour (0xff17181c), c.x, c.y + bodyR, false);
    g.setGradientFill (skirt);
    g.fillEllipse (c.x - bodyR, c.y - bodyR, bodyR * 2.0f, bodyR * 2.0f);

    // knurling
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    const int knurls = small ? 18 : 30;
    for (int i = 0; i < knurls; ++i)
    {
        const float a = valueAngle + juce::MathConstants<float>::twoPi * (float) i / (float) knurls;
        g.drawLine (c.x + std::sin (a) * bodyR * 0.80f, c.y - std::cos (a) * bodyR * 0.80f,
                    c.x + std::sin (a) * bodyR * 0.98f, c.y - std::cos (a) * bodyR * 0.98f, 1.0f);
    }

    const float capR = bodyR * 0.74f;
    juce::ColourGradient cap (juce::Colour (0xfffbf5e6), c.x - capR * 0.5f, c.y - capR, theme::creamDark.darker (0.25f), c.x + capR * 0.4f, c.y + capR, true);
    g.setGradientFill (cap);
    g.fillEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.drawEllipse (c.x - capR, c.y - capR, capR * 2.0f, capR * 2.0f, 1.0f);

    // pointer
    const float p1 = capR * 0.15f, p2 = bodyR * 0.96f;
    const juce::Point<float> a1 (c.x + std::sin (valueAngle) * p1, c.y - std::cos (valueAngle) * p1);
    const juce::Point<float> a2 (c.x + std::sin (valueAngle) * p2, c.y - std::cos (valueAngle) * p2);
    g.setColour (juce::Colours::black.withAlpha (0.4f));
    g.drawLine ({ a1.translated (0.6f, 1.0f), a2.translated (0.6f, 1.0f) }, small ? 2.6f : 3.6f);
    g.setColour (theme::pointer);
    g.drawLine ({ a1, a2 }, small ? 2.2f : 3.0f);
}

void AlphaLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float, float,
                                         juce::Slider::SliderStyle style, juce::Slider& s)
{
    auto r = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h);
    const bool vertical = style == juce::Slider::LinearVertical;

    // slot
    auto slot = vertical ? r.withSizeKeepingCentre (6.0f, r.getHeight()) : r.withSizeKeepingCentre (r.getWidth(), 6.0f);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillRoundedRectangle (slot, 3.0f);
    g.setColour (juce::Colours::white.withAlpha (0.12f));
    g.drawRoundedRectangle (slot.translated (0.0f, 1.0f), 3.0f, 1.0f);

    // scale ticks
    g.setColour (theme::cream.withAlpha (0.55f));
    for (int i = 0; i <= 10; ++i)
    {
        if (vertical)
        {
            const float ty = r.getY() + r.getHeight() * (float) i / 10.0f;
            const float len = i % 5 == 0 ? 7.0f : 4.0f;
            g.drawLine (slot.getX() - 3.0f - len, ty, slot.getX() - 3.0f, ty, 1.0f);
            g.drawLine (slot.getRight() + 3.0f, ty, slot.getRight() + 3.0f + len, ty, 1.0f);
        }
        else
        {
            const float tx = r.getX() + r.getWidth() * (float) i / 10.0f;
            const float len = i % 5 == 0 ? 6.0f : 3.0f;
            g.drawLine (tx, slot.getY() - 3.0f - len, tx, slot.getY() - 3.0f, 1.0f);
        }
    }

    // cap
    juce::Rectangle<float> cap;
    if (vertical) cap = juce::Rectangle<float> (juce::jmin (r.getWidth(), 30.0f), 16.0f).withCentre ({ r.getCentreX(), pos });
    else          cap = juce::Rectangle<float> (14.0f, juce::jmin (r.getHeight(), 26.0f)).withCentre ({ pos, r.getCentreY() });

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (cap.translated (1.0f, 2.0f), 2.5f);
    juce::ColourGradient grad (juce::Colour (0xffe9e6df), cap.getX(), cap.getY(), juce::Colour (0xff8d8f95), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (theme::ink);
    if (vertical) g.drawLine (cap.getX() + 3.0f, cap.getCentreY(), cap.getRight() - 3.0f, cap.getCentreY(), 2.0f);
    else          g.drawLine (cap.getCentreX(), cap.getY() + 3.0f, cap.getCentreX(), cap.getBottom() - 3.0f, 2.0f);
    juce::ignoreUnused (s);
}

juce::Slider::SliderLayout AlphaLookAndFeel::getSliderLayout (juce::Slider& s)
{
    auto layout = LookAndFeel_V4::getSliderLayout (s);
    return layout;
}

juce::Label* AlphaLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (fonts::mono (13.0f));
    l->setColour (juce::Label::textColourId, theme::cream);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::textWhenEditingColourId, theme::cream);
    l->setColour (juce::TextEditor::backgroundColourId, theme::displayBg);
    return l;
}

//==============================================================================
void AlphaLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&,
                                             bool highlighted, bool down)
{
    auto r = b.getLocalBounds().toFloat().reduced (1.0f, 1.0f);
    const bool on = b.getToggleState();
    const juce::String style = b.getProperties().getWithDefault ("style", "keycap");

    if (style == "big")
    {
        // large action button (amber when active)
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (r.translated (0.0f, 2.5f), 5.0f);
        auto base = on ? theme::amber : theme::orange;
        if (! b.isEnabled()) base = theme::keycapDark;
        juce::ColourGradient grad (base.brighter (highlighted ? 0.35f : 0.2f), r.getX(), r.getY(), base.darker (down ? 0.5f : 0.3f), r.getX(), r.getBottom(), false);
        g.setGradientFill (grad);
        g.fillRoundedRectangle (r.translated (0.0f, down ? 1.5f : 0.0f), 5.0f);
        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawRoundedRectangle (r.translated (0.0f, down ? 1.5f : 0.0f), 5.0f, 1.2f);
        return;
    }

    // keycap: dark well + beige cap, lit cap when toggled on
    g.setColour (juce::Colours::black.withAlpha (0.6f));
    g.fillRoundedRectangle (r, 3.5f);
    auto cap = r.reduced (2.0f).withTrimmedBottom (down ? 0.0f : 2.0f).translated (0.0f, down ? 1.0f : 0.0f);

    juce::Colour base = on ? theme::orange : theme::keycap;
    if (! b.isEnabled()) base = base.withMultipliedBrightness (0.6f);
    if (highlighted && ! down) base = base.brighter (0.08f);

    juce::ColourGradient grad (base.brighter (0.15f), cap.getX(), cap.getY(), base.darker (0.25f), cap.getX(), cap.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (cap, 2.5f);
    g.setColour (juce::Colours::white.withAlpha (0.35f));
    g.drawLine (cap.getX() + 3.0f, cap.getY() + 1.0f, cap.getRight() - 3.0f, cap.getY() + 1.0f, 1.0f);

    if (on)
    {
        g.setColour (theme::amber.withAlpha (0.25f));
        g.drawRoundedRectangle (cap.expanded (1.5f), 3.5f, 2.0f);
    }
}

void AlphaLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool down)
{
    const juce::String style = b.getProperties().getWithDefault ("style", "keycap");
    auto r = b.getLocalBounds().toFloat().reduced (4.0f, 2.0f);
    if (style != "big") r = r.withTrimmedBottom (2.0f);
    if (down) r = r.translated (0.0f, 1.0f);

    g.setFont (getTextButtonFont (b, b.getHeight()));
    g.setColour (b.isEnabled() ? theme::ink : theme::ink.withAlpha (0.5f));
    g.drawFittedText (b.getButtonText(), r.toNearestInt(), juce::Justification::centred, 2, 0.8f);
}

juce::Font AlphaLookAndFeel::getTextButtonFont (juce::TextButton& b, int h)
{
    const juce::String style = b.getProperties().getWithDefault ("style", "keycap");
    return style == "big" ? fonts::title (juce::jmin (15.0f, (float) h * 0.42f))
                          : fonts::label (juce::jmin (16.0f, (float) h * 0.6f), true);
}

void AlphaLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& b, bool highlighted, bool)
{
    // LED + legend
    auto r = b.getLocalBounds().toFloat();
    const float d = juce::jmin (12.0f, r.getHeight() - 4.0f);
    auto led = r.removeFromLeft (d + 6.0f).withSizeKeepingCentre (d, d);
    const bool on = b.getToggleState();
    const auto col = b.findColour (juce::ToggleButton::tickColourId);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (led.expanded (2.0f));
    if (on)
    {
        g.setColour (col.withAlpha (0.35f));
        g.fillEllipse (led.expanded (4.0f));
    }
    juce::ColourGradient grad (on ? col.brighter (0.6f) : col.darker (2.2f), led.getCentreX(), led.getY(),
                               on ? col : col.darker (3.0f), led.getCentreX(), led.getBottom(), false);
    g.setGradientFill (grad);
    g.fillEllipse (led);

    drawLegend (g, b.getButtonText(), r.withTrimmedLeft (2.0f), juce::Justification::centredLeft,
                juce::jmin (15.0f, r.getHeight() * 0.8f), highlighted ? theme::cream.brighter() : theme::cream);
}

//==============================================================================
void AlphaLookAndFeel::drawComboBox (juce::Graphics& g, int w, int h, bool, int, int, int, int, juce::ComboBox& box)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h).reduced (0.5f);
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r, 3.0f, 1.5f);
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine (r.getX() + 3, r.getBottom() + 0.5f, r.getRight() - 3, r.getBottom() + 0.5f);

    auto arrow = r.removeFromRight ((float) h).reduced ((float) h * 0.32f);
    juce::Path p;
    p.addTriangle (arrow.getX(), arrow.getY(), arrow.getRight(), arrow.getY(), arrow.getCentreX(), arrow.getBottom());
    g.setColour (box.findColour (juce::ComboBox::arrowColourId));
    g.fillPath (p);
}

juce::Font AlphaLookAndFeel::getComboBoxFont (juce::ComboBox& b)
{
    return fonts::mono (juce::jmin (15.0f, (float) b.getHeight() * 0.62f));
}

void AlphaLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    label.setBounds (4, 1, box.getWidth() - box.getHeight() - 4, box.getHeight() - 2);
    label.setFont (getComboBoxFont (box));
}

juce::Font AlphaLookAndFeel::getPopupMenuFont()            { return fonts::label (17.0f); }

void AlphaLookAndFeel::drawPopupMenuBackground (juce::Graphics& g, int w, int h)
{
    g.fillAll (theme::headerBg);
    g.setColour (theme::panel);
    g.drawRect (0, 0, w, h, 2);
}

juce::Font AlphaLookAndFeel::getLabelFont (juce::Label& l)
{
    return l.getFont();
}

void AlphaLookAndFeel::drawProgressBar (juce::Graphics& g, juce::ProgressBar&, int w, int h, double progress, const juce::String& text)
{
    auto r = juce::Rectangle<float> (0, 0, (float) w, (float) h);
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (r, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (r.reduced (0.5f), 3.0f, 1.0f);

    auto inner = r.reduced (3.0f);
    const int segs = juce::jmax (4, (int) (inner.getWidth() / 7.0f));
    const float sw = inner.getWidth() / (float) segs;
    const bool indeterminate = progress < 0.0 || progress > 1.0;
    const double t = juce::Time::getMillisecondCounterHiRes() / 1000.0;

    for (int i = 0; i < segs; ++i)
    {
        bool lit = indeterminate ? (((int) (t * 12.0) - i) % 8 + 8) % 8 < 3 : (double) (i + 1) / segs <= progress + 1.0e-6;
        auto seg = juce::Rectangle<float> (inner.getX() + i * sw + 1.0f, inner.getY(), sw - 2.0f, inner.getHeight());
        g.setColour (lit ? theme::segOn : theme::segOff);
        g.fillRect (seg);
    }

    if (text.isNotEmpty())
    {
        g.setFont (fonts::mono (juce::jmin (14.0f, (float) h * 0.7f)));
        g.setColour (juce::Colours::black.withAlpha (0.8f));
        g.drawText (text, r.translated (1, 1), juce::Justification::centred, false);
        g.setColour (theme::cream);
        g.drawText (text, r, juce::Justification::centred, false);
    }
}

void AlphaLookAndFeel::drawScrollbar (juce::Graphics& g, juce::ScrollBar&, int x, int y, int w, int h, bool vertical,
                                      int thumbStart, int thumbSize, bool over, bool)
{
    auto track = juce::Rectangle<float> ((float) x, (float) y, (float) w, (float) h).reduced (2.0f);
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (track, 3.0f);
    juce::Rectangle<float> thumb = vertical ? juce::Rectangle<float> (track.getX(), (float) thumbStart, track.getWidth(), (float) thumbSize)
                                            : juce::Rectangle<float> ((float) thumbStart, track.getY(), (float) thumbSize, track.getHeight());
    g.setColour (over ? theme::orange : theme::steel);
    g.fillRoundedRectangle (thumb.reduced (1.0f), 3.0f);
}

void AlphaLookAndFeel::drawTooltip (juce::Graphics& g, const juce::String& text, int w, int h)
{
    g.fillAll (theme::headerBg);
    g.setColour (theme::orange);
    g.drawRect (0, 0, w, h, 1);
    juce::AttributedString s;
    s.append (text, fonts::label (16.0f), theme::cream);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, (float) w - 12.0f);
    tl.draw (g, juce::Rectangle<float> (6.0f, 4.0f, (float) w - 12.0f, (float) h - 8.0f));
}

juce::Rectangle<int> AlphaLookAndFeel::getTooltipBounds (const juce::String& text, juce::Point<int> pos, juce::Rectangle<int> parent)
{
    juce::AttributedString s;
    s.append (text, fonts::label (16.0f), theme::cream);
    juce::TextLayout tl;
    tl.createLayoutWithBalancedLineLengths (s, 320.0f);
    const int w = (int) tl.getWidth() + 16, h = (int) tl.getHeight() + 10;
    return juce::Rectangle<int> (pos.x > parent.getCentreX() ? pos.x - (w + 12) : pos.x + 24,
                                 pos.y > parent.getCentreY() ? pos.y - (h + 6) : pos.y + 6, w, h).constrainedWithin (parent);
}

} // namespace am
