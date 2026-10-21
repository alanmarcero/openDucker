# openDucker design

An original one-knob rhythmic volume shaper for producers. The amount knob controls ducking depth from dry to silence on each pulse. Four selectable recovery slopes and a beat-rate selector provide the musical variation; no sidechain routing is required. DJ Duck is an original yellow duck in headphones at a compact DJ deck.

## Architecture

Use C++20 and JUCE 8.0.9, matching the existing openSausage plugin family. A dependency-free DSP core owns the envelope and smoothing. JUCE adapts host transport, parameters and state; its native editor owns artwork and visualization. Ship VST3 for macOS universal and Windows x64, plus AU on macOS. No standalone edition: a host is necessary for musical sync.

Alternative: a free-running tempo oscillator is simple but drifts and fails seeks. A custom VST3 SDK implementation gives lower-level timing access but duplicates the existing cross-platform wrapper and AU support. Choose JUCE with an absolute PPQ anchor every block and double-precision per-sample musical position.

## Timing and audio contract

- Zero reported latency; no audio lookahead, allocation, locks or file operations in processing.
- For each sample, phase comes from host PPQ at block start plus sample offset times BPM/(60 * sample rate), divided by the selected cycle length.
- Rate choices: 1/2, 1/4 (default), 1/8, 1/16, 1/4 triplet, 1/8 dotted. Values are 2, 1, 0.5, 0.25, 2/3, 0.75 quarter notes.
- Re-anchor every block so play, seek, loop and tempo changes never accumulate clock drift. Honor reported loop points within a buffer. Negative PPQ is valid.
- Smooth, Punch, Snap, Swell all reach minimum gain at phase zero. Recovery curves join a short smooth pre-duck ramp before the next beat; the boundary has continuous value and first derivative. This small pre-duck is part of the shape, not audio latency.
- Interpolate amount and shape/rate changes over 5 ms without delaying the underlying beat phase. No smoothing filter is applied to the periodic envelope. Transport discontinuities crossfade the old gain into the new host-aligned envelope over 2 ms; this deliberately trades the first 2 ms of amplitude after a seek for click suppression, without slewing the phase.
- Missing/invalid PPQ or tempo, or stopped transport, ramps to dry. Never invent a free-running clock and claim it is synced. The UI distinguishes Playing, Stopped and No host sync.
- Gain is always in [0,1], applied equally to all channels. No amplification, saturation or hard clipper; already clipped input cannot be repaired. Float and double precision, mono and stereo supported.
- Host data is the accuracy boundary: block-level APIs cannot reveal unreported intra-buffer tempo ramps/seeks. Test and document this rather than promise host-independent timing perfection.

## UI

720 x 460 native editor. Charcoal #151B20, panel #202A30, mint #8FE2CE, yellow #FFCF53, paper #F2F1E8, muted #9EADB5. One large amount knob on the left; original DJ Duck on the right; a full-width gain envelope below with a host phase cursor. Four slope buttons, one rate dropdown and one power/bypass button. Large readable sans-serif typography, restrained rounded surfaces, clear keyboard focus and descriptive tooltips. No fake meters or implementation copy.

## Verification and release

Test exact beat minima, buffer partition invariance, long timelines, sample rates 44.1–192 kHz, tempo changes, negative PPQ, loops, seeks, invalid timing, transport start/stop, bounded output and stereo identity. Include continuous-boundary and automation transient checks. Test native parameter controls, session recall, float/double processing and capture actual editor screenshots. Run pluginval and AU validation locally where possible; CI repeats DSP/sanitizers and native builds on macOS/Windows. Release ZIPs include exact bundle names and checksums. Update the manager catalog after real release assets exist and prove install/remove using isolated folders.

Publish a public GPL-3.0 repository under alanmarcero/openDucker; preserve JUCE notices. The user explicitly requested planning, development, publication and manager integration in one task.

## Primary timing references

- https://docs.juce.com/master/classjuce_1_1AudioPlayHead_1_1PositionInfo.html
- https://steinbergmedia.github.io/vst3_doc/vstinterfaces/structSteinberg_1_1Vst_1_1ProcessContext.html
