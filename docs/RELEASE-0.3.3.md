# Overport Galaxy XR 0.3.3

Fixes long vibration tails caused by ineffective haptic cancellation in the Galaxy XR runtime chain, observed when hovering over Arizona Sunshine menu items.

- The common adapter sends long scalar vibration effects in renewable 20 ms slices. Zero amplitude and Stop cancel renewal, while sustained effects retain their requested duration. This applies to all games converted with the default Galaxy XR preset.
- Galaxy XR is now the only patching target. The CLI uses it by default, rejects other targets, and keeps `--target=galaxyxr` compatible. Converted APKs omit the unused Huawei, Meta, Pico and YVR OpenXR loaders.
- Linux file selection and saving use a themed FlatLaf Swing chooser to avoid blank native GTK dialogs on affected Wayland desktops. Dialogs follow the application light/dark theme and are owned by the main window. The fix was confirmed on the affected Linux desktop.
- Windows x64, Linux x64, macOS ARM64/x64 and Android patcher packages remain supported. Desktop packages include Java; the optional CLI JAR requires Java 21.

Reconvert existing game APKs to receive the haptic fix. Keep your existing workspace and signing key so rebuilt games retain their signing identity. Updating the patcher alone does not update installed games.

Validation: the 20 ms diagnostic removed the physical vibration tail in Arizona Sunshine. Scheduler tests passed on the host and in an isolated ARM64 test on the headset, including ineffective Stop, sustained effects, cancellation, session teardown and independent hands. Sustained effects with the complete scheduler still need gameplay validation; compatibility with every game is not established.

macOS packages are not Developer ID signed or notarized. Linux packages target an Ubuntu 22.04-compatible graphical desktop. `SHA256SUMS.txt` contains checksums for all packages.
