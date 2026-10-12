# Local multiplayer

Development builds expose **Multijugador local** directly in the launcher.
Select your ROM and BIOS normally, enable the adaptive widescreen mod, and open
this button. No network, Discord account, Hamachi or external launcher is required.

## Options

- **Players/windows:** one through four. All consoles share one in-process cable
  session. Choose the game's multiplayer mode on each console to join the party;
  starting the windows does not skip the game's character and party menus.
- **Monitor:** `-1` automatically chooses the second monitor if present, otherwise
  the first. Explicit monitor indices start at zero.
- **Layout:** automatic uses the whole usable desktop for one window, two stacked
  windows for two, two above and one below for three, and a 2×2 grid for four.
  Equal rows and columns are also available. The taskbar is excluded.
- **Controller:** SDL device index, starting at zero. Defaults are 0, 1, 2 and 3.
  Duplicate assignments are rejected. `-1` disables the controller for that seat.
  Mapped controllers use SDL's game-controller bindings; unmapped DirectInput
  joysticks use axes 0/1, hat 0, buttons 0/1 for A/B, 8/9 for Select/Start and
  5/4 for R/L. The stick deadzone is configurable.
- **Audio:** only the selected player's audio is mixed into the output.
- **Integer scaling:** optional nearest-neighbor integer scaling may leave borders.
  Without it, proportional nearest-neighbor scaling fills the allocated tile as
  closely as the renderer's adaptive width limit allows. Pixels are never stretched
  to a different aspect ratio. Color output is RAW.

Keyboard input follows the focused window: arrows, X/Z, Enter/Right Shift and V/C.
Controller input continues when another application has focus. A detached controller
is reacquired by its SDL GUID when it returns; identical models are matched to an
available unclaimed device. Closing any window or pressing Ctrl+Q ends the session.

## Saves

Every seat has its own 64 KiB flash save under the launcher's configuration directory:

```text
saves/local/player-1/shining_soul_2.sav
saves/local/player-2/shining_soul_2.sav
saves/local/player-3/shining_soul_2.sav
saves/local/player-4/shining_soul_2.sav
```

These files are independent of ordinary solo and online saves. Existing files are
loaded at startup. Dirty saves are queued periodically and all saves are flushed
on normal shutdown. Write failures are reported rather than silently discarded.
This does not replace using the game's save function.

## Validation

`py -3.12 re/test_local_multiplayer.py` exercises the real hidden launcher handoff
and 1/2/3/4-window sessions, plus the column layout. It checks non-overlapping
placement, RAW adaptive presentation, distinct virtual mapped/raw joystick input,
and separate 64 KiB save files. Tests run muted with hidden windows. They qualify
boot, routing and persistence; they do not claim a full four-player combat playthrough
or compatibility with every physical controller.
