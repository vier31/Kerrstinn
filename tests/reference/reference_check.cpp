// Runs random sessions through Johan Larsby's original engine (original/*.cpp,
// compiled with shim.h) and through the port, and compares the processed
// buffers bit for bit after every operation.
//
// usage: reference_check [sessions=200] [seed=1] [fjord.wav]

#include "../../engine/KerrFloatLoop.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <memory>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <random>
#include <string>
#include <vector>

#define protected public
#include "engine.h" // original/engine.h
#undef protected

#undef rand
#undef srand
#undef malloc
#undef realloc

//------------------------------------------------------------------------
// shim.h implementations

static kerr::Rand refRng;
void ref_srand (unsigned int seed) { refRng.seed (seed); }
int ref_rand() { return refRng.next(); }

// Room for the worst overrun (Granny reads up to two slices past the end).
void* ref_malloc (size_t n) { return std::calloc (1, 3 * n + 4096); }
void* ref_realloc (void* p, size_t n)
{
    std::free (p);
    return ref_malloc (n);
}
int ref_abs_trunc (float x) { return kerr::absTrunc (x); }

//------------------------------------------------------------------------
// The parts of AKerrStinn (original/KerrStinn.cpp) that drive the engine,
// copied as they are.

struct RefPlugin
{
    engine myEngine;
    float p[kerr::kNumParams];
    int numbars = 1, mySlices = 8;

    RefPlugin()
    {
        for (int i = 0; i < kerr::kNumParams; i++)
            p[i] = kerr::defaultValue (i);
        setProgram ("");
    }

    void setParameter (int index, float value)
    {
        if (index >= 0 && index < kerr::kNumParams)
            p[index] = value;
        numbars = int (p[kerr::kNumBars] * 7.99) + 1;
        mySlices = (int) (((p[kerr::kSlices] * 32) + 1)) * 2;
        myEngine.setParams (p[kerr::kRand] * 65535, mySlices, false, p[kerr::kBisyness] * 100,
                            (p[kerr::kGenerationMatrix] * (float) (DnumTypes - 1) + 0.99) + 1);
        for (int i = 1; i < ((p[kerr::kSlices] * 63) + 1); i++)
            myEngine.bigWorker (i);
    }

    void setProgram (const char* file)
    {
        if (std::strlen (file) != 0)
            myEngine.loadFile ((char*) file);
        setParameter (kerr::kRand, p[kerr::kRand]);
        setParameter (kerr::kSlices, p[kerr::kSlices]);
        setParameter (kerr::kBisyness, p[kerr::kBisyness]);
        setParameter (kerr::kGenerationMatrix, p[kerr::kGenerationMatrix]);
        setParameter (kerr::kOn, p[kerr::kOn]);
        setParameter (kerr::kNumBars, p[kerr::kNumBars]);
    }

    // KerrStinnEditor::valueChanged() for the waveform (tag 666).
    void editorLoad (const char* file)
    {
        myEngine.loadFile ((char*) file);
        setParameter (666, 0.0f);
    }
};

//------------------------------------------------------------------------

struct Loop
{
    int channels = 2;
    std::vector<int16_t> pcm; // interleaved
    int frames() const { return (int) pcm.size() / channels; }
};

static void put32 (std::FILE* f, uint32_t v) { std::fwrite (&v, 4, 1, f); }
static void put16 (std::FILE* f, uint16_t v) { std::fwrite (&v, 2, 1, f); }

static void writeWav (const std::string& path, const Loop& l)
{
    std::FILE* f = std::fopen (path.c_str(), "wb");
    const uint32_t bytes = (uint32_t) l.pcm.size() * 2;
    std::fwrite ("RIFF", 4, 1, f);
    put32 (f, 36 + bytes);
    std::fwrite ("WAVEfmt ", 8, 1, f);
    put32 (f, 16);
    put16 (f, 1);
    put16 (f, (uint16_t) l.channels);
    put32 (f, 44100);
    put32 (f, 44100 * 2 * l.channels);
    put16 (f, (uint16_t) (2 * l.channels));
    put16 (f, 16);
    std::fwrite ("data", 4, 1, f);
    put32 (f, bytes);
    std::fwrite (l.pcm.data(), 2, l.pcm.size(), f);
    std::fclose (f);
}

