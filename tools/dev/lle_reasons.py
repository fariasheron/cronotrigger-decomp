import json,collections,sys
d=json.load(open(sys.argv[1] if len(sys.argv)>1 else 'src/gen/program_manifest.json'))
c=collections.Counter(); ex=collections.defaultdict(list)
for x in d['nodes'].values():
    if x['disposition']!='aot_eligible':
        for r in x['reasons'] or ['?']:
            k=str(r)[:90]; c[k]+=1; ex[k].append('%06X m%dx%d'%(x['key']['pc24'],x['key']['m'],x['key']['x']))
for k,v in c.most_common(40): print(v,k,ex[k][:4])
