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
p.add_argument('--open-menu', action='store_true')
p.add_argument('--pingpong', type=int, default=0, help='Reverse horizontal movement every N frames without releasing other keys')
p.add_argument('--capture-every', type=int, default=120)
p.add_argument('--native-view', action='store_true', help='Disable the main widescreen mod only inside the isolated test app')
p.add_argument('--width', type=int, default=284)
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
if a.native_view:
    (staged/'mods/state.toml').write_text('format_version = 1\n\n[[package]]\nid = "shining-soul-2.enhancement.adaptive-view"\nversion = "1.0.0"\n\n[[feature]]\npackage_id = "shining-soul-2.enhancement.adaptive-view"\nid = "adaptive-view"\nenabled = false\n')
replay = out/'input.txt'
replay.write_text(f'# gbarecomp-keyinput-v1\n0,0x{a.keys:04X}\n')
env = dict(os.environ, SDL_AUDIODRIVER='dummy', GBARECOMP_SS2_WS_TRACE='1',
           GBARECOMP_INPUT_REPLAY=str(replay), GBARECOMP_VIEW_WIDTH=str(a.width))
window_args=['--no-window']
if a.width != 284 and not a.native_view:
    env.update(SDL_VIDEODRIVER='dummy',GBARECOMP_FORCE_DRAWABLE=f'{a.width*4}x640')
    window_args=['--resize-view']
with (out/'runtime.log').open('wb') as log:
    process = subprocess.Popen([str(staged/exe.name), '--no-launcher', '--bios',
        str(ROOT/'build/gba_bios.bin'), '--rom', str(ROOT/'roms/shining_soul_2_europe.gba'),
        '--load-state', str(ROOT/f'roms/shining_soul_2_europe.state{a.slot}'),
        *window_args, '--volume', '0', '--screen', 'raw', '--view-width', str(a.width), '--tcp', '19935',
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
        if a.open_menu:
            cmd('run_frames',n=2,keyinput=0x3ff)
            cmd('run_frames',n=2,keyinput=0x3f7)
            cmd('run_frames',n=30,keyinput=0x3ff)
            cmd('run_frames',n=2,keyinput=0x3fe)
            cmd('run_frames',n=60,keyinput=0x3ff)
        rows=[]
        for frame in range(a.frames):
            keys=a.keys
            if a.pingpong:
                keys=(keys | 0x30) & ~(0x10 if (frame//a.pingpong)%2==0 else 0x20)
            cmd('run_frames',n=1,keyinput=keys)
            iw=bytes.fromhex(cmd('read_iwram',addr=0x3170,len=0xa0)['data'])
            io=bytes.fromhex(cmd('read_io',addr=0,len=0x60)['data'])
            shot=cmd('screenshot');rgb=bytes.fromhex(shot['data']);w=shot['w']
            expected_width=240 if a.native_view else a.width
            assert w == expected_width, f'Widescreen test accidentally running at {w}, expected {expected_width}'
            left=(w-240)//2
            native=b''.join(rgb[(y*w+left)*3:(y*w+left+240)*3] for y in range(160))
            camera=struct.unpack_from('<2h',iw,0x7c)
            maps=[struct.unpack_from('<I',iw,0x18+24*i+12)[0] for i in range(3)]
            # Black sidebars are detected below the HUD, not on the title letterbox.
            lit=sum(any(rgb[(y*w+x)*3:(y*w+x)*3+3])
                    for y in range(48,160) for x in list(range(left))+list(range(left+240,w)))
            row=dict(frame=frame,camera=camera,maps=maps,dispcnt=struct.unpack_from('<H',io)[0],
                     native_sha256=hashlib.sha256(native).hexdigest(),state_hash=cmd('state_hash'),
                     windows=struct.unpack_from('<6H',io,0x40),
                     scroll=[struct.unpack_from('<2H',io,0x14+4*i) for i in range(3)],margin_lit=lit)
            row['oam']=cmd('read_oam',addr=0,len=1024)['data']
            rows.append(row)
            if frame in (0,120,150,240):
                for region,size in [('iwram',32768),('ewram',262144),('vram',98304),('io',1024),('oam',1024),('pal',1024)]:
                    (out/f'{frame:04}-{region}.bin').write_bytes(bytes.fromhex(cmd('read_'+region,addr=0,len=size)['data']))
            if frame%a.capture_every==0 or (len(rows)>1 and (maps!=rows[-2]['maps'] or (lit==0)!=(rows[-2]['margin_lit']==0))):
                (out/f'frame-{frame:04}.ppm').write_bytes(f'P6\n{w} 160\n255\n'.encode()+rgb)
        cmd('quit');process.wait(timeout=15)
        (out/'frames.json').write_text(json.dumps(rows))
        print(out)
        print('frames',len(rows),'map_changes',sum(rows[i]['maps']!=rows[i-1]['maps'] for i in range(1,len(rows))),
              'black_margin_frames',sum(r['margin_lit']==0 for r in rows))
    finally:
        if process.poll() is None:process.terminate();process.wait(timeout=10)
