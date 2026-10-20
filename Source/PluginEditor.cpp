#include "PluginEditor.h"

#if JucePlugin_Build_Standalone
 #include <juce_audio_plugin_client/Standalone/juce_StandaloneFilterWindow.h>
#endif


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
    g.fillRoundedRectangle (area, 8.0f);
    g.setColour (colours::panelEdge);
    g.drawRoundedRectangle (area.reduced (0.5f), 8.0f, 1.0f);

    area = area.reduced (16.0f, 12.0f);
    paintPotential (g, area.removeFromLeft (270.0f));
    area.removeFromLeft (24.0f);
    paintResponse (g, area);
}

void BarrierDisplay::paintHeading (juce::Graphics& g, juce::Rectangle<float>& area, const juce::String& text)
{
    g.setColour (colours::label);
    g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
    g.drawText (text, area.removeFromTop (22.0f), juce::Justification::topLeft);
}

void BarrierDisplay::paintPotential (juce::Graphics& g, juce::Rectangle<float> area)
{
    paintHeading (g, area, "Barrier and wavefunction");

    const bool twin = settings.mode == qs::FilterMode::resonantTunnel || settings.mode == qs::FilterMode::resonantReflect;
    const double span = twin ? 2.0 * settings.width + settings.gap : settings.width;
    const double margin = 6.0;
    const double totalX = span + 2.0 * margin;

    const float baseY = area.getBottom() - 6.0f;
    const float topY = area.getY() + 26.0f;
    auto toScreenX = [&] (double x) { return area.getX() + (float) ((x + margin) / totalX) * area.getWidth(); };

    // Barriers, height V0 = 1.
    g.setColour (colours::barrier.withAlpha (0.45f));
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
    g.strokePath (psi, juce::PathStrokeType (2.2f));

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("tunnelled " + juce::String (100.0 * result.transmitted * result.transmitted, 1) + "%",
                area.withTop (area.getY()).withHeight (18.0f), juce::Justification::topRight);
}

