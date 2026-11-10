// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#pragma once

#include "KerrFloatLoop.h"

#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include <array>
#include <atomic>
#include <optional>

// AKerrStinn (original/KerrStinn.cpp). The engine runs on a worker thread:
// every parameter change reprocesses the whole loop, far too slow for the
// audio thread. The worker publishes immutable snapshots of the processed
// loop, which the audio thread and the editor pick up.
class KerrStinnProcessor : public juce::AudioProcessor,
                           private juce::Thread
{
public:
    struct Snapshot : juce::ReferenceCountedObject
    {
        using Ptr = juce::ReferenceCountedObjectPtr<Snapshot>;
        std::array<std::vector<float>, 2> processed; // numFrames + 1 (guard sample)
        int numFrames = 0;
        int numSlices = 2;
        bool loaded = false;
    };

    enum Status { statusReady, statusLoaded, statusBadFile };

    static constexpr const char* paramIDs[kerr::kNumParams] { "random", "slices", "bisyness", "generator", "bars", "onoff" };

    KerrStinnProcessor();
    ~KerrStinnProcessor() override;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override;
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override { return true; }

    const juce::String getName() const override { return "KerrStinn"; }
    bool acceptsMidi() const override { return true; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }

    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return "Init"; }
    void changeProgramName (int, const juce::String&) override {}

    void getStateInformation (juce::MemoryBlock&) override;
    void setStateInformation (const void*, int) override;

    // Editor interface
    void loadLoop (const juce::File& file);
    Snapshot::Ptr getLatestSnapshot() const;
    int getLoadCount() const { return loadCount.load(); }
    Status getStatus() const { return (Status) status.load(); }
    juce::String getLoopFileName() const;
    kerr::Rand::Flavour getRandFlavour() const { return (kerr::Rand::Flavour) randFlavour.load(); }
    void setRandFlavour (kerr::Rand::Flavour f);
    juce::AudioFormatManager& getFormatManager() { return formats; }

    // Blocks until the worker has handled every pending request (tests).
    void waitUntilIdle();

    juce::AudioProcessorValueTreeState apvts;

private:
    struct LoopSource
    {
        juce::String path;
        juce::MemoryBlock data;
    };

    struct Restore
    {
        std::array<float, kerr::kNumParams> values;
        LoopSource source;
        kerr::Rand::Flavour flavour;
    };

    static juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

    void run() override;
    void loadIntoEngine (const LoopSource& source);
    void publish();

    // Worker-thread-only state
    kerr::FloatKerrStinn kerrStinn;
    kerr::FloatLoop loop;
    std::array<float, kerr::kNumParams> applied {};
    std::vector<Snapshot::Ptr> releasePool;

    // Requests from the message thread
    juce::CriticalSection requestLock;
    std::optional<LoopSource> pendingLoad;
    std::optional<Restore> pendingRestore;
    bool pendingRecompute = false;
    std::atomic<bool> pollingSuspended { false };
    std::atomic<bool> busy { false };

    // Published state
    juce::SpinLock snapshotLock;
    Snapshot::Ptr latest;
    Snapshot::Ptr audioSnapshot;
    std::atomic<int> loadCount { 0 }, status { statusReady };
    std::atomic<int> randFlavour { (int) kerr::Rand::kMac };

    mutable juce::CriticalSection sourceLock;
    LoopSource currentSource;

    std::array<std::atomic<float>*, kerr::kNumParams> rawParams {};
    juce::AudioFormatManager formats;

    // Transport, as tracked by the original's preProcess()
    double barPhase = 0.0, phasePerSample = 0.0, lastPpq = -1.0;
    long bar = 0;
    bool transportMoving = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (KerrStinnProcessor)
};
