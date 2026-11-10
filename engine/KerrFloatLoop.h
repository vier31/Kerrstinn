// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KERR_FLOAT_LOOP_H
#define KERR_FLOAT_LOOP_H

// Float storage for desktop hosts (plugin, Rack, tests).

#include <vector>

#include "KerrStinn.h"

namespace kerr {

struct FloatBuffer
{
    float* p;
    FloatBuffer() : p (0) {}
    float get (int i) const { return p[i]; }
    void set (int i, float v) { p[i] = v; }
};

typedef KerrStinn<FloatBuffer> FloatKerrStinn;

// engine::convertToFloat(): 16-bit samples were divided by 32767.
inline float fromPcm16 (int x) { return (float) ((double) x / 32767.0); }

// Owns the four buffers (plus the guard sample) and attaches them.
class FloatLoop
{
public:
    // right may be null for mono: the original copied L into R and
    // processed both.
    void load (FloatKerrStinn& k, const float* left, const float* right, int frames)
    {
        k.engine.detach();
        for (int ch = 0; ch < 2; ch++)
        {
            const float* src = (ch == 1 && right != 0) ? right : left;
            in[ch].assign (src, src + frames);
            in[ch].push_back (0.0f);
            // A fresh plugin's buffers were fresh (zeroed) memory.
            out[ch].assign ((size_t) frames + 1, 0.0f);
            k.engine.in[ch].p = in[ch].data();
            k.engine.out[ch].p = out[ch].data();
        }
        k.engine.attach (frames, 2);
    }

    const float* processed (int ch) const { return out[ch].data(); }

private:
    std::vector<float> in[2], out[2];
};

} // namespace kerr

#endif
