// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#include "PluginProcessor.h"
#include "PluginEditor.h"

namespace
{
constexpr int maxEmbeddedBytes = 64 * 1024 * 1024;

// AKerrStinn::getParameterDisplay(); the generator shows its name, as the
// editor did via getParameterLabel().
juce::String displayFor (int index, float v)
{
    switch (index)
    {
        case kerr::kRand:             return juce::String (kerr::seedFor (v));
        case kerr::kSlices:           return juce::String (kerr::slicesFor (v));
        case kerr::kBisyness:         return juce::String (kerr::bisynessFor (v));
        case kerr::kGenerationMatrix: return kerr::generatorName (kerr::generatorFor (v));
        case kerr::kNumBars:          return juce::String (kerr::barsFor (v));
        case kerr::kOn:               return juce::exactlyEqual (v, 0.0f) ? "OFF" : "ON";
        default:                      return {};
    }
}
} // namespace

//==============================================================================
KerrStinnProcessor::KerrStinnProcessor()
    : juce::AudioProcessor (BusesProperties().withOutput ("Output", juce::AudioChannelSet::stereo(), true)),
      juce::Thread ("KerrStinn engine"),
      apvts (*this, nullptr, "KerrStinn", createLayout())
{
    formats.registerBasicFormats();

    for (int i = 0; i < kerr::kNumParams; ++i)
    {
        rawParams[(size_t) i] = apvts.getRawParameterValue (paramIDs[i]);
        applied[(size_t) i] = kerrStinn.getParameter (i);
    }

    publish();
    startThread (juce::Thread::Priority::normal);
}

KerrStinnProcessor::~KerrStinnProcessor()
{
    stopThread (2000);
}

juce::AudioProcessorValueTreeState::ParameterLayout KerrStinnProcessor::createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;
    for (int i = 0; i < kerr::kNumParams; ++i)
    {
        layout.add (std::make_unique<juce::AudioParameterFloat> (
            juce::ParameterID { paramIDs[i], 1 }, kerr::kParamNames[i], juce::NormalisableRange<float> (0.0f, 1.0f),
            kerr::defaultValue (i),
            juce::AudioParameterFloatAttributes().withStringFromValueFunction (
                [i] (float v, int) { return displayFor (i, v); })));
    }
    return layout;
}

bool KerrStinnProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    return layouts.getMainOutputChannelSet() == juce::AudioChannelSet::stereo();
}

//==============================================================================
void KerrStinnProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::ScopedNoDenormals noDenormals;
    midi.clear();

    {
        const juce::SpinLock::ScopedTryLockType lock (snapshotLock);
        if (lock.isLocked() && audioSnapshot != latest)
            audioSnapshot = latest;
    }

    // AKerrStinn::preProcess(): the bar position from the host. A "bar" is
    // (ppqPos - barStartPos) / 4 and lasts `denominator` beats, as in the
    // original; it only fits 4/4. A transport that does not move is silent.
    if (auto* playHead = getPlayHead())
    {
        if (auto pos = playHead->getPosition())
        {
            const auto bpm = pos->getBpm();
            const auto ppq = pos->getPpqPosition();
            if (bpm && ppq && *bpm > 0.0)
            {
                const double barStart = pos->getPpqPositionOfLastBarStart().orFallback (0.0);
                const int denominator = pos->getTimeSignature().orFallback (juce::AudioPlayHead::TimeSignature {}).denominator;
                barPhase = std::max (0.0, (*ppq - barStart) / 4.0);
                phasePerSample = *bpm / (getSampleRate() * 60.0 * (double) denominator);
                bar = (long) std::max (0.0, *ppq / 4.0);
                transportMoving = ! juce::exactlyEqual (*ppq, lastPpq);
                lastPpq = *ppq;
            }
        }
    }

    const auto* snap = audioSnapshot.get();
    if (snap == nullptr || ! snap->loaded || ! transportMoving || snap->numFrames <= 0
        || juce::exactlyEqual (rawParams[kerr::kOn]->load(), 0.0f))
    {
        buffer.clear();
        return;
    }

    // processReplacing(): nearest-sample playback of the processed loop,
    // stretched over `Bars` bars.
    const int bars = kerr::barsFor (rawParams[kerr::kNumBars]->load());
    const auto* pl = snap->processed[0].data();
    const auto* pr = snap->processed[1].data();
    auto* outL = buffer.getWritePointer (0);
    auto* outR = buffer.getWritePointer (1);

    for (int i = 0; i < buffer.getNumSamples(); ++i)
    {
        const float inpos = kerr::positionFor (barPhase + phasePerSample * i, bar, bars);
        const int idx = kerr::indexFor (inpos, snap->numFrames);
        const bool valid = idx >= 0 && idx <= snap->numFrames;
        outL[i] = valid ? pl[idx] : 0.0f;
        outR[i] = valid ? pr[idx] : 0.0f;
    }
}

