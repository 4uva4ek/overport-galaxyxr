# Releasing Windows, macOS, Linux and Android together

Workflow: `.github/workflows/galaxyxr-release.yml` (Galaxy XR applications).
It builds the native adapter and the open OpenXR dispatcher once (NDK r29 and the SDK
CMake 3.22.1 from `sdkmanager`), then builds Windows, Android, Linux x64 and macOS (ARM64 and x64) in parallel.
All platform jobs must succeed before the release job can publish. No game inputs are needed.
`tools/check_packaged_adapter.py` verifies that all distributions carry the same adapter,
dispatcher and licence notices as the checked-out resources.

## One-time Android signing setup

Repository secrets:

- `GXR_KEYSTORE_BASE64`: base64 of the dedicated PKCS12 keystore.
- `GXR_KEYSTORE_PASSWORD`: keystore and key password.

Key alias: `overport-galaxyxr`. Keep a private offline backup of the key and password.
Never put them in source, build artifacts or release assets. Missing keys fail the
Android job; it never silently publishes an unsigned/debug-signed replacement.
The temporary runner keystore is removed even after failure.

## Version and validation

1. Update `galaxyxr.properties` version and increase Android `versionCode`.
2. Match `VersionManager.DISTRIBUTION_VERSION` and add `docs/RELEASE-VERSION.md`.
   Keep upstream `VersionManager.VERSION` unchanged unless backend compatibility changes.
3. Run `python tools/check_release_version.py` and the focused changed-behavior checks.
4. Push to `galaxyxr`, then dispatch **Galaxy XR applications** on that branch.
   Manual runs build all platforms and upload artifacts without publishing.
5. Download and check the GUI and conversion results. Retain the existing signing
   workspace when rebuilding an installed game.
6. Create and push `galaxyxr-vVERSION` at the validated commit. Tag runs build both
   applications and publish one release only when all succeed.

Release assets: signed Android APK, Windows x64 portable ZIP with Java 21 runtime,
Linux x64 portable tar.gz, macOS ARM64 and x64 DMGs,
optional CLI JAR, and `SHA256SUMS.txt`. Windows packages are not Authenticode signed.
macOS packages are not Developer ID signed or notarized. Updates on macOS and Linux
are downloaded and verified in-app, then installed manually. Linux archives preserve
executable permissions and runtime symlinks. Linux builds use Ubuntu 22.04; run them
in a compatible graphical environment. Each desktop package bundles its own Java runtime.

Apple package metadata uses release major + 1 (for example, 0.3.1 becomes 1.3.1),
because Apple requires a positive major. Release names and in-app versions stay unchanged.

For local builds, select JDK 21 with `JAVA_HOME`. Run
`:composeApp:createReleaseDistributable` on the target OS/architecture;
`:composeApp:packageReleaseDmg` creates macOS images and
`:composeApp:packageReleaseDeb` optionally creates Debian installers on a
Debian/Ubuntu host with `dpkg` and `fakeroot`. The release workflow ships the portable
Linux tar.gz; DEB creation is not available on every Linux distribution.

GitHub source archives provide the matching source. Do not move published tags or
replace signing identities; use a new version for subsequent changes.

The legacy upstream build workflow targets `master`; Galaxy XR releases use the
dedicated workflow above. CLI branch CI remains a quick independent check.
