#!/usr/bin/env python3
"""sweep.py <outdir> [workers]  -- decomp check across many places in the game.
Jobs: SWEEP_LIST="loc[:pokes] ..." (hex location; pokes = scen/p_<name>.json) or the
default set below. Each job builds a save that puts the party at that location
(mksave.py over scen/w0.wram.bin), boots, continues, then presses random buttons
(walk, talk, menu, battle commands) for SWEEP_N steps, with CT_DECOMP_CHECK=1 and
CT_PROF. Writes <outdir>/summary.txt: per routine calls/checked/bad/fail summed over
jobs, every difference line, and the union profile (prof.txt)."""
import sys, os, subprocess, random, shutil, json, threading, queue, collections, zlib
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
from mksave import build
P = os.path.dirname(os.path.dirname(here)); B = os.environ.get('BUILD', f'{P}/build')
out = os.path.abspath(sys.argv[1]); W = int(sys.argv[2]) if len(sys.argv) > 2 else 4
os.makedirs(out, exist_ok=True)
base = open(os.environ.get('SWEEP_BASE', f'{here}/scen/w0.wram.bin'), 'rb').read()
N = int(os.environ.get('SWEEP_N', '300'))
def prep(w):
    d = f'/home/claude/ct/dk{w}'; os.makedirs(d + '/saves', exist_ok=True)
    for f in ('ChronoTriggerSNESRecomp', 'keybinds.ini', 'rom.cfg'): shutil.copy2(f'{B}/{f}', d)
    open(f'{d}/config.ini', 'w').write(open(f'{B}/config.ini').read().replace('DisableFrameDelay = 0', 'DisableFrameDelay = 1')
                                       .replace('EnableAudio = 1', 'EnableAudio = 0'))
    if not os.path.exists(f'{d}/assets'): shutil.copytree(f'{B}/assets', f'{d}/assets')
    return d
dirs = [prep(w) for w in range(W)]
def script(seed):
    r = random.Random(seed)
    L = ['wait 400', 'press start 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 360']
    btns = os.environ.get('SWEEP_BTNS', 'up down left right up down left right a a a b x y start').split()
    for i in range(N):
        b = r.choice(btns)
        L += [f'press {b} {r.randint(4, 30)}', f'wait {r.randint(2, 40)}']
    L += ['wait 30', 'quit']
    return '\n'.join(L) + '\n'
def run(w, job):
    d = dirs[w]
    loc, _, prof = job.partition(':')
    pokes = [(0x000100, int(loc, 16).to_bytes(2, 'little'))]
    if prof: pokes += [(int(a, 16), bytes.fromhex(h)) for a, h in json.load(open(f'{here}/scen/p_{prof}.json'))]
    open(f'{d}/saves/save.srm', 'wb').write(build(base, None, 0, pokes))
    for f in os.listdir(f'{d}/saves'):
        if f.endswith('.sav'): os.remove(f'{d}/saves/{f}')
    tag = job.replace(':', '_'); sp = f'{out}/{tag}.script'
    open(sp, 'w').write(script(zlib.crc32(job.encode()) & 0xFFFF ^ int(os.environ.get('SWEEP_SEED', '0'))))
    env = dict(os.environ, CT_DECOMP_CHECK=os.environ.get('CHECK', '1'), CT_DECOMP_LOG=f'{out}/{tag}.dc',
               CT_PROF=f'{out}/{tag}.prof', SNESRECOMP_TURBO='1', SDL_VIDEODRIVER='offscreen',
               SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy')
    with open(f'{out}/{tag}.log', 'w') as lf:
        try: rc = subprocess.run(['./ChronoTriggerSNESRecomp', '--no-launcher', '--script', sp, '/home/claude/ct.sfc'],
                                 cwd=d, env=env, stdout=lf, stderr=subprocess.STDOUT,
                                 timeout=int(os.environ.get('SWEEP_TMO', '400'))).returncode
        except subprocess.TimeoutExpired: rc = 'timeout'
    return job, rc
DEFAULT = ['001', '002', '01B', '01E', '0A3', '140', '1F0', '1F3', '005', '006', '1B7:mid', '0A3:late',
           '140:late', '01E:mid', '1F2:mid', '0C1:late']
jobs = os.environ['SWEEP_LIST'].split() if os.environ.get('SWEEP_LIST') else DEFAULT
q = queue.Queue(); [q.put(j) for j in jobs]; res = []
def worker(w):
    while True:
        try: j = q.get_nowait()
        except queue.Empty: return
        res.append(run(w, j))
ts = [threading.Thread(target=worker, args=(w,)) for w in range(W)]; [t.start() for t in ts]; [t.join() for t in ts]
stat = collections.defaultdict(lambda: [0, 0, 0, 0]); diffs = []; prof = collections.Counter()
for j, rc in res:
    tag = j.replace(':', '_')
    if os.path.exists(f'{out}/{tag}.dc'):
        for l in open(f'{out}/{tag}.dc'):
            if l.startswith('S '):
                p = l.split(); s = stat[p[1]]
                for k in range(4): s[k] += int(p[2 + k])
            elif l[:2] in ('D ', 'F '): diffs.append(f'{tag}: {l.rstrip()}')
    if os.path.exists(f'{out}/{tag}.prof'):
        for l in open(f'{out}/{tag}.prof'):
            a, n = l.split(); prof[a] += int(n)
    for l in open(f'{out}/{tag}.log'):
        if '[decomp]' in l: diffs.append(f'{tag}: {l.rstrip()}')
with open(f'{out}/prof.txt', 'w') as f:
    for a, n in sorted(prof.items()): f.write(f'{a} {n}\n')
with open(f'{out}/summary.txt', 'w') as f:
    f.write('exit codes: ' + ' '.join(f'{j}={rc}' for j, rc in sorted(res)) + '\n')
    for pc, s in sorted(stat.items()): f.write(f'S {pc} calls={s[0]} checked={s[1]} bad={s[2]} fail={s[3]}\n')
    f.write('\n'.join(diffs) + '\n')
bad = sum(s[2] for s in stat.values())
print(f'jobs {len(res)} nonzero {[x for x in res if x[1] != 0]} routines {len(stat)} bad {bad} profiled {len(prof)}')
