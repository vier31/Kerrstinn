// KerrStinn - slicer/glitcher after Johan Larsby's 2007 original.
// Copyright (C) 2007 Johan Larsby, 2026 Jan Schröder.
// SPDX-License-Identifier: GPL-3.0-or-later

#ifndef KERR_ENGINE_H
#define KERR_ENGINE_H

// Port of original/engine.cpp and original/engineWorkers.cpp.
//
// The code follows the original line by line: same evaluation order, same
// rand() calls, same int/float/double conversions. Everything works in place
// on the output buffers, which persist between parameter changes, so the
// result depends on the history of changes. That is how the original sounds.
//
// Deviations, all where the original had undefined behaviour:
//  * Reads outside [0, length] return 0, writes outside are dropped. Index
//    `length` is a real guard sample: the original wrote it and read it back.
//  * Integer division/modulo by zero (General Cluster at 2 slices, Granny on
//    tiny slices) behaves like ARM's sdiv: x / 0 == 0, x % 0 == x.
//  * abs((int) x) for NaN or huge x gives INT_MIN, as on the original's x86.
//  * PiiTiiVii at amount 1.0 outputs 0 where the original produced NaN.
//
// Storage is a template parameter so the firmware can keep 16-bit samples.
// A Buffer needs `float get(int) const` and `void set(int, float)` for
// indices 0..length (length + 1 samples). C++98, no allocation.

#include <math.h>
#include <stdio.h>
#include <string.h>

#include "KerrRand.h"

namespace kerr {

static const int kNumTypes = 26;  // DnumTypes: the Generator knob's range; 17-26 are "uninit"
static const int kMaxSlices = 66; // Slices knob maximum: 2 * (32 + 1)

// abs((int) x) with the x86 result for NaN and out-of-range values.
inline int absTrunc (float x)
{
    if (! (x > -2147483648.0f && x < 2147483648.0f))
        return (-2147483647 - 1);
    const int t = (int) x;
    return t < 0 ? -t : t;
}

inline int safeDiv (int a, int b) { return b == 0 ? 0 : a / b; }
inline int safeMod (int a, int b) { return b == 0 ? a : a % b; }

struct Slice
{
    bool isBackward;
    bool changedPosition;
    int position;
    bool changedPositionMIX;
    int positionMIX;
    bool changedPositionSilence;
    bool isPitched;
    int pitchType;
    bool isDistorted;
    int distortionTime;
    bool isGapped;
    int gappedType;
    bool isFFT;
    char note;
    bool slide;
    int slideamount;
    bool isAttack;
    bool isRelease;
    int attack;
    int release;
    bool IsGranny;
    float grannySize;
    float grannyMove;
    float grannySpeed;
    float DistortionAmount;
    float distortionThreshold;
    bool isSwapped;
    int swappedWith;
};

template <class Buffer>
class Engine
{
public:
    Engine() : allocated (false), wavesize (0), channels (0),
               startSeed (0), numSlices (0), keepFirst (false), bisyness (0), renderType (0)
    {
        strcpy (rendertypename, "ununit");
        // The original constructor left some fields uninitialised; setParams()
        // clears every slice it uses before reading it.
        memset (mySlice, 0, sizeof (mySlice));
        for (int i = 0; i <= kMaxSlices; i++)
        {
            mySlice[i].position = i;
            mySlice[i].changedPositionMIX = i != 0; // sic: "changedPositionMIX = i"
            mySlice[i].positionMIX = i;
            mySlice[i].attack = 2;
            mySlice[i].release = 2;
        }
    }

    // The owner fills in[] with the loop (both channels; for mono, in[1]
    // holds a copy of in[0], as convertToFloat() did) and sets out[] to
    // storage of length + 1 samples, then calls attach(). Mono processing
    // (channels == 1) skips the right channel.
    Buffer in[2];
    Buffer out[2];

    void attach (int length, int numChannels)
    {
        wavesize = length;
        channels = numChannels;
        allocated = length > 0;
    }

    void detach() { allocated = false; }

