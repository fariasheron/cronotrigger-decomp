# Chrono Trigger (SNES) — decompilation

A native PC port of **Chrono Trigger** (USA, SNES) whose game code is a readable,
hand-written C decompilation: about 2,000 routines in `src/decomp/`, each one
verified against the original 65816 code by fuzzing and by an in-game checker that
runs the original side by side and compares the machine state.

The project started as a static recompilation built on
[snesrecomp](https://github.com/mstan/snesrecomp). The framework still provides the
runtime (PPU, DSP, DMA, host), the generated fallback code and the 65816 interpreter
for the few parts that stay interpreted (self-modifying scene blocks and the
sound-driver handshake in bank C7).

> **This repository contains no game data.** No ROM, graphics, music, text, tables
> or generated code is distributed here or in any release. To build or run it you
> need your own legally obtained copy of the game. Everything the game needs at run
> time is read from that ROM.

*Português:* decompilação em C do Chrono Trigger (SNES, versão americana). O
repositório não contém nada da ROM: para compilar e jogar, use a sua própria cópia
do jogo (CRC32 `2d206bf7`).

## Your ROM

| | |
|---|---|
| Game | Chrono Trigger (USA) |
| Size | 4 MiB, HiROM, no copier header |
| CRC32 | `2d206bf7` |
| SHA-1 | `de5822f4f2f7a55acb8926d4c0eaa63d5d989312` |
| SHA-256 | `06d1c2b06b716052c5596aaa0c2e5632a027fee1a9a28439e509f813c30829a9` |

A `.smc` with a 512-byte copier header must be stripped first
(`tail -c 4194304 game.smc > ct.sfc`). `tools/regen.sh` and the executable refuse any
other dump.

## Build (Linux)

Requirements: git, CMake 3.20+, a C compiler, Python 3, a Rust toolchain (cargo) and
SDL2 development files (`apt install libsdl2-dev`).

```sh
git clone --recursive https://github.com/fariasheron/cronotrigger-decomp
cd cronotrigger-decomp
bash tools/setup.sh                      # submodules + engine patch
bash tools/regen.sh --rom /path/to/ct.sfc
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSNESRECOMP_SDL_BACKEND=SDL2
cmake --build build -j
./build/ChronoTriggerSNESRecomp /path/to/ct.sfc
```

`tools/regen.sh` verifies the ROM and generates `src/gen/*.c` and `recomp/funcs.h`
locally. That generated code comes from your ROM, so it is git-ignored and must never
be committed. With no ROM on the command line the recomp-ui launcher opens a ROM
picker. Windows builds are cross-compiled with mingw-w64 (`tools/win/build_win.sh`).

## Layout

| Path | What lives there |
|---|---|
| `src/decomp/` | The decompilation: field, events, menus, battle, video, sound, decompression and the code the game copies to WRAM (7E) |
| `src/decomp/dc_rt.c`, `dc.h`, `ops65.h` | Small runtime the decompiled C uses (CPU registers, flags, memory access) |
| `src/decomp/dc_cost.inc`, `live.inc` | Per-routine timing (master cycles) and register liveness, produced by `tools/decomp/` |
| `src/` | Host glue: `main.c`, `game_rtl.c` (frame driver), `ct_tr.c` (optional translation pack loader) |
| `recomp/` | Recompiler input: `bank*.cfg`, `symbols.toml`, coverage profiles (addresses only) |
| `patches/snesrecomp-ct-decomp.patch` | Changes to snesrecomp this port needs (decomp hooks, fixes); applied by `tools/setup.sh` |
| `tools/decomp/` | Verification: fuzzer, in-game checker scripts, coverage and reachability analysis |
| `tools/tr/` | Builders for an optional translation pack / ROM patch from text *you* supply |
| `tools/dev/` | Debugging helpers (disassembler, bisection, profiles). Some paths are hard-coded to the original dev machine |
| `snesrecomp/`, `recomp-ui/` | Framework submodules, pinned in `framework_pins.txt` |

## How it was verified

* **Fuzzing:** each decompiled routine runs from thousands of random machine states
  against the original in the interpreter, and the resulting machine state must
  match.
* **In-game checker:** while the game plays, the checker runs the original next to the
  C version of every routine it reaches and stops at the first difference.
* **Scenario replay:** scripted runs (boot, new game, field, menus, battles, endings)
  compared frame by frame and on the DSP output against the recompiled build.

Fuzz and checker runs need save states taken from your own game; none are included.

## License

The scaffold and this project's code are distributed under the **PolyForm
Noncommercial License 1.0.0** (see `LICENSE`), the same terms as snesrecomp, from
whose project template this repository was created. snesrecomp is not vendored: it is
a submodule plus a patch (`patches/`), under its own license
(`snesrecomp/LICENSE`, `snesrecomp/THIRD_PARTY_ATTRIBUTION.md`). recomp-ui is MIT.

Required Notice: Copyright (c) 2026 Matthew Stanley (snesrecomp and the project
template).

Chrono Trigger is © Square Enix. This project is not affiliated with or endorsed by
Square Enix. None of these licenses cover the game or its data, which are not
distributed here.
