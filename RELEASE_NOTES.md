# ShiningSoul2Recomp v0.1.1 — 2026-10-11

Historical visual/netplay release for Shining Soul II Europe AU2P.

## Included

Windows x64 static recompilation, recomp-ui ROM/BIOS/settings launcher, adaptive
16:9 authored field maps, repositioned native-size HUD, both EWRAM and ROM-backed
maps, 2–4 linked-console direct netplay, selectable delay-sync/rollback, paired
checkpoints, and ROM/BIOS/build/cable identity validation. Muted qualification
scripts exercise presentation, transport, loss, jitter and rollback. Launcher
branding removes inherited references to unrelated game projects.

## Validation

Four independent loopback processes each simulated four consoles using distinct
seats. Delay-sync and rollback completed 240 ticks with 45 ms latency, ±15 ms
jitter and 1.5% simulated loss, producing byte-identical paired checkpoints.
The field fixture validated three 90×40 maps and a shared camera; left/right
captures showed real additional content. Other documented state tests reported
FULLY_STATIC with zero dispatch misses. This does not establish whole-game coverage.

## Known limits

Each user supplies their own ROM and BIOS. Local impaired-network tests do not
qualify prolonged external-PC gameplay. Unsupported menus/dialogs/cinematics
retain native centered composition. Infrastructure supported reconnect/checkpoint
resume, but a real mid-game four-PC outage remained unqualified in this release.
Four-player rollback was experimental; delay-sync was recommended initially.
No ROMs, BIOS dumps, saves, states or derived dumps are distributed.
See README.md for assets and current setup; newer releases have additional fixes.
