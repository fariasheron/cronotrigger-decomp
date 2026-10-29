#!/usr/bin/env bash
# Fetch the framework submodules at their pinned commits and apply this
# project's engine patch to snesrecomp. Safe to run more than once.
set -euo pipefail
cd "$(dirname "$0")/.."
git submodule update --init recomp-ui snesrecomp
if git -C snesrecomp apply --reverse --check ../patches/snesrecomp-ct-decomp.patch 2>/dev/null; then
    echo "snesrecomp: patch already applied"
else
    git -C snesrecomp apply ../patches/snesrecomp-ct-decomp.patch
    echo "snesrecomp: patch applied"
fi
git -C snesrecomp submodule update --init --recursive
