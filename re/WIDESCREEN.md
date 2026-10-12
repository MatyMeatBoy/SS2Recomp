# Widescreen state-4 validation — 2026-10-11

Launch `build/ShiningSoul2Recomp.exe` or the root BAT. Fresh installations enable
adaptive view at 284×160, preserving 160 vertical pixels; fullscreen adapts to
the monitor. Menus and unsupported cinematics remain centered.

## Original fix

The provider initially accepted only decompressed EWRAM maps. State 4's castle
uses uncompressed ROM maps. `RunOptions::max_view_width` also had to be declared
for fixed/headless expansion. The IWRAM descriptors at `0x03003188`, stride 24,
contain these maps:

| Layer | ROM address | Tiles | Verified camera |
| --- | --- | --- | --- |
| BG1 | `0x08634B18` | 90×40 | (239,33) |
| BG2 | `0x08632EF8` | 90×40 | (239,33) |
| BG3 | `0x086312D8` | 90×40 | (239,33) |

Each matched 49/49 distributed visible-ring probes. Reads validate alignment,
dimensions and bounds. The scene cache resets at frame boundaries and restores;
HUD panels keep their native size and attach to the expanded edges.

## Reproduction

`re/ws4_probe.py` loads the user's state via TCP and dumps memory.
`re/test_widescreen.py` runs muted/windowless left/right inputs for 180 frames
from state 4 and states 1–3 for 120 frames each, using separate saves. Stationary
state 4 (120 frames) and root launch (30 frames) were also checked. Those runs
reported FULLY_STATIC and zero dispatch misses. Captures were inspected for real
map content and camera movement. These are route-specific, not whole-game or
multiplayer coverage. Map boundaries do not invent terrain. Game-side actor
spawn/culling expansion remains separate work.

See [the newer scene-transition fix](../docs/SCENE_TRANSITIONS.md) for window
masking, right-margin OBJ placement and 720-frame deterministic comparisons.
