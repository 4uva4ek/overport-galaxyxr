# Changelog

## Galaxy XR 0.2.0 — 2026-10-14

- Open OpenXR dispatcher: the Galaxy XR preset now packages
  [openxr-dispatcher-open](https://github.com/Molier/openxr-dispatcher-open) 1.0.0 (MIT,
  pinned commit `68479ea`, built from source by `native/galaxyxr/build_dispatcher.sh`) as
  `libopenxr_loader.so` instead of the closed Overport dispatcher (`patch_galaxyxr_dispatcher`,
  `--dispatcher=open`, GUI switch). The adapter chain, the preserved generic loader and the
  configuration files are unchanged; the open dispatcher passes oversized swapchains to the
  runtime, so the hash-guarded swapchain abort fix is applied only with `--dispatcher=overport`,
  which keeps the closed dispatcher. Licence notices ship as resources next to the binary.
  Validated with Job Simulator 1.4.0.6515 on SM-I610 (GLES session, BOOST and 90 Hz accepted,
  hand skeleton rebuilt, no crash); see `docs/OPEN-DISPATCHER-GALAXYXR-2026-10-14.md`.
- Patches now run in registration order regardless of the order of the requested map, so
  library copies always precede the patches that rewrite the copied libraries (the GUI map is
  not insertion ordered).
- Hand tracking permission: converted games that declare Quest hand tracking
  (`com.oculus.permission.HAND_TRACKING`, `oculus.permission.handtracking` or the
  `oculus.software.handtracking` feature) now request `android.permission.HAND_TRACKING`
  through the launcher bridge activity before the first start. New CLI option
  `--hands=auto|on|off` (default auto) and a GUI switch. The bridge activity requests the
  combined permission list, so the eye foveation experiment shares it.
- Evidence: Job Simulator 1.4.0.6515 on SM-I610 declared the Quest permissions but ran with
  `android.permission.HAND_TRACKING` denied; the runtime already advertised
  `XR_EXT_hand_tracking`, `XR_FB_hand_tracking_aim` and `XR_FB_hand_tracking_mesh`, and
  OVRPlugin reported hand tracking as supported.
- Hand skeleton rebuild (`--hand-mesh=fix`, default): the SM-I610 runtime returns an incomplete
  `XR_FB_hand_tracking_mesh` description (garbage `jointParents`, zero `jointRadii`, finger tip
  and metacarpal bind poses duplicated from other joints or identity) while its
  `XR_EXT_hand_tracking` joints follow the OpenXR convention. OVRPlugin builds the Quest skeleton
  from that description, so the adapter now rebuilds parents, radii and the missing bind joints
  in `xrGetHandMeshFB` from the valid joints and the latest live sample; live joints are untouched.
  OVRPlugin 1.88 converts the bind poses with a fixed per-hand quaternion and its bone math is only
  well defined when the wrist bind orientation equals that quaternion (as in the Meta runtime), so the
  rebuilt pose, vertices and normals are expressed in that root frame; the palm equals the wrist and the
  wrist lists the palm as parent, because the Overport dispatcher clamps parent values above 25 to 0.
  `--hand-mesh=runtime` keeps the runtime description. Host test
  `native/galaxyxr/test_hand_tracking.cpp` replays the captured runtime data.
- Known issue: the permission bridge activity in full space followed by the game launch crashed
  `com.android.systemui` once (libhwui animator race, SM-I610 I610UEU2AZF3), which the system
  recovers from with a zygote restart. The bridge is not yet moved to home space.

## Galaxy XR 0.1.2 — 2026-10-14

- Unity EGL startup fix now locates the no-error context request by its ARM64 instruction
  signature (`mov w1, #0x31b3; mov w2, #1; bl`) instead of a whole-file digest, so every
  Unity build with exactly one such site is patched. Binaries with no match or several
  matches stay untouched and are reported as `NOT APPLIED`.
- Signature checks pass on Unity 2022.3.33f1 (Beat Saber 1.40.8, output identical to 0.1.0)
  and 2022.3.67f1 (Job Simulator 1.4.0.6515, which crashed in `ovrp_SetupLayer` after
  `EGL_BAD_MATCH` without the fix). Job Simulator starts and runs on SM-I610 after reconversion;
  see `docs/JOB-SIMULATOR-GALAXYXR-2026-10-14.md`.

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
