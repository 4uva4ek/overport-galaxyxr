# Overport Galaxy XR 0.2.0

Hand tracking for converted games and a choice of OpenXR dispatcher.

- **Hand tracking.** Games that declare Quest hand tracking in their manifest now ask for the
  Android XR `HAND_TRACKING` permission before the first start (new switch in the GUI,
  `--hands=auto|on|off` in the CLI, default auto). The Galaxy XR runtime returns an incomplete
  `XR_FB_hand_tracking_mesh` skeleton (parents, radii, finger tip and metacarpal bind poses),
  which made OVRPlugin draw twisted hands; the adapter now rebuilds that skeleton from the valid
  joints and the live hand and expresses it in the root frame OVRPlugin 1.88 expects
  (`--hand-mesh=fix`, default; `runtime` keeps the runtime description). Live joints are not
  modified; the runtime still does the tracking.
- **Open OpenXR dispatcher.** Converted APKs carry
  [openxr-dispatcher-open](https://github.com/Molier/openxr-dispatcher-open) 1.0.0 (MIT, built
  from the pinned upstream commit `68479ea`) as `libopenxr_loader.so` instead of the closed
  Overport dispatcher. New switch **Open OpenXR dispatcher** in the GUI (on by default) and
  `--dispatcher=open|overport` in the CLI; `overport` keeps the closed dispatcher together with
  the hash-guarded swapchain abort fix, which the open dispatcher does not need. Dispatcher logs:
  `adb logcat -s OpenXRDispatch`.
- Patches are applied in a fixed order independent of the order they were requested in, so the
  GUI and the CLI produce the same APK.

Reconvert games from their original APK to get hands and the open dispatcher; updating the
application does not change an APK converted by 0.1.x. Resolution, performance defaults and the
EGL fix are unchanged. 0.1.x users can install this version from **Updates**.

Known issue: the permission request activity runs in full space and starts the game right after
the dialog; on SM-I610 (I610UEU2AZF3) this once crashed the system UI, which recovered with a
system restart. The granted permission persists, so later launches skip the dialog. Converting
with the hand tracking switch off avoids the dialog; the permission can then be granted in the
system settings.

Validation: Job Simulator 1.4.0.6515 on Samsung SM-I610 with the open dispatcher (GLES session,
CPU/GPU BOOST and 90 Hz accepted, both hand skeletons rebuilt, Unity reaches `Finished Setup`,
no crash) and with the rebuilt hand skeleton (OVRPlugin skeleton verified against the Meta
layout; the user reports usable hands with some remaining deformation). Host tests for the
adapter policy and the hand skeleton replay captured runtime data; Windows/Android compilation
and updater tests pass. Details: `docs/OPEN-DISPATCHER-GALAXYXR-2026-10-14.md` and the
changelog. The CLI-only `--foveation=eye` experiment is unchanged and not validated. Other games
remain untested.
