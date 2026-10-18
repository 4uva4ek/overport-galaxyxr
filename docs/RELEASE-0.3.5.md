# Overport Galaxy XR 0.3.5

Eye tracked mode now updates the fragment-density map from live gaze in compatible Vulkan games. BONELAB testing confirmed movement in both axes after correcting vertical coordinates.

- Shared starting values: area 0.15, gain 24, density 0.35. Existing numeric fields and sliders configure each patched game.
- Vulkan uses a smooth quality transition, with full quality when gaze is invalid or stale. Normal mode does not include the diagnostic square or timed switching.
- Fix OpenXR loader initialization when the backend exposes xrInitializeLoaderKHR through xrGetInstanceProcAddr only.
- Includes the existing FPS HUD and hand-permission changes from 0.3.4.

Vulkan support currently requires the game's existing fragment-density attachment and the supported Galaxy XR runtime version. This is not support for every Vulkan title. Runtime map refresh has not been benchmarked; visual tracking was confirmed, but long-session stability and the new gain 24 tuning still need testing. The same values can produce different visual results on GLES and Vulkan.

Repatch and reinstall games to apply the new backend or settings. This release does not modify Suite modules, system files, or firmware.