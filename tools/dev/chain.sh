#!/bin/bash
# chain.sh <start-slot> <chain-slot> <seed> <rounds> <frames>: random play in rounds, each round starts from the
# previous round's end state (saved into <chain-slot>) so story progress accumulates. Captures tier-2 per round.
P=/home/claude/ChronoTriggerSNESRecomp; S=/tmp/claude-0/scr
start=$1; slot=$2; seed=$3; rounds=$4; frames=$5
cur=$start
for r in $(seq 1 $rounds); do
  python3 $S/rnd.py $((seed*100+r)) $frames > $S/ch_${slot}_$r.txt
  echo "savestate $slot" >> $S/ch_${slot}_$r.txt
  bash $P/tools/dev/cap.sh $cur $S/ch_${slot}_$r.txt $S/chn_${slot}_$r 4000 > $S/chn_${slot}_$r.res 2>&1
  echo "round $r: $(cat $S/chn_${slot}_$r.res) traps=$(grep -c -iE 'badpc|watchdog' $S/chn_${slot}_$r.log)"
  cur=$slot
done
