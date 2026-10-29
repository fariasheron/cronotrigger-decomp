#!/usr/bin/env python3
"""llestat.py <native_hist> <all_interp_hist>: interpreted share outside the vblank idle loops + hottest ranges and covering nodes"""
import sys; sys.path.pop(0)
import json
P='/home/claude/ChronoTriggerSNESRecomp/'
def load(p): return {int(a,16):int(b) for a,b in (l.split() for l in open(p))}
n=load(sys.argv[1]); t=load(sys.argv[2])
idle={0xC09F5C,0xC09F5F,0xC09E93,0xC09E96,0xC09E98,0xC0A967,0xC0A969,0xC0A96A,0xC0A96B}
N=sum(c for pc,c in n.items() if pc not in idle); T=sum(c for pc,c in t.items() if pc not in idle)
print('interpreted outside idle: %d of %d = %.1f%%'%(N,T,100*N/T))
m=json.load(open(P+'src/gen/program_manifest.json'))['nodes']
pcs=sorted(pc for pc in n if pc not in idle); rs=[]
for p in pcs:
    if rs and p-rs[-1][1]<=24 and (p>>16)==(rs[-1][0]>>16): rs[-1][1]=p; rs[-1][2]+=n[p]
    else: rs.append([p,p,n[p]])
rs.sort(key=lambda r:-r[2])
for a,b,c in rs[:int(sys.argv[3]) if len(sys.argv)>3 else 20]:
    cov=[k+'/'+v['disposition'][:3] for k,v in m.items() if v['min_pc24']<=a<=v['max_pc24']][:3]
    print(f'{a:06X}-{b:06X} {100*c/N:5.1f}% {cov}')
