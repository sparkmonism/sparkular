#pragma once

#include "ExciterProcessor.h"
#include <functional>
#include <memory>

// Songcraft Audio palette: dark brown with a lime accent.
namespace sc
{
inline const juce::Colour kBg      { 0xff1b130e };
inline const juce::Colour kPanel   { 0xff231913 };
inline const juce::Colour kPanelHi { 0xff2e2219 };
inline const juce::Colour kLine    { 0xff40322a };
inline const juce::Colour kTrack   { 0xff3a2d24 };
inline const juce::Colour kText    { 0xfff0e8df };
inline const juce::Colour kMuted   { 0xff9b8b7c };
inline const juce::Colour kAccent  { 0xffb5f23d };
inline const juce::Colour kRed     { 0xffff5a4d };
} // namespace sc

// Flat vector look shared across Songcraft Audio plugins.
class SongcraftLookAndFeel : public juce::LookAndFeel_V4
{
public:
    SongcraftLookAndFeel();

    void drawRotarySlider (juce::Graphics&, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider&) override;

    void drawToggleButton (juce::Graphics&, juce::ToggleButton&,
                           bool shouldDrawButtonAsHighlighted,
                           bool shouldDrawButtonAsDown) override;

    void drawButtonBackground (juce::Graphics&, juce::Button&, const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    void drawButtonText (juce::Graphics&, juce::TextButton&,
                         bool shouldDrawButtonAsHighlighted,
                         bool shouldDrawButtonAsDown) override;

    juce::Label* createSliderTextBox (juce::Slider&) override;
    juce::Font getComboBoxFont (juce::ComboBox&) override;
};

// Small status lamp for the harmonics LED: off / lime (harmonics) / red (hard clipping).
class LedLamp : public juce::Component
{
public:
    void setState (int newState)
    {
        if (newState != state)
        {
            state = newState;
            repaint();
        }
    }

    void paint (juce::Graphics& g) override;

private:
    int state = 0;
};

// Draws the pre (input) and post (output) filter bands of the selected channel
// on a log frequency axis. Drag the band edges to set the filter frequencies.
class FrequencyWindow : public juce::Component
{
public:
    FrequencyWindow (ExciterProcessor&, std::function<int()> selectedChannel);

    void paint (juce::Graphics&) override;
    void mouseDown (const juce::MouseEvent&) override;
    void mouseDrag (const juce::MouseEvent&) override;
    void mouseUp (const juce::MouseEvent&) override;

private:
    juce::Rectangle<float> plotBounds() const;
    float freqToX (float f) const;
    float xToFreq (float x) const;
    juce::String edgeId (int edge) const;
    float edgeFreq (int edge) const;
    int edgeAt (juce::Point<float> p) const;

    ExciterProcessor& proc;
    std::function<int()> channelFn;

    int dragEdge = -1;
    juce::RangedAudioParameter* dragParam = nullptr;
};

class ExciterEditor : public juce::AudioProcessorEditor,
                      private juce::Timer
{
public:
    explicit ExciterEditor (ExciterProcessor&);
    ~ExciterEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    void timerCallback() override;

    struct Knob
    {
        juce::Slider slider;
        juce::Label label;
        std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> att;
    };

    using SliderAttachment = juce::AudioProcessorValueTreeState::SliderAttachment;
    using ButtonAttachment = juce::AudioProcessorValueTreeState::ButtonAttachment;
    using ComboAttachment  = juce::AudioProcessorValueTreeState::ComboBoxAttachment;

    void setupKnob (Knob&, const juce::String& text, bool big);
    void setupPill (juce::Button&, const juce::String& text);
    void selectChannel (int channel);
    void bindChannel (int channel);
    void copyChannel (int from, int to);
    void syncGanged();

    ExciterProcessor& proc;
    SongcraftLookAndFeel lnf;

    int selectedCh = 0;
    bool lastGanged = false;

    FrequencyWindow freqWin;
    LedLamp led;

    Knob harmK, amountK, widthK, secondK, subtleK;

    juce::ToggleButton bypassBtn, linearBtn, gangedBtn, msBtn, monitorBtn;
    juce::ToggleButton subtleBtn, secondBtn, masterBtn, adaptiveBtn;
    juce::ToggleButton ch1Btn, ch2Btn;
    juce::TextButton copy12Btn, copy21Btn;
    juce::ComboBox osBox, slopeBox, modeBox;

    // attachments (declared after the components they drive, so they go first)
    std::unique_ptr<ButtonAttachment> bypassAtt, linearAtt, gangedAtt, msAtt, monitorAtt;
    std::unique_ptr<ButtonAttachment> subtleAtt, secondAtt, masterAtt, adaptiveAtt;
    std::unique_ptr<ComboAttachment> osAtt, slopeAtt, modeAtt;

    juce::Rectangle<int> freqPanel, ctrlPanel;
    juce::Rectangle<int> osText, slopeText, modeText;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ExciterEditor)
};
