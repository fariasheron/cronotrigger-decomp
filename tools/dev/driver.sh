#!/bin/bash
# driver.sh <first-batch-no> <batches> <start-slots...>: unattended coverage loop.
# Each batch: 3 voyage chains (3 rounds x 30k frames) from the given slots + a TRAP_BADPC replay of the
# previous batch's first round, then mkprof -> check.sh -> auto_force_lle until 0 new -> local commit.
# Next batch starts from each chain's last-round slot. Log: /tmp/claude-0/scr/driver.log
P=/home/claude/ChronoTriggerSNESRecomp; S=/tmp/claude-0/scr; L=$S/driver.log
b=$1; n=$2; shift 2; slots=("$@")
cd $P
for k in $(seq 1 $n); do
  echo "=== batch $b $(date -u +%H:%M) from slots ${slots[*]}" >> $L
  bases=(); i=0
  for s in "${slots[@]}"; do
    base=$((300 + b*30 + i*10)); bases+=($base)
    gens=(rnd_voy.py rnd_sea.py rnd_town.py); GEN=${gens[$((i%3))]} bash tools/dev/chainp.sh $s $base $((base*7+k)) 3 30000 > $S/drv_$base.log 2>&1 &
    sleep 3; i=$((i+1))
  done
  if [ -n "$prev_base" ]; then
    grep -v savestate $S/ch_${prev_base}_1.txt > $S/drv_rp.txt
    SNESRECOMP_TRAP_BADPC=1 bash tools/dev/play.sh $prev_slot $S/drv_rp.txt $S/drv_rp 4000 >/dev/null 2>&1
    echo "replay of $prev_base: traps=$(grep -c -iE 'badpc|watchdog' $S/drv_rp.log) $(tail -1 $S/drv_rp.log)" >> $L
  fi
  wait
  for base in "${bases[@]}"; do echo "chain $base: $(grep -c 'traps=0' $S/drv_$base.log)/3 clean; $(grep -v 'traps=0' $S/drv_$base.log | head -2)" >> $L; done
  files=$(for base in "${bases[@]}"; do ls $S/chn_${base}_*.t2.json; done)
  python3 tools/dev/mkprof.py drv$b $files >> $L 2>&1
  for t in 1 2 3 4; do
    bash tools/dev/check.sh > $S/drv_check.log 2>&1; tail -2 $S/drv_check.log >> $L
    r=$(python3 tools/dev/auto_force_lle.py . /home/claude/ct.sfc 2>&1 | tail -1); echo "afl: $r" >> $L
    echo "$r" | grep -q "^0 new" && break
  done
  col=$(grep -o 'max colours late [0-9]*' $S/drv_check.log | awk '{print $4}')
  if [ -z "$col" ] || [ "$col" -lt 40 ]; then
    echo "!!! demo check failed (colours=$col); reverting drv$b" >> $L
    git checkout -- recomp 2>/dev/null; rm -f recomp/profiles/drv$b.json; bash tools/dev/check.sh >> $L 2>&1
  else
    git add -A recomp >/dev/null; git commit -qm "Add drv$b profile (unattended voyage batch)

Co-Authored-By: Claude Opus 5.5 (1M context) <noreply@anthropic.com>
Claude-Session: https://claude.ai/code/session_014y7qUiKFFFXMVnsUJAZjkA" >> $L 2>&1
  fi
  prev_base=${bases[0]}; prev_slot=${slots[0]}
  slots=(); for base in "${bases[@]}"; do slots+=($((base+3))); done
  b=$((b+1))
done
echo "=== driver done $(date -u +%H:%M)" >> $L
