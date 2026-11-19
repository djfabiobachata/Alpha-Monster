#include <juce_gui_extra/juce_gui_extra.h>
#include "MixMainComponent.h"

namespace am
{

/** Scales the fixed-layout surface to any window size. */
class ScaledContent : public juce::Component
{
public:
    ScaledContent()
    {
        addAndMakeVisible (main);
        setSize (MixMainComponent::defaultWidth, MixMainComponent::defaultHeight);
    }
    void resized() override
    {
        const float s = juce::jmin ((float) getWidth() / MixMainComponent::defaultWidth,
                                    (float) getHeight() / MixMainComponent::defaultHeight);
        main.setBounds (0, 0, MixMainComponent::defaultWidth, MixMainComponent::defaultHeight);
        main.setTransform (juce::AffineTransform::scale (s));
    }
    void paint (juce::Graphics& g) override { g.fillAll (theme::bodyDark); }

private:
    MixMainComponent main;
};

class MainWindow : public juce::DocumentWindow
{
public:
    explicit MainWindow (const juce::String& name)
        : DocumentWindow (name, theme::headerBg, DocumentWindow::allButtons)
    {
        setUsingNativeTitleBar (true);
        setContentOwned (new ScaledContent(), true);
        setResizable (true, true);

        const auto area = juce::Desktop::getInstance().getDisplays().getPrimaryDisplay() != nullptr
                              ? juce::Desktop::getInstance().getDisplays().getPrimaryDisplay()->userArea
                              : juce::Rectangle<int> (0, 0, 1440, 900);
        const float fit = juce::jmin (1.0f, (float) area.getWidth() * 0.95f / MixMainComponent::defaultWidth,
                                      (float) area.getHeight() * 0.92f / MixMainComponent::defaultHeight);
        setResizeLimits (MixMainComponent::defaultWidth / 2, MixMainComponent::defaultHeight / 2, 4000, 2500);
        if (auto* c = getConstrainer())
            c->setFixedAspectRatio ((double) MixMainComponent::defaultWidth / MixMainComponent::defaultHeight);
        centreWithSize ((int) (MixMainComponent::defaultWidth * fit), (int) (MixMainComponent::defaultHeight * fit));
        setVisible (true);
    }

    void closeButtonPressed() override { juce::JUCEApplication::getInstance()->systemRequestedQuit(); }
};

class MixApplication : public juce::JUCEApplication
{
public:
    const juce::String getApplicationName() override    { return "Alpha Monster Mix"; }
    const juce::String getApplicationVersion() override { return AM_VERSION_STRING; }
    bool moreThanOneInstanceAllowed() override          { return true; }

    void initialise (const juce::String&) override
    {
        juce::LookAndFeel::setDefaultLookAndFeel (&lnf);
        window = std::make_unique<MainWindow> (getApplicationName());
    }

    void shutdown() override
    {
        window.reset();
        juce::LookAndFeel::setDefaultLookAndFeel (nullptr);
    }

    void systemRequestedQuit() override { quit(); }

private:
    AlphaLookAndFeel lnf;
    std::unique_ptr<MainWindow> window;
};

} // namespace am

START_JUCE_APPLICATION (am::MixApplication)
