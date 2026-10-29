#!/bin/bash
# regen + build + headless run with tier-2 capture. usage: iterate.sh [frames_seconds]
P=/home/claude/ChronoTriggerSNESRecomp
SECS=${1:-62}
cd $P && bash tools/regen.sh --rom /home/claude/ct.sfc > /tmp/it_regen.log 2>&1 || { echo regen-fail; tail /tmp/it_regen.log; exit 1; }
grep -E "^analysis: [0-9]+ roots|AOT-eligible" /tmp/it_regen.log
cmake --build build -j8 >/tmp/it_b.log 2>&1 || { echo build-fail; grep error /tmp/it_b.log|head; exit 1; }
cmake --build build-diag -j8 >/tmp/it_d.log 2>&1 || { echo diag-build-fail; exit 1; }
rm -rf /tmp/it_fr; mkdir -p /tmp/it_fr
cd build-diag && SNESRECOMP_TIER2_CAPTURE=1 SNESRECOMP_TIER2_MANIFEST=/tmp/it_t2.json SNESRECOMP_FRAME_BMP_DIR=/tmp/it_fr SNESRECOMP_FRAME_BMP_START=0 SNESRECOMP_FRAME_BMP_END=4000 SNESRECOMP_FRAME_BMP_STEP=60 SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout $SECS ./ChronoTriggerSNESRecomp --no-launcher /home/claude/ct.sfc > /tmp/it_run.log 2>&1
grep -E "exit:|FATAL|abort" /tmp/it_run.log | head -3
python3 -c "
import json;d=json.load(open('/tmp/it_t2.json'));print('tier hits',d['total_tier_hits'],'sites',d['distinct_sites'],'bails',sum(x['bail_hits'] for x in d['discoveries']))"
echo "distinct frames: $(md5sum /tmp/it_fr/*.bmp | awk '{print $1}' | sort -u | wc -l) / $(ls /tmp/it_fr | wc -l)"
