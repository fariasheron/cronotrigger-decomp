#!/usr/bin/env python3
"""inventory.py <src/gen> <out.json>
One record per generated body: pc24, variant, blocks, callees, I/O registers touched,
size (lines). Used to order the decomp work (leaves first) and to track progress."""
import sys, os, re, json, collections
d, outp = sys.argv[1], sys.argv[2]
HDR = re.compile(r'^RecompReturn (bank_([0-9A-F]{2})_([0-9A-F]{4})_(M[01]X[01]))\(CpuState \*cpu\) \{$')
LBL = re.compile(r'^  L_([0-9A-F]{4})_M[01]X[01]:')
CALL = re.compile(r'_r = (bank_[0-9A-F]{2}_[0-9A-F]{4})_M[01]X[01]\(cpu\)')
LLE = re.compile(r'interp_tier_run_call_frame\(cpu, 0x([0-9a-f]{6})u')
IO = re.compile(r'cpu_(read|write)(8|16)\(cpu, [^,]+, \(uint16\)\(0x(2[01][0-9a-f]{2}|4[23][0-9a-f]{2})\)')
recs = {}
for fn in sorted(os.listdir(d)):
    if not fn.endswith('.c'): continue
    cur = None
    for line in open(os.path.join(d, fn)):
        m = HDR.match(line)
        if m:
            name = m.group(1)
            cur = recs[name] = dict(name=name, pc=m.group(2)+m.group(3), var=m.group(4), file=fn,
                                    blocks=[], calls=set(), io=set(), lines=0,
                                    rts=False, rtl=False)
            continue
        if cur is None: continue
        if line.startswith('}'): cur = None; continue
        cur['lines'] += 1
        m = LBL.match(line)
        if m: cur['blocks'].append(m.group(1))
        for m in CALL.finditer(line): cur['calls'].add(m.group(1)[5:].replace('_', ''))
        for m in LLE.finditer(line): cur['calls'].add(m.group(1).upper())
        for m in IO.finditer(line): cur['io'].add(m.group(3))
        if 'RTS host return' in line: cur['rts'] = True
        if 'RTL host return' in line: cur['rtl'] = True
for r in recs.values():
    r['calls'] = sorted(r['calls'] - {r['pc']}); r['io'] = sorted(r['io'])
json.dump(sorted(recs.values(), key=lambda r: (r['pc'], r['var'])), open(outp, 'w'), indent=0)
pcs = {r['pc'] for r in recs.values()}
leaves = {r['pc'] for r in recs.values() if not r['calls']}
print(f"bodies {len(recs)} functions {len(pcs)} leaves {len(leaves)} io {len({r['pc'] for r in recs.values() if r['io']})}")
bybank = collections.Counter(p[:2] for p in pcs)
print(' '.join(f"{b}:{n}" for b, n in sorted(bybank.items())))
