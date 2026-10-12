"""Recovery qualification: real TCP, abrupt processes, snapshot equality."""
import argparse, hashlib, json, os, re, selectors, socket, subprocess, time
from pathlib import Path

ROOT=Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--native',action='store_true')
p.add_argument('--predictive',action='store_true',help='Qualify the development streaming/prediction backend')
p.add_argument('--authoritative',action='store_true',help='Host ticks independently of peer input and reconnects peers individually')
p.add_argument('--launcher',action='store_true',help='Exercise the hidden experimental launcher and its supervisor')
p.add_argument('--executable',type=Path,help='Qualify an extracted release executable')
p.add_argument('--players',type=int,choices=[2,3,4],default=2)
p.add_argument('--fault',choices=['none','guest','host','network','desync','watchdog'],default='none')
p.add_argument('--frames',type=int,default=180)
p.add_argument('--at',type=int,default=60)
p.add_argument('--cut-seconds',type=float,default=12)
p.add_argument('--fault-seat',type=int,help='Interrupt only this guest; healthy peers connect directly')
p.add_argument('--delay-ms',type=float,default=0,help='Added one-way TCP proxy delay for input/recovery qualification')
p.add_argument('--initial-state',type=Path)
p.add_argument('--inputs',type=Path)
p.add_argument('--timeout',type=float,default=120)
a=p.parse_args()
if (a.predictive or a.authoritative) and not a.native:p.error('Development backend qualification requires --native')
if a.fault_seat is not None and (a.fault!='network' or not 1<=a.fault_seat<a.players):p.error('--fault-seat requires a network fault and a valid guest seat')
if a.fault=='watchdog' and not a.native:p.error('Watchdog qualification requires --native')
if a.launcher and (not a.native or a.fault not in ('none','desync','network','watchdog')):p.error('Launcher tests require native; abrupt parent termination is unsupported')
if a.fault!='none' and a.frames<=a.at+30:p.error('Fault tests need frames > at + 30')
if a.initial_state and a.players!=3:p.error('Current native class fixture has three consoles')
out=ROOT/'re/run'/f'recovery-{a.players}-{"native" if a.native else "protocol"}-{a.fault}-{time.time_ns()}'
out.mkdir(parents=True)
exe=ROOT/'build/netplay-testing'/('ShiningSoul2Recomp.exe' if a.native else 'SS2RecoveryProtocolTest.exe')
if a.executable:exe=a.executable.resolve()
port=22170; proxyport=22180
env=dict(os.environ,SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
if a.predictive:env.update(SS2_PREDICTIVE_ONLINE='1',SS2_ONLINE_LINK_TRACE='1')
if a.authoritative:env.update(SS2_AUTHORITATIVE_ONLINE='1',SS2_ONLINE_LINK_TRACE='1')
if a.authoritative:env['SS2_AUTHORITATIVE_PREDICTION']='1' if a.predictive else '0'
processes={};logs=[];commands={};peer_envs={};restart_count=0
selector=selectors.DefaultSelector();listener=None;pairs={};pending={};wire_bytes=0
blocked_until=0.;faulted=False;started=time.monotonic()
send_after={}
read_eof=set()
cut_progress=None
host_ticks_during_cut=0
healthy_progress={}
healthy_ticks_during_cut={}

def launch(seat):
    f=(out/f'peer{seat}.log').open('ab');logs.append(f)
    processes[seat]=subprocess.Popen(commands[seat],cwd=ROOT,env=peer_envs[seat],stdout=f,stderr=f)

def close_pair(s):
    other=pairs.pop(s,None)
    for q in (s,other):
        if q is None:continue
        pairs.pop(q,None);pending.pop(q,None)
        read_eof.discard(q);send_after.pop(q,None)
        try:selector.unregister(q)
        except (KeyError,ValueError):pass
        q.close()

def interest(s):
    events=(0 if s in read_eof else selectors.EVENT_READ) | (selectors.EVENT_WRITE if pending[s] else 0)
    try:selector.get_key(s)
    except KeyError:
        if events:selector.register(s,events)
    else:
        if events:selector.modify(s,events)
        else:selector.unregister(s)

def finish_direction(s):
    # FIN does not discard bytes already accepted by the proxy. In particular,
    # the final EndAck may be delayed after a guest exits normally.
    other=pairs[s]
    if not pending[s] and other in read_eof:
        try:s.shutdown(socket.SHUT_WR)
        except OSError:pass
    if s in read_eof and other in read_eof and not pending[s] and not pending[other]:
        close_pair(s)
    elif s in pairs:interest(s)

def progress(seat):
    text=(out/f'peer{seat}.log').read_text(errors='replace')
    worker=out/f'seat{seat}/game.stdout.log'
    if worker.exists():text+=worker.read_text(errors='replace')
    matches=re.findall(r'(?:recovery )?committed=(\d+)',text)
    return max(map(int,matches),default=0)

try:
    if a.fault=='network' or a.delay_ms:
        listener=socket.socket();listener.bind(('127.0.0.1',proxyport));listener.listen();listener.setblocking(False)
        selector.register(listener,selectors.EVENT_READ)
    for seat in range(a.players):
        directory=out/f'seat{seat}';directory.mkdir()
        use_proxy=listener and (a.fault_seat is None or seat==a.fault_seat)
        endpoint=f'127.0.0.1:{proxyport if use_proxy else port}'
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
            if a.launcher:
                commands[seat]=[str(exe),'--launcher','--bios',str(ROOT/'build/gba_bios.bin'),
                    '--rom',str(ROOT/'roms/shining_soul_2_europe.gba'),'--volume','0','--scale','1',
                    '--frames',str(a.frames),'--save-path',str(directory/'solo.sav'),str(ROOT/'game.toml')]
                peer_envs[seat].pop('SDL_VIDEODRIVER',None)
                peer_envs[seat]['LNG_TEST_HIDDEN']='1'
                uiport=proxyport if use_proxy and seat else port
                peer_envs[seat]['LNG_SCRIPT']=f'wait:25;experimental:{a.players},{seat},72111,{uiport},127.0.0.1;wait:5;experimental_launch'
            if a.fault=='desync' and seat==1:peer_envs[seat]['SS2_RECOVERY_CORRUPT_AT']=str(a.at)
            if a.fault=='watchdog' and seat==1:
                peer_envs[seat]['SS2_RECOVERY_TEST_CRASH_AT']=str(a.at)
                if not a.launcher:
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
                cut_progress=progress(0)
                healthy_progress={s:progress(s) for s in range(1,a.players) if a.fault_seat is not None and s!=a.fault_seat}
                for q in list(pairs):close_pair(q)
        if cut_progress is not None and time.monotonic()<blocked_until:
            host_ticks_during_cut=max(host_ticks_during_cut,progress(0)-cut_progress)
            for s,start in healthy_progress.items():
                healthy_ticks_during_cut[s]=max(healthy_ticks_during_cut.get(s,0),progress(s)-start)
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
                if mask&selectors.EVENT_WRITE and pending[s] and time.monotonic()>=send_after.get(s,0):
                    try:n=s.send(pending[s])
                    except BlockingIOError:n=0
                    except OSError:close_pair(s);continue
                    del pending[s][:n]
                    if not pending[s]:finish_direction(s)
                    if s not in pairs:continue
                if mask&selectors.EVENT_READ:
                    try:data=s.recv(65536)
                    except BlockingIOError:continue
                    except OSError:close_pair(s);continue
                    if not data:
                        read_eof.add(s);interest(s);finish_direction(pairs[s]);continue
                    wire_bytes+=len(data);other=pairs[s]
                    if not pending[other]:send_after[other]=time.monotonic()+a.delay_ms/1000
                    pending[other].extend(data);interest(other)
        codes=[processes[s].poll() for s in range(a.players)]
        if all(c is not None for c in codes):break
        if any(c not in (None,0) for c in codes):break
    result=dict(settings={k:str(v) if isinstance(v,Path) else v for k,v in vars(a).items()},
        elapsed=round(time.monotonic()-started,2),exit_codes=[processes[s].poll() for s in range(a.players)],
        fault_injected=faulted or a.fault in ('none','desync'),restarts=restart_count,proxy_bytes=wire_bytes,
        executable_sha256=hashlib.sha256(exe.read_bytes()).hexdigest())
    result['host_ticks_during_cut']=host_ticks_during_cut
    result['healthy_ticks_during_cut']=healthy_ticks_during_cut
finally:
    for process in processes.values():
        if process.poll() is None:
            if a.launcher:subprocess.run(['taskkill','/PID',str(process.pid),'/T','/F'],stdout=subprocess.DEVNULL,stderr=subprocess.DEVNULL,creationflags=subprocess.CREATE_NO_WINDOW)
            else:process.kill()
        process.wait(timeout=5)
    for f in logs:f.close()
    for s in list(pairs):close_pair(s)
    if listener:listener.close()
    selector.close()
result['state_sha256']={f.parent.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in out.glob('seat*/final.state')}
result['presentation_cadence']={}
for seat in range(a.players):
    matches=re.findall(r'online cadence samples=(\d+) mean_ms=([\d.]+) p95_ms=([\d.]+) max_ms=([\d.]+) gaps_over_33ms=(\d+)',(out/f'peer{seat}.log').read_text(errors='replace'))
    if matches:
        n,mean,p95,maximum,slow=matches[-1]
        result['presentation_cadence'][str(seat)]=dict(samples=int(n),mean_ms=float(mean),p95_ms=float(p95),max_ms=float(maximum),gaps_over_33ms=int(slow))
result['passed']=result['exit_codes']==[0]*a.players and result['fault_injected'] and len(result['state_sha256'])==a.players and len(set(result['state_sha256'].values()))==1
if a.authoritative and a.fault=='network':result['passed'] &= host_ticks_during_cut>=30
if a.authoritative and a.fault_seat is not None:
    result['passed'] &= len(healthy_ticks_during_cut)==a.players-2 and all(n>=30 for n in healthy_ticks_during_cut.values())
(out/'result.json').write_text(json.dumps(result,indent=2))
print(out);print('PASS' if result['passed'] else 'FAIL',result['exit_codes'],result['elapsed'])
if not result['passed']:
    for seat in range(a.players):print((out/f'peer{seat}.log').read_text(errors='replace')[-1200:])
raise SystemExit(0 if result['passed'] else 1)
