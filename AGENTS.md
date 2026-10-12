# Working agreements

- Keep user-facing responses in Spanish and as short as practical through 2027.
  Keep source documentation, commit messages and release notes in English.
- Online fluency is the primary acceptance criterion. Reject designs that stop
  the entire world for a delayed peer or repeatedly replay expensive whole-world
  snapshots. Correct final hashes alone do not qualify an online implementation.
- Report separately: synchronization/recovery, frame pacing, actual gameplay,
  and real Internet testing. Boot/menu probes are not combat qualification.
- Target 2–4 players. Rejoining a guest must not roll back or pause healthy peers.
- Preserve user saves, states and credentials. Never distribute ROMs, BIOS or
  ROM-derived generated sources. Do not edit generated recompilation shards.
- Keep RAW color as the default and adaptive widescreen without stretching.
- Prefer muted headless tests. Visible tests use the second monitor and must not
  steal focus. Monitor available RAM and build with low parallelism while the
  user plays on the primary monitor.
- Cheat codes and character hacks are deferred until the user requests them again.
- Publish an EXE at the root of release ZIPs; launcher flows should not require
  users to invoke BAT files. Preserve older releases and user data during cleanup.

See `docs/MODERN_ONLINE.md` and `docs/online-development-results.json` for the
current development backend, measured limitations and qualification evidence.
