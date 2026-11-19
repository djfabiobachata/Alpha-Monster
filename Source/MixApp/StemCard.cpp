#include "StemCard.h"

namespace am
{

StemCard::StemCard()
{
    titleHeight = 24.0f;
    addAndMakeVisible (level);
    level.setCaption ("VOLUME dB");

    pan.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    pan.setRange (-100.0, 100.0, 1.0);
    pan.setTextBoxStyle (juce::Slider::NoTextBox, true, 0, 0);
    pan.getProperties().set ("bipolar", true);
    pan.getProperties().set ("readOnly", true);
    pan.setInterceptsMouseClicks (false, false);
    pan.setValue (0.0, juce::dontSendNotification);
    addAndMakeVisible (pan);

    for (auto* m : { &width, &depth, &reverb })
        addAndMakeVisible (m);
    addAndMakeVisible (monoLed);
    addAndMakeVisible (stereoLed);
    addAndMakeVisible (info);
    info.fontHeight = 12.0f;
    addAndMakeVisible (listen);
    listen.onClick = [this] { if (onListen) onListen(); };
    listen.setTooltip ("Ascolta questo strumento separato dal reference");
}

void StemCard::setStem (StemType type, const StemProfile* s)
{
    isMix = false;
    setTitle (stemName (type));
    level.setCaption ("VOLUME dB");
    accent = theme::stemColour ((int) type);
    available = s != nullptr && s->profile.valid;
    listen.setVisible (true);
    listen.setEnabled (available);

    if (! available)
    {
        level.setText ("----");
        pan.setValue (0.0, juce::dontSendNotification);
        panText = "--";
        width.setValue (0.0f, "--");
        depth.setValue (0.0f, "--");
        reverb.setValue (0.0f, "--");
        monoLed.setOn (false);
        stereoLed.setOn (false);
        info.setLines ({ s == nullptr ? "Non analizzato" : "Assente / silenzioso", "in questo brano" });
        repaint();
        return;
    }

    const auto& p = s->profile;
    level.setText (juce::String (s->levelRelMixDb, 1));
    pan.setValue (p.pan * 100.0f, juce::dontSendNotification);
    panText = p.panText();
    width.setValue (juce::jlimit (0.0f, 1.0f, p.width / 0.8f), juce::String (juce::roundToInt (p.width * 100.0f)) + "%");
    depth.setValue (p.depth, p.depthText());
    reverb.setValue (p.reverbAmount, juce::String (juce::roundToInt (p.reverbAmount * 100.0f)) + "%");
    monoLed.setOn (p.isMono);
    stereoLed.setOn (! p.isMono);
    info.setLines ({ "RT60   " + (p.rt60 > 0.0f ? juce::String (p.rt60, 2) + " s" : juce::String ("n/d")),
                     "TONO   " + formatFrequency (p.centroidHz) + "Hz  " + juce::String (p.tiltDbPerOct, 1) + "dB/o",
                     "CREST  " + juce::String (p.crestDb, 1) + " dB",
                     "ATTIVO " + juce::String (juce::roundToInt (p.activeFraction * 100.0f)) + "%" });
    repaint();
}

void StemCard::setMixSummary (const SignalProfile& p, const juce::String& name)
{
    isMix = true;
    setTitle ("Mix completo");
    level.setCaption ("LUFS");
    accent = theme::cream;
    available = p.valid;
    listen.setVisible (true);
    listen.setEnabled (p.valid);
    if (! p.valid)
    {
        level.setText ("----");
        info.setLines ({ "Carica un reference" });
        return;
    }
    level.setText (juce::String (p.integratedLufs, 1));
    pan.setValue (p.pan * 100.0f, juce::dontSendNotification);
    panText = p.panText();
    width.setValue (juce::jlimit (0.0f, 1.0f, p.width / 0.8f), juce::String (juce::roundToInt (p.width * 100.0f)) + "%");
    depth.setValue (p.depth, p.depthText());
    reverb.setValue (p.reverbAmount, juce::String (juce::roundToInt (p.reverbAmount * 100.0f)) + "%");
    monoLed.setOn (p.isMono);
    stereoLed.setOn (! p.isMono);
    info.setLines ({ name.substring (0, 22),
                     "TP " + juce::String (p.truePeakDb, 1) + " dBTP",
                     "LRA " + juce::String (p.loudnessRange, 1) + " LU",
                     "TILT " + juce::String (p.tiltDbPerOct, 1) + " dB/oct" });
    repaint();
}

void StemCard::setYourResult (const SignalProfile* y, float)
{
    if (y == nullptr || ! y->valid)
    {
        width.setSecondary (-1.0f);
        depth.setSecondary (-1.0f);
        reverb.setSecondary (-1.0f);
        return;
    }
    width.setSecondary (juce::jlimit (0.0f, 1.0f, y->width / 0.8f));
    depth.setSecondary (y->depth);
    reverb.setSecondary (y->reverbAmount);
}

void StemCard::resized()
{
    auto c = getContentBounds();
    auto top = c.removeFromTop (78);
    pan.setBounds (top.removeFromRight (70).withTrimmedBottom (12));
    level.setBounds (top.withTrimmedRight (4));
    c.removeFromTop (4);
    for (auto* m : { &width, &depth, &reverb })
    {
        m->setBounds (c.removeFromTop (32));
        c.removeFromTop (3);
    }
    auto leds = c.removeFromTop (20);
    monoLed.setBounds (leds.removeFromLeft (leds.getWidth() / 2));
    stereoLed.setBounds (leds);
    c.removeFromTop (3);
    listen.setBounds (c.removeFromBottom (24).reduced (16, 0));
    c.removeFromBottom (4);
    info.setBounds (c);
}

void StemCard::paint (juce::Graphics& g)
{
    ModulePanel::paint (g);
    // coloured tag in the title bar
    auto bar = getLocalBounds().toFloat().reduced (8.0f, 5.0f).withHeight (titleHeight - 8.0f);
    g.setColour (accent);
    g.fillEllipse (bar.getX() + 4.0f, bar.getCentreY() - 4.0f, 8.0f, 8.0f);

    // pan legend under the pan knob
    auto pr = pan.getBounds().toFloat();
    AlphaLookAndFeel::drawLegend (g, "PAN " + panText, pr.withY (pr.getBottom() - 2.0f).withHeight (14.0f),
                                  juce::Justification::centred, 12.0f, available ? theme::cream : theme::creamDark);
}

} // namespace am
