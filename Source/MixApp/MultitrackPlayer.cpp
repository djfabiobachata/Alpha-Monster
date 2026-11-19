#include "MultitrackPlayer.h"
#include <map>

namespace am
{

//==============================================================================
void PlayerLane::computePeaks (int buckets)
{
    peaks.assign ((size_t) buckets, 0.0f);
    firstSoundSec = 0.0;
    if (audio == nullptr || audio->getNumSamples() == 0) return;

    const int n = audio->getNumSamples();
    const float* l = audio->getReadPointer (0);
    const float* r = audio->getReadPointer (juce::jmin (1, audio->getNumChannels() - 1));
    const double perBucket = (double) n / buckets;
    const int stride = juce::jmax (1, (int) (perBucket / 256.0));   // ~256 reads per bucket is plenty

    float maxAll = 0.0f;
    for (int b = 0; b < buckets; ++b)
    {
        const int s0 = (int) (b * perBucket), s1 = juce::jmin (n, (int) ((b + 1) * perBucket));
        float m = 0.0f;
        for (int i = s0; i < s1; i += stride)
            m = juce::jmax (m, std::abs (l[i]), std::abs (r[i]));
        peaks[(size_t) b] = m;
        maxAll = juce::jmax (maxAll, m);
    }

    const float threshold = maxAll * 0.05f;   // -26 dB below the loudest point
    for (int b = 0; b < buckets; ++b)
        if (peaks[(size_t) b] > threshold && maxAll > 1.0e-5f)
        {
            firstSoundSec = juce::jmax (0.0, (b - 2) * perBucket / sampleRate);
            break;
        }
}

//==============================================================================
void PlaybackEngine::setLanes (std::vector<LanePtr> newLanes)
{
    std::map<juce::String, std::pair<bool, bool>> previous;
    for (auto& l : getLanes())
        previous[l->key()] = { l->mute.load(), l->solo.load() };

    double len = 0.0;
    for (auto& l : newLanes)
    {
        if (auto it = previous.find (l->key()); it != previous.end())
        {
            l->mute.store (it->second.first);
            l->solo.store (it->second.second);
        }
        len = juce::jmax (len, l->lengthSec());
    }

    {
        const juce::SpinLock::ScopedLockType sl (lock);
        lanes = std::move (newLanes);
    }
    length.store (len);
    if (getPositionSec() > len) setPositionSec (0.0);
}

void PlaybackEngine::clearLanes()
{
    const juce::SpinLock::ScopedLockType sl (lock);
    lanes.clear();
    playing.store (false);
}

std::vector<LanePtr> PlaybackEngine::getLanes() const
{
    const juce::SpinLock::ScopedLockType sl (lock);
    return lanes;
}

void PlaybackEngine::play()
{
    if (length.load() <= 0.0) return;
    if (getPositionSec() >= length.load() - 0.05) setPositionSec (0.0);
    playing.store (true);
}

void PlaybackEngine::pause() { playing.store (false); }

void PlaybackEngine::setPositionSec (double s)
{
    position.store ((int64_t) (juce::jlimit (0.0, juce::jmax (0.0, length.load()), s) * deviceRate.load()));
}

void PlaybackEngine::setSectionEnabled (int section, bool on)
{
    sectionOn[(size_t) juce::jlimit (0, 2, section)].store (on);
    if (onStateChange) onStateChange();
}

bool PlaybackEngine::isAudible (const PlayerLane& lane) const
{
    bool anySolo = false;
    {
        const juce::SpinLock::ScopedLockType sl (lock);
        for (auto& l : lanes) if (l->solo.load()) { anySolo = true; break; }
    }
    if (anySolo) return lane.solo.load();
    return sectionOn[(size_t) lane.section].load() && ! lane.mute.load();
}

void PlaybackEngine::prepareToPlay (int, double sampleRate)
{
    const double oldRate = deviceRate.load();
    const double sec = (double) position.load() / oldRate;
    deviceRate.store (sampleRate);
    position.store ((int64_t) (sec * sampleRate));
}

void PlaybackEngine::getNextAudioBlock (const juce::AudioSourceChannelInfo& info)
{
    info.clearActiveBufferRegion();
    if (! playing.load()) return;

    const juce::SpinLock::ScopedTryLockType sl (lock);
    if (! sl.isLocked()) return;

    const int n = info.numSamples;
    const double rate = deviceRate.load();
    const int64_t pos = position.load();
    const int outCh = info.buffer->getNumChannels();
    const bool match = levelMatch.load();

    bool anySolo = false;
    for (auto& l : lanes) if (l->solo.load()) { anySolo = true; break; }

    for (auto& lanePtr : lanes)
    {
        auto& lane = *lanePtr;
        if (lane.audio == nullptr) continue;
        const bool audible = anySolo ? lane.solo.load() : (sectionOn[(size_t) lane.section].load() && ! lane.mute.load());
        const float target = audible ? (match ? lane.levelMatchGain : 1.0f) : 0.0f;
        const float start = lane.currentGain;
        lane.currentGain = target;
        if (start == 0.0f && target == 0.0f) continue;

        const auto& src = *lane.audio;
        const int len = src.getNumSamples();
        const double ratio = lane.sampleRate / rate;

        for (int ch = 0; ch < juce::jmin (2, outCh); ++ch)
        {
            const float* in = src.getReadPointer (juce::jmin (ch, src.getNumChannels() - 1));
            float* out = info.buffer->getWritePointer (ch, info.startSample);
            for (int i = 0; i < n; ++i)
            {
                const float g = start + (target - start) * (float) i / (float) n;
                const double sp = (double) (pos + i) * ratio;
                const int idx = (int) sp;
                if (idx + 1 >= len) break;
                const float frac = (float) (sp - idx);
                out[i] += g * (in[idx] + frac * (in[idx + 1] - in[idx]));
            }
        }
    }

    // safety: never let the sum clip hard (many lanes unmuted)
    for (int ch = 0; ch < juce::jmin (2, outCh); ++ch)
    {
        float* out = info.buffer->getWritePointer (ch, info.startSample);
        for (int i = 0; i < n; ++i) out[i] = std::tanh (out[i]);
    }

    const int64_t next = pos + n;
    position.store (next);
    if ((double) next / rate >= length.load())
    {
        playing.store (false);
        position.store ((int64_t) (length.load() * rate));
    }
}

//==============================================================================
/** Time ruler above the waveforms; click / drag to seek. */
class PlayerComponent::Ruler : public juce::Component
{
public:
    explicit Ruler (PlaybackEngine& e) : engine (e) {}

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::headerBg);
        g.fillRect (r);
        auto wave = r.withTrimmedLeft ((float) headerWidth);
        const double len = engine.getLengthSec();
        if (len <= 0.0) return;

