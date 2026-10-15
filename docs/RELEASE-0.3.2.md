# Overport Galaxy XR 0.3.2

Desktop support now includes macOS and Linux alongside Windows and Android.

- Windows x64: extract the ZIP and run `OverportGalaxyXR.exe`.
- Linux x64: extract the tar.gz and run `OverportGalaxyXR/bin/OverportGalaxyXR` in a graphical desktop. Built on Ubuntu 22.04.
- macOS: choose `macos-arm64.dmg` for Apple Silicon or `macos-x64.dmg` for Intel, then copy the app to Applications. These packages are not Developer ID signed or notarized.
- Android: install the signed APK.

All desktop packages include Java. The optional CLI JAR requires Java 21.
Updates on macOS and Linux download and verify the matching package; quit the app and install it manually. Windows and Android retain their existing update flows.

The Galaxy XR conversion preset and native adapter are unchanged from 0.3.1. Existing converted APKs do not need to be reconverted. Keep your existing workspace and signing key.

`SHA256SUMS.txt` contains checksums for every package.
