# Overport Galaxy XR

A Samsung Galaxy XR fork of [ovrport/app](https://github.com/ovrport/app), maintained by **4uva4ek**.

Graphical applications for **Windows and Android**, with an optional Java CLI.
Download both from [Releases](https://github.com/4uva4ek/overport-galaxyxr/releases).
On Windows, extract the ZIP and run `OverportGalaxyXR.exe` (Java is included).
On Android, install the APK and open **Overport Galaxy XR**.
Select a game APK, adjust resolution/performance if needed, and save the converted APK.

The Galaxy XR preset applies to every APK you convert: **3552×3840 per eye**,
**CPU/GPU BOOST** and **90 Hz when supported**, Android XR metadata, a signature-guarded Unity
EGL compatibility fix, and a fix for Overport's 4096-pixel swapchain abort.
Games still control their render scale; runtime hints do not guarantee 90 FPS.

```sh
java -jar overport-galaxyxr-0.1.2-cli.jar patch --input=game.apk --target=galaxyxr --output=patched
```

The optional CLI requires Java 21. The target uses the inspected ARM64 runtime 3.4.3-aa54c3f.
Use `--texture-size=2320x2700` to reduce rendering load, or `--texture-size=native`
to restore the runtime's recommendation. `--performance=app` restores application
control of CPU/GPU hints and refresh rate. Explicit `--patches` overrides the preset.
Both graphical applications include these settings; additional patches are under Advanced.

[Russian usage guide](README-GALAXYXR.md) | [Validation](docs/BEAT-SABER-GALAXYXR-2026-10-13.md) |
[Native adapter/build](native/galaxyxr/README.md) | [Upstream README](README-UPSTREAM.md)

All modifications are confined to the converted application. Keep your original APK
and signing workspace for rollback. No firmware, system runtime, root modules or
persistent device settings are changed. Reboot does not undo an APK update.
Game files and closed third-party runtime binaries are not distributed here.
Compatibility beyond tested applications is not guaranteed.

## Build

Build the native adapter first using Android NDK r29 (Linux or WSL), then the CLI:

```sh
ANDROID_NDK_HOME=/path/to/android-ndk-r29 bash native/galaxyxr/build.sh
./gradlew :overportcli:shadowJar
./gradlew :composeApp:createReleaseDistributable # Windows host, portable app with Java
./gradlew :composeApp:assembleRelease # Android SDK 36; signing setup below
```

[Release workflow and Android signing](docs/RELEASING.md): one tag builds and publishes
both applications. Manual workflow runs produce artifacts without publishing.

## Credits and license

Original Overport by [ovrport](https://github.com/ovrport/app), GPLv3; see [LICENSE](LICENSE).
Powered by Compose Multiplatform and ARSCLib. Manifest-only patch support follows
an approach from [OVRPort-PFDMR](https://github.com/urscaviezel/OVRPort-PFDMR).
Khronos OpenXR headers retain their attribution and MIT license under `native/galaxyxr/include`.
