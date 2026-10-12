# v0.2.3 — Adaptive widescreen refinements and RAW colors

Rapid horizontal movement could display unrelated scenery or floating signs in
the expanded field margins. The authored-map renderer now reconciles the game's
full camera with the current scanline scroll, preventing a 256-pixel ring jump
during camera updates. Native game logic and the original viewport are unchanged.

## Presentation

- The equipment menu's animated decorative background now reflects into both
  widescreen margins as part of the main Adaptive Widescreen package.
- Inventory cells, cursor and statistics retain their original positions.
- Field transitions mask the expanded scenery while the original game closes
  its scene windows; sprites keep correct coordinates at the right screen edge.
- RAW is the default palette mode, including launches that bypass the launcher.
- The retired separate equipment-background package is removed from new builds.

## Validation

- Supplied state 5: 480 continuous movement frames at 480x160 and 1,800 at
  284x160, reversing direction without releasing movement.
- The 480-frame comparison found no changes in guest-state hashes or native
  viewport pixel hashes.
- Both traces reported FULLY_STATIC with no dispatch misses or interpreted instructions.
- PPU regression fixtures passed camera/scroll phase mismatches, mirrored
  inventory pixels, UI isolation, right-edge sprites and closed scene masks.
- Equipment-menu enabled/disabled comparison: 300 frames with matching guest
  and native viewport hashes, as documented in the equipment validation report.
- Final release executable: a further 480-frame state-5 trace passed. Four
  native experimental peers recovered from a three-second network cut at
  tick 45 and completed tick 120 with identical final snapshots.

These checks cover the recorded routes, not every map or the entire game.

## Online status

Existing experimental LAN/Hamachi recovery and Discord-linked recomp-net lobbies
remain available. This release does not introduce the new decomp-backed online
architecture. Existing confirmed-input play may wait or slow down under network
latency. Modern asynchronous gameplay synchronization is a separate development
effort; no zero-latency or seamless-reconnection claim is made here.

## Installation

Extract the full ZIP and open `build/ShiningSoul2Recomp.exe`. Supply your own
verified Europe ROM and original GBA BIOS, then enable Adaptive Widescreen in
Mods. Keep your existing saves and credentials separately when updating.

The ZIP excludes ROMs, BIOS dumps, user saves, save states, credentials and logs.
