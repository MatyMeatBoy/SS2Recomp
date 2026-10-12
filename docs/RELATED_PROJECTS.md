# Related Shining Soul II projects

## SS2-netplay

[Ddhuet/SS2-netplay](https://github.com/Ddhuet/SS2-netplay) implements a separate
experimental mGBA/Tango rollback harness. Its useful material is the methodology:
record ordered per-console inputs and starting save identities, replay a direct
baseline, then compare settled states under delayed-input rollback. Diagnose the
first differing subsystem rather than masking differing bytes or declaring
recovery a correctness fix.

Its provenance notes describe restore fixes involving RCNT, savedata, timing
queues and video contention. These are **audit targets**, not patches that can
be copied into gbarecomp: the runtime and snapshot representations differ.
Some dependency mirrors are private. Its September 6 shop desync investigation
also demonstrates that correctly delivered inputs alone do not establish correct
rollback restoration. No code from that project has been integrated here.

## Speedrun autosplitter

[speedrun-shining-soul-II](https://github.com/lIlIl-IlIlIlIl/speedrun-shining-soul-II)
is a BizHawk/LiveSplit Lua autosplitter for the **Japanese** ROM, not a recorded
speedrun or a controller playback. Its ordered eleven boss milestones can inform
a future full-game regression route. Its Japanese memory addresses and boss HP
values must be independently verified against the supported European ROM before
using them as automated assertions. This source does not establish start-to-finish
coverage for ShiningSoul2Recomp.