        double step = 1.0;
        for (double s : { 1.0, 2.0, 5.0, 10.0, 15.0, 30.0, 60.0 })
            if (wave.getWidth() / (len / s) >= 60.0f) { step = s; break; }
            else step = s;

        g.setFont (fonts::mono (11.0f));
        for (double t = 0.0; t <= len; t += step)
        {
            const float x = wave.getX() + (float) (t / len) * wave.getWidth();
            g.setColour (theme::cream.withAlpha (0.5f));
            g.drawVerticalLine ((int) x, r.getBottom() - 7.0f, r.getBottom());
            const int m = (int) t / 60, s = (int) t % 60;
            g.setColour (theme::creamDark);
            g.drawText (juce::String (m) + ":" + juce::String (s).paddedLeft ('0', 2), juce::Rectangle<float> (x + 3.0f, r.getY(), 50.0f, r.getHeight() - 6.0f),
                        juce::Justification::centredLeft, false);
        }
        const float px = wave.getX() + (float) (engine.getPositionSec() / len) * wave.getWidth();
        juce::Path tri;
        tri.addTriangle (px - 6.0f, r.getY() + 2.0f, px + 6.0f, r.getY() + 2.0f, px, r.getBottom() - 2.0f);
        g.setColour (theme::amber);
        g.fillPath (tri);
        AlphaLookAndFeel::drawLegend (g, "TIMELINE", r.withWidth ((float) headerWidth).reduced (10.0f, 0.0f), juce::Justification::centredLeft, 12.0f, theme::creamDark);
    }

    void mouseDown (const juce::MouseEvent& e) override { seek (e.x); }
    void mouseDrag (const juce::MouseEvent& e) override { seek (e.x); }

