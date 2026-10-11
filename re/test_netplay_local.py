"""Two to four real processes over loopback; optional bidirectional WAN impairment."""
import argparse, hashlib, heapq, json, os, random, re, selectors, socket, subprocess, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--mode', choices=['delay-sync','rollback'], default='delay-sync')
p.add_argument('--players', type=int, choices=[2, 3, 4], default=4)
p.add_argument('--frames',type=int,default=120)
p.add_argument('--latency-ms',type=float,default=0)
p.add_argument('--jitter-ms',type=float,default=0)
p.add_argument('--loss',type=float,default=0)
p.add_argument('--timeout',type=float,default=50)
p.add_argument('--blackout-at',type=float,default=0,help='Seconds after launch to interrupt one guest link')
p.add_argument('--blackout-seconds',type=float,default=0)
p.add_argument('--blackout-seat',type=int,default=1)
p.add_argument('--probe-state',type=Path,help='Native whole-cable state for scripted gameplay qualification')
p.add_argument('--probe-inputs',type=Path,help='Relative tick and pressed-button words, one per seat')
p.add_argument('--input-delay',type=int,choices=range(2,21))
p.add_argument('--require-replay',action='store_true',help='Fail unless at least one peer resimulates ticks')
p.add_argument('--verbose',action='store_true')
p.add_argument('--safe',action='store_true',help='Confirmed-input profile with no established-session silence timeout')
a=p.parse_args()
if a.safe:
    a.mode='delay-sync'
if bool(a.probe_state) != bool(a.probe_inputs):
    p.error('--probe-state and --probe-inputs must be supplied together')
if not 1 <= a.blackout_seat < a.players:
    p.error('--blackout-seat must identify a guest')
