#!/bin/bash
# usage: force_try.sh profile.json PC24... -> game_try result with those routines forced to LLE
P=/home/claude/ChronoTriggerSNESRecomp
prof=$1; shift
cp $P/recomp/bank00.cfg /tmp/claude-0/scr/bank00.cfg.orig
python3 - "$@" <<'PY'
import sys
p='/home/claude/ChronoTriggerSNESRecomp/recomp/bank00.cfg';s=open(p).read()
blk='# >>> BEGIN bisect force_lle\n'+''.join('force_lle %s\n'%a for a in sys.argv[1:])+'# <<< END bisect force_lle\n\n'
open(p,'w').write(s.replace('# >>> BEGIN symbols.toml',blk+'# >>> BEGIN symbols.toml',1))
PY
bash $P/tools/dev/game_try.sh $prof
cp /tmp/claude-0/scr/bank00.cfg.orig $P/recomp/bank00.cfg
