/* Chrono Trigger decomp: shared helpers.
 *
 * Each decompiled routine is plain C over named WRAM variables (ram.h). It
 * replaces the recompiled body of the original 65816 routine through the
 * hook the code generator puts at the top of every generated routine: a
 * linked dc_XXXXXX() (weak reference) runs instead of the translated body,
 * for every M/X variant. Adding or changing a decompiled routine only needs
 * this file compiled and the game relinked, no regeneration.
 *
 * The glue (DC_ENTRY bodies) moves values between the 65816 registers and
 * the C arguments/results, and returns with the routine's own RTS/RTL frame.
 * Everything a caller can observe (WRAM, SRAM, A/X/Y, flags, DB, D) must end
 * up exactly as the original leaves it: CT_DECOMP_CHECK=1 runs the original
 * in the interpreter in a forked child on every sampled call and logs any
 * difference (dc_rt.c). */
#ifndef CT_DECOMP_DC_H
#define CT_DECOMP_DC_H

#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include "cpu_state.h"

typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef int8_t s8;
typedef int16_t s16;

extern CpuState g_cpu;

/* ---- memory ------------------------------------------------------------ */
/* WRAM by its 17-bit offset: 7E:xxxx = 0x0xxxx, 7F:xxxx = 0x1xxxx. Low RAM
 * 00:0000-1FFF is the same memory as 7E:0000-1FFF. */
#define WRAM (g_cpu.ram)
static inline u8  ram8(u32 a)  { return WRAM[a & 0x1FFFF]; }
static inline u16 ram16(u32 a) { return (u16)(WRAM[a & 0x1FFFF] | WRAM[(a + 1) & 0x1FFFF] << 8); }
static inline u32 ram24(u32 a) { return ram16(a) | (u32)ram8(a + 2) << 16; }
static inline void ram8_set(u32 a, u8 v)   { WRAM[a & 0x1FFFF] = v; }
static inline void ram16_set(u32 a, u16 v) { WRAM[a & 0x1FFFF] = (u8)v; WRAM[(a + 1) & 0x1FFFF] = (u8)(v >> 8); }

/* Any 24-bit bus address (ROM, WRAM, SRAM, I/O) through the emulated bus.
 * ROM reads go through here so the translation pack hooks still apply. */
static inline u8  bus8(u32 a)  { return cpu_read8(&g_cpu, (u8)(a >> 16), (u16)a); }
static inline u16 bus16(u32 a) { return cpu_read16(&g_cpu, (u8)(a >> 16), (u16)a); }
static inline u32 bus24(u32 a) { return bus16(a) | (u32)bus8(a + 2) << 16; }
static inline void bus8_set(u32 a, u8 v)   { cpu_write8(&g_cpu, (u8)(a >> 16), (u16)a, v); }
static inline void bus16_set(u32 a, u16 v) { cpu_write16(&g_cpu, (u8)(a >> 16), (u16)a, v); }
#define rom8  bus8
#define rom16 bus16
#define rom24 bus24

/* Hardware registers (bank 00). */
static inline void io8_set(u16 r, u8 v)   { cpu_write8(&g_cpu, 0, r, v); }
static inline void io16_set(u16 r, u16 v) { cpu_write16(&g_cpu, 0, r, v); }
static inline u8   io8(u16 r)             { return cpu_read8(&g_cpu, 0, r); }

/* Direct page: most routines run with a fixed D (field code: D = $0100) and
 * the decomp names those variables by their absolute WRAM address. A
 * routine states the D it relies on; a different D is reported (once per
 * routine) on stderr and in the check log. */
void dc_assume_d_failed(u32 pc, u16 want);
#define DC_ASSUME_D(pc, d) do { if (g_cpu.D != (d)) dc_assume_d_failed(0x##pc, (d)); } while (0)
void dc_assume_m_failed(u32 pc, int want);
/* The routine only makes sense with this accumulator width (the other entry
 * the analysis found decodes as garbage); logs once when entered otherwise. */
