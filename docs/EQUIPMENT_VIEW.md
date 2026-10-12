# Adaptive equipment background

The equipment background is part of the main **Adaptive Widescreen** package.
The same Mods switch controls the field and equipment presentation; there is no
separate inventory option. It expands the original scrolling green
decorative background without stretching the image. The interactive inventory,
item sprites, equipment slots, cursor, statistics and description remain centered
at their original coordinates and pixel size.

## Rendering contract

The European inventory uses mode 0, `DISPCNT=0x1D40`,
`BG0CNT=0x1C08`, `BG2CNT=0x9A01`, and `BG3CNT=0x1F03`.
BG3 is the scrolling 256x256 decorative texture. The expanded columns reflect
it at the native viewport's pixel edges: X=-1 samples X=0, and X=240 samples
X=239. Scroll and palette animation stay intact, with no scaling or texture seam.
BG2 contains inventory cells,
labels and description panels; repeating BG2 would expose unrelated menu content.
Only BG3 and color effects are allowed in the added columns. BG0, BG2 and OBJ
cannot leak into these margins. Native columns are composed normally.

The provider recognizes this register layout conservatively. Loading/fade frames
and other screens retain their existing policy. Field maps still use authored
map data rather than repeating the hardware tile ring. No guest RAM, ROM, save
data or controller coordinates are modified.

## Regression procedure

`re/test_scene_transition.py --slot 5 --open-menu --frames 300` opens the
equipment menu from the user's local state 5 using Start, then A. Add
`--native-view` to disable the main widescreen package **only in the isolated
test directory**.
Compare every guest state hash and native viewport hash, check that disabled
margins remain black and enabled settled margins contain the decorative plane.
Tests are muted and windowless; personal save states and mod selections are not
modified.

This expands the background, not the inventory capacity or arrangement. Moving
interactive cells would require coordinating the menu's hit testing, navigation
and sprite/background emitters.

## Validation

The enabled and disabled builds were compared for 300 equipment-menu frames:
all guest state hashes and all native viewport hashes matched. The disabled
main widescreen package retained black margins. Settled enabled frames rendered the decorative
background in both margins.

`SS2SceneProbe <inventory-dump-directory>` also checks reflection pixel by pixel
through the actual PPU, verifies that BG0/BG2/OBJ do not leak into the margins,
and verifies that disabling the option restores pillarboxing. Existing right-edge
OBJ wrapping and closed-scene transition fixtures still pass.

The reproduced state-5 field artifact and its separate camera/scroll regression
are documented in [FIELD_MARGIN_FIX.md](FIELD_MARGIN_FIX.md). Continuous
left/right traces retain per-frame OAM attributes, guest hashes and screenshots.
