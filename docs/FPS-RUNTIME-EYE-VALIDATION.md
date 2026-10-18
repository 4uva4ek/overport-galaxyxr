# FPS HUD and runtime-eye integration validation

Validated against runtime-eye base `33367c0` and FPS PR head `dbc0739`.

Quality arguments retain indices 6/7/8; FPS HUD uses index 9 (off by default).
The UI retains gaze mode and quality controls alongside the independent FPS switch.
One xrEndFrame backend call submits the HUD; gaze mapping uses original projections.
Both gaze sampling and FPS latching run after successful xrWaitFrame.
HUD extension fallback retains Android eye tracking and META foveation extensions.

Passed locally:
- FPS metrics, HUD lifecycle, runtime-eye extension integration, combined QCOM/runtime-eye/HUD test, runtime-eye guards, eye profiles and haptics.
- Combined test: visible HUD and moving gaze, all focal slots, invalid gaze full-quality fallback, frame counting without the quad, gaze continuing after HUD rejection.
- ASan/UBSan on the combined test.
- NDK r29 ARM64 API 29 build with warnings as errors; LOAD alignment 16 KiB.
- JDK 21 CLI shadowJar and Compose compileKotlinDesktop; built-JAR CheckFpsHud including explicit quality and mixed flags.
- Packaged adapter byte equality: SHA256 7b4a37334c4343229cb9406b0bee9ed372a0d55092a2a3832c4f572d9c046261.

No headset installation or launch was performed for this integration. CPU Surface
support, visual coexistence and overhead require a worn-device check.
The original HUD limitation remains: a blocked vendor Surface call can delay joined
teardown, and a rejected layer returns the backend frame error without retry.