private:
    void seek (int x)
    {
        const float w = (float) (getWidth() - headerWidth);
        if (w <= 0.0f) return;
        engine.setPositionSec (juce::jlimit (0.0f, 1.0f, (float) (x - headerWidth) / w) * engine.getLengthSec());
        if (auto* p = getParentComponent()) p->repaint();
    }
    PlaybackEngine& engine;
};

//==============================================================================
class PlayerComponent::SectionRow : public juce::Component
{
public:
    explicit SectionRow (juce::String t, juce::Colour c) : title (std::move (t)), colour (c) {}
    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::panelTitle);
        g.fillRect (r);
        g.setColour (colour);
        g.fillRect (r.withWidth (5.0f));
        g.setColour (theme::cream);
        g.setFont (fonts::title (12.0f));
        g.drawText (title, r.withTrimmedLeft (14.0f), juce::Justification::centredLeft, false);
    }
private:
    juce::String title;
    juce::Colour colour;
};

//==============================================================================
class PlayerComponent::LaneRow : public juce::Component
{
public:
    LaneRow (PlaybackEngine& e, LanePtr l) : lane (std::move (l)), engine (e)
    {
        for (auto* b : { &muteButton, &soloButton })
        {
            b->setClickingTogglesState (true);
            addAndMakeVisible (b);
        }
        muteButton.setTooltip ("Mute this lane");
        soloButton.setTooltip ("Solo: play only the soloed lanes");
        muteButton.setToggleState (lane->mute.load(), juce::dontSendNotification);
        soloButton.setToggleState (lane->solo.load(), juce::dontSendNotification);
        muteButton.onClick = [this] { lane->mute.store (muteButton.getToggleState()); if (engine.onStateChange) engine.onStateChange(); };
        soloButton.onClick = [this] { lane->solo.store (soloButton.getToggleState()); if (engine.onStateChange) engine.onStateChange(); };
    }

    void syncButtons()
    {
        muteButton.setToggleState (lane->mute.load(), juce::dontSendNotification);
        soloButton.setToggleState (lane->solo.load(), juce::dontSendNotification);
        const bool a = engine.isAudible (*lane);
        if (a != audible) { audible = a; waveImage = {}; }
        repaint();
    }

    void resized() override
    {
        auto r = getLocalBounds().removeFromLeft (headerWidth).reduced (8, 0);
        auto buttons = r.removeFromRight (64).withSizeKeepingCentre (64, 24);
        muteButton.setBounds (buttons.removeFromLeft (31));
        soloButton.setBounds (buttons.removeFromRight (31));
        waveImage = {};
    }

    void paint (juce::Graphics& g) override
    {
        auto r = getLocalBounds().toFloat();
        g.setColour (theme::displayBg);
        g.fillRect (r);
        g.setColour (juce::Colours::black);
        g.drawHorizontalLine (getHeight() - 1, 0.0f, r.getRight());

        // header
        auto head = r.removeFromLeft ((float) headerWidth);
        g.setColour (theme::bodyDark);
        g.fillRect (head);
        g.setColour (lane->colour);
        g.fillRect (head.withWidth (5.0f));
        g.setFont (lane->isMix ? fonts::label (16.0f, true) : fonts::mono (13.5f));
        g.setColour (audible ? theme::cream : theme::creamDark.withAlpha (0.6f));
        g.drawText (lane->name, head.withTrimmedLeft (14.0f).withTrimmedRight (76.0f), juce::Justification::centredLeft, true);

        // waveform (cached)
        auto wave = r;
        const int w = (int) wave.getWidth(), h = (int) wave.getHeight();
        if (w > 0 && h > 0 && (waveImage.isNull() || waveImage.getWidth() != w || waveImage.getHeight() != h))
            renderWave (w, h);
        if (! waveImage.isNull())
            g.drawImageAt (waveImage, (int) wave.getX(), (int) wave.getY());

        // playhead
        const double len = engine.getLengthSec();
        if (len > 0.0)
        {
            const float px = wave.getX() + (float) (engine.getPositionSec() / len) * wave.getWidth();
            g.setColour (theme::amber);
            g.drawLine (px, wave.getY(), px, wave.getBottom(), 1.5f);
        }
    }

