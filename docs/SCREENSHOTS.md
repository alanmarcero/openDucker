# Native UI screenshots

The three images in `docs/screenshots/` were captured on macOS on 2026-11-06 from the actual v2.0.0 JUCE processor and editor test harness (`Tools/PluginTests.cpp`), rendered at 2x (2000 × 1020) from the 1000 × 510 native editor. Each is preceded by rendering a synthetic kick and bass through the processor so the display shows real input and output peaks:

- `openDucker-sync.png`: Sync mode at 128 BPM, 1/4 length, Classic S curve, Mix 79%.
- `openDucker-audio.png`: Audio trigger, Trapezoid curve, slope length 72%.
- `openDucker-band-midi.png`: MIDI trigger, Valley curve, BAND enabled at 150 Hz.

Reproduce with `ctest --test-dir build --output-on-failure`; the native test writes its snapshots into `build/screenshots/`. These are application captures, not mockups.