static bool readWav16 (const char* path, Loop& l)
{
    std::FILE* f = std::fopen (path, "rb");
    if (! f)
        return false;
    std::vector<uint8_t> d;
    uint8_t buf[65536];
    size_t n;
    while ((n = std::fread (buf, 1, sizeof buf, f)) > 0)
        d.insert (d.end(), buf, buf + n);
    std::fclose (f);
    for (size_t pos = 12; pos + 8 <= d.size();)
    {
        uint32_t len;
        std::memcpy (&len, &d[pos + 4], 4);
        if (! std::memcmp (&d[pos], "fmt ", 4))
            l.channels = d[pos + 10];
        if (! std::memcmp (&d[pos], "data", 4))
        {
            l.pcm.resize (len / 2);
            std::memcpy (l.pcm.data(), &d[pos + 8], l.pcm.size() * 2);
            return true;
        }
        pos += 8 + len + (len & 1);
    }
    return false;
}

static void portLoad (kerr::FloatKerrStinn& k, kerr::FloatLoop& store, const Loop& l)
{
    const int n = l.frames();
    std::vector<float> left ((size_t) n), right ((size_t) n);
    for (int i = 0; i < n; i++)
    {
        left[(size_t) i] = kerr::fromPcm16 (l.pcm[(size_t) (i * l.channels)]);
        right[(size_t) i] = kerr::fromPcm16 (l.pcm[(size_t) (i * l.channels + l.channels - 1)]);
    }
    store.load (k, left.data(), l.channels == 2 ? right.data() : nullptr, n);
}

