# Changelog

## Galaxy XR 0.1.2 — 2026-10-14

- Unity EGL startup fix now locates the no-error context request by its ARM64 instruction
  signature (`mov w1, #0x31b3; mov w2, #1; bl`) instead of a whole-file digest, so every
  Unity build with exactly one such site is patched. Binaries with no match or several
  matches stay untouched and are reported as `NOT APPLIED`.
- Signature checks pass on Unity 2022.3.33f1 (Beat Saber 1.40.8, output identical to 0.1.0)
  and 2022.3.67f1 (Job Simulator 1.4.0.6515, which crashed in `ovrp_SetupLayer` after
  `EGL_BAD_MATCH` without the fix). Headset results: `docs/JOB-SIMULATOR-GALAXYXR-2026-10-14.md`.

## Galaxy XR 0.1.0 — 2026-10-13

- Adapted Windows and Android GUI: Galaxy XR defaults, resolution selector, performance toggle and advanced patches.
- Windows portable package with Java included; Android fork identity and stable release signing.
- Unified GitHub Actions release pipeline builds both applications before publishing.
- Galaxy XR CLI preset with Android XR manifest metadata.
- Default 3552x3840 per-eye recommendation, CPU/GPU BOOST and supported 90 Hz.
- Guarded Unity EGL startup fix and Overport swapchain-size abort fix.
- Application-local OpenXR adapter with optional app-controlled performance.
- Original Quest controller layout; no game-specific button remapping.
- Quiet release build, source/build documentation and focused compatibility checks.

Validated on Beat Saber 1.40.8 and Samsung SM-I610. Other games are untested.
Performance and refresh requests remain subject to runtime limits.
Java 21 required for the optional CLI. No game APKs or closed runtime binaries included.
