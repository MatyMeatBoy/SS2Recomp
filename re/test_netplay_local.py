"""Four real processes over loopback; optional bidirectional WAN impairment."""
import argparse, hashlib, heapq, json, os, random, selectors, socket, subprocess, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
p=argparse.ArgumentParser()
p.add_argument('--mode', choices=['delay-sync','rollback'], default='delay-sync')
p.add_argument('--frames',type=int,default=120)
p.add_argument('--latency-ms',type=float,default=0)
p.add_argument('--jitter-ms',type=float,default=0)
p.add_argument('--loss',type=float,default=0)
p.add_argument('--timeout',type=float,default=50)
a=p.parse_args()
out=ROOT/'re/run'/f'net4-{a.mode}-{a.latency_ms:g}ms-{a.loss:g}loss-{time.time_ns()}'
out.mkdir(parents=True)
env=dict(os.environ, SDL_VIDEODRIVER='dummy',SDL_AUDIODRIVER='dummy',SDL_RENDER_DRIVER='software',
         GBARECOMP_SS2_RAM_TRACE='1')
sel=selectors.DefaultSelector(); queues=[]; sockets=[]; processes=[]; logs=[]
rng=random.Random(1379); serial=0; packets=0; dropped=0
host=('127.0.0.1',21940)
try:
    # One proxy endpoint per guest preserves each source identity at the hub.
    for seat in range(1,4):
        s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM)
        s.bind(('127.0.0.1',21950+seat)); s.setblocking(False)
        sel.register(s,selectors.EVENT_READ,('127.0.0.1',21940+seat));sockets.append(s)
    for seat in range(4):
        logfile=(out/f'peer{seat}.log').open('wb');logs.append(logfile)
        args=[str(ROOT/'build/netplay-testing/ShiningSoul2Recomp.exe'),'--no-launcher',
              '--bios',str(ROOT/'build/gba_bios.bin'),'--rom',str(ROOT/'roms/shining_soul_2_europe.gba'),
              '--volume','0','--scale','1','--frames',str(a.frames),
              '--save-path',str(out/f'peer{seat}.sav'),'--netplay-players','4',
              '--netplay-seat',str(seat),'--netplay-session','41011',
              '--netplay-bind',f'127.0.0.1:{21940+seat}','--netplay-'+a.mode,
              '--netplay-checkpoint',str(out/f'peer{seat}.paired'),str(ROOT/'game.toml')]
        if seat:args+=['--netplay-peer',f'127.0.0.1:{21950+seat}']
        processes.append(subprocess.Popen(args,cwd=out,env=env,stdout=logfile,stderr=logfile))
    start=time.monotonic()
    while any(q.poll() is None for q in processes) and time.monotonic()-start<a.timeout:
        for key,_ in sel.select(.002):
            s=key.fileobj
            try: data,source=s.recvfrom(65535)
            except (BlockingIOError, ConnectionResetError):continue
            packets+=1
            if rng.random()<a.loss: dropped+=1;continue
            target=key.data if source==host else host
            delay=max(0,a.latency_ms+rng.uniform(-a.jitter_ms,a.jitter_ms))/1000
            serial+=1;heapq.heappush(queues,(time.monotonic()+delay,serial,s,data,target))
        while queues and queues[0][0]<=time.monotonic():
            _,_,s,data,target=heapq.heappop(queues);s.sendto(data,target)
        if any(q.poll() not in (None,0) for q in processes):break
    result=dict(settings=vars(a),elapsed_seconds=round(time.monotonic()-start,2),
                packets=packets,dropped=dropped,exit_codes=[q.poll() for q in processes])
finally:
    for q in processes:
        if q.poll() is None:q.terminate()
    for q in processes:q.wait(timeout=10)
    for f in logs:f.close()
    for s in sockets:s.close()
    sel.close()
result['checkpoint_sha256']={f.name:hashlib.sha256(f.read_bytes()).hexdigest() for f in out.glob('*.paired')}
result['passed']=result['exit_codes']==[0]*4 and len(result['checkpoint_sha256'])==4 and len(set(result['checkpoint_sha256'].values()))==1
(out/'result.json').write_text(json.dumps(result,indent=2))
print(out);print(json.dumps(result,indent=2))
for seat in range(4):
    print(f'--- peer {seat} ---')
    print((out/f'peer{seat}.log').read_text(errors='replace')[-1800:])
raise SystemExit(0 if result['passed'] else 1)
