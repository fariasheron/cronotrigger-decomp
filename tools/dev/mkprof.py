#!/usr/bin/env python3
"""mkprof.py <name> <manifest.t2.json>...: merge new, filtered tier-2 discoveries into recomp/profiles/<name>.json"""
import json,sys,glob,os
P=os.path.dirname(os.path.abspath(__file__))+'/../../recomp/profiles/'
name=sys.argv[1]
base=set(x['target_pc24'] for f in glob.glob(P+'*.json') if not f.endswith('/'+name+'.json') for x in json.load(open(f))['discoveries'])
tmpl=None; seen={}; bail=0
for m in sys.argv[2:]:
    d=json.load(open(m)); tmpl=tmpl or d
    for x in d['discoveries']:
        pc=x['target_pc24']
        if pc in base or pc in seen or (int(pc,16)>>16) in (0xD3,0x7E,0x7F): continue
        if x['bail_hits']: bail+=1; continue
        seen[pc]=x
tmpl['discoveries']=list(seen.values()); tmpl.pop('ram_routines',None)
json.dump(tmpl,open(P+name+'.json','w'),indent=1)
print(len(seen),'new roots; skipped with bails:',bail)
