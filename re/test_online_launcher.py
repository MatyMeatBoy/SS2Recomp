"""Two hidden launchers using stored login and a password-protected online room.
No authentication UI is driven. Private logs/credentials remain under re/run.
"""
import hashlib, json, os, shutil, subprocess, time
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
out=ROOT/'re/run'/f'online-launcher-{time.time_ns()}'
out.mkdir(parents=True)
name=f'SS2-QA-{int(time.time())}'
scripts=[
 f'wait:25;np_connect;wait:300;np_status;np_create:{name};wait:120;np_ready;wait:900;np_status;shot:{out.as_posix()}/host-lobby.png;np_start;wait:1200;np_status;quit',
 f'wait:25;np_connect;wait:600;np_list;wait:120;np_join:{name};wait:180;np_ready;wait:600;np_status;shot:{out.as_posix()}/guest-lobby.png;wait:1200;np_status;quit']
peers=[]; streams=[]
try:
 for seat,script in enumerate(scripts):
  directory=out/f'seat{seat}'
  directory.mkdir()
  exe=directory/'ShiningSoul2Recomp.exe'
  shutil.copy2(ROOT/'build/ShiningSoul2Recomp.exe',exe)
  shutil.copy2(ROOT/'build/SDL2.dll',directory/'SDL2.dll')
  shutil.copytree(ROOT/'build/assets',directory/'assets')
  secret=ROOT/'build/netplay_secret'
  if secret.exists():shutil.copy2(secret,directory/'netplay_secret')
  (directory/'rom.cfg').write_text(str(ROOT/'roms/shining_soul_2_europe.gba')+'\n')
  (directory/'bios.cfg').write_text(str(ROOT/'build/gba_bios.bin')+'\n')
  (directory/'config.ini').write_text('[Launcher]\nvolume=0\nfullscreen=0\nnetplay_view=1\n')
  env=dict(os.environ,LNG_TEST_HIDDEN='1',LNG_SCRIPT=script,SS2_RECOVERY_TEST='1',SDL_AUDIODRIVER='dummy')
  stream=(out/f'peer{seat}.log').open('wb');streams.append(stream)
  peers.append(subprocess.Popen([str(exe),'--launcher','--bios',str(ROOT/'build/gba_bios.bin'),
    '--rom',str(ROOT/'roms/shining_soul_2_europe.gba'),'--volume','0','--frames','120',
    '--save-path',str(directory/'owner.sav')],cwd=directory,env=env,stdout=stream,stderr=stream,
    creationflags=subprocess.CREATE_NO_WINDOW))
 deadline=time.monotonic()+100
 while any(p.poll() is None for p in peers) and time.monotonic()<deadline:time.sleep(.2)
 result={'exit_codes':[p.poll() for p in peers], 'executable_sha256':hashlib.sha256((ROOT/'build/ShiningSoul2Recomp.exe').read_bytes()).hexdigest()}
finally:
 for p in peers:
  if p.poll() is None:p.kill()
  p.wait()
 for f in streams:f.close()
result['handovers']=[]
for seat in range(2):
 trace=out/f'seat{seat}/netplay-last.log'
 result['handovers'].append(trace.read_text() if trace.exists() else '')
pairs=list(out.glob('seat*/netplay/*.paired'))
result['checkpoints']={p.parent.parent.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in pairs}
result['passed']=result['exit_codes']==[0,0] and len(pairs)==2 and len(set(result['checkpoints'].values()))==1
(out/'result.json').write_text(json.dumps(result,indent=2))
print(out)
print('PASS' if result['passed'] else 'FAIL: inspect private logs',result['exit_codes'])
raise SystemExit(0 if result['passed'] else 1)
