#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <array>

// The colours the editor paints with. They belong to the current theme: applyTheme() changes
// them for every open editor, so read them at paint time rather than keeping copies.
namespace colours
{
inline juce::Colour background, panel, panelEdge;
inline juce::Colour control, controlEdge;  // knob bodies and drop-downs
inline juce::Colour track;                 // unlit part of a knob's arc
inline juce::Colour grid, text, label, dimText;
inline juce::Colour wave, barrier;

// One accent per section, so related controls read as a group.
inline juce::Colour oscillators, filter, ampEnvelope, filterEnvelope, lfo, midi;

inline juce::Colour whiteKey, blackKey, keyEdge;
} // namespace colours

struct Theme
{
    const char* name;
    bool minilogueLayout;  // controls placed where their minilogue xd knob sits
    juce::uint32 background, panel, panelEdge, control, controlEdge, track, grid, text, label, dimText;
    juce::uint32 wave, barrier, oscillators, filter, ampEnvelope, filterEnvelope, lfo, midi;
    juce::uint32 whiteKey, blackKey, keyEdge;
};

inline const std::array<Theme, 6>& themes()
{
    static const std::array<Theme, 6> list { {
        { "Quantum Night", false,
          0xff0e1016, 0xff181c26, 0xff262c3b, 0xff222736, 0xff3a4257, 0xff303749, 0xff2a3040, 0xffeef1f7, 0xffc3c9d6, 0xff8f98ad,
          0xff5ad1e6, 0xffc77dff, 0xff5ad1e6, 0xffc77dff, 0xff6ee7a0, 0xffe58cf0, 0xffffc861, 0xff7aa7ff,
          0xffe4e7ee, 0xff1b1f2a, 0xff9aa1b2 },
        { "minilogue xd", true,  // black panel, white legends, one red LED colour
          0xff0b0b0c, 0xff151517, 0xff2c2c30, 0xff202023, 0xff4a4a50, 0xff2e2e33, 0xff26262a, 0xfff4f4f4, 0xffdcdcdc, 0xff8e8e94,
          0xffff4b3e, 0xfff4f4f4, 0xffd9d9de, 0xffd9d9de, 0xffd9d9de, 0xffd9d9de, 0xffd9d9de, 0xffd9d9de,
          0xfff2f2f0, 0xff141414, 0xff8e8e94 },
        { "Paper Lab", false,  // light, like a printed physics diagram
          0xfff2efe8, 0xfffbfaf6, 0xffd9d4c7, 0xffece8de, 0xffb9b2a2, 0xffddd8cc, 0xffe6e1d6, 0xff1d1f24, 0xff3d424d, 0xff767b86,
          0xff0a7f95, 0xff8b3fd1, 0xff0a7f95, 0xff8b3fd1, 0xff1f8a52, 0xffb23fb8, 0xffc27c00, 0xff2f5fd0,
          0xfffffdf8, 0xff2a2c31, 0xffb9b2a2 },
        { "Phosphor", false,  // green oscilloscope screen with an amber barrier
          0xff050a07, 0xff0b140e, 0xff18301f, 0xff102016, 0xff2a5236, 0xff1a3322, 0xff143020, 0xffd8ffe2, 0xff9fdcae, 0xff5f8f6c,
          0xff39ff88, 0xffffb000, 0xff39ff88, 0xffffb000, 0xff7dffb0, 0xffffd166, 0xffb6ff3b, 0xff4fe3c1,
          0xffcfe9d6, 0xff0b140e, 0xff5f8f6c },
        { "Synthwave", false,
          0xff140a24, 0xff1f1036, 0xff3a1f5c, 0xff2a1748, 0xff563585, 0xff3b2160, 0xff2e1a4d, 0xfffdf0ff, 0xffe0c6f5, 0xffa387c0,
          0xff2de2e6, 0xffff3cac, 0xff2de2e6, 0xffff3cac, 0xff7cf29c, 0xffff6c11, 0xffffd319, 0xff8f7dff,
          0xfff4e9ff, 0xff1f1036, 0xffa387c0 },
        { "Graphite", false,  // warm greys with orange
          0xff161616, 0xff202020, 0xff333333, 0xff2b2b2b, 0xff4a4a4a, 0xff3a3a3a, 0xff2e2e2e, 0xfff2f2f2, 0xffc8c8c8, 0xff8c8c8c,
          0xffff8a3d, 0xff9ec1ff, 0xffff9f43, 0xffff6b4a, 0xffe6e6e6, 0xffffc145, 0xff9ec1ff, 0xffb8b8b8,
          0xffececec, 0xff111111, 0xff8c8c8c },
    } };
    return list;
}

