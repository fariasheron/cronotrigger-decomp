/* Hand-written bodies for code the recompiler could not translate.
 *
 * Empty at scaffold time. Fill this in as regeneration reports unresolved
 * targets — and fill it in with real behaviour, never with a value invented
 * to get past the gap. An invented result turns a loud failure into a silent
 * wrong one (recomp-ai-rules/PRINCIPLES.md, "No stubs"). If a routine cannot
 * be translated yet, leave it to the interpreter tier instead.
 */

#include "cpu_state.h"

/* SNESRECOMP_SELFTEST hook (src/game_rtl.c): native-vs-interpreted checks for
 * individual routines go here when a divergence needs pinning down. */
void uwnh_selftest(CpuState *cpu) { (void)cpu; }