//==============================================================================
void KerrStinnProcessor::run()
{
    while (! threadShouldExit())
    {
        wait (4);
        busy = true;

        std::optional<Restore> restore;
        std::optional<LoopSource> load;
        bool recompute = false;
        {
            const juce::ScopedLock sl (requestLock);
            restore.swap (pendingRestore);
            load.swap (pendingLoad);
            std::swap (recompute, pendingRecompute);
        }

        bool changed = false;
        kerrStinn.engine.random().setFlavour ((kerr::Rand::Flavour) randFlavour.load());

        if (restore)
        {
            // setChunk() -> setProgram()
            kerrStinn.engine.random().setFlavour (restore->flavour);
            if (restore->source.path.isNotEmpty() || restore->source.data.getSize() > 0)
                loadIntoEngine (restore->source);
            kerrStinn.restore (restore->values.data());
            applied = restore->values;
            pollingSuspended = false;
            changed = true;
        }

        if (load)
        {
            // The editor's load: loadFile(), then setParameter (666).
            loadIntoEngine (*load);
            if (kerrStinn.engine.isAllocated())
                kerrStinn.loaded();
            changed = true;
        }

        if (recompute)
        {
            kerrStinn.setParameter (kerr::kRecompute, 0.0f);
            changed = true;
        }

        if (! pollingSuspended)
        {
            // Changes made since the last pass, in parameter order. Each one
            // is a full pass on top of the previous result, as in the original.
            for (int i = 0; i < kerr::kNumParams; ++i)
            {
                const float v = rawParams[(size_t) i]->load();
                if (! juce::exactlyEqual (v, applied[(size_t) i]))
                {
                    applied[(size_t) i] = v;
                    kerrStinn.setParameter (i, v);
                    changed = true;
                }
            }
        }

        if (changed)
            publish();

        releasePool.erase (std::remove_if (releasePool.begin(), releasePool.end(),
                                           [] (const Snapshot::Ptr& p) { return p->getReferenceCount() == 1; }),
                           releasePool.end());
        busy = false;
    }
}

void KerrStinnProcessor::loadIntoEngine (const LoopSource& source)
{
    LoopSource resolved = source;
    std::unique_ptr<juce::AudioFormatReader> reader;

    const juce::File file (source.path);
    if (source.path.isNotEmpty() && file.existsAsFile()
        && file.getSize() <= maxEmbeddedBytes && file.loadFileAsData (resolved.data))
    {
        reader.reset (formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (resolved.data, false)));
    }
    else if (source.data.getSize() > 0)
    {
        resolved.data = source.data; // file moved or missing: use the copy saved with the project
        reader.reset (formats.createReaderFor (std::make_unique<juce::MemoryInputStream> (resolved.data, false)));
    }

    const int numFrames = reader != nullptr ? (int) std::min<juce::int64> (reader->lengthInSamples, 1 << 26) : 0;

    if (numFrames < 1)
    {
        // GetWavInfo() failing leaves the engine unallocated.
        kerrStinn.engine.detach();
        status = statusBadFile;
    }
    else
    {
        const bool stereo = reader->numChannels > 1;
        juce::AudioBuffer<float> audio (stereo ? 2 : 1, numFrames);
        reader->read (&audio, 0, numFrames, 0, true, stereo);

        // 16-bit files are scaled like convertToFloat(): x / 32767.
        if (reader->bitsPerSample == 16 && ! reader->usesFloatingPointData)
            for (int ch = 0; ch < audio.getNumChannels(); ++ch)
                for (auto& x : juce::Span (audio.getWritePointer (ch), (size_t) numFrames))
                    x = kerr::fromPcm16 ((int) std::lround (x * 32768.0f));

        loop.load (kerrStinn, audio.getReadPointer (0), stereo ? audio.getReadPointer (1) : nullptr, numFrames);
        status = statusLoaded;

        const juce::ScopedLock sl (sourceLock);
        currentSource = resolved;
    }

    ++loadCount;
}

