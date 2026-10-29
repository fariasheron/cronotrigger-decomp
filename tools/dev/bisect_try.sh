#!/bin/bash
# usage: bisect_try.sh manifest.json -> prints max colour count seen in frames 2400..3300 (character demo has 40+)
P=/home/claude/ChronoTriggerSNESRecomp
rm -f $P/recomp/profiles/*.json; cp "$1" $P/recomp/profiles/try.json
cd $P && bash tools/regen.sh --rom /home/claude/ct.sfc >/tmp/bis_regen.log 2>&1
cmake --build build -j8 >/tmp/bis_build.log 2>&1
D=/tmp/bis_fr; rm -rf $D; mkdir -p $D
cd build && SNESRECOMP_FRAME_BMP_DIR=$D SNESRECOMP_FRAME_BMP_START=2400 SNESRECOMP_FRAME_BMP_END=3300 SNESRECOMP_FRAME_BMP_STEP=30 SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout 58 ./ChronoTriggerSNESRecomp --no-launcher /home/claude/ct.sfc >/tmp/bis_run.log 2>&1 || true
python3 -c "
from PIL import Image;import glob
m=0
for f in glob.glob('$D/*.bmp'):
    m=max(m,len(Image.open(f).convert('RGB').getcolors(1<<16) or []))
print(m)"
