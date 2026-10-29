#!/bin/bash
# equiv.sh <slot> <body.txt> <outdir> [timeout]: run the body natively and interpreter-only, compare WRAM/VRAM at every dump
P=/home/claude/ChronoTriggerSNESRecomp
slot=$1; body=$2; out=$3; tmo=${4:-600}
bash $P/tools/dev/play.sh $slot $body $out.nat $tmo >/dev/null
SNESRECOMP_LLE_BOUNCE=0 bash $P/tools/dev/play.sh $slot $body $out.lle $tmo >/dev/null
python3 - $out <<'PY'
import sys,glob,os,hashlib
o=sys.argv[1]; bad=0; n=0
for f in sorted(glob.glob(o+'.nat/*.wram.bin')):
    t=os.path.basename(f).split('.')[0]; n+=1
    for kind in ('wram','vram'):
        a=open(f'{o}.nat/{t}.{kind}.bin','rb').read(); g=f'{o}.lle/{t}.{kind}.bin'
        b=open(g,'rb').read() if os.path.exists(g) else b''
        if a!=b:
            diff=[i for i in range(min(len(a),len(b))) if a[i]!=b[i]]
            print('DIFF',t,kind,len(diff),'bytes, first',hex(diff[0]) if diff else 'len'); bad+=1
print('dumps',n,'mismatches',bad)
PY
