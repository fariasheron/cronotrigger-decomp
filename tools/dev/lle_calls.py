#!/usr/bin/env python3
"""lle_calls.py <hist.txt>: call targets the interpreter executed (JSL/JSR from an interpreted PC) that are not
analysed nodes, weighted by the interpreted steps in the ~callee range. Suggests new `func` roots."""
import sys; sys.path.pop(0)  # tools/dev/bisect.py would shadow the stdlib module
import json,collections
P='/home/claude/ChronoTriggerSNESRecomp/'
rom=open('/home/claude/ct.sfc','rb').read()
def rd(pc):
    b=pc>>16; a=pc&0xFFFF
    if b>=0xC0: return rom[((b-0xC0)<<16)|a]
    if b<0x40 and a>=0x8000: return rom[(b<<16)|a]
    return None
m=json.load(open(P+'src/gen/program_manifest.json'))
nodes={}
for k,n in m['nodes'].items():
    nodes[n['key']['pc24'] & 0xFFFFFF]=n['disposition']
hist={int(a,16):int(b) for a,b in (l.split() for l in open(sys.argv[1]))}
calls=collections.Counter()
for pc,c in hist.items():
    op=rd(pc)
    if op==0x22: t=rd(pc+1)|rd(pc+2)<<8|rd(pc+3)<<16
    elif op==0x20: t=(pc&0xFF0000)|rd(pc+1)|rd(pc+2)<<8
    else: continue
    calls[t]+=c
# weight: steps executed in [t, t+0x600) as a rough body size
pcs=sorted(hist)
import bisect
res=[]
for t,c in calls.items():
    i=bisect.bisect_left(pcs,t); j=bisect.bisect_left(pcs,t+0x600)
    res.append((sum(hist[p] for p in pcs[i:j]),t,c))
for w,t,c in sorted(res,reverse=True)[:int(sys.argv[2]) if len(sys.argv)>2 else 30]:
    print(f'{t:06X} calls={c} steps~{w} {nodes.get(t, nodes.get(t|0xC00000,"not-analysed"))}')
print(len(nodes),'nodes known')
