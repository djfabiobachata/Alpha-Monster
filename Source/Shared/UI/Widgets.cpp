#include "Widgets.h"

namespace am
{

//==============================================================================
juce::Rectangle<int> ModulePanel::getContentBounds() const
{
    return getLocalBounds().reduced (8, 6).withTrimmedTop (title.isNotEmpty() ? (int) titleHeight : 0);
}

void ModulePanel::paint (juce::Graphics& g)
{
    AlphaLookAndFeel::drawModulePanel (g, getLocalBounds().toFloat(), title, titleHeight);
}

//==============================================================================
uint8_t SevenSegmentDisplay::segmentsFor (juce::juce_wchar c)
{
    // bit order: a b c d e f g
    enum { A = 1, B = 2, C = 4, D = 8, E = 16, F = 32, G = 64 };
    switch (c)
    {
        case '0': case 'O': return A | B | C | D | E | F;
        case '1': case 'I': return B | C;
        case '2': case 'Z': return A | B | G | E | D;
        case '3': return A | B | G | C | D;
        case '4': return F | G | B | C;
        case '5': case 'S': case 's': return A | F | G | C | D;
        case '6': return A | F | G | E | D | C;
        case '7': return A | B | C;
        case '8': case 'B': return A | B | C | D | E | F | G;
        case '9': return A | B | C | D | F | G;
        case '-': return G;
        case '_': return D;
        case 'A': case 'a': return A | B | C | E | F | G;
        case 'b': return C | D | E | F | G;
        case 'C': return A | D | E | F;
        case 'c': return D | E | G;
        case 'd': case 'D': return B | C | D | E | G;
        case 'E': case 'e': return A | D | E | F | G;
        case 'F': case 'f': return A | E | F | G;
        case 'G': case 'g': return A | C | D | E | F;
        case 'H': return B | C | E | F | G;
        case 'h': return C | E | F | G;
        case 'i': return C;
        case 'J': case 'j': return B | C | D | E;
        case 'L': case 'l': return D | E | F;
        case 'M': case 'm': case 'N': case 'n': return C | E | G;
        case 'o': return C | D | E | G;
        case 'P': case 'p': return A | B | E | F | G;
        case 'q': case 'Q': return A | B | C | F | G;
        case 'R': case 'r': return E | G;
        case 'T': case 't': return D | E | F | G;
        case 'U': case 'V': return B | C | D | E | F;
        case 'u': case 'v': return C | D | E;
        case 'Y': case 'y': return B | C | D | F | G;
        case '=': return D | G;
        case '\'': return F;
        default: return 0;
    }
}

void SevenSegmentDisplay::setText (const juce::String& t)
{
    if (t != text) { text = t; repaint(); }
}

void SevenSegmentDisplay::drawDigit (juce::Graphics& g, juce::Rectangle<float> cell, uint8_t segs, bool dot)
{
    const float w = cell.getWidth() * 0.60f, h = cell.getHeight();
    const float t = juce::jmax (1.6f, w * 0.19f);   // segment thickness
    const float x = cell.getX() + cell.getWidth() * 0.10f, y = cell.getY();
    const float skew = 0.10f;
    const float mid = y + h * 0.5f;

    auto seg = [&] (juce::Point<float> p1, juce::Point<float> p2, bool horizontal)
    {
        juce::Path p;
        const float ht = t * 0.5f;
        if (horizontal)
        {
            p.startNewSubPath (p1.x + ht, p1.y);
            p.lineTo (p1.x + t, p1.y - ht);
            p.lineTo (p2.x - t, p2.y - ht);
            p.lineTo (p2.x - ht, p2.y);
            p.lineTo (p2.x - t, p2.y + ht);
            p.lineTo (p1.x + t, p1.y + ht);
        }
        else
        {
            p.startNewSubPath (p1.x, p1.y + ht);
            p.lineTo (p1.x + ht, p1.y + t);
            p.lineTo (p2.x + ht, p2.y - t);
            p.lineTo (p2.x, p2.y - ht);
            p.lineTo (p2.x - ht, p2.y - t);
            p.lineTo (p1.x - ht, p1.y + t);
        }
        p.closeSubPath();
        // italic skew around the cell centre
        p.applyTransform (juce::AffineTransform::shear (-skew, 0.0f).translated (skew * mid, 0.0f));
        return p;
    };

    const juce::Point<float> tl (x, y + t * 0.5f), tr (x + w, y + t * 0.5f);
    const juce::Point<float> ml (x, mid), mr (x + w, mid);
    const juce::Point<float> bl (x, y + h - t * 0.5f), br (x + w, y + h - t * 0.5f);

    const juce::Path paths[7] = {
        seg (tl, tr, true),  // a
        seg (tr, mr, false), // b
        seg (mr, br, false), // c
        seg (bl, br, true),  // d
        seg (ml, bl, false), // e
        seg (tl, ml, false), // f
        seg (ml, mr, true)   // g
    };

    for (int i = 0; i < 7; ++i)
    {
        const bool lit = (segs >> i) & 1;
        if (lit)
        {
            g.setColour (onColour.withAlpha (0.25f));
            g.strokePath (paths[i], juce::PathStrokeType (t * 0.7f));
            g.setColour (onColour);
        }
        else
        {
            g.setColour (onColour.interpolatedWith (theme::displayBg, 0.86f));
        }
        g.fillPath (paths[i]);
    }

    const float dd = t * 1.05f;
    const float dx = x + w + t * 0.75f - skew * (h * 0.5f);
    if (dot)
    {
        g.setColour (onColour.withAlpha (0.25f));
        g.fillEllipse (dx - dd * 0.25f, y + h - dd * 1.25f, dd * 1.5f, dd * 1.5f);
    }
    g.setColour (dot ? onColour : onColour.interpolatedWith (theme::displayBg, 0.86f));
    g.fillEllipse (dx, y + h - dd, dd, dd);
}

void SevenSegmentDisplay::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
    {
        AlphaLookAndFeel::drawLegend (g, caption, r.removeFromBottom (15.0f), juce::Justification::centred, 13.0f);
        r.removeFromBottom (1.0f);
    }

