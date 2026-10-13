# Galaxy XR validation, 2026-10-13

Beat Saber 1.40.8, Samsung SM-I610, ARM64 Overport runtime 3.4.3-aa54c3f.
The user confirmed launch, gameplay, improved clarity, and pause with the left
controller's separate Menu button. Compatibility with other games remains untested.

## Startup

The original conversion initialized OpenXR but failed at shared EGL context
creation with `EGL_BAD_MATCH`, then crashed after the unsuccessful session call.
Disassembly identified Unity's no-error EGL context request. Disabling that
request in the inspected Unity binary resolved startup in the headset test.
The complete runtime context attributes were not independently captured.

`UnityGlesCompatibility.kt` changes one byte (`22` to `02`) at file offset
`0x7f2a78` (VA `0x7f3a78`), changing `mov w2, #1` to `mov w2, #0`.
Full SHA-256 and instruction checks restrict the patch to this known binary:

- Input: `8203664e3b9203e9731251ea958ca2b1d9f0fc1e3b10032c34a61e5ee8068891`
- Output: `7d468de5616967bd87a38a03e685641ade94c0d1f9abe4c8e6a642e5dadc9c05`

Unknown Unity binaries are left untouched with an explicit `NOT APPLIED` report.
Already patched inputs are recognized. The Galaxy preset selects this patch;
the upstream recommended patch list does not.

Update 2026-10-14: the digest guard was replaced by an instruction-signature search
that produces the same output for this binary; see
[JOB-SIMULATOR-GALAXYXR-2026-10-14.md](JOB-SIMULATOR-GALAXYXR-2026-10-14.md).

## Resolution and performance

The initial runtime recommendation was 1856x2160 per eye. The user confirmed
improved clarity at 2320x2700 and requested 3552x3840. The game multiplies menu
resolution, producing 4272x4608 textures, which triggered Overport's own 4096px
abort. `SwapchainCompatibility.kt` replaces only the two branches leading to
that abort. Native OpenXR remains responsible for validating actual limits.

- Known loader input SHA-256: `21821370b886ac25b048836573a68438dc45c0cf9f3e7e6c9f415e28e7578b05`
- Patched SHA-256: `620a025ef73eb9f306dcae9db4e6097ebaa7d8343a3f9ab4db4409b9a65bf39d`
- Instruction offsets: `0x18b8c`, `0x18b94`; unknown inputs fail closed.

Device logs confirm successful 3552x3840 and 4272x4608 swapchains, accepted
CPU/GPU BOOST requests, supported refresh rates 60/72/90 Hz, and an accepted
90 Hz request. The user reports good operation. Accepted hints are not measured
clock speeds or a guarantee of sustained 90 FPS.

The source-built application-local loader adapter preserves the original generic
loader as a sibling library. `--performance=app` restores app control; unsupported
extensions and refused hints do not turn a successful session begin into failure.

## Quest controller layout

Read-only diagnostics confirmed matching OpenXR clicks and original OVRPlugin
button bits: A `0x1`, B `0x2`, X `0x100`, Y `0x200`, Menu `0x100000`.
The user confirmed that the separate left Menu button opens pause during a song.
No remapping is needed or included. The optional diagnostic observer is compiled
out of release builds. OVRPlugin remains byte-identical to the input.

## Checks and scope

Focused checks cover guarded binary edits, idempotence, unknown/truncated inputs,
target defaults, native extension negotiation, session lifecycle, failed hints,
90 Hz availability and app-controlled performance. Java 21 builds the CLI;
NDK r29 builds the ARM64 adapter. APK signatures are checked before replacement.

Only the converted non-system APK is updated. No root, module, system runtime,
firmware or persistent device setting changes are involved. A saved previous APK
with the same signing key provides reinstall rollback; reboot does not undo an
APK update. Force-stop independently stops the app and adapter.

Game binaries and raw diagnostics are retained locally, not distributed.

- [EGL no-error context specification](https://registry.khronos.org/EGL/extensions/KHR/EGL_KHR_create_context_no_error.txt)
- [OpenXR refresh request](https://registry.khronos.org/OpenXR/specs/1.1/man/html/xrRequestDisplayRefreshRateFB.html)
- [Samsung controller buttons](https://www.samsung.com/us/support/answer/ANS10007502/)
