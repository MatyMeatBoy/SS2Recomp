# Netplay validation — 2026-10-12

Tests use separate processes with verified local assets and compare complete
final states. Local network impairment is not a substitute for sustained gameplay
between homes. Latency values below are one-way proxy delay, not RTT; guest-to-guest
traffic through a 3–4 player host traverses additional legs.

## v0.2.2: port handling and ICE handoff

Binary SHA-256: `45273e43495db77c31cd2e8706d1ce76274e4eb573f23071ff1fda702125741a`.

| Path | Scenario | Result |
| --- | --- | --- |
| Experimental, 2 instances | Launcher + supervisor, 90 ticks | Equal states; 8.81 s |
| Experimental, 4 instances | Three-second TCP cut, 120 ticks | Reconnected, equal states; 20.81 s |
| Experimental, occupied port | Attempt to create room | Visible form error; launcher remains open |
| Online, 2 instances | Real private lobby, stored login, ICE, 16:9 | 120 ticks; equal checkpoints; both exit 0 |
| ICE library, 3 sessions | Adopted room agents, routed guest traffic | `host_ice_test` PASS |

The online test creates/joins a real backend room and waits for ICE on one PC.
It uses an existing stored login, neutral controls and a short boot sequence;
it does not automate authentication or qualify external-home combat.
[Public results](recovery-v0.2.2-results.json).
Reproduction: `py -3.12 re/test_online_launcher.py` requires a stored local login.

## v0.2.1: integrated launcher

Binary SHA-256: `30736d328f15f45eb82a9e03ad4a0993f2237c801909b02787fc58873c512b61`.

| Consoles | Scenario | Result |
| --- | --- | --- |
| 2 | Guest NTSTATUS crash at tick 30, automatic restart | 120 ticks; equal states; 11.20 s |
| 4 | Four launcher participants | 120 ticks; equal states; 11.20 s |
| 3 | Distinct class-selection inputs, 12-second TCP cut | 120 ticks; equal states; 32.20 s |
| 2 | Extracted release ZIP, launcher + supervisor | 90 ticks; equal states; 7.09 s |

Dashboard/form captures were inspected for original cover art and four seats.
Tests were hidden and muted. Diagnostic GUI scripts do not establish complete
manual workflows, physical controller mapping or separate-home performance.
[Public results](recovery-v0.2.1-results.json).

## v0.2.0: recoverable TCP

Binary SHA-256: `afd336e0e07af70a70c944774b59e30bbb00caa3a175659749cccba2ef69666a`.
Every listed run exited 0 and agreed on full final states.

| Consoles | Scene | Injected fault | Ticks | Time |
| --- | --- | --- | --- | --- |
| 3 | Distinct class-selection inputs | Host killed/restarted at tick 60 | 180 | 39.17 s |
| 3 | Distinct class-selection inputs | Guest EWRAM corruption at frame 30 | 120 | 29.05 s |
| 4 | Boot | Guest killed/rejoined at tick 30 | 120 | 12.80 s |
| 2 | Boot | NTSTATUS crash and supervisor restart | 120 | 11.06 s |
| 3 | Distinct class-selection inputs | 65-second TCP cut at tick 30 | 120 | 83.83 s |

The cut proxy counted 10,830,324 cumulative bytes, including full join/recovery
snapshots; this is not steady-state bandwidth. Sampled memory was 76–81 MiB per
instance, not a bound. Independent ROM-free protocol tests also covered ordinary
2-player traffic, host restart, divergence, 4-player guest restart and a 65-second
3-player cut. They complement rather than replace actual GBA tests.
[Public results](recovery-v0.2.0-results.json).

## Historical UDP results

| Players | Mode | Ticks | One-way latency / jitter / loss | Result |
| --- | --- | --- | --- | --- |
| 2 | Rollback | 360 | 80 ms / ±25 ms / 2% | Equal checkpoints; exit 0 |
| 3 | Delay-sync | 360 | 45 ms / ±15 ms / 1.5% | Equal checkpoints; exit 0 |
| 4 | Rollback | 360 | 45 ms / ±15 ms / 1.5% | Equal checkpoints; exit 0 |
| 4 | Delay-sync | 600 | 45 ms / ±15 ms / 1.5% | Equal checkpoints; exit 0; 26.06 s |
| 4 | Historical safe UDP profile | 360 | 45 ms / ±15 ms / 1.5% | Equal checkpoints; exit 0; 34.70 s |
| 4 | Delay-sync | Target 600 | Same, guest 1 cut for 2 s at second 6 | No checkpoint before 60-second deadline |

