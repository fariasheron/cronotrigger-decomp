#!/usr/bin/env python3
"""evinj.py <outdir> <jobfile> [workers]  -- run injected field event scripts with the decomp check.
Each line of <jobfile> is one run:  name loc[:pokes] frames hex [hex ...]
The run boots the sweep save for loc (mksave.py over scen/w0.wram.bin, pokes from
scen/p_<name>.json), continues into the map, then writes the event bytes of the k-th hex
string to 7F:3F01 + 0x40 k and points the script position of object k (7E:1180 + 2 k,
relative to 7F:2001) at it, so object k runs it on its next turn (objects the room does not
use stay idle). Commands that end their object end with 00 (at priority 7 the object then
stays on it). As in the port's Mode 7 scene check (NOTAS.md, m7.py), "FF id" runs scene id
through C3:0000. It then waits <frames>, pressing A every 64 frames to close text windows,
with CT_DECOMP_CHECK=1, CT_PROF and audio off; output <outdir>/<name>.{dc,prof,log}, plus
start/end dumps in <outdir>/<name>.d."""
import sys, os, json, shutil, subprocess, threading, queue
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
from mksave import build
P = os.path.dirname(os.path.dirname(here)); B = os.environ.get('BUILD', f'{P}/build')
out = os.path.abspath(sys.argv[1]); W = int(sys.argv[3]) if len(sys.argv) > 3 else 2
os.makedirs(out, exist_ok=True)
base = open(f'{here}/scen/w0.wram.bin', 'rb').read()
def prep(w):
    d = f'/home/claude/ct/dk{w}'; os.makedirs(d + '/saves', exist_ok=True)
    for f in ('ChronoTriggerSNESRecomp', 'keybinds.ini', 'rom.cfg'): shutil.copy2(f'{B}/{f}', d)
    open(f'{d}/config.ini', 'w').write(open(f'{B}/config.ini').read().replace('DisableFrameDelay = 0', 'DisableFrameDelay = 1')
                                       .replace('EnableAudio = 1', 'EnableAudio = 0'))
    if not os.path.exists(f'{d}/assets'): shutil.copytree(f'{B}/assets', f'{d}/assets')
    return d
dirs = [prep(w) for w in range(W)]
def run(w, line):
    name, job, frames, *code = line.split()
    d = dirs[w]; loc, _, prof = job.partition(':')
    pokes = [(0x000100, int(loc, 16).to_bytes(2, 'little'))]
    if prof: pokes += [(int(a, 16), bytes.fromhex(h)) for a, h in json.load(open(f'{here}/scen/p_{prof}.json'))]
    for f in os.listdir(f'{d}/saves'): os.remove(f'{d}/saves/{f}')
    open(f'{d}/saves/save.srm', 'wb').write(build(base, None, 0, pokes))
    L = ['wait 400', 'press start 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 300',
         'dump start']
    ptr = b''
    for k, h in enumerate(code):
        off = 0x1F00 + 0x40 * k
        L.append('poke %06X %s' % (0x12001 + off, h))
        ptr += off.to_bytes(2, 'little')
    L.append('poke 1180 %s' % ptr.hex())
    for i in range(int(frames) // 64): L += ['wait 59', 'press a 5']
    L += ['dump end', 'quit']
    sp = f'{out}/{name}.script'; open(sp, 'w').write('\n'.join(L) + '\n')
    os.makedirs(f'{out}/{name}.d', exist_ok=True)
    env = dict(os.environ, CT_DECOMP_CHECK=os.environ.get('CHECK', '1'), CT_DECOMP_LOG=f'{out}/{name}.dc',
               CT_PROF=f'{out}/{name}.prof', SNESRECOMP_TURBO='1', SDL_VIDEODRIVER='offscreen',
               SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy', SNESRECOMP_DUMP_DIR=f'{out}/{name}.d')
    with open(f'{out}/{name}.log', 'w') as lf:
        try: rc = subprocess.run(['./ChronoTriggerSNESRecomp', '--no-launcher', '--script', sp, '/home/claude/ct.sfc'],
                                 cwd=d, env=env, stdout=lf, stderr=subprocess.STDOUT, timeout=600).returncode
        except subprocess.TimeoutExpired: rc = 'timeout'
    print(name, rc, flush=True)
lines = [l for l in open(sys.argv[2]).read().split('\n') if l.strip() and not l.startswith('#')]
q = queue.Queue(); [q.put(l) for l in lines]
def worker(w):
    while True:
        try: l = q.get_nowait()
        except queue.Empty: return
        run(w, l)
ts = [threading.Thread(target=worker, args=(w,)) for w in range(W)]; [t.start() for t in ts]; [t.join() for t in ts]
