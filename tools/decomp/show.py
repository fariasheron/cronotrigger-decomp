#!/usr/bin/env python3
"""show.py <pc24> [...]: compact disassembly + live outputs + entry modes + profile count.
Profile from $DC_PROF (default /tmp/sw1/prof.txt)."""
import sys, os, collections
here = os.path.dirname(os.path.abspath(__file__)); sys.path.insert(0, here)
from fdis import walk, fmt, nm
import routines
root = os.path.dirname(os.path.dirname(here))
rom = open(f'{root}/ct.sfc', 'rb').read()
modes = collections.defaultdict(list)
for pc, m, x, d in routines.load(): modes[pc].append((m, x, d[0]))
live = {}
for l in open(f'{root}/src/decomp/live.inc'):
    if l.startswith('{'): a, b = l.strip('{},\n').split(','); live[int(a, 16)] = int(b, 16)
prof = collections.Counter()
pf = os.environ.get('DC_PROF', '/tmp/sw1/prof.txt')
if os.path.exists(pf):
    for l in open(pf): a, n = l.split(); prof[int(a, 16)] = int(n)
def lv(m): return ''.join(c for c, b in zip('AXYNVZC', (1, 2, 4, 8, 16, 32, 64)) if m & b) or '-'
for arg in sys.argv[1:]:
    pc = int(arg, 16)
    ms = modes.get(pc, [(1, 0, '?')])
    print(f'==== {pc:06X} modes={",".join(f"M{m}X{x}{d}" for m, x, d in ms)} live={lv(live.get(pc, 127))} prof={prof[pc]} {nm(pc)}')
    s, labels, calls = walk(rom, pc, ms[0][0], ms[0][1])
    prev = None
    for a in sorted(s):
        if prev is not None and a != prev: print('   ---')
        if a in labels: print(f'L{a & 0xFFFF:04X}:')
        e = s[a]; t = fmt(a, e)
        print(f'  {a & 0xFFFF:04X} ' + t[10:].split(';')[0].rstrip() + ('' if e[0] == ms[0][0] and e[1] == ms[0][1] else f'   ; m{e[0]}x{e[1]}'))
        prev = (a & 0xFF0000) | ((a + 1 + e[5]) & 0xFFFF)
