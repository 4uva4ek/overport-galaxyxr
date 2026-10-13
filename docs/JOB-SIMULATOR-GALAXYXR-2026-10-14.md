# Job Simulator startup on Galaxy XR, 2026-10-14

Job Simulator 1.4.0.6515 (versionCode 49, Unity 2022.3.67f1), converted with the
0.1.1 Windows application, crashed about three seconds after launch on Samsung SM-I610.

## Diagnosis

Two tombstones from the user's launches (00:51:48 and 00:52:00) show the same native
crash: `SIGSEGV` in `memset` called from `libOVRPlugin.so` `ovrp_SetupLayer+88` via
`OculusDisplayProvider::CreateLayer`. This is the signature that Beat Saber 1.40.8 produced
before its EGL fix: `eglCreateContext` for the Android XR shared context fails with
`EGL_BAD_MATCH`, `xrCreateSession` returns `XR_ERROR_INITIALIZATION_FAILED`, OVRPlugin
continues with an invalid session, `xrCreateSwapchain` and `xrEnumerateSwapchainImages`
return `XR_ERROR_HANDLE_INVALID`, and the uninitialized image count drives `memset` off
the heap.

The converted APK's `libunity.so` was byte-identical to the original (SHA-256
`74d5de038a50c46a02214530326fe533b8a79dc5f985e97babd490fe0ff1a634`): the 0.1.x EGL patch
only recognized the Unity 2022.3.33f1 digest and reported `NOT APPLIED` for this binary.

## Change

`UnityGlesCompatibility` now searches the executable `PT_LOAD` segments of an ARM64
`libunity.so` for the instruction sequence that appends
`EGL_CONTEXT_OPENGL_NO_ERROR_KHR` (0x31b3) with value 1:

```
mov w1, #0x31b3
mov w2, #1        ; changed to mov w2, #0
bl  <append attribute>
```

The patch applies only when exactly one site exists; it still changes a single byte
(`22` to `02`). Zero or several matches leave the library untouched with `NOT APPLIED`.
An already patched site (`mov w2, #0`) is reported as already applied.

| Binary | Unity | Site (file offset) | Output SHA-256 |
| --- | --- | --- | --- |
| Beat Saber 1.40.8 | 2022.3.33f1 | `0x7f2a78` | `7d468de5616967bd87a38a03e685641ade94c0d1f9abe4c8e6a642e5dadc9c05` (unchanged from 0.1.0) |
| Job Simulator 1.4.0.6515 | 2022.3.67f1 | `0xa47fc4` | `51837ef32fbff79bb0316eacaed846609ff21ec82a3e056e5eed829f4373b59e` |

`tools/CheckUnityGles.java` verifies, per supplied original library: one unpatched site,
a single changed byte, idempotence, no mutation for a broken or duplicated signature,
and rejection of truncated or non-ELF input.

## Validation

Headset test, 2026-10-14 01:33 (SM-I610, firmware I610UEU2AZF3): the APK reconverted with the
signature-based patch was installed over the previous conversion with the same signing key
and launched cold. The log shows `OverportGXR` policy and BOOST requests accepted, supported
refresh rates 60/72/90 Hz with 90 Hz accepted, `CompositorOpenXR_GLES::Layer::Initialize():
CreateSwapchain for eye 0: ..., 4 stages` for each layer, no `EGL_BAD_MATCH`, no
`XR_ERROR_INITIALIZATION_FAILED`, and Unity reaching `Finished Setup` and the first-time
introduction. No new tombstone or dropbox crash entry was recorded. The user confirmed that the
game works. Evidence: `diagnostics/overport-jobsimulator-20261014/` in the workspace
(tombstones, launch logs, APK comparison, check output); not distributed with the repository.

## Scope

Only the converted non-system APK changes. No firmware, system runtime, root module or
persistent device setting is involved. Reinstalling the previously saved APK with the same
signing key is the rollback; a reboot does not undo an APK update.
