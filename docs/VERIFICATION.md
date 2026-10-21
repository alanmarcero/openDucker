# Verification

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
