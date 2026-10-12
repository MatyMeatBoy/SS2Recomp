# Decomp-backed online development

Reference: [SS2Decomp](https://github.com/MatyMeatBoy/SS2Decomp), pinned research
commit `70497b6fc1f81b657105c3135f1215dc87db2273`.

## Integrated code

`src/online/ss2_link_protocol.cpp` implements the EU cartridge's 24-byte
application block and MultiSio checksum contract from the matched
`sub_0800191C`/`sub_08001990` functions. Driver packets occupy 32 bytes; only the
first 28 bytes participate in the checksum. The unused overrun catcher is not
part of the application payload. The old reference `include/multi_sio.h` says
20 bytes, but the matched code and `structs.h` explicitly use 24.

The application mailbox assembler exposes complete announce/chunk messages
without inventing acknowledgments for missing data. It bounds messages to the
128-byte payload of the game's 0x88-byte queue entries, rejects conflicting
duplicates and resets incomplete messages on state restoration. The higher-level
broadcast/receiver C in the referenced decomp is still under NONMATCHING; its
semantics are research evidence, not proof of equivalence for a native replacement.

Set `SS2_ONLINE_LINK_TRACE=1` during development to observe complete messages in
the existing recovery runner. This is passive: it never modifies guest buffers.

## Predictive development backend

Set `SS2_PREDICTIVE_ONLINE=1` and use the existing experimental LAN/Hamachi path
on **all** peers. This is a developer-only backend, not a new launcher default.
The identity and recovery directory distinguish it from confirmed-input rooms.

Each PC still runs the linked consoles internally. The network sends frame-tagged
inputs rather than timing the emulated cable against the Internet. Local input
can advance before remote confirmation; held remote input is predicted for at
most 24 frames. Incorrect predictions restore the confirmed snapshot and replay
the saved local inputs. Replay does not submit new inputs or play duplicate audio.
Replay work is serviced between UI/network pumps with a four-millisecond budget
(an individual simulation step may exceed that budget).

The host orders confirmed input frames and supplies state hashes. Divergence
causes an authoritative state transfer and a new synchronization generation.
Wi-Fi loss discards speculation, reconnects with the existing seat credential,
restores the host's confirmed snapshot and resets the prediction timeline. Player
saves are installed before the initial authoritative snapshot is captured.
Only confirmed snapshots are persisted, hashed or used for rejoining.

This removes the per-frame network acknowledgment barrier in the development
backend. TCP remains its current transport, so loss can still block delivery.
Prediction has a finite horizon; sustained outages stop progress. There is no
zero-latency, zero-stutter, host-migration or Internet-combat guarantee.
Discord/ICE adoption for this backend is not yet implemented.

## Independent host authority (development)

Set `SS2_AUTHORITATIVE_ONLINE=1` on **every** peer to select the next backend
through the experimental LAN/Hamachi path. It uses a separate protocol identity
and `authoritative-v3` recovery directory. The public launcher default remains
unchanged while gameplay and performance qualification are incomplete.

After the initial save exchange and startup barrier, the host advances its
linked world without waiting for input or hash acknowledgments from guests.
Guest messages carry monotonically ordered input samples, independent of their
prediction timeline. The host applies the latest sample on its next tick;
samples expire after 250 ms, and a disconnected or repairing seat receives
neutral input. Its internal console stays connected to the local cable.
The host does not rewind for a late command.

The default authoritative path displays confirmed world frames without
whole-world prediction or rollback. Controls are sampled independently at the
local input cadence, even while waiting for the next world frame. This trades
round-trip input latency for avoiding expensive speculative state restoration.
Set `SS2_AUTHORITATIVE_PREDICTION=1` explicitly to compare the older predictive
path; it is a research option, not the fluency default.
A reconnect or hash mismatch transfers the current authoritative snapshot to
**that guest only**. The host and healthy peers do not restore old snapshots or
enter a new global synchronization barrier. In-flight replies are drained during
repair. Excessive divergence or outbound queue pressure disconnects the affected
seat rather than stopping everyone. Catch-up processes at most one committed
world frame per UI/network pump; replay retains its separate time budget.

This is a host-authoritative whole-world implementation, not yet native entity
replication. Each PC still simulates the internally linked consoles. An isolated
client stops receiving new world frames; other peers keep receiving the host's
world. Optional prediction has a finite horizon and can visibly correct movement.
Expensive simulation or replay can still miss the frame budget.
Disconnect neutrality does not mean invulnerability: the game may still damage
that character. Host migration and Discord/ICE routing are not implemented here.

The next game-level adapter must give player commands acknowledged sequence IDs,
make combat, loot, inventory, RNG and map transitions host-owned, deduplicate
reliable events across reconnects and interpolate remote entity presentation
without modifying authoritative state. These are required before describing
the result as a completed modern ARPG networking system.

Design references: Glenn Fiedler's
[client prediction and server authority](https://gafferongames.com/post/networked_physics_2004/)
and [snapshot interpolation](https://gafferongames.com/post/snapshot_interpolation/).

## Qualification so far

- Portable core: checksum corruption/truncation, duplicate and partial chunks,
  all seats for 2–4 players, 1,619 predicted-input corrections, bounded outage
  prediction and authoritative resume passed.
- Four native peers, 180 frames, changing per-seat inputs and 50 ms proxy delay
  per direction: identical final snapshots, real rollback corrections observed.
- Four native peers, 180 frames, three-second network interruption at tick 60:
  resumed and finished with identical snapshots.
- Host-authoritative backend: four native peers, 300 frames, three-second
  interruption of all guests: host continued advancing; identical final states.
- Host-authoritative backend: interruption of guest 1 only: the host and both
  healthy guests advanced during the outage, then all four final states matched.
- Host-authoritative backend: injected memory divergence at frame 60 was repaired
  and all four peers finished with identical states.

The recorded outage progress uses logs emitted every 30 ticks, so it is a
coarse lower bound, not a frame-rate measurement. The three-console story probe
also exposed insufficient real-time throughput on this development PC; reconnect
success is not evidence of a sustained 60 Hz gameplay budget.

A 120-frame, three-console story fixture measured state restore at 57.321 ms
mean / 75.944 ms p95, versus 3.247 ms mean serialization and 2.575 ms mean
probe digest calculation. This fixture is not a dungeon benchmark. Its restore
cost is sufficient to reject frequent whole-world rollback as the default
fluency strategy on this PC. Native player movement prediction will need a
smaller, game-specific reconciliation domain instead.

The latest four-peer, 300-frame headless run with 50 ms proxy delay per direction
passed final-state equality, but failed the fluency objective: mean presentation
gaps were about 39 ms and p95 gaps about 57–62 ms. All four peers shared one
development PC, so this is not a measurement of a friend's Internet connection.
It nevertheless prevents declaring the candidate ready. Correctness and fluency
qualification are separate; a `passed` recovery result means correctness only.

Native tests above exercise boot/menu simulation, not a connected dungeon battle.
The proxy approximates application delay on local TCP; it is not a real WAN or
packet-loss model. More work is required for gameplay event semantics, reliable
event sequence IDs, transport-independent resume, latency/jitter measurement,
rollback CPU/audio quality, four distinct characters and sustained combat.

## Integration policy

Do not copy hardware-register C into the host or edit generated recompilation
shards. Integrate verified contracts and pure routines behind native interfaces,
with differential fixtures against the original cartridge. Game-level event
replication will require replacing both the driver and the game's waiting
mailboxes/barriers, plus authoritative combat, inventory, map transitions and
random-state ownership. Merely replacing serial registers cannot provide that.
