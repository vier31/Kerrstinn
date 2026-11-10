// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KERR_STINN_H
#define KERR_STINN_H

// The host-independent part of original/KerrStinn.cpp (AKerrStinn): the six
// normalised parameters, how they map onto the engine, the reprocessing pass
// each change triggers, and the bar position used for playback.
//
// A pass is split into steps so the firmware can spread it over its main
// loop: begin*() starts it and returns `end`, then work(i) must be called for
// i = 1 .. end-1 in order. Desktop hosts use the blocking helpers.
// C++98, no allocation.

#include "KerrEngine.h"

namespace kerr {

enum Param
{
    kRand = 0,
    kSlices,
    kBisyness,
    kGenerationMatrix,
    kNumBars,
    kOn,
    kNumParams,
    kRecompute = 666 // the editor's "file loaded" tag: reprocess, change nothing
};

// AKerrStinn::setProgram() applies the parameters in this order.
static const int kRestoreOrder[kNumParams] = { kRand, kSlices, kBisyness, kGenerationMatrix, kOn, kNumBars };

static const char* const kParamNames[kNumParams] = { "Random", "Slices", "Bisyness", "Generator", "Bars", "On" };

inline float defaultValue (int index)
{
    static const float d[kNumParams] = { 0.2f, 0.1212121212f, 0.5f, 0.3f, 0.0f, 1.0f };
    return d[index];
}

// getParameterDisplay() values.
inline int seedFor (float v) { return (int) (v * 65535); }
inline int slicesFor (float v) { return (int) (((v * 32) + 1)) * 2; }
inline int bisynessFor (float v) { return (int) (v * 100); }
inline int generatorFor (float v) { return (int) ((v * (float) (kNumTypes - 1) + 0.99) + 1); }
inline int barsFor (float v) { return int (v * 7.99) + 1; }

inline const char* generatorName (int type)
{
    static const char* const names[17] = {
        "uninit", "Botox", "General Cluster", "Retardler", "Frikadish A", "Freaky 4",
        "Silence Sometimes", "Magaluf", "Wiiooowww (AR)", "Circle teller", "Passfelt",
        "Granny's favorite", "R.A.W.MEAT", "Tuzticle", "ElgKebab", "PiiTiiVii", "RythmSickness"
    };
    return (type >= 1 && type <= 16) ? names[type] : names[0];
}

template <class Buffer>
class KerrStinn
{
public:
    Engine<Buffer> engine;

    KerrStinn() : numbars (1), mySlices (8)
    {
        for (int i = 0; i < kNumParams; i++)
            params[i] = defaultValue (i);
        // The constructor's setProgram(0): no file yet, so this only sets
        // the engine's slice count, generator and name.
        for (int i = 0; i < kNumParams; i++)
            setParameter (kRestoreOrder[i], params[i]);
    }

    float getParameter (int index) const { return params[index]; }
    int getNumBars() const { return numbars; }
    int getNumSlices() const { return mySlices; }
    bool isOn() const { return params[kOn] != 0.0f; }

    // AKerrStinn::setParameter(). Returns the pass end for work().
    int beginSetParameter (int index, float value)
    {
        if (index >= 0 && index < kNumParams)
            params[index] = value;

        numbars = barsFor (params[kNumBars]);
        mySlices = slicesFor (params[kSlices]);

        engine.setParams (seedFor (params[kRand]), mySlices, false, bisynessFor (params[kBisyness]),
                          generatorFor (params[kGenerationMatrix]));
        // sic: 63, not the slice count, so higher slices keep their last result.
        int end = 1;
        while (end < ((params[kSlices] * 63) + 1))
            end++;
        return end;
    }

    // engine::loadFile() after the owner has filled engine.in[] and called
    // engine.attach(). Processes every slice with the current settings.
    int beginLoad() { return engine.getNumSlices() + 1; }

    void work (int i) { engine.bigWorker (i); }

    void setParameter (int index, float value)
    {
        const int end = beginSetParameter (index, value);
        for (int i = 1; i < end; i++)
            work (i);
    }

    // The editor's file load: loadFile(), then setParameter(666).
    void loaded()
    {
        const int end = beginLoad();
        for (int i = 1; i < end; i++)
            work (i);
        setParameter (kRecompute, 0.0f);
    }

    // setChunk() -> setProgram(): the chunk replaces all six values, then the
    // file is loaded (the owner did that) and every parameter is applied in
    // the original's order, each pass already seeing all the new values.
    void restore (const float* values)
    {
        for (int i = 0; i < kNumParams; i++)
            params[i] = values[i];
        if (engine.isAllocated())
        {
            const int end = beginLoad();
            for (int i = 1; i < end; i++)
                work (i);
        }
        for (int i = 0; i < kNumParams; i++)
            setParameter (kRestoreOrder[i], params[kRestoreOrder[i]]);
    }

    // processReplacing()'s read position. barPhase is the position inside
    // the current bar (0..1), bar the host's bar count. The original divided
    // by (bar length + block size), which made it depend on the host's
    // buffer size; that term is left out.
    float positionFor (double barPhase, long bar) const
    {
        float inpos = (float) barPhase;
        inpos = inpos / numbars;
        inpos += ((1.0 / numbars) * (int) (bar % numbars)); // float += double, as in the original
        if (inpos > 1.0)
            inpos = 0;
        return inpos;
    }

    float sample (int side, float inpos) const { return engine.getData (side, inpos); }

private:
    float params[kNumParams];
    int numbars;
    int mySlices;
};

} // namespace kerr

#endif
