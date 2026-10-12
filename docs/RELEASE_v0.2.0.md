# v0.2.0 — Automatic room recovery

Experimental Windows x64 release. Connection loss pauses confirmed advancement
until the participant returns. Reconnecting restores the complete linked-console
and cable state; remaining players do not need to create a new game.

## Changes

- Direct TCP `--netplay-safe` profile for 2–4 players.
- All-player inputs and full-state digests confirmed before each frame commits.
- Automatic transient-divergence repair; persistent errors fail explicitly.
- Atomic checkpoints and durable confirmed-input journal for original-host restart.
- Seat credentials, build/ROM/BIOS checks, and connection generations.
- Windows crash supervision; voluntary window closure does not restart the game.
- Historical `Online Host.bat` / `Online Invitado.bat` launchers with room/player/seat
  arguments, original color settings and unstretched 16:9.

## Historical setup for this release

Extract the ZIP and select your own European ROM and BIOS through
`Jugar ShiningSoul2Recomp.bat`. Start `Online Host.bat` and have guests start
`Online Invitado.bat` with the host's reachable address. Allow TCP 19740 on the VPN.

```bat
"Online Host.bat" -Players 4 -Session 24680
"Online Invitado.bat" -Players 4 -Seat 1 -Session 24680 -HostAddress 25.x.x.x
```

Additional guests use seats 2 and 3. Match version, assets, player count and room.
Keep `build/netplay/recovery`; use a different session ID for a fresh room.
Credentials must remain private. Later releases integrate these launchers into UI.

## Validation and limits

Local tests passed original-host restart (3 consoles), injected divergence (3),
guest rejoin (4), automatic crash restart (2), and a 65-second TCP cut (3).
Every completed test produced identical full states. Scenes covered boot and
class selection with distinct inputs, not prolonged linked combat.

[Evidence](https://github.com/MatyMeatBoy/SS2Recomp/blob/v0.2.0/docs/NETPLAY_VALIDATION.md)
· [Protocol](https://github.com/MatyMeatBoy/SS2Recomp/blob/v0.2.0/docs/NETPLAY_RECOVERY.md).
The per-frame barrier can introduce delay. The original host must return with its
files and reachable address; no host migration. Discord/lobby/relay retain the
previous transport. External-home Hamachi and sustained combat remain unqualified.

The package excludes ROMs, BIOS dumps, saves, states and credentials. Dependencies
ship notices in `licenses`; gbarecomp retains PolyForm Noncommercial 1.0.0.
These multiplayer tests do not establish FULLY_STATIC coverage.