    void mouseDown (const juce::MouseEvent& e) override { seek (e.x); }
    void mouseDrag (const juce::MouseEvent& e) override { seek (e.x); }
    void mouseDoubleClick (const juce::MouseEvent& e) override
    {
        if (e.x < headerWidth)
        {
            // double click on the name: solo this lane only
            for (auto& l : engine.getLanes()) l->solo.store (l == lane ? ! lane->solo.load() : false);
            if (engine.onStateChange) engine.onStateChange();
        }
    }

    LanePtr lane;

private:
    void seek (int x)
    {
        if (x < headerWidth) return;
        const float w = (float) (getWidth() - headerWidth);
        engine.setPositionSec (juce::jlimit (0.0f, 1.0f, (float) (x - headerWidth) / w) * engine.getLengthSec());
        if (auto* p = findParentComponentOfClass<PlayerComponent>()) p->repaint();
    }

    void renderWave (int w, int h)
    {
        waveImage = juce::Image (juce::Image::ARGB, w, h, true);
        juce::Graphics g (waveImage);
        const double total = engine.getLengthSec();
        if (total <= 0.0 || lane->peaks.empty()) return;

        const float span = (float) (lane->lengthSec() / total) * (float) w;   // lanes may be shorter than the timeline
        const auto col = audible ? lane->colour : lane->colour.withSaturation (0.15f).withAlpha (0.45f);
        const float mid = h * 0.5f;
        const int nb = (int) lane->peaks.size();

        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.drawHorizontalLine ((int) mid, 0.0f, (float) w);
        g.setColour (col);
        for (int x = 0; x < (int) span; ++x)
        {
            const int b0 = (int) ((float) x / span * nb), b1 = juce::jmax (b0 + 1, (int) ((float) (x + 1) / span * nb));
            float m = 0.0f;
            for (int b = b0; b < juce::jmin (nb, b1); ++b) m = juce::jmax (m, lane->peaks[(size_t) b]);
            const float a = juce::jmin (1.0f, m) * (mid - 3.0f);
            g.drawVerticalLine (x, mid - a, mid + a + 1.0f);
        }
    }

    PlaybackEngine& engine;
    juce::TextButton muteButton { "M" }, soloButton { "S" };
    juce::Image waveImage;
    bool audible = true;
};

//==============================================================================
class PlayerComponent::LaneList : public juce::Component
{
public:
    void paint (juce::Graphics& g) override { g.fillAll (theme::bodyDark); }
    std::vector<std::unique_ptr<juce::Component>> rows;
    std::vector<LaneRow*> laneRows;
};

