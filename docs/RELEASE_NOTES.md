openDucker 2.0.0 is a complete redesign into a Kickstart-style volume ducker.

- 16 selectable curves with a draggable slope handle, one-cycle waveform display, and curve arrows.
- Length 1/8, 1/4, 1/2 and 1/1 locked to the host tempo.
- SYNC, MIDI and AUDIO triggers. MIDI restarts the curve on note-on to the sample; Audio listens to an optional sidechain input (or the main input) with an adjustable threshold. Retrigger and Shift controls included.
- Mix knob, plus BAND mode that ducks only the low band (30 Hz–1 kHz Linkwitz-Riley split) with a headphone solo.
- Trigger smoothing options, resizable editor, and a new yellow-on-charcoal interface.
- Removed: the v1 Amount knob, Hold/Recovery/Phase, effects section (delay, reverb, sustain, destroy), and mascot artwork.

v1 sessions load, but v1 settings are not carried over because the parameters have changed. New screenshots in the README are captures of the running editor.

Install the macOS universal VST3/AU or Windows x64 VST3 ZIP, or update through OpenVST Manager. Close your DAW before replacing the plugin and rescan afterward. SHA256SUMS.txt covers both archives. macOS bundles are ad-hoc signed; Windows binaries have no publisher certificate. No commercial-DAW listening session is claimed; see docs/VERIFICATION.md for executed checks.
