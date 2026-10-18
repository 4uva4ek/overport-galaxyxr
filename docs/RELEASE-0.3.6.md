# Overport Galaxy XR 0.3.6

Experimental GLES eye-tracked foveation now holds the last valid stereo focal position for up to 300 ms when the tracker reports one or both eyes closed. This avoids immediate full-quality transitions during brief closed-eye reports, including potentially incorrect reports from the tracker.

- The deadline is measured from the last genuinely valid sample; held samples never extend it.
- Both eye coordinates update together, preventing a partially updated focal position.
- Longer closures, tracking/API failures, invalid view poses and session stops still fall back to full quality.
- Added diagnostics distinguishing closed-eye reports, stale frames and held gaze.
- Shared defaults remain area 0.15, gain 24, density 0.35.

This change applies to the GLES runtime-eye path. Vulkan support from 0.3.5 is unchanged. Foveation remains experimental; game compatibility and long-session stability require further testing. Repatch and reinstall games to apply this update. No Suite or system modifications.

Validation: targeted native tests cover holding, timeout, recovery, tracking loss and session reset; ARM64 build and test APK signature checks passed.