"""Muted, windowless state regressions. Never writes the user's saves/states."""
import os
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 're/run'
env = dict(os.environ, SDL_AUDIODRIVER='dummy', GBARECOMP_SS2_WS_TRACE='1')
for name, slot, keys, frames in [('left',4,0x3df,180), ('right',4,0x3ef,180),
                                ('state1',1,0x3ff,120), ('state2',2,0x3ff,120),
                                ('state3',3,0x3ff,120)]:
    replay = OUT / f'ws-{name}.input'
    # Replays use absolute PPU frames, including the loaded state's counter.
    replay.write_text(f'# gbarecomp-keyinput-v1\n0,0x{keys:04X}\n')
    runenv = dict(env, GBARECOMP_INPUT_REPLAY=str(replay))
    result = subprocess.run([str(ROOT/'build/ShiningSoul2Recomp.exe'), '--no-launcher',
        '--bios', 'gba_bios.bin', '--rom', '../roms/shining_soul_2_europe.gba',
        '--load-state', f'../roms/shining_soul_2_europe.state{slot}',
        '--frames', str(frames), '--no-window', '--volume', '0',
        '--save-path', f'../re/run/ws-{name}.sav',
        '--dump-png', f'../re/run/ws-{name}.png', '../game.toml'],
        cwd=ROOT/'build', env=runenv, capture_output=True, timeout=60)
    log = (result.stdout+result.stderr).decode(errors='replace')
    (OUT/f'ws-{name}.log').write_text(log)
    assert result.returncode == 0, (name,result.returncode,log[-2000:])
    assert 'self_heal_coverage=FULLY_STATIC dispatch_misses=0' in log, name
    assert 'effective=284x160' in log, name
    print(name, 'PASS', flush=True)
