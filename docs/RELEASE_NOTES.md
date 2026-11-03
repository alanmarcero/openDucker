openDucker v1.3.0 expands the Amount control so useful ducking sits lower on the dial and 100% delivers extreme pumping.

- Keep the 0–100 control, with the previous maximum now at 50%.
- Above 50%, progressively deepen the recovery curve. At 100%, Smooth is about 24 dB down halfway through the beat, compared with about 6 dB at the previous maximum.
- Update the live envelope preview and Amount tooltip to match the stronger range. Zero Amount remains transparent; Hold, Recovery, Phase and host sync still work across the full range.
- Include the reverb improvements made since v1.2.0: denser diffusion, modulated delays and stronger high-frequency damping.
- Default new delay instances to 1/8 dotted so repeats fall between duck pulses. Existing sessions keep their saved delay length.

Saved Amount settings and automation retain their values and will sound stronger. Halve previous Amount values to reproduce the old ducking strength; the new default remains 65%.

Install the macOS universal VST3/AU or Windows x64 VST3 ZIP, or update through OpenVST Manager. Close your DAW before replacing the plugin and rescan afterward. SHA256SUMS.txt covers both archives. macOS bundles are ad-hoc signed; Windows binaries have no publisher certificate.

Ducking follows host PPQ/BPM. Stopped or missing transport timing disables ducking while FX can still ring out. Tail timing remains an estimate. No commercial-DAW listening session is claimed; see docs/VERIFICATION.md for executed checks.