void BarrierDisplay::paintResponse (juce::Graphics& g, juce::Rectangle<float> area)
{
    juce::String mode;
    if (auto* param = processor.parameters.getParameter ("filterMode"))
        mode = param->getCurrentValueAsText();
    paintHeading (g, area, "Filter response: " + mode);
    area.removeFromTop (4.0f);
    area.removeFromBottom (18.0f);
    area.removeFromLeft (34.0f);  // dB scale

    const double minHz = 20.0, maxHz = 20000.0;
    const float minDb = -60.0f, maxDb = 6.0f;

    auto xFor = [&] (double hz) { return area.getX() + (float) (std::log (hz / minHz) / std::log (maxHz / minHz)) * area.getWidth(); };
    auto yFor = [&] (float db) { return juce::jmap (juce::jlimit (minDb, maxDb, db), maxDb, minDb, area.getY(), area.getBottom()); };

    g.setFont (juce::FontOptions (13.0f));
    for (double hz : { 100.0, 1000.0, 10000.0 })
    {
        g.setColour (colours::grid);
        g.drawVerticalLine ((int) xFor (hz), area.getY(), area.getBottom());
        g.setColour (colours::dimText);
        g.drawText (hz >= 1000.0 ? juce::String ((int) (hz / 1000.0)) + " kHz" : juce::String ((int) hz) + " Hz",
                    juce::Rectangle<float> (xFor (hz) - 30.0f, area.getBottom() + 3.0f, 60.0f, 15.0f), juce::Justification::centred);
    }
    for (float db : { 0.0f, -24.0f, -48.0f })
    {
        g.setColour (colours::grid);
        g.drawHorizontalLine ((int) yFor (db), area.getX(), area.getRight());
        g.setColour (colours::dimText);
        g.drawText (juce::String ((int) db) + " dB", juce::Rectangle<float> (area.getX() - 40.0f, yFor (db) - 8.0f, 36.0f, 16.0f),
                    juce::Justification::centredRight);
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

    g.setColour (colours::barrier.withAlpha (0.55f));
    g.strokePath (curve (settings.barrierHz), juce::PathStrokeType (1.2f));

    auto live = curve (liveBarrierHz);
    auto fill = live;
    fill.lineTo (area.getRight(), area.getBottom());
    fill.lineTo (area.getX(), area.getBottom());
    fill.closeSubPath();
    g.setGradientFill (juce::ColourGradient (colours::wave.withAlpha (0.25f), 0.0f, area.getY(),
                                             colours::wave.withAlpha (0.0f), 0.0f, area.getBottom(), false));
    g.fillPath (fill);
    g.setColour (colours::wave);
    g.strokePath (live, juce::PathStrokeType (2.5f));

    // Barrier marker with its frequency in a tag that stays inside the plot.
    const float markerX = xFor (juce::jlimit (minHz, maxHz, liveBarrierHz));
    g.setColour (colours::barrier);
    g.drawLine (markerX, area.getY(), markerX, area.getBottom(), 1.5f);

    const auto tagText = "barrier " + juce::String (juce::roundToInt (liveBarrierHz)) + " Hz";
    g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
    const float tagWidth = 128.0f;
    float tagX = markerX + 6.0f;
    if (tagX + tagWidth > area.getRight())
        tagX = markerX - 6.0f - tagWidth;
    const auto tag = juce::Rectangle<float> (tagX, area.getY() + 4.0f, tagWidth, 22.0f);
    g.setColour (colours::barrier.withAlpha (0.9f));
    g.fillRoundedRectangle (tag, 4.0f);
    g.setColour (colours::background);
    g.drawText (tagText, tag, juce::Justification::centred);
}

//==============================================================================
void QuantumSynthEditor::Control::resized()
{
    auto area = getLocalBounds();
    label.setBounds (area.removeFromTop (labelHeight));

    // A drop-down sits where a knob would, so labels and controls line up across a row.
    if (box.isVisible())
        box.setBounds (area.withTrimmedBottom (valueHeight).withSizeKeepingCentre (area.getWidth() - 8, boxHeight));
    else
        slider.setBounds (area);
}

int QuantumSynthEditor::Section::naturalWidth() const
{
    int width = 2 * 14;
    for (auto* c : controls)
        width += c->preferredWidth;
    return width;
}

void QuantumSynthEditor::Section::layOutControls (juce::Rectangle<int> area) const
{
    // Spare width is shared out evenly between the controls.
    int spare = area.getWidth();
    for (auto* c : controls)
        spare -= c->preferredWidth;
    const int extra = juce::jmax (0, spare) / juce::jmax (1, (int) controls.size());

    for (auto* c : controls)
        c->setBounds (area.removeFromLeft (c->preferredWidth + extra).withSizeKeepingCentre (c->preferredWidth, area.getHeight()));
}

void QuantumSynthEditor::addSection (const juce::String& title, juce::Colour accent, std::vector<Control*> sectionControls)
{
    for (auto* c : sectionControls)
        c->slider.setColour (juce::Slider::rotarySliderFillColourId, accent);
    sections.push_back ({ title, accent, std::move (sectionControls), {} });
}

QuantumSynthEditor::Control* QuantumSynthEditor::addKnob (const juce::String& paramId, const juce::String& name)
{
    auto c = std::make_unique<Control>();
    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::FontOptions (15.0f));
    c->slider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, c->preferredWidth, Control::valueHeight);
    c->addAndMakeVisible (c->label);
    c->addAndMakeVisible (c->slider);
    c->addChildComponent (c->box);
    c->sliderAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (synth.parameters, paramId, c->slider);
    c->setInterceptsMouseClicks (false, true);
    addAndMakeVisible (*c);
    controls.push_back (std::move (c));
    return controls.back().get();
}

QuantumSynthEditor::Control* QuantumSynthEditor::addChoice (const juce::String& paramId, const juce::String& name)
{
    auto c = std::make_unique<Control>();
    c->preferredWidth = 128;
    c->label.setText (name, juce::dontSendNotification);
    c->label.setJustificationType (juce::Justification::centred);
    c->label.setFont (juce::FontOptions (15.0f));

    if (auto* param = dynamic_cast<juce::AudioParameterChoice*> (synth.parameters.getParameter (paramId)))
        c->box.addItemList (param->choices, 1);

    c->addAndMakeVisible (c->label);
    c->addAndMakeVisible (c->box);
    c->addChildComponent (c->slider);
    c->boxAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (synth.parameters, paramId, c->box);
    c->setInterceptsMouseClicks (false, true);
    addAndMakeVisible (*c);
    controls.push_back (std::move (c));
    return controls.back().get();
}

