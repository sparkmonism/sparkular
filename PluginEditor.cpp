#include "PluginEditor.h"
#include <algorithm>
#include <cmath>

using namespace sc;

namespace
{
juce::Font caps (float size, float kerning = 0.15f)
{
    return juce::Font (juce::FontOptions (size).withStyle ("Bold")).withExtraKerningFactor (kerning);
}

constexpr float kDialStart = juce::MathConstants<float>::pi * 1.2f;
constexpr float kDialEnd   = juce::MathConstants<float>::pi * 2.8f;

juce::String formatHz (float f)
{
    if (f >= 10000.0f) return juce::String (f / 1000.0f, 1) + "k";
    if (f >= 1000.0f)  return juce::String (f / 1000.0f, 2) + "k";
    return juce::String ((int) std::round (f));
}

const char* const kEdgeSuffix[4] = { "prehp", "prelp", "posthp", "postlp" };
} // namespace

//==============================================================================
SongcraftLookAndFeel::SongcraftLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, kBg);

    setColour (juce::Slider::textBoxTextColourId, kText);
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    setColour (juce::Slider::textBoxHighlightColourId, kAccent.withAlpha (0.35f));
    setColour (juce::CaretComponent::caretColourId, kAccent);

    setColour (juce::ComboBox::backgroundColourId, kPanelHi);
    setColour (juce::ComboBox::outlineColourId, kLine);
    setColour (juce::ComboBox::textColourId, kText);
    setColour (juce::ComboBox::arrowColourId, kMuted);

    setColour (juce::PopupMenu::backgroundColourId, kPanelHi);
    setColour (juce::PopupMenu::textColourId, kText);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, kAccent);
    setColour (juce::PopupMenu::highlightedTextColourId, kBg);
}

void SongcraftLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                             float sliderPos, float startAngle, float endAngle,
                                             juce::Slider& slider)
{
    const bool big = (bool) slider.getProperties().getWithDefault ("big", false);

    auto bounds = juce::Rectangle<float> ((float) x, (float) y, (float) width, (float) height)
                      .reduced (big ? 10.0f : 7.0f);
    const float dia = juce::jmin (bounds.getWidth(), bounds.getHeight());
    const auto area = bounds.withSizeKeepingCentre (dia, dia);
    const auto c = area.getCentre();

    const float radius = dia * 0.5f;
    const float thick  = big ? 6.0f : 4.0f;
    const float arcR   = radius - thick * 0.5f;
    const float angle  = startAngle + sliderPos * (endAngle - startAngle);

    juce::Path track;
    track.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, endAngle, true);
    g.setColour (kTrack);
    g.strokePath (track, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    if (angle - startAngle > 0.001f)
    {
        juce::Path val;
        val.addCentredArc (c.x, c.y, arcR, arcR, 0.0f, startAngle, angle, true);
        g.setColour (slider.isEnabled() ? kAccent : kMuted);
        g.strokePath (val, juce::PathStrokeType (thick, juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    const float innerR = radius - thick - (big ? 12.0f : 7.0f);
    g.setColour (kPanelHi);
    g.fillEllipse (c.x - innerR, c.y - innerR, innerR * 2.0f, innerR * 2.0f);
    g.setColour (kLine);
    g.drawEllipse (c.x - innerR, c.y - innerR, innerR * 2.0f, innerR * 2.0f, 1.0f);

    juce::Path pointer;
    pointer.addRoundedRectangle (-1.5f, -innerR * 0.9f, 3.0f, innerR * 0.5f, 1.5f);
    g.setColour (kText);
    g.fillPath (pointer, juce::AffineTransform::rotation (angle).translated (c.x, c.y));
}

void SongcraftLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                             bool highlighted, bool)
{
    const auto b  = button.getLocalBounds().toFloat();
    const bool on = button.getToggleState();

    if (button.getProperties().contains ("pill"))
    {
        const auto r = b.reduced (1.0f);
        const float corner = r.getHeight() * 0.5f;

        g.setColour (on ? kAccent : kPanelHi);
        g.fillRoundedRectangle (r, corner);

        if (highlighted && ! on)
        {
            g.setColour (juce::Colours::white.withAlpha (0.05f));
            g.fillRoundedRectangle (r, corner);
        }

        g.setColour (on ? kAccent : kLine);
        g.drawRoundedRectangle (r, corner, 1.0f);

        g.setColour (on ? kBg : kText);
        g.setFont (caps (button.getProperties().contains ("small") ? 10.5f : 12.0f, 0.12f));
        g.drawText (button.getButtonText(), r.toNearestInt(), juce::Justification::centred, false);
    }
    else
    {
        constexpr float w = 26.0f, h = 14.0f;
        const juce::Rectangle<float> sw (b.getX() + 2.0f, b.getCentreY() - h * 0.5f, w, h);

        g.setColour (on ? kAccent.withAlpha (0.9f) : kTrack);
        g.fillRoundedRectangle (sw, h * 0.5f);

        const float kd = h - 4.0f;
        const float kx = on ? sw.getRight() - kd - 2.0f : sw.getX() + 2.0f;
        g.setColour (on ? kBg : kMuted);
        g.fillEllipse (kx, sw.getY() + 2.0f, kd, kd);

        g.setColour (on ? kText : kMuted);
        g.setFont (caps (11.0f, 0.12f));
        g.drawText (button.getButtonText(), b.withTrimmedLeft (w + 10.0f).toNearestInt(),
                    juce::Justification::centredLeft, false);
    }
}

void SongcraftLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                                 const juce::Colour&, bool highlighted, bool down)
{
    const auto r = button.getLocalBounds().toFloat().reduced (1.0f);
    const float corner = r.getHeight() * 0.5f;

    g.setColour (down ? kAccent.withAlpha (0.35f) : kPanelHi);
    g.fillRoundedRectangle (r, corner);

    if (highlighted && ! down)
    {
        g.setColour (juce::Colours::white.withAlpha (0.05f));
        g.fillRoundedRectangle (r, corner);
    }

    g.setColour (kLine);
    g.drawRoundedRectangle (r, corner, 1.0f);
}

void SongcraftLookAndFeel::drawButtonText (juce::Graphics& g, juce::TextButton& button, bool, bool)
{
    g.setColour (button.isEnabled() ? kText : kMuted);
    g.setFont (caps (10.5f, 0.12f));
    g.drawText (button.getButtonText(), button.getLocalBounds(), juce::Justification::centred, false);
}

juce::Label* SongcraftLookAndFeel::createSliderTextBox (juce::Slider& s)
{
    auto* l = juce::LookAndFeel_V4::createSliderTextBox (s);
    l->setFont (juce::Font (juce::FontOptions (11.5f)));
    l->setColour (juce::Label::textColourId, kText);
    l->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
    l->setColour (juce::Label::outlineColourId, juce::Colours::transparentBlack);
    return l;
}

juce::Font SongcraftLookAndFeel::getComboBoxFont (juce::ComboBox&)
{
    return juce::Font (juce::FontOptions (12.0f));
}

//==============================================================================
void LedLamp::paint (juce::Graphics& g)
{
    const auto b = getLocalBounds().toFloat().reduced (1.0f);
    const float d = std::min (b.getWidth(), b.getHeight());
    const auto r = b.withSizeKeepingCentre (d, d);

    const juce::Colour c = state == 2 ? kRed : (state == 1 ? kAccent : kTrack);

    if (state != 0)
    {
        g.setColour (c.withAlpha (0.25f));
        g.fillEllipse (r.expanded (2.5f));
    }

    g.setColour (c);
    g.fillEllipse (r);
    g.setColour (kLine);
    g.drawEllipse (r, 1.0f);
}

//==============================================================================
FrequencyWindow::FrequencyWindow (ExciterProcessor& p, std::function<int()> selectedChannel)
    : proc (p), channelFn (std::move (selectedChannel))
{
    setMouseCursor (juce::MouseCursor::LeftRightResizeCursor);
}

juce::Rectangle<float> FrequencyWindow::plotBounds() const
{
    return getLocalBounds().toFloat().reduced (16.0f, 12.0f).withTrimmedBottom (14.0f);
}

float FrequencyWindow::freqToX (float f) const
{
    const auto plot = plotBounds();
    const float t = (std::log10 (std::max (f, 20.0f)) - std::log10 (20.0f)) / 3.0f;
    return plot.getX() + plot.getWidth() * juce::jlimit (0.0f, 1.0f, t);
}

float FrequencyWindow::xToFreq (float x) const
{
    const auto plot = plotBounds();
    const float t = juce::jlimit (0.0f, 1.0f, (x - plot.getX()) / plot.getWidth());
    return 20.0f * std::pow (10.0f, 3.0f * t);
}

juce::String FrequencyWindow::edgeId (int edge) const
{
    return ExciterProcessor::pid (channelFn() + 1, kEdgeSuffix[edge]);
}

float FrequencyWindow::edgeFreq (int edge) const
{
    return proc.apvts.getRawParameterValue (edgeId (edge))->load();
}

int FrequencyWindow::edgeAt (juce::Point<float> p) const
{
    const auto plot = plotBounds();
    const int row = p.y < plot.getCentreY() ? 0 : 1;
    const int e0 = row * 2, e1 = row * 2 + 1;

    const float d0 = std::abs (p.x - freqToX (edgeFreq (e0)));
    const float d1 = std::abs (p.x - freqToX (edgeFreq (e1)));

    return std::min (d0, d1) < 18.0f ? (d0 <= d1 ? e0 : e1) : -1;
}

void FrequencyWindow::mouseDown (const juce::MouseEvent& e)
{
    dragEdge = edgeAt (e.position);
    dragParam = nullptr;

    if (dragEdge >= 0)
    {
        dragParam = proc.apvts.getParameter (edgeId (dragEdge));
        if (dragParam != nullptr)
            dragParam->beginChangeGesture();
    }
}

void FrequencyWindow::mouseDrag (const juce::MouseEvent& e)
{
    if (dragEdge < 0 || dragParam == nullptr)
        return;

    float f = xToFreq (e.position.x);
    const int base = (dragEdge / 2) * 2;

    if (dragEdge % 2 == 0) // high-pass edge stays below its low-pass edge
        f = std::min (f, edgeFreq (base + 1) / 1.05f);
    else
        f = std::max (f, edgeFreq (base) * 1.05f);

    f = juce::jlimit (20.0f, 20000.0f, f);
    dragParam->setValueNotifyingHost (dragParam->convertTo0to1 (f));
    repaint();
}

void FrequencyWindow::mouseUp (const juce::MouseEvent&)
{
    if (dragParam != nullptr)
        dragParam->endChangeGesture();

    dragParam = nullptr;
    dragEdge = -1;
}

void FrequencyWindow::paint (juce::Graphics& g)
{
    const auto plot = plotBounds();
    const float rowH = plot.getHeight() * 0.5f - 4.0f;

    const juce::Rectangle<float> rows[2] = {
        { plot.getX(), plot.getY(), plot.getWidth(), rowH },
        { plot.getX(), plot.getBottom() - rowH, plot.getWidth(), rowH }
    };

    for (const auto& r : rows)
    {
        g.setColour (kPanelHi);
        g.fillRoundedRectangle (r, 8.0f);
    }

    // grid
    static const float gridF[] = { 50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f };
    g.setColour (kLine.withAlpha (0.8f));
    for (float f : gridF)
    {
        const int x = juce::roundToInt (freqToX (f));
        for (const auto& r : rows)
            g.drawVerticalLine (x, r.getY() + 3.0f, r.getBottom() - 3.0f);
    }

    g.setColour (kMuted);
    g.setFont (caps (9.5f, 0.1f));
    const struct { float f; const char* text; } labels[] = { { 100.0f, "100" }, { 1000.0f, "1k" }, { 10000.0f, "10k" } };
    for (const auto& l : labels)
        g.drawText (l.text, juce::Rectangle<float> (freqToX (l.f) - 20.0f, plot.getBottom() + 3.0f, 40.0f, 12.0f),
                    juce::Justification::centred, false);

    // pre band (accent) and post band (neutral)
    for (int row = 0; row < 2; ++row)
    {
        const auto& r = rows[row];
        const float x0 = freqToX (edgeFreq (row * 2));
        const float x1 = freqToX (edgeFreq (row * 2 + 1));
        const juce::Rectangle<float> band (x0, r.getY(), std::max (3.0f, x1 - x0), r.getHeight());
        const juce::Colour col = row == 0 ? kAccent : kText;

        g.setColour (col.withAlpha (row == 0 ? 0.20f : 0.10f));
        g.fillRoundedRectangle (band, 6.0f);
        g.setColour (col.withAlpha (row == 0 ? 0.9f : 0.55f));
        g.drawRoundedRectangle (band.reduced (0.5f), 6.0f, 1.2f);

        // grab handles
        g.setColour (col);
        g.fillRoundedRectangle (x0 - 1.5f, r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f, 1.5f);
        g.fillRoundedRectangle (x1 - 1.5f, r.getY() + 8.0f, 3.0f, r.getHeight() - 16.0f, 1.5f);

        // edge readouts
        g.setColour (kText);
        g.setFont (caps (10.0f, 0.05f));
        g.drawText (formatHz (edgeFreq (row * 2)),
                    juce::Rectangle<float> (x0 + 6.0f, r.getBottom() - 18.0f, 50.0f, 12.0f),
                    juce::Justification::centredLeft, false);
        g.drawText (formatHz (edgeFreq (row * 2 + 1)),
                    juce::Rectangle<float> (x1 - 56.0f, r.getBottom() - 18.0f, 50.0f, 12.0f),
                    juce::Justification::centredRight, false);

        g.setColour (kMuted);
        g.setFont (caps (9.5f, 0.18f));
        g.drawText (row == 0 ? "PRE" : "POST",
                    juce::Rectangle<float> (r.getX() + 10.0f, r.getY() + 6.0f, 60.0f, 12.0f),
                    juce::Justification::centredLeft, false);
    }
}

//==============================================================================
void ExciterEditor::setupKnob (Knob& k, const juce::String& text, bool big)
{
    k.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    k.slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, big ? 130 : 72, big ? 22 : 18);
    k.slider.setRotaryParameters (kDialStart, kDialEnd, true);
    k.slider.getProperties().set ("big", big);

    k.label.setText (text, juce::dontSendNotification);
    k.label.setFont (caps (10.0f, 0.14f));
    k.label.setColour (juce::Label::textColourId, kMuted);
    k.label.setJustificationType (juce::Justification::centred);
    k.label.setInterceptsMouseClicks (false, false);

    addAndMakeVisible (k.slider);
    addAndMakeVisible (k.label);
}