    // bezel
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillRoundedRectangle (r.translated (0.0f, 1.5f), 4.0f);
    g.setColour (juce::Colour (0xff2a2c31));
    g.fillRoundedRectangle (r, 4.0f);
    auto glass = r.reduced (3.0f);
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (glass, 2.5f);

    // parse characters, attaching '.' to the previous cell
    struct Cell { uint8_t segs = 0; bool dot = false; };
    std::vector<Cell> parsed;
    for (auto c : text)
    {
        if ((c == '.' || c == ',') && ! parsed.empty() && ! parsed.back().dot) { parsed.back().dot = true; continue; }
        if (c == '.' || c == ',') { parsed.push_back ({ 0, true }); continue; }
        parsed.push_back ({ segmentsFor (c), false });
    }

    const int n = juce::jmax (cells, (int) parsed.size());
    const auto inner = glass.reduced (glass.getHeight() * 0.14f, glass.getHeight() * 0.16f);
    const float cellW = juce::jmin (inner.getWidth() / (float) n, inner.getHeight() * 0.72f);
    const float startX = inner.getRight() - cellW * (float) n;
    const int offset = n - (int) parsed.size();

    for (int i = 0; i < n; ++i)
    {
        const auto cell = juce::Rectangle<float> (startX + cellW * (float) i, inner.getY(), cellW, inner.getHeight());
        const int pi = i - offset;
        if (pi >= 0) drawDigit (g, cell, parsed[(size_t) pi].segs, parsed[(size_t) pi].dot);
        else         drawDigit (g, cell, 0, false);
    }

    // glass reflection
    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.07f), glass.getX(), glass.getY(),
                                             juce::Colours::transparentWhite, glass.getX(), glass.getCentreY(), false));
    g.fillRoundedRectangle (glass.withHeight (glass.getHeight() * 0.5f), 2.5f);
}

//==============================================================================
void TerminalScreen::addLine (const juce::String& l, int maxLines)
{
    lines.add (l);
    while (lines.size() > maxLines) lines.remove (0);
    repaint();
}

