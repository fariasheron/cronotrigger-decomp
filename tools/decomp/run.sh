#!/bin/bash
# run.sh <scenario.txt> <outdir> [extra env...]: headless unthrottled run of the current build
# with the decomp check and the routine profile on. Results: <outdir>/dc.log, prof.txt, *.fb.bmp
P=$(realpath $(dirname $0)/../..); R=${RUNDIR:-/home/claude/ct/run}; B=${BUILD:-$P/build}
scen=$(realpath $1); out=$(realpath -m $2); shift 2
mkdir -p $out $R/saves; rm -f $out/*
for f in ChronoTriggerSNESRecomp keybinds.ini rom.cfg; do cp $B/$f $R/; done
sed -e 's/DisableFrameDelay = 0/DisableFrameDelay = 1/' -e 's/EnableAudio = 1/EnableAudio = 0/' $B/config.ini > $R/config.ini
[ -d $R/assets ] || cp -r $B/assets $R/
find $R/saves -maxdepth 1 -type f -delete; [ -n "$SRM" ] && cp "$SRM" $R/saves/save.srm; [ -n "$STATE" ] && cp "$STATE" $R/saves/save1.sav
cd $R && env CT_DECOMP_CHECK=${CHECK:-1} CT_DECOMP_LOG=$out/dc.log CT_PROF=$out/prof.txt SNESRECOMP_DUMP_DIR=$out \
  SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy "$@" \
  timeout -k 10 ${TMO:-600} ./ChronoTriggerSNESRecomp --no-launcher --script $scen /home/claude/ct.sfc > $out/out.log 2>&1
echo "exit=$? $(grep -c '^D ' $out/dc.log 2>/dev/null) diffs; $(tail -1 $out/out.log)"
