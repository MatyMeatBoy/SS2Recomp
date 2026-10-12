# Architecture

ShiningSoul2Recomp separates deterministic guest behavior from presentation and networking.

```text
User ROM + BIOS -> gba_recompile -> generated/ -> ShiningSoul2Recomp.exe
                                                   | native 240x160 PPU
                                                   | adaptive widescreen
                                                   | recomp-ui launcher
                                                   | linked GBA session
                                                   + inputs / snapshots / confirmations
```

## Adaptive presentation

`src/ss2_extended_view.cpp` supplies real authored tilemap entries for expanded
columns. The castle fixture uses three 90x40 ROM maps. Providers validate
alignment, bounds, dimensions, scroll registers and the visible VRAM ring.
HUD panels retain their native size and move to the edges. Unsupported menus
and cinematics remain centered. [Transition policy and tests](SCENE_TRANSITIONS.md).

## Network execution

Every peer simulates all 2–4 consoles on one local virtual cable. Only inputs,
state snapshots, confirmations and control messages travel over the network;
video is rendered locally. Session identity includes ROM, BIOS, build, save
hardware and seat mapping. Each owner's battery save is installed at startup.

The ordinary recomp-net path supports delay-sync and rollback. Delay-sync is
the default for 3–4 players. Rollback remains experimental because stress tests
detected checkpoint mismatches. Confirmed paired saves require a shared tick.

The experimental Windows TCP profile adds durable checkpoints, an input journal,
seat credentials, reconnect and a crash supervisor. Its current confirmed-frame
barrier can slow forward progress with latency. Pipelined input delivery and a
bounded prediction timeline are work in progress and are not enabled yet.
See [recovery](NETPLAY_RECOVERY.md) and [qualification limits](NETPLAY_VALIDATION.md).

## Source provenance

Generated guest code comes from the user's verified ROM and is not tracked.
Framework integration changes are reproduced by `tools/prepare_gbarecomp.ps1`
using the checked-in patches. Never edit generated C++ as a bug fix.
Scripts in `re/` use muted, isolated fixtures; private artifacts remain ignored.
