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

## Qualification so far

- Portable core: checksum corruption/truncation, duplicate and partial chunks,
  all seats for 2–4 players, 1,619 predicted-input corrections, bounded outage
  prediction and authoritative resume passed.
- Four native peers, 180 frames, changing per-seat inputs and 50 ms proxy delay
  per direction: identical final snapshots, real rollback corrections observed.
- Four native peers, 180 frames, three-second network interruption at tick 60:
  resumed and finished with identical snapshots.

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
