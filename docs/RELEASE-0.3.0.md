# Overport Galaxy XR 0.3.0

Hand joint frames aligned to the Meta conventions and a hand-tuned bind pose.

- **Hand joint frames** (`--hand-frames=meta`, default). The Galaxy XR runtime rolls the wrist and
  the little-metacarpal joint frames about their bone axes differently from the Meta runtime whose
  conventions Quest hand rigs expect (captured SM-I610 data: wrist +Y rolled 13 degrees toward the
  thumb from the palm-plane normal, little metacarpal rolled 5 degrees toward the thumb where the
  Meta `Pinky0` bone rolls 24 degrees away from it). The adapter now takes the wrist roll from the
  palm joint and rolls the little metacarpal 24 degrees toward the ulnar side; positions and the
  wrist Z axis are unchanged, so the tracked fingers stay where the runtime puts them.
  `--hand-frames=runtime` keeps the runtime frames.
- **Hand-tuned bind pose** (`--hand-bind=edited`, default). The bind joints handed to OVRPlugin
  come from a built-in table of 26 left-hand joints (positions, rolls and finger-tip orientations)
  tuned by hand on SM-I610 against the live joints and the Meta rest pose; the right hand is
  mirrored. Finger frames are re-derived along the bones, so parents, radii, mesh vertices and the
  live joints are untouched. `--hand-bind=rebuild` restores the bind pose derived from the runtime
  data only. The GUI uses the defaults.

Reconvert games from their original APK to get the new hand defaults; updating the application
does not change an APK converted by 0.2.0. Resolution, performance defaults, the EGL fix and the
open dispatcher are unchanged. 0.1.x and 0.2.0 users can install this version from **Updates**.

Validation: Job Simulator 1.4.0.6515 on Samsung SM-I610 (I610UEU2AZF3) with the open dispatcher,
`handframes=meta` and `handbind=edited`: hands tracked and drawn, the remaining offset of the
little finger follows the runtime joints themselves. Host tests replay the captured runtime data
for the default and the edited bind path on both hands; the converted APK was verified to carry
the built adapter and the configuration before installation. Other games remain untested.
