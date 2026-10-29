#!/usr/bin/env python3
"""List JSR/JSL targets that pop their own return frame (PLA/PLX/.. before
any push) or write S (TCS/TXS) early: routines the native call ABI can't
represent. Output: force_lle lines."""
import sys
sys.path.insert(0, '/home/claude/ChronoTriggerSNESRecomp/tools/dev')
from dis65816 import OPS, SIZE
rom = open(sys.argv[1], 'rb').read()
PUSH = {'PHA','PHX','PHY','PHP','PHB','PHD','PEA','PEI','PHK','PER'}
PULL = {'PLA','PLX','PLY','PLP','PLB','PLD'}
targets = {}
for bank in range(0xC0, 0xE0):
    base = (bank & 0x3F) << 16
    for off in range(0x10000 - 3):
        o = base + off
        op = rom[o]
        if op == 0x20:   # JSR abs (same bank)
            t = (bank << 16) | rom[o+1] | rom[o+2] << 8
            targets.setdefault(t, 0); targets[t] += 1
        elif op == 0x22:
            t = rom[o+1] | rom[o+2] << 8 | rom[o+3] << 16
            if 0xC0 <= (t >> 16) <= 0xDF:
                targets.setdefault(t, 0); targets[t] += 1
def check(t, m, x):
    pc = t; depth = 0
    for _ in range(24):
        o = (pc & 0x3FFFFF)
        mn, mode = OPS[rom[o]]
        if mode == 'immm': n = 1 if m else 2
        elif mode == 'immx': n = 1 if x else 2
        else: n = SIZE.get(mode, 1)
        v = int.from_bytes(rom[o+1:o+1+n], 'little') if n else 0
        if mn in PUSH: depth += 1
        if mn in PULL:
            depth -= 1
            if depth < 0: return mn
        if mn in ('TCS', 'TXS'): return mn
        if mn in ('RTS', 'RTL', 'RTI', 'JMP', 'JML', 'BRA', 'BRL', 'BRK', 'STP'): return None
        if mn == 'REP': m = 0 if v & 0x20 else m; x = 0 if v & 0x10 else x
        if mn == 'SEP': m = 1 if v & 0x20 else m; x = 1 if v & 0x10 else x
        pc = (pc & 0xFF0000) | ((pc + 1 + n) & 0xFFFF)
    return None
# Only count targets that look like real call targets (called >= 2 times).
for t, n in sorted(targets.items()):
    if n < 2: continue
    hits = {check(t, m, x) for m in (0, 1) for x in (0, 1)} - {None}
    if hits:
        print('force_lle %06X   # %d call sites, pops/sets S early (%s)' % (t, n, ','.join(sorted(hits))))