#define DC_ASSUME_M(pc, m) do { if (g_cpu.m_flag != (m)) dc_assume_m_failed(0x##pc, (m)); } while (0)
void dc_assume_x_failed(u32 pc, int want);
/* Same for the index width. */
#define DC_ASSUME_X(pc, x) do { if (g_cpu.x_flag != (x)) dc_assume_x_failed(0x##pc, (x)); } while (0)
/* Direct page (bank 0): WRAM below $2000, the bus (I/O) above. */
static inline u8   b0_8(u16 a)            { return a < 0x2000 ? ram8(a) : bus8(a); }
static inline void b0_8_set(u16 a, u8 v)   { if (a < 0x2000) ram8_set(a, v); else bus8_set(a, v); }
static inline u8   dp8(u8 o)               { return b0_8((u16)(g_cpu.D + o)); }
static inline u16  dp16(u8 o)              { u16 a = (u16)(g_cpu.D + o); return (u16)(b0_8(a) | b0_8((u16)(a + 1)) << 8); }
/* MVN: copy A+1 bytes from src:X to dst:Y upward; leaves A = FFFF, DB = dst. */
static inline void dc_mvn(u8 dst, u8 src)
{
    do {
        bus8_set((u32)dst << 16 | g_cpu.Y, bus8((u32)src << 16 | g_cpu.X));
        g_cpu.X++; g_cpu.Y++;
        if (g_cpu.x_flag) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
        g_cpu.A--;
    } while (g_cpu.A != 0xFFFF);
    g_cpu.DB = dst;
}
/* MVP: the same, copying downward from src:X to dst:Y. */
static inline void dc_mvp(u8 dst, u8 src)
{
    do {
        bus8_set((u32)dst << 16 | g_cpu.Y, bus8((u32)src << 16 | g_cpu.X));
        g_cpu.X--; g_cpu.Y--;
        if (g_cpu.x_flag) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
        g_cpu.A--;
    } while (g_cpu.A != 0xFFFF);
    g_cpu.DB = dst;
}
static inline void dp8_set(u8 o, u8 v)     { b0_8_set((u16)(g_cpu.D + o), v); }
static inline void dp16_set(u8 o, u16 v)   { u16 a = (u16)(g_cpu.D + o); b0_8_set(a, (u8)v); b0_8_set((u16)(a + 1), (u8)(v >> 8)); }
/* Absolute addressing (abs, abs+X): the data bank register selects the
 * bank, and the address may carry into the next bank. */
static inline u32  dba(u32 a)              { return ((u32)g_cpu.DB << 16) + a; }
static inline u8   abs8(u32 a)             { return bus8(dba(a)); }
static inline u16  abs16(u32 a)            { return bus16(dba(a)); }
/* A store to a PPU data port or DMA enable through DB = 7E/7F lands in WRAM: noted for the
 * fuzz report (usually a spec whose DB keeps the routine's hardware writes off the PPU). */
extern u16 g_dc_hw_in_wram;
static inline void dc_note_hw(u32 x)
{
    u16 a = (u16)x;
    if ((x >> 17) == 0x3F && (a == 0x2104 || a == 0x2118 || a == 0x2119 || a == 0x2122 || a == 0x2180 || a == 0x420B || a == 0x420C))
        g_dc_hw_in_wram = a;
}
static inline void abs8_set(u32 a, u8 v)   { dc_note_hw(dba(a)); bus8_set(dba(a), v); }
static inline void abs16_set(u32 a, u16 v) { dc_note_hw(dba(a)); dc_note_hw(dba(a) + 1); bus16_set(dba(a), v); }

/* ---- registers --------------------------------------------------------- */
#define R (&g_cpu)
static inline void dc_set_flags_nz8(u8 v)   { g_cpu._flag_Z = v == 0; g_cpu._flag_N = v >> 7; }
static inline void dc_set_flags_nz16(u16 v) { g_cpu._flag_Z = v == 0; g_cpu._flag_N = v >> 15; }
/* Flags as CMP/CPX/CPY leave them (a - b). */
static inline void dc_cmp8(u8 a, u8 b)    { g_cpu._flag_C = a >= b; dc_set_flags_nz8((u8)(a - b)); }
static inline void dc_cmp16(u16 a, u16 b) { g_cpu._flag_C = a >= b; dc_set_flags_nz16((u16)(a - b)); }
/* ADC/SBC (binary mode) with the carry already in the C flag. */
static inline u8 dc_adc8(u8 a, u8 b)
{ unsigned s = a + b + g_cpu._flag_C; u8 r = (u8)s; g_cpu._flag_C = s > 0xFF;
  g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 7) & 1; dc_set_flags_nz8(r); return r; }
static inline u8 dc_sbc8(u8 a, u8 b)
{ return dc_adc8(a, (u8)~b); }
static inline u16 dc_adc16(u16 a, u16 b)
{ u32 s = (u32)a + b + g_cpu._flag_C; u16 r = (u16)s; g_cpu._flag_C = s > 0xFFFF;
  g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1; dc_set_flags_nz16(r); return r; }
static inline u16 dc_sbc16(u16 a, u16 b)
{ return dc_adc16(a, (u16)~b); }
/* Sync P from the per-flag mirrors after the glue changed flags / M / X. */
void dc_sync_p(void);
/* Set the M/X width flags (REP/SEP) the way the original leaves them. */
void dc_set_mx(int m, int x);
/* A in 8-bit mode keeps its high byte (B). */
static inline void dc_set_a8(u8 v)  { g_cpu.A = (u16)((g_cpu.A & 0xFF00) | v); }

