// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "PluginProcessor.h"

#include <juce_gui_basics/juce_gui_basics.h>

// original/KerrStinnEditor.cpp: same artwork, layout and texts.
class KerrStinnEditor : public juce::AudioProcessorEditor,
                        public juce::FileDragAndDropTarget,
                        private juce::Timer
{
public:
    explicit KerrStinnEditor (KerrStinnProcessor&);
    ~KerrStinnEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    void mouseDown (const juce::MouseEvent&) override;

    bool isInterestedInFileDrag (const juce::StringArray&) override;
    void filesDropped (const juce::StringArray&, int, int) override;

private:
    class FilmstripKnob;
    class OnOffButton;
    class WaveDisplay;

    void timerCallback() override;
    void openFileChooser();
    void showMenu();

    KerrStinnProcessor& kerr;
    juce::Component content;
    juce::Image background;

    std::unique_ptr<WaveDisplay> wave;
    std::array<std::unique_ptr<FilmstripKnob>, 5> knobs;
    std::unique_ptr<OnOffButton> onOff;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> sliderAttachments;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> buttonAttachment;

    std::unique_ptr<juce::FileChooser> chooser;
    juce::String statusText, shownValues;
    int seenLoadCount = 0;
    const void* lastSnapshot = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KerrStinnEditor)
};
