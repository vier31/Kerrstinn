// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KERR_RAND_H
#define KERR_RAND_H

// The original called the C library's srand()/rand(), so the pattern a
// "Random" seed produces depended on the platform it was built for. Both
// 2007 implementations are reproduced so a seed always gives the same loop.
//
// C++98, no allocation: also compiled into the Clouds firmware.

namespace kerr {

class Rand
{
public:
    enum Flavour
    {
        kMac,     // Darwin libc: Park-Miller "minimal standard", RAND_MAX 0x7fffffff
        kWindows  // MSVC CRT: LCG, RAND_MAX 0x7fff
    };

    Rand() : flavour_ (kMac), state_ (1) {}

    void setFlavour (Flavour f) { flavour_ = f; }
    Flavour flavour() const { return flavour_; }

    void seed (unsigned int s) { state_ = s; }

    int next()
    {
        if (flavour_ == kWindows)
        {
            state_ = state_ * 214013u + 2531011u;
            return (int) ((state_ >> 16) & 0x7fffu);
        }

        // Darwin Libc stdlib/FreeBSD/rand.c do_rand(), non-weak-seeding branch.
        int x = (int) state_;
        if (x == 0)
            x = 123459876;
        const int hi = x / 127773;
        const int lo = x % 127773;
        x = 16807 * lo - 2836 * hi;
        if (x < 0)
            x += 0x7fffffff;
        state_ = (unsigned int) x;
        return x;
    }

private:
    Flavour flavour_;
    unsigned int state_;
};

} // namespace kerr

#endif