void ExciterEditor::setupPill (juce::Button& b, const juce::String& text)
{
    b.setButtonText (text);
    b.getProperties().set ("pill", true);
    b.getProperties().set ("small", true);
    b.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    addAndMakeVisible (b);
}

ExciterEditor::ExciterEditor (ExciterProcessor& p)
    : juce::AudioProcessorEditor (&p), proc (p),
      freqWin (p, [this] { return selectedCh; })
{
    setLookAndFeel (&lnf);

    addAndMakeVisible (freqWin);
    addAndMakeVisible (led);

    setupKnob (harmK,   "HARMONICS",      true);
    setupKnob (amountK, "EXCITER AMOUNT", true);
    setupKnob (widthK,  "WIDTH",          false);
    setupKnob (secondK, "2ND LEVEL",      false);
    setupKnob (subtleK, "SUBTLE GAIN",    false);

    widthK.att = std::make_unique<SliderAttachment> (proc.apvts, "width", widthK.slider);

    setupPill (bypassBtn,   "BYPASS");
    setupPill (linearBtn,   "LINEAR PHASE");
    setupPill (gangedBtn,   "GANGED");
    setupPill (msBtn,       "M/S");
    setupPill (monitorBtn,  "MONITOR");
    setupPill (subtleBtn,   "SUBTLE");
    setupPill (secondBtn,   "2ND");
    setupPill (masterBtn,   "MASTERING");
    setupPill (adaptiveBtn, "ADAPTIVE");
    setupPill (ch1Btn,      "CH 1");
    setupPill (ch2Btn,      "CH 2");

    bypassAtt  = std::make_unique<ButtonAttachment> (proc.apvts, "bypass",  bypassBtn);
    linearAtt  = std::make_unique<ButtonAttachment> (proc.apvts, "linear",  linearBtn);
    gangedAtt  = std::make_unique<ButtonAttachment> (proc.apvts, "ganged",  gangedBtn);
    msAtt      = std::make_unique<ButtonAttachment> (proc.apvts, "ms",      msBtn);
    monitorAtt = std::make_unique<ButtonAttachment> (proc.apvts, "monitor", monitorBtn);

    ch1Btn.setRadioGroupId (100);
    ch2Btn.setRadioGroupId (100);
    ch1Btn.onClick = [this] { selectChannel (0); };
    ch2Btn.onClick = [this] { selectChannel (1); };

    copy12Btn.setButtonText ("COPY 1 > 2");
    copy21Btn.setButtonText ("COPY 2 > 1");
    copy12Btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    copy21Btn.setMouseCursor (juce::MouseCursor::PointingHandCursor);
    copy12Btn.onClick = [this] { copyChannel (0, 1); };
    copy21Btn.onClick = [this] { copyChannel (1, 0); };
    addAndMakeVisible (copy12Btn);
    addAndMakeVisible (copy21Btn);

    auto fill = [this] (juce::ComboBox& box, const juce::String& paramId)
    {
        if (auto* choice = dynamic_cast<juce::AudioParameterChoice*> (proc.apvts.getParameter (paramId)))
            box.addItemList (choice->choices, 1);
        addAndMakeVisible (box);
    };

    fill (osBox,    "oversampling");
    fill (slopeBox, "slope");
    fill (modeBox,  ExciterProcessor::pid (1, "mode"));

    osAtt    = std::make_unique<ComboAttachment> (proc.apvts, "oversampling", osBox);
    slopeAtt = std::make_unique<ComboAttachment> (proc.apvts, "slope",        slopeBox);

    setSize (760, 600);

    selectChannel (0); // binds the per-channel controls
    syncGanged();

    sendLookAndFeelChange();
    startTimerHz (30);
}

