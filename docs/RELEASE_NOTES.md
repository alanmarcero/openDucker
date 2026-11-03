openDucker v1.3.1 reduces maximum ducking from about 24 dB to about 18 dB at the Smooth curve's midpoint.

- Reduce the extra ducking above 50% Amount. At 100%, Smooth gain halfway through a 120 BPM quarter-note cycle is about -17.83 dB, versus -23.77 dB in v1.3.0.
- Keep 0–50% unchanged, with 50% still matching the maximum from before v1.3.0.
- The live envelope preview follows the adjusted curve. Saved values, automation, Hold, Recovery, Phase and host sync retain their existing controls.

Saved Amount settings above 50% will duck less deeply than v1.3.0. The default remains 65%. For sessions from before v1.3.0, halve Amount values to reproduce their original ducking strength.

Install the macOS universal VST3/AU or Windows x64 VST3 ZIP, or update through OpenVST Manager. Close your DAW before replacing the plugin and rescan afterward. SHA256SUMS.txt covers both archives. macOS bundles are ad-hoc signed; Windows binaries have no publisher certificate.

Ducking follows host PPQ/BPM. Stopped or missing transport timing disables ducking while FX can still ring out. Tail timing remains an estimate. No commercial-DAW listening session is claimed; see docs/VERIFICATION.md for executed checks.
