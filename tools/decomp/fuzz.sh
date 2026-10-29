#!/bin/bash
# fuzz.sh <spec> <outdir> [N] [state]: fuzz the decompiled routines in <spec> from a saved
# machine state (default: Crono's room). Result lines Z (summary), D (difference), F (failed run).
H=$(dirname $(realpath $0))
STATE=${4:-$H/scen/crono_room.sav} CHECK=0 $H/run.sh $H/scen/fuzz.txt $2 SNESRECOMP_TURBO=1 \
  CT_DECOMP_FUZZ=$(realpath $1) CT_DECOMP_FUZZ_FRAME=20 CT_DECOMP_FUZZ_N=${3:-300} >/dev/null
grep -v "^Z" $2/dc.log | head -40
awk '/^Z/ {n++; split($4,b,"="); if (b[2]>0) bad++} END {print n" routines fuzzed, "bad+0" with differences"}' $2/dc.log