ExciterEditor::~ExciterEditor()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void ExciterEditor::bindChannel (int channel)
{
    const int n = channel + 1;
    auto id = [n] (const char* s) { return ExciterProcessor::pid (n, s); };

    harmK.att.reset();
    amountK.att.reset();
    secondK.att.reset();
    subtleK.att.reset();
    subtleAtt.reset();
    secondAtt.reset();
    masterAtt.reset();
    adaptiveAtt.reset();
    modeAtt.reset();

    harmK.att   = std::make_unique<SliderAttachment> (proc.apvts, id ("harm"),       harmK.slider);
    amountK.att = std::make_unique<SliderAttachment> (proc.apvts, id ("amount"),     amountK.slider);
    secondK.att = std::make_unique<SliderAttachment> (proc.apvts, id ("2ndlvl"),     secondK.slider);
    subtleK.att = std::make_unique<SliderAttachment> (proc.apvts, id ("subtlegain"), subtleK.slider);

    subtleAtt   = std::make_unique<ButtonAttachment> (proc.apvts, id ("subtle"),   subtleBtn);
    secondAtt   = std::make_unique<ButtonAttachment> (proc.apvts, id ("2nd"),      secondBtn);
    masterAtt   = std::make_unique<ButtonAttachment> (proc.apvts, id ("master"),   masterBtn);
    adaptiveAtt = std::make_unique<ButtonAttachment> (proc.apvts, id ("adaptive"), adaptiveBtn);

    modeAtt = std::make_unique<ComboAttachment> (proc.apvts, id ("mode"), modeBox);
}

