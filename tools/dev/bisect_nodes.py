"""Find native nodes whose forcing to LLE fixes the new-game flow.
usage: bisect_nodes.py profile.json nodes.json"""
import json, subprocess, sys, re
P = '/home/claude/ChronoTriggerSNESRecomp'
prof, nodes = sys.argv[1], json.load(open(sys.argv[2]))
CFG = f'{P}/recomp/bank00.cfg'
orig = open(CFG).read()
B, E = '# >>> BEGIN bisect force_lle', '# <<< END bisect force_lle'
def ok(forced):
    blk = B + '\n' + ''.join('force_lle %06X\n' % n for n in forced) + E + '\n'
    open(CFG, 'w').write(orig.replace('# >>> BEGIN symbols.toml', blk + '\n# >>> BEGIN symbols.toml', 1))
    cs, town = map(int, subprocess.check_output(['bash', f'{P}/tools/dev/game_try.sh', prof]).split())
    good = cs >= 29 and town >= 40
    print(len(forced), 'forced ->', cs, town, 'ok' if good else 'BAD', flush=True)
    return good
known = []
try:
    for rnd in range(6):
        if ok(known): print('FIXED by', ['%06X' % k for k in known], flush=True); break
        rest = [n for n in nodes if n not in known]
        if not ok(known + rest): print('NOT FIXABLE by these nodes', flush=True); break
        lo, hi = 0, len(rest)
        while hi - lo > 1:
            mid = (lo + hi) // 2
            if ok(known + rest[:mid]): hi = mid
            else: lo = mid
        c = rest[hi - 1]; print('CULPRIT %06X' % c, flush=True); known.append(c)
finally:
    open(CFG, 'w').write(orig)
print('FINAL', ['%06X' % k for k in known], flush=True)
