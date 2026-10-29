"""Find profile targets that break the new-game flow. usage: bisect_game.py full.json"""
import json, subprocess, sys
base = json.load(open(sys.argv[1]))
tg = {}
for d in base['discoveries']: tg.setdefault(d['target_pc24'], []).append(d)
T = sorted(tg)
TRY = '/home/claude/ChronoTriggerSNESRecomp/tools/dev/game_try.sh'
def ok(ts):
    m = dict(base); m['discoveries'] = [d for t in ts for d in tg[t]]
    json.dump(m, open('/tmp/claude-0/scr/sub.json', 'w'))
    cs, town = map(int, subprocess.check_output(['bash', TRY, '/tmp/claude-0/scr/sub.json']).split())
    good = cs >= 29 and town >= 40
    print(len(ts), 'targets ->', cs, town, 'ok' if good else 'BAD', flush=True)
    return good
known = []
for rnd in range(6):
    good = [t for t in T if t not in known]
    if ok(good): print('remaining set OK; culprits:', known, flush=True); break
    lo, hi = 0, len(good)
    while hi - lo > 1:
        mid = (lo + hi) // 2
        if ok(good[:mid]): lo = mid
        else: hi = mid
    c = good[hi - 1]; print('CULPRIT', c, tg[c], flush=True); known.append(c)
print('FINAL culprits', known, flush=True)
