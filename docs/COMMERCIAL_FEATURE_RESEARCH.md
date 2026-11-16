# Commercial feature research

## Reference: Cableguys ShaperBox 3

The vendor manual describes a sidechain section with high-pass and low-pass filtering, plus a solo option for listening to the filtered sidechain. Those controls help tune which source frequencies drive the effect.

Primary vendor source: [ShaperBox 3 Manual (PDF)](https://downloads.cableguys.com/Cableguys-ShaperBox-3-Manual.pdf?v=3.5.5) (accessed 2026-11-16).

## Added to openDucker 2.4.0

1. **Detector high-pass:** an automatable HPF removes low-frequency energy before audio trigger detection.
2. **Detector low-pass:** an automatable LPF removes high-frequency energy before audio trigger detection.
3. **Filtered detector listen:** an automatable monitor switch routes the filtered sidechain to the output for audition; when no sidechain bus is connected, it auditions the main input used by the detector.

Both filter cutoffs default to 0 Hz, which bypasses the corresponding stage and preserves the existing detector response. Each filter is a first-order, allocation-free stage. A cutoff of 0 Hz means off.

## Scope and limits

The filter stages are intentionally light and are for selecting detector content, not for shaping the audible program signal. Listen replaces the normal processed output while enabled. The feature does not add filter resonance, multiple filter slopes, or separate left/right cutoff controls.
