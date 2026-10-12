# v0.2.4 — Direct EXE package and online development groundwork

The Windows ZIP now opens directly with `ShiningSoul2Recomp.exe` at its root.
It contains no BAT entry points or nested build directory. SDL2, assets and Mods
stay beside the executable; internal recovery helpers remain under `tools`.
The experimental launcher locates those helpers in both the release layout
and existing developer builds.

All v0.2.3 RAW palette and adaptive widescreen fixes are retained, including
the state-5 field margins and mirrored equipment background.

## Online development

The source now includes the decomp-backed EU link-block/checksum contract, a
bounded application-message assembler, and an opt-in streaming/prediction
backend with authoritative recovery. This backend is disabled by default;
ordinary experimental rooms retain their existing behavior. Discord-linked
rooms do not yet use the new backend.

Portable rollback tests passed 1,619 corrections across 2–4 players. Four native
instances also passed changing inputs with added local proxy delay and a
separate three-second network interruption, finishing with identical snapshots.
These are boot/menu tests, not sustained online dungeon combat or a claim of
perfect responsiveness. See `docs/MODERN_ONLINE.md` for activation and limits.

## Start

Extract the complete ZIP, open `ShiningSoul2Recomp.exe`, select your own verified
Europe ROM and original GBA BIOS, then use the launcher. No ROM, BIOS, save,
credentials or private logs are included. Preserve your existing user data.
