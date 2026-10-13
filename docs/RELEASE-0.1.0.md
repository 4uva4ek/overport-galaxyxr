First Galaxy XR release with a graphical interface for Windows and Android.

- **Windows:** extract the `windows-x64.zip` and run `OverportGalaxyXR.exe`. Java is bundled.
- **Android:** install the signed `android.apk` and open Overport Galaxy XR.
- Select a Quest game APK, choose resolution/performance, then save the converted APK.

Both applications use the same Galaxy XR preset: 3552x3840 per eye, CPU/GPU BOOST,
supported 90 Hz, Android XR metadata, guarded Unity EGL and swapchain fixes.
Lower resolution and app-controlled performance are available in the main screen;
optional patches are under Advanced. The original Quest controller layout is preserved.

Validated runtime behavior on Beat Saber 1.40.8 / Samsung SM-I610: launch, gameplay,
larger textures, accepted BOOST/90 Hz requests, and pause with the separate left Menu
button. Other games remain untested. Hints do not guarantee 90 FPS. The preset uses
the inspected ARM64 runtime 3.4.3-aa54c3f; unknown Unity binaries are skipped, unknown
Overport loaders are rejected by the swapchain guard.

Changes affect only the converted APK. Keep the original APK and patcher's signing
workspace for reinstall rollback. No root, firmware or system runtime changes.
Do not clear Android application data if you need to retain its APK signing identity.
The Windows executable is not Authenticode-signed; Android releases use a stable
fork-specific signing key. No game APKs or closed runtime binaries are distributed.

The optional `cli.jar` requires Java 21:

```sh
java -jar overport-galaxyxr-0.1.0-cli.jar patch --input=game.apk --target=galaxyxr --output=patched
```

Original Overport by ovrport; Galaxy XR fork by 4uva4ek. GPLv3, third-party attribution retained.
