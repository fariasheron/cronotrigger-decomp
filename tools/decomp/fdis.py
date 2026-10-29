#!/usr/bin/env python3
"""fdis.py <rom> <pc24> [m x]  -- disassemble one routine by following its control flow.
Branch targets get labels, JSR/JSL targets show names from tools/decomp/names.txt
("pc24 name" per line). M/X widths are tracked along each path (REP/SEP; PLP keeps the
state of the matching PHP when it is on the same path)."""
import sys, os
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'dev'))
from dis65816 import OPS, FMT, SIZE, rom_off
here = os.path.dirname(os.path.abspath(__file__))
NAMES = {}
if os.path.exists(f'{here}/names.txt'):
    for l in open(f'{here}/names.txt'):
        p = l.split()
        if len(p) >= 2 and not l.startswith('#'): NAMES[int(p[0], 16)] = p[1]
def nm(a): return NAMES.get(a, '')
def decode(rom, pc, m, x):
    o = rom_off(pc); op = rom[o]; mn, mode = OPS[op]
    if mode == 'immm': n = 1 if m else 2
    elif mode == 'immx': n = 1 if x else 2
    else: n = SIZE.get(mode, 1)
    b = rom[o+1:o+1+n]; v = int.from_bytes(b, 'little') if n else 0
    return op, mn, mode, n, b, v
def walk(rom, start, m, x, maxn=4000):
    seen = {}; work = [(start, m, x, ())]; labels = set(); calls = []
    while work and len(seen) < maxn:
        pc, m, x, php = work.pop()
        while True:
            if pc in seen: break
            op, mn, mode, n, b, v = decode(rom, pc, m, x)
            seen[pc] = (m, x, op, mn, mode, n, b, v)
            nxt = (pc & 0xFF0000) | ((pc + 1 + n) & 0xFFFF)
            if mn == 'REP': m = 0 if v & 0x20 else m; x = 0 if v & 0x10 else x
            if mn == 'SEP': m = 1 if v & 0x20 else m; x = 1 if v & 0x10 else x
            if mn == 'PHP': php = php + ((m, x),)
            if mn == 'PLP' and php: m, x = php[-1]; php = php[:-1]
            if mode in ('rel', 'rell'):
                d = v - 256 if mode == 'rel' and v > 127 else (v - 65536 if mode == 'rell' and v > 32767 else v)
                t = (pc & 0xFF0000) | ((nxt + d) & 0xFFFF)
                labels.add(t); work.append((t, m, x, php))
                if mn in ('BRA', 'BRL'): break
            if mn == 'JMP' and mode == 'abs':
                t = (pc & 0xFF0000) | v; labels.add(t); work.append((t, m, x, php)); break
            if mn in ('JSR', 'JSL') and mode in ('abs', 'long'):
                calls.append((pc & 0xFF0000) | v if mode == 'abs' else v)
            if mn in ('RTS', 'RTL', 'RTI', 'JML', 'STP', 'BRK') or (mn == 'JMP') or (mn == 'JSR' and mode == 'absxi' and False):
                break
            pc = nxt
    return seen, labels, calls
def fmt(pc, e):
    m, x, op, mn, mode, n, b, v = e
    if mode in ('immm', 'immx', 'imm8'): arg = '#$%0*X' % (2 * n, v)
    elif mode == 'rel': arg = 'L%04X' % (((pc & 0xFFFF) + 2 + (v - 256 if v > 127 else v)) & 0xFFFF)
    elif mode == 'rell': arg = 'L%04X' % (((pc & 0xFFFF) + 3 + (v - 65536 if v > 32767 else v)) & 0xFFFF)
    elif mode == 'mv': arg = '$%02X,$%02X' % (b[1], b[0])
    else: arg = FMT[mode].format(v) if FMT[mode] else ''
    if mn in ('JSR', 'JMP') and mode == 'abs':
        t = (pc & 0xFF0000) | v; arg += ('  ; ' + nm(t)) if nm(t) else ''
    if mn in ('JSL', 'JML') and mode == 'long':
        arg += ('  ; ' + nm(v)) if nm(v) else ''
    return '%-9s %-4s %-22s ; m%d x%d' % (' '.join('%02X' % c for c in [op] + list(b)), mn, arg, m, x)
if __name__ == '__main__':
    rom = open(sys.argv[1], 'rb').read()
    pc = int(sys.argv[2], 16)
    m = int(sys.argv[3]) if len(sys.argv) > 3 else 1
    x = int(sys.argv[4]) if len(sys.argv) > 4 else 0
    seen, labels, calls = walk(rom, pc, m, x)
    print(f'; {pc:06X} {nm(pc)}')
    prev = None
    for a in sorted(seen):
        if prev is not None and a != prev: print('        ; ---')
        if a in labels: print(f'L{a & 0xFFFF:04X}:')
        e = seen[a]
        print(f'  {a>>16:02X}:{a&0xFFFF:04X}  ' + fmt(a, e))
        prev = (a & 0xFF0000) | ((a + 1 + e[5]) & 0xFFFF)
