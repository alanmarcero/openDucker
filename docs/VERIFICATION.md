# Verification

## v1.2.0 algorithm and routing update

Local verification on 2026-10-27, Apple Silicon macOS, JUCE 8.0.9:

- Release AU and VST3 builds pass; both bundle signatures verify and report version 1.2.0.
- CTest passes core, effects and native plugin suites (3/3). ASan/UBSan passes both DSP suites (2/2).
- The built VST3 passes pluginval 1.0.4 at strictness level 5.
- 72 actual-processor routing cases cover Dry/Wet/Both × Before/After across Delay, Reverb, Sustain, Always Destroy, Parallel and Serial in float and double. They compare output samples against clean and pre-enveloped reference renders.
- New regressions reproduced standalone Sustain being skipped and serial tail estimates excluding the delay. Both pass after fixes. Standalone Destroy is audible on continuous input and reports its short stutter tail to hosts. Parallel reverb starts before the first delay repeat; fully wet processing does not leak the source.
- Hold, Recovery and Phase move/shape the actual audio envelope. Core checks cover buffer partition invariance at 44.1–192 kHz, step automation, and continuously automated Phase at block sizes 1, 16, 64 and 256. Automation regressions caught a phase-sweep click and a one-sample-block stall; both were fixed.
- New controls are exercised in the native editor and round-trip through session state. Missing new controls restore legacy envelope/Serial/Tail settings, even on a previously edited processor. Existing parameter IDs and version hints remain unchanged.
- The native editor capture was inspected at 1080 × 1350. A pre-existing intermittent spark-frame comparison now allows up to three frames to observe movement, retaining its original pixel threshold.
- Independent review and focused re-review found no remaining actionable issue after the automation and Destroy-tail fixes.
- Release publisher tests pass (11/11). The tagged release workflow also builds and validates macOS universal and native Windows x64 before publishing packages.

These are offline audio renders, native control tests and plugin-host validation. No commercial-DAW listening session is claimed. Clean routing means free of the ducking envelope; effects can still color or amplify the wet branch.

## Effects stage and animated editor

Local verification on 2026-10-21, Apple Silicon macOS, JUCE 8.0.9:

- Release VST3 and AU builds pass; both bundle signatures verify.
- CTest: core, effects, and native plugin suites pass (3/3).
- Effects suite passes AddressSanitizer and UndefinedBehaviorSanitizer, including full-buffer reset and subsequent silence.
- VST3 passes pluginval strictness level 5. A separate local AU factory harness loads the built component, selects Plate, renders 102,400 stereo frames, then verifies silence after an explicit AU reset. This does not replace or register the installed plugin globally.
- Direct old/new AU comparison confirms the original four parameter IDs and order are preserved; 19 new controls follow them.
- At a 25-second setting, measured RT60 at 48 kHz is 23.82 seconds for Room, 24.52 for Hall, and 24.41 for Plate.
- DSP coverage includes delay timing/feedback, mono and ping-pong including mode changes with stored echoes, independent EQ shelves, width, sustain, final-tail Destroy, short-tail activity, smooth stage/time changes, and reset without stale history. Stability checks cover 44.1–192 kHz.
- Native tests exercise all Dry/Wet/Both and Before/After combinations in float and double, full bypass, mono output and metering, fully wet startup, old/new state recall, host-reset silence, EQ drag detents with continuous automation, every control, panel collapse, host-synced speaker motion, and animated sparks at 70%. The 1080 × 1200 editor capture was inspected.
- Independent DSP review and focused re-review pass after correcting short-tail activation, existing-tail mono conversion, and bulk reset work. Reset now invalidates buffer history without clearing sample storage; measured median 0.167 microseconds at 48/192/384 kHz.
- A local optimized effects benchmark processed six seconds of audio in 0.034 seconds at 48 kHz and 0.145 seconds at 192 kHz, with both effects and Sustain active. These hardware-specific figures exclude host/editor overhead and active Destroy processing.

