# v0.2.1 — Experimental netplay inside the launcher

LAN/Hamachi configuration and automatic recovery are integrated into
**Netplay (experimental)**. Separate host, guest and Online Seguro BAT launchers
are no longer needed. The ordinary **NETPLAY** button retains recomp-net rooms
and Discord-linked login on its separate transport.

## Playing

Extract the complete ZIP. Everyone uses the same version and their own European
ROM and BIOS. Open `build/ShiningSoul2Recomp.exe` or `Jugar ShiningSoul2Recomp.bat`.
For Hamachi, join the same private network and allow the app through its firewall.
The host selects the player count and creates a room. Guests use the same count,
room and TCP port, distinct seats and the host's reachable IPv4. Once connected,
enter multiplayer inside the game and accept the prompts.

This release defaults to room **24681**, TCP port **19740**. Use a new room after
a build-identity error or for a new game. Retain `build/netplay/recovery` and seat
credentials to resume an existing room.

## Changes

- Integrated 2–4 player form: seat, IP/DNS, room, port and view.
- Experimental button initially selects 16:9.
- Internal supervisor restarts Windows crashes, but not voluntary normal exits.
- Supervisor/game logs remain beside private recovery files.
- Original requested cover art replaces the previous illustration.
- ZIP includes launcher images, fonts and notices; separate online BATs removed.
- Interface/runtime patches retained for reproducible source builds.

## Validation and limits

Hidden launcher starts, crash recovery and complete state equality were tested.
See [the v0.2.1 evidence](https://github.com/MatyMeatBoy/SS2Recomp/blob/v0.2.1/docs/NETPLAY_VALIDATION.md).
The experimental profile is direct TCP for LAN/VPN/Hamachi. Ordinary online rooms
do not inherit it. Separate-home gameplay and live Discord were unverified in
this release. Disconnects pause progress; latency can delay gameplay. The original
host must return; automatic migration is unavailable.

No ROMs, BIOS dumps, saves, states or credentials are included. Dependency notices
ship with the package; gbarecomp retains PolyForm Noncommercial 1.0.0. Multiplayer
coverage is not labeled FULLY_STATIC without a corresponding coverage report.
