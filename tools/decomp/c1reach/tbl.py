import sys,re,json,collections,glob
sys.path.insert(0,'tools/decomp')
from fdis import walk
rom=open('ct.sfc','rb').read()
def rd(a): return rom[a&0x3FFFFF]
def w(a): return rd(a)|rd(a+1)<<8
done=set()
for f in glob.glob('src/decomp/*.c'):
    done|=set(re.findall(r'DC_ENTRY\((C1[0-9A-F]{4})',open(f).read()))
ex=set(l.split()[0] for l in open('/tmp/claude-0/swprof.txt'))
trusted=sorted({e for e in done|ex if e.startswith('C1')})
cover=set()
for e in trusted:
    s,_,_=walk(rom,int(e,16),1,0)
    for a,v in s.items():
        for k in range(1+v[5]): cover.add(a+k)
d=json.load(open('src/gen/program_manifest.json'))
bases=set(); sitebase={}
for k,v in d['nodes'].items():
    for dm in v['demands']:
        if dm['kind']=='static_dispatch':
            s=dm['site_pc24']
            if rd(s) in (0x7C,0xFC) and s>>16==0xC1: b=0xC10000|w(s+1); bases.add(b); sitebase[s]=b
real=set(); tbl={}
for b in sorted(bases):
    nx=min([x for x in bases if x>b],default=0xC20000)
    i=0
    OV={0xC10051:10,0xC12D81:15,0xC17A6B:219,0xC17C4D:0,0xC19CB1:0,0xC1B92D:26}
    if b in OV: nx=b+2*OV[b]
    while b+2*i+1<nx and i<256:
        a=b+2*i
        if a in cover or a+1 in cover: break
        t=0xC10000|w(a)
        if t in cover and 'C1%04X'%(t&0xFFFF) not in done and t not in [int(x,16) for x in []]:
            pass
        real.add('%06X'%t); i+=1
    tbl[b]=i
left=[l.split()[0] for l in open('/tmp/claude-0/c1left.txt') if l.strip()]
r=[e for e in left if e in real]
print(len(trusted),'trusted', len(cover),'bytes covered')
print(len(r),'left entries inside a bounded table;',len(left)-len(r),'outside')
for b in sorted(tbl): print('%06X %d'%(b,tbl[b]))
open('/tmp/claude-0/intbl.txt','w').write('\n'.join(r)+'\n')
open('/tmp/claude-0/outtbl.txt','w').write('\n'.join(e for e in left if e not in real)+'\n')
