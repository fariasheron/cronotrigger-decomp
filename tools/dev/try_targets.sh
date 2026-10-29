#!/bin/bash
# usage: try_targets.sh TARGET... (hex like C1FE7A) -> distinct frames in 0..600 using uw-old sandbox
P=/home/claude/work/uw-old
python3 - "$@" <<'PY'
import json,sys
b=json.load(open('/tmp/t2.json'))
ts=set('0x'+a.upper() for a in sys.argv[1:])
subs=[d for d in b['discoveries'] if d['target_pc24'] in ts]
if not subs:
    subs=[{"site_pc24":"0xC00000","target_pc24":t,"entry_mx":"M0X0","site_kind":"call_gap","clean_hits":1,"bail_hits":0,"first_frame":1,"last_frame":1} for t in ts]
b['discoveries']=subs; b['ram_routines']=[]
json.dump(b,open('/home/claude/work/uw-old/recomp/profiles/only.json','w'))
PY
cd $P && bash tools/regen.sh --rom /home/claude/ct.sfc >/tmp/tt_regen.log 2>&1 || { echo regen-fail; exit 1; }
grep "analysis:" /tmp/tt_regen.log | head -1
cmake --build build -j8 >/tmp/tt_build.log 2>&1 || { echo build-fail; exit 1; }
D=/tmp/tt_fr; rm -rf $D; mkdir -p $D
cd build && SNESRECOMP_FRAME_BMP_DIR=$D SNESRECOMP_FRAME_BMP_START=0 SNESRECOMP_FRAME_BMP_END=1500 SNESRECOMP_FRAME_BMP_STEP=15 SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout 29 ./ChronoTriggerSNESRecomp --no-launcher /home/claude/ct.sfc >/tmp/tt_run.log 2>&1
echo "distinct frames: $(md5sum $D/*.bmp | awk '{print $1}' | sort -u | wc -l)"
