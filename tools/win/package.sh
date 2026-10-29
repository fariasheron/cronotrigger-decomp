#!/bin/bash
# package.sh <version> <previous version>: Windows and Linux zips into the private dist folder
# (ROM-derived: never public). Each zip is the previous release's zip with the new executable,
# LEIA-ME and traducao/ tools; launcher state (rom.cfg, last_run_report.json) is left out.
set -e
V=$1; PV=$2; R=$(cd "$(dirname "$0")/../.." && pwd); D=${DIST:-/mnt/project-files/chrono-trigger/dist}; T=$(mktemp -d)
for os in windows-x64 linux-x64; do
  pos=$os; [ $os = linux-x64 ] && [ ! -f $D/chronotrigger-$PV-$os.zip ] && pos=linux-x86_64
  N=chronotrigger-$V-$os; P=$T/$N
  (cd $T && unzip -q $D/chronotrigger-$PV-$pos.zip && mv chronotrigger-$PV-$pos $N)
  rm -f $P/ChronoTriggerSNESRecomp $P/ChronoTriggerSNESRecomp.exe $P/rom.cfg $P/last_run_report.json $P/ptbr.bin; rm -rf $P/saves; mkdir -p $P/saves
  if [ $os = windows-x64 ]; then cp $R/build-win/ChronoTriggerSNESRecomp.exe $P/ChronoTrigger.exe; else cp $R/build/ChronoTriggerSNESRecomp $P/ChronoTrigger; fi
  cp $R/tools/win/LEIA-ME.txt $P/
  rm -rf $P/traducao; mkdir -p $P/traducao
  cp $R/tools/tr/build_pack.py $R/tools/tr/build_rom.py $R/tools/tr/glyph.py $R/tools/tr/TRADUCAO.txt $P/traducao/
  (cd $T && rm -f $D/$N.zip && python3 -c "import shutil;shutil.make_archive('$D/$N','zip','$T','$N')")
  echo $D/$N.zip
done
rm -rf $T