void ExciterEditor::selectChannel (int channel)
{
    selectedCh = channel;
    ch1Btn.setToggleState (channel == 0, juce::dontSendNotification);
    ch2Btn.setToggleState (channel == 1, juce::dontSendNotification);
    bindChannel (channel);
    freqWin.repaint();
}

void ExciterEditor::copyChannel (int from, int to)
{
    static const char* const suffixes[] = { "prehp", "prelp", "posthp", "postlp", "harm", "subtle", "2nd",
                                            "amount", "master", "mode", "2ndlvl", "subtlegain", "adaptive" };

    for (auto* s : suffixes)
    {
        auto* src = proc.apvts.getParameter (ExciterProcessor::pid (from + 1, s));
        auto* dst = proc.apvts.getParameter (ExciterProcessor::pid (to + 1, s));

        if (src != nullptr && dst != nullptr)
        {
            dst->beginChangeGesture();
            dst->setValueNotifyingHost (src->getValue());
            dst->endChangeGesture();
        }
    }
}

void ExciterEditor::syncGanged()
{
    const bool ganged = proc.apvts.getRawParameterValue ("ganged")->load() > 0.5f;
    if (ganged == lastGanged)
        return;

    lastGanged = ganged;

    // While ganged, Ch 1's settings drive both channels, so only Ch 1 is editable.
    if (ganged && selectedCh != 0)
        selectChannel (0);

    for (juce::Component* c : { (juce::Component*) &ch2Btn, (juce::Component*) &copy12Btn, (juce::Component*) &copy21Btn })
    {
        c->setEnabled (! ganged);
        c->setAlpha (ganged ? 0.35f : 1.0f);
    }
}

void ExciterEditor::timerCallback()
{
    led.setState (proc.ledState.load());
    syncGanged();
    freqWin.repaint();
}