inline void applyTheme (const Theme& t)
{
    using juce::Colour;
    colours::background = Colour (t.background);
    colours::panel = Colour (t.panel);
    colours::panelEdge = Colour (t.panelEdge);
    colours::control = Colour (t.control);
    colours::controlEdge = Colour (t.controlEdge);
    colours::track = Colour (t.track);
    colours::grid = Colour (t.grid);
    colours::text = Colour (t.text);
    colours::label = Colour (t.label);
    colours::dimText = Colour (t.dimText);
    colours::wave = Colour (t.wave);
    colours::barrier = Colour (t.barrier);
    colours::oscillators = Colour (t.oscillators);
    colours::filter = Colour (t.filter);
    colours::ampEnvelope = Colour (t.ampEnvelope);
    colours::filterEnvelope = Colour (t.filterEnvelope);
    colours::lfo = Colour (t.lfo);
    colours::midi = Colour (t.midi);
    colours::whiteKey = Colour (t.whiteKey);
    colours::blackKey = Colour (t.blackKey);
    colours::keyEdge = Colour (t.keyEdge);
}

// Flat theme with large, high-contrast knobs, values and drop-downs.
class QuantumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QuantumLookAndFeel()
    {
        if (colours::background.isTransparent())
            applyTheme (themes()[0]);
        refreshColours();
    }

    // Picks up the current theme's colours; call after applyTheme.
    void refreshColours()
    {
        setColour (juce::ResizableWindow::backgroundColourId, colours::background);
        setColour (juce::Label::textColourId, colours::label);

        setColour (juce::Slider::textBoxTextColourId, colours::text);
        setColour (juce::Slider::textBoxBackgroundColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        setColour (juce::Slider::textBoxHighlightColourId, colours::controlEdge);

        setColour (juce::ComboBox::backgroundColourId, colours::control);
        setColour (juce::ComboBox::outlineColourId, colours::controlEdge);
        setColour (juce::ComboBox::textColourId, colours::text);
        setColour (juce::ComboBox::arrowColourId, colours::label);

        setColour (juce::PopupMenu::backgroundColourId, colours::panel);
        setColour (juce::PopupMenu::textColourId, colours::text);
        setColour (juce::PopupMenu::highlightedBackgroundColourId, colours::controlEdge);
        setColour (juce::PopupMenu::highlightedTextColourId, colours::text);

        setColour (juce::TextEditor::backgroundColourId, colours::control);
        setColour (juce::TextEditor::textColourId, colours::text);
        setColour (juce::TextEditor::highlightColourId, colours::controlEdge);
        setColour (juce::TextEditor::outlineColourId, colours::controlEdge);
        setColour (juce::TextEditor::focusedOutlineColourId, colours::wave);
        setColour (juce::CaretComponent::caretColourId, colours::text);

        setColour (juce::TextButton::buttonColourId, colours::control);
        setColour (juce::TextButton::buttonOnColourId, colours::controlEdge);
        setColour (juce::TextButton::textColourOffId, colours::text);
        setColour (juce::TextButton::textColourOnId, colours::text);
        setColour (juce::PopupMenu::headerTextColourId, colours::dimText);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& button, const juce::Colour& background,
                               bool highlighted, bool down) override
    {
        const auto bounds = button.getLocalBounds().toFloat().reduced (0.5f);
        g.setColour (background.brighter (down ? 0.2f : (highlighted ? 0.1f : 0.0f)));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (colours::controlEdge);
        g.drawRoundedRectangle (bounds, 5.0f, 1.2f);
    }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::FontOptions (15.0f); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height, float position,
                           float startAngle, float endAngle, juce::Slider& slider) override
    {
        const auto bounds = juce::Rectangle<int> (x, y, width, height).toFloat().reduced (2.0f);
        const float radius = juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f;
        const auto centre = bounds.getCentre();
        const float arcWidth = juce::jmax (3.0f, radius * 0.14f);
        const float arcRadius = radius - arcWidth * 0.5f;
        const auto accent = slider.findColour (juce::Slider::rotarySliderFillColourId);
        const float angle = startAngle + position * (endAngle - startAngle);

        const juce::PathStrokeType arcStroke (arcWidth, juce::PathStrokeType::curved, juce::PathStrokeType::rounded);

        juce::Path track;
        track.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f, startAngle, endAngle, true);
        g.setColour (colours::track);
        g.strokePath (track, arcStroke);

        // Bipolar parameters (envelope amount, fine tune) light up from their zero point.
        float fromAngle = startAngle;
        if (slider.getMinimum() < 0.0 && slider.getMaximum() > 0.0)
            fromAngle = startAngle + (float) slider.valueToProportionOfLength (0.0) * (endAngle - startAngle);

        if (std::abs (angle - fromAngle) > 0.01f)
        {
            juce::Path value;
            value.addCentredArc (centre.x, centre.y, arcRadius, arcRadius, 0.0f,
                                 juce::jmin (fromAngle, angle), juce::jmax (fromAngle, angle), true);
            g.setColour (slider.isEnabled() ? accent : colours::dimText);
            g.strokePath (value, arcStroke);
        }

        const float bodyRadius = arcRadius - arcWidth * 0.5f - 4.0f;
        const auto body = juce::Rectangle<float> (bodyRadius * 2.0f, bodyRadius * 2.0f).withCentre (centre);
        g.setColour (slider.isMouseOverOrDragging() ? colours::control.brighter (0.12f) : colours::control);
        g.fillEllipse (body);
        g.setColour (colours::controlEdge);
        g.drawEllipse (body, 1.2f);

        g.setColour (colours::text);
        g.drawLine ({ centre.getPointOnCircumference (bodyRadius * 0.25f, angle),
                      centre.getPointOnCircumference (bodyRadius * 0.85f, angle) },
                    juce::jmax (2.0f, radius * 0.08f));
    }

    juce::Label* createSliderTextBox (juce::Slider& slider) override
    {
        auto* box = LookAndFeel_V4::createSliderTextBox (slider);
        box->setFont (juce::FontOptions (15.0f));
        box->setJustificationType (juce::Justification::centred);
        return box;
    }

    void drawComboBox (juce::Graphics& g, int width, int height, bool, int, int, int, int, juce::ComboBox& box) override
    {
        const auto bounds = juce::Rectangle<int> (width, height).toFloat().reduced (0.5f);
        g.setColour (box.findColour (juce::ComboBox::backgroundColourId).brighter (box.isMouseOver (true) ? 0.1f : 0.0f));
        g.fillRoundedRectangle (bounds, 5.0f);
        g.setColour (box.hasKeyboardFocus (true) ? colours::text : box.findColour (juce::ComboBox::outlineColourId));
        g.drawRoundedRectangle (bounds, 5.0f, 1.2f);

        juce::Path arrow;
        const float cx = (float) width - 14.0f, cy = (float) height * 0.5f;
        arrow.startNewSubPath (cx - 4.5f, cy - 2.0f);
        arrow.lineTo (cx, cy + 2.5f);
        arrow.lineTo (cx + 4.5f, cy - 2.0f);
        g.setColour (box.findColour (juce::ComboBox::arrowColourId));
        g.strokePath (arrow, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
    }

    juce::Font getComboBoxFont (juce::ComboBox&) override { return juce::FontOptions (15.0f); }
    juce::Font getPopupMenuFont() override { return juce::FontOptions (15.0f); }

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override
    {
        label.setBounds (8, 1, box.getWidth() - 30, box.getHeight() - 2);
        label.setFont (getComboBoxFont (box));
    }
};
