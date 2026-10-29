#!/usr/bin/env python3
"""funclist.py <src/gen> > list: one line per generated body, "pc24 variant frame name".
frame = return-frame size the callers push (2 JSR, 3 JSL), taken from the generated call
sites; bodies never called directly fall back to their own RTS/RTL. Ambiguous ones are skipped."""
import sys,os,re,collections
VAR={'M0X0':0,'M0X1':1,'M1X0':2,'M1X1':3}
d=sys.argv[1]
calls=collections.defaultdict(set); body={}
for fn in sorted(os.listdir(d)):
    if not fn.endswith('.c'): continue
    cur=None; hrv=None; hl=-99
    for ln,line in enumerate(open(os.path.join(d,fn))):
        m=re.match(r'RecompReturn (bank_[0-9A-F]{2}_[0-9A-F]{4}_M[01]X[01])\(CpuState \*cpu\) \{',line)
        if m: cur=m.group(1); body[cur]=set(); continue
        m=re.search(r'host_return_valid = ([23]);',line)
        if m: hrv=int(m.group(1)); hl=ln; continue
        m=re.search(r'_r = (bank_[0-9A-F]{2}_[0-9A-F]{4}_M[01]X[01])\(cpu\)',line)
        if m and ln-hl<=12: calls[m.group(1)].add(hrv)
        if cur:
            if '/* RTS host return' in line: body[cur].add(2)
            elif '/* RTL host return' in line: body[cur].add(3)
for name in sorted(body):
    fs=calls.get(name) or body[name]
    if len(fs)!=1: continue
    m=re.match(r'bank_([0-9A-F]{2})_([0-9A-F]{4})_(M[01]X[01])$',name)
    print('%s%s %d %d %s'%(m.group(1),m.group(2),VAR[m.group(3)],next(iter(fs)),name))
