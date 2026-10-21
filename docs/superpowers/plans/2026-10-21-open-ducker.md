# openDucker implementation plan

**Goal:** Publish a tested host-synced ducking plugin and make it installable through OpenVST Manager.

**Architecture:** A dependency-free sample clock/envelope engine, a JUCE processor adapter, and a native editor. Reuse the family's CI/release conventions.

**Tech stack:** C++20, JUCE 8.0.9, CMake, CTest, GitHub Actions.

**Spec:** [Design](../specs/2026-10-21-open-ducker-design.md).

## Global constraints

Zero latency. No allocation or locks in the DSP. Gain in [0,1]. Absolute host phase. Original artwork. VST3 macOS universal/Windows x64 and AU macOS. Published assets must be tested before catalog integration.

## Review focus

Missing host timing must never imply sync. Loops crossing buffers must preserve phase. Smoothing must not shift every beat. Audio precision must match the host. Packaging must work through the real manager installer.

## Task 1: DSP core

- [ ] Write Tests/CoreTests.cpp against Transport, Parameters, Engine::prepare, Engine::beginBlock and Engine::nextGain in Source/Core/Ducker.h. Assert beat minima at samples 0/24000/48000 for 120 BPM/48 kHz; compare 1/64/511/1024 partitions; check negative PPQ, loops, seeks, long timelines, rate/shape changes, missing timing, output bounds and stereo-safe scalar gain.
- [ ] Compile the tests and observe the missing implementation failure.
- [ ] Implement double-precision phase, continuous four-slope envelopes, parameter smoothing and transport transitions in Source/Core/Ducker.cpp.
- [ ] Run CTest in release and with address/undefined sanitizers; commit the core.

## Task 2: Native plugin and artwork

- [ ] Generate a transparent DJ Duck mascot and promotional artwork; save exact prompts in assets/ARTWORK.md.
- [ ] Create Tools/PluginTests.cpp for actual processor transport, session recall, controls and editor captures. Use literal host positions and expected audio gain, not core helpers for expected results.
- [ ] Add JUCE adapter in Source/Plugin/PluginProcessor.h/.cpp with amount, slope, rate and bypass APVTS parameters. Implement float/double processing and state persistence.
- [ ] Add native editor in Source/Plugin/PluginEditor.h/.cpp. A shared envelope function draws the preview; UI snapshots read atomic telemetry only.
- [ ] Build VST3/AU; run native integration tests, inspect screenshots and validate using pluginval/AU tools.

## Task 3: Public release and manager

- [ ] Add README, LICENSE, THIRD_PARTY.md and build/release workflows. Build CI uses pinned JUCE, sanitizers, native tests, pluginval, macOS universal and Windows x64.
- [ ] Review all code with an independent reviewer, fix confirmed findings, rerun relevant checks.
- [ ] Create public alanmarcero/openDucker, push main, run CI and publish v1.0.0 ZIP assets/checksums after successful checks.
- [ ] Add exact VST3/AU bundle names to the manager catalog; run manager tests/build and install/remove the released archive in temporary folders.
- [ ] Push manager catalog update, verify remote contents and report release/source links plus verification limits.