QuantumSynthEditor::QuantumSynthEditor (QuantumSynthProcessor& p)
    : AudioProcessorEditor (&p), synth (p), display (p),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (display);
    addAndMakeVisible (keyboard);
    keyboard.setAvailableRange (24, 96);
    keyboard.setLowestVisibleKey (24);
    keyboard.setColour (juce::MidiKeyboardComponent::whiteNoteColourId, juce::Colour (0xffe4e7ee));
    keyboard.setColour (juce::MidiKeyboardComponent::blackNoteColourId, juce::Colour (0xff1b1f2a));
    keyboard.setColour (juce::MidiKeyboardComponent::keySeparatorLineColourId, juce::Colour (0xff9aa1b2));
    keyboard.setColour (juce::MidiKeyboardComponent::mouseOverKeyOverlayColourId, colours::wave.withAlpha (0.3f));
    keyboard.setColour (juce::MidiKeyboardComponent::keyDownOverlayColourId, colours::wave.withAlpha (0.8f));
    keyboard.setColour (juce::MidiKeyboardComponent::shadowColourId, juce::Colours::transparentBlack);

    addSection ("OSCILLATORS", colours::oscillators,
                { addChoice ("osc1Wave", "Osc 1 wave"), addChoice ("osc2Wave", "Osc 2 wave"),
                  addKnob ("osc2Semi", "Semitones"), addKnob ("osc2Fine", "Fine"), addKnob ("oscMix", "Mix") });
    addSection ("QUANTUM FILTER", colours::filter,
                { addChoice ("filterMode", "Mode"), addKnob ("barrier", "Barrier"), addKnob ("width", "Width"),
                  addKnob ("gap", "Gap"), addKnob ("envAmount", "Env amount"), addKnob ("keyTrack", "Key track") });
    addSection ("AMP ENVELOPE", colours::ampEnvelope,
                { addKnob ("ampA", "Attack"), addKnob ("ampD", "Decay"), addKnob ("ampS", "Sustain"), addKnob ("ampR", "Release") });
    addSection ("FILTER ENVELOPE", colours::filterEnvelope,
                { addKnob ("filtA", "Attack"), addKnob ("filtD", "Decay"), addKnob ("filtS", "Sustain"), addKnob ("filtR", "Release") });
    addSection ("LFO", colours::lfo,
                { addChoice ("lfoWave", "Wave"), addChoice ("lfoTarget", "Target"),
                  addKnob ("lfoRate", "Rate"), addKnob ("lfoAmount", "Amount") });
    addSection ("MIDI AND OUTPUT", colours::midi,
                { addChoice ("ccMap", "CC map"), addKnob ("bendRange", "Bend"), addKnob ("gain", "Gain") });

    lastControl.setText ("Turn a minilogue xd knob to see what it controls", juce::dontSendNotification);
    lastControl.setJustificationType (juce::Justification::centred);
    lastControl.setColour (juce::Label::textColourId, colours::dimText);
    lastControl.setFont (juce::FontOptions (14.0f));
    lastControl.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (lastControl);

    setResizable (true, false);  // the host or standalone window frame does the resizing
    setResizeLimits (designWidth * 3 / 4, designHeight * 3 / 4, designWidth * 2, designHeight * 2);
    getConstrainer()->setFixedAspectRatio ((double) designWidth / designHeight);
    setSize (designWidth, designHeight);
    startTimerHz (10);
}

QuantumSynthEditor::~QuantumSynthEditor()
{
    setLookAndFeel (nullptr);
}

