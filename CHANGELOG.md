# Changelog

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