void TerminalScreen::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff2a2c31));
    g.fillRoundedRectangle (r, 5.0f);
    auto glass = r.reduced (4.0f);
    g.setColour (juce::Colour (0xff0a0806));
    g.fillRoundedRectangle (glass, 3.0f);

    g.setFont (fonts::mono (fontHeight));
    const float lh = fontHeight * 1.18f;
    auto text = glass.reduced (8.0f, 6.0f);
    const int maxVisible = juce::jmax (1, (int) (text.getHeight() / lh));
    const int first = juce::jmax (0, lines.size() - maxVisible);

    for (int i = first; i < lines.size(); ++i)
    {
        auto row = text.removeFromTop (lh);
        const auto& line = lines[i];
        juce::Colour c = textColour;
        if (line.startsWith ("!")) c = theme::ledRed;
        else if (line.startsWith (">")) c = theme::ledGreen.withAlpha (0.9f);
        g.setColour (c.withAlpha (0.18f));
        g.drawText (line.trimCharactersAtStart ("!>"), row.translated (0.0f, 0.6f).expanded (0.6f, 0.0f), juce::Justification::centredLeft, true);
        g.setColour (c);
        g.drawText (line.trimCharactersAtStart ("!>"), row, juce::Justification::centredLeft, true);
    }

    // scanlines
    g.setColour (juce::Colours::black.withAlpha (0.18f));
    for (float y = glass.getY(); y < glass.getBottom(); y += 3.0f)
        g.drawHorizontalLine ((int) y, glass.getX(), glass.getRight());
}

//==============================================================================
NeedleMeter::NeedleMeter (juce::String l, float minDb, float maxDb, float redFromDb)
    : label (std::move (l)), lo (minDb), hi (maxDb), red (redFromDb), target (minDb), current (minDb)
{
    startTimerHz (30);
}

float NeedleMeter::angleFor (float db) const
{
    const float t = juce::jlimit (0.0f, 1.0f, (db - lo) / (hi - lo));
    return juce::degreesToRadians (-48.0f + 96.0f * t);
}

void NeedleMeter::timerCallback()
{
    const float k = target > current ? 0.35f : 0.12f;   // VU-like ballistics
    const float next = current + (target - current) * k;
    if (std::abs (next - current) > 0.01f) { current = next; repaint(); }
}

void NeedleMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    g.setColour (juce::Colour (0xff25272c));
    g.fillRoundedRectangle (r, 5.0f);
    auto face = r.reduced (5.0f);
    juce::ColourGradient grad (juce::Colour (0xfff6edd5), face.getCentreX(), face.getY(), juce::Colour (0xffd4c39b), face.getCentreX(), face.getBottom(), false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (face, 3.0f);

    const juce::Point<float> pivot (face.getCentreX(), face.getBottom() + face.getHeight() * 0.18f);
    const float radius = face.getHeight() * 0.95f;

    // scale arc
    juce::Path arc;
    arc.addCentredArc (pivot.x, pivot.y, radius * 0.82f, radius * 0.82f, 0.0f, angleFor (lo), angleFor (red), true);
    g.setColour (theme::ink);
    g.strokePath (arc, juce::PathStrokeType (1.3f));
    juce::Path redArc;
    redArc.addCentredArc (pivot.x, pivot.y, radius * 0.82f, radius * 0.82f, 0.0f, angleFor (red), angleFor (hi), true);
    g.setColour (juce::Colour (0xffc8231b));
    g.strokePath (redArc, juce::PathStrokeType (4.0f));

    g.setFont (fonts::label (juce::jmax (9.0f, face.getHeight() * 0.13f), true));
    const float step = (hi - lo) > 30.0f ? 10.0f : ((hi - lo) > 12.0f ? 5.0f : 1.0f);
    for (float db = std::ceil (lo / step) * step; db <= hi + 0.01f; db += step)
    {
        const float a = angleFor (db);
        const float r1 = radius * 0.82f, r2 = radius * 0.90f;
        g.setColour (db >= red ? juce::Colour (0xffc8231b) : theme::ink);
        g.drawLine (pivot.x + std::sin (a) * r1, pivot.y - std::cos (a) * r1, pivot.x + std::sin (a) * r2, pivot.y - std::cos (a) * r2, 1.4f);
        const auto tp = juce::Point<float> (pivot.x + std::sin (a) * radius * 0.99f, pivot.y - std::cos (a) * radius * 0.99f);
        g.drawText (juce::String ((int) db), juce::Rectangle<float> (26.0f, 12.0f).withCentre (tp), juce::Justification::centred, false);
    }

    g.setColour (theme::ink.withAlpha (0.8f));
    g.setFont (fonts::label (juce::jmax (10.0f, face.getHeight() * 0.16f), true));
    g.drawText (label, face.withTrimmedTop (face.getHeight() * 0.48f).withTrimmedBottom (face.getHeight() * 0.18f), juce::Justification::centred, false);

    // needle
    const float a = angleFor (current);
    g.saveState();
    g.reduceClipRegion (face.toNearestInt());
    g.setColour (juce::Colours::black.withAlpha (0.25f));
    g.drawLine (pivot.x + 2.0f, pivot.y, pivot.x + 2.0f + std::sin (a) * radius * 0.93f, pivot.y - std::cos (a) * radius * 0.93f, 2.0f);
    g.setColour (juce::Colour (0xffb51d16));
    g.drawLine (pivot.x, pivot.y, pivot.x + std::sin (a) * radius * 0.93f, pivot.y - std::cos (a) * radius * 0.93f, 1.6f);
    g.restoreState();

    // hood shadow at the bottom
    auto hood = face.removeFromBottom (face.getHeight() * 0.18f);
    g.setColour (juce::Colour (0xff2e3036));
    g.fillRoundedRectangle (hood, 2.0f);

    g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.18f), r.getX(), r.getY(), juce::Colours::transparentWhite, r.getX(), r.getCentreY(), false));
    g.fillRoundedRectangle (r.reduced (5.0f).withHeight (r.getHeight() * 0.35f), 3.0f);
}

