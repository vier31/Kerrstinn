// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PluginEditor.h"
#include "BinaryData.h"

namespace
{
constexpr int editorWidth = 482;
constexpr int editorHeight = 254;

const char* const titleText = "KERRSTINN: WEAPON OF BREAK CONSTRUCTION";

juce::Font labelFont()
{
    return juce::Font (juce::FontOptions ("Helvetica", 12.0f, juce::Font::bold));
}

juce::Image png (const void* data, int size)
{
    return juce::ImageCache::getFromMemory (data, size);
}
} // namespace

//==============================================================================
// CAnimKnob: picks a frame from a vertical strip of 60x60 frames (bmp00129).
class KerrStinnEditor::FilmstripKnob : public juce::Slider
{
public:
    FilmstripKnob()
        : juce::Slider (juce::Slider::RotaryHorizontalVerticalDrag, juce::Slider::NoTextBox),
          strip (png (BinaryData::bmp00129_png, BinaryData::bmp00129_pngSize))
    {
        setMouseDragSensitivity (200);
        setVelocityBasedMode (false);
        setPopupDisplayEnabled (false, false, nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        const int frameSize = strip.getWidth();
        const int frames = strip.getHeight() / frameSize;
        const auto norm = (float) valueToProportionOfLength (getValue());
        const int frame = juce::jlimit (0, frames - 1, (int) (norm * (float) (frames - 1) + 0.5f));
        g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, frame * frameSize, frameSize, frameSize);
    }

private:
    juce::Image strip;
};

//==============================================================================
// COnOffButton: 40x40 frames, off on top, on below (bmp00131).
class KerrStinnEditor::OnOffButton : public juce::Button
{
public:
    OnOffButton()
        : juce::Button ("On/Off"),
          strip (png (BinaryData::bmp00131_png, BinaryData::bmp00131_pngSize))
    {
        setClickingTogglesState (true);
    }

    void paintButton (juce::Graphics& g, bool, bool) override
    {
        const int h = strip.getHeight() / 2;
        g.drawImage (strip, 0, 0, getWidth(), getHeight(), 0, getToggleState() ? h : 0, strip.getWidth(), h);
    }

private:
    juce::Image strip;
};

//==============================================================================
// COscilloscope: both channels of the processed loop as min/max lines plus
// white slice markers. Click to open a loop; files can be dropped on the editor.
class KerrStinnEditor::WaveDisplay : public juce::Component
{
public:
    explicit WaveDisplay (KerrStinnEditor& o)
        : owner (o), frame (png (BinaryData::bmp00132_png, BinaryData::bmp00132_pngSize))
    {
        setMouseCursor (juce::MouseCursor::PointingHandCursor);
    }

    void setSnapshot (KerrStinnProcessor::Snapshot::Ptr s)
    {
        snapshot = std::move (s);
        repaint();
    }

    void paint (juce::Graphics& g) override
    {
        g.drawImageAt (frame, 0, 0);

        if (snapshot == nullptr || snapshot->numFrames <= 0)
            return;

        const int w = frame.getWidth();
        const int h = frame.getHeight();

        // The original scales by the display's absolute centre in editor coordinates.
        const float centre = (float) (getBottom() - h / 2);
        const float samplesPerPixel = (float) snapshot->numFrames / (float) w;

        g.setColour (juce::Colours::black);
        for (const auto& data : snapshot->processed)
        {
            for (int x = 1; x < w - 1; ++x)
            {
                float hi = 0.0f, lo = 0.0f;
                for (int k = 0; (float) k < samplesPerPixel; ++k)
                {
                    const int idx = (int) ((float) x * samplesPerPixel) - k;
                    const float v = (idx >= 0 && idx < (int) data.size()) ? data[(size_t) idx] : 0.0f;
                    hi = std::max (hi, v);
                    lo = std::min (lo, v);
                }
                const int yHi = (int) (hi * 0.9f * centre + centre) - getY();
                const int yLo = (int) (centre + lo * 0.9f * centre) - getY();
                g.drawVerticalLine (x, (float) std::min (yHi, yLo), (float) std::max (yHi, yLo) + 1.0f);
            }
        }

        g.setColour (juce::Colours::white);
        const int slices = snapshot->numSlices;
        for (int i = 1; i < slices; ++i)
        {
            const int x = (int) (((float) w / (float) slices) * (float) i);
            g.drawVerticalLine (x, 10.0f, (float) (h - 10));
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        if (e.mods.isPopupMenu())
            owner.showMenu();
        else
            owner.openFileChooser();
    }

private:
    KerrStinnEditor& owner;
    juce::Image frame;
    KerrStinnProcessor::Snapshot::Ptr snapshot;
};

//==============================================================================
KerrStinnEditor::KerrStinnEditor (KerrStinnProcessor& p)
    : juce::AudioProcessorEditor (p),
      kerr (p),
      background (png (BinaryData::bmp00128_png, BinaryData::bmp00128_pngSize))
{
    content.setInterceptsMouseClicks (false, true);
    addAndMakeVisible (content);

    wave = std::make_unique<WaveDisplay> (*this);
    wave->setBounds ((editorWidth - 422) / 2, 10, 422, 82);
    content.addAndMakeVisible (*wave);

    // Knob order on screen: Random, Slices, Bisyness, Bars, Generator.
    const int params[] { kerr::kRand, kerr::kSlices, kerr::kBisyness, kerr::kNumBars, kerr::kGenerationMatrix };
    const int xs[] { 50, 120, 190, 260, 375 };
    for (size_t i = 0; i < knobs.size(); ++i)
    {
        knobs[i] = std::make_unique<FilmstripKnob>();
        knobs[i]->setBounds (xs[i], 110, 60, 60);
        content.addAndMakeVisible (*knobs[i]);
        sliderAttachments.push_back (std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            kerr.apvts, KerrStinnProcessor::paramIDs[params[i]], *knobs[i]));
    }

