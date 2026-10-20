#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

namespace colours
{
const juce::Colour background { 0xff0e1016 };
const juce::Colour panel { 0xff181c26 };
const juce::Colour panelEdge { 0xff262c3b };
const juce::Colour control { 0xff222736 };     // knob bodies and drop-downs
const juce::Colour controlEdge { 0xff3a4257 };
const juce::Colour track { 0xff303749 };       // unlit part of a knob's arc
const juce::Colour grid { 0xff2a3040 };
const juce::Colour text { 0xffeef1f7 };
const juce::Colour label { 0xffc3c9d6 };
const juce::Colour dimText { 0xff8f98ad };

const juce::Colour wave { 0xff5ad1e6 };
const juce::Colour barrier { 0xffc77dff };

// One accent per section, so related controls read as a group.
const juce::Colour oscillators { 0xff5ad1e6 };
const juce::Colour filter { 0xffc77dff };
const juce::Colour ampEnvelope { 0xff6ee7a0 };
const juce::Colour filterEnvelope { 0xffe58cf0 };
const juce::Colour lfo { 0xffffc861 };
const juce::Colour midi { 0xff7aa7ff };
} // namespace colours

// Flat dark theme with large, high-contrast knobs, values and drop-downs.
class QuantumLookAndFeel : public juce::LookAndFeel_V4
{
public:
    QuantumLookAndFeel()
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
    }

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