//==============================================================================
BarMeter::BarMeter (float minDb, float maxDb, bool inv)
    : lo (minDb), hi (maxDb), inverted (inv), level (inv ? 0.0f : minDb), peak (level)
{
    startTimerHz (30);
}

void BarMeter::setLevel (float db)
{
    if (inverted) { level = db; if (db > peak) { peak = db; peakHold = 30; } }
    else
    {
        level = juce::jmax (db, level - 1.2f);
        if (db > peak) { peak = db; peakHold = 40; }
    }
}

void BarMeter::timerCallback()
{
    if (peakHold > 0) --peakHold;
    else peak = inverted ? juce::jmax (0.0f, peak - 0.5f) : juce::jmax (lo, peak - 0.8f);
    repaint();
}

void BarMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
        AlphaLookAndFeel::drawLegend (g, caption, r.removeFromBottom (14.0f), juce::Justification::centred, 12.0f);

    g.setColour (juce::Colour (0xff1c1d21));
    g.fillRoundedRectangle (r, 3.0f);
    auto inner = r.reduced (3.0f);
    const int segs = juce::jmax (8, (int) (inner.getHeight() / 5.0f));
    const float sh = inner.getHeight() / (float) segs;

    for (int i = 0; i < segs; ++i)
    {
        const float frac = (float) (i + 1) / (float) segs;
        const float segDb = inverted ? frac * (hi - lo) : lo + frac * (hi - lo);
        bool lit = inverted ? segDb <= level + 0.01f : segDb <= level;
        const bool isPeak = inverted ? std::abs (segDb - peak) < (hi - lo) / segs : std::abs (segDb - peak) < (hi - lo) / segs;

        juce::Colour c = inverted ? theme::amber
                                  : (segDb > -0.5f ? theme::ledRed : (segDb > -9.0f ? theme::ledAmber : theme::ledGreen));
        auto seg = inverted ? juce::Rectangle<float> (inner.getX(), inner.getY() + sh * (float) i, inner.getWidth(), sh - 1.5f)
                            : juce::Rectangle<float> (inner.getX(), inner.getBottom() - sh * (float) (i + 1), inner.getWidth(), sh - 1.5f);
        g.setColour (lit || isPeak ? c : c.withAlpha (0.12f));
        g.fillRect (seg);
    }
}

