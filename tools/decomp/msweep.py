#!/usr/bin/env python3
"""msweep.py <outdir> <workers> <kind> <src> [src ...]  -- scripted-input decomp check.
kind battle: each src is a savestate (e.g. scen/battle_*.sav from mkbattle.py), loaded from
  slot 1; then MS_N random battle menu macros: Attack (A, A), Tech (down, A, a few moves
  in the tech list, A, a target move, A), Item (down, down, A, moves, A, A) or a wait
  for the gauges. Double/triple techs show up in the tech list once two or three members
  are ready, so the late-pokes states reach them.
kind menu: each src is loc[:pokes] (the sweep save, mksave.py); after continuing, MS_N
  macros open the main menu (X), go right 0-6 icons, A, then 4-12 random moves/A/B
  presses inside, then B x4 back to the field.
Seed: MS_SEED (default 0) + the index of the src. Runs with CT_DECOMP_CHECK=1, CT_PROF and
audio off in /home/claude/ct/dk<w>; output <outdir>/<tag>.{dc,prof,log,script}."""
import sys, os, json, random, shutil, subprocess, threading, queue
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
from mksave import build
P = os.path.dirname(os.path.dirname(here)); B = os.environ.get('BUILD', f'{P}/build')
out = os.path.abspath(sys.argv[1]); W = int(sys.argv[2]); kind = sys.argv[3]; srcs = sys.argv[4:]
os.makedirs(out, exist_ok=True)
N = int(os.environ.get('MS_N', '120')); seed0 = int(os.environ.get('MS_SEED', '0'))
base = open(f'{here}/scen/w0.wram.bin', 'rb').read()
def prep(w):
    d = f'/home/claude/ct/dk{w}'; os.makedirs(d + '/saves', exist_ok=True)
    for f in ('ChronoTriggerSNESRecomp', 'keybinds.ini', 'rom.cfg'): shutil.copy2(f'{B}/{f}', d)
    open(f'{d}/config.ini', 'w').write(open(f'{B}/config.ini').read().replace('DisableFrameDelay = 0', 'DisableFrameDelay = 1')
                                       .replace('EnableAudio = 1', 'EnableAudio = 0'))
    if not os.path.exists(f'{d}/assets'): shutil.copytree(f'{B}/assets', f'{d}/assets')
    return d
dirs = [prep(w) for w in range(W)]
def p(L, b, h=5, w=12): L += [f'press {b} {h}', f'wait {w}']
def moves(L, r, n):
    for _ in range(n): p(L, r.choice(['up', 'down', 'left', 'right']), 4, 8)
def battle(r):
    L = ['wait 2', 'loadstate 1', 'wait 30']
    for _ in range(N):
        m = r.choice(['attack', 'tech', 'tech', 'item', 'wait'])
        if m == 'attack': p(L, 'a'); p(L, 'a', 5, 40)
        elif m == 'tech': p(L, 'down'); p(L, 'a', 5, 20); moves(L, r, r.randint(0, 4)); p(L, 'a', 5, 15); moves(L, r, r.randint(0, 2)); p(L, 'a', 5, 60)
        elif m == 'item': p(L, 'down'); p(L, 'down'); p(L, 'a', 5, 20); moves(L, r, r.randint(0, 5)); p(L, 'a', 5, 15); p(L, 'a', 5, 60)
        else: L.append(f'wait {r.randint(30, 120)}')
    return L
def menu(r):
    L = ['wait 400', 'press start 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 360']
    for _ in range(N):
        p(L, 'x', 5, 50)
        for _ in range(r.randint(0, 6)): p(L, 'right', 4, 10)
        p(L, 'a', 5, 30)
        for _ in range(r.randint(4, 12)): p(L, r.choice(['up', 'down', 'left', 'right', 'a', 'a', 'b', 'y']), 4, 14)
        for _ in range(4): p(L, 'b', 5, 20)
    return L
def run(w, i, src):
    d = dirs[w]
    for f in os.listdir(f'{d}/saves'): os.remove(f'{d}/saves/{f}')
    r = random.Random(seed0 * 1000 + i)
    if kind == 'battle':
        tag = os.path.basename(src)[:-4]; shutil.copy(src, f'{d}/saves/save1.sav'); L = battle(r)
    else:
        tag = 'menu_' + src.replace(':', '_'); loc, _, prof = src.partition(':')
        pokes = [(0x000100, int(loc, 16).to_bytes(2, 'little'))]
        if prof: pokes += [(int(a, 16), bytes.fromhex(h)) for a, h in json.load(open(f'{here}/scen/p_{prof}.json'))]
        open(f'{d}/saves/save.srm', 'wb').write(build(base, None, 0, pokes)); L = menu(r)
    sp = f'{out}/{tag}.script'; open(sp, 'w').write('\n'.join(L + ['quit']) + '\n')
    env = dict(os.environ, CT_DECOMP_CHECK='1', CT_DECOMP_LOG=f'{out}/{tag}.dc', CT_PROF=f'{out}/{tag}.prof',
               SNESRECOMP_TURBO='1', SDL_VIDEODRIVER='offscreen', SDL_RENDER_DRIVER='software', SDL_AUDIODRIVER='dummy')
    with open(f'{out}/{tag}.log', 'w') as lf:
        try: rc = subprocess.run(['./ChronoTriggerSNESRecomp', '--no-launcher', '--script', sp, '/home/claude/ct.sfc'],
                                 cwd=d, env=env, stdout=lf, stderr=subprocess.STDOUT, timeout=1200).returncode
        except subprocess.TimeoutExpired: rc = 'timeout'
    print(tag, rc, flush=True)
q = queue.Queue(); [q.put((i, s)) for i, s in enumerate(srcs)]
def worker(w):
    while True:
        try: i, s = q.get_nowait()
        except queue.Empty: return
        run(w, i, s)
ts = [threading.Thread(target=worker, args=(w,)) for w in range(W)]; [t.start() for t in ts]; [t.join() for t in ts]
