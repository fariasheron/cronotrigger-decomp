#!/bin/bash
# regen + build + demo check with current profiles. prints AOT stats, max colours (frames 2400..3300; demo has 40+), watchdog/crash lines
P=/home/claude/ChronoTriggerSNESRecomp
cd $P && rm -f src/gen/.snesrecomp-cache.json && bash tools/regen.sh --rom /home/claude/ct.sfc >/tmp/ck_regen.log 2>&1 || { echo regen-fail; tail -5 /tmp/ck_regen.log; exit 1; }
grep -E "exact AOT" /tmp/ck_regen.log
cmake --build build -j8 >/tmp/ck_build.log 2>&1 || { echo build-fail; grep -m5 error /tmp/ck_build.log; exit 1; }
D=/tmp/ck_fr; rm -rf $D; mkdir -p $D
cd build && SNESRECOMP_FRAME_BMP_DIR=$D SNESRECOMP_FRAME_BMP_START=0 SNESRECOMP_FRAME_BMP_END=3300 SNESRECOMP_FRAME_BMP_STEP=30 SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout ${1:-58} ./ChronoTriggerSNESRecomp --no-launcher /home/claude/ct.sfc >/tmp/ck_run.log 2>&1
grep -iE "watchdog|fatal|abort|smash" /tmp/ck_run.log | head -3
python3 -c "
from PIL import Image;import glob
fs=sorted(glob.glob('$D/*.bmp'));cs=[len(Image.open(f).convert('RGB').getcolors(1<<16) or []) for f in fs]
print('frames',len(fs),'max colours late',max(cs[80:] or [0]))"