//==============================================================================
void ScaleMeter::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    auto legends = r.removeFromBottom (13.0f);
    AlphaLookAndFeel::drawLegend (g, left, legends, juce::Justification::centredLeft, 12.0f, theme::creamDark);
    AlphaLookAndFeel::drawLegend (g, right, legends, juce::Justification::centredRight, 12.0f, theme::creamDark);
    if (text.isNotEmpty())
        AlphaLookAndFeel::drawLegend (g, text, legends, juce::Justification::centred, 12.0f, theme::cream);

    auto track = r.reduced (2.0f, 2.0f);
    g.setColour (theme::displayBg);
    g.fillRoundedRectangle (track, 3.0f);
    g.setColour (juce::Colours::black);
    g.drawRoundedRectangle (track, 3.0f, 1.0f);

    auto inner = track.reduced (3.0f);
    for (int i = 0; i <= 20; ++i)
    {
        const float x = inner.getX() + inner.getWidth() * (float) i / 20.0f;
        g.setColour (theme::cream.withAlpha (i % 5 == 0 ? 0.45f : 0.18f));
        g.drawVerticalLine ((int) x, inner.getY() + (i % 5 == 0 ? 0.0f : inner.getHeight() * 0.45f), inner.getBottom());
    }

    // fill
    auto fill = inner.withWidth (inner.getWidth() * value);
    g.setGradientFill (juce::ColourGradient (pointerColour.withAlpha (0.15f), fill.getX(), 0.0f, pointerColour.withAlpha (0.55f), fill.getRight(), 0.0f, false));
    g.fillRect (fill);

    if (secondary >= 0.0f)
    {
        const float sx = inner.getX() + inner.getWidth() * juce::jlimit (0.0f, 1.0f, secondary);
        g.setColour (theme::crtTrace);
        g.drawLine (sx, inner.getY() - 1.0f, sx, inner.getBottom() + 1.0f, 2.0f);
    }

    const float px = inner.getX() + inner.getWidth() * value;
    g.setColour (pointerColour);
    g.drawLine (px, inner.getY() - 2.0f, px, inner.getBottom() + 2.0f, 2.5f);
}

//==============================================================================
void LedLamp::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    const float d = juce::jmin (11.0f, r.getHeight() - 4.0f);
    auto led = r.removeFromLeft (d + 6.0f).withSizeKeepingCentre (d, d);
    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.fillEllipse (led.expanded (2.0f));
    if (on)
    {
        g.setColour (colour.withAlpha (0.3f));
        g.fillEllipse (led.expanded (4.0f));
    }
    g.setColour (on ? colour : colour.darker (2.5f));
    g.fillEllipse (led);
    g.setColour (juce::Colours::white.withAlpha (on ? 0.6f : 0.15f));
    g.fillEllipse (led.getX() + d * 0.25f, led.getY() + d * 0.15f, d * 0.3f, d * 0.25f);
    AlphaLookAndFeel::drawLegend (g, label, r, juce::Justification::centredLeft, juce::jmin (14.0f, r.getHeight() * 0.8f),
                                  on ? theme::cream : theme::creamDark);
}

//==============================================================================
void CrtScreen::paint (juce::Graphics& g)
{
    auto r = getLocalBounds().toFloat();
    if (caption.isNotEmpty())
        AlphaLookAndFeel::drawLegend (g, caption, r.removeFromBottom (15.0f), juce::Justification::centred, 13.0f);

    // cream bezel
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (r.translated (0.0f, 2.0f), 10.0f);
    juce::ColourGradient bezel (juce::Colour (0xfff2ead7), r.getX(), r.getY(), juce::Colour (0xffb8ad94), r.getX(), r.getBottom(), false);
    g.setGradientFill (bezel);
    g.fillRoundedRectangle (r, 10.0f);
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.drawRoundedRectangle (r.reduced (0.5f), 10.0f, 1.0f);

    auto glass = r.reduced (juce::jmin (10.0f, r.getWidth() * 0.05f));
    g.setColour (juce::Colour (0xff111515));
    g.fillRoundedRectangle (glass.expanded (2.0f), 14.0f);
    juce::ColourGradient tube (theme::crtBg.brighter (0.25f), glass.getCentreX(), glass.getCentreY(), theme::crtBg.darker (0.6f), glass.getX(), glass.getY(), true);
    g.setGradientFill (tube);
    g.fillRoundedRectangle (glass, 14.0f);

    {
        juce::Graphics::ScopedSaveState s (g);
        juce::Path clip;
        clip.addRoundedRectangle (glass, 14.0f);
        g.reduceClipRegion (clip);
        paintScreen (g, glass);

        g.setColour (juce::Colours::black.withAlpha (0.16f));
        for (float y = glass.getY(); y < glass.getBottom(); y += 2.0f)
            g.drawHorizontalLine ((int) y, glass.getX(), glass.getRight());

        g.setGradientFill (juce::ColourGradient (juce::Colours::white.withAlpha (0.10f), glass.getX(), glass.getY(),
                                                 juce::Colours::transparentWhite, glass.getX() + glass.getWidth() * 0.4f, glass.getY() + glass.getHeight() * 0.5f, false));
        g.fillRect (glass);
    }
}

