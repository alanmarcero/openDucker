# openDucker 2.8.4

- **Buffer safety:** Guards `DuckerProcessor::process` against 0-channel and mismatched bus buffers before reading samples.

# openDucker 2.8.3

- **Editor polish:** Refines lock controls, curve display, preset browsing, and the shared suite layout.
- **Build and licensing:** Aligns JUCE 8 licensing and hardens the CI workflow.

# openDucker 2.8.0

- **Smooth Ballistics & Curves:** Eliminated C0 derivative corners in ducking curves and refined attack/recovery ballistics with click-free legato retriggering.
- **Phase-Coherent Crossover & Detection:** Upgraded to a flat-sum Linkwitz-Riley crossover without zipper noise, and added 24 dB/oct sidechain filtering with improved transient detection on dense bass material.
- **True Lookahead & FX Stabilization:** Added latency-reported 3 ms lookahead delay, frequency-dependent reverb damping, DC-guarded delay feedback, and antialiased Destroy bitcrushing.

![openDucker native editor](https://raw.githubusercontent.com/alanmarcero/openDucker/main/docs/screenshots/preview.png)
