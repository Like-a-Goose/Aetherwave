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
    if (qs::isSpectral (settings.mode))
        paintLevels (g, area);
    else
        paintScattering (g, area);
}

// Scattering modes: the potential and the exact wavefunction of a particle at E = 0.6 V0
// coming in from the left (solved with the same transfer matrices as the filter).
void BarrierDisplay::paintScattering (juce::Graphics& g, juce::Rectangle<float> area)
{
    paintHeading (g, area, "Potential and wavefunction");

    qs::Potential potential;
    if (settings.mode == qs::FilterMode::softTunnel)
    {
        // Draw the smooth barrier as a fine staircase of the same sech^2 shape.
        const double alpha = qs::softAlpha (settings.width), half = 3.0 / alpha;
        const int slices = qs::Potential::maxSegments;
        for (int n = 0; n < slices; ++n)
        {
            const double c = std::cosh (alpha * (-half + (n + 0.5) * 2.0 * half / slices));
            potential.add ({ 2.0 * half / slices, 1.0 / (c * c) });
        }
    }
    else
    {
        potential = qs::potentialFor (settings.mode, settings.width, settings.gap);
    }

    const double span = potential.length();
    const double margin = juce::jmax (6.0, span * 0.25);
    const double x0 = -margin, x1 = span + margin;
    const double energy = 0.6;

    // Vertical scale: from a bit below V = 0 up to V0 (a deep well runs off the bottom).
    double vMin = -0.45;
    for (int j = 0; j < potential.count; ++j)
        vMin = juce::jlimit (-1.0, vMin, potential.segments[(size_t) j].potential);
    const float potentialTop = area.getY() + 26.0f, potentialBottom = area.getBottom() - 4.0f;
    auto toScreenX = [&] (double x) { return area.getX() + (float) ((x - x0) / (x1 - x0)) * area.getWidth(); };
    auto toScreenY = [&] (double v)
    {
        return juce::jmap ((float) juce::jmax (v, vMin - 0.2), (float) vMin, 1.0f, potentialBottom, potentialTop);
    };
    const float zeroY = toScreenY (0.0);

    // The potential, filled down to (or up to) V = 0.
    juce::Path shape;
    shape.startNewSubPath (toScreenX (x0), zeroY);
    double x = 0.0;
    shape.lineTo (toScreenX (0.0), zeroY);
    for (int j = 0; j < potential.count; ++j)
    {
        const auto& seg = potential.segments[(size_t) j];
        shape.lineTo (toScreenX (x), toScreenY (seg.potential));
        x += seg.length;
        shape.lineTo (toScreenX (x), toScreenY (seg.potential));
    }
    shape.lineTo (toScreenX (x), toScreenY (potential.right));
    shape.lineTo (toScreenX (x1), toScreenY (potential.right));
    shape.lineTo (toScreenX (x1), zeroY);
    shape.closeSubPath();
    g.setColour (colours::barrier.withAlpha (0.45f));
    g.fillPath (shape);

    g.setColour (colours::grid);
    g.drawHorizontalLine ((int) zeroY, area.getX(), area.getRight());
    const float energyY = toScreenY (energy);
    const float dashes[] = { 4.0f, 4.0f };
    g.drawDashedLine ({ area.getX(), energyY, area.getRight(), energyY }, dashes, 2, 1.0f);

    // Real part of psi, drawn around the particle's energy line.
    std::vector<double> xs;
    const int steps = juce::jmax (100, (int) area.getWidth() * 2);
    for (int i = 0; i <= steps; ++i)
        xs.push_back (x0 + (x1 - x0) * i / steps);
    const auto psi = qs::wavefunction (energy, potential, xs);

    double peak = 1.0e-9;
    for (double v : psi)
        peak = std::max (peak, std::abs (v));
    const float amplitude = juce::jmin (energyY - area.getY() - 24.0f, area.getBottom() - energyY - 2.0f) / (float) peak;

    juce::Path wave;
    for (size_t i = 0; i < psi.size(); ++i)
    {
        const auto point = juce::Point<float> (toScreenX (xs[i]), energyY - (float) psi[i] * amplitude);
        if (i == 0)
            wave.startNewSubPath (point);
        else
            wave.lineTo (point);
    }
    g.setColour (colours::wave);
    g.strokePath (wave, juce::PathStrokeType (2.2f));

    const auto result = settings.mode == qs::FilterMode::softTunnel
                            ? qs::scatterSoft (energy, qs::softAlpha (settings.width))
                            : qs::scatter (energy, potential);
    g.setColour (colours::text);
    g.setFont (juce::FontOptions (14.0f));
    g.drawText ("tunnelled " + juce::String (100.0 * result.transmitted * result.transmitted, 1) + "%",
                area.withHeight (18.0f), juce::Justification::topRight);
}

