"""Recovery qualification: real TCP, abrupt processes, snapshot equality."""
import argparse, hashlib, json, os, re, selectors, socket, subprocess, time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--native',action='store_true')
p.add_argument('--players',type=int,choices=[2,3,4],default=2)
p.add_argument('--fault',choices=['none','guest','host','network','desync','watchdog'],default='none')
p.add_argument('--frames',type=int,default=180)
p.add_argument('--at',type=int,default=60)
p.add_argument('--cut-seconds',type=float,default=12)
p.add_argument('--initial-state',type=Path)
p.add_argument('--inputs',type=Path)
p.add_argument('--timeout',type=float,default=120)
a=p.parse_args()
if a.fault=='watchdog' and not a.native:p.error('Watchdog qualification requires --native')
if a.fault!='none' and a.frames<=a.at+30:p.error('Fault tests need frames > at + 30')
if a.initial_state and a.players!=3:p.error('Current native class fixture has three consoles')
out=ROOT/'re/run'/f'recovery-{a.players}-{"native" if a.native else "protocol"}-{a.fault}-{time.time_ns()}'
out.mkdir(parents=True)
exe=ROOT/'build/netplay-testing'/('ShiningSoul2Recomp.exe' if a.native else 'SS2RecoveryProtocolTest.exe')
port=22170; proxyport=22180
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
processes={};logs=[];commands={};peer_envs={};restart_count=0
selector=selectors.DefaultSelector();listener=None;pairs={};pending={};wire_bytes=0
blocked_until=0.;faulted=False;started=time.monotonic()

def launch(seat):
    f=(out/f'peer{seat}.log').open('ab');logs.append(f)
    processes[seat]=subprocess.Popen(commands[seat],cwd=ROOT,env=peer_envs[seat],stdout=f,stderr=f)

def close_pair(s):
    other=pairs.pop(s,None)
    for q in (s,other):
        if q is None:continue
        pairs.pop(q,None);pending.pop(q,None)
        try:selector.unregister(q)
        except (KeyError,ValueError):pass
        q.close()

def progress(seat):
    text=(out/f'peer{seat}.log').read_text(errors='replace')
    matches=re.findall(r'(?:recovery )?committed=(\d+)',text)
    return max(map(int,matches),default=0)

