#!/usr/bin/env python3
"""ramprof.py <sweepdir>: WRAM-resident routines seen by tier-2 capture -> `ram_routine`
directives in recomp/bank00.cfg (AUTO block).
The capture's byte image stops at its first RTS, but routines branch past it, so each body is
re-read from the sweep's WRAM dumps and extended to cover every instruction reachable from the
entry (branches both ways, stopping at returns and jumps). The runtime guards every dispatch
with a live byte match, so a body that changes later just falls back to the interpreter."""
import json,sys,os,re,glob,collections
sys.path.insert(0,os.path.dirname(os.path.abspath(__file__)))
from dis65816 import OPS, SIZE
P=os.path.dirname(os.path.abspath(__file__))+'/../../recomp/bank00.cfg'
sw=sys.argv[1]
best={}
for m in glob.glob(sw+'/*.t2.json'):
    for r in json.load(open(m)).get('ram_routines',[]):
        if not r['terminated'] or r['nondeterministic'] or len(r['bytes'])<4: continue
        k=(int(r['entry_pc24'],16),r['entry_mx']); v=best.setdefault(k,{})
        v[r['bytes']]=v.get(r['bytes'],0)+r['hits']
dumps=[open(f,'rb').read() for f in sorted(glob.glob(sw+'/*.wram.bin'))]
def walk(w,entry,m,x):
    """reachable [lo,hi) span within the 64K bank; None if decoding runs off"""
    seen=set(); todo=[(entry,m,x)]; hi=entry
    while todo:
        pc,m,x=todo.pop()
        while True:
            if pc in seen or pc<entry or pc>=entry+0x800: break
            seen.add(pc); op=w[pc]; mn,mode=OPS[op]
            n=(1 if m else 2) if mode=='immm' else (1 if x else 2) if mode=='immx' else SIZE.get(mode,1)
            v=int.from_bytes(w[pc+1:pc+1+n],'little'); hi=max(hi,pc+1+n)
            if mn in ('BRK','STP','COP'): return None
            if mn=='REP': m=0 if v&0x20 else m; x=0 if v&0x10 else x
            if mn=='SEP': m=1 if v&0x20 else m; x=1 if v&0x10 else x
            nxt=pc+1+n
            if mode=='rel': todo.append((nxt+(v-256 if v>127 else v),m,x))
            if mode=='rell': todo.append(((nxt+(v-65536 if v>32767 else v))&0xFFFF,m,x))
            if mn in ('RTS','RTL','RTI','BRA','BRL','JML','WAI'): break
            if mn=='JMP':
                if mode=='abs': todo.append((v,m,x))
                break
            pc=nxt
    return hi
# Bodies whose code jumps to another WRAM routine outside their own image: the
# emitted cross-function goto has no target, so keep them interpreted.
SKIP={0x7E9ABD,0x7E9DBC,0x7EB574,0x7E395E}  # 7E395E: its M1X0 body breaks the intro when called natively
# One byte image per WRAM address: the area is reused by different overlays,
# and a variant captured from another overlay would be called natively from
# its neighbours without the guard. Keep the variants agreeing with the
# most-hit image at that address.
top={}
for (pc,mx),v in best.items():
    b,h=max(v.items(),key=lambda t:t[1])
    if pc not in top or h>top[pc][1]: top[pc]=(b,h)
for k in list(best):
    b=max(best[k],key=best[k].get); ref=top[k[0]][0]; n=min(len(b),len(ref))
    if b[:n]!=ref[:n]: del best[k]
# ...and one M/X variant per address (the most-hit one): a second variant
# called natively from a neighbour (7E5B99 -> 7E395E M1X0) broke the intro.
for k in list(best):
    mine=sum(best[k].values())
    if any(o[0]==k[0] and o!=k and sum(best[o].values())>mine for o in best): del best[k]
lines=[]; stats=collections.Counter()
for (pc,mx),v in sorted(best.items()):
    if pc in SKIP: continue
    cap=bytes.fromhex(max(v,key=v.get)); a=pc&0xFFFF
    if pc>>16!=0x7E: lines.append('ram_routine %06X %s %s'%(pc,mx,cap.hex().upper())); continue
    imgs=collections.Counter()
    for w in dumps:
        if w[a:a+len(cap)]!=cap: continue
        hi=walk(w,a,mx[1]=='1',mx[3]=='1')
        if hi: imgs[w[a:max(hi,a+len(cap))]]+=1
    if imgs:
        body=imgs.most_common(1)[0][0]; stats['extended' if len(body)>len(cap) else 'same']+=1
    else: body=cap; stats['nodump']+=1
    lines.append('ram_routine %06X %s %s'%(pc,mx,body.hex().upper()))
s=open(P).read(); B='# >>> BEGIN auto ram_routine (tools/dev/ramprof.py)\n'; E='# <<< END auto ram_routine\n'
s=re.sub(re.escape(B)+'.*?'+re.escape(E),'',s,flags=re.S)
s=s.rstrip('\n')+'\n\n'+B+''.join(l+'\n' for l in lines)+E
open(P,'w').write(s); print(len(lines),'ram routines',dict(stats))