/* Let emulated time pass (master clocks) in loops that wait on the beam:
 * decompiled code otherwise takes no time at all. */
static inline void dc_spend(unsigned mc) { g_cpu.master_cycles += mc; }
/* The V counter (213D, after a 2137 latch) as a timing-dependent routine reads it.
 * In check mode's replay (dc_rt.c) the n-th read can be scripted, to show that a
 * difference from the original only comes from where the beam was. */
extern int g_dc_vscript;
u8 dc_vscript_read(void);
u8 dc_beam_latch(void);   /* abs8(0x2137) (SLHV); the check's latch replay overrides what it latches */
static inline u8 dc_vcount_read(void) { return g_dc_vscript < 0 ? bus8(((u32)g_cpu.DB << 16) + 0x213D) : dc_vscript_read(); }

/* ---- calls into routines that are not decompiled yet ------------------ */
/* JSR/JSL to `target` as the original instruction at `site` (its address)
 * would: pushes the same return address, runs the routine (generated,
 * decompiled or interpreted) and comes back here. */
void dc_jsr(u32 target, u32 site);
void dc_jsl(u32 target, u32 site);
/* The same for a callee that may have to finish in the interpreter: under
 * the interpreted scheduler a compiled body that reaches a yield point (the
 * sound driver C7:0004 always does) unwinds to the scheduler instead of
 * returning. Returns 1 then: the caller must return straight away (no more
 * guest work), and the routine's dc_leave hands the unwind on, so the
 * scheduler finishes the callee and the ORIGINAL code after `site`. Only for
 * a site where the guest state (stack, registers the code after it reads,
 * memory) is the original's, and only from the body of the DC_ENTRY that
 * owns the site (not from code reached by a direct glue call). */
int dc_jsr_y(u32 target, u32 site);
int dc_jsl_y(u32 target, u32 site);
/* Push/pull bytes on the 65816 stack (for routines whose callers or
 * callees use the stack contents). */
void dc_push8(u8 v);
void dc_push16(u16 v);
u8   dc_pull8(void);
u16  dc_pull16(void);

/* ---- glue -------------------------------------------------------------- */
/* mc0: master cycles at entry; dc_leave tops the routine up to the original's
 * measured cost (dc_cost.inc), counting what callees and dc_spend already charged. */
/* check mode: check_fd/pid = the reference child; spare_* = a child kept at the entry
 * state for timing-dependent routines (dc_rt.c s_timed), to replay the decomp. */
typedef struct DcFrame { u32 pc; u16 entry_s; u8 hrv; u8 size; int check_fd; uint64_t mc0; int pid, spare_pid, spare_w, spare_r; } DcFrame;
void dc_enter(DcFrame *f);           /* check mode: fork the reference */
RecompReturn dc_leave(DcFrame *f);   /* RTS/RTL with the caller's frame */
RecompReturn dc_run_original(DcFrame *f);   /* CT_DECOMP_ORIGINAL: the ROM code instead */
RecompReturn dc_resume_original(DcFrame *f, u32 pc24);   /* finish a call in the interpreter from pc24 */
void dc_frames_passed(void);   /* check mode: host frames passed in a callee; drop the running check */
int dc_check_mode(void);

/* DC_ENTRY(C258B2, RTS) { glue } defines dc_C258B2, the hle_func target.
 * ret is RTS (2-byte frame) or RTL (3-byte frame). Other decompiled C calls
 * the glue directly as dc_C258B2_glue(); that call goes through the CT_PROF
 * entry hook like dc_enter does (one test of a NULL pointer when it is off),
 * so prof.txt counts routines run only from C. */
#define DC_RTS 2
#define DC_RTL 3
void dc_register(u32 pc, RecompReturn (*fn)(CpuState *));
extern void (*g_func_entry_hook)(uint32_t pc24);
#define DC_ENTRY(pc, ret) \
    static void dc_##pc##_body(void); \
    static inline __attribute__((unused)) void dc_##pc##_glue(void) \
    { \
        if (__builtin_expect(g_func_entry_hook != NULL, 0)) g_func_entry_hook(0x##pc); \
        dc_##pc##_body(); \
    } \
    RecompReturn dc_##pc(CpuState *cpu); \
    __attribute__((constructor)) static void dc_##pc##_reg(void) { dc_register(0x##pc, dc_##pc); } \
    RecompReturn dc_##pc(CpuState *cpu) { \
        (void)cpu; \
        DcFrame f = { 0x##pc, g_cpu.S, g_cpu.host_return_valid, DC_##ret, -1, g_cpu.master_cycles, -1, -1, -1, -1 }; \
        dc_enter(&f); \
        if (f.check_fd == -2) return dc_run_original(&f); \
        dc_##pc##_body(); \
        return dc_leave(&f); \
    } \
    static void dc_##pc##_body(void)

#endif
