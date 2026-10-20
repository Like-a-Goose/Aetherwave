#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif

namespace colours
{
const juce::Colour background { 0xff12141c };
const juce::Colour panel { 0xff1c1f2b };
const juce::Colour grid { 0xff2c3142 };
const juce::Colour text { 0xffc9cedb };
const juce::Colour dimText { 0xff7d8499 };
const juce::Colour wave { 0xff5ad1e6 };
const juce::Colour barrier { 0xffc77dff };
} // namespace colours

//==============================================================================
BarrierDisplay::BarrierDisplay (QuantumSynthProcessor& p) : processor (p)
{
    settings = processor.getBaseFilterSettings();
    startTimerHz (30);
}

void BarrierDisplay::timerCallback()
{
    settings = processor.getBaseFilterSettings();
    const double live = processor.getLiveBarrierHz();
    liveBarrierHz = live > 0.0 ? live : settings.barrierHz;
    repaint();
}

void BarrierDisplay::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat();
    g.setColour (colours::panel);
    g.fillRoundedRectangle (area, 6.0f);

    area = area.reduced (12.0f);
    paintPotential (g, area.removeFromLeft (240.0f));
    area.removeFromLeft (16.0f);
    paintResponse (g, area);
}

void BarrierDisplay::paintPotential (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (colours::dimText);
    g.setFont (13.0f);
    g.drawText ("Potential and wavefunction", area.removeFromTop (18.0f), juce::Justification::topLeft);

    const bool twin = settings.mode == qs::FilterMode::resonantTunnel || settings.mode == qs::FilterMode::resonantReflect;
    const double span = twin ? 2.0 * settings.width + settings.gap : settings.width;
    const double margin = 6.0;
    const double totalX = span + 2.0 * margin;

    const float baseY = area.getBottom() - 6.0f;
    const float topY = area.getY() + 26.0f;
    auto toScreenX = [&] (double x) { return area.getX() + (float) ((x + margin) / totalX) * area.getWidth(); };

    // Barriers, height V0 = 1.
    g.setColour (colours::barrier.withAlpha (0.35f));
    auto drawBarrier = [&] (double x0)
    {
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (toScreenX (x0), topY, toScreenX (x0 + settings.width), baseY));
    };
    drawBarrier (0.0);
    if (twin)
        drawBarrier (settings.width + settings.gap);

    g.setColour (colours::grid);
    g.drawHorizontalLine ((int) baseY, area.getX(), area.getRight());

    // Real part of a particle at E = 0.6 V0: incoming plus reflected on the left, decaying
    // inside the barrier, and whatever tunnelled through on the right.
    qs::BarrierGeometry geometry;
    geometry.width = settings.width;
    geometry.gap = settings.gap;
    geometry.shape = twin ? qs::BarrierShape::twin : qs::BarrierShape::single;
    const double energy = 0.6;
    const auto result = qs::scatter (energy, geometry);

    const float midY = (topY + baseY) * 0.5f;
    const float amp = (baseY - topY) * 0.3f;
    const double k = std::sqrt (energy);
    const double kappa = std::sqrt (1.0 - energy);

    juce::Path psi;
    const int steps = 300;
    for (int i = 0; i <= steps; ++i)
    {
        const double x = -margin + totalX * i / steps;
        double y;

        if (x < 0.0)
            y = std::cos (k * x * 4.0) * (1.0 + result.reflected) * 0.5;
        else if (x > span)
            y = std::cos (k * (x - span) * 4.0) * result.transmitted;
        else
        {
            // Inside the barrier region: exponential decay towards the transmitted amplitude.
            const double depth = twin ? std::min (x, span - x) / span * 2.0 : x / span;
            const double decayed = std::exp (-kappa * (twin ? std::min (x, span - x) : x));
            y = juce::jmap (depth, decayed, (double) result.transmitted) * ((1.0 + result.reflected) * 0.5);
        }

        const auto px = toScreenX (x);
        const auto py = midY - (float) y * amp;
        if (i == 0)
            psi.startNewSubPath (px, py);
        else
            psi.lineTo (px, py);
    }

    g.setColour (colours::wave);
    g.strokePath (psi, juce::PathStrokeType (1.6f));

    g.setColour (colours::dimText);
    g.setFont (12.0f);
    g.drawText ("tunnelled: " + juce::String (100.0 * result.transmitted * result.transmitted, 1) + "%",
                area.withTop (baseY - 22.0f).withBottom (baseY - 4.0f), juce::Justification::bottomRight);
}