No commercial-DAW listening session is claimed. Tail timing is an estimate, and serial delay/reverb energy can continue beyond it.

## Original release verification

Local verification on 2026-10-21, Apple Silicon macOS, AppleClang 21, JUCE 8.0.9:

- Release CTest: core and native plugin suites pass.
- Independent code review and re-review: clean after fixing loop/tempo discontinuities and immediate slope feedback.
- Debug DSP suite with AddressSanitizer and UndefinedBehaviorSanitizer: pass.
- pluginval 1.0.4, strictness level 5, built VST3: pass.
- macOS AU: Apple's auval and pluginval strictness level 5 pass after local registration.
- Native screenshots reviewed at 1080 x 690; slope, rate, bypass and amount bindings exercised, including immediate host automation feedback and session recall.
- OpenVST Manager catalog change: 63 tests pass; TypeScript/frontend production build passes.
- OpenVST Manager UI: nine Playwright tests pass with Chrome and Node 24.
- A local optimized core benchmark generated 5.12 million gain values in 0.048 seconds (about 0.045% of one realtime 48 kHz core). This excludes host/editor overhead and is hardware-specific.

## DSP coverage

Exact pulse minima at samples 0, 24000 and 48000 for 120 BPM at 48 kHz; partition invariance across buffers of 1, 7, 64, 511, 1024 and 4096 samples; 44.1, 48, 88.2, 96 and 192 kHz; negative timeline positions; one million quarter notes into a session; all four slopes and six rates; gain bounds; continuity at cycle boundaries; tempo changes; invalid/missing tempo or PPQ; stop/start and seek transitions; transparent zero amount and control smoothing.

Review found two transition defects not caught by the initial suite. Both were reproduced and fixed before publication: fractional loop discontinuities now blend amplitude without changing phase; tempo changes smooth envelope width independently of phase. Regression tests cover in-buffer and block-boundary fractional loops, dotted rates, whole-cycle loop minima, loop partition invariance and tempo changes inside pre-duck. Floating-point rounding at exact loop endpoints is also covered by the partition comparison.

## Accuracy limits

These tests prove the plugin's behavior against supplied host timing; they do not certify every DAW. No manual commercial-DAW listening session is claimed. Hosts must provide valid PPQ/BPM and report timing changes at audio callback boundaries. The README describes the 2 ms discontinuity transitions and missing-timing behavior.

GitHub Actions rebuilds and validates the release on macOS universal and Windows x64. The release workflow publishes only after all jobs pass. Release-specific results and manager installation evidence are recorded with the release.

## Published release

[v1.0.0](https://github.com/alanmarcero/openDucker/releases/tag/v1.0.0) uses the exact archives from [passing CI run 39624213765](https://github.com/alanmarcero/openDucker/actions/runs/39624213765), source a0e9869ddd6bb3a641f080057878bcc1e077cf93. Both native jobs passed their CTest suites and pluginval level 5. The macOS archive contains arm64 and x86_64 VST3/AU; Windows contains x64 VST3. Archives include project/dependency notices and SOURCE.txt.

The real OpenVST Manager service downloaded both public archives without authentication, verified GitHub-provided SHA-256 digests, installed all three bundles, detected managed v1.0.0 receipts and removed them. macOS signatures and architecture slices were checked after extraction. Windows archive selection and installation were tested on macOS with Windows platform selection; native Windows execution passed in CI. The existing manager UI passed nine tests with the catalog addition.

Checksums:

- macOS: `10833ed12166efbaee2d34df67283034499ddcef260758fda248c3a95b617f0c`
- Windows: `16eeff48c70b9ad4ba097d0ac2687ff506262a91a718d026b31eff5c0181fc4c`

The [manager catalog update](https://github.com/alanmarcero/alan-m-OpenVSTManager/commit/38362a1) is on main. The real GitHubClient fetched the live public catalog and verified both bundle names after publication.
