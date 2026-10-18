<img src="composeApp/icons/app-icon.png" width="96" alt="">

# Overport Galaxy XR

A Samsung Galaxy XR fork of [ovrport/app](https://github.com/ovrport/app), maintained by **4uva4ek**.

Graphical applications for **Windows, macOS, Linux and Android**, with an optional Java CLI.
Download packages from [Releases](https://github.com/4uva4ek/overport-galaxyxr/releases).
On Windows, extract the ZIP and run `OverportGalaxyXR.exe` (Java is included).
On macOS, choose the DMG for Apple Silicon (`macos-arm64`) or Intel (`macos-x64`),
open it and copy the app to Applications. These builds are not Developer ID signed or notarized.
On Linux x64, extract the `.tar.gz` and run `OverportGalaxyXR/bin/OverportGalaxyXR`.
Desktop packages include Java; Linux requires a graphical desktop and its system libraries.
On Android, install the APK and open **Overport Galaxy XR**.
Select a game APK, adjust resolution/performance if needed, and save the converted APK.

Galaxy XR is the only supported headset target. The preset applies to every APK you convert: **3552×3840 per eye**,
**CPU/GPU BOOST** and **90 Hz when supported**, Android XR metadata, a signature-guarded Unity
EGL compatibility fix, and a fix for Overport's 4096-pixel swapchain abort.
Games still control their render scale; runtime hints do not guarantee 90 FPS.

```sh
java -jar overport-galaxyxr-0.1.2-cli.jar patch --input=game.apk --output=patched
```

Haptics uses cancellable 20 ms pulses for all converted games, preserving sustained effects.
Already converted games need conversion again to receive adapter updates.

The optional CLI requires Java 21. The target uses the inspected ARM64 runtime 3.4.3-aa54c3f.
Use `--texture-size=2320x2700` to reduce rendering load, or `--texture-size=native`
to restore the runtime's recommendation. `--performance=app` restores application
control of CPU/GPU hints and refresh rate. Explicit `--patches` overrides the preset.
All graphical applications include these settings; additional patches are under Advanced.

### Optional FPS overlay

Enable **FPS overlay** in the Galaxy XR conversion settings, or add
`--fps-hud=on` to the CLI command. It defaults to off. Reconvert and restart the
game to change this option; existing converted APKs are unchanged.

The small view-locked panel shows **APP FPS** (successful app projection frame
submissions per second) separately from the runtime's current **Hz**. It does
not count compositor refreshes as game frames, and it is not a GPU-completion
or unique-image counter. Repeatedly submitting the same image can still count
as submissions. Use demanding gameplay scenes when comparing resolutions.

The overlay uses `XR_KHR_android_surface_swapchain` with a CPU-painted Android
Surface, so it does not alter the game's GLES/Vulkan state. Unsupported surface
creation, missing Android VM information, or a full composition-layer budget
leave the original game layers unchanged. No root or overlay permission is
needed. Host checks cover frame counting and lifecycle; device validation is
still required before relying on the overlay for performance tuning.

[Russian usage guide](README-GALAXYXR.md) | [Validation](docs/BEAT-SABER-GALAXYXR-2026-10-13.md) |
[Native adapter/build](native/galaxyxr/README.md) | [Upstream README](README-UPSTREAM.md)

All modifications are confined to the converted application. Keep your original APK
and signing workspace for rollback. No firmware, system runtime, root modules or
persistent device settings are changed. Reboot does not undo an APK update.
Game files and closed third-party runtime binaries are not distributed here.
Compatibility beyond tested applications is not guaranteed.

## Build

Use JDK 21 via `JAVA_HOME`; desktop packages must be built on their target OS/architecture.

Build the native adapter first using Android NDK r29 (Linux or WSL), then the CLI:

```sh
ANDROID_NDK_HOME=/path/to/android-ndk-r29 bash native/galaxyxr/build.sh
./gradlew :overportcli:shadowJar
./gradlew :composeApp:createReleaseDistributable # Current desktop OS, portable app with Java
./gradlew :composeApp:packageReleaseDmg # macOS host; DMG installer
./gradlew :composeApp:packageReleaseDeb # Debian/Ubuntu host with dpkg/fakeroot; optional package
./gradlew :composeApp:assembleRelease # Android SDK 36; signing setup below
```

[Release workflow and Android signing](docs/RELEASING.md): one tag builds and publishes
all platforms. Manual workflow runs produce artifacts without publishing.

## Credits and license

Original Overport by [ovrport](https://github.com/ovrport/app), GPLv3; see [LICENSE](LICENSE).
Powered by Compose Multiplatform and ARSCLib. Manifest-only patch support follows
an approach from [OVRPort-PFDMR](https://github.com/urscaviezel/OVRPort-PFDMR).
Khronos OpenXR headers retain their attribution and MIT license under `native/galaxyxr/include`.
