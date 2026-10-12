"""Windowless TCP trace of field transitions; isolated saves, no UI focus."""
import argparse, hashlib, json, os, shutil, socket, struct, subprocess, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p = argparse.ArgumentParser()
p.add_argument('--exe', type=Path, default=ROOT/'build/ShiningSoul2Recomp.exe')
p.add_argument('--frames', type=int, default=480)
p.add_argument('--keys', type=lambda x: int(x, 0), default=0x3ef)
p.add_argument('--slot', type=int, default=4)
p.add_argument('--label', default='baseline')
a = p.parse_args()
out = ROOT/'re/run'/f'transition-{a.label}-{time.time_ns()}'
out.mkdir(parents=True)
staged = out/'app'; staged.mkdir()
exe = a.exe.resolve()
shutil.copy2(exe, staged/exe.name)
for name in ('SDL2.dll',):
    shutil.copy2(exe.parent/name, staged/name)
for name in ('assets', 'mods'):
    if (exe.parent/name).exists():
        shutil.copytree(exe.parent/name,staged/name,ignore=shutil.ignore_patterns('state.toml'))
replay = out/'input.txt'
replay.write_text(f'# gbarecomp-keyinput-v1\n0,0x{a.keys:04X}\n')
env = dict(os.environ, SDL_AUDIODRIVER='dummy', GBARECOMP_SS2_WS_TRACE='1',
           GBARECOMP_INPUT_REPLAY=str(replay))
with (out/'runtime.log').open('wb') as log:
    process = subprocess.Popen([str(staged/exe.name), '--no-launcher', '--bios',
        str(ROOT/'build/gba_bios.bin'), '--rom', str(ROOT/'roms/shining_soul_2_europe.gba'),
        '--load-state', str(ROOT/f'roms/shining_soul_2_europe.state{a.slot}'),
        '--no-window', '--volume', '0', '--screen', 'raw', '--view-width', '284', '--tcp', '19935',
        '--save-path', str(out/'isolated.sav'), str(ROOT/'game.toml')],
        cwd=ROOT, env=env, stdout=log, stderr=log)
    try:
        for _ in range(100):
            try:
                sock=socket.create_connection(('127.0.0.1',19935), timeout=2)
                break
            except OSError: time.sleep(.1)
        else: raise RuntimeError('debug server unavailable')
        sock.settimeout(30)
        stream=sock.makefile('rwb')
        def cmd(name, **kw):
            stream.write((json.dumps(dict(cmd=name, **kw))+'\n').encode());stream.flush()
            r=json.loads(stream.readline())
            if not r.get('ok'): raise RuntimeError(r)
            return r
        cmd('savestate_load',path=str(ROOT/f'roms/shining_soul_2_europe.state{a.slot}'))
        rows=[]
        for frame in range(a.frames):
            cmd('run_frames',n=1,keyinput=a.keys)
            iw=bytes.fromhex(cmd('read_iwram',addr=0x3170,len=0xa0)['data'])
            io=bytes.fromhex(cmd('read_io',addr=0,len=0x60)['data'])
            shot=cmd('screenshot');rgb=bytes.fromhex(shot['data']);w=shot['w']
            assert w == 284, f'Widescreen test accidentally running at {w}'
            native=b''.join(rgb[(y*w+22)*3:(y*w+262)*3] for y in range(160))
            camera=struct.unpack_from('<2h',iw,0x7c)
            maps=[struct.unpack_from('<I',iw,0x18+24*i+12)[0] for i in range(3)]
            # Black sidebars are detected below the HUD, not on the title letterbox.
            lit=sum(any(rgb[(y*w+x)*3:(y*w+x)*3+3])
                    for y in range(48,160) for x in list(range(22))+list(range(w-22,w)))
            row=dict(frame=frame,camera=camera,maps=maps,dispcnt=struct.unpack_from('<H',io)[0],
                     native_sha256=hashlib.sha256(native).hexdigest(),state_hash=cmd('state_hash'),
                     windows=struct.unpack_from('<6H',io,0x40),
                     scroll=[struct.unpack_from('<2H',io,0x14+4*i) for i in range(3)],margin_lit=lit)
            rows.append(row)
            if frame%120==0 or (len(rows)>1 and (maps!=rows[-2]['maps'] or (lit==0)!=(rows[-2]['margin_lit']==0))):
                (out/f'frame-{frame:04}.ppm').write_bytes(f'P6\n{w} 160\n255\n'.encode()+rgb)
        cmd('quit');process.wait(timeout=15)
        (out/'frames.json').write_text(json.dumps(rows))
        print(out)
        print('frames',len(rows),'map_changes',sum(rows[i]['maps']!=rows[i-1]['maps'] for i in range(1,len(rows))),
              'black_margin_frames',sum(r['margin_lit']==0 for r in rows))
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=10)
