# Widescreen scene-transition fixes

The development build now closes the expanded field alongside the original
240×160 viewport when Shining Soul II changes maps. Previously, its WIN0
transition hid the center while authored background margins bypassed that mask,
leaving strips of the previous scene and sprites visible at the edges.

An optional PPU margin-window callback now lets the game apply the same transition
mask to background and object pixels in the expanded columns. SS2 activates this
policy only for its observed WIN0 blackout configuration: inside mask 0, outside
mask `0x3F`. The callback reads the PPU's scanline IO snapshot. Normal gameplay,
dialog policies, and other games retain their existing window behavior.

The game also interprets world OBJ coordinates independently from relocated HUD
icons. Raw X values 256–304 remain on the right; wrapped negative positions are
decoded separately. Default signed 9-bit decoding previously moved right-margin
objects to the left at X=256. This fixes coordinate placement; it does not claim
to widen every actor-spawn or game-side culling rule.

Camera selection prefers the game's full scroll plus global and layer offsets,
validated against hardware scroll registers and the visible tile ring. Matching
fallback remains available for special maps. No guest memory, ROM code, save
format, or deterministic gameplay state is patched by these visual changes.

## Evidence

The local route starts from the user's state 4, walks down through the castle
exit and town, and reaches the world map. It runs without a window or audio and
uses isolated battery-save paths. Reference and candidate apps are staged with
fresh mod settings, ensuring both actually render at 284×160.

- 720 consecutive frames compared: zero guest-state differences and zero
  differences within the original 240×160 viewport.
- Closed-transition scenery leakage: 32 reference frames, zero corrected frames.
- The native runner reported `FULLY_STATIC` with zero dispatch misses on this route.
- `SS2SceneProbe` passed real PPU composition for a world OBJ at X=256, wrapped
  negative coordinates, and a completely closed transition containing BGs and OBJs.
- Existing left/right movement and states 1–3 widescreen regressions passed.
- Two recoverable netplay processes completed 120 ticks with identical final states.

Machine-readable results: [scene-transition-results.json](scene-transition-results.json).
This is route-specific evidence, not whole-game visual or multiplayer qualification.
The original game's intentional map-change blackout remains; old scenery leaking
around it is removed. Further actor culling work may still be needed in other areas.

## Reproducing

Use your own verified ROM, BIOS, state 4 and existing `ws4_*` debug dumps.
ROM-derived fixtures remain ignored under `roms/` and `re/run/`.

```powershell
cmake -S . -B build -DSS2_BUILD_LINK_PROBE=ON
cmake --build build --target SS2SceneProbe -j1
./build/netplay-testing/SS2SceneProbe.exe
py -3.12 re/test_scene_transition.py --exe build/netplay-testing/ShiningSoul2Recomp.exe --frames 720 --keys 0x37f --label candidate
py -3.12 re/test_widescreen.py
```

The TCP trace records every guest state hash, native viewport hash, camera,
map pointer, scroll register and window mask. Snapshots and logs remain local.

## Decompilation reference

Research used [SS2Decomp](https://github.com/MatyMeatBoy/SS2Decomp) at commit
`70497b6fc1f81b657105c3135f1215dc87db2273`, particularly `sub_08009594`
(full scroll to hardware registers), `sub_0800AD04` (camera clamping), and
the OBJ emitters in `code_08006D24`. Matching assembly and the user's original
ROM remain the reference; decompiled names and structures alone are not a
runtime correctness oracle.
