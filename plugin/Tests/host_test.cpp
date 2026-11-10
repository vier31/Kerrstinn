// Drives KerrStinnProcessor like a host (transport, parameter changes,
// project save/restore) and compares every output sample with the engine run
// directly.
#include "../Source/PluginProcessor.h"

namespace
{
int failures = 0;

void expect (bool ok, const juce::String& what)
{
    std::printf ("%s %s\n", ok ? "ok  " : "FAIL", what.toRawUTF8());
    failures += ok ? 0 : 1;
}

struct PlayHead : juce::AudioPlayHead
{
    double bpm = 120.0, ppq = 0.0;
    bool playing = true;
    juce::Optional<PositionInfo> getPosition() const override
    {
        PositionInfo p;
        p.setBpm (bpm);
        p.setPpqPosition (ppq);
        p.setPpqPositionOfLastBarStart (std::floor (ppq / 4.0) * 4.0);
        p.setTimeSignature (TimeSignature { 4, 4 });
        p.setIsPlaying (playing);
        return p;
    }
};

// The loop as the processor decodes it.
struct Reference
{
    kerr::FloatKerrStinn k;
    kerr::FloatLoop loop;
    std::vector<float> l, r;

    explicit Reference (const juce::File& f)
    {
        juce::AudioFormatManager fm;
        fm.registerBasicFormats();
        std::unique_ptr<juce::AudioFormatReader> reader (fm.createReaderFor (f));
        const int n = (int) reader->lengthInSamples;
        juce::AudioBuffer<float> a ((int) reader->numChannels, n);
        reader->read (&a, 0, n, 0, true, true);
        l.resize ((size_t) n);
        r.resize ((size_t) n);
        for (int i = 0; i < n; ++i)
        {
            l[(size_t) i] = kerr::fromPcm16 ((int) std::lround (a.getSample (0, i) * 32768.0f));
            r[(size_t) i] = kerr::fromPcm16 ((int) std::lround (a.getSample (a.getNumChannels() - 1, i) * 32768.0f));
        }
        loop.load (k, l.data(), a.getNumChannels() > 1 ? r.data() : nullptr, n);
    }
};

bool sameAsSnapshot (const KerrStinnProcessor& p, const Reference& ref)
{
    auto s = p.getLatestSnapshot();
    const int n = ref.k.engine.getLength();
    if (s == nullptr || ! s->loaded || s->numFrames != n)
        return false;
    for (int ch = 0; ch < 2; ++ch)
        if (std::memcmp (s->processed[(size_t) ch].data(), ref.loop.processed (ch), sizeof (float) * (size_t) (n + 1)) != 0)
            return false;
    return true;
}

void setParam (KerrStinnProcessor& p, int index, float v)
{
    p.apvts.getParameter (KerrStinnProcessor::paramIDs[index])->setValueNotifyingHost (v);
}
} // namespace

int main()
{
    juce::ScopedJuceInitialiser_GUI juce;
    const juce::File fjord (KERR_FJORD);

    KerrStinnProcessor proc;
    PlayHead head;
    proc.setPlayHead (&head);
    proc.setPlayConfigDetails (0, 2, 44100.0, 512);
    proc.prepareToPlay (44100.0, 512);

    proc.loadLoop (fjord);
    proc.waitUntilIdle();
    Reference ref (fjord);
    ref.k.loaded();
    expect (sameAsSnapshot (proc, ref), "editor load matches loadFile() + setParameter(666)");

    // Parameter changes: one pass each, in parameter order.
    const float values[] { 0.61f, 0.4f, 0.8f, 7.0f / 25.0f, 1.0f / 7.99f };
    for (int i = 0; i < 5; ++i)
    {
        setParam (proc, i, values[i]);
        proc.waitUntilIdle();
        // The host's parameter quantisation is what the engine sees.
        ref.k.setParameter (i, proc.apvts.getRawParameterValue (KerrStinnProcessor::paramIDs[i])->load());
    }
    expect (sameAsSnapshot (proc, ref), "parameter changes match the engine pass for pass");

    // Playback: 4 bars at 120 BPM, two of them per loop (Bars = 2).
    juce::AudioBuffer<float> buffer (2, 512);
    juce::MidiBuffer midi;
    bool playbackOk = true;
    const double ppqPerSample = head.bpm / (60.0 * 44100.0);
    int64_t t = 0;
    for (int block = 0; block < 4 * 44100 * 2 / 512; ++block)
    {
        head.ppq = (double) t * ppqPerSample;
        proc.processBlock (buffer, midi);
        const double ppq = head.ppq;
        for (int i = 0; i < 512; ++i)
        {
            const double phase = (ppq - std::floor (ppq / 4.0) * 4.0) / 4.0 + head.bpm / (44100.0 * 60.0 * 4.0) * i;
            const float pos = kerr::positionFor (phase, (long) (ppq / 4.0), ref.k.getNumBars());
            const int idx = kerr::indexFor (pos, ref.k.engine.getLength());
            for (int ch = 0; ch < 2; ++ch)
                playbackOk &= juce::exactlyEqual (buffer.getSample (ch, i), ref.loop.processed (ch)[idx]);
        }
        t += 512;
    }
    expect (playbackOk, "playback follows the host's bar position sample for sample");

    // A stopped transport (ppq not moving) is silent.
    proc.processBlock (buffer, midi);
    expect (buffer.getMagnitude (0, 512) == 0.0f, "stopped transport is silent");

    // Project save and restore into a fresh instance.
    juce::MemoryBlock state;
    proc.getStateInformation (state);
    {
        KerrStinnProcessor restored;
        restored.setStateInformation (state.getData(), (int) state.getSize());
        restored.waitUntilIdle();
        Reference r2 (fjord);
        float v[kerr::kNumParams];
        for (int i = 0; i < kerr::kNumParams; ++i)
            v[i] = proc.apvts.getRawParameterValue (KerrStinnProcessor::paramIDs[i])->load();
        r2.k.restore (v);
        expect (sameAsSnapshot (restored, r2), "restored project matches setChunk() -> setProgram()");
    }

    // The loop file has moved: the copy embedded in the project is used.
    {
        auto xml = juce::AudioProcessor::getXmlFromBinary (state.getData(), (int) state.getSize());
        xml->setAttribute ("loopPath", "/nonexistent/fjord.wav");
        juce::MemoryBlock moved;
        juce::AudioProcessor::copyXmlToBinary (*xml, moved);
        KerrStinnProcessor restored;
        restored.setStateInformation (moved.getData(), (int) moved.getSize());
        restored.waitUntilIdle();
        auto s = restored.getLatestSnapshot();
        expect (s != nullptr && s->loaded && s->numFrames == ref.k.engine.getLength(), "project with a moved loop restores from the embedded copy");
    }

    // Windows random patterns differ from the Mac ones.
    {
        proc.setRandFlavour (kerr::Rand::kWindows);
        proc.waitUntilIdle();
        ref.k.engine.random().setFlavour (kerr::Rand::kWindows);
        ref.k.setParameter (kerr::kRecompute, 0.0f);
        expect (sameAsSnapshot (proc, ref), "Windows rand() flavour");
    }

    std::printf (failures == 0 ? "host_test: all passed\n" : "host_test: %d failed\n", failures);
    return failures == 0 ? 0 : 1;
}
