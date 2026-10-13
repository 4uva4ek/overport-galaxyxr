# Overport Galaxy XR 0.1.1

Windows and Android now check GitHub Releases for updates when the app opens.
Use **Updates** to check again, download a newer stable release, and install it.
Downloads are checked against the release asset's size and SHA-256 before use.

- **Windows portable:** Update and restart replaces the app in its current directory.
  The previous app is kept in a sibling `.OverportGalaxyXR-backup-*` directory.
  A failed replacement or launch restores the previous app where possible.
  The installation directory must be writable; no administrator privileges are requested.
  Recovery details are in `%LOCALAPPDATA%\OverportGalaxyXR\updates\last-install.txt`.
- **Android:** the APK must match the installed app's package and signing certificate,
  and have a newer version code. Android asks for installation permission and confirmation.
  After allowing updates from Overport, return and press **Download and install** again.
- Settings, patched games, and the persistent APK signing identity are preserved.
- Updating is disabled while an APK is open for patching. Finish or cancel that job first.

Install this version once using the release APK or Windows ZIP to get the updater;
0.1.0 only links to the release page. Future versions can be installed within the app.

The validated Galaxy XR runtime adapter from 0.1.0 is unchanged. Experimental
eye-tracked foveation is not included in this release.

Validation: Windows/Android compilation, release selection and download integrity tests,
Windows archive path checks, and Windows replacement/rollback/incomplete-package tests.
Android installation still requires the system installer; there is no silent installation.
