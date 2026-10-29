#!/bin/bash
# usage: game_try.sh profile.json -> "<charselect colours> <town colours>" using attract profiles + given subset
P=/home/claude/ChronoTriggerSNESRecomp
S=/tmp/claude-0/scr
cp "$1" $P/recomp/profiles/play1.json
cd $P && rm -f src/gen/.snesrecomp-cache.json && bash tools/regen.sh --rom /home/claude/ct.sfc >/tmp/gt_regen.log 2>&1 || { echo "0 0"; exit 0; }
cmake --build build -j8 >/tmp/gt_build.log 2>&1 || { echo "0 0"; exit 0; }
D=$S/gt; mkdir -p $D; rm -f $D/*.bmp $D/*.bin $D/*.json
cd build && SNESRECOMP_DUMP_DIR=$D SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout 120 ./ChronoTriggerSNESRecomp --no-launcher --script $S/t.txt /home/claude/ct.sfc >/tmp/gt_run.log 2>&1
python3 -c "
from PIL import Image
def c(f):
    try: return len(Image.open(f).convert('RGB').getcolors(1<<16) or [])
    except Exception: return 0
print(c('$D/cs.fb.bmp'), c('$D/town.fb.bmp'))"