//==============================================================================
float SpectrumScreen::xFor (float hz, juce::Rectangle<float> r) const
{
    const float t = (std::log10 (juce::jmax (hz, 20.0f)) - std::log10 (20.0f)) / (std::log10 (20000.0f) - std::log10 (20.0f));
    return r.getX() + r.getWidth() * t;
}

void SpectrumScreen::paintScreen (juce::Graphics& g, juce::Rectangle<float> glass)
{
    auto r = glass.reduced (10.0f, 8.0f);
    g.setFont (fonts::mono (10.0f));

    for (float f : { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f })
    {
        const float x = xFor (f, r);
        g.setColour (theme::crtGrid);
        g.drawVerticalLine ((int) x, r.getY(), r.getBottom());
        g.setColour (theme::crtTrace.withAlpha (0.45f));
        g.drawText (f >= 1000.0f ? juce::String ((int) (f / 1000.0f)) + "k" : juce::String ((int) f),
                    juce::Rectangle<float> (x + 2.0f, r.getBottom() - 11.0f, 30.0f, 11.0f), juce::Justification::left, false);
    }
    const float step = (hi - lo) > 40.0f ? 12.0f : 6.0f;
    for (float db = std::ceil (lo / step) * step; db <= hi; db += step)
    {
        const float y = juce::jmap (db, lo, hi, r.getBottom(), r.getY());
        g.setColour (std::abs (db) < 0.01f && showZeroLine ? theme::crtTrace.withAlpha (0.4f) : theme::crtGrid);
        g.drawHorizontalLine ((int) y, r.getX(), r.getRight());
    }

    for (auto& c : curves)
    {
        if (c.freqs.size() < 2 || c.freqs.size() != c.db.size()) continue;
        juce::Path p;
        for (size_t i = 0; i < c.freqs.size(); ++i)
        {
            const float x = xFor (c.freqs[i], r);
            const float y = juce::jmap (juce::jlimit (lo - 6.0f, hi + 6.0f, c.db[i]), lo, hi, r.getBottom(), r.getY());
            if (i == 0) p.startNewSubPath (x, y); else p.lineTo (x, y);
        }
        if (c.filled)
        {
            juce::Path f (p);
            f.lineTo (xFor (c.freqs.back(), r), r.getBottom());
            f.lineTo (xFor (c.freqs.front(), r), r.getBottom());
            f.closeSubPath();
            g.setColour (c.colour.withAlpha (0.14f));
            g.fillPath (f);
        }
        g.setColour (c.colour.withAlpha (0.25f));
        g.strokePath (p, juce::PathStrokeType (c.thickness * 3.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (c.colour);
        g.strokePath (p, juce::PathStrokeType (c.thickness, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    // legend
    float ly = r.getY() + 2.0f;
    g.setFont (fonts::mono (11.0f));
    for (auto& c : curves)
    {
        if (c.name.isEmpty()) continue;
        g.setColour (c.colour);
        g.fillRect (r.getRight() - 112.0f, ly + 4.0f, 10.0f, 3.0f);
        g.drawText (c.name, juce::Rectangle<float> (r.getRight() - 98.0f, ly, 98.0f, 12.0f), juce::Justification::left, false);
        ly += 13.0f;
    }
}

//==============================================================================
VectorScope::VectorScope()
{
    bufL.resize (capacity);
    bufR.resize (capacity);
    startTimerHz (30);
}

void VectorScope::pushSamples (const float* l, const float* r, int n) noexcept
{
    // decimate by 2 to cover more time per frame
    int s1, n1, s2, n2;
    const int count = juce::jmin (n / 2, fifo.getFreeSpace());
    if (count <= 0) return;
    fifo.prepareToWrite (count, s1, n1, s2, n2);
    for (int i = 0; i < n1; ++i) { bufL[(size_t) (s1 + i)] = l[i * 2]; bufR[(size_t) (s1 + i)] = r[i * 2]; }
    for (int i = 0; i < n2; ++i) { bufL[(size_t) (s2 + i)] = l[(n1 + i) * 2]; bufR[(size_t) (s2 + i)] = r[(n1 + i) * 2]; }
    fifo.finishedWrite (n1 + n2);
}

void VectorScope::timerCallback()
{
    const int ready = fifo.getNumReady();
    int s1, n1, s2, n2;
    fifo.prepareToRead (ready, s1, n1, s2, n2);
    points.clear();
    auto add = [this] (int idx)
    {
        const float l = bufL[(size_t) idx], r = bufR[(size_t) idx];
        points.push_back ({ (r - l) * 0.7071f, (l + r) * 0.7071f });   // side on X, mid on Y
    };
    for (int i = 0; i < n1; ++i) add (s1 + i);
    for (int i = 0; i < n2; ++i) add (s2 + i);
    fifo.finishedRead (n1 + n2);
    repaint();
}

void VectorScope::paintScreen (juce::Graphics& g, juce::Rectangle<float> glass)
{
    auto r = glass.reduced (8.0f);
    const auto c = r.getCentre();
    const float rad = juce::jmin (r.getWidth(), r.getHeight()) * 0.5f;

    g.setColour (theme::crtGrid);
    g.drawEllipse (c.x - rad, c.y - rad, rad * 2.0f, rad * 2.0f, 1.0f);
    g.drawLine (c.x - rad, c.y, c.x + rad, c.y);
    g.drawLine (c.x, c.y - rad, c.x, c.y + rad);
    g.drawLine (c.x - rad * 0.707f, c.y - rad * 0.707f, c.x + rad * 0.707f, c.y + rad * 0.707f);
    g.drawLine (c.x + rad * 0.707f, c.y - rad * 0.707f, c.x - rad * 0.707f, c.y + rad * 0.707f);

    g.setFont (fonts::mono (10.0f));
    g.setColour (theme::crtTrace.withAlpha (0.5f));
    g.drawText ("L", juce::Rectangle<float> (c.x - rad * 0.8f - 6.0f, c.y - rad * 0.8f - 6.0f, 12.0f, 12.0f), juce::Justification::centred, false);
    g.drawText ("R", juce::Rectangle<float> (c.x + rad * 0.8f - 6.0f, c.y - rad * 0.8f - 6.0f, 12.0f, 12.0f), juce::Justification::centred, false);

    g.setColour (theme::crtTrace.withAlpha (0.55f));
    for (auto& p : points)
    {
        const float x = c.x + juce::jlimit (-1.2f, 1.2f, p.x) * rad;
        const float y = c.y - juce::jlimit (-1.2f, 1.2f, p.y) * rad;
        g.fillRect (x, y, 1.5f, 1.5f);
    }

    // correlation bar
    const float corr = correlation.load();
    auto bar = juce::Rectangle<float> (r.getX() + 4.0f, r.getBottom() - 8.0f, r.getWidth() - 8.0f, 4.0f);
    g.setColour (theme::crtGrid);
    g.fillRect (bar);
    const float cx = bar.getX() + bar.getWidth() * (corr + 1.0f) * 0.5f;
    g.setColour (corr < 0.0f ? theme::ledRed : theme::crtTrace);
    g.fillRect (juce::Rectangle<float> (4.0f, 8.0f).withCentre ({ cx, bar.getCentreY() }));
}

} // namespace am
