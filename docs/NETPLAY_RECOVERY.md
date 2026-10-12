# Automatic room recovery — v0.2.2 (experimental)

On Windows, `--netplay-safe` with direct addresses selects recoverable TCP for
2–4 players. Ordinary recomp-net UDP/Discord/relay sessions use a separate
transport and do not inherit this recovery protocol.

## Starting a room

1. Open `build/ShiningSoul2Recomp.exe` and select your own ROM and BIOS.
2. Select **Netplay (experimental)**. One participant creates the room as host;
   guests choose distinct seats and enter its reachable IPv4 or DNS address.
3. Everyone must use the same player count, TCP port (19740 by default), room
   ID (24683 by default), build and assets.
4. A new room ID starts a fresh session. Retain the existing room directory and
   credentials to recover its last confirmed state.

Hamachi is optional; a reachable LAN or VPN is sufficient. Public-IP hosting
requires routing and firewall access to the TCP port. The protocol is intended
for known LAN/VPN participants; it does not provide TLS or public-service
first-join authorization.

## Recovery behavior

- Wi-Fi interruption: wait, retry every second, restore the common state.
- Guest restart: retain the local seat credential and receive the host state;
  an old independent battery save does not replace an established session.
- Host restart: restore the checkpoint and replay verified journal inputs.
- Windows crash: the integrated supervisor restarts the worker. Normal exit or
  validation errors do not cause a restart loop.
- Transient divergence: restore the host's confirmed state and require matching
  digests before continuing. Repeated failed repairs stop with an explicit error.

All consoles stay connected internally. Network admission controls frame
progress, rather than disconnecting the in-game cable. The current protocol
waits for every input and state digest, so it can pause or slow with latency.
It does not promise zero lag or imperceptible rollback. Prediction/pipelining
is under development and has not replaced this confirmed-frame profile.

## Durable authority

The original host is authoritative. It emits COMMIT only after matching full
state digests and flushing the corresponding journal record to disk. Every
120 ticks it atomically replaces the complete consoles-and-cable checkpoint.
Journal replay reconstructs subsequent frames and verifies every state hash.
An interrupted partial final record is discarded; corrupt complete records,
checkpoints or build identities produce explicit errors.

Connection generations prevent old packets becoming new inputs. A private seat
credential protects an occupied seat after its first join; do not share it.
Local files live in `build/netplay/recovery/room-ID/seat-N` and stay out of releases.
The original host must return with those files and a reachable address. There
is no automatic host migration or fixed-IP rediscovery. DNS is resolved again
when reconnecting. Different builds, ROMs, BIOS files or mappings are rejected.

## Evidence and remaining work

Local tests covered host/guest restarts, transient divergence, a 65-second cut,
and the integrated crash supervisor. They covered boot and distinct inputs
at class selection, not sustained linked combat or separate-home Hamachi play.
Equal states establish synchronization for those tests, not whole-game correctness
or acceptable sustained performance. [Detailed evidence](NETPLAY_VALIDATION.md).

## Future decompilation integration

`src/ss2_recovery.h` separates capture, restore, advance and per-tick input from
transport/persistence. `src/ss2_recovery_runner.cpp` adapts the linked GBA world.
A decompilation can replace the adapter while preserving authority and identity;
snapshot compatibility is not promised. Widescreen, color LUTs and local audio
are presentation state rather than simulation authority.

References: [Mario Kart Recomp](https://github.com/mstan/MarioKartSuperCircuitRecomp/releases),
[psxrecomp](https://github.com/RetroPortingToolKit/psxrecomp/blob/master/docs/NETPLAY.md),
and [GGPO API](https://github.com/pond3r/ggpo/blob/master/src/include/ggponet.h).
This release uses a project-specific TCP recovery protocol, separate from UDP rollback.