out=ROOT/'re/run'/f'net{a.players}-{a.mode}-{a.latency_ms:g}ms-{a.loss:g}loss-{time.time_ns()}'
out.mkdir(parents=True)
env=dict(os.environ, SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software')
sel=selectors.DefaultSelector(); queues=[]; sockets=[]; processes=[]; logs=[]
rng=random.Random(1379); serial=0; packets=0; dropped=0
blackout_dropped=0
executable=ROOT/'build/netplay-testing/ShiningSoul2Recomp.exe'
if a.probe_state:
    executable=ROOT/'build/netplay-testing/SS2LinkProbe.exe'
host=('127.0.0.1',21940)
start=time.monotonic()
def blacked_out(sock, now):
    return (sock.getsockname()[1]==21950+a.blackout_seat and
            a.blackout_seconds>0 and a.blackout_at<=now-start<a.blackout_at+a.blackout_seconds)
try:
    # One proxy endpoint per guest preserves each source identity at the hub.
    for seat in range(1,a.players):
        s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        s.bind(('127.0.0.1',21950+seat)); s.setblocking(False)
        sel.register(s,selectors.EVENT_READ,('127.0.0.1',21940+seat));sockets.append(s)
    for seat in range(a.players):
        logfile=(out/f'peer{seat}.log').open('wb');logs.append(logfile)
        args=[str(executable),'--no-launcher',
              '--bios',str(ROOT/'build/gba_bios.bin'),'--rom',str(ROOT/'roms/shining_soul_2_europe.gba'),
              '--volume','0','--scale','1','--frames',str(a.frames),
              '--save-path',str(out/f'peer{seat}.sav'),'--netplay-players',str(a.players),
              '--netplay-seat',str(seat),'--netplay-session','41011',
              '--netplay-bind',f'127.0.0.1:{21940+seat}','--netplay-'+a.mode,
              '--netplay-checkpoint',str(out/f'peer{seat}.paired'),str(ROOT/'game.toml')]
        if a.safe:args+=['--netplay-safe']
        if seat:args+=['--netplay-peer',f'127.0.0.1:{21950+seat}']
        if a.input_delay is not None:args+=['--netplay-delay',str(a.input_delay)]
        peer_env=env
        if a.probe_state:
            peer_env=dict(env,GBA_LINK_PROBE_PLAYERS=str(a.players),GBA_LINK_PROBE_SLICES='1',
                GBA_LINK_PROBE_STATE_IN=str(a.probe_state.resolve()),
                GBA_LINK_PROBE_INPUTS=str(a.probe_inputs.resolve()),
                GBA_LINK_PROBE_STATE_OUT=str(out/f'peer{seat}.paired'),
                GBA_LINK_PROBE_CAPTURE_PREFIX=str(out/f'peer{seat}-screen'),
                GBA_LINK_PROBE_NET_SEAT=str(seat),GBA_LINK_PROBE_NET_SESSION='41011',
                GBA_LINK_PROBE_NET_BIND=f'127.0.0.1:{21940+seat}',
                GBA_LINK_PROBE_NET_ROLLBACK='1' if a.mode=='rollback' else '0',
                GBA_LINK_PROBE_NET_TIMEOUT_MS=str(int(a.timeout*1000)))
            if a.safe:peer_env['GBA_LINK_PROBE_NET_SAFE']='1'
            if seat or a.players==2:
                peer_env['GBA_LINK_PROBE_NET_PEER']=f'127.0.0.1:{21950+seat}' if seat else '127.0.0.1:21951'
            if a.input_delay is not None:
                peer_env['GBA_LINK_PROBE_NET_DELAY']=str(a.input_delay)
            args=[str(executable),str(ROOT/'roms/shining_soul_2_europe.gba'),
                  str(ROOT/'build/gba_bios.bin'),str(a.frames),'flash512']
        processes.append(subprocess.Popen(args,cwd=out,env=peer_env,stdout=logfile,stderr=logfile))
    start=time.monotonic()
    while any(q.poll() is None for q in processes) and time.monotonic()-start<a.timeout:
        for key,_ in sel.select(.002):
            s=key.fileobj
            try: data,source=s.recvfrom(65535)
            except (BlockingIOError, ConnectionResetError):continue
            packets+=1
            if blacked_out(s,time.monotonic()):blackout_dropped+=1;continue
            if rng.random()<a.loss: dropped+=1;continue
            target=key.data if source==host else host
            delay=max(0,a.latency_ms+rng.uniform(-a.jitter_ms,a.jitter_ms))/1000
            serial+=1;heapq.heappush(queues,(time.monotonic()+delay,serial,s,data,target))
        while queues and queues[0][0]<=time.monotonic():
            _,_,s,data,target=heapq.heappop(queues)
            if blacked_out(s,time.monotonic()):blackout_dropped+=1;continue
            s.sendto(data,target)
        if any(q.poll() not in (None,0) for q in processes):break
    result=dict(settings={k:str(v) if isinstance(v,Path) else v for k,v in vars(a).items()},elapsed_seconds=round(time.monotonic()-start,2),
                packets=packets,dropped=dropped,blackout_dropped=blackout_dropped,
                executable_sha256=hashlib.sha256(executable.read_bytes()).hexdigest(),
                exit_codes=[q.poll() for q in processes])
finally:
    for q in processes:
        if q.poll() is None:q.terminate()
    for q in processes:q.wait(timeout=10)
    for f in logs:f.close()
    for s in sockets:s.close()
    sel.close()
result['checkpoint_sha256']={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in out.glob('*.paired')}
result['replay_ticks']={f.name:[int(n) for n in re.findall(r'(?:netplay replay=|network agreed[^\n]*replay=)(\d+)',f.read_text(errors='replace'))] for f in out.glob('peer*.log')}
result['input_profile']='scripted pressed buttons' if a.probe_inputs else 'neutral (no controller input injected)'
if a.probe_inputs:
    result['input_sha256']=hashlib.sha256(a.probe_inputs.read_bytes()).hexdigest()
    result['initial_state_sha256']=hashlib.sha256(a.probe_state.read_bytes()).hexdigest()
result['replay_observed']=any(n>0 for counts in result['replay_ticks'].values() for n in counts)
result['resim_episodes']={f.name:len(re.findall(r'RESIM episode #',f.read_text(errors='replace'))) for f in out.glob('peer*.log')}
result['checkpoint_agreed']=result['exit_codes']==[0]*a.players and len(result['checkpoint_sha256'])==a.players and len(set(result['checkpoint_sha256'].values()))==1
result['passed']=result['checkpoint_agreed']
if a.require_replay:
    result['passed']=result['passed'] and result['replay_observed']
(out/'result.json').write_text(json.dumps(result,indent=2))
print(out)
print('PASS' if result['passed'] else 'FAIL', 'exits=',result['exit_codes'],
      'replay=',result['replay_ticks'],'episodes=',result['resim_episodes'])
if a.verbose:
    print(json.dumps(result,indent=2))
    for seat in range(a.players):
        print(f'--- peer {seat} ---')
        print((out/f'peer{seat}.log').read_text(errors='replace')[-1800:])
raise SystemExit(0 if result['passed'] else 1)
