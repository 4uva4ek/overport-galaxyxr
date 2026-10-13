# Releasing Windows and Android together

Workflow: `.github/workflows/galaxyxr-release.yml` (Galaxy XR applications).
It builds the native adapter once, then builds Windows and Android in parallel.
Both must succeed before the release job can publish. No game inputs are needed.

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
   Manual runs build both applications and upload artifacts without publishing.
5. Download and check the GUI and conversion results. Retain the existing signing
   workspace when rebuilding an installed game.
6. Create and push `galaxyxr-vVERSION` at the validated commit. Tag runs build both
   applications and publish one release only when both succeed.

Release assets: signed Android APK, Windows x64 portable ZIP with Java 21 runtime,
optional CLI JAR, and `SHA256SUMS.txt`. Windows packages are not Authenticode signed.
GitHub source archives provide the matching source. Do not move published tags or
replace signing identities; use a new version for subsequent changes.

The legacy upstream build workflow targets `master`; Galaxy XR releases use the
dedicated workflow above. CLI branch CI remains a quick independent check.
