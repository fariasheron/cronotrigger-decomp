#!/usr/bin/env python3
"""mkram7e.py <in.sav> <wram.bin> <out.sav>
Fuzz state for the routines that run from WRAM (bank 7E): the code only
exists in WRAM in the mode that loads it (on the field those addresses hold
data), so both sides of the fuzz would run garbage from a field state. This
copies the bytes of every `ram_routine 7E....` in recomp/bank00.cfg into the
WRAM of a savestate. The savestate WRAM blob is located by matching the
WRAM image it was made from (tools/decomp/scen/w0.wram.bin for crono_room)."""
import sys, re, os
here = os.path.dirname(os.path.abspath(__file__))
s = bytearray(open(sys.argv[1], 'rb').read())
w = open(sys.argv[2], 'rb').read()
offs = {s.find(w[a:a + 64]) - a for a in (0x0100, 0x8000, 0xC000)}
assert len(offs) == 1 and min(offs) > 0, offs
base = offs.pop()
cfg = open(os.path.join(here, '..', '..', 'recomp', 'bank00.cfg')).read()
n = 0
for m in re.finditer(r'^ram_routine 7E([0-9A-F]{4}) \S+ ([0-9A-F]+)', cfg, re.M):
    a = int(m.group(1), 16); b = bytes.fromhex(m.group(2))
    s[base + a:base + a + len(b)] = b; n += 1
open(sys.argv[3], 'wb').write(s)
print(f'WRAM at {base:#x}, {n} routines poked')