void QuantumSynthEditor::timerCallback()
{
    // Show which synth parameter the last minilogue xd knob moved, so the mapping is easy to learn.
    const int cc = synth.getLastXdControl();
    if (cc != shownControl)
    {
        shownControl = cc;
        if (const auto* c = qs::xd::findControl (cc))
            if (auto* param = synth.parameters.getParameter (c->paramId))
            {
                lastControl.setText (juce::String ("minilogue xd  ") + c->panelName + juce::String (juce::CharPointer_UTF8 ("  \xe2\x86\x92  "))
                                         + param->getName (64),
                                     juce::dontSendNotification);
                lastControl.setColour (juce::Label::textColourId, colours::text);
                repaint (statusPill.toFloat().transformedBy (juce::AffineTransform::scale (scale)).getSmallestIntegerContainer());
            }
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
    g.addTransform (juce::AffineTransform::scale (scale));

    // Header: name on the left, minilogue xd status on the right.
    auto title = header;
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (24.0f, juce::Font::bold));
    g.drawText ("QUANTUM SYNTH", title.removeFromLeft (220), juce::Justification::centredLeft);
    g.setColour (colours::dimText);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("tunnelling filter synth", title.removeFromLeft (220), juce::Justification::centredLeft);

    const bool xdSeen = shownControl >= 0;
    g.setColour (xdSeen ? colours::midi.withAlpha (0.18f) : colours::panel);
    g.fillRoundedRectangle (statusPill.toFloat(), (float) statusPill.getHeight() * 0.5f);
    g.setColour (xdSeen ? colours::midi : colours::panelEdge);
    g.drawRoundedRectangle (statusPill.toFloat().reduced (0.5f), (float) statusPill.getHeight() * 0.5f, 1.0f);

    for (const auto& section : sections)
    {
        const auto bounds = section.bounds.toFloat();
        g.setColour (colours::panel);
        g.fillRoundedRectangle (bounds, 8.0f);
        g.setColour (colours::panelEdge);
        g.drawRoundedRectangle (bounds.reduced (0.5f), 8.0f, 1.0f);

        // Accent strip and title.
        g.setColour (section.accent);
        g.fillRoundedRectangle (bounds.getX() + 14.0f, bounds.getY() + 14.0f, 4.0f, 16.0f, 2.0f);
        g.setFont (juce::FontOptions (15.0f, juce::Font::bold));
        g.drawText (section.title, section.bounds.reduced (26, 0).withTrimmedTop (11).withHeight (22), juce::Justification::centredLeft);
    }
}

void QuantumSynthEditor::layOutRow (juce::Rectangle<int> row, std::initializer_list<size_t> sectionIndices)
{
    // Sections share the row in proportion to their content, so every row is flush at both ends.
    constexpr int gap = 12;
    int natural = 0;
    for (auto index : sectionIndices)
        natural += sections[index].naturalWidth();
    const int available = row.getWidth() - gap * ((int) sectionIndices.size() - 1);

    size_t n = 0;
    for (auto index : sectionIndices)
    {
        auto& section = sections[index];
        const bool last = ++n == sectionIndices.size();
        const int width = last ? row.getWidth() : section.naturalWidth() * available / natural;

        section.bounds = row.removeFromLeft (width);
        row.removeFromLeft (gap);
        section.layOutControls (section.bounds.reduced (14, 10).withTrimmedTop (30));
    }
}

void QuantumSynthEditor::resized()
{
    scale = (float) getWidth() / (float) designWidth;
    for (auto* child : getChildren())
        child->setTransform (juce::AffineTransform::scale (scale));

    auto area = juce::Rectangle<int> (designWidth, designHeight).reduced (14);

    header = area.removeFromTop (40);
    statusPill = header.withLeft (header.getRight() - 440).withSizeKeepingCentre (440, 32);
    lastControl.setBounds (statusPill.reduced (14, 0));
    area.removeFromTop (8);

    keyboard.setBounds (area.removeFromBottom (76));
    keyboard.setKeyWidth ((float) keyboard.getWidth() / 43.0f);  // 43 white keys from C1 to C7
    area.removeFromBottom (12);

    // Display, with the MIDI and output section beside it.
    auto top = area.removeFromTop (208);
    auto& midi = sections[5];
    midi.bounds = top.removeFromRight (2 * 84 + 2 * 14 + 20);
    top.removeFromRight (12);
    display.setBounds (top);
    {
        auto inner = midi.bounds.reduced (14, 10).withTrimmedTop (30);
        // The CC map drop-down on top (its empty value row overlaps the knob labels below
        // and lets clicks through), the Bend and Gain knobs underneath.
        midi.controls[0]->setBounds (inner.withHeight (Control::labelHeight + Control::boxHeight + Control::valueHeight));
        inner.removeFromTop (Control::labelHeight + Control::boxHeight + 6);
        midi.controls[1]->setBounds (inner.removeFromLeft (inner.getWidth() / 2));
        midi.controls[2]->setBounds (inner);
    }
    area.removeFromTop (12);

    const int rowHeight = (area.getHeight() - 12) / 2;
    layOutRow (area.removeFromTop (rowHeight), { 0, 1 });
    area.removeFromTop (12);
    layOutRow (area.removeFromTop (rowHeight), { 2, 3, 4 });
}
