#!/usr/bin/env python3
"""Scan the functions the analyzer materialised (src/gen/program_manifest.json)
for routines that pop their own return frame before pushing anything
(callee-pops-args / return-address juggling). The native call ABI can't
represent those, so keep them on the interpreter tier via force_lle.
Rewrites the AUTO block in recomp/bank00.cfg; prints how many were added.
usage: auto_force_lle.py <project root> <rom.sfc>"""
import json, sys, re
sys.path.insert(0, '/home/claude/ChronoTriggerSNESRecomp/tools/dev')
from dis65816 import OPS, SIZE
root, rompath = sys.argv[1], sys.argv[2]
rom = open(rompath, 'rb').read()
PUSH = {'PHA','PHX','PHY','PHP','PHB','PHD','PEA','PEI','PHK','PER'}
PULL = {'PLA','PLX','PLY'}
def pops_return_frame(pc, m, x):
    depth = 0
    for _ in range(32):
        o = pc & 0x3FFFFF
        mn, mode = OPS[rom[o]]
        n = (1 if m else 2) if mode == 'immm' else (1 if x else 2) if mode == 'immx' else SIZE.get(mode, 1)
        v = int.from_bytes(rom[o+1:o+1+n], 'little') if n else 0
        if mn in PUSH: depth += 1
        if mn in PULL:
            depth -= 1
            if depth < 0: return True
        if mn in ('RTS','RTL','RTI','JMP','JML','BRA','BRL','BRK','STP','BEQ','BNE','BCC','BCS','BPL','BMI','BVC','BVS'):
            return False
        if mn == 'REP': m = 0 if v & 0x20 else m; x = 0 if v & 0x10 else x
        if mn == 'SEP': m = 1 if v & 0x20 else m; x = 1 if v & 0x10 else x
        pc = (pc & 0xFF0000) | ((pc + 1 + n) & 0xFFFF)
    return False
nodes = json.load(open(f'{root}/src/gen/program_manifest.json'))['nodes']
# Koei function-pointer call: push return-1 and target-1, then RTL into the
# target ("SEP; LDA #bank; PHA; REP; LDA #ret; PHA; LDA $05; PHA; SEP; LDA $04;
# PHA; REP; RTL"). Native code treats that RTL as the function's return and
# hands back an unbalanced stack, so the whole function stays interpreted.
RTL_CALL = re.compile(rb'\xE2\x20\xA9.\x48\xC2\x20\xA9..\x48\xA5\x05\x48\xE2\x20\xA5\x04\x48\xC2\x20\x6B', re.S)
rtl_calls = [0xC00000 + m.end() - 1 for m in RTL_CALL.finditer(rom)]
cfgp = f'{root}/recomp/bank00.cfg'
cfg = open(cfgp).read()
BEGIN, END = '# >>> BEGIN auto force_lle (tools/auto_force_lle.py)', '# <<< END auto force_lle'
existing = set(re.findall(r'^force_lle ([0-9A-Fa-f]{6})', cfg, re.M))
# Routines with a hand-written native body (hle_func) are handled already.
import glob
hle = set()
for f in glob.glob(f'{root}/recomp/*.cfg'):
    t = open(f).read()
    b = re.search(r'^bank = ([0-9A-Fa-f]+)', t, re.M)
    for pc16 in re.findall(r'^hle_func ([0-9A-Fa-f]+)', t, re.M):
        hle.add('%06X' % ((int(b.group(1), 16) << 16) | int(pc16, 16)))
new = set()
for k, v in nodes.items():
    key = v['key']; pc = key['pc24']
    if (pc >> 16) in (0x7E, 0x7F): continue
    if v['disposition'] != 'aot_eligible': continue
    if any(v['min_pc24'] <= r <= v['max_pc24'] for r in rtl_calls) and '%06X' % pc not in hle:
        new.add('%06X' % pc)
    if pops_return_frame(pc, key['m'], key['x']):
        if '%06X' % pc not in hle: new.add('%06X' % pc)
blk = re.search(re.escape(BEGIN) + r'.*?' + re.escape(END), cfg, re.S)
prev = set(re.findall(r'force_lle ([0-9A-F]{6})', blk.group(0))) if blk else set()
allauto = sorted(((prev | new) - (existing - prev)) - hle)
text = BEGIN + '\n' + ''.join(f'force_lle {a}\n' for a in allauto) + END
cfg = cfg.replace(blk.group(0), text) if blk else cfg.replace('# >>> BEGIN symbols.toml', text + '\n\n# >>> BEGIN symbols.toml', 1)
open(cfgp, 'w').write(cfg)
print(len(new - prev - existing), 'new;', len(allauto), 'auto total:', ' '.join(sorted(new - prev - existing)))
