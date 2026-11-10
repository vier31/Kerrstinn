// Runs a script of operations on the port and dumps both processed buffers
// after each one; emulate_original.py runs the same script on the original
// 2007 Mac binary and compares.
//
// usage: engine_cli input.bin ops.txt out.bin
//   input.bin: int32 frames, then frames float L, frames float R
//   ops: "init" | "load" | "set <index> <float bits, hex>" | "windows"
#include "KerrFloatLoop.h"

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>

int main (int argc, char** argv)
{
    if (argc != 4)
    {
        std::fprintf (stderr, "usage: engine_cli input.bin ops.txt out.bin\n");
        return 1;
    }

    std::ifstream in (argv[1], std::ios::binary);
    int32_t n = 0;
    in.read ((char*) &n, 4);
    std::vector<float> left ((size_t) n), right ((size_t) n);
    in.read ((char*) left.data(), n * 4);
    in.read ((char*) right.data(), n * 4);

    kerr::FloatKerrStinn k;
    kerr::FloatLoop store;
    bool loaded = false;
    std::ifstream ops (argv[2]);
    std::FILE* out = std::fopen (argv[3], "wb");
    std::string line;

    while (std::getline (ops, line))
    {
        std::istringstream ls (line);
        std::string op;
        ls >> op;

        if (op == "init")
        {
            for (int idx : kerr::kRestoreOrder)
                k.setParameter (idx, k.getParameter (idx));
        }
        else if (op == "load")
        {
            // The emulator calls loadFile()'s processing directly, without
            // the editor's follow-up setParameter(666).
            store.load (k, left.data(), right.data(), n);
            const int end = k.beginLoad();
            for (int i = 1; i < end; i++)
                k.work (i);
            loaded = true;
        }
        else if (op == "set")
        {
            int idx = 0;
            std::string hex;
            ls >> idx >> hex;
            uint32_t bits = (uint32_t) std::strtoul (hex.c_str(), nullptr, 16);
            float v;
            std::memcpy (&v, &bits, 4);
            k.setParameter (idx, v);
        }
        else if (op == "windows")
        {
            k.engine.random().setFlavour (kerr::Rand::kWindows);
        }
        else
        {
            continue;
        }

        const std::vector<float> silence ((size_t) n, 0.0f);
        for (int ch = 0; ch < 2; ++ch)
            std::fwrite (loaded ? store.processed (ch) : silence.data(), 4, (size_t) n, out);
    }

    std::fclose (out);
    return 0;
}