//==============================================================================
void ExciterEditor::paint (juce::Graphics& g)
{
    g.setGradientFill (juce::ColourGradient (kBg.brighter (0.06f), 0.0f, 0.0f,
                                             kBg, 0.0f, (float) getHeight(), false));
    g.fillAll();

    // header
    g.setColour (kMuted);
    g.setFont (caps (10.5f, 0.25f));
    g.drawText ("SONGCRAFT AUDIO", 28, 22, 300, 14, juce::Justification::centredLeft, false);

    g.setColour (kText);
    g.setFont (caps (30.0f, 0.02f));
    g.drawText ("Sparkular", 26, 36, 300, 38, juce::Justification::centredLeft, false);

    g.setColour (kMuted);
    g.setFont (caps (9.5f, 0.18f));
    g.drawText ("OVERSAMPLING", osText, juce::Justification::centredLeft, false);
    g.drawText ("SLOPE", slopeText, juce::Justification::centredLeft, false);
    g.drawText ("MODE", modeText, juce::Justification::centredRight, false);

    for (auto r : { freqPanel, ctrlPanel })
    {
        g.setColour (kPanel);
        g.fillRoundedRectangle (r.toFloat(), 14.0f);
        g.setColour (kLine);
        g.drawRoundedRectangle (r.toFloat().reduced (0.5f), 14.0f, 1.0f);
    }

    // divider under the channel / link row
    g.setColour (kLine.withAlpha (0.7f));
    g.drawHorizontalLine (ctrlPanel.getY() + 44, (float) ctrlPanel.getX() + 16.0f,
                          (float) ctrlPanel.getRight() - 16.0f);
}

void ExciterEditor::resized()
{
    constexpr int margin = 24;
    const int W = getWidth();

    // header controls, right to left
    int cursor = W - margin;

    osBox.setBounds (cursor - 84, 38, 84, 28);
    osText = { osBox.getX(), 22, osBox.getWidth(), 14 };
    cursor -= 84 + 14;

    slopeBox.setBounds (cursor - 84, 38, 84, 28);
    slopeText = { slopeBox.getX(), 22, slopeBox.getWidth(), 14 };
    cursor -= 84 + 14;

    linearBtn.setBounds (cursor - 112, 40, 112, 24);
    cursor -= 112 + 10;

    bypassBtn.setBounds (cursor - 78, 40, 78, 24);

    // panels
    freqPanel = { margin, 92, W - 2 * margin, 142 };
    freqWin.setBounds (freqPanel);
    ctrlPanel = { margin, 248, W - 2 * margin, 328 };

    const int cx = ctrlPanel.getX();
    const int cy = ctrlPanel.getY();

    // row A: channel select, link modes, copy
    const int ay = cy + 10;
    int x = cx + 16;
    ch1Btn.setBounds (x, ay, 60, 24);      x += 66;
    ch2Btn.setBounds (x, ay, 60, 24);      x += 66 + 10;
    gangedBtn.setBounds (x, ay, 84, 24);   x += 84 + 6;
    msBtn.setBounds (x, ay, 56, 24);

    copy21Btn.setBounds (ctrlPanel.getRight() - 16 - 100, ay, 100, 24);
    copy12Btn.setBounds (copy21Btn.getX() - 8 - 100, ay, 100, 24);

    // row B: dials
    harmK.label.setBounds (cx + 16, cy + 54, 190, 14);
    harmK.slider.setBounds (cx + 16, cy + 68, 190, 200);
    led.setBounds (cx + 16 + 190 - 16, cy + 54, 12, 12);

    amountK.label.setBounds (cx + 216, cy + 54, 190, 14);
    amountK.slider.setBounds (cx + 216, cy + 68, 190, 200);

    Knob* small[3] = { &widthK, &secondK, &subtleK };
    const int kx = cx + 418;
    for (int i = 0; i < 3; ++i)
    {
        small[i]->label.setBounds (kx + i * 94, cy + 108, 94, 14);
        small[i]->slider.setBounds (kx + i * 94, cy + 124, 94, 140);
    }

    // row C: switches and harmonics mode
    const int ry = ctrlPanel.getBottom() - 16 - 26;
    int px = cx + 16;
    subtleBtn.setBounds (px, ry, 84, 26);    px += 84 + 8;
    secondBtn.setBounds (px, ry, 56, 26);    px += 56 + 8;
    masterBtn.setBounds (px, ry, 104, 26);   px += 104 + 8;
    monitorBtn.setBounds (px, ry, 88, 26);   px += 88 + 8;
    adaptiveBtn.setBounds (px, ry, 96, 26);

    modeBox.setBounds (ctrlPanel.getRight() - 16 - 110, ry - 1, 110, 28);
    modeText = { modeBox.getX() - 50, ry + 5, 44, 14 };
}