void BarrierDisplay::paintResponse (juce::Graphics& g, juce::Rectangle<float> area)
{
    g.setColour (colours::dimText);
    g.setFont (13.0f);
    g.drawText ("Filter response", area.removeFromTop (18.0f), juce::Justification::topLeft);
    area.removeFromBottom (14.0f);

    const double minHz = 20.0, maxHz = 20000.0;
    const float minDb = -60.0f, maxDb = 6.0f;

    auto xFor = [&] (double hz) { return area.getX() + (float) (std::log (hz / minHz) / std::log (maxHz / minHz)) * area.getWidth(); };
    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), maxDb, minDb, area.getY(), area.getBottom()); };

    g.setFont (11.0f);
    for (double hz : { 100.0, 1000.0, 10000.0 })
    {
        g.setColour (colours::grid);
        g.drawVerticalLine ((int) xFor (hz), area.getY(), area.getBottom());
        g.setColour (colours::dimText);
        g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + "k" : juce::String ((int) hz),
                    juce::Rectangle<float> (xFor (hz) - 20.0f, area.getBottom() + 1.0f, 40.0f, 13.0f), juce::Justification::centred);
    }
    for (float db : { 0.0f, -12.0f, -24.0f, -36.0f, -48.0f })
    {
        g.setColour (colours::grid);
        g.drawHorizontalLine ((int) yFor (db), area.getX(), area.getRight());
    }

    auto curve = [&] (double barrierHz)
    {
        auto s = settings;
        s.barrierHz = barrierHz;
        juce::Path path;
        const int points = (int) area.getWidth();
        for (int i = 0; i <= points; ++i)
        {
            const double hz = minHz * std::pow (maxHz / minHz, (double) i / points);
            const float db = juce::Decibels::gainToDecibels ((float) qs::tunnelResponse (hz, s), minDb);
            if (i == 0)
                path.startNewSubPath (xFor (hz), yFor (db));
            else
                path.lineTo (xFor (hz), yFor (db));
        }
        return path;
    };

    g.setColour (colours::barrier.withAlpha (0.5f));
    g.strokePath (curve (settings.barrierHz), juce::PathStrokeType (1.0f));

    g.setColour (colours::wave);
    g.strokePath (curve (liveBarrierHz), juce::PathStrokeType (2.0f));

    const float markerX = xFor (juce::jlimit (minHz, maxHz, liveBarrierHz));
    g.setColour (colours::barrier);
    g.drawVerticalLine ((int) markerX, area.getY(), area.getBottom());
    g.setColour (colours::text);
    g.drawText ("barrier " + juce::String ((int) liveBarrierHz) + " Hz",
                juce::Rectangle<float> (markerX + 4.0f, area.getY(), 120.0f, 14.0f), juce::Justification::left);
}

//==============================================================================
void QuantumSynthEditor::Control::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (16));

    if (box.isVisible())
        box.setBounds (area.removeFromTop (26).reduced (2, 0));
    else
        slider.setBounds (area);
}

QuantumSynthEditor::Control* QuantumSynthEditor::addKnob (const juce::String& paramId, const juce::String& name)
{
    auto c = std::make_unique<Control>();
    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setColour (juce::Label::textColourId, colours::text);
    c->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 70, 16);
    c->slider.setColour (juce::Slider::rotarySliderFillColourId, colours::wave);
    c->slider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
    c->addAndMakeVisible (c->label);
    c->addAndMakeVisible (c->slider);
    c->addChildComponent (c->box);
    c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (processor.parameters, paramId, c->slider);
    addAndMakeVisible (*c);
    controls.push_back (std::move (c));
    return controls.back().get();
}

QuantumSynthEditor::Control* QuantumSynthEditor::addChoice (const juce::String& paramId, const juce::String& name)
{
    auto c = std::make_unique<Control>();
    c->preferredWidth = 104;
    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setColour (juce::Label::textColourId, colours::text);

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (processor.parameters.getParameter (paramId)))
        c->box.addItemList (param->choices, 1);

    c->addAndMakeVisible (c->label);
    c->addAndMakeVisible (c->box);
    c->addChildComponent (c->slider);
    c->boxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (processor.parameters, paramId, c->box);
    addAndMakeVisible (*c);
    controls.push_back (std::move (c));
    return controls.back().get();
}

QuantumSynthEditor::QuantumSynthEditor (QuantumSynthProcessor& p)
    : AudioProcessorEditor (&p), processor (p), display (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    addAndMakeVisible (display);
    addAndMakeVisible (keyboard);
    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (24);

    sections.push_back ({ "Oscillators",
                          { addChoice ("osc1Wave", "Osc 1"), addChoice ("osc2Wave", "Osc 2"),
                            addKnob ("osc2Semi", "Semi"), addKnob ("osc2Fine", "Fine"), addKnob ("oscMix", "Mix") }, {} });
    sections.push_back ({ "Quantum filter",
                          { addChoice ("filterMode", "Mode"), addKnob ("barrier", "Barrier"), addKnob ("width", "Width"),
                            addKnob ("gap", "Gap"), addKnob ("envAmount", "Env Amt"), addKnob ("keyTrack", "Key Trk") }, {} });
    sections.push_back ({ "Amp envelope",
                          { addKnob ("ampA", "Attack"), addKnob ("ampD", "Decay"), addKnob ("ampS", "Sustain"), addKnob ("ampR", "Release") }, {} });
    sections.push_back ({ "Filter envelope",
                          { addKnob ("filtA", "Attack"), addKnob ("filtD", "Decay"), addKnob ("filtS", "Sustain"), addKnob ("filtR", "Release") }, {} });
    sections.push_back ({ "LFO",
                          { addChoice ("lfoWave", "Wave"), addChoice ("lfoTarget", "Target"),
                            addKnob ("lfoRate", "Rate"), addKnob ("lfoAmount", "Amount") }, {} });
    sections.push_back ({ "MIDI and output",
                          { addChoice ("ccMap", "CC map"), addKnob ("bendRange", "Bend"), addKnob ("gain", "Gain") }, {} });

    lastControl.setJustificationType (juce::Justification::centredRight);
    lastControl.setColour (juce::Label::textColourId, colours::dimText);
    lastControl.setFont (juce::FontOptions (12.0f));
    lastControl.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (lastControl);

    setSize (1260, 600);
    startTimerHz (10);
}