// Bound modes: the confining potential, its energy levels and (for the box and the
// oscillator) the first few eigenstates drawn on their levels.
void BarrierDisplay::paintLevels (juce::Graphics& g, juce::Rectangle<float> area)
{
    paintHeading (g, area, "Energy levels");
    area.removeFromTop (6.0f);
    area.removeFromBottom (4.0f);

    const auto mode = settings.mode;
    const int levels = mode == qs::FilterMode::hydrogen ? 7 : 4;

    // Energy of level n, and the energy range shown (bottom of the potential to the top).
    auto levelEnergy = [mode] (int n) -> double
    {
        if (mode == qs::FilterMode::box) return (double) (n + 1) * (n + 1);
        if (mode == qs::FilterMode::oscillator) return n + 0.5;
        return -1.0 / ((double) (n + 1) * (n + 1));  // hydrogen, ionisation limit at 0
    };
    const double eLow = mode == qs::FilterMode::hydrogen ? -1.15 : 0.0;
    const double eHigh = mode == qs::FilterMode::box ? 18.0 : (mode == qs::FilterMode::oscillator ? 4.4 : 0.15);
    auto toY = [&] (double e) { return area.getBottom() - (float) ((e - eLow) / (eHigh - eLow)) * area.getHeight(); };
    auto toX = [&] (double u) { return area.getCentreX() + (float) u * area.getWidth() * 0.5f; };  // u in -1..1

    // The potential.
    g.setColour (colours::barrier.withAlpha (0.8f));
    juce::Path wall;
    if (mode == qs::FilterMode::box)
    {
        const float left = toX (-0.7), right = toX (0.7);
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (left - 6.0f, area.getY(), left, area.getBottom()));
        g.fillRect (juce::Rectangle<float>::leftTopRightBottom (right, area.getY(), right + 6.0f, area.getBottom()));
        g.drawHorizontalLine ((int) area.getBottom(), left, right);
    }
    else
    {
        for (int i = 0; i <= 200; ++i)
        {
            const double u = -1.0 + 2.0 * i / 200.0;
            const double v = mode == qs::FilterMode::oscillator ? 4.4 * u * u
                                                                : -0.08 / juce::jmax (0.012, std::abs (u));
            const auto point = juce::Point<float> (toX (u), toY (juce::jlimit (eLow, eHigh, v)));
            if (i == 0)
                wall.startNewSubPath (point);
            else
                wall.lineTo (point);
        }
        g.strokePath (wall, juce::PathStrokeType (2.0f));
    }

    // Levels, and eigenstates drawn on them.
    for (int n = 0; n < levels; ++n)
    {
        const double e = levelEnergy (n);
        const float y = toY (e);

        // How far the level reaches: the classically allowed region.
        double reach = 0.7;
        if (mode == qs::FilterMode::oscillator)
            reach = std::sqrt (e / 4.4);
        else if (mode == qs::FilterMode::hydrogen)
            reach = juce::jmin (1.0, 0.08 / -e);

        g.setColour (colours::label.withAlpha (0.6f));
        g.drawHorizontalLine ((int) y, toX (-reach), toX (reach));

        if (mode == qs::FilterMode::hydrogen)
            continue;

        std::vector<juce::Point<double>> samples;  // (u, psi)
        for (int i = 0; i <= 160; ++i)
        {
            const double u = -1.0 + 2.0 * i / 160.0;
            double psi;
            if (mode == qs::FilterMode::box)
            {
                if (std::abs (u) > 0.7)
                    continue;
                psi = std::sin ((n + 1) * 3.14159265358979323846 * (u + 0.7) / 1.4);
            }
            else
            {
                // Hermite-Gauss functions. The parabola drawn is 4.4 u^2 = xi^2 / 2 in units of hbar omega.
                if (4.4 * u * u > e + 0.8)
                    continue;  // stop a little past the turning points, where the tails die out
                const double xi = u * std::sqrt (8.8);
                double h0 = 1.0, h1 = 2.0 * xi;
                double h = n == 0 ? h0 : h1;
                for (int k = 1; k < n; ++k)
                {
                    h = 2.0 * xi * h1 - 2.0 * k * h0;
                    h0 = h1;
                    h1 = h;
                }
                psi = h * std::exp (-0.5 * xi * xi);
            }
            samples.push_back ({ u, psi });
        }

        double peak = 1.0e-9;
        for (const auto& p : samples)
            peak = std::max (peak, std::abs (p.y));
        const float amplitude = (float) (toY (0.0) - toY (mode == qs::FilterMode::box ? 1.4 : 0.4));

        juce::Path state;
        for (size_t i = 0; i < samples.size(); ++i)
        {
            const auto point = juce::Point<float> (toX (samples[i].x), y - (float) (samples[i].y / peak) * amplitude);
            if (i == 0)
                state.startNewSubPath (point);
            else
                state.lineTo (point);
        }
        g.setColour (colours::wave);
        g.strokePath (state, juce::PathStrokeType (2.0f));
    }

    g.setColour (colours::text);
    g.setFont (juce::FontOptions (14.0f));
    const char* rule = mode == qs::FilterMode::box ? "lines at n\u00b2 \u00d7 barrier"
                     : mode == qs::FilterMode::oscillator ? "lines at (n + \u00bd) \u00d7 barrier"
                                                          : "lines at (1 \u2212 1/n\u00b2) \u00d7 barrier";
    g.drawText (juce::String (juce::CharPointer_UTF8 (rule)), area.withHeight (18.0f).translated (0.0f, -28.0f),
                juce::Justification::topRight);
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

