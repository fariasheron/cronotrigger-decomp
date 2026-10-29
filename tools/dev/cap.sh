#!/bin/bash
# cap.sh <slot> <body.txt> <outdir> [timeout]: like play.sh, but captures a tier-2 manifest to <outdir>.t2.json
P=/home/claude/ChronoTriggerSNESRecomp
slot=$1; body=$2; out=$3; tmo=${4:-600}
mkdir -p $out; rm -f $out/*
{ printf 'wait 120\nloadstate %s\nwait 20\n' $slot; cat $body; printf '\nquit\n'; } > $out.script
cd $P/build && SNESRECOMP_TIER2_CAPTURE=1 SNESRECOMP_TIER2_MANIFEST=$out.t2.json SNESRECOMP_DUMP_DIR=$out SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout $tmo ./ChronoTriggerSNESRecomp --no-launcher --script $out.script /home/claude/ct.sfc > $out.log 2>&1
echo "exit=$? $(tail -1 $out.log)"
