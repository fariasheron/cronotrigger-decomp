#!/bin/bash
# capb.sh <script.txt> <outdir> [timeout]: run a script from power-on, capture a tier-2 manifest to <outdir>.t2.json
P=/home/claude/ChronoTriggerSNESRecomp
body=$(realpath $1); out=$(realpath -m $2); tmo=${3:-900}
mkdir -p $out; rm -f $out/*
cd $P/build && SNESRECOMP_TIER2_CAPTURE=1 SNESRECOMP_TIER2_MANIFEST=$out.t2.json SNESRECOMP_DUMP_DIR=$out SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout $tmo ./ChronoTriggerSNESRecomp --no-launcher --script $body /home/claude/ct.sfc > $out.log 2>&1
echo "exit=$? $(tail -1 $out.log)"
