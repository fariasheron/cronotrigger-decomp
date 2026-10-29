import sys,re,json,glob,collections
sys.path.insert(0,'tools/decomp')
from fdis import walk
exec(open('/tmp/claude-0/tbl.py').read().split("left=[l.split()")[0])  # gives done, ex, bases, tbl via loop
rom=open('ct.sfc','rb').read()
bnd={}
for b in sorted(bases):
    bnd[b]=tbl[b]
modes=collections.defaultdict(list)
for k in d['nodes']: modes[int(k[:6],16)].append((int(k[8]),int(k[10])))
real=set(int(e,16) for e in trusted)
for k,v in d['nodes'].items():
    if k[:2]!='C1':
        for dm in v['demands']:
            t=dm.get('target')
            if dm['kind'] in('direct_call','direct_tail_call') and t and t['pc24']>>16==0xC1: real.add(t['pc24'])
for f in glob.glob('recomp/profiles/*.json'):
    for x in json.load(open(f)).get('discoveries',[]):
        t=int(x['target_pc24'],16)
        if t>>16==0xC1: real.add(t)
why={e:'base' for e in real}
work=list(real); done_w=set()
while work:
    e=work.pop()
    if e in done_w: continue
    done_w.add(e)
    for m,x in (modes.get(e) or [(1,0)]):
        s,labels,calls=walk(rom,e,m,x)
        tg=[c for c in calls]
        for a,v in s.items():
            mn,mode=v[3],v[4]
            if mn=='JMP' and mode in('abs',): tg.append((a&0xFF0000)|v[7])
            if mn in('JSR','JMP') and mode=='absxi':
                b=(a&0xFF0000)|v[7]
                n=bnd.get(b,0)
                for i in range(n): tg.append((b&0xFF0000)|rom[(b+2*i)&0x3FFFFF]|rom[(b+2*i+1)&0x3FFFFF]<<8)
        for t in tg:
            if t>>16==0xC1 and t not in real:
                real.add(t); why[t]='%06X'%e; work.append(t)
left=[l.split()[0] for l in open('/tmp/claude-0/c1left.txt') if l.strip()]
r=[e for e in left if int(e,16) in real]; u=[e for e in left if int(e,16) not in real]
print(len(r),'real',len(u),'unreached')
open('/tmp/claude-0/c1real.txt','w').write('\n'.join('%s %s'%(e,why[int(e,16)]) for e in r)+'\n')
open('/tmp/claude-0/c1fake.txt','w').write('\n'.join(u)+'\n')