Successful neutral-input rollback tests reported `replay=0`, so they do not prove
late-input correction or imperceptible rollback. Total duration includes startup,
state transfers and checkpointing; it is not gameplay FPS. The 4-player cut failure
was reproducible, but its first blocking condition was not located.

### Distinct-input follow-up

`PrimatesGBA/test/party.txt` was adapted into `re/party_native_es.txt` with a
720-tick startup wait instead of 420. Captures verified Spanish menus and three
classes. This is class selection, not linked combat. `SS2LinkProbe` reuses the
application's compiled guest corpus and the framework's link-session tools.
It installs Flash dispatch per console and changes seat-specific buttons every
three ticks. Probe `.paired` fixtures are native test snapshots, not interchangeable
with the application's ordinary saved-session archives.

| Three-console test | Result |
| --- | --- |
| Loopback rollback, distinct inputs | One corrected rollback tick per process; equal states |
| 45 ms / ±15 ms / 1.5%, default delay | Equal states, no replay; does not satisfy `--require-replay` |
| Rollback, 100 ms / ±60 ms / 2%, requested delay 2 | 16/16/20 RESIM episodes; checkpoint digest mismatch |
| Delay-sync, 100 ms / ±60 ms / 2%, requested delay 2 | Equal states; exit 0 |
| Delay-sync, 45 ms / ±15 ms / 1.5%, two-second cut | Equal states; exit 0; 14.84 s |
| Historical safe UDP, same impairments, 65-second cut | Equal states; exit 0; 79.14 s |

Long-cut final SHA-256: `906882bd819c9acd4fa19886651ff2da9245570e33aecac0794ac91a1666a8ca`.
Probe SHA-256: `a6668d802d66fd7b50c4dfb88e2c7ab5b72c3f33c4e0dfb78475cadd00a2f723`.
That cut observed 1,461,378 datagrams, including 861,529 dropped during the artificial
blackout. Four executable processes generated 1,422,821 datagrams in 34.70 seconds
even without a cut (binary `09d37a14d5cd572b58136d92962c7543c8594dde13caba1d308cf6cdb8ad2384`).
This excessive traffic still needs retransmission investigation; equality does
not imply efficient transport. These results belong to the older UDP profile.

Stress memory samples were 304–307 MiB per process, about 917 MiB total; neutral
4-player samples were approximately 81 MiB each, 324 MiB total. Distinct-input
loopback rollback reported about 11.6 forward FPS; stressed delay-sync about 6.5.
Each run simulated nine consoles on one PC plus a proxy. These measurements do
not predict separate-PC performance or prove invisible corrections.

Flash code copied to EWRAM `02001688 Thumb` previously failed because the single-
player callback was not installed per session context. `setup_netplay_instance`
now installs the same verified-copy dispatcher on each `GbaInstance`. The
2-player test failed before and passed after that change, without generated-code
edits or additional HLE. Multiplayer logs do not emit the single-player coverage
summary, so no FULLY_STATIC claim is inferred from them.

## Reproduction and remaining qualification

Use your own assets and ignored local fixtures. Examples:

```text
cmake -S . -B build -DSS2_BUILD_LINK_PROBE=ON
cmake --build build --target SS2LinkProbe -j1
py -3.12 re/test_party_recomp.py --stop party_class
py -3.12 re/test_recovery.py --native --players 4 --frames 120 --at 30 --fault guest
py -3.12 re/test_recovery.py --native --players 2 --frames 120 --at 30 --fault watchdog
py -3.12 re/test_recovery.py --players 3 --frames 180 --at 60 --fault network --cut-seconds 65 --timeout 150
```

For native class-selection recovery, pass the local three-console `party.state`
through `--initial-state` and `re/party_stress_inputs.txt` through `--inputs`.
For stressed UDP qualification, `re/test_netplay_local.py` supports `--players`,
`--mode`, `--latency-ms`, `--jitter-ms`, `--loss`, `--input-delay`, `--require-replay`,
`--probe-state`, `--probe-inputs`, and blackout options. Logs, snapshots and private
credentials remain under ignored `re/run/`.

Remaining work: prolonged linked combat with distinct inputs, checkpoint resume
on fresh sessions, external-home VPN/relay tests, sustained forward FPS and input
latency, traffic/RAM measurement, and first-divergence diagnosis for stressed
rollback. Recoverable prediction/pipelining is still being implemented; its
presence in source does not establish qualified gameplay. The original host is
still required; automatic host migration has not been implemented.
