openDucker v1.2.0 improves FX audibility, makes routing explicit, and adds ducker envelope controls.

- Add Hold, Recovery and Phase controls with a live envelope preview and click-suppressed automation.
- Add Parallel FX flow so delay echoes and immediate reverb remain audible. Serial flow remains available.
- Fix Sustain being silently skipped with Delay and Reverb off.
- Add Always Destroy for continuous audio and standalone use. Tail mode retains phrase-end processing.
- Correct the serial delay-plus-reverb tail estimate and report standalone Destroy's stutter tail to hosts.
- Show whether the dry signal and FX are clean, ducked before FX, or ducked after FX.
- Preserve older sessions' Serial/Tail modes, envelope defaults and existing automation parameter IDs.
- Verify every Dry/Wet/Both × Before/After combination for each FX stage and both flows in float and double precision.

Install the macOS universal VST3/AU or Windows x64 VST3 ZIP, or update through OpenVST Manager. Close your DAW before replacing the plugin and rescan afterward. SHA256SUMS.txt covers both archives. macOS bundles are ad-hoc signed; Windows binaries have no publisher certificate.

Ducking follows host PPQ/BPM. Stopped or missing transport timing disables ducking while FX can still ring out. Tail timing remains an estimate. No commercial-DAW listening session is claimed; see docs/VERIFICATION.md for executed checks.
