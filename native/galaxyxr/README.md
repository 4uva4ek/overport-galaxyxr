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
validates real dimension limits and returns OpenXR errors.

## Build and validation

Linux/WSL, Android NDK r29, Python 3, host C++17 compiler, Java 21:

```sh
python3 native/galaxyxr/generate_forwarders.py
export ANDROID_NDK_HOME=/path/to/android-ndk-r29
bash native/galaxyxr/build.sh
mkdir -p native/galaxyxr/build
g++ -std=c++17 -O2 -Wall -Wextra -Werror -Wno-missing-field-initializers \
  -Inative/galaxyxr/include native/galaxyxr/test_policy.cpp -ldl -pthread \
  -o native/galaxyxr/build/test_policy
native/galaxyxr/build/test_policy
native/galaxyxr/build/test_policy app
./gradlew :overportcli:shadowJar
```

The library uses 16 KiB segment alignment and static C++ support. Generated
libraries are build outputs. `GXR_INPUT_DIAGNOSTICS=1` enables local input
diagnostics; release builds omit input event logging and the OVRPlugin observer.

The Java checks in `tools/` take independently obtained original libraries to
verify exact changes, hash rejection and idempotence. No game or closed runtime
libraries are included in the repository.

OpenXR headers: copyright 2017–2024 The Khronos Group Inc., MIT license in
`include/LICENSE` (upstream also offers Apache-2.0). Source:
[KhronosGroup/OpenXR-SDK](https://github.com/KhronosGroup/OpenXR-SDK).
The adapter and fork changes follow the repository GPLv3 license.