//==============================================================================
PresetBar::PresetBar (PresetManager& manager) : presets (manager)
{
    // Arrows drawn as text so they scale with the rest of the editor.
    previous.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x97\x80")));
    next.setButtonText (juce::String (juce::CharPointer_UTF8 ("\xe2\x96\xb6")));
    previous.setTooltip ("Previous preset");
    next.setTooltip ("Next preset");
    browse.setTooltip ("Browse presets");
    save.setTooltip ("Save the current sound as a preset");

    previous.onClick = [this] { step (-1); };
    next.onClick = [this] { step (1); };
    browse.onClick = [this] { showMenu(); };
    save.onClick = [this] { nameEditor.isVisible() ? finishSave (true) : beginSave(); };

    nameEditor.setFont (juce::FontOptions (15.0f));
    nameEditor.setTextToShowWhenEmpty ("Preset name", colours::dimText);
    nameEditor.setIndents (8, 6);
    nameEditor.onReturnKey = [this] { finishSave (true); };
    nameEditor.onEscapeKey = [this] { finishSave (false); };
    nameEditor.onFocusLost = [this] { finishSave (false); };

    for (auto* b : { &previous, &next, &browse, &save })
        addAndMakeVisible (*b);
    addChildComponent (nameEditor);

    refresh();
    startTimerHz (5);
}

void PresetBar::resized()
{
    auto area = getLocalBounds();
    save.setBounds (area.removeFromRight (64));
    area.removeFromRight (8);
    previous.setBounds (area.removeFromLeft (32));
    next.setBounds (area.removeFromRight (32));
    area.reduce (6, 0);
    browse.setBounds (area);
    nameEditor.setBounds (area);
}

void PresetBar::timerCallback()
{
    refresh();
}

void PresetBar::refresh()
{
    // The name of what is loaded, marked once it has been changed and not saved.
    auto text = presets.getCurrentName();
    if (presets.isModified())
        text += "  (edited)";
    if (text != shownText)
    {
        shownText = text;
        browse.setButtonText (text);
    }
}

void PresetBar::step (int delta)
{
    const auto entries = presets.getEntries();
    if (entries.isEmpty())
        return;
    const int current = presets.indexOfCurrent (entries);
    const int target = current < 0 ? 0 : (current + delta + entries.size()) % entries.size();
    presets.load (entries[target]);
    refresh();
}

