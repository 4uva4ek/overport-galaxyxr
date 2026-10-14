# Eye foveation experiment

**Current status: stopped; clean 0.1.0 APK restored after reported headset restarts.**
Experimental eye mode is not validated for release. No further deployment should
occur without investigating the system failure and renewed user direction.

User requested an app-local test on Galaxy XR. `--target=galaxyxr --foveation=eye`
converts supported nonzero application FB foveation profiles to META eye-tracked
profiles, preserving level/dynamic mode. Default remains `app`. This does not
force foveation on applications that never create foveation profiles/swapchains.

The adapter negotiates all four required extensions, checks system support,
preserves caller chains, falls back to the original profile on creation failure,
and refreshes active eye profiles after each successful frame wait. It retains
app-released eye profiles internally until swapchains stop referencing them.
No gaze coordinates are logged. A refused frame update stops additional updates;
the runtime retains the last applied profile, so this is not a guaranteed
full-resolution fallback when tracking becomes invalid.

Primary references:
- https://developer.android.com/develop/xr/openxr/extensions
- https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_META_foveation_eye_tracked.html
- https://registry.khronos.org/OpenXR/specs/1.1/man/html/XR_FB_foveation.html

Native profile lifecycle/fallback and existing performance-policy tests pass.
ARM64 build and CLI packaging pass. Beat Saber 1.40.8 test APK was installed with
`adb install -r` successfully; signer matches the previous tested app.

- Adapter SHA-256: `ae1948af372873a40f8568c99ff45bab71b6c54c12a4bfb0ad1987fdefef4821`
- APK SHA-256: `1041b13e30011b267358318e0d1357dd2a646883e825a84dad9de87399d1ce9c`
- Evidence: `diagnostics/overport-beatsaber-20261013/eye-foveation/`
- Rollback: reinstall saved `release-0.1.0/beatsaber.output.apk` from the same evidence root.

First live launch confirms required extensions and system support, BOOST and
90 Hz requests succeed. Fine/coarse eye permissions were still denied at the
last check. App settings opened for the user to grant permission. Active profile,
valid gaze, visual quality and performance are not yet confirmed.

Scope is only the application APK. No system files, firmware, module state or
startup scripts changed. Reboot does not undo an APK update; reinstall is rollback.

## Permission launch fix

User reports the game works, but no permission dialog appears automatically.
Fine/coarse permission now confirmed granted manually. Live log still has no
converted eye profile or valid gaze event; eye foveation remains unconfirmed.

The eye option now injects a small permission activity for each launcher entry
(including aliases). It moves only launcher intent filters, retains the original
activity and XR properties, asks for EYE_TRACKING_FINE before launching the game,
and continues after denial or a request exception. Already granted permission
skips the dialog. Unknown Android permission suppression rules remain OS-owned;
permanently denied permissions may require app settings. Direct explicit starts
of the original activity bypass the launcher gate.

CLI build, smali assembly, APK signature and launchable-activity checks passed.
Replacement installed successfully. At 19:29:20 PID26502 logged "Eye permission
gate complete; starting original activity" with the existing grant preserved.
The permission-denied dialog path has not yet been tested live; no permissions
were revoked or reset to manufacture a test. Evidence: sibling `eye-permission/`.

## Direct stereo GLES test

User enabled the headset developer dashboard and reports Foveation level: None,
alongside working BOOST/90 FPS. Fine eye permission is granted. No converted
application profile reached the adapter in retained logs. The precise upstream
reason (no requested profile versus interception by Overport) is not established.

Explicit eye mode now also prepares a foveation-capable stereo GLES color array
and directly applies a MEDIUM eye profile. Eligibility: arraySize=2, faceCount=1,
mipCount=1, width/height>=512, color attachment, not static. Depth, mono targets,
Vulkan and other targets retain the earlier application-profile path. The direct
profile takes precedence over app foveation updates for that swapchain. This
supersedes the initial statement that eye mode never forces application profiles.
Refused swapchain augmentation retries original creation; refused profile or
application leaves rendering running without a forced eye profile.

Focused lifecycle tests now cover direct activation, app-off override, depth
exclusion, creation refusal and profile permission refusal. Tests, ARM64 and CLI
builds, APK signature and diff checks pass. Replacement installed and launched.
- Adapter SHA256: `93c4b0750f229580ead91a4eeb603917c17a75053103e98095e1ed67e5f25d72`
- APK SHA256: `8146f8700afc372997466ce6b096805da918684cf28ca7ca1a0d6f470b3669ef`
- Evidence: sibling `eye-direct/`.
Initial PID21791 has created instances; direct profile acceptance and valid gaze
are pending a worn-headset session. This is not a validated performance gain.

Later live result (19:41:06+, PID21791): direct profile creation and application
both XR_SUCCESS for native and menu textures; per-frame updates succeed. However
eye-state query returns XR_ERROR_RUNTIME_FAILURE (-2), and the runtime reports
`Using static model with level 2`. Initial runtime diagnostics also report a
negative lens acuity/root extraction failure. No system/calibration changes were
made. Accepted level is confirmed, gaze-driven movement is not. Saved full relevant
messages in `eye-direct/runtime-findings.txt`; investigation remains open.

## Reboot report and rollback

User could not test because the headset restarted, and suspects the developer
dashboard. Device reports `reboot,rescueparty_warm_reboot`. Crash buffer contains
SystemUI aborts at 19:34:33 and 19:42:02 with `sp<> assignment detected data race`,
inside libhwui PropertyValuesAnimatorSet / AnimatedVectorDrawable / ProgressBar.
XR home also segfaulted at 19:42:03. The identical SystemUI signature predates the
direct-eye build installed at 19:40. This does not establish dashboard or
foveation causality; no rendering experiment was repeated to reproduce it.

Force-stopped the game and restored the original clean release-0.1.0 APK.
Installation succeeded; on-device SHA256 equals
`d3f6c1b2bd1bd92a313d31961b1bdafeacdf42d42bb543ee8f55f5f372419a0a`.
This removes both experimental foveation and the eye permission gate. Permissions
and device settings were left untouched. No system writes or reboot issued.
Evidence: `eye-direct/crash-after-reboot.txt`, `eye-direct/system-restarts.txt`.