//==============================================================================
PlayerComponent::PlayerComponent (PlaybackEngine& e) : engine (e)
{
    setWantsKeyboardFocus (true);
    ruler = std::make_unique<Ruler> (engine);
    list = std::make_unique<LaneList>();
    addAndMakeVisible (*ruler);
    viewport.setViewedComponent (list.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    for (auto* b : { &rewindButton, &playButton, &unmuteButton })
        addAndMakeVisible (b);
    playButton.getProperties().set ("style", "big");
    rewindButton.onClick = [this] { engine.setPositionSec (0.0); repaint(); };
    playButton.onClick = [this] { engine.togglePlay(); updateButtons(); };
    unmuteButton.setTooltip ("Clear all solos and go back to the default mutes (only the mixes play)");
    unmuteButton.onClick = [this]
    {
        for (auto& l : engine.getLanes()) { l->solo.store (false); l->mute.store (! l->isMix); }
        if (engine.onStateChange) engine.onStateChange();
    };

    const char* names[] = { "REFERENCE", "ORIGINAL", "RESULT" };
    const char* tips[] = { "Listen to the reference (click with Ctrl/Cmd to add it to what is playing)",
                           "Listen to your original song",
                           "Listen to the Alpha Monster result" };
    for (int i = 0; i < 3; ++i)
    {
        auto& b = sectionButtons[(size_t) i];
        b.setButtonText (names[i]);
        b.setTooltip (tips[i]);
        b.setClickingTogglesState (false);
        b.onClick = [this, i]
        {
            if (juce::ModifierKeys::getCurrentModifiers().isCommandDown())
                engine.setSectionEnabled (i, ! engine.isSectionEnabled (i));
            else
                showOnlySection (i);
        };
        addAndMakeVisible (b);
    }

    levelMatchToggle.setButtonText ("LEVEL MATCH");
    levelMatchToggle.setTooltip ("Play every section at the same loudness, for a fair A/B comparison");
    levelMatchToggle.setToggleState (engine.isLevelMatch(), juce::dontSendNotification);
    levelMatchToggle.setColour (juce::ToggleButton::tickColourId, theme::ledGreen);
    levelMatchToggle.onClick = [this] { engine.setLevelMatch (levelMatchToggle.getToggleState()); };
    addAndMakeVisible (levelMatchToggle);

    timeDisplay.setColour (theme::amber);
    addAndMakeVisible (timeDisplay);
    addAndMakeVisible (hint);
    hint.fontHeight = 13.0f;
    hint.setLines ({ "Click a waveform or the timeline to move the playhead.  SPACE = play/pause.",
                     "M = mute, S = solo (double-click a name to solo only that lane).  Ctrl/Cmd+click a section to combine." });

    juce::Component::SafePointer<PlayerComponent> safeThis (this);
    engine.onStateChange = [safeThis]
    {
        juce::MessageManager::callAsync ([safeThis]
        {
            if (safeThis != nullptr) safeThis->updateButtons();
        });
    };

    refresh();
    startTimerHz (30);
}

PlayerComponent::~PlayerComponent()
{
    engine.onStateChange = nullptr;
}

void PlayerComponent::refresh()
{
    list->rows.clear();
    list->laneRows.clear();
    const auto lanes = engine.getLanes();

    const char* titles[] = { "REFERENCE", "YOUR ORIGINAL SONG", "RESULT - ALPHA MONSTER" };
    const juce::Colour colours[] = { theme::cream, theme::ledRed, theme::crtTrace };
    int y = 0;
    const int w = juce::jmax (400, viewport.getWidth() - viewport.getScrollBarThickness());
    for (int s = 0; s < PlayerLane::numSections; ++s)
    {
        bool any = false;
        for (auto& l : lanes) any |= (l->section == s);
        if (! any) continue;

        auto header = std::make_unique<SectionRow> (titles[s], colours[s]);
        header->setBounds (0, y, w, 24);
        y += 24;
        list->addAndMakeVisible (*header);
        list->rows.push_back (std::move (header));

        for (auto& l : lanes)
        {
            if (l->section != s) continue;
            auto row = std::make_unique<LaneRow> (engine, l);
            const int h = l->isMix ? 58 : 46;
            row->setBounds (0, y, w, h);
            y += h;
            list->addAndMakeVisible (*row);
            list->laneRows.push_back (row.get());
            list->rows.push_back (std::move (row));
        }
        y += 6;
    }
    list->setSize (w, juce::jmax (y, 10));
    updateButtons();
    repaint();
}

void PlayerComponent::auditionLane (const juce::String& key)
{
    LanePtr target;
    for (auto& l : engine.getLanes())
    {
        l->solo.store (l->key() == key);
        if (l->key() == key) target = l;
    }
    if (target == nullptr) return;
    engine.setPositionSec (target->firstSoundSec);
    engine.play();
    updateButtons();
}

void PlayerComponent::showOnlySection (int section)
{
    for (auto& l : engine.getLanes()) l->solo.store (false);
    for (int i = 0; i < 3; ++i) engine.setSectionEnabled (i, i == section);
    updateButtons();
}

void PlayerComponent::updateButtons()
{
    playButton.setButtonText (engine.isPlaying() ? "PAUSE" : "PLAY");
    playButton.setToggleState (engine.isPlaying(), juce::dontSendNotification);
    const auto lanes = engine.getLanes();
    for (int i = 0; i < 3; ++i)
    {
        bool exists = false;
        for (auto& l : lanes) exists |= (l->section == i);
        sectionButtons[(size_t) i].setEnabled (exists);
        sectionButtons[(size_t) i].setToggleState (exists && engine.isSectionEnabled (i), juce::dontSendNotification);
    }
    for (auto* r : list->laneRows) r->syncButtons();
}

juce::String PlayerComponent::formatTime (double sec) const
{
    const int m = (int) sec / 60;
    const double s = sec - m * 60;
    return juce::String (m) + ":" + juce::String (s, 1).paddedLeft ('0', 4);
}

void PlayerComponent::paint (juce::Graphics& g)
{
    AlphaLookAndFeel::drawBodyBackground (g, getLocalBounds().toFloat());
    auto top = getLocalBounds().removeFromTop (100).toFloat().reduced (6.0f);
    g.setColour (theme::headerBg);
    g.fillRoundedRectangle (top, 8.0f);
    AlphaLookAndFeel::drawLogo (g, juce::Rectangle<float> (16.0f, 14.0f, 300.0f, 40.0f), "Player");
    AlphaLookAndFeel::drawLegend (g, "LISTEN TO", juce::Rectangle<float> ((float) sectionButtons[0].getX(), 12.0f, 200.0f, 14.0f),
                                  juce::Justification::centredLeft, 13.0f);
}

void PlayerComponent::resized()
{
    auto r = getLocalBounds();
    auto top = r.removeFromTop (100).reduced (14, 12);
    top.removeFromLeft (310);
    rewindButton.setBounds (top.removeFromLeft (46).withSizeKeepingCentre (46, 36));
    top.removeFromLeft (6);
    playButton.setBounds (top.removeFromLeft (120).withSizeKeepingCentre (120, 44));
    top.removeFromLeft (10);
    timeDisplay.setBounds (top.removeFromLeft (170).withSizeKeepingCentre (170, 50));
    top.removeFromLeft (20);
    auto sections = top.removeFromLeft (390);
    sections.removeFromTop (16);
    for (auto& b : sectionButtons)
        b.setBounds (sections.removeFromLeft (130).reduced (3, 4));
    top.removeFromLeft (12);
    auto right = top;
    levelMatchToggle.setBounds (right.removeFromTop (right.getHeight() / 2).withWidth (150));
    unmuteButton.setBounds (right.withWidth (130).reduced (0, 4));

    auto bottom = r.removeFromBottom (64).reduced (8, 4);
    hint.setBounds (bottom);

    r.reduce (8, 0);
    ruler->setBounds (r.removeFromTop (24));
    viewport.setBounds (r);
    refresh();
}

bool PlayerComponent::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::spaceKey)
    {
        engine.togglePlay();
        updateButtons();
        return true;
    }
    if (k == juce::KeyPress::homeKey) { engine.setPositionSec (0.0); return true; }
    return false;
}

void PlayerComponent::timerCallback()
{
    timeDisplay.setText (formatTime (engine.getPositionSec()));
    const bool playing = engine.isPlaying();
    if (playButton.getToggleState() != playing) updateButtons();
    ruler->repaint();
    for (auto* r : list->laneRows) r->repaint();
}

//==============================================================================
PlayerWindow::PlayerWindow (PlayerComponent& content)
    : DocumentWindow ("Alpha Monster - Player", theme::headerBg, DocumentWindow::allButtons)
{
    setUsingNativeTitleBar (true);
    setContentNonOwned (&content, false);
    setResizable (true, false);
    setResizeLimits (900, 420, 4000, 3000);
    centreWithSize (1280, 820);
}

} // namespace am
