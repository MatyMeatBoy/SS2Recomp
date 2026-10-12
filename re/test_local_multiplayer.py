"""Exercise the real launcher handoff, local cable and isolated SDL controllers."""
import json, os, re, subprocess, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
out = ROOT / 're/run' / f'local-multiplayer-{time.time_ns()}'
out.mkdir(parents=True)
results = []
for players, layout in [(1, 0), (2, 0), (3, 0), (4, 0), (4, 1)]:
    directory = out / f'{players}-{layout}'
    directory.mkdir()
    env = dict(os.environ, LNG_TEST_HIDDEN='1', SDL_AUDIODRIVER='dummy',
               SDL_RENDER_DRIVER='software', SS2_LOCAL_TEST='1',
               SS2_LOCAL_VIRTUAL_PADS='1', SS2_LOCAL_DIRECTORY=str(directory),
               LNG_SCRIPT=f'wait:25;local:{players},-1,{layout};wait:5;local_launch')
    env.pop('SDL_VIDEODRIVER', None)
    command = [str(ROOT/'build/netplay-testing/ShiningSoul2Recomp.exe'), '--launcher',
               '--bios', str(ROOT/'build/gba_bios.bin'), '--rom',
               str(ROOT/'roms/shining_soul_2_europe.gba'), '--volume', '0',
               '--frames', '120', '--save-path', str(directory/'unused.sav'), str(ROOT/'game.toml')]
    process = subprocess.run(command, cwd=ROOT, env=env, capture_output=True, timeout=60)
    text = process.stdout.decode(errors='replace') + process.stderr.decode(errors='replace')
    (directory/'output.log').write_text(text)
    assert process.returncode == 0, text[-4000:]
    assert f'local complete players={players} frames=120' in text, text[-4000:]
    rectangles = re.findall(r'local window seat=\d+ display=\d+ rect=(-?\d+),(-?\d+),(\d+),(\d+)', text)
    assert len(rectangles) == players, text
    rectangles = [tuple(map(int, r)) for r in rectangles]
    for i, (x, y, w, h) in enumerate(rectangles):
        assert w > 0 and h > 0
        for xx, yy, ww, hh in rectangles[:i]:
            assert x+w <= xx or xx+ww <= x or y+h <= yy or yy+hh <= y
    masks = re.findall(r'local input seat=(\d+) mask=(\d+) mapped=(\d+)', text)
    assert [int(mask) for _, mask, _ in masks] == [64, 16, 128, 32][:players], masks
    assert [int(mapped) for _, _, mapped in masks] == [1, 0, 1, 0][:players], masks
    for i in range(players):
        assert (directory/f'player-{i+1}/shining_soul_2.sav').stat().st_size == 65536
        assert (directory/f'player-{i+1}.bmp').stat().st_size > 1000
    results.append(dict(players=players, layout=layout, rectangles=rectangles, input_masks=masks))
    print(f'PASS local {players} players layout={layout}', flush=True)
(out/'result.json').write_text(json.dumps(results, indent=2))
print(out)
