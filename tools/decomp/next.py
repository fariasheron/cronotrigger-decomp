#!/usr/bin/env python3
"""next.py <inv.json> <bank> [n] [skip...]: undone routines of a bank whose
callees are all decompiled, smallest first (pc, lines, variants)."""
import json, re, glob, sys
inv = json.load(open(sys.argv[1]))
bank = sys.argv[2].upper(); n = int(sys.argv[3]) if len(sys.argv) > 3 else 30
skip = set(a.upper() for a in sys.argv[4:])
recs = inv if isinstance(inv, list) else list(inv.values())
done = set()
for f in glob.glob('src/decomp/*.c'):
    t = open(f).read()
    done |= set(re.findall(r'(?:DC_ENTRY|WALK_ENTRY|MAP_ROW3|LINE_JOB|LAYER_JOBS|SPLIT_JOBS|VM_CMD)\((\w{6})', t))
by = {}
for r in recs: by.setdefault(r['pc'], []).append(r)
rows = []
for pc, rs in by.items():
    if not pc.startswith(bank) or pc in done or pc in skip: continue
    cs = set().union(*[set(r['calls']) for r in rs])
    if any(c not in done and c != pc for c in cs): continue
    rows.append((max(r['lines'] for r in rs), pc, ','.join(r['var'] for r in rs)))
rows.sort()
left = sum(1 for pc in by if pc.startswith(bank) and pc not in done)
print(f'# {bank}: {left} left, {len(rows)} ready')
for r in rows[:n]: print(r[1], r[0], r[2])