void PresetBar::showMenu()
{
    enum { saveId = 100000, deleteId, folderId };

    const auto entries = presets.getEntries();
    const int current = presets.indexOfCurrent (entries);

    juce::PopupMenu menu;
    juce::String category;
    for (int i = 0; i < entries.size(); ++i)
    {
        if (entries[i].category != category)
        {
            category = entries[i].category;
            menu.addSectionHeader (category == "User" ? "Your presets" : category);
        }
        menu.addItem (i + 1, entries[i].name, true, i == current);
    }
    if (category != "User")
    {
        menu.addSectionHeader ("Your presets");
        menu.addItem (-1, "Saved presets show up here", false, false);
    }

    const bool userPresetLoaded = current >= 0 && ! entries[current].isFactory();
    menu.addSeparator();
    menu.addItem (saveId, "Save as new preset...");
    menu.addItem (deleteId, "Delete \"" + presets.getCurrentName() + "\"", userPresetLoaded);
    menu.addItem (folderId, "Show presets folder");

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (browse),
                        [this, entries, current] (int result)
                        {
                            if (result == saveId)
                                beginSave();
                            else if (result == deleteId)
                                presets.deleteUserPreset (entries[current]);
                            else if (result == folderId)
                            {
                                const auto folder = PresetManager::getUserFolder();
                                folder.createDirectory();
                                folder.startAsProcess();
                            }
                            else if (result > 0 && result <= entries.size())
                                presets.load (entries[result - 1]);
                            refresh();
                        });
}

void PresetBar::beginSave()
{
    const bool userPreset = presets.getCurrentFactoryIndex() < 0;
    nameEditor.setText (userPreset ? presets.getCurrentName() : juce::String(), false);
    nameEditor.selectAll();
    nameEditor.setVisible (true);
    browse.setVisible (false);
    nameEditor.grabKeyboardFocus();
}

void PresetBar::finishSave (bool commit)
{
    if (! nameEditor.isVisible())
        return;

    const auto name = nameEditor.getText().trim();
    nameEditor.setVisible (false);
    browse.setVisible (true);
    if (commit && name.isNotEmpty())
        presets.saveUserPreset (name);
    refresh();
}

//==============================================================================
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
    : AudioProcessorEditor (&p), synth (p), display (p), presetBar (p.presets),
      keyboard (p.keyboardState, juce::MidiKeyboardComponent::horizontalKeyboard)
{
    setLookAndFeel (&lookAndFeel);

    addAndMakeVisible (display);
    addAndMakeVisible (presetBar);
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
    auto* modeChoice = addChoice ("filterMode", "Mode");
    modeChoice->preferredWidth = 172;  // room for "Harmonic Oscillator"
    gapControl = addKnob ("gap", "Gap");
    addSection ("QUANTUM FILTER", colours::filter,
                { modeChoice, addKnob ("barrier", "Barrier"), addKnob ("width", "Width"),
                  gapControl, addKnob ("envAmount", "Env amount"), addKnob ("keyTrack", "Key track") });
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

    // Gap only matters for modes with more than one barrier.
    const auto mode = (qs::FilterMode) (int) synth.parameters.getRawParameterValue ("filterMode")->load();
    gapControl->setAlpha (qs::usesGap (mode) ? 1.0f : 0.4f);

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
    g.drawText ("QUANTUM SYNTH", title.removeFromLeft (titleWidth), juce::Justification::centredLeft);
    g.setColour (colours::dimText);
    g.setFont (juce::FontOptions (15.0f));
    g.drawText ("tunnelling filter synth", title.removeFromLeft (subtitleWidth), juce::Justification::centredLeft);

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
    statusPill = header.withLeft (header.getRight() - 380).withSizeKeepingCentre (380, 32);
    lastControl.setBounds (statusPill.reduced (14, 0));
    presetBar.setBounds (juce::Rectangle<int>::leftTopRightBottom (header.getX() + titleWidth + subtitleWidth, statusPill.getY(),
                                                                    statusPill.getX() - 16, statusPill.getBottom()));
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
