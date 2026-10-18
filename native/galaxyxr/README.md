# Galaxy XR application-local OpenXR policy

The CLI packages this adapter, built from source. APK conversion preserves the
original generic loader as `libopenxr_loader_gxrbase.so` and places this adapter
at its old filename. It loads that APK-local original by a sibling path and
forwards core exports, initialization and GIPA dispatch. It never writes
settings, changes services or modifies the system runtime.

With `performance=max` in the generated APK-local `liboverport.galaxyxr.config.so`,
instance creation adds supported performance/refresh extensions. Successful
session begin requests CPU/GPU BOOST and enumerates refresh rates, requesting
90 Hz only if listed. Subsequent app requests keep these preferences. Unsupported
extensions or refused hints cannot fail an otherwise successful session begin.
`--performance=app` preserves application control. No button remapping is performed.

The separate hash-guarded Kotlin patch changes two branches leading to `abort()`
in Overport 3.4.3-aa54c3f ARM64 `xrCreateSwapchain`. The native runtime still
validates real dimension limits and returns OpenXR errors. It applies only when
the closed dispatcher is kept (`--dispatcher=overport`).

## Open dispatcher

By default the converted APK carries [openxr-dispatcher-open](https://github.com/Molier/openxr-dispatcher-open)
(MIT) as `libopenxr_loader.so` instead of the closed Overport dispatcher.
`build_dispatcher.sh` clones the pinned upstream commit (v1.0.0,
`68479ea28e69202577cba51fbcab9baeb827a7fc`), builds it with the upstream
`CMakeLists.txt` (NDK r29, CMake and Ninja from the Android SDK), checks the
SONAME, 16 KiB segment alignment and the export list, and stores the result as
`overportcli/src/main/resources/galaxyxr/arm64-v8a/libopenxr_loader.bin`; the
licence notices `LICENSE-openxr-dispatcher-open.txt` and `LICENSE-cJSON.txt`
next to it are compared with the upstream files.

```sh
export ANDROID_NDK_HOME=/path/to/android-ndk-r29
export CMAKE_DIR=/path/to/android-sdk/cmake/3.22.1/bin   # optional, else PATH
bash native/galaxyxr/build_dispatcher.sh
```

How it meets this adapter on Galaxy XR: for `ro.product.brand` samsung the
dispatcher loads `libopenxr_loader_generic.so` by name, which is this adapter,
and resolves every function, including `xrInitializeLoaderKHR` and
`xrCreateInstance`, through the adapter's `xrGetInstanceProcAddr`; the adapter
returns its own hooks there, so the performance, hand tracking and eye hooks
are reached as before. The dispatcher reads `liboverport.config.so`
(`suggested_texture_*`, `suggested_cpu_level/gpu_level`, `verbose_logging`),
answers foveation profiles, `xrUpdateSwapchainFB` and
`xrGetRecommendedLayerResolutionMETA` (1504x1504) locally like the closed one,
clamps `XR_FB_hand_tracking_mesh` parents above 25 to 0 (the rebuilt skeleton
already lists the palm as the wrist parent), and passes oversized swapchains to
the runtime. Logs: `adb logcat -s OpenXRDispatch`.

## Haptic cancellation

The adapter hooks scalar vibration and Stop through GIPA for every converted
application. Positive durations above 20 ms are sent in 20 ms slices, renewed
roughly every 10 ms until the requested deadline. Zero amplitude and Stop cancel
renewals; session/action teardown cancels outstanding work. Explicit subaction
paths keep the hands independent. Short effects, other haptic types and extension
chains are forwarded directly.

In Arizona Sunshine, captured menu commands requested two seconds followed by
zero amplitude about 33 ms later. Explicit Stop alone still left the physical
tail; a temporary 20 ms pulse test removed it. The exact downstream component
ignoring cancellation has not been isolated. The scheduler is covered by host
and ARM64 mock-backend tests, including a backend whose Stop does nothing; the
complete scheduler still needs a sustained-effects gameplay check.

## Build and validation

### FPS diagnostics

The HUD can be combined with `--foveation=runtime-eye` and the density/gain/area
controls. Performance patch arguments retain quality at indices 6/7/8; the
optional HUD argument is index 9 (off by default). The single `xrEndFrame`
wrapper submits once, then records eye mapping from the original application
layers. `xrWaitFrame` updates both the frame latch and the gaze sample.
`test_qcom_foveation` exercises the combined public hooks with a mocked Surface:
visible HUD, moving gaze, all focal slots, invalid-gaze full quality, one backend
submission, FPS excluding the quad, and continued gaze after HUD rejection.
`test_fps_integration` checks that HUD extension fallback retains Android gaze
and META foveation extensions. These host checks do not establish device Surface
support, visual quality, or performance overhead.

The optional `fps_hud=on` adapter configuration is emitted by `--fps-hud=on`
and the graphical FPS-overlay switch. It is read at process start and defaults
to off. The adapter negotiates Android surface swapchains only for this option;
failed optional extension negotiation retries without the HUD while preserving
the existing application/performance/eye-foveation extension set.

The HUD appends one VIEW-space quad only to visible/renderable app projection
frames and only within `maxLayerCount`. A producer worker paints its Android
Surface at most twice per second without accessing the game's graphics context
or queues. The worker copies immutable text snapshots; buffer locking and CPU
painting run off the game's normal frame-submission path.
Surface swapchains use the KHR-required zero format/sample/face/array/mip fields,
and are never enumerated, acquired, waited, or released as ordinary swapchains.
The worker is stopped and fully joined before `xrEndSession`, nonvisible event
delivery, and resource destruction. It is never detached; a stalled vendor
buffer call can therefore delay teardown. Resources are destroyed before their
owning session/instance. Optional setup failure disables the HUD. A backend
`xrEndFrame` error is returned unchanged, without retrying the frame, and
disables the HUD for subsequent frames. That error may still cause a game that
aborts on OpenXR errors to exit, so validate the opt-in HUD on the target runtime.

`APP FPS` counts only successful original projection submissions using a
monotonic clock and a one-second window. Zero-layer frames, failed submissions,
and the diagnostic quad do not add frames. Pipelined wait results are queued and
latched at the corresponding successful `xrBeginFrame`, including a begin that
discards a previous frame. Visibility transitions and pauses reset the sample.
`DISPLAY HZ` comes separately from `xrGetDisplayRefreshRateFB`; `--`
means unavailable/warming up. Neither metric proves unique GPU-completed images
or captures compositor reprojection. Sparse `OverportGXR` log messages report
initialization failures and each completed submission-rate sample.

Run the focused host tests with the same include/warning flags as below:

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Inative/galaxyxr/include native/galaxyxr/test_fps_metrics.cpp -o native/galaxyxr/build/test_fps_metrics
native/galaxyxr/build/test_fps_metrics
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Inative/galaxyxr/include native/galaxyxr/test_fps_hud.cpp -pthread -o native/galaxyxr/build/test_fps_hud
native/galaxyxr/build/test_fps_hud
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Inative/galaxyxr/include native/galaxyxr/test_fps_integration.cpp -ldl -pthread -o native/galaxyxr/build/test_fps_integration
native/galaxyxr/build/test_fps_integration
```

Linux/WSL, Android NDK r29, Python 3, host C++17 compiler, Java 21:

```sh
python3 native/galaxyxr/generate_forwarders.py
export ANDROID_NDK_HOME=/path/to/android-ndk-r29
bash native/galaxyxr/build_dispatcher.sh
bash native/galaxyxr/build.sh
mkdir -p native/galaxyxr/build
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Inative/galaxyxr/include native/galaxyxr/test_policy.cpp -ldl -pthread \
  -o native/galaxyxr/build/test_policy
native/galaxyxr/build/test_policy
native/galaxyxr/build/test_policy app
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Inative/galaxyxr/include native/galaxyxr/test_hand_tracking.cpp -ldl -pthread \
  -o native/galaxyxr/build/test_hand_tracking
native/galaxyxr/build/test_hand_tracking
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Inative/galaxyxr/include native/galaxyxr/test_haptics.cpp -ldl -pthread \
  -o native/galaxyxr/build/test_haptics
native/galaxyxr/build/test_haptics
./gradlew :overportcli:shadowJar
```

The library uses 16 KiB segment alignment and static C++ support. Generated
libraries are build outputs. `GXR_INPUT_DIAGNOSTICS=1` enables local input
diagnostics; release builds omit input event logging and the OVRPlugin observer.
`GXR_HAND_DIAGNOSTICS=1` logs hand tracker creation, the rebuilt
`XR_FB_hand_tracking_mesh` skeleton and sampled joint frames (`OverportGXR: HAND`);
release builds omit it. `hand_tracking.inc` always rebuilds the runtime hand
skeleton unless the generated configuration contains `handmesh=runtime`, and
re-rolls the live wrist (from the palm joint) and little-metacarpal frames to the
Meta conventions unless it contains `handframes=runtime`, and replaces the bind
joints with the hand-tuned table (`kEditedBind`) when it contains `handbind=edited`
(the CLI default; `handbind=rebuild` keeps the derived bind pose). The host test
replays the SM-I610 runtime data captured on 2026-10-14; `test_hand_tracking edited`
checks the table path for both hands.
The adapter is packaged as a `.bin` Java resource because Android's resource
packager filters `.so` files outside JNI directories. APK conversion writes the
same bytes under the required `.so` name; the patcher never loads this ARM64 library.

The Java checks in `tools/` take independently obtained original libraries to
verify exact changes, hash rejection and idempotence. No game or closed runtime
libraries are included in the repository.

OpenXR headers: copyright 2017–2024 The Khronos Group Inc., MIT license in
`include/LICENSE` (upstream also offers Apache-2.0). Source:
[KhronosGroup/OpenXR-SDK](https://github.com/KhronosGroup/OpenXR-SDK).
The adapter and fork changes follow the repository GPLv3 license.
