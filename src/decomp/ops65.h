/* Small 65816 instruction helpers shared by the battle banks (CD, CF, ...).
 * Same semantics as the ones at the top of battle_c1.c. */
#pragma once
#include "ram.h"

static inline __attribute__((unused)) void lda8(u8 v) { dc_set_a8(v); dc_set_flags_nz8(v); }
static inline __attribute__((unused)) void lda16(u16 v) { g_cpu.A = v; dc_set_flags_nz16(v); }
/* LDX/LDY in the current index width. */
static inline __attribute__((unused)) void ldx(u16 v)
{
    if (g_cpu.x_flag) { g_cpu.X = v & 0xFF; dc_set_flags_nz8((u8)v); }
    else { g_cpu.X = v; dc_set_flags_nz16(v); }
}
static inline __attribute__((unused)) void ldy(u16 v)
{
    if (g_cpu.x_flag) { g_cpu.Y = v & 0xFF; dc_set_flags_nz8((u8)v); }
    else { g_cpu.Y = v; dc_set_flags_nz16(v); }
}
static inline __attribute__((unused)) u16 abs_x(u16 a) { return g_cpu.x_flag ? abs8(a) : abs16(a); }
static inline __attribute__((unused)) u16 dp_x(u8 o) { return g_cpu.x_flag ? dp8(o) : dp16(o); }
static inline __attribute__((unused)) void stx_dp(u8 o) { if (g_cpu.x_flag) dp8_set(o, (u8)g_cpu.X); else dp16_set(o, g_cpu.X); }
static inline __attribute__((unused)) void tdc(void) { g_cpu.A = g_cpu.D; dc_set_flags_nz16(g_cpu.D); }
/* JSR (table,X) in the bank of site: entries t[X / 2] when decompiled in this file, else the original. */
typedef void (*glue_fn)(void);
static inline __attribute__((unused)) void jump_table(const glue_fn *t, unsigned n, u16 table, u32 site)
{
    unsigned i = g_cpu.X >> 1;
    u32 bank = site & 0xFF0000;
    if (!(g_cpu.X & 1) && i < n) t[i]();
    else dc_jsr(bank | bus16(bank | (u16)(table + g_cpu.X)), site);
}
/* XBA: N and Z from the new low byte. */
static inline __attribute__((unused)) void xba(void) { g_cpu.A = (u16)(g_cpu.A >> 8 | g_cpu.A << 8); dc_set_flags_nz8((u8)g_cpu.A); }
/* TAX/TAY take B too when the index is 16-bit. */
static inline __attribute__((unused)) void tax(void) { ldx(g_cpu.A); }
static inline __attribute__((unused)) void tay(void) { ldy(g_cpu.A); }
static inline __attribute__((unused)) void inx(void) { ldx((u16)(g_cpu.X + 1)); }
static inline __attribute__((unused)) void dex(void) { ldx((u16)(g_cpu.X - 1)); }
static inline __attribute__((unused)) void cpx(u16 v) { if (g_cpu.x_flag) dc_cmp8((u8)g_cpu.X, (u8)v); else dc_cmp16(g_cpu.X, v); }
static inline __attribute__((unused)) void inc_abs(u16 a) { u8 v = (u8)(abs8(a) + 1); abs8_set(a, v); dc_set_flags_nz8(v); }
static inline __attribute__((unused)) void dec_abs(u16 a) { u8 v = (u8)(abs8(a) - 1); abs8_set(a, v); dc_set_flags_nz8(v); }
static inline __attribute__((unused)) u8 asl8(u8 v) { g_cpu._flag_C = v >> 7; v = (u8)(v << 1); dc_set_flags_nz8(v); return v; }
static inline __attribute__((unused)) void php(void) { dc_sync_p(); dc_push8(g_cpu.P); }
static inline __attribute__((unused)) void plp(void)
{
    g_cpu.P = dc_pull8();
    cpu_p_to_mirrors(&g_cpu);
    if (g_cpu.x_flag) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
}
static inline __attribute__((unused)) void phx(void) { if (g_cpu.x_flag) dc_push8((u8)g_cpu.X); else dc_push16(g_cpu.X); }
static inline __attribute__((unused)) void plx(void) { ldx(g_cpu.x_flag ? dc_pull8() : dc_pull16()); }
static inline __attribute__((unused)) void stx_abs(u16 a) { if (g_cpu.x_flag) abs8_set(a, (u8)g_cpu.X); else abs16_set(a, g_cpu.X); }
static inline __attribute__((unused)) void sty_abs(u16 a) { if (g_cpu.x_flag) abs8_set(a, (u8)g_cpu.Y); else abs16_set(a, g_cpu.Y); }
static inline __attribute__((unused)) void cpy(u16 v) { if (g_cpu.x_flag) dc_cmp8((u8)g_cpu.Y, (u8)v); else dc_cmp16(g_cpu.Y, v); }
static inline __attribute__((unused)) void iny(void) { ldy((u16)(g_cpu.Y + 1)); }
/* [dp] long pointer. */
static inline __attribute__((unused)) u32 lp(u8 o) { return dp16(o) | (u32)dp8((u8)(o + 2)) << 16; }
static inline __attribute__((unused)) void sta_all(const u16 *a, unsigned n) { for (unsigned k = 0; k < n; k++) abs8_set(a[k], (u8)g_cpu.A); }
/* Shifts of A in either accumulator width (the multiply/divide-by-2^n stubs at 010D..011E). */
static inline __attribute__((unused)) void asl_a(int n)
{
    for (int k = 0; k < n; k++) {
        if (g_cpu.m_flag) { u8 v = (u8)g_cpu.A; g_cpu._flag_C = v >> 7; dc_set_a8((u8)(v << 1)); dc_set_flags_nz8((u8)(v << 1)); }
        else { g_cpu._flag_C = g_cpu.A >> 15; g_cpu.A = (u16)(g_cpu.A << 1); dc_set_flags_nz16(g_cpu.A); }
    }
}
static inline __attribute__((unused)) void lsr_a(int n)
{
    for (int k = 0; k < n; k++) {
        if (g_cpu.m_flag) { u8 v = (u8)g_cpu.A; g_cpu._flag_C = v & 1; dc_set_a8(v >> 1); dc_set_flags_nz8(v >> 1); }
        else { g_cpu._flag_C = g_cpu.A & 1; g_cpu.A >>= 1; dc_set_flags_nz16(g_cpu.A); }
    }
}
