# State-5 field margin regression

The field engine may update its full RAM camera before writing the matching
hardware scroll. During that interval, searching partially uploaded tile rings
can select an identical pattern one ring (256 pixels) away. The native viewport
continues to use the hardware ring, while authored widescreen margins show
unrelated scenery or floating shop signs.

The provider now anchors the origin to the full engine coordinate and reconciles
small camera/scroll differences against the current hardware scroll modulo 256.
Differences outside the validated 16-pixel movement interval retain the existing
ring-validation fallback. The change affects presentation only.

Validation on the supplied Europe state 5:

- 480 continuous horizontal frames at 480x160, reversing every 210 frames.
- Zero guest-state or native-viewport hash differences against the preceding build.
- The captured frame-150 margin no longer contains the reproduced floating signs.
- 1,800 continuous horizontal frames at 284x160 (16:9 rounded to whole pixels).
- Both traces report FULLY_STATIC, zero dispatch misses and zero interpreted instructions.
- `SS2SceneProbe` checks 1,800 margin tile samples with RAM camera offsets
  -8, -2, +2 and +8 before scroll writes, plus existing OBJ/transition and
  mirrored inventory checks.

This covers the reproduced state-5 case, not every map or possible transition.
No user save files are modified by these tests. RAW is the game configuration's
default palette mode; launcher configuration already selects RAW.

Test executable SHA-256:
`d318561af0e698a5bfb8080cc9644032bd73e2f1c57f74e523fcc9b69357f59f`.
