# Overport Galaxy XR 0.3.4

Includes the merged optional in-headset APP FPS overlay (PR #1), disabled by default. It measures successful app projection submissions, separately from display refresh rate. Worn-headset validation of the HUD remains pending.

The enabled Hand tracking permission switch now requests Android XR hand-tracking permission before the game starts regardless of Quest manifest markers. Previously the switch selected automatic detection, which could omit the request for games such as BONELAB. The disabled switch still omits the request; CLI `--hands=auto` retains automatic detection.

Repatch the original game APK and install the resulting APK to receive this change. Updating Overport alone does not modify installed games. Already granted permissions do not show another dialog. This change does not fix Vulkan eye-tracked foveation or the separately observed SystemUI crashes.
