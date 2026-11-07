openDucker 2.1.0 restores the full openDucker FX rack into the openDucker 2 interface.

- **Restored FX chain:** Tempo-synced Delay (`1/32` to `1 bar`, Mono / Ping-pong, Feedback, Bass & Treble EQ shelves), modulated stereo Reverb (`Room`, `Hall`, `Plate`, 0.2–25 s decay, Width, Bass & Treble EQ shelves), Sustain compression, Destroy bitcrush/stutter/drive (`Tail` / `Always`), FX Mix, and Output trim (−24 to +12 dB) with live peak meter.
- **Flexible ducking & FX routing:** Duck `Dry`, `Wet`, or `Both` (`Before FX` or `After FX`), and run Delay + Reverb in `Serial` or `Parallel`.
- **Collapsible 3-card FX rack:** Integrated across the bottom of the 1000 px wide openDucker 2 layout with a `Hide FX` / `Show FX` toggle button alongside the BAND controls.
- **Full v2.0.0 ducker capabilities preserved:** 16 curves, draggable slope handle, dual input/output waveform display, SYNC / MIDI / AUDIO triggers with sidechain support, Retrigger, Shift, BAND Linkwitz-Riley low-end split with solo, and the animated DJ Duck booth.

v2.0.0 sessions recall cleanly with FX disabled by default so existing sessions sound identical on upgrade.

Install the macOS universal VST3/AU or Windows x64 VST3 ZIP, or update through OpenVST Manager. Close your DAW before replacing the plugin and rescan afterward. SHA256SUMS.txt covers both archives. macOS bundles are ad-hoc signed; Windows binaries have no publisher certificate. No commercial-DAW listening session is claimed; see docs/VERIFICATION.md for executed checks.