    bool isAllocated() const { return allocated; }
    int getLength() const { return wavesize; }
    int getChannels() const { return channels; }
    int getNumSlices() const { return numSlices; }
    const char* getrendertypename() const { return rendertypename; }
    Rand& random() { return rng; }
    const Slice& sliceState (int i) const { return mySlice[i]; }

    // engine::getData(side, position): nearest-sample read of the output.
    float getData (int side, float position) const
    {
        return rd (out[side], (int) (wavesize * position));
    }

    //--------------------------------------------------------------------
    void bigWorker (int slice)
    {
        if (! allocated)
            return;
        int mySlice = slice - 1;
        if (mySlice == -1)
            mySlice = numSlices;

        // The original ran every worker on L, then on R. The channels never
        // interact, so running all workers per channel is equivalent.
        for (int ch = 0; ch < channels; ch++)
        {
            Buffer& o = out[ch];
            smallWorkerBackwards (in[ch], o, mySlice);
            smallWorkerDistortion (o, o, mySlice);
            smallWorkerPitcher (o, o, mySlice);
            smallWorkerGapper (o, o, mySlice);
            smallWorkerReallocate (o, o, mySlice);
            smallWorkerReallocateMixed (o, o, mySlice);
            smallWorkerFFT (o, o, mySlice);
            smallWorkerSwap (o, o, mySlice);
            smallWorkerAR (o, o, mySlice);
            smallWorkerGranny (o, o, mySlice);
        }
    }

