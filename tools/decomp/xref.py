#!/usr/bin/env python3
"""xref.py <rom> <inventory.json> <out.txt>
Disassemble every known routine (fdis.walk) and write one line per instruction:
"routine pc24 m x mnemonic operand". grep this file for RAM addresses
(e.g. 'STA \\$1801') to see who reads and writes them."""
import sys, os, json
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fdis import walk, fmt
rom = open(sys.argv[1], 'rb').read()
inv = json.load(open(sys.argv[2]))
seen_pc = set(); out = open(sys.argv[3], 'w')
import routines as _rt
for pc, m, x, disp in _rt.load():
    if (pc >> 16) < 0xC0: continue
    r = {'pc': f'{pc:06X}'}
    if (pc, m, x) in seen_pc: continue
    seen_pc.add((pc, m, x))
    s, labels, calls = walk(rom, pc, m, x)
    for a in sorted(s):
        out.write(f"{r['pc']} {a:06X} " + fmt(a, s[a]).split(';')[0][10:].rstrip() + f" m{s[a][0]}x{s[a][1]}\n")
