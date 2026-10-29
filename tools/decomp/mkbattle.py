#!/usr/bin/env python3
"""mkbattle.py <loc[:pokes]> <seed> [rundir]  -- battle savestates for the decomp check.
Builds the sweep save for the job (mksave.py over scen/w0.wram.bin, location poke at
7E:0100, pokes from scen/p_<name>.json), boots, continues, then presses random
buttons from random.Random(seed) (mostly A and walking) and takes a savestate every
900 frames from frame 1500 (slots 1-15). Each state is then loaded for 90 frames with
CT_PROF; the ones where bank C1 (battle) code runs are kept as
scen/battle_<loc>[_<pokes>]_<seed>_<slot>.sav (git-ignored, like the other states). Play one
with run.sh: STATE=tools/decomp/scen/battle_006_3_2.sav run.sh <script with
"loadstate 1"> <outdir>. Locations 006 and 01E have encounters near the spawn."""
import sys, os, json, random, shutil, subprocess
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
from mksave import build
P = os.path.dirname(os.path.dirname(here)); B = os.environ.get('BUILD', f'{P}/build')
job, seed = sys.argv[1], int(sys.argv[2])
R = sys.argv[3] if len(sys.argv) > 3 else '/home/claude/ct/run-battle'
loc, _, prof = job.partition(':')
os.makedirs(f'{R}/saves', exist_ok=True)
for f in ('ChronoTriggerSNESRecomp', 'keybinds.ini', 'rom.cfg'): shutil.copy2(f'{B}/{f}', R)
open(f'{R}/config.ini', 'w').write(open(f'{B}/config.ini').read().replace('DisableFrameDelay = 0', 'DisableFrameDelay = 1')
                                   .replace('EnableAudio = 1', 'EnableAudio = 0'))
if not os.path.exists(f'{R}/assets'): shutil.copytree(f'{B}/assets', f'{R}/assets')
def clear():
    for f in os.listdir(f'{R}/saves'): os.remove(f'{R}/saves/{f}')
pokes = [(0x000100, int(loc, 16).to_bytes(2, 'little'))]
if prof: pokes += [(int(a, 16), bytes.fromhex(h)) for a, h in json.load(open(f'{here}/scen/p_{prof}.json'))]
clear()
open(f'{R}/saves/save.srm', 'wb').write(build(open(f'{here}/scen/w0.wram.bin', 'rb').read(), None, 0, pokes))
r = random.Random(seed)
L = ['wait 400', 'press start 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 120', 'press a 5', 'wait 360']
f, k, nxt = 1645, 1, 1500
while k < 16:
    b = r.choice('a a a a b up down left right up down left right'.split())
    h, w = r.randint(4, 30), r.randint(2, 40)
    L += [f'press {b} {h}', f'wait {w}']; f += h + w
    if f >= nxt: L.append(f'savestate {k}'); k += 1; nxt += 900
L.append('quit')
env = dict(os.environ, SNESRECOMP_TURBO='1', SDL_VIDEODRIVER='offscreen', SDL_RENDER_DRIVER='software',
           SDL_AUDIODRIVER='dummy')
def run(lines, **e):
    open(f'{R}/run.script', 'w').write('\n'.join(lines) + '\n')
    subprocess.run(['./ChronoTriggerSNESRecomp', '--no-launcher', '--script', f'{R}/run.script', '/home/claude/ct.sfc'],
                   cwd=R, env=dict(env, **e), stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=900)
run(L)
states = {n: open(f'{R}/saves/save{n}.sav', 'rb').read() for n in range(1, 16) if os.path.exists(f'{R}/saves/save{n}.sav')}
kept = []
for n, s in sorted(states.items()):
    clear(); open(f'{R}/saves/save1.sav', 'wb').write(s)
    run(['wait 2', 'loadstate 1', 'wait 90', 'quit'], CT_PROF=f'{R}/probe.prof')
    c1 = sum(1 for l in open(f'{R}/probe.prof') if l.startswith('C1'))
    if c1:
        name = f"battle_{loc}{'_' + prof if prof else ''}_{seed}_{n}.sav"; open(f'{here}/scen/{name}', 'wb').write(s); kept.append(f'{name} (C1 {c1})')
print(f'{len(states)} states, kept: ' + ', '.join(kept))