    //--------------------------------------------------------------------
    void setParams (int inStartSeed, int inNumSlices, bool inKeepFirst, int inBisyness, int inrenderType)
    {
        startSeed = inStartSeed;
        numSlices = inNumSlices;
        keepFirst = inKeepFirst;
        bisyness = inBisyness;
        renderType = inrenderType;
        strcpy (rendertypename, "uninit");

        //--------------------------- Null everything
        for (int i = 0; i < numSlices && i <= kMaxSlices; i++)
        {
            mySlice[i].isBackward = false;
            mySlice[i].changedPositionMIX = false;
            mySlice[i].positionMIX = i;
            mySlice[i].changedPosition = false;
            mySlice[i].position = i;
            mySlice[i].changedPositionSilence = true;
            mySlice[i].isPitched = false;
            mySlice[i].isDistorted = false;
            mySlice[i].distortionTime = i;
            mySlice[i].isGapped = false;
            mySlice[i].gappedType = 0;
            mySlice[i].isFFT = false;
            mySlice[i].note = 0;
            mySlice[i].slide = false;
            mySlice[i].slideamount = 0;
            mySlice[i].isAttack = false;
            mySlice[i].isRelease = false;
            mySlice[i].attack = 2;
            mySlice[i].release = 2;
            mySlice[i].IsGranny = false;
            mySlice[i].DistortionAmount = 0;
            mySlice[i].distortionThreshold = 0;
            mySlice[i].isSwapped = false;
            mySlice[i].swappedWith = 0;
        }

        rng.seed ((unsigned int) startSeed);

        if (renderType == 1)
        {
            strcpy (rendertypename, "Botox");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                if (keepFirst && i == 0)
                {
                    mySlice[i].position = 0;
                }
                else
                {
                    if (getrandb() && i != 0) // copy previous
                    {
                        mySlice[i] = mySlice[i - 1];
                    }
                    else
                    {
                        mySlice[i].isBackward = getrandb();
                        mySlice[i].changedPosition = getrandb();
                        mySlice[i].position = getrand (numSlices - i);
                        mySlice[i].isPitched = getrandb();
                        mySlice[i].pitchType = 0;
                        mySlice[i].isGapped = getrandb();
                        mySlice[i].gappedType = 19;
                    }
                }
            }
        }
        else if (renderType == 2)
        {
            strcpy (rendertypename, "General Cluster");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].changedPosition = getrandb();
                const int quarter = numSlices / 4;
                const int r = getrand2 (i);
                mySlice[i].position = safeMod (i, quarter) * safeDiv (r, quarter);
            }
        }
        else if (renderType == 3)
        {
            strcpy (rendertypename, "Retardler");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].isBackward = getrandb();
                mySlice[i].changedPosition = getrandb();
            }
        }
        else if (renderType == 4)
        {
            strcpy (rendertypename, "Frikadish A");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].isFFT = getrandb();
                mySlice[i].note = (char) (getrand (100) + 1);
                mySlice[i].slide = getrandb();
                mySlice[i].slideamount = getrand (2);
            }
        }
        else if (renderType == 5)
        {
            strcpy (rendertypename, "Freaky 4");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                if ((i % 4) == 0)
                {
                    mySlice[i].isBackward = getrandb();
                    mySlice[i].isFFT = getrandb();
                    mySlice[i].note = (char) (getrand (100) + 1);
                    mySlice[i].slide = getrandb();
                    mySlice[i].slideamount = getrand (2);
                }
            }
        }
        else if (renderType == 6)
        {
            strcpy (rendertypename, "Silence Sometimes");
            if (! allocated)
                return;
            int tok = getrand (numSlices / 2) + 1;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].changedPositionMIX = getrandb();
                mySlice[i].positionMIX = (i % tok) * (numSlices / tok);
                mySlice[i].changedPositionSilence = true;
            }
        }
        else if (renderType == 7)
        {
            strcpy (rendertypename, "Magaluf");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                if (keepFirst && i == 0)
                {
                    mySlice[i].position = 0;
                }
                else
                {
                    if (getrandb() && i != 0) // copy previous
                    {
                        mySlice[i] = mySlice[i - 1];
                    }
                    else
                    {
                        mySlice[i].isBackward = getrandb();
                        mySlice[i].changedPosition = getrandb();
                        mySlice[i].position = getrand (numSlices - i);
                        mySlice[i].isPitched = getrandb();
                        mySlice[i].pitchType = 0;
                        mySlice[i].isGapped = getrandb();
                        mySlice[i].gappedType = 19;
                        mySlice[i].isFFT = getrandb();
                        mySlice[i].note = (char) (getrand (100) + 1);
                        mySlice[i].slide = getrandb();
                        mySlice[i].slideamount = getrand (2);
                    }
                }
            }
        }
        else if (renderType == 8)
        {
            strcpy (rendertypename, "Wiiooowww (AR)");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].isAttack = getrandb();
                mySlice[i].isRelease = getrandb();
                mySlice[i].attack = getrand (100);
                mySlice[i].release = getrand (100);
            }
        }
        else if (renderType == 9)
        {
            strcpy (rendertypename, "Circle teller");
            if (! allocated)
                return;
            int arne = getrand (numSlices);
            int barne = getrand (numSlices);
            int carne = getrand (numSlices);
            int darne = getrand (numSlices);
            for (int i = 0; i < numSlices; i++)
            {
                int myNumber = getrand (3);
                mySlice[i].changedPosition = getrandb();
                if (myNumber == 0)
                {
                    mySlice[i].position = arne;
                }
                if (myNumber == 1)
                {
                    mySlice[i].position = barne;
                    mySlice[i].isGapped = getrandb();
                    mySlice[i].gappedType = getrand (19);
                }
                if (myNumber == 2)
                {
                    mySlice[i].position = carne;
                    mySlice[i].isBackward = getrandb();
                }
                if (myNumber == 3)
                {
                    mySlice[i].isFFT = getrandb();
                    mySlice[i].position = darne;
                    mySlice[i].note = (char) (getrand (100) + 1);
                    mySlice[i].slide = getrandb();
                    mySlice[i].slideamount = getrand (2);
                }
            }
        }
        else if (renderType == 10)
        {
            strcpy (rendertypename, "Passfelt");
            if (! allocated)
                return;
            int forevery = getrand (numSlices - 1) + 1;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].changedPosition = getrandb();
                if (i % forevery == 1)
                {
                    mySlice[i].isBackward = getrandb();
                    mySlice[i].isFFT = getrandb();
                    mySlice[i].note = (char) (getrand (100) + 1);
                    mySlice[i].slide = getrandb();
                    mySlice[i].slideamount = getrand (2);
                }
            }
        }
        else if (renderType == 11)
        {
            strcpy (rendertypename, "Granny's favorite");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].IsGranny = getrandb();
                mySlice[i].grannySize = grannyFormula();
                mySlice[i].grannyMove = grannyFormula();
                mySlice[i].grannySpeed = grannyFormula();
            }
        }
        else if (renderType == 12)
        {
            strcpy (rendertypename, "R.A.W.MEAT");
            if (! allocated)
                return;
            bool lastwas = false;
            int numGranns = 0;
            int numcount = 0;
            for (int i = 0; i < numSlices; i++)
            {
                if (! lastwas)
                {
                    lastwas = getrandb();
                    mySlice[i].IsGranny = lastwas;
                    mySlice[i].grannySize = grannyFormula();
                    mySlice[i].grannyMove = grannyFormula();
                    mySlice[i].grannySpeed = grannyFormula();
                    numGranns = getrand (numSlices - i);
                }
                else
                {
                    mySlice[i].IsGranny = lastwas;
                    mySlice[i].grannySize = mySlice[i - 1].grannySize;
                    mySlice[i].grannyMove = mySlice[i - 1].grannyMove;
                    mySlice[i].grannySpeed = mySlice[i - 1].grannySpeed;
                    numcount++;
                    if (numcount == numGranns)
                        lastwas = false;
                }
            }
        }
        else if (renderType == 13)
        {
            strcpy (rendertypename, "Tuzticle");
            if (! allocated)
                return;
            bool lastwas = false;
            int startGranny = getrand2 (numSlices);
            for (int i = 0; i < numSlices; i++)
            {
                if (lastwas)
                {
                    mySlice[i].changedPositionMIX = true;
                    mySlice[i].positionMIX = getrand (numSlices);
                    mySlice[i].changedPositionSilence = false;
                }
                if (startGranny == i)
                {
                    lastwas = true;
                    mySlice[i].changedPositionMIX = true;
                    mySlice[i].positionMIX = getrand (numSlices);
                    mySlice[i].changedPositionSilence = false;
                }
            }
        }
        else if (renderType == 14)
        {
            strcpy (rendertypename, "ElgKebab");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                int banan = getrand (numSlices) + 1;
                if ((i % banan) == 0)
                {
                    mySlice[i].isFFT = getrandb();
                    mySlice[i].note = (char) (100 - ((getrand (8) + 1) * 2));
                    mySlice[i].isAttack = getrandb();
                    mySlice[i].isRelease = getrandb();
                    mySlice[i].attack = getrand (100);
                    mySlice[i].release = getrand (100);
                    mySlice[i].changedPosition = getrandb();
                    mySlice[i].position = getrand (numSlices);
                }
            }
        }
        else if (renderType == 15)
        {
            strcpy (rendertypename, "PiiTiiVii");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].isDistorted = getrandb();
                mySlice[i].DistortionAmount = (float) (getrand (100) / 100.0);
                mySlice[i].distortionThreshold = (float) (getrand (30) / 100.0);
            }
        }
        else if (renderType == 16)
        {
            strcpy (rendertypename, "RythmSickness");
            if (! allocated)
                return;
            for (int i = 0; i < numSlices; i++)
            {
                mySlice[i].isSwapped = getrandb();
                mySlice[i].swappedWith = getrand (numSlices);
            }
        }
    }

