# Overport Galaxy XR 0.1.2

The Unity EGL startup fix now applies to every ARM64 Unity build that contains the
no-error context request, not only to the Unity 2022.3.33f1 binary from Beat Saber.

- `libunity.so` is searched for the instruction sequence that appends
  `EGL_CONTEXT_OPENGL_NO_ERROR_KHR = 1`; the single value byte becomes 0.
  The patch applies only when exactly one such site exists. Zero or several matches
  leave the library untouched and the log says `NOT APPLIED`.
- Job Simulator 1.4.0.6515 (Unity 2022.3.67f1) previously crashed about three seconds
  after launch: `eglCreateContext` returned `EGL_BAD_MATCH`, `xrCreateSession` failed and
  OVRPlugin crashed in `ovrp_SetupLayer`. Reconverting it with this version applies the fix.
- Beat Saber 1.40.8 output is byte-identical to 0.1.0 and 0.1.1.

Reconvert affected games from their original APK; an APK converted by 0.1.1 is not
changed by updating the application. The Galaxy XR runtime adapter, resolution and
performance defaults are unchanged. 0.1.1 users can install this version from **Updates**.

Validation: focused checks on the two inspected Unity libraries (signature match,
single changed byte, idempotence, broken/duplicated signature and truncated input),
Windows/Android compilation, updater tests. Job Simulator 1.4.0.6515 reconverted with this
fix starts and runs on Samsung SM-I610 (user-confirmed); details in
`docs/JOB-SIMULATOR-GALAXYXR-2026-10-14.md`. Other games remain untested.
