# v0.2.2 — Experimental rooms and ICE connections

Creating an experimental room on an occupied port previously closed the launcher
and left the error in a hidden log. Port availability is now checked before
leaving the form, so users can change it and retry. Later supervisor failures
show an error dialog with the log location.

## LAN / Hamachi

Open `build/ShiningSoul2Recomp.exe` and **Netplay (experimental)**. The host
selects 2–4 players and creates the room. Guests choose different seats and use
the same room/port plus the host's reachable Hamachi IPv4. Two instances on one
PC use one host and one guest connecting to `127.0.0.1`. Choose another shared
port if the default is occupied or reserved.

The initial room ID is now **24683**, separating recovery files from the previous
build. Keep old files: this version neither deletes nor converts states from
another build identity. Waiting/reconnecting shows an in-game panel, including
fullscreen. TCP still uses confirmed inputs and can pause while a participant
is missing. There is no promise of zero latency, invisible rollback or host migration.

## Discord-linked / recomp-net rooms

Static libjuice is enabled, and connected lobby ICE agents are transferred into
the GBA session. Previously the runner attempted conventional UDP endpoints
without adopting the transport selected by the launcher. Agents are now retained
until the runner takes ownership, seat mappings are validated, and failed starts
release their resources.

## Validation

- Two experimental launcher instances: 90 ticks, clean exit, identical final states.
- Four experimental launcher instances: 120 ticks, three-second network cut,
  successful recovery and identical final states.
- Occupied port: visible form error; launcher stays open.
- ICE library: three sessions adopt lobby agents and route guest datagrams via host.
- Real online lobby: two instances on one PC, stored login, ICE, 16:9 and identical
  confirmed checkpoints at tick 120.

These short local tests do not qualify sustained combat between countries.
ROMs, BIOS dumps, saves, credentials and private logs are excluded from the ZIP.
