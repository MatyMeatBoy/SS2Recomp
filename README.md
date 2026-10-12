# ShiningSoul2Recomp

<p align="center">
  <img src="docs/branding/boxart.png" alt="ShiningSoul2Recomp" width="300">
</p>

<p align="center">
  <a href="https://github.com/MatyMeatBoy/SS2Recomp/releases/latest"><img src="https://img.shields.io/github/v/release/MatyMeatBoy/SS2Recomp?style=flat-square&color=d6a94a" alt="Latest release"></a>
  <a href="https://github.com/MatyMeatBoy/SS2Recomp/releases"><img src="https://img.shields.io/github/downloads/MatyMeatBoy/SS2Recomp/total?style=flat-square&color=335c9e" alt="Downloads"></a>
  <a href="https://github.com/MatyMeatBoy/SS2Recomp/blob/master/LICENSE"><img src="https://img.shields.io/badge/license-MIT-586069?style=flat-square" alt="MIT license"></a>
</p>

A static recompilation of **Shining Soul II (Europe, AU2P)** built on
gbarecomp, with adaptive widescreen and linked GBA multiplayer for 2–4 players.

## Project status

| Feature | Status |
| --- | --- |
| Static guest execution | Verified on documented test routes; whole-game coverage is not claimed |
| Adaptive widescreen | Real authored field maps, starting at 284×160; no stretching |
| Map transitions | Expanded scenery follows the game's scene-transition masks |
| Sprite placement | Right-margin coordinates preserved across X=255/256 |
| Recoverable LAN netplay | Experimental, 2–4 players; can wait for network confirmations |
| Discord-linked lobby / ICE | Two local processes tested through the real lobby; external-home gameplay remains unverified |
| Predictive recoverable netplay | Developer opt-in; short native local delay/reconnect tests passed, sustained gameplay unqualified |
| Published Windows release | [v0.2.4](https://github.com/MatyMeatBoy/SS2Recomp/releases/tag/v0.2.4) |

See [architecture](docs/ARCHITECTURE.md), [network validation](docs/NETPLAY_VALIDATION.md),
and [scene-transition validation](docs/SCENE_TRANSITIONS.md), plus the
[field-margin regression](docs/FIELD_MARGIN_FIX.md).
See [decomp-backed online development](docs/MODERN_ONLINE.md) for the new backend and its limits.

## Windows quick start

1. Download and extract the complete release ZIP, preserving its folders.
2. Open `ShiningSoul2Recomp.exe` at the package root. No BAT is needed.
3. Select your own ROM and BIOS in the launcher.
4. Use the Mods tab to enable adaptive widescreen. This includes the mirrored
   equipment backdrop; interactive cells and cursor stay centered.
   Adaptive widescreen starts at 284×160 and adapts
   to the display in fullscreen. Menus and unsupported scenes retain native composition.

Required assets:

| Asset | Expected SHA-1 |
| --- | --- |
| Shining Soul II Europe, AU2P | `aa1288be9257337abdfe3034898cf6c7aa778fbc` |
| Original 16 KiB GBA BIOS dump | `300c20df6731a33952ded8c436f7f186d25d3492` |

The repository and releases do not include ROMs, BIOS dumps, saves, save states,
or ROM-derived generated code. Supply your own compatible files. Source builds
also recognize `roms/shining_soul_2_europe.gba` and `gbarecomp/bios/gba_bios.bin`.

## Multiplayer

**Netplay (experimental)** is the integrated direct-TCP recovery profile. Choose
2–4 participants and one host. Guests select distinct seats and enter the host's
reachable LAN or VPN address, the same TCP port (19740 by default), and room ID
(24683 by default). Two instances on one PC can use `127.0.0.1`.

Hamachi is optional: any reachable LAN/VPN works. Tests on one PC do not prove
performance between homes. Each PC simulates every linked console internally;
owner saves are exchanged at startup. The current recovery profile waits for
confirmed inputs and matching state digests, so network latency can reduce
forward progress. Development builds offer opt-in prediction and an independent
host-authoritative backend (`SS2_AUTHORITATIVE_ONLINE=1` on every PC): a missing
guest no longer stops the host or healthy peers, and recovery repairs that guest
individually. This is not yet the public default or qualified for sustained combat
and real-time performance. See [implementation and test limits](docs/MODERN_ONLINE.md).

The original host owns the recovery checkpoint and input journal. Keep the room
folder to resume after a crash or connection loss. The original host must return;
there is no automatic migration to another PC. See [recovery design and setup](docs/NETPLAY_RECOVERY.md).

The ordinary **Netplay** button uses recomp-net, with lobby/ICE and direct UDP
paths. Recovery from the experimental TCP profile does not automatically apply
to that transport. Rollback is experimental: stress tests detected checkpoint
mismatches, while delay-sync agreed on the final state. Neither transport has
been qualified for sustained linked combat between separate homes.

Four-player direct UDP examples:

```text
ShiningSoul2Recomp.exe --netplay-players 4 --netplay-seat 0 --netplay-session 41011 --netplay-bind 0.0.0.0:21940 --netplay-delay-sync
ShiningSoul2Recomp.exe --netplay-players 4 --netplay-seat 1 --netplay-session 41011 --netplay-bind 0.0.0.0:21941 --netplay-peer HOST_IP:21940 --netplay-delay-sync
```

Each additional guest needs its own seat and local port. Use
`--netplay-rollback` to explicitly select rollback. Three- and four-player
sessions default to delay-sync. Paired checkpoints require compatible assets,
build identities and all participants; do not mix independent save states.

## Building and testing

Use CMake, a MinGW compiler with SDL2, and Python 3.12 for qualification scripts.
Initialize submodules and run:

```powershell
powershell -ExecutionPolicy Bypass -File tools/prepare_gbarecomp.ps1
powershell -ExecutionPolicy Bypass -File tools/regen.ps1
powershell -ExecutionPolicy Bypass -File tools/regen_bios.ps1
```

The preparation script pins the tested recomp-net revision and applies the
tracked framework patches. Configure CMake for the local toolchain, then build
with `-j1` on memory-constrained systems. Never edit `generated/` manually.
Check coverage reports instead of assuming every route is fully static.

Scene qualification is documented in [docs/SCENE_TRANSITIONS.md](docs/SCENE_TRANSITIONS.md).
Muted tests use isolated saves and hidden/windowless instances. Network logs and
private room credentials stay under ignored local directories.

## Credits and licensing

Built on [gbarecomp](https://github.com/mstan/gbarecomp),
[recomp-ui](https://github.com/mstan/recomp-ui), and recomp-net. Game-specific
research also uses [SS2Decomp](https://github.com/MatyMeatBoy/SS2Decomp).
The application code is MIT-licensed; dependencies retain their own licenses.
Shining Soul II and its assets belong to their respective rights holders.
