# Native UI screenshots

The three images in `docs/screenshots/` were captured on macOS on 2026-11-17 from the actual v2.4.1 JUCE processor and editor test harness (`Tools/PluginTests.cpp`), rendered at 2x (2000 × 1520) from the 1000 × 760 native editor with the FX panel expanded. Each is preceded by rendering a synthetic kick and bass through the processor so the display and output meter show real signal peaks. The detector filter row is visible above the FX rack:

- `openDucker-sync.png`: Sync mode at 128 BPM, 1/4 length, Classic S curve, Mix 79%, FX On.
- `openDucker-audio.png`: Audio trigger, Trapezoid curve, slope length 72%, FX On.
- `openDucker-band-midi.png`: MIDI trigger, Valley curve, BAND enabled at 150 Hz, FX On.

Reproduce with `ctest --test-dir build --output-on-failure`; the native test writes its snapshots into `build/screenshots/`. These are application captures, not mockups.
