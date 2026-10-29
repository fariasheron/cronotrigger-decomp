#!/bin/bash
# play.sh <slot> <body.txt> <outdir> [timeout]: load a savestate, run script body, montage the dumps to <outdir>.png
P=/home/claude/ChronoTriggerSNESRecomp
slot=$1; body=$2; out=$(realpath -m $3); tmo=${4:-120}
mkdir -p $out; rm -f $out/*.bmp $out/*.bin $out/*.json $out/*.tsv $out/*.bgrx
{ printf 'wait 120\nloadstate %s\nwait 20\n' $slot; cat $body; printf '\nquit\n'; } > $out.script
cd $P/build && SNESRECOMP_DUMP_DIR=$out SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout $tmo ./ChronoTriggerSNESRecomp --no-launcher --script $out.script /home/claude/ct.sfc > $out.log 2>&1
python3 - $out <<'PY'
import sys,glob,os
from PIL import Image
d=sys.argv[1]
fs=sorted(glob.glob(d+'/*.fb.bmp'),key=os.path.getmtime)
if not fs: print('no dumps'); sys.exit()
n=len(fs); cols=min(n,6); rows=(n+cols-1)//cols
m=Image.new('RGB',(192*cols,168*rows))
for k,f in enumerate(fs): m.paste(Image.open(f).convert('RGB').resize((192,168)),((k%cols)*192,(k//cols)*168))
m.save(d+'.png'); print(n,'dumps ->',d+'.png', [os.path.basename(f).split('.')[0] for f in fs])
PY
