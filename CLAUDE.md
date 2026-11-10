# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

KerrStinn (Johan Larsby, 2007) and its ports. Fork of github.com/Larsby/Kerrstinn (`upstream`), which is not maintained; `origin` is vier31/Kerrstinn (public, it is a fork of a public repo). GPL-3.0-or-later code, CC BY-SA 4.0 assets (LICENSES.md). The Clouds firmware is a sibling repo, `../uClouds-Firmware`, which vendors `engine/`.

## Commands

```sh
git submodule update --init                       # JUCE 8.0.15 in plugin/JUCE
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build                               # VST3/AU/Standalone, copied to ~/Library/Audio/Plug-Ins
cmake --build build --target check                # rand_check + reference_check (two seeds); builds cxx98_check
cmake --build build --target host_test && ./build/plugin/host_test_artefacts/Release/host_test
KERR_DEBUG=1 ./build/tests/reference_check 1 <seed>   # dumps slice state at the first mismatch

cd rack && make && make test && make install      # RACK_DIR defaults to ~/Code/toolchains/Rack-SDK-2.6.4

# Against the original 2007 Mac binary (not in the repo; needs a venv with unicorn + numpy):
python tests/emulate_original.py <KerrStinn.vst/Contents/MacOS/KerrStinn> build/tests/engine_cli 150 777
```

`-DKERR_BUILD_PLUGIN=OFF` configures only the engine tests (no JUCE needed). `auval -v aumu Kstn Shrk` should pass.

## Architecture

- `original/`: Larsby's published source. **Never edit**: `reference_check` compiles `engine.cpp` and `engineWorkers.cpp` from here. `tests/reference/patch_original.cmake` applies two patches in the build tree (x86 `abs((int) NaN)`, NaN → 0) and fails if their text disappears; `shim.h` swaps `rand()` for `kerr::Rand` and pads allocations with zeros.
- `engine/`: header-only, **C++98, no allocation**, because the firmware compiles it too. `KerrEngine.h` is `engine` (`setParams` + the `smallWorker*` effects, kept line by line), templated on a `Buffer` with `get`/`set` over `length + 1` samples (index `length` is a real guard sample the original reads and writes). `KerrStinn.h` is the host-independent part of `AKerrStinn`: parameter mapping, the reprocessing pass (`begin*()` + `work(i)` so the firmware can step it), `restore()` (all six values replaced first, then six passes in `kRestoreOrder`), and `positionFor()`/`indexFor()` for playback. `KerrFloatLoop.h` is the desktop float storage.
- **Bit-exactness is the main constraint.** Keep the original's evaluation order, `rand()` call order and int/float/double conversions. Build with `-ffp-contract=off` and without fast-math (`KERR_FP_FLAGS`; the Rack Makefile overrides the SDK's `-funsafe-math-optimizations` for `KerrStinn.cpp`). After any engine change run `check`, then `make check_engine` / `sync_engine` in the firmware.
- `plugin/Source/PluginProcessor.*`: the engine runs on a `juce::Thread`; the message thread posts `pendingLoad` / `pendingRestore` / `pendingRecompute` under `requestLock`, the worker applies changed APVTS params in index order and publishes ref-counted `Snapshot`s, which `processBlock` picks up with a try-lock. Old snapshots are freed on the worker (`releasePool`). State: APVTS params + rand flavour + loop path + embedded loop file. Plugin codes `Shrk`/`Kstn` and parameter IDs must not change (existing projects).
- `rack/src/KerrStinn.cpp`: same threading model with `std::thread`; 24 PPQN clock, CV only counts when it changes a parameter's step (or is latched on TRIG), patch storage keeps a copy of the WAV.