    onOff = std::make_unique<OnOffButton>();
    onOff->setBounds (330, 130, 40, 40);
    content.addAndMakeVisible (*onOff);
    buttonAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        kerr.apvts, KerrStinnProcessor::paramIDs[kerr::kOn], *onOff);

    statusText = kerr.getStatus() == KerrStinnProcessor::statusBadFile ? "No error yet" : "Kerstinn ready to GOGOGO";
    seenLoadCount = kerr.getLoadCount();

    setResizable (true, true);
    setResizeLimits (editorWidth, editorHeight, editorWidth * 3, editorHeight * 3);
    if (auto* c = getConstrainer())
        c->setFixedAspectRatio ((double) editorWidth / (double) editorHeight);
    setSize (editorWidth, editorHeight);

    timerCallback();
    startTimerHz (30);
}

KerrStinnEditor::~KerrStinnEditor()
{
    stopTimer();
}

void KerrStinnEditor::resized()
{
    content.setBounds (0, 0, editorWidth, editorHeight);
    content.setTransform (juce::AffineTransform::scale ((float) getWidth() / (float) editorWidth));
}

void KerrStinnEditor::paint (juce::Graphics& g)
{
    g.addTransform (content.getTransform());
    g.drawImageAt (background, 0, 0);

    auto value = [this] (int index)
    {
        auto* param = kerr.apvts.getParameter (KerrStinnProcessor::paramIDs[index]);
        return param->getCurrentValueAsText();
    };

    struct Column { int x0, x1; int param; const char* name; };
    const Column columns[] {
        { 50, 120, kerr::kRand, "Random" },
        { 120, 190, kerr::kSlices, "Slices" },
        { 190, 260, kerr::kBisyness, "Bisyness" },
        { 260, 330, kerr::kNumBars, "Bars" },
        { 352, 467, kerr::kGenerationMatrix, "Generator" },
    };

    g.setColour (juce::Colours::black);
    g.setFont (labelFont());
    for (const auto& c : columns)
    {
        g.drawText (value (c.param), c.x0, 180, c.x1 - c.x0, 12, juce::Justification::centred, false);
        g.drawText (c.name, c.x0, 200, c.x1 - c.x0, 12, juce::Justification::centred, false);
    }

    g.drawText (statusText, 0, editorHeight - 20, editorWidth, 20, juce::Justification::centred, false);
}

void KerrStinnEditor::timerCallback()
{
    const int loads = kerr.getLoadCount();
    if (loads != seenLoadCount)
    {
        // KerrStinnEditor::valueChanged() after a load. Its if/else chain only
        // ever showed these two.
        seenLoadCount = loads;
        statusText = kerr.getStatus() == KerrStinnProcessor::statusBadFile ? "Bad bad wave, misbehaving!!!" : titleText;
    }

    auto snap = kerr.getLatestSnapshot();
    if (snap != nullptr && snap.get() != lastSnapshot)
    {
        lastSnapshot = snap.get();
        wave->setSnapshot (snap);
    }

    juce::String values = statusText;
    for (auto* id : KerrStinnProcessor::paramIDs)
        values << "|" << kerr.apvts.getParameter (id)->getCurrentValueAsText();

    if (values != shownValues)
    {
        shownValues = values;
        repaint (getLocalArea (&content, juce::Rectangle<int> (0, 175, editorWidth, editorHeight - 175)));
    }
}

void KerrStinnEditor::mouseDown (const juce::MouseEvent& e)
{
    if (e.mods.isPopupMenu())
        showMenu();
}

void KerrStinnEditor::openFileChooser()
{
    chooser = std::make_unique<juce::FileChooser> ("Select a File to Open", juce::File(),
                                                   kerr.getFormatManager().getWildcardForAllFormats());
    chooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                          [this] (const juce::FileChooser& fc)
                          {
                              const auto file = fc.getResult();
                              if (file.existsAsFile())
                                  kerr.loadLoop (file);
                          });
}

void KerrStinnEditor::showMenu()
{
    juce::PopupMenu menu;
    menu.addItem (1, "Load loop...");
    menu.addSeparator();
    menu.addSectionHeader ("Random patterns as in");
    const bool windows = kerr.getRandFlavour() == kerr::Rand::kWindows;
    menu.addItem (2, "Mac version (2007)", true, ! windows);
    menu.addItem (3, "Windows version (2007)", true, windows);

    const auto name = kerr.getLoopFileName();
    if (name.isNotEmpty())
    {
        menu.addSeparator();
        menu.addItem (4, "Loop: " + name, false, false);
    }

    menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
                        [this] (int result)
                        {
                            if (result == 1)
                                openFileChooser();
                            else if (result == 2)
                                kerr.setRandFlavour (kerr::Rand::kMac);
                            else if (result == 3)
                                kerr.setRandFlavour (kerr::Rand::kWindows);
                        });
}

bool KerrStinnEditor::isInterestedInFileDrag (const juce::StringArray& files)
{
    const auto wildcard = kerr.getFormatManager().getWildcardForAllFormats();
    for (const auto& f : files)
        for (const auto& pattern : juce::StringArray::fromTokens (wildcard, ";", {}))
            if (juce::File (f).getFileName().matchesWildcard (pattern.trim(), true))
                return true;
    return false;
}

void KerrStinnEditor::filesDropped (const juce::StringArray& files, int, int)
{
    if (! files.isEmpty())
        kerr.loadLoop (juce::File (files[0]));
}