void KerrStinnProcessor::publish()
{
    Snapshot::Ptr s = new Snapshot();
    s->loaded = kerrStinn.engine.isAllocated();
    s->numSlices = kerrStinn.getNumSlices();

    if (s->loaded)
    {
        s->numFrames = kerrStinn.engine.getLength();
        for (int ch = 0; ch < 2; ++ch)
            s->processed[(size_t) ch].assign (loop.processed (ch), loop.processed (ch) + s->numFrames + 1);
    }

    releasePool.push_back (s);
    const juce::SpinLock::ScopedLockType lock (snapshotLock);
    latest = s;
}

KerrStinnProcessor::Snapshot::Ptr KerrStinnProcessor::getLatestSnapshot() const
{
    const juce::SpinLock::ScopedLockType lock (snapshotLock);
    return latest;
}

void KerrStinnProcessor::waitUntilIdle()
{
    for (;;)
    {
        bool pending;
        {
            const juce::ScopedLock sl (requestLock);
            pending = pendingLoad || pendingRestore || pendingRecompute;
        }
        bool paramsPending = false;
        for (int i = 0; i < kerr::kNumParams; ++i)
            paramsPending |= ! juce::exactlyEqual (rawParams[(size_t) i]->load(), applied[(size_t) i]);

        if (! pending && ! busy && ! paramsPending)
            return;
        notify();
        juce::Thread::sleep (2);
    }
}

//==============================================================================
void KerrStinnProcessor::loadLoop (const juce::File& file)
{
    {
        const juce::ScopedLock sl (requestLock);
        pendingLoad = LoopSource { file.getFullPathName(), {} };
    }
    notify();
}

juce::String KerrStinnProcessor::getLoopFileName() const
{
    const juce::ScopedLock sl (sourceLock);
    return juce::File (currentSource.path).getFileName();
}

void KerrStinnProcessor::setRandFlavour (kerr::Rand::Flavour f)
{
    randFlavour = (int) f;
    {
        const juce::ScopedLock sl (requestLock);
        pendingRecompute = true;
    }
    notify();
}

void KerrStinnProcessor::getStateInformation (juce::MemoryBlock& dest)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    xml->setAttribute ("rand", getRandFlavour() == kerr::Rand::kWindows ? "windows" : "mac");

    {
        const juce::ScopedLock sl (sourceLock);
        xml->setAttribute ("loopPath", currentSource.path);
        if (currentSource.data.getSize() > 0)
            xml->createNewChildElement ("LoopData")->addTextElement (currentSource.data.toBase64Encoding());
    }

    copyXmlToBinary (*xml, dest);
}

void KerrStinnProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xml (getXmlFromBinary (data, sizeInBytes));
    if (xml == nullptr || ! xml->hasTagName (apvts.state.getType()))
        return;

    Restore r;
    r.flavour = xml->getStringAttribute ("rand") == "windows" ? kerr::Rand::kWindows : kerr::Rand::kMac;
    r.source.path = xml->getStringAttribute ("loopPath");
    if (auto* loopData = xml->getChildByName ("LoopData"))
        r.source.data.fromBase64Encoding (loopData->getAllSubText());
    xml->deleteAllChildElementsWithTagName ("LoopData");

    pollingSuspended = true;
    apvts.replaceState (juce::ValueTree::fromXml (*xml));
    for (int i = 0; i < kerr::kNumParams; ++i)
        r.values[(size_t) i] = rawParams[(size_t) i]->load();

    randFlavour = (int) r.flavour;
    {
        const juce::ScopedLock sl (requestLock);
        pendingRestore = std::move (r);
    }
    notify();
}

//==============================================================================
juce::AudioProcessorEditor* KerrStinnProcessor::createEditor()
{
    return new KerrStinnEditor (*this);
}

juce::AudioProcessor* JUCE_CALLTYPE createPluginFilter()
{
    return new KerrStinnProcessor();
}
