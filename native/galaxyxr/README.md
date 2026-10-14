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

## Build and validation

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
