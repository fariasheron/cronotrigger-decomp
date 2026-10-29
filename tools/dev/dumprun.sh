#!/bin/bash
# dumprun.sh <build dir> <out dir> <script>
mkdir -p $2; rm -f $2/*
cd $1 && SNESRECOMP_DUMP_DIR=$2 SDL_VIDEODRIVER=offscreen SDL_RENDER_DRIVER=software SDL_AUDIODRIVER=dummy timeout ${TMO:-120} ./ChronoTriggerSNESRecomp --no-launcher --script $3 /home/claude/ct.sfc > $2.log 2>&1
grep -c "dump" $2.log