private:
    bool allocated;
    int wavesize;
    int channels;
    char rendertypename[52];

    int startSeed;
    int numSlices;
    bool keepFirst;
    int bisyness;
    int renderType;

    Slice mySlice[kMaxSlices + 1];
    Rand rng;

    float rd (const Buffer& b, int i) const
    {
        return (i >= 0 && i <= wavesize) ? b.get (i) : 0.0f;
    }

    void wr (Buffer& b, int i, float v)
    {
        if (i >= 0 && i <= wavesize)
            b.set (i, v);
    }

    //--------------------------------------------------------------------
    int getrand (int max)
    {
        float fslump = (float) ((float) bisyness / 100.0);
        int toReturn = (int) ((float) (rng.next() % (max + 1)) * (float) fslump);
        return toReturn;
    }

    int getrand2 (int max)
    {
        int toReturn = (int) ((float) (rng.next() % (max + 1)));
        return toReturn;
    }

    int getrandb()
    {
        if ((rng.next() % 100) < bisyness)
            return 1;
        else
            return 0;
    }

    // #define FORMULA ((getrand(50)+1)/100.f);
    float grannyFormula() { return (getrand (50) + 1) / 100.f; }

    //--------------------------------------------------------------------
    void smallWorkerAR (Buffer& in, Buffer& out, int slice)
    {
        int i;
        float mambo = ((float) wavesize / (float) numSlices);

        int first = (int) ((slice) * mambo);
        int second = (int) ((slice + 1) * mambo);

        for (i = 0; i < mambo; i++)
            wr (out, i + first, rd (in, i + first));

        if (mySlice[slice].isAttack)
        {
            float attacklength = (mySlice[slice].attack / 100.0f);
            float myAttack = 0.0;
            float myAttackChanger = (float) (1.0 / (mambo * attacklength));

            for (i = 0; i < mambo * attacklength; i++)
            {
                wr (in, i + first, rd (in, i + first) * myAttack);
                myAttack += myAttackChanger;
            }
        }
        if (mySlice[slice].isRelease)
        {
            float releaselength = (mySlice[slice].release / 100.0f);
            float myRelease = 0.0;
            float myReleaseChanger = (float) (1.0 / (mambo * releaselength));

            for (i = 0; i < mambo * releaselength; i++)
            {
                wr (out, second - i, rd (out, second - i) * myRelease);
                myRelease += myReleaseChanger;
            }
        }
    }

    void smallWorkerBackwards (Buffer& in, Buffer& out, int slice)
    {
        int i;
        float mambo = ((float) wavesize / (float) numSlices);

        int first = (int) ((slice) * mambo);
        int second = (int) ((slice + 1) * mambo);

        if (mySlice[slice].isBackward)
        {
            for (i = 0; i < mambo; i++)
                wr (out, i + first, rd (in, second - i));
        }
        else
        {
            for (i = 0; i < mambo; i++)
                wr (out, i + first, rd (in, i + first));
        }
    }

    void smallWorkerDistortion (Buffer& in, Buffer& out, int slice)
    {
        if (! mySlice[slice].isDistorted)
            return;

        float inMY;
        int i;
        float mambo = ((float) wavesize / (float) numSlices);
        const float thr = mySlice[slice].distortionThreshold;

        float k = 2 * mySlice[slice].DistortionAmount / (1 - mySlice[slice].DistortionAmount);
        for (i = (int) (slice * mambo); i < (slice + 1) * mambo; i++)
        {
            inMY = rd (in, i);
            if (inMY > thr || inMY < -thr)
                inMY = fabsf (fabsf (fmodf (inMY - thr, thr * 4)) - thr * 2) - thr;
            inMY = (1 + k) * rd (in, i) / (1 + k * absTrunc (inMY));
            if (inMY != inMY)
                inMY = 0.0f;
            if (inMY > 1.0)
                inMY = 1.0;
            else if (inMY < -1.0)
                inMY = -1.0;
            wr (out, i, inMY);
        }
    }

    void smallWorkerReallocate (Buffer& in, Buffer& out, int slice)
    {
        if (! mySlice[slice].changedPosition)
            return;
        if (mySlice[slice].position >= numSlices)
            return;

        int mambo = wavesize / numSlices;
        int first = slice * mambo;
        int second = (mySlice[slice].position) * mambo;

        for (int i = 0; i < mambo; i++)
            wr (out, i + first, rd (in, i + second));
    }

    void smallWorkerReallocateMixed (Buffer& in, Buffer& out, int slice)
    {
        if (! mySlice[slice].changedPositionMIX)
            return;
        if (mySlice[slice].positionMIX >= numSlices)
            return;

        int mambo = wavesize / numSlices;
        int first = slice * mambo;
        int second = (mySlice[slice].positionMIX) * mambo;

        for (int i = mambo; i > 0; i--)
        {
            wr (out, i + first, rd (out, i + first) + rd (in, i + second));
            if (mySlice[slice].changedPositionSilence)
                wr (out, i + second, 0.0f);
        }
    }

    void smallWorkerPitcher (Buffer& in, Buffer& out, int slice)
    {
        int i;
        int mambo = wavesize / numSlices;

        if (! mySlice[slice].isPitched)
            return;

        // Every generator sets pitchType 0, so none of this ever ran.
        const int type = mySlice[slice].pitchType;
        if (type < 1 || type > 3)
            return;
        const int parts = type + 1;
        const int first = slice * mambo;
        for (i = 0; i < mambo / parts; i++)
        {
            const float v = rd (in, (i * parts) + first);
            for (int p = 0; p < parts; p++)
                wr (out, i + first + (mambo / parts) * p, v);
        }
    }

    void smallWorkerGapper (Buffer& in, Buffer& out, int slice)
    {
        (void) in;
        if (! mySlice[slice].isGapped)
            return;

        // gappedType -> (divisions, bit mask of the silenced parts). Only
        // zeros are written, so the order of the original's writes is moot.
        static const unsigned char kGaps[24][2] = {
            { 0, 0 },
            { 1, 1 },                                       // 1: all
            { 2, 1 }, { 2, 2 },                             // 2-3: halves
            { 3, 1 }, { 3, 2 }, { 3, 4 }, { 3, 3 }, { 3, 5 }, { 3, 2 }, // 4-9: thirds (9 == 5)
            { 4, 1 }, { 4, 2 }, { 4, 4 }, { 4, 8 },         // 10-13: one quarter
            { 4, 3 }, { 4, 5 }, { 4, 9 }, { 4, 6 }, { 4, 10 }, { 4, 12 }, // 14-19: two quarters
            { 4, 14 }, { 4, 13 }, { 4, 11 }, { 4, 7 }       // 20-23: three quarters
        };
        const int type = mySlice[slice].gappedType;
        if (type < 1 || type > 23)
            return;

        int mambo = wavesize / numSlices;
        const int first = slice * mambo;
        const int div = kGaps[type][0];
        const int len = mambo / div;
        for (int p = 0; p < div; p++)
            if (kGaps[type][1] & (1 << p))
                for (int i = 0; i < len; i++)
                    wr (out, first + len * p + i, 0.0f);
    }

    void smallWorkerFFT (Buffer& in, Buffer& out, int slice)
    {
        int mambo = wavesize / numSlices;

        if (! mySlice[slice].isFFT)
            return;

        int first = slice * mambo;
        for (int i = 0; i < mambo; i++)
        {
            int destination = (first + (i % (int (mySlice[slice].note + 1) * 110))) + (mySlice[slice].slideamount + i);

            if (destination > wavesize)
                destination = wavesize;
            if (destination < 1)
                destination = 1;

            wr (out, i + first, rd (in, destination));
        }
    }

    void smallWorkerGranny (Buffer& in, Buffer& out, int slice)
    {
        int mambo = wavesize / numSlices;

        if (! mySlice[slice].IsGranny)
            return;

        int first = slice * mambo;
        const int size = (int) (mySlice[slice].grannySize * mambo);
        const int move = (int) (mySlice[slice].grannyMove * mambo);
        for (int i = 0; i < mambo; i++)
        {
            // "doktigt alien crazy"
            wr (out, i + first, rd (in, ((safeMod (i, size)) + (int) (mySlice[slice].grannySpeed * i) + move) + first));
        }
    }

    void smallWorkerSwap (Buffer& in, Buffer& out, int slice)
    {
        int mambo = wavesize / numSlices;

        if (! mySlice[slice].isSwapped)
            return;

        int first = slice * mambo;
        int second = mySlice[slice].swappedWith; // sic: a slice number used as a sample offset
        for (int i = 0; i < mambo; i++)
        {
            wr (out, i + first, rd (in, i + first));
            wr (out, i + second, rd (in, i + second));
            wr (out, i + first, rd (out, i + first) + rd (out, i + second));
            wr (out, i + second, rd (out, i + first) - rd (out, i + second));
            wr (out, i + first, rd (out, i + first) - rd (out, i + second));
        }
    }
};

} // namespace kerr

#endif
