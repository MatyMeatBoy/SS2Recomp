import json, os, socket, subprocess, time
from pathlib import Path

root = Path(__file__).resolve().parents[1]
out = root / 're/run'
env = dict(os.environ, SDL_AUDIODRIVER='dummy', GBARECOMP_SS2_WS_TRACE='1')
with (out / 'ws4-tcp.log').open('wb') as log:
    proc = subprocess.Popen([str(root/'build/ShiningSoul2Recomp.exe'),
        '--no-launcher', '--bios', 'gba_bios.bin', '--rom', '../roms/shining_soul_2_europe.gba',
        '--load-state', '../roms/shining_soul_2_europe.state4', '--no-window', '--volume', '0',
        '--view-width', '284', '--tcp', '19931', '--save-path', '../re/run/ws4-test.sav',
        '../game.toml'], cwd=root/'build', env=env, stdout=log, stderr=log)
    try:
        for i in range(100):
            try:
                sock = socket.create_connection(('127.0.0.1',19931), timeout=2)
                break
            except OSError:
                time.sleep(.1)
        else:
            raise RuntimeError('Debug server did not start')
        sock.settimeout(30)
        stream = sock.makefile('rwb')
        def cmd(name, **kw):
            stream.write((json.dumps(dict(cmd=name, **kw))+'\n').encode()); stream.flush()
            result=json.loads(stream.readline())
            assert result.get('ok'), result
            return result
        cmd('savestate_load', path='../roms/shining_soul_2_europe.state4')
        for i in range(2): cmd('step')
        for name, size in [('iwram',32768),('ewram',262144),('vram',98304),('io',1024),('oam',1024)]:
            result=cmd('read_'+name,addr=0,len=size)
            (out/('ws4_'+name+'.bin')).write_bytes(bytes.fromhex(result['data']))
        (out/'ws4-ppu.json').write_text(json.dumps(cmd('ppu_state')))
        print(cmd('state_hash'))
        cmd('quit')
        proc.wait(timeout=10)
    finally:
        if proc.poll() is None:
            proc.terminate(); proc.wait(timeout=10)