int main (int argc, char** argv)
{
    const int sessions = argc > 1 ? std::atoi (argv[1]) : 200;
    const unsigned seed = argc > 2 ? (unsigned) std::atoi (argv[2]) : 1u;
    Loop fjord;
    const bool haveFjord = argc > 3 && readWav16 (argv[3], fjord);

    std::mt19937 gen (seed);
    auto uni = [&] (int lo, int hi) { return std::uniform_int_distribution<int> (lo, hi) (gen); };
    auto value = [&]() -> float {
        const int r = uni (0, 9);
        if (r == 0) return 0.0f;
        if (r == 1) return 1.0f;
        return std::uniform_real_distribution<float> (0.0f, 1.0f) (gen);
    };

    const std::string wav = std::string (std::getenv ("TMPDIR") ? std::getenv ("TMPDIR") : "/tmp")
                            + "/kerr_reference_" + std::to_string (seed) + ".wav";
    long ops = 0, bad = 0;
    int coverage[kerr::kNumTypes + 1] = {};

    for (int s = 0; s < sessions; s++)
    {
        auto ref = std::make_unique<RefPlugin>();
        auto port = std::make_unique<kerr::FloatKerrStinn>();
        kerr::FloatLoop store;
        Loop loop;
        const bool windows = uni (0, 3) == 0;
        refRng.setFlavour (windows ? kerr::Rand::kWindows : kerr::Rand::kMac);
        port->engine.random().setFlavour (refRng.flavour());

        for (int op = 0; op < 30; op++)
        {
            const int kind = op == 0 ? 0 : uni (0, 19);
            std::string what;
            if (kind == 0) // load a loop via the editor
            {
                if (haveFjord && uni (0, 3) == 0)
                    loop = fjord;
                else
                {
                    loop.channels = uni (1, 2);
                    loop.pcm.resize ((size_t) (uni (300, 40000) * loop.channels));
                    double ph = 0, f = uni (20, 2000) / 44100.0;
                    for (auto& x : loop.pcm)
                        x = (int16_t) (uni (0, 3) ? 20000 * std::sin (ph += f) : uni (-32768, 32767));
                }
                writeWav (wav, loop);
                ref->editorLoad (wav.c_str());
                portLoad (*port, store, loop);
                port->loaded();
                what = "load " + std::to_string (loop.frames()) + "x" + std::to_string (loop.channels);
            }
            else if (kind == 1) // project reload (setChunk)
            {
                float v[kerr::kNumParams];
                for (float& x : v)
                    x = value();
                std::memcpy (ref->p, v, sizeof v);
                ref->setProgram (wav.c_str());
                portLoad (*port, store, loop);
                port->restore (v);
                what = "restore";
            }
            else
            {
                const int idx = uni (0, kerr::kNumParams - 1);
                const float v = idx == kerr::kGenerationMatrix && uni (0, 1) ? uni (0, 15) / 25.0f : value();
                ref->setParameter (idx, v);
                port->setParameter (idx, v);
                what = "set " + std::to_string (idx) + " " + std::to_string (v);
            }
            coverage[kerr::generatorFor (port->getParameter (kerr::kGenerationMatrix))]++;
            ops++;

            const int n = loop.frames();
            for (int ch = 0; ch < 2; ch++)
            {
                const float* a = ref->myEngine.getPointerToData (ch);
                const float* b = store.processed (ch);
                for (int i = 0; i <= n; i++)
                    if (std::memcmp (&a[i], &b[i], 4) != 0)
                    {
                        if (bad < 10)
                            std::printf ("session %d op %d (%s, gen %d, %d slices): ch %d sample %d of %d: ref %.9g port %.9g\n",
                                         s, op, what.c_str(), kerr::generatorFor (port->getParameter (kerr::kGenerationMatrix)),
                                         port->getNumSlices(), ch, i, n, a[i], b[i]);
                        if (bad == 0 && std::getenv ("KERR_DEBUG"))
                        {
                            const int sl = i / (n / port->getNumSlices());
                            std::printf ("  in[%d] ref %.9g port %.9g\n", i, (ch ? ref->myEngine.InWaveR : ref->myEngine.InWave)[i], port->engine.in[ch].get (i));
                            for (int q = i - 3; q < i + 6; q++) std::printf ("    %d: in %.6f ref %.6f port %.6f\n", q, port->engine.in[ch].get (q), a[q], b[q]);
                            for (int q = std::max (0, sl - 1); q <= sl + 1; q++)
                            {
                                const auto& r = ref->myEngine.mySlice[q];
                                const auto& o = port->engine.sliceState (q);
                                std::printf ("  slice %d ref: bw%d cp%d pos%d mix%d pmix%d sil%d dist%d amt%.6g thr%.6g gap%d/%d fft%d note%d sl%d gr%d sw%d/%d ar%d%d %d/%d\n", q,
                                    r.isBackward, r.changedPosition, r.position, r.changedPositionMIX, r.positionMIX, r.changedPositionSilence, r.isDistorted, r.DistortionAmount, r.distortionThreshold, r.isGapped, r.gappedType, r.isFFT, r.note, r.slideamount, r.IsGranny, r.isSwapped, r.swappedWith, r.isAttack, r.isRelease, r.attack, r.release);
                                std::printf ("  slice %d prt: bw%d cp%d pos%d mix%d pmix%d sil%d dist%d amt%.6g thr%.6g gap%d/%d fft%d note%d sl%d gr%d sw%d/%d ar%d%d %d/%d\n", q,
                                    o.isBackward, o.changedPosition, o.position, o.changedPositionMIX, o.positionMIX, o.changedPositionSilence, o.isDistorted, o.DistortionAmount, o.distortionThreshold, o.isGapped, o.gappedType, o.isFFT, o.note, o.slideamount, o.IsGranny, o.isSwapped, o.swappedWith, o.isAttack, o.isRelease, o.attack, o.release);
                            }
                        }
                        bad++;
                        ch = 2;
                        break;
                    }
            }
        }
    }
    std::remove (wav.c_str());

    std::printf ("ops/generator:");
    for (int g = 1; g <= kerr::kNumTypes; g++)
        std::printf (" %d", coverage[g]);
    std::printf ("\n%ld operations, %ld mismatching\n", ops, bad);
    return bad == 0 ? 0 : 1;
}