QuantumSynthEditor::~QuantumSynthEditor() = default;

void QuantumSynthEditor::timerCallback()
{
    // Show which synth parameter the last minilogue xd knob moved, so the mapping is easy to learn.
    const int cc = processor.getLastXdControl();
    if (cc != shownControl)
    {
        shownControl = cc;
        if (const auto* c = qs::xd::findControl (cc))
            if (auto* param = processor.parameters.getParameter (c->paramId))
                lastControl.setText (juce::String ("minilogue xd ") + c->panelName + " > " + param->getName (64),
                                     juce::dontSendNotification);
    }

    if (++midiCheckCounter >= 20)  // every 2 seconds
    {
        midiCheckCounter = 0;
        enableMinilogueMidiInput();
    }
}

void QuantumSynthEditor::enableMinilogueMidiInput()
{
#if JucePlugin_Build_Standalone
    // The Linux standalone app opens no MIDI inputs by itself. When a minilogue xd is plugged
    // in, enable its keyboard and knob port once; after that the Options dialog is in charge.
    auto* holder = juce::StandalonePluginHolder::getInstance();
    if (holder == nullptr)
        return;

    juce::Array<juce::MidiDeviceInfo> candidates;
    for (const auto& device : juce::MidiInput::getAvailableDevices())
        if (device.name.containsIgnoreCase ("minilogue xd") && ! autoEnabledMidiInputs.contains (device.identifier))
            candidates.add (device);

    if (candidates.isEmpty())
        return;

    // The xd has two USB MIDI ports. Only the first one (KBD/KNOB) sends what is played,
    // so enabling one port avoids doubled notes if the other also echoes.
    auto chosen = candidates.getFirst();
    for (const auto& device : candidates)
        if (device.name.containsIgnoreCase ("KBD"))
            chosen = device;

    for (const auto& device : candidates)
        autoEnabledMidiInputs.add (device.identifier);

    auto& devices = holder->deviceManager;
    const bool alreadyListening = std::any_of (candidates.begin(), candidates.end(), [&] (const juce::MidiDeviceInfo& d)
                                               { return devices.isMidiInputDeviceEnabled (d.identifier); });
    if (! alreadyListening)
        devices.setMidiInputDeviceEnabled (chosen.identifier, true);
#endif
}

void QuantumSynthEditor::paint (juce::Graphics& g)
{
    g.fillAll (colours::background);

    for (const auto& section : sections)
    {
        g.setColour (colours::panel);
        g.fillRoundedRectangle (section.bounds.toFloat(), 6.0f);
        g.setColour (colours::dimText);
        g.setFont (13.0f);
        g.drawText (section.title, section.bounds.reduced (10, 4).removeFromTop (18), juce::Justification::topLeft);
    }
}

void QuantumSynthEditor::resized()
{
    auto area = getLocalBounds().reduced (12);

    keyboard.setBounds (area.removeFromBottom (64));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f);  // 43 white keys from C1 to C7
    area.removeFromBottom (10);
    display.setBounds (area.removeFromTop (190));
    lastControl.setBounds (display.getBounds().reduced (12, 8).removeFromTop (18).removeFromRight (360));
    area.removeFromTop (10);

    auto layoutRow = [this] (juce::Rectangle<int> row, std::initializer_list<int> sectionIndices)
    {
        for (int index : sectionIndices)
        {
            auto& section = sections[(size_t) index];
            int width = 20;
            for (auto* c : section.controls)
                width += c->preferredWidth;

            section.bounds = row.removeFromLeft (width);
            row.removeFromLeft (10);

            auto inner = section.bounds.reduced (10, 6);
            inner.removeFromTop (20);
            for (auto* c : section.controls)
                c->setBounds (inner.removeFromLeft (c->preferredWidth).withSizeKeepingCentre (c->preferredWidth,
                                                                                              c->box.isVisible() ? 46 : inner.getHeight()));
        }
    };

    const int rowHeight = (area.getHeight() - 10) / 2;
    layoutRow (area.removeFromTop (rowHeight), { 0, 1, 5 });
    area.removeFromTop (10);
    layoutRow (area.removeFromTop (rowHeight), { 2, 3, 4 });
}
