# Native UI screenshot

`assets/native-ui.png` was captured on macOS on 2026-10-27 from the actual v1.2.0 JUCE processor/editor test harness at 48 kHz and 128 BPM. It shows the default controls, including Hold, Recovery, Phase, Parallel FX flow and Always Destroy. FX starts disabled. The 720 × 900 native editor is captured at 1.5× (1080 × 1350); controls, artwork and version labels come from the running editor.

Reproduce with `ctest --test-dir build --output-on-failure`. The native test writes its snapshots into `build/screenshots/`; the README uses `openDucker.png`.

This is an application capture, not a generated mockup. The source artwork remains documented in `assets/ARTWORK.md` and `assets/ANALOG-CYBER.md`.
