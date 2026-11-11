# KerrStinn

KerrStinn is a loop slicer and glitcher by Johan Larsby (shuriken.se),
released in 2007 as a freeware VST. Its source was published in 2026 under
the GPL. This repository holds the original source and new ports built on it:

| Folder | What |
| --- | --- |
| `original/` | Johan Larsby's VST 2 source (Windows), artwork, `fjord.wav` and the manual, as published |
| `engine/` | The slicing engine, ported from `original/engine*.cpp`: header-only C++98, shared by all ports |
| `plugin/` | VST3 / AU / Standalone (JUCE 8), with the original editor artwork and layout |
| `rack/` | VCV Rack 2 module, clocked by a 24 PPQN input |
| `tests/` | Engine tests against the original code |

## Using it

The plugin is an instrument. Load it on a MIDI track, click the waveform (or
drop a file on it) to load a loop, then start the transport. Playback follows
the host's bar position, so the loop is stretched to *Bars* bars at any tempo.
Every knob change re-slices the loop **on top of the previous result**, as in
the original, so a session builds up its own history. Right-click for the
menu: the Mac and Windows builds of 2007 used different `rand()` functions,
and the menu picks which one's patterns a seed gives.

The loop path and a copy of the file (≤ 64 MB) are saved with the project.

## Build

```sh
git submodule update --init                     # JUCE 8.0.15 in plugin/JUCE
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build                             # plugin, copied to ~/Library/Audio/Plug-Ins
cmake --build build --target check              # engine tests

cd rack && make && make install                 # Rack module (RACK_DIR: Rack SDK 2.6.x)
```

## Verification

| Test | What it checks |
| --- | --- |
| `reference_check` | Compiles `original/engine.cpp` and `engineWorkers.cpp` as published and runs random sessions (all 16 generators, loads, project restores, both `rand()` flavours) through the original and the port. Every processed sample must be bit-identical after every operation. |
| `rand_check` | The Mac `rand()` flavour matches macOS `rand()` for every seed the Random knob can produce. |
| `tests/emulate_original.py` | Runs the original 2007 Mac binary (not included) in an x86 emulator and compares it with `engine_cli`. 3865 of 3868 operations are bit-identical; the rest are the NaN case below. This also shows that the published source is the code behind the 1.2 release. |
| `host_test` (`plugin/`) | Drives the plugin processor like a host: loading, parameter changes, playback against the transport, project save/restore, a moved loop file. |
| `module_test` (`rack/`, `make test`) | Drives the Rack module with a 24 PPQN clock: playback, reset, CV latching, re-glitch, patch restore. |
| `cxx98_check` | The engine compiles as C++98 with warnings as errors, for the firmware. |

`auval` passes for the AU.

## Differences from the original

The engine follows the original line by line, including its quirks: in-place
processing, a slice number used as a sample offset in RythmSickness, the
Slices loop bound of 63, generators 17–26 that do nothing. It differs only
where the original's behaviour was undefined:

* Reads past the loop return 0, writes past it are dropped.
* Integer division by zero (General Cluster at 2 slices, Granny on very short
  slices) no longer crashes: `x / 0` is 0 and `x % 0` is `x`.
* PiiTiiVii at amount 1.0 (only reachable with Bisyness 100) outputs 0 where
  the original produced NaN.

The ports also differ from the 2007 plugin in a few ways:

* Playback no longer depends on the host's buffer size (the original divided
  the bar position by bar length + block size).
* Loops can be any format JUCE (plugin) or dr_wav (Rack) reads. The original
  only read 8/16-bit WAV, and its 8-bit path was nearly silent. 16-bit files
  are still scaled by 1/32767, as in the original.
* There is no audio input. The original passed its input through until a loop
  was loaded.
* The original had 16 program slots; the plugin has one state per instance.

## Licence

The code is licensed under the GNU General Public License, version 3 or later
([LICENSE](LICENSE)). The original artwork, `fjord.wav` and the manual are
licensed under CC BY-SA 4.0 ([LICENSE-ASSETS](LICENSE-ASSETS)); the plugin's
editor uses that artwork. See [LICENSES.md](LICENSES.md).

KerrStinn © 2007 Johan Larsby. Ports © 2026 Jan Schröder.
