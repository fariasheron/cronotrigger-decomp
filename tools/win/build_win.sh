#!/bin/bash
# build_win.sh [sdl2 kit dir]: cross-compile the Windows .exe into build-win/
set -e
cd "$(dirname "$0")/../.."
SDL=${1:-/home/claude/win/SDL2-2.30.8/x86_64-w64-mingw32}
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_TOOLCHAIN_FILE=tools/win/mingw.cmake -DSDL2_ROOT=$SDL -DSDL2_DIR=$SDL/lib/cmake/SDL2 >/dev/null
cmake --build build-win -j4
cp $SDL/bin/SDL2.dll build-win/