try:
    if a.fault=='network':
        listener=socket.socket();listener.bind(('127.0.0.1',proxyport));listener.listen();listener.setblocking(False)
        selector.register(listener,selectors.EVENT_READ)
    for seat in range(a.players):
        directory=out/f'seat{seat}';directory.mkdir()
        endpoint=f'127.0.0.1:{proxyport if a.fault=="network" else port}'
        if a.native:
            commands[seat]=[str(exe),'--no-launcher','--bios',str(ROOT/'build/gba_bios.bin'),
                '--rom',str(ROOT/'roms/shining_soul_2_europe.gba'),'--volume','0','--scale','1',
                '--screen','raw','--frames',str(a.frames),'--save-path',str(directory/'solo.sav'),
                '--netplay-safe','--netplay-players',str(a.players),'--netplay-seat',str(seat),
                '--netplay-session','72111','--netplay-bind',f'127.0.0.1:{port+seat}',
                *(['--netplay-peer',endpoint] if seat else []),str(ROOT/'game.toml')]
            peer_envs[seat]=dict(env,SS2_RECOVERY_TEST='1',SS2_RECOVERY_DIRECTORY=str(directory),
                SS2_RECOVERY_STATE_OUT=str(directory/'final.state'))
            if a.initial_state:peer_envs[seat]['SS2_RECOVERY_INITIAL_STATE']=str(a.initial_state.resolve())
            if a.inputs:peer_envs[seat]['SS2_RECOVERY_INPUTS']=str(a.inputs.resolve())
            if a.fault=='desync' and seat==1:peer_envs[seat]['SS2_RECOVERY_CORRUPT_AT']=str(a.at)
            if a.fault=='watchdog' and seat==1:
                peer_envs[seat]['SS2_RECOVERY_TEST_CRASH_AT']=str(a.at)
                # The script uses the root build binary, as the distributed BAT does.
                commands[seat]=['powershell.exe','-NoProfile','-ExecutionPolicy','Bypass','-File',str(ROOT/'tools/run_recoverable.ps1'),*commands[seat][1:]]
        else:
            # The host binds the real port; guests dial a proxy for network faults.
            commands[seat]=[str(exe),str(seat),str(a.players),str(port if seat==0 else (proxyport if a.fault=='network' else port)),str(a.frames),str(directory),str(a.at if a.fault=='desync' and seat==1 else 0)]
            peer_envs[seat]=env
        launch(seat)
    while time.monotonic()-started<a.timeout:
        if a.fault=='watchdog' and (out/'seat1/qualification.crashed').exists():faulted=True
        if not faulted and a.fault not in ('none','desync','watchdog') and progress(0)>=a.at:
            faulted=True
            if a.fault in ('guest','host'):
                seat=1 if a.fault=='guest' else 0
                processes[seat].kill();processes[seat].wait(timeout=5)
                time.sleep(.3);launch(seat);restart_count+=1
            else:
                blocked_until=time.monotonic()+a.cut_seconds
                for q in list(pairs):close_pair(q)
        if not selector.get_map():time.sleep(.002)
        for key,mask in (selector.select(.002) if selector.get_map() else []):
            s=key.fileobj
            if s==listener:
                try:client,_=listener.accept()
                except BlockingIOError:continue
                if time.monotonic()<blocked_until:client.close();continue
                upstream=socket.socket();upstream.settimeout(1)
                try:upstream.connect(('127.0.0.1',port))
                except OSError:client.close();upstream.close();continue
                for q in (client,upstream):q.setblocking(False);selector.register(q,selectors.EVENT_READ);pending[q]=bytearray()
                pairs[client]=upstream;pairs[upstream]=client
            else:
                if s not in pairs:continue
                if mask&selectors.EVENT_WRITE and pending[s]:
                    try:n=s.send(pending[s])
                    except BlockingIOError:n=0
                    except OSError:close_pair(s);continue
                    del pending[s][:n]
                    if not pending[s]:selector.modify(s,selectors.EVENT_READ)
                if mask&selectors.EVENT_READ:
                    try:data=s.recv(65536)
                    except BlockingIOError:continue
                    except OSError:close_pair(s);continue
                    if not data:close_pair(s);continue
                    wire_bytes+=len(data);other=pairs[s];pending[other].extend(data);selector.modify(other,selectors.EVENT_READ|selectors.EVENT_WRITE)
        codes=[processes[s].poll() for s in range(a.players)]
        if all(c is not None for c in codes):break
        if any(c not in (None,0) for c in codes):break
    result=dict(settings={k:str(v) if isinstance(v,Path) else v for k,v in vars(a).items()},
        elapsed=round(time.monotonic()-started,2),exit_codes=[processes[s].poll() for s in range(a.players)],
        fault_injected=faulted or a.fault in ('none','desync'),restarts=restart_count,proxy_bytes=wire_bytes,
        executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
finally:
    for process in processes.values():
        if process.poll() is None:process.kill()
        process.wait(timeout=5)
    for f in logs:f.close()
    for s in list(pairs):close_pair(s)
    if listener:listener.close()
    selector.close()
result['state_sha256']={f.parent.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in out.glob('seat*/final.state')}
result['passed']=result['exit_codes']==[0]*a.players and result['fault_injected'] and len(result['state_sha256'])==a.players and len(set(result['state_sha256'].values()))==1
(out/'result.json').write_text(json.dumps(result,indent=2))
print(out);print('PASS' if result['passed'] else 'FAIL',result['exit_codes'],result['elapsed'])
if not result['passed']:
    for seat in range(a.players):print((out/f'peer{seat}.log').read_text(errors='replace')[-1200:])
raise SystemExit(0 if result['passed'] else 1)
