import json,subprocess,time,sys
base=json.load(open('/tmp/t2_nod3.json'))
D=base['discoveries']
# group by target
tg={}
for d in D: tg.setdefault(d['target_pc24'],[]).append(d)
T=sorted(tg)
def ok(ts):
    sub=[d for t in ts for d in tg[t]]
    m=dict(base); m['discoveries']=sub; m['ram_routines']=[]; json.dump(m,open('/tmp/sub.json','w'))
    n=int(subprocess.check_output(['/home/claude/ChronoTriggerSNESRecomp/tools/dev/bisect_try.sh','/tmp/sub.json']).strip())
    print(len(ts),'targets ->',n,flush=True); return n>=THRESH
import sys
mk=lambda ts: None
THRESH=0
sub=[];m=dict(base); m['discoveries']=[]; m['ram_routines']=[]; json.dump(m,open('/tmp/sub.json','w'))
basen=int(subprocess.check_output(['/home/claude/ChronoTriggerSNESRecomp/tools/dev/bisect_try.sh','/tmp/sub.json']).strip())
THRESH=30; print('baseline',basen,'thresh',THRESH,flush=True)
known=[]   # culprits found
rest=T[:]
for rnd in range(4):
    good=[t for t in rest if t not in known]
    if ok(good): print('remaining set OK; culprits:',known); break
    lo,hi=0,len(good)  # smallest prefix length that fails: ok(good[:lo]) true, ok(good[:hi]) false
    while hi-lo>1:
        mid=(lo+hi)//2
        if ok(good[:mid]): lo=mid
        else: hi=mid
    c=good[hi-1]; print('CULPRIT',c,tg[c],flush=True); known.append(c)
print('FINAL culprits',known)
