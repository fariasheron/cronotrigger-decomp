# reach.py <BANK hex>: which generated entries of a bank are reachable from trusted code
# (decompiled routines, runtime profile discoveries, kept-generated routines), following
# direct calls/jumps and JSR/JMP (abs,X) tables (entries taken while they point into the bank, cap 64).
import sys,re,json,glob,collections
sys.path.insert(0,'tools/decomp')
from fdis import walk
BK=int(sys.argv[1],16)
TB={int(k,16):int(v) for k,v in (x.split(':') for x in sys.argv[2:])}  # table base:count
rom=open('/home/claude/ct.sfc','rb').read()
def rd(a): return rom[a&0x3FFFFF]
def w(a): return rd(a)|rd(a+1)<<8
done=set()
for f in glob.glob('src/decomp/*.c'):
    done|=set(re.findall(r'(?:DC_ENTRY|WALK_ENTRY|MAP_ROW3|LINE_JOB|LAYER_JOBS|SPLIT_JOBS|VM_CMD)\((\w{6})',open(f).read()))
keep=set(l.split()[0] for l in open('tools/decomp/keep_generated.txt') if l.strip() and not l.startswith('#'))
prof=set()
for f in glob.glob('recomp/profiles/*.json'):
    for x in json.load(open(f)).get('discoveries',[]):
        prof.add(x['target_pc24'][2:].upper()); prof.add(x['site_pc24'][2:].upper())
nc=set(l.split()[0] for l in open('tools/decomp/not_code_c1.txt') if l.strip() and not l.startswith('#'))
trusted=done|keep|prof|set(k[:6] for k in json.load(open('src/gen/program_manifest.json'))['nodes'] if k.startswith('C1') and k[:6] not in nc)
d=json.load(open('src/gen/program_manifest.json'))
modes=collections.defaultdict(list)
for k in d['nodes']: modes[int(k[:6],16)].append((int(k[8]),int(k[10])))
real={}
for k,v in d['nodes'].items():
    c=k[:6]
    if int(c,16)>>16==BK: continue
    if c not in trusted and not c.startswith('00'): continue
    for dm in v['demands']:
        t=dm.get('target')
        if t and t['pc24']>>16==BK and dm['kind'] in('direct_call','direct_tail_call'): real.setdefault(t['pc24'],c)
for p in prof:
    if int(p,16)>>16==BK: real.setdefault(int(p,16),'profile')
tables={}
work=list(real); seen=set()
while work:
    e=work.pop()
    if e in seen: continue
    seen.add(e)
    for m,x in (modes.get(e) or [(1,1)]):
        s,labels,calls=walk(rom,e,m,x)
        tg=list(calls)
        for a,v in s.items():
            mn,mode=v[3],v[4]
            if mn=='JMP' and mode=='abs': tg.append((a&0xFF0000)|v[7])
            if mn in('JSR','JMP') and mode=='absxi':
                b=(a&0xFF0000)|v[7]; n=TB.get(b,0)
                for i in range(n): tg.append((b&0xFF0000)|w(b+2*i))
                tables[b]='%06X'%a if b not in TB else n
        for t in tg:
            if t>>16==BK and t not in real:
                real[t]='%06X'%e; work.append(t)
left=sorted(set(k[:6] for k in d['nodes'] if int(k[:6],16)>>16==BK)-done)
r=[e for e in left if int(e,16) in real]; u=[e for e in left if int(e,16) not in real]
print(len(left),'left;',len(r),'reached;',len(u),'unreached')
print('tables:',' '.join('%06X:%s'%(b,n) for b,n in sorted(tables.items())))
print('REACHED',' '.join('%s<%s'%(e,real[int(e,16)]) for e in r))
print('UNREACHED',' '.join(u))
