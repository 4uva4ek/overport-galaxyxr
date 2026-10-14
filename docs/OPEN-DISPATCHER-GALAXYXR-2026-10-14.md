# Open OpenXR dispatcher on Galaxy XR, 2026-10-14

Converted Quest games load `lib/arm64-v8a/libopenxr_loader.so` first. Until now that file was
the closed Overport dispatcher 3.4.3-aa54c3f from the runtime cache, patched by
`patch_galaxyxr_swapchain` to remove its 4096 px swapchain abort. The Galaxy XR preset now
packages [openxr-dispatcher-open](https://github.com/Molier/openxr-dispatcher-open) 1.0.0
(MIT, commit `68479ea28e69202577cba51fbcab9baeb827a7fc`) there instead; `--dispatcher=overport`
and the GUI switch restore the closed dispatcher with its swapchain fix.

## Compatibility with the adapter

Read from the upstream sources before the headset test:

- Next loader: for `ro.product.brand` samsung the dispatcher loads `libopenxr_loader_generic.so`
  by name, which the conversion replaces with the Galaxy XR adapter; the adapter keeps loading the
  preserved original loader `libopenxr_loader_gxrbase.so` by its sibling path.
- Function resolution: every function, including `xrInitializeLoaderKHR`,
  `xrEnumerateInstanceExtensionProperties` and `xrCreateInstance`, is resolved through the next
  loader's `xrGetInstanceProcAddr`. The adapter substitutes its own hooks in that call, so its
  performance, refresh-rate, hand-tracking and eye-foveation hooks are reached.
- Settings: the dispatcher reads the same `liboverport.config.so` keys the closed one reads
  (`suggested_texture_*`, `suggested_cpu_level/gpu_level`, `verbose_logging`, ...), found through
  `dladdr` without JNI.
- Answered locally like the closed dispatcher: foveation profiles, `xrUpdateSwapchainFB`,
  `xrGetSwapchainStateFB`, `xrGetRecommendedLayerResolutionMETA` (1504x1504), the performance
  metrics functions. `xrGetHandMeshFB` is forwarded and then clamps joint parents above 25 to 0;
  the rebuilt hand skeleton already lists the palm as the wrist parent.
- Oversized swapchains and unknown formats are passed to the runtime instead of aborting, so the
  hash-guarded swapchain patch is unnecessary and, because its digest check targets the closed
  binary, not applicable.

## Build and packaging

`native/galaxyxr/build_dispatcher.sh` clones the pinned commit, builds it with the upstream
`CMakeLists.txt` (NDK 29.0.14206865, CMake 3.22.1, Ninja; API 29, Release), checks the SONAME,
the NEEDED list (liblog, libdl, libm, libc), 16 KiB LOAD alignment and the export list
(57 exports: all 56 of the closed dispatcher plus `xrLocateSpaces`), compares the packaged
licence notices with the upstream files and stores the result as the CLI resource
`galaxyxr/arm64-v8a/libopenxr_loader.bin`. The local Windows build produced the same bytes as
the earlier local build (SHA-256 `bfd0f452be91187ce88f4eb70f6fc6cdd1bae09e33250fcba95635e61426e561`,
75416 bytes). Both CI workflows build it next to the adapter; `tools/check_packaged_adapter.py`
verifies the jar and the Android APK carry the adapter, the dispatcher and the notices.

The Linux CI build of the same commit (run 38489581588, NDK 29.0.14206865) is 75256 bytes,
SHA-256 `47c3ce136e97712762ea0fb7f96962a1ac7fc4a238e7214775ff994bc5da70f8`. A section-by-section
comparison with the Windows build shows identical `.text`, `.rodata`, `.data`, `.data.rel.ro`,
dynamic symbol and relocation tables, PLT/GOT and `.dynamic`; only `.comment` (the Windows
toolchain records two compiler identification strings), the `.note.gnu.build-id` derived from
it and the section-header offsets differ. Released packages therefore carry code identical to
the headset-tested binary.

Patches now run in `PatchStore` registration order regardless of the requested map order,
so `patch_copy_libraries` always precedes `patch_galaxyxr_dispatcher` and
`patch_galaxyxr_performance`; the GUI map (`mutableStateMapOf`) is not insertion ordered.

## Validation

Job Simulator 1.4.0.6515 converted with `--target=galaxyxr --hands=off` (open dispatcher
`bfd0f452...`, adapter `886dc8ae...`, preserved generic loader `07f34aaf...`, adapter config
`performance=max foveation=app hands=off handmesh=fix`), installed on SM-I610 at 04:05:20 and
launched by the user at 04:07:32 (`OpenXRDispatch` and `OverportGXR` tags):

- `open dispatcher 1.0.0: next loader libopenxr_loader_generic.so (brand 'samsung')`; settings
  read from the installed `liboverport.config.so` (3552x3840, cpu 4, gpu 4).
- Runtime reports 88 extensions; `xrCreateInstance(Unity, 24 of 52 extensions) -> XR_SUCCESS`
  with `XrInstanceCreateInfoAndroidKHR` added; the adapter logs
  `Policy: maxPerformance=1; extensions perf=1 refresh=1` for the same instance.
- `xrCreateSession (GLES) -> XR_SUCCESS`; Meta reference spaces mapped to LOCAL and native
  LOCAL_FLOOR; adapter BOOST requests accepted (CPU 0, GPU 0) and 90 Hz accepted among
  60/72/90 Hz.
- OVRPlugin creates its GLES swapchains (`4 stages`), the adapter rebuilds both hand skeletons
  from the runtime mesh, Unity reaches `Finished Setup`; the game process stays resumed in
  full space with no tombstone, ANR or system restart during the capture.
- Gaps logged once by the dispatcher: `XR_META_headset_id` and 28 further Quest extensions not
  offered by the runtime, Touch Pro / Touch Plus / Microsoft hand interaction binding profiles
  rejected (`XR_ERROR_PATH_UNSUPPORTED`), `xrSetColorSpaceFB(3)` refused by the runtime and
  answered with success, the chained frame struct 1000212000 removed.

Evidence: `diagnostics/openxr-dispatcher-open-20261014/` in the workspace (build logs,
converted APKs for both dispatchers, `verify.txt`, `launch-logcat.txt`); not distributed.
The `--dispatcher=overport` conversion of the same input reproduces the previous output
(closed dispatcher with the swapchain patch, SHA-256 `620a025e...`).

## Scope

Only the converted non-system APK changes; the headset keeps its firmware, system OpenXR
runtime and settings. The closed dispatcher remains available per conversion. Reinstalling the
previously saved APK with the same signing key is the rollback.
