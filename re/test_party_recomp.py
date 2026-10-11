"""Port the original PrimatesGBA party script to the native cable qualifier."""
import argparse, hashlib, json, os, re, subprocess, time
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
parser = argparse.ArgumentParser()
parser.add_argument('--script', type=Path, default=ROOT/'re/party_native_es.txt')
parser.add_argument('--stop', default='party_game')
parser.add_argument('--state-in', type=Path)
parser.add_argument('--from-title', action='store_true')
parser.add_argument('--start-after', help='Resume the old script after a shot marker')
parser.add_argument('--no-raster', action='store_true')
parser.add_argument('--timeout', type=float, default=120)
args = parser.parse_args()
out = ROOT/'re/run'/f'party-native-{time.time_ns()}'
out.mkdir(parents=True)
bits = dict(A=1, B=2, S=8, R=16, L=32, U=64, D=128)
tick = 0
rows = []
skip_to_title = args.from_title
skip_to_marker = args.start_after
reached_stop = False
for line in args.script.read_text().splitlines():
    fields = line.split()
    if not fields or line.lstrip().startswith('#'):
        continue
    if skip_to_marker:
        if fields==['shot',skip_to_marker]:
            skip_to_marker = None
        continue
    if skip_to_title:
        if fields[1:4] != ['S','S','S']:
            continue
        skip_to_title = False
    if fields[0]=='shot':
        if fields[1]==args.stop:
            reached_stop = True
            break
        continue
    if not fields[0].isdigit():
        continue
    duration = int(fields[0])
    buttons = [sum(bits[key] for key in item if key in bits) for item in fields[1:4]]
    if len(buttons)!=3:
        raise ValueError(line)
    rows.append(str(tick)+' '+' '.join(hex(value) for value in buttons))
    tick += duration
if skip_to_title or skip_to_marker or not reached_stop or not rows:
    parser.error('script did not contain the requested start/stop markers and input rows')
inputs = out/'inputs.txt'
inputs.write_text('\n'.join(rows)+'\n')
env = dict(os.environ, GBA_LINK_PROBE_PLAYERS='3', GBA_LINK_PROBE_SLICES='1',
           GBA_LINK_PROBE_INPUTS=str(inputs), GBA_LINK_PROBE_STATE_OUT=str(out/'party.state'),
           GBA_LINK_PROBE_CAPTURE_PREFIX=str(out/'screen'))
if args.state_in:
    env['GBA_LINK_PROBE_STATE_IN'] = str(args.state_in.resolve())
if args.no_raster:
    env['SS2_PROBE_NO_RASTER'] = '1'
with (out/'probe.log').open('wb') as log:
    run = subprocess.run([str(ROOT/'build/netplay-testing/SS2LinkProbe.exe'),
        str(ROOT/'roms/shining_soul_2_europe.gba'), str(ROOT/'build/gba_bios.bin'),
        str(tick), 'flash512'], env=env, cwd=out, stdout=log, stderr=log, timeout=args.timeout)
print(out, flush=True)
print('frames=', tick, 'exit=', run.returncode, flush=True)
log_text = (out/'probe.log').read_text(errors='replace')
print(log_text[-2000:])
result = dict(frames=tick, exit_code=run.returncode, script=str(args.script.resolve()),
              stop=args.stop, input_sha256=hashlib.sha256(inputs.read_bytes()).hexdigest(),
              no_raster=args.no_raster,
              performance=re.findall(r'simulation players=.*',log_text))
if (out/'party.state').exists():
    result['state_sha256'] = hashlib.sha256((out/'party.state').read_bytes()).hexdigest()
(out/'result.json').write_text(json.dumps(result,indent=2))
if run.returncode==0 and not args.no_raster:
    from PIL import Image
    montage = Image.new('RGB',(720,160))
    for seat in range(3):
        montage.paste(Image.open(out/f'screen{seat}.ppm'),(seat*240,0))
    montage.resize((1440,320),Image.Resampling.NEAREST).save(out/'screens.png')
raise SystemExit(run.returncode)
