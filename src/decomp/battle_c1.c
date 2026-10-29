/* Bank C1: the battle engine (turn flow, commands, battle menus). The
 * battle code runs with an 8-bit accumulator; index width varies. */
#include "ram.h"

static void lda8(u8 v) { dc_set_a8(v); dc_set_flags_nz8(v); }
static void lda16(u16 v) { g_cpu.A = v; dc_set_flags_nz16(v); }
/* LDX/LDY in the current index width. */
static void ldx(u16 v)
{
    if (g_cpu.x_flag) { g_cpu.X = v & 0xFF; dc_set_flags_nz8((u8)v); }
    else { g_cpu.X = v; dc_set_flags_nz16(v); }
}
static void ldy(u16 v)
{
    if (g_cpu.x_flag) { g_cpu.Y = v & 0xFF; dc_set_flags_nz8((u8)v); }
    else { g_cpu.Y = v; dc_set_flags_nz16(v); }
}
static u16 abs_x(u16 a) { return g_cpu.x_flag ? abs8(a) : abs16(a); }
static u16 dp_x(u8 o) { return g_cpu.x_flag ? dp8(o) : dp16(o); }
static void stx_dp(u8 o) { if (g_cpu.x_flag) dp8_set(o, (u8)g_cpu.X); else dp16_set(o, g_cpu.X); }
static void tdc(void) { g_cpu.A = g_cpu.D; dc_set_flags_nz16(g_cpu.D); }
/* TAX/TAY take B too when the index is 16-bit. */
static void tax(void) { ldx(g_cpu.A); }
static void tay(void) { ldy(g_cpu.A); }
static void inx(void) { ldx((u16)(g_cpu.X + 1)); }
static void dex(void) { ldx((u16)(g_cpu.X - 1)); }
static void cpx(u16 v) { if (g_cpu.x_flag) dc_cmp8((u8)g_cpu.X, (u8)v); else dc_cmp16(g_cpu.X, v); }
static void inc_abs(u16 a) { u8 v = (u8)(abs8(a) + 1); abs8_set(a, v); dc_set_flags_nz8(v); }
static void dec_abs(u16 a) { u8 v = (u8)(abs8(a) - 1); abs8_set(a, v); dc_set_flags_nz8(v); }
static u8 asl8(u8 v) { g_cpu._flag_C = v >> 7; v = (u8)(v << 1); dc_set_flags_nz8(v); return v; }
static void php(void) { dc_sync_p(); dc_push8(g_cpu.P); }
static void plp(void)
{
    g_cpu.P = dc_pull8();
    cpu_p_to_mirrors(&g_cpu);
    if (g_cpu.x_flag) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
}
static void phx(void) { if (g_cpu.x_flag) dc_push8((u8)g_cpu.X); else dc_push16(g_cpu.X); }
static void plx(void) { ldx(g_cpu.x_flag ? dc_pull8() : dc_pull16()); }
static void stx_abs(u16 a) { if (g_cpu.x_flag) abs8_set(a, (u8)g_cpu.X); else abs16_set(a, g_cpu.X); }
static void sty_abs(u16 a) { if (g_cpu.x_flag) abs8_set(a, (u8)g_cpu.Y); else abs16_set(a, g_cpu.Y); }
static void cpy(u16 v) { if (g_cpu.x_flag) dc_cmp8((u8)g_cpu.Y, (u8)v); else dc_cmp16(g_cpu.Y, v); }
static void iny(void) { ldy((u16)(g_cpu.Y + 1)); }
/* [dp] long pointer. */
static u32 lp(u8 o) { return dp16(o) | (u32)dp8((u8)(o + 2)) << 16; }
static void sta_all(const u16 *a, unsigned n) { for (unsigned k = 0; k < n; k++) abs8_set(a[k], (u8)g_cpu.A); }
/* A - n with the carry set (SEC; SBC #n). */
static void sub_a(u8 n) { g_cpu._flag_C = 1; dc_set_a8(dc_sbc8((u8)g_cpu.A, n)); }

/* Shifts of A in either accumulator width (the multiply/divide-by-2^n stubs at 010D..011E). */
static void asl_a(int n)
{
    for (int k = 0; k < n; k++) {
        if (g_cpu.m_flag) { u8 v = (u8)g_cpu.A; g_cpu._flag_C = v >> 7; dc_set_a8((u8)(v << 1)); dc_set_flags_nz8((u8)(v << 1)); }
        else { g_cpu._flag_C = g_cpu.A >> 15; g_cpu.A = (u16)(g_cpu.A << 1); dc_set_flags_nz16(g_cpu.A); }
    }
}
static void lsr_a(int n)
{
    for (int k = 0; k < n; k++) {
        if (g_cpu.m_flag) { u8 v = (u8)g_cpu.A; g_cpu._flag_C = v & 1; dc_set_a8(v >> 1); dc_set_flags_nz8(v >> 1); }
        else { g_cpu._flag_C = g_cpu.A & 1; g_cpu.A >>= 1; dc_set_flags_nz16(g_cpu.A); }
    }
}
DC_ENTRY(C1010D, RTS) { asl_a(8); }
DC_ENTRY(C10111, RTS) { asl_a(4); }
DC_ENTRY(C10112, RTS) { asl_a(3); }
DC_ENTRY(C10116, RTS) { lsr_a(8); }
DC_ENTRY(C10118, RTS) { lsr_a(6); }
DC_ENTRY(C10119, RTS) { lsr_a(5); }
DC_ENTRY(C1011A, RTS) { lsr_a(4); }
DC_ENTRY(C1011B, RTS) { lsr_a(3); }

/* ---- bare returns -------------------------------------------------------- */
DC_ENTRY(C10A87, RTS) {}
DC_ENTRY(C134A6, RTS) {}
DC_ENTRY(C1475A, RTS) {}
DC_ENTRY(C1767D, RTS) {}
DC_ENTRY(C19980, RTS) {}
DC_ENTRY(C1A3F6, RTS) {}
DC_ENTRY(C1EDB9, RTS) {}
DC_ENTRY(C1FDAD, RTS) {}
DC_ENTRY(C1BC60, RTS) {}

/* ---- hardware multiply / divide ------------------------------------------ */
/* C1:0089  dp AF = AD * AE; 77/78 keep the factors. */
DC_ENTRY(C10089, RTS)
{
    u8 a = dp8(0xAD), b = dp8(0xAE);
    dp8_set(0x77, a);
    bus8_set(0x004202, a);
    dp8_set(0x78, b);
    bus8_set(0x004203, b);
    dc_set_mx(0, g_cpu.x_flag);
    dc_spend(16);
    dp16_set(0xAF, bus16(0x004216));
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:00D7  dp B5 = (B2:B1) / B3, B7 = remainder; 79..7B keep the operands.
 * A029 is set while the divider is busy. */
DC_ENTRY(C100D7, RTS)
{
    inc_abs(0xA029);
    u8 lo = dp8(0xB1), hi = dp8(0xB2), d = dp8(0xB3);
    dp8_set(0x79, lo);
    bus8_set(0x004204, lo);
    dp8_set(0x7A, hi);
    bus8_set(0x004205, hi);
    dp8_set(0x7B, d);
    bus8_set(0x004206, d);
    dc_set_mx(0, g_cpu.x_flag);
    dc_spend(6 * 12);
    dp16_set(0xB5, bus16(0x004214));
    dp16_set(0xB7, bus16(0x004216));
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0xA029, 0);
}

/* ---- small setters -------------------------------------------------------- */
DC_ENTRY(C1BD6B, RTS) { abs8_set(0xAF22, 0); }
DC_ENTRY(C14CF0, RTS) { ldx(abs_x(0xA010)); abs8_set(0x5DAC + g_cpu.X, 0); }
DC_ENTRY(C199B8, RTS) { lda8(0x02); abs8_set(0xB3B8, 0x02); }
DC_ENTRY(C1D1B5, RTS) { lda8(abs8(0xAEF1)); dp8_set(0x1A, (u8)g_cpu.A); }
DC_ENTRY(C120D6, RTS)
{
    lda8(abs8(0x95D5));
    abs8_set(0x99C0, (u8)g_cpu.A);
    abs8_set(0xA62D, (u8)g_cpu.A);
}
DC_ENTRY(C177D1, RTS) { lda8(0x80); abs8_set(0xA2B0, 0x80); abs8_set(0xA2B2, 0x80); }
DC_ENTRY(C1DB57, RTS) { abs8_set(0xAD9B, 0); dec_abs(0xB2C7); }
DC_ENTRY(C1EB0E, RTS)
{
    tdc();
    lda8(abs8(0xAEED)); abs8_set(0xAE4D, (u8)g_cpu.A);
    lda8(abs8(0xAEEE)); abs8_set(0xAE4E, (u8)g_cpu.A);
}
DC_ENTRY(C1B70E, RTS)
{
    static const u16 a81[] = { 0xB188, 0xB189, 0xB18A }, a41[] = { 0x93EE, 0x93F5, 0x93FC };
    lda8(0x81); sta_all(a81, 3);
    lda8(0x41); sta_all(a41, 3);
}
DC_ENTRY(C1ACF2, RTS)
{
    tdc();
    abs8_set(0xAE90, (u8)g_cpu.A);
    lda8(abs8(0xB18B)); abs8_set(0xAE91, (u8)g_cpu.A);
    lda8(abs8(0xAEE3)); abs8_set(0xAE92, (u8)g_cpu.A);
    lda8(abs8(0xAEE4)); abs8_set(0xAE93, (u8)g_cpu.A);
}
DC_ENTRY(C10EBA, RTS)
{
    static const u16 a[] = { 0xA6E3, 0xA6E5, 0xA723, 0xA725, 0xA763, 0xA765,
                             0xA7A3, 0xA7A5, 0xA7E3, 0xA7E5, 0xA823, 0xA825 };
    lda8(0xFF);
    sta_all(a, 12);
}
DC_ENTRY(C1A42E, RTS)
{
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8(abs8(0xB252), 3));
    abs8_set(0xAECC, (u8)g_cpu.A);
    lda8(0x01);
    abs8_set(0xAECB, 1);
}
DC_ENTRY(C1A43D, RTS)
{
    tdc();
    lda8(abs8(0xB252));
    dc_set_a8(asl8(asl8((u8)g_cpu.A)));
    tax();
    lda8(abs8(0xB19E + g_cpu.X) & 0x0F);
    abs8_set(0xAECC, (u8)g_cpu.A);
    lda8(0x01);
    abs8_set(0xAECB, 1);
}

/* 9604..9608 from dp bytes (a negative source stores 0). */
static void args_9604(int s0, int s1, int s2, int s3, int s4)
{
    int s[5] = { s0, s1, s2, s3, s4 };
    for (int k = 0; k < 5; k++) {
        if (s[k] < 0) { abs8_set(0x9604 + k, 0); continue; }
        lda8(dp8((u8)s[k]));
        abs8_set(0x9604 + k, (u8)g_cpu.A);
    }
}
DC_ENTRY(C1C78D, RTS) { args_9604(0x0A, 0x00, 0x06, 0x08, 0x0E); }
DC_ENTRY(C1C7A7, RTS) { args_9604(0x0A, 0x06, -1, 0x08, -1); }
DC_ENTRY(C1C7BD, RTS) { args_9604(0x0A, 0x08, -1, -1, -1); }
DC_ENTRY(C1C7D1, RTS) { args_9604(0x00, 0x06, 0x02, -1, -1); }
DC_ENTRY(C1C7E7, RTS) { args_9604(0x0A, 0x02, -1, 0x08, -1); }
DC_ENTRY(C1C7FD, RTS) { args_9604(0x0A, 0x0C, -1, 0x08, -1); }
DC_ENTRY(C1C813, RTS) { args_9604(0x0A, 0x0C, 0x06, 0x08, 0x0E); }

/* X = D, stored to the given dp words (clearing them). */
static void clear_dp_x(const u8 *o, unsigned n)
{
    tdc();
    tax();
    for (unsigned k = 0; k < n; k++) stx_dp(o[k]);
}
static void lda_sta_dp(u32 src, u8 o) { lda8(abs8(src)); dp8_set(o, (u8)g_cpu.A); }
DC_ENTRY(C1D14F, RTS)
{
    static const u8 o[] = { 0x16, 0x18 };
    clear_dp_x(o, 2);
    lda_sta_dp(0xAEE7, 0x16);
    lda_sta_dp(0xAEE8, 0x18);
}
DC_ENTRY(C1D1A4, RTS)
{
    static const u8 o[] = { 0x16, 0x18 };
    clear_dp_x(o, 2);
    lda_sta_dp(0xAEEB, 0x16);
    lda_sta_dp(0xAEEC, 0x18);
}
DC_ENTRY(C1D160, RTS)
{
    static const u8 o[] = { 0x16, 0x18 };
    clear_dp_x(o, 2);
    lda_sta_dp(0xAEF0, 0x16);
    ldx(abs_x(0xB1F6));
    lda_sta_dp(0x5E46 + g_cpu.X, 0x18);
}
DC_ENTRY(C1D1BB, RTS)
{
    static const u8 o[] = { 0x1E, 0x20 };
    clear_dp_x(o, 2);
    ldx(abs_x(0xB1F4));
    lda_sta_dp(0x5E73 + g_cpu.X, 0x20);
    lda_sta_dp(0x5E74 + g_cpu.X, 0x1E);
}
DC_ENTRY(C1D132, RTS)
{
    static const u8 o[] = { 0x16, 0x18, 0x1A };
    clear_dp_x(o, 3);
    ldx(abs_x(0xB1F4));
    lda_sta_dp(0x5E66 + g_cpu.X, 0x16);
    lda8(abs8(0xAEE7) & 0x1F);
    dp8_set(0x18, (u8)g_cpu.A);
    lda_sta_dp(0xAEE8, 0x1A);
}
/* X = B18C * 4 (with D's high byte). */
static void x_b18c4(void)
{
    lda8(abs8(0xB18C));
    dc_set_a8(asl8(asl8((u8)g_cpu.A)));
    tax();
}
DC_ENTRY(C1D1F9, RTS)
{
    static const u8 o[] = { 0x16 };
    clear_dp_x(o, 1);
    x_b18c4();
    lda8(bus8(0xCC05CF + g_cpu.X));
    dp8_set(0x16, (u8)g_cpu.A);
}
DC_ENTRY(C1D20A, RTS)
{
    static const u8 o[] = { 0x16 };
    clear_dp_x(o, 1);
    x_b18c4();
    lda8(bus8(0xCC05CD + g_cpu.X));
    dp8_set(0x16, (u8)g_cpu.A);
    lda8(bus8(0xCC05CE + g_cpu.X));
    dp8_set(0x18, (u8)g_cpu.A);
}

/* C1:3191 ..  Starts slot $94's effect: timer d0 and the speed byte from CC:xxxx. */
static void effect_start(u8 d0, u32 src)
{
    ldx(dp_x(0x94));
    inc_abs(0xA48E + g_cpu.X);
    abs8_set(0xA499 + g_cpu.X, 0);
    lda8(0xFF); abs8_set(0xA4DB + g_cpu.X, 0xFF);
    lda8(d0);   abs8_set(0xA4D0 + g_cpu.X, d0);
    lda8(0x01); abs8_set(0xA4BA + g_cpu.X, 0x01);
    lda8(bus8(src));
    abs8_set(0xA4C5 + g_cpu.X, (u8)g_cpu.A);
}
DC_ENTRY(C13191, RTS) { effect_start(0x10, 0xCCF5FD); }

/* ---- A2B0/A2B2/A2B4: positions of the current target ---------------------- */
DC_ENTRY(C17639, RTS)
{
    lda8(abs8(0xA014 + g_cpu.Y));
    tax();
    lda8(abs8(0x1D0C + g_cpu.X)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); abs8_set(0xA2B2, (u8)g_cpu.A);
}
DC_ENTRY(C175D7, RTS)
{
    ldx(g_cpu.Y);
    lda8(abs8(0xA014 + g_cpu.X));
    tax();
    lda8(abs8(0x96DB + g_cpu.X));
    abs8_set(0xA2B4, (u8)g_cpu.A);
}
DC_ENTRY(C1760A, RTS)
{
    lda8((u8)g_cpu.Y);
    sub_a(0x14);
    abs8_set(0xA2B4, (u8)g_cpu.A);
}
DC_ENTRY(C177DA, RTS)
{
    lda8((u8)g_cpu.Y);
    sub_a(0x19);
    tax();
    lda8(abs8(0x99AF + g_cpu.X)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x99B2 + g_cpu.X)); abs8_set(0xA2B2, (u8)g_cpu.A);
}
DC_ENTRY(C17901, RTS)
{
    lda8((u8)g_cpu.Y);
    sub_a(0x2A);
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    lda8(abs8(0x5D80 + g_cpu.X)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x5D81 + g_cpu.X)); abs8_set(0xA2B2, (u8)g_cpu.A);
}

/* ---- 99C0 / A62D lists ------------------------------------------------------ */
DC_ENTRY(C127D9, RTS)
{
    lda8(0xFF);
    for (int x = 0x0B; x >= 0; x--) { abs8_set(0x99C0 + x, 0xFF); abs8_set(0xA62D + x, 0xFF); }
    ldx(0xFFFF);
}
DC_ENTRY(C127E8, RTS)
{
    lda8(0x80);
    abs8_set(0x960C, 0x80);
    for (int x = 0x0A; x >= 0; x--) { lda8(abs8(0x99C0 + x)); abs8_set(0xA62D + x, (u8)g_cpu.A); }
    ldx(0xFFFF);
}
/* First used entry of 99C0 (X = 0B when none). */
DC_ENTRY(C1282D, RTS)
{
    tdc();
    tax();
    for (;;) {
        lda8(abs8(0x99C0 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) return;
        inx();
        cpx(0x000B);
        if (g_cpu._flag_Z) return;
    }
}
/* Drops the first entry of 99C0 when it is used (bit 7 set). */
DC_ENTRY(C127C5, RTS)
{
    lda8(abs8(0x99C0));
    if (!(g_cpu.A & 0x80)) return;
    tdc();
    tax();
    do {
        lda8(abs8(0x99C1 + g_cpu.X));
        abs8_set(0x99C0 + g_cpu.X, (u8)g_cpu.A);
        inx();
        cpx(0x000B);
    } while (!g_cpu._flag_Z);
}
DC_ENTRY(C1AC46, RTS)
{
    tdc();
    tax();
    lda8(0xFF);
    do {
        abs8_set(0xAECC + g_cpu.X, 0xFF);
        abs8_set(0xAD8E + g_cpu.X, 0xFF);
        inx();
        cpx(0x000B);
    } while (!g_cpu._flag_C);
}
/* C1:0BBF  Copies 94A0,Y.. (up to Y = 12) to 9A29,X with a 2D byte after each. */
DC_ENTRY(C10BBF, RTS)
{
    do {
        lda8(abs8(0x94A0 + g_cpu.Y));
        abs8_set(0x9A29 + g_cpu.X, (u8)g_cpu.A);
        inx();
        lda8(0x2D);
        abs8_set(0x9A29 + g_cpu.X, 0x2D);
        inx();
        ldy((u16)(g_cpu.Y + 1));
        dc_cmp16(g_cpu.Y, 0x0012);
    } while (!g_cpu._flag_Z);
}
/* C1:1C3A  Copies 180 bytes from D1:5800 to 0B40. */
DC_ENTRY(C11C3A, RTS)
{
    tdc();
    tax();
    do {
        lda8(bus8(0xD15800 + g_cpu.X));
        abs8_set(0x0B40 + g_cpu.X, (u8)g_cpu.A);
        inx();
        cpx(0x0180);
    } while (!g_cpu._flag_Z);
}

/* ---- comparisons --------------------------------------------------------- */
/* C1:2BDA / 2BEE  DEC 9872 unless the first object's coordinate is below the second's. */
static void cmp_pair(u16 tab)
{
    lda8(abs8(0x986F)); tax();
    lda8(abs8(0x9870)); tay();
    lda8(abs8(tab + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, abs8(tab + g_cpu.Y));
    if (!g_cpu._flag_C) return;
    dec_abs(0x9872);
}
DC_ENTRY(C12BDA, RTS) { cmp_pair(0x1D23); }
DC_ENTRY(C12BEE, RTS) { cmp_pair(0x1D0C); }
/* C1:103E  A = A0D1 of the entry A6D9[$80] (Z: none / zero). */
DC_ENTRY(C1103E, RTS)
{
    lda8(dp8(0x80));
    tax();
    lda8(abs8(0xA6D9 + g_cpu.X));
    if (g_cpu.A & 0x80) return;
    tax();
    lda8(abs8(0xA0D1 + g_cpu.X));
    if (!g_cpu._flag_Z) return;
    tdc();
}

/* ---- battle script commands (pointer at [$E6]) ---------------------------- */
/* C1:75BB  The script pointer E6/E8 moves on by A (16-bit, B included). */
static void script_advance(void)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    dp16_set(0xE6, dc_adc16(g_cpu.A, dp16(0xE6)));
    dp16_set(0xE8, dc_adc16(dp16(0xE8), 0));
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
static u8 script_byte(void) { iny(); return bus8(lp(0xE6) + g_cpu.Y); }
static void op_done(u8 len) { inc_abs(0xA00F); lda8(len); script_advance(); }
DC_ENTRY(C158DC, RTS) { lda8(0x01); abs8_set(0xCFFF, 0x01); script_advance(); }
DC_ENTRY(C158E4, RTS) { abs8_set(0xCFFF, 0); lda8(0x01); script_advance(); }
DC_ENTRY(C15ABD, RTS) { op_done(1); }
DC_ENTRY(C15FD3, RTS) { ldx(abs_x(0xA2B6)); abs8_set(0xA1D8 + g_cpu.X, 0); lda8(0x01); script_advance(); }
DC_ENTRY(C15940, RTS) { inc_abs(0x5D9C); op_done(1); }
DC_ENTRY(C1594B, RTS) { inc_abs(0x5D9D); op_done(1); }
DC_ENTRY(C15714, RTS)
{
    iny();
    ldx(abs_x(0xA010));
    lda8(bus8(lp(0xE6) + g_cpu.Y));
    abs8_set(0x5DFD + g_cpu.X, (u8)g_cpu.A);
    lda8(0x02);
    script_advance();
}
DC_ENTRY(C1590C, RTS) { lda8(script_byte()); abs8_set(0x5D9D, (u8)g_cpu.A); op_done(2); }
DC_ENTRY(C15A99, RTS)                            /* swaps 5D9C and 5D9D */
{
    u8 a = abs8(0x5D9C), b = abs8(0x5D9D);
    abs8_set(0x5D9C, b);
    lda8(a);
    abs8_set(0x5D9D, a);
    op_done(1);
}
DC_ENTRY(C158EC, RTS)
{
    lda8(script_byte()); tax();
    lda8(script_byte());
    abs8_set(0x5D80 + g_cpu.X, (u8)g_cpu.A);
    op_done(3);
}
DC_ENTRY(C1597B, RTS)
{
    lda8(script_byte()); tax();
    iny();
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8(bus8(lp(0xE6) + g_cpu.Y), abs8(0x5D80 + g_cpu.X)));
    abs8_set(0x5D80 + g_cpu.X, (u8)g_cpu.A);
    op_done(3);
}
/* C1:56FB  Jumps to the saved script position A569/A56B of slot A00E. */
DC_ENTRY(C156FB, RTS)
{
    lda8(abs8(0xA00E)); tay();
    dp16_set(0xE6, abs16(0xA569 + g_cpu.Y));
    dp16_set(0xE8, abs16(0xA56B + g_cpu.Y));
    tdc();
    inc_abs(0xA00F);
    script_advance();
}
/* Skips the 2-byte command unless its operand equals the variable. */
static void op_if_eq(u16 var)
{
    lda8(script_byte());
    dc_cmp8((u8)g_cpu.A, abs8(var));
    if (g_cpu._flag_Z) { inc_abs(0xA00F); lda8(0x02); }
    else tdc();
    script_advance();
}
DC_ENTRY(C15743, RTS) { op_if_eq(0x5D9C); }
DC_ENTRY(C15756, RTS) { op_if_eq(0x5D9D); }
DC_ENTRY(C14CF7, RTS)
{
    lda8(abs8(0xA3D1));
    if (g_cpu._flag_Z) { inc_abs(0x9876); return; }
    tdc();
    script_advance();
}

/* ---- more comparisons on 1D0C / 1D23 -------------------------------------- */
/* DEC 9872 when (coordinate of 986F) / 16 is below k (or not below, when `ge`). */
static void cmp_half(u16 tab, u8 k, int ge)
{
    lda8(abs8(0x986F)); tax();
    lda8(abs8(tab + g_cpu.X));
    lsr_a(4);
    dc_cmp8((u8)g_cpu.A, k);
    if (g_cpu._flag_C == ge) dec_abs(0x9872);
}
DC_ENTRY(C12CA7, RTS) { cmp_half(0x1D23, 0x08, 0); }
DC_ENTRY(C12CBA, RTS) { cmp_half(0x1D23, 0x08, 1); }
DC_ENTRY(C12CCD, RTS) { cmp_half(0x1D0C, 0x0B, 0); }
DC_ENTRY(C12CE0, RTS) { cmp_half(0x1D0C, 0x05, 1); }
/* DEC 9872 when the two objects are more than 20 apart vertically. */
DC_ENTRY(C12BBC, RTS)
{
    lda8(abs8(0x986F)); tax();
    lda8(abs8(0x9870)); tay();
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(abs8(0x1D23 + g_cpu.X), abs8(0x1D23 + g_cpu.Y)));
    if (g_cpu._flag_N) lda8((u8)((g_cpu.A ^ 0xFF) + 1));
    dc_cmp8((u8)g_cpu.A, 0x20);
    if (g_cpu._flag_Z || !g_cpu._flag_C) return;
    dec_abs(0x9872);
}

/* ---- target / menu helpers ------------------------------------------------ */
DC_ENTRY(C1DE87, RTS)
{
    lda8(abs8(0xB1FD));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_C) {
        lda8(dp8(0x16) & dp8(0x18));
        if (!g_cpu._flag_Z) { lda8(0x01); dp8_set(0x16, 1); return; }
    }
    tdc();
    dp8_set(0x16, (u8)g_cpu.A);
}
/* C1:D086  A = 00 when item Y is not more than the 2C53 count and 2C55 is 0, else FF. */
DC_ENTRY(C1D086, RTS)
{
    sty_abs(0xAEF7);
    cpy(0);
    if (!g_cpu._flag_Z) {
        ldx(abs_x(0x2C53));
        cpx(abs_x(0xAEF7));
        if (!g_cpu._flag_C) {
            lda8(abs8(0x2C55));
            if (g_cpu._flag_Z) { lda8(0x00); return; }
        }
    }
    lda8(0xFF);
}
/* C1:CFE9  A = item Y if it is in the inventory list 2400 (00 when not). */
DC_ENTRY(C1CFE9, RTS)
{
    sty_abs(0xAEF7);
    cpy(0);
    if (!g_cpu._flag_Z) {
        ldx(0);
        do {
            lda8(abs8(0x2400 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, abs8(0xAEF7));
            if (g_cpu._flag_Z) return;
            inx();
            cpx(0x0100);
        } while (!g_cpu._flag_C);
    }
    lda8(0x00);
}
/* C1:D056  Uses up one of item Y: A = FF when the last one went, 00 when none. */
DC_ENTRY(C1D056, RTS)
{
    sty_abs(0xAEF7);
    cpy(0);
    if (!g_cpu._flag_Z) {
        ldx(0);
        do {
            lda8(abs8(0x2400 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, abs8(0xAEF7));
            if (g_cpu._flag_Z) {
                lda8(abs8(0x2500 + g_cpu.X));
                if (g_cpu._flag_Z) break;
                lda8((u8)(g_cpu.A - 1));
                abs8_set(0x2500 + g_cpu.X, (u8)g_cpu.A);
                if (!g_cpu._flag_Z) return;
                abs8_set(0x2400 + g_cpu.X, 0);
                lda8(0xFF);
                return;
            }
            inx();
            cpx(0x0100);
        } while (!g_cpu._flag_C);
    }
    lda8(0x00);
}
DC_ENTRY(C1D1D3, RTS)
{
    static const u8 o[] = { 0x16, 0x18, 0x1A };
    clear_dp_x(o, 3);
    tdc();
    x_b18c4();
    lda8(bus8(0xCC05CF + g_cpu.X) & 0x3F); dp8_set(0x16, (u8)g_cpu.A);
    lda8(bus8(0xCC05CE + g_cpu.X) & 0x0F); dp8_set(0x18, (u8)g_cpu.A);
    lda8(bus8(0xCC05CD + g_cpu.X));        dp8_set(0x1A, (u8)g_cpu.A);
}
DC_ENTRY(C1D174, RTS)
{
    static const u8 o[] = { 0x16, 0x18 };
    clear_dp_x(o, 2);
    for (int k = 0; k < 2; k++) {
        if (k) tdc();
        lda8(abs8(k ? 0xAEEA : 0xAEE9));
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.A, abs16(k ? 0xB1F6 : 0xB1F4));
        tax();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x5E2D + g_cpu.X));
        dp8_set(k ? 0x18 : 0x16, (u8)g_cpu.A);
    }
}
DC_ENTRY(C1D508, RTS)
{
    tdc(); tay();
    lda8((u8)(abs8(0xAD8D) - 1));
    tax();
    lda8(abs8(0xAD8E + g_cpu.X));
    dc_set_a8(asl8(asl8((u8)g_cpu.A)));
    tax();
    do {
        lda8(abs8(0xB18E + g_cpu.Y));
        abs8_set(0xB192 + g_cpu.X, (u8)g_cpu.A);
        iny(); inx();
        cpy(0x0004);
    } while (!g_cpu._flag_C);
}
/* Y += 0C as TYA; CLC; ADC; TAY (B rides along into Y). */
static void y_add_0c(void)
{
    lda8((u8)g_cpu.Y);
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.A, 0x0C));
    tay();
}
DC_ENTRY(C1B3BB, RTS)                            /* keeps only bit 6 of the 8 party flags 29C6 */
{
    tdc(); tax(); tay();
    do {
        lda8(abs8(0x29C6 + g_cpu.Y) & 0x40);
        abs8_set(0x29C6 + g_cpu.Y, (u8)g_cpu.A);
        y_add_0c();
        inx();
        cpx(0x0008);
    } while (!g_cpu._flag_C);
}
DC_ENTRY(C1B3D2, RTS)
{
    tdc(); tax(); tay();
    do {
        lda8(abs8(0xAF0D + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xAF15 + g_cpu.X));
            if (!(g_cpu.A & 0x80)) {
                lda8((abs8(0x29C6 + g_cpu.Y) & 0x40) | 0x40);
                abs8_set(0x29C6 + g_cpu.Y, (u8)g_cpu.A);
            }
            else g_cpu._flag_Z = 0;
        }
        y_add_0c();
        inx();
        cpx(0x0008);
    } while (!g_cpu._flag_C);
}
DC_ENTRY(C1B093, RTS)
{
    lda8(abs8(0x5E2F + g_cpu.X) & 0xFE);
    abs8_set(0x5E2F + g_cpu.X, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    u16 v = abs16(0x5E32 + g_cpu.X);
    g_cpu._flag_C = (v >> 2) & 1;
    g_cpu.A = v >> 3;
    dc_cmp16(g_cpu.A, abs16(0x5E30 + g_cpu.X));
    if (g_cpu._flag_C) {
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x5E2F + g_cpu.X) | 0x01);
        abs8_set(0x5E2F + g_cpu.X, (u8)g_cpu.A);
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:A3F7  Lists the used slots of AEFF (00..02) at AECC, count in AECB. */
DC_ENTRY(C1A3F7, RTS)
{
    tdc(); tax(); tay();
    do {
        lda8(abs8(0xAEFF + g_cpu.Y));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            lda8((u8)g_cpu.Y);
            abs8_set(0xAECC + g_cpu.X, (u8)g_cpu.A);
            inx();
        }
        iny();
        cpy(0x0003);
    } while (!g_cpu._flag_Z);
    lda8((u8)g_cpu.X);
    abs8_set(0xAECB, (u8)g_cpu.A);
}
DC_ENTRY(C177EC, RTS)
{
    lda8((u8)g_cpu.Y);
    sub_a(0x1C);
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(abs16(0xA260 + g_cpu.X), abs16(0xA240 + g_cpu.X));
    abs16_set(0xA2B0, g_cpu.A);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(abs16(0xA270 + g_cpu.X), abs16(0xA250 + g_cpu.X));
    abs16_set(0xA2B2, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* A = 8000 >> dp00 (counting dp00 down to 0, 16-bit) ORed into `dst`. */
static void bit_or(u16 dst)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0x8000;
    for (;;) {
        ldx(dp_x(0x00));
        if (g_cpu._flag_Z) break;
        g_cpu._flag_C = g_cpu.A & 1;
        g_cpu.A >>= 1;
        dp16_set(0x00, (u16)(dp16(0x00) - 1));
    }
    lda16(g_cpu.A | abs16(dst));
    abs16_set(dst, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:AD09  AE95 = bit (15 - AECC) when AECB is set and AECC is used. */
DC_ENTRY(C1AD09, RTS)
{
    tdc(); tax();
    stx_abs(0xAE95);
    lda8(abs8(0xAECB));
    if (g_cpu._flag_Z) return;
    lda8(abs8(0xAECC));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    tax();
    stx_dp(0x00);
    bit_or(0xAE95);
}
/* C1:30B6  Clears slot $94's effect state; for party slots copies its 1A-byte
 * record 2100+slot*16 to 2300. */
DC_ENTRY(C130B6, RTS)
{
    ldx(dp_x(0x94));
    static const u16 z[] = { 0xA483, 0xA48E, 0xA457, 0xA46D, 0xA478 };
    for (int k = 0; k < 5; k++) abs8_set(z[k] + g_cpu.X, 0);
    cpx(0x0003);
    if (g_cpu._flag_C) return;
    lda8(abs8(0x993C + g_cpu.X));
    asl_a(4);
    tay();
    ldx(0x001A);
    do {
        lda8(abs8(0x2100 + g_cpu.Y));
        abs8_set(0x2300 + g_cpu.Y, (u8)g_cpu.A);
        iny();
        dex();
    } while (!g_cpu._flag_Z);
}
/* C1:00A7  dp A9..AB = (A8:A7) * A5 (with DB = D's low byte for the multiplier). */
DC_ENTRY(C100A7, RTS)
{
    dc_push8(g_cpu.DB);
    tdc();
    dc_push8((u8)g_cpu.A);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
    u8 a = dp8(0xA5);
    dp8_set(0x77, a); abs8_set(0x4202, a);
    lda8(dp8(0xA7));
    dp8_set(0x78, (u8)g_cpu.A); abs8_set(0x4203, (u8)g_cpu.A);
    php();
    dc_set_a8(dp8(0xA8));
    dp8_set(0xAB, 0);
    g_cpu.X = abs_x(0x4216);
    dp8_set(0x78, (u8)g_cpu.A); abs8_set(0x4203, (u8)g_cpu.A);
    stx_dp(0xA9);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    dp16_set(0xAA, dc_adc16(dp16(0xAA), abs16(0x4216)));
    tdc();
    plp();
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}
/* C1:C90B  2E:2C = 28 * 2A (shift-and-add; 2A is shifted out). */
static void ror_dp16(u8 o)
{
    u16 v = dp16(o);
    u16 r = (u16)(v >> 1 | (u16)g_cpu._flag_C << 15);
    g_cpu._flag_C = v & 1;
    dp16_set(o, r);
    dc_set_flags_nz16(r);
}
DC_ENTRY(C1C90B, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    ldx(0x0010);
    dp16_set(0x2C, 0);
    dp16_set(0x2E, 0);
    do {
        ror_dp16(0x2A);
        if (g_cpu._flag_C) {
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(dp16(0x28), dp16(0x2E));
            dp16_set(0x2E, g_cpu.A);
        }
        ror_dp16(0x2E);
        ror_dp16(0x2C);
        dex();
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(C11B67, RTS)
{
    lda8(abs8(0x95D6));
    if (g_cpu.A & 0x80) return;
    tax();
    abs8_set(0x9F38 + g_cpu.X, 0);
    lda8(abs8(0xA863 + g_cpu.X)); abs8_set(0x95DF + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xA866 + g_cpu.X)); abs8_set(0x95EB + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x95D6)); tax();
    abs8_set(0xA6D9 + g_cpu.X, (u8)g_cpu.A);
    lda8(0xFE); abs8_set(0xA6DF, 0xFE);
    lda8(abs8(0x95D7)); abs8_set(0x95D6, (u8)g_cpu.A);
    lda8(abs8(0x95D8)); abs8_set(0x95D7, (u8)g_cpu.A);
    lda8(abs8(0x95D8)); abs8_set(0x95D9, (u8)g_cpu.A);
    dec_abs(0x95DA);
    inc_abs(0xA6DE);
    lda8(abs8(0xA6DD));
    if (!(g_cpu.A & 0x80)) return;
    lda8((u8)g_cpu.X);
    abs8_set(0xA6DD, (u8)g_cpu.A);
}
/* C1:B725  Battle menu window setup; copies 15 bytes of FD:xxxx (by 29AC) to 93EE. */
DC_ENTRY(C1B725, RTS)
{
    abs8_set(0x99D4, 0);
    lda8(0x01); abs8_set(0x99D8, 1);
    lda8(0x00);
    abs8_set(0x99DD, 0); abs8_set(0x99DE, 0); abs8_set(0x99DF, 0);
    lda8(0x30);
    abs8_set(0x9F22, 0x30); abs8_set(0x9F23, 0x30); abs8_set(0x9F24, 0x30);
    lda8(abs8(0x29AC));
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xFDB957 + g_cpu.X));
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    tay();
    do {
        lda8(bus8(0xFD0000 + g_cpu.X));
        abs8_set(0x93EE + g_cpu.Y, (u8)g_cpu.A);
        inx(); iny();
        cpy(0x0015);
    } while (!g_cpu._flag_C);
}
DC_ENTRY(C108E8, RTS)
{
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    dc_set_mx(0, g_cpu.x_flag);
    u16 m = abs16(0x9F20);
    dc_set_flags_nz16(m);
    if (m == 1) { dc_set_flags_nz16(0); lda16(bus16(0xCCFA23 + g_cpu.X)); }
    else { if (m) dc_set_flags_nz16((u16)(m - 1)); lda16(bus16(0xCCFA29 + g_cpu.X)); }
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(0x29);
    static const u16 a[] = { 0x0CC0, 0x0CC2, 0x0CC4, 0x0CC6, 0x0CC8, 0x0D00, 0x0D02, 0x0D04, 0x0D06, 0x0D08 };
    for (int k = 0; k < 10; k++) abs8_set(a[k] + g_cpu.X, 0x29);
}
/* C1:2136 / 2163  Picks the party slot holding character 5 / 4 (when able
 * to act) as the only target; 9613 = 80 when there is none. */
static void target_char(u8 ch)
{
    lda8(ch);
    dp8_set(0x80, ch);
    inc_abs(0x960A);
    ldx(0x0002);
    do {
        lda8(abs8(0x96F5 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xA09B + g_cpu.X));
            if (g_cpu._flag_Z) {
                lda8(abs8(0x2980 + g_cpu.X));
                dc_cmp8((u8)g_cpu.A, dp8(0x80));
                if (g_cpu._flag_Z) {
                    lda8((u8)g_cpu.X);
                    abs8_set(0x99C0, (u8)g_cpu.A);
                    abs8_set(0xA62D, (u8)g_cpu.A);
                    return;
                }
            }
        }
        dex();
    } while (!g_cpu._flag_N);
    lda8(0x80);
    abs8_set(0x9613, 0x80);
}
DC_ENTRY(C12136, RTS) { target_char(0x05); }
DC_ENTRY(C12163, RTS) { target_char(0x04); }
DC_ENTRY(C1ACCE, RTS)
{
    tdc();
    lda8(abs8(0xAEE5));
    tax();
    static const u8 v[] = { 0x04, 0x04, 0x0D, 0x0D };
    for (int k = 0; k < 4; k++) {
        cpx((u16)(k + 1));
        if (g_cpu._flag_Z) lda8(v[k]);
    }
    dp8_set(0x0E, (u8)g_cpu.A);
}
DC_ENTRY(C1CBF6, RTS)
{
    dc_cmp8((u8)g_cpu.A, 0xA2);
    if (g_cpu._flag_Z) {
        lda8((u8)g_cpu.Y);
        lsr_a(1);
    } else {
        dc_cmp8((u8)g_cpu.A, 0xA3);
        if (!g_cpu._flag_Z) return;
        lda8((u8)g_cpu.Y);
        lsr_a(1);
        dc_set_a8(dc_adc8((u8)g_cpu.A, 0));
        lsr_a(1);
    }
    dc_set_a8(dc_adc8((u8)g_cpu.A, 0));
    tay();
}
DC_ENTRY(C1AC89, RTS)
{
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.A, 0x03));
    lda8((u8)g_cpu.A | abs8(0xB18E));
    abs8_set(0xB18E, (u8)g_cpu.A);
    abs8_set(0xB191, 0);
    lda8(abs8(0xAEE3));
    dc_cmp8((u8)g_cpu.A, 0x00);
    if (g_cpu._flag_Z) abs8_set(0xB18F, 0);
    else {
        dc_cmp8((u8)g_cpu.A, 0x01);
        if (g_cpu._flag_Z) {
            lda8(abs8(0xB18E) | 0x80); abs8_set(0xB18E, (u8)g_cpu.A);
            abs8_set(0xB18F, 0);
            lda8(0x04); abs8_set(0xAEEB, 0x04);
        } else {
            dc_cmp8((u8)g_cpu.A, 0x02);
            if (g_cpu._flag_Z) {
                lda8(abs8(0xB18E) | 0x40); abs8_set(0xB18E, (u8)g_cpu.A);
                lda8(dp8(0x0E)); abs8_set(0xB18F, (u8)g_cpu.A);
            }
        }
    }
    lda8(abs8(0xB18E));
    dp8_set(0x10, (u8)g_cpu.A);
}
DC_ENTRY(C1DB5E, RTS)
{
    tdc();
    ldx(abs_x(0xB1F6));
    lda8(dp8(0x16));
    g_cpu._flag_Z = !(g_cpu.A & 0x80);
    if (!g_cpu._flag_Z) { lda8(abs8(0x5E4A + g_cpu.X) & 0x7F); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A); }
    lda8(abs8(0x5E4A + g_cpu.X) & 0xC0); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
    lda8(0);                             abs8_set(0x5E4B + g_cpu.X, 0);
    lda8(abs8(0x5E4C + g_cpu.X) & 0xC2); abs8_set(0x5E4C + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xB1FD));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_C) return;
    tax();
    static const u16 z[] = { 0xAFB6, 0xB02F, 0xAFC1, 0xAFCC, 0xB00E };
    for (int k = 0; k < 5; k++) abs8_set(z[k] + g_cpu.X, 0);
    lda8(abs8(0xAE51 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_C) return;
    abs8_set(0xAE51 + g_cpu.X, 0);
}
/* C1:7313  AAFE = AAFE / A; then the larger of AAFC / AAFE keeps the difference, the other 0. */
DC_ENTRY(C17313, RTS)
{
    dp8_set(0xB3, (u8)g_cpu.A);
    lda8(abs8(0xAAFE));
    dp8_set(0xB1, (u8)g_cpu.A);
    dp8_set(0xB2, 0);
    dc_jsr(0xC100D7, 0xC1731C);
    ldx(dp_x(0xB5));
    stx_abs(0xAAFE);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc16(abs16(0xAAFC), abs16(0xAAFE));
    if (g_cpu._flag_C) {
        abs16_set(0xAAFE, g_cpu.A);
        tdc();
        abs16_set(0xAAFC, g_cpu.A);
    } else {
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(abs16(0xAAFE), abs16(0xAAFC));
        abs16_set(0xAAFC, g_cpu.A);
        tdc();
        abs16_set(0xAAFE, g_cpu.A);
    }
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:0BD3  Fills 3 entries of 9A29,X and 99E9,X (X = $82) with FF 2D. */
DC_ENTRY(C10BD3, RTS)
{
    for (int t = 0; t < 2; t++) {
        u16 base = t ? 0x99E9 : 0x9A29;
        tdc(); tay();
        ldx(dp_x(0x82));
        do {
            lda8(0xFF); abs8_set(base + g_cpu.X, 0xFF); inx();
            lda8(0x2D); abs8_set(base + g_cpu.X, 0x2D); inx();
            iny();
            cpy(0x0003);
        } while (!g_cpu._flag_Z);
    }
}
/* C1:0174  9499 (16-bit, < 100 assumed) to two digit tiles 949E/949F from
 * CC:F903, with 949C/949D = FF.  9499 is left at value - 10 * (tens + 1). */
DC_ENTRY(C10174, RTS)
{
    phx();
    tdc(); tax();
    do {
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(abs16(0x9499), 0x000A);
        abs16_set(0x9499, g_cpu.A);
        inx();
    } while (g_cpu._flag_C);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(abs16(0x9499), 0x000A);
    dex();
    phx();
    dc_set_mx(1, g_cpu.x_flag);
    tax();
    lda8(bus8(0xCCF903 + g_cpu.X)); abs8_set(0x949F, (u8)g_cpu.A);
    plx();
    lda8(bus8(0xCCF903 + g_cpu.X)); abs8_set(0x949E, (u8)g_cpu.A);
    lda8(0xFF); abs8_set(0x949D, 0xFF); abs8_set(0x949C, 0xFF);
    plx();
}
/* After the reset at 30B6. */
static void effect_restart(u8 d0, u32 src) { dc_C130B6_glue(); effect_start(d0, src); }
DC_ENTRY(C13141, RTS) { effect_restart(0x70, 0xCCF603); }
DC_ENTRY(C13163, RTS) { effect_restart(0x60, 0xCCF602); }
DC_ENTRY(C131B0, RTS) { effect_restart(0x30, 0xCCF5FF); }
DC_ENTRY(C131D2, RTS) { effect_restart(0x40, 0xCCF600); }
DC_ENTRY(C131F4, RTS) { effect_restart(0x50, 0xCCF601); }
DC_ENTRY(C13185, RTS)
{
    dc_C130B6_glue();
    ldx(dp_x(0x94));
    inc_abs(0xA46D + g_cpu.X);
    abs8_set(0xA499 + g_cpu.X, 0);
}

/* ---- products through C90B (2E:2C = 28 * 2A) ------------------------------- */
DC_ENTRY(C1FDBF, RTL) { dc_C1C90B_glue(); }
static void mul_args(u16 a28, u16 a2A)
{
    ldx(a28); stx_dp(0x28);
    ldx(a2A); stx_dp(0x2A);
    dc_C1C90B_glue();
}
/* X = A (with B), then LDA #n; TAX (B kept). */
static u16 a_with(u8 n) { lda8(n); return g_cpu.A; }
DC_ENTRY(C1E9A3, RTS)                            /* B1F4 = B18B * 80 */
{
    tdc();
    lda8(abs8(0xB18B));
    tax(); stx_dp(0x2A);
    ldx(a_with(0x80)); stx_dp(0x28);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C));
    stx_abs(0xB1F4);
}
DC_ENTRY(C1E9B8, RTS)                            /* B1F6 = (party slot) * 80 */
{
    tdc();
    lda8((u8)(abs8(0xAD8D) - 1));
    tax();
    lda8(abs8(0xAD8E + g_cpu.X));
    abs8_set(0xB1FD, (u8)g_cpu.A);
    tax(); stx_dp(0x2A);
    ldx(a_with(0x80)); stx_dp(0x28);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C));
    stx_abs(0xB1F6);
}
/* A * 0B as an index into the CC:1BEB records. */
static void rec_0b(void)
{
    tax(); stx_dp(0x28);
    ldx(a_with(0x0B)); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C));
}
static void rec_name(u32 field)
{
    rec_0b();
    lda8(bus8(field + g_cpu.X) & 0x7F);
    tax();
    lda8(bus8(0xCC253B + g_cpu.X));
}
DC_ENTRY(C1CB48, RTS) { rec_name(0xCC1BF0); }
DC_ENTRY(C1CB61, RTS) { rec_name(0xCC1BF1); }
DC_ENTRY(C1CB7A, RTS) { rec_name(0xCC1BF2); }
DC_ENTRY(C1CB93, RTS)
{
    rec_0b();
    lda8(bus8(0xCC1BEB + g_cpu.X) & 0x7F);
    dp8_set(0x2C, (u8)g_cpu.A);
    dc_set_a8(asl8((u8)g_cpu.A));
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.A, dp8(0x2C)));
    tax();
    for (int k = 0; k < 3; k++) {
        lda8(bus8(0xCC249F + k + g_cpu.X));
        abs8_set(0xB2EA + k, (u8)g_cpu.A);
    }
}
/* dp o = v * 4 + 2C * AD9B (the product's low byte). */
static void slot_ofs(u8 o, u16 v, int swap)
{
    tdc(); tax(); stx_dp(o);
    lda8(abs8(v));
    dc_set_a8(asl8(asl8((u8)g_cpu.A)));
    dp8_set(o, (u8)g_cpu.A);
    lda8(abs8(0xAD9B));
    tax(); stx_dp(swap ? 0x28 : 0x2A);
    ldx(0x002C); stx_dp(swap ? 0x2A : 0x28);
    dc_C1C90B_glue();
    lda8(dp8(0x2C));
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.A, dp8(o)));
    dp8_set(o, (u8)g_cpu.A);
}
DC_ENTRY(C1E89F, RTS) { slot_ofs(0x0E, 0xB1FD, 0); }
DC_ENTRY(C1E8C0, RTS) { slot_ofs(0x10, 0xB18B, 1); }
DC_ENTRY(C1EDDA, RTS)
{
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E47 + g_cpu.X) & dp8(0x1C));
    if (g_cpu._flag_Z) return;
    ldx(abs_x(0xAD89)); stx_dp(0x28);
    lda8(dp8(0x1E));
    tax(); stx_dp(0x2A);
    dc_C1C90B_glue();
    dc_set_mx(0, g_cpu.x_flag);
    u16 v = dp16(0x2C);
    g_cpu._flag_C = v & 1;
    abs16_set(0xAD89, v >> 1);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(C1EDFD, RTS)
{
    tdc(); tax();
    abs8_set(0xAD9A, (u8)g_cpu.A);
    do {
        lda8(abs8(0xAEFF + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) inc_abs(0xAD9A);
        inx();
        cpx(0x0003);
    } while (!g_cpu._flag_C);
    lda8((u8)(abs8(0xAD9A) + 1));
    tax(); stx_dp(0x28);
    ldx(abs_x(0xAD89)); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C));
    stx_abs(0xAD89);
}
/* C1:01F9  A = (sine table C0:F900 of the angle A*4, signed) * $AE, high byte. */
DC_ENTRY(C101F9, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    ldx((u16)(g_cpu.A << 2) & 0x03FF);
    lda16(bus16(0xC0F900 + g_cpu.X) & 0x00FF);
    cpx(0x0200);
    if (g_cpu._flag_C) lda16((u16)((g_cpu.A ^ 0xFFFF) + 1));
    dp16_set(0xA7, g_cpu.A);
    lda16(dp16(0xAE) & 0x00FF);
    dp16_set(0xA5, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
    dc_C100A7_glue();
    lda8(dp8(0xAA));
}

/* ---- target list helpers ----------------------------------------------------- */
DC_ENTRY(C18862, RTS)                            /* clears the 4 bytes B192 + B18B*4 */
{
    tdc(); tay();
    lda8(abs8(0xB18B));
    dc_set_a8(asl8(asl8((u8)g_cpu.A)));
    tax();
    tdc();
    do {
        abs8_set(0xB192 + g_cpu.X, (u8)g_cpu.A);
        inx(); iny();
        cpy(0x0004);
    } while (!g_cpu._flag_C);
}
DC_ENTRY(C1B202, RTS)
{
    lda8((abs8(0x5E4B + g_cpu.X) ^ abs8(0xAE7A + g_cpu.Y)) & 0x04);
    if (!g_cpu._flag_Z) {
        lda8(0xFF); abs8_set(0xB268 + g_cpu.Y, 0xFF);
        lda8(0x00); abs8_set(0xAE7A + g_cpu.Y, 0x00);
    }
    lda8(abs8(0x5E4B + g_cpu.X) & 0x80);
    if (g_cpu._flag_Z) return;
    lda8(0x04);
    abs8_set(0x5E0A + g_cpu.Y, 0x04);
}
DC_ENTRY(C1C8F7, RTS)
{
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    tax();
    lda8(abs8(0xAEFF + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) { lda8((u8)g_cpu.X); return; }
    lda8(abs8(0xAE6D + g_cpu.X));
    tax();
}
DC_ENTRY(C1104E, RTS)                            /* digit tile 73 (blank zero) -> FF */
{
    lda8(abs8(0x949D));
    dc_cmp8((u8)g_cpu.A, 0x73);
    if (g_cpu._flag_Z) { lda8(0xFF); abs8_set(0x949D, 0xFF); }
    lda8(abs8(0x949E));
    dc_cmp8((u8)g_cpu.A, 0x73);
    if (!g_cpu._flag_Z) return;
    lda8(abs8(0x949D));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) return;
    lda8(0xFF);
    abs8_set(0x949E, 0xFF);
}
DC_ENTRY(C1AEFD, RTS)
{
    lda8(abs8(0xB1CF));
    if (g_cpu.A & 0x01) {
        tdc();
        lda8(abs8(0xAECC) & 0x0F);
        abs8_set(0xAED8, (u8)g_cpu.A);
        lda8(0x80);
        abs8_set(0xB1CF, 0x80);
        return;
    }
    lda8(abs8(0xB1CF));
    g_cpu._flag_Z = !(g_cpu.A & 0x80);
    if (g_cpu._flag_Z) return;
    tdc();
    abs8_set(0xB1CF, (u8)g_cpu.A);
}
/* C1:2814 / 27FA  Moves the cursor 9614 to the previous / next used entry of 99C0. */
DC_ENTRY(C12814, RTS)
{
    dc_C1282D_glue();
    if (g_cpu._flag_Z) return;
    do {
        dec_abs(0x9614);
        lda8(abs8(0x9614));
        if (g_cpu._flag_N) { lda8(0x0A); abs8_set(0x9614, 0x0A); }
        tax();
        lda8(abs8(0x99C0 + g_cpu.X));
    } while (g_cpu._flag_N);
}
DC_ENTRY(C127FA, RTS)
{
    dc_C1282D_glue();
    if (g_cpu._flag_Z) return;
    do {
        inc_abs(0x9614);
        lda8(abs8(0x9614));
        dc_cmp8((u8)g_cpu.A, 0x0B);
        if (g_cpu._flag_Z) { tdc(); abs8_set(0x9614, (u8)g_cpu.A); }
        tax();
        lda8(abs8(0x99C0 + g_cpu.X));
    } while (g_cpu._flag_N);
}
DC_ENTRY(C13772, RTS)
{
    ldx(dp_x(0x9C));
    lda8(abs8(0x9829 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    abs8_set(0xA5FD + g_cpu.X, 0);
    lda8(abs8(0x9752 + g_cpu.X));
    tay();
    lda8(abs8(0x1D0C + g_cpu.Y));
    dc_cmp8((u8)g_cpu.A, abs8(0x9831 + g_cpu.X));
    if (g_cpu._flag_Z) {
        lda8(abs8(0x1D23 + g_cpu.Y));
        dc_cmp8((u8)g_cpu.A, abs8(0x9839 + g_cpu.X));
        if (g_cpu._flag_Z) return;
    }
    lda8(abs8(0x5E1D + g_cpu.X));
    abs8_set(0x5E0D + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x9829 + g_cpu.X, 0);
}
DC_ENTRY(C1A411, RTS)
{
    dc_C1A3F7_glue();
    ldy(0x0003);
    do {
        lda8(abs8(0xAEFF + g_cpu.Y));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            lda8((u8)g_cpu.Y);
            abs8_set(0xAECC + g_cpu.X, (u8)g_cpu.A);
            inx();
        }
        iny();
        cpy(0x000B);
    } while (!g_cpu._flag_Z);
    lda8((u8)g_cpu.X);
    abs8_set(0xAECB, (u8)g_cpu.A);
}
/* C1:1115  Four OAM entries at 0700 from CC:F604, placed at object 95D5. */
DC_ENTRY(C11115, RTS)
{
    tdc(); tax();
    lda8(abs8(0x95D5));
    tay();
    do {
        g_cpu._flag_C = 0;
        dc_set_a8(dc_adc8(bus8(0xCCF604 + g_cpu.X), abs8(0x1D0C + g_cpu.Y)));
        abs8_set(0x0700 + g_cpu.X, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        dc_set_a8(dc_adc8(bus8(0xCCF605 + g_cpu.X), abs8(0x1D23 + g_cpu.Y)));
        abs8_set(0x0701 + g_cpu.X, (u8)g_cpu.A);
        lda8(bus8(0xCCF606 + g_cpu.X));
        abs8_set(0x0702 + g_cpu.X, (u8)g_cpu.A);
        lda8((bus8(0xCCF607 + g_cpu.X) & 0xF1) | abs8(0x9F46));
        abs8_set(0x0703 + g_cpu.X, (u8)g_cpu.A);
        inx(); inx(); inx(); inx();
        cpx(0x0010);
    } while (!g_cpu._flag_Z);
    lda8(0xAA);
    abs8_set(0x0900, 0xAA);
}
/* C1:BD80  (entered with Z from the caller: clear halves AFAB,X) Adjusts the
 * ATB speed AFAB,X: doubled (max FF) with status bit 20 of 5E4B,Y, at least 1. */
DC_ENTRY(C1BD80, RTS)
{
    if (!g_cpu._flag_Z) {
        u8 v = abs8(0xAFAB + g_cpu.X);
        g_cpu._flag_C = v & 1;
        lda8(v >> 1);
        abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A);
    }
    lda8(abs8(0x5E4B + g_cpu.Y));
    if (g_cpu.A & 0x20) {
        lda8(asl8(abs8(0xAFAB + g_cpu.X)));
        if (g_cpu._flag_C) lda8(0xFF);
        abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A);
    }
    else g_cpu._flag_Z = 1;
    lda8(abs8(0xAFAB + g_cpu.X));
    if (g_cpu._flag_Z) { lda8((u8)(g_cpu.A + 1)); abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A); }
    cpx(0x0003);
    if (g_cpu._flag_C) return;
    lda8(abs8(0xAFAB + g_cpu.X));
    abs8_set(0x99DD + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x9F22 + g_cpu.X, (u8)g_cpu.A);
}

/* ---- batch 4 --------------------------------------------------------------- */
DC_ENTRY(C118AA, RTS) { dc_C11115_glue(); dp8_set(0xEE, 0); dp8_set(0xEF, 0); }
/* For the 3 party slots ($16): B18B = slot, B1F4 = slot * 80, then clear. */
DC_ENTRY(C1EAE8, RTS)
{
    tdc(); tax(); stx_dp(0x16);
    do {
        lda8(dp8(0x16));
        abs8_set(0xB18B, (u8)g_cpu.A);
        dc_C1E9A3_glue();
        ldx(abs_x(0xB1F4));
        tdc();
        abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
        dp8_set(0x16, (u8)(dp8(0x16) + 1));
        lda8(dp8(0x16));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_C);
}
/* Clears the status bytes of the 3 party members (keeps bit 7 of 5E4A, clears bit 0 of 5E2F). */
DC_ENTRY(C1EA9D, RTS)
{
    tdc(); tax(); stx_dp(0x16);
    do {
        lda8(dp8(0x16));
        abs8_set(0xB18B, (u8)g_cpu.A);
        dc_C1E9A3_glue();
        ldx(abs_x(0xB1F4));
        tdc();
        for (int k = 0; k < 9; k++) abs8_set(0x5E4B + k + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x5E4A + g_cpu.X) & 0x80); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x5E2F + g_cpu.X) & 0xFE); abs8_set(0x5E2F + g_cpu.X, (u8)g_cpu.A);
        lda8(dp8(0x16));
        tax();
        tdc();
        abs8_set(0xAE51 + g_cpu.X, (u8)g_cpu.A);
        dp8_set(0x16, (u8)(dp8(0x16) + 1));
        lda8(dp8(0x16));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_C);
}
DC_ENTRY(C15FC1, RTS)
{
    dp8_set(0x8E, 0);
    ldx(abs_x(0xA2B6));
    inc_abs(0xA1D8 + g_cpu.X);
    lda8(dp8(0x8E));
    if (!g_cpu._flag_Z) return;
    lda8(0x01);
    script_advance();
}
/* C1:AD35  AE99 = the bits of the AECC slots 1..AECB-1. */
DC_ENTRY(C1AD35, RTS)
{
    tdc(); tax();
    stx_abs(0xAE99);
    lda8(abs8(0xAECB));
    if (g_cpu._flag_Z) return;
    dp8_set(0x02, (u8)g_cpu.A);
    for (;;) {
        dp8_set(0x02, (u8)(dp8(0x02) - 1));
        lda8(dp8(0x02));
        if (g_cpu._flag_Z) return;
        tax();
        lda8(abs8(0xAECC + g_cpu.X));
        tax();
        stx_dp(0x00);
        bit_or(0xAE99);
    }
}
/* C1:A3D1  (0E),Y += $10, saturating at FF; bit 7 of $10 means subtract
 * (and is cleared). */
DC_ENTRY(C1A3D1, RTS)
{
    u32 a = (u32)dp16(0x0E) + g_cpu.Y;
    lda8(dp8(0x10));
    if (!g_cpu._flag_N) {
        g_cpu._flag_C = 0;
        dc_set_a8(dc_adc8((u8)g_cpu.A, abs8(a)));
        if (g_cpu._flag_C) lda8(0xFF);
    } else {
        lda8((u8)g_cpu.A & 0x7F);
        dp8_set(0x10, (u8)g_cpu.A);
        lda8(abs8(a));
        dc_cmp8((u8)g_cpu.A, dp8(0x10));
        int ge = g_cpu._flag_C;
        g_cpu._flag_C = 1;
        dc_set_a8(dc_sbc8((u8)g_cpu.A, dp8(0x10)));
        if (!ge && g_cpu._flag_N) tdc();
    }
    abs8_set(a, (u8)g_cpu.A);
}
/* C1:B4AA  B1D2 = X, or 8D08 / 8D1E for an enemy (B252 + 3) in the given states. */
DC_ENTRY(C1B4AA, RTS)
{
    tdc();
    stx_abs(0xB1D2);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0xB252), 0x03));
    tax(); stx_dp(0x28);
    ldx(0x0080); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C));
    lda8(abs8(0x5E4E + g_cpu.X) | abs8(0x5E53 + g_cpu.X));
    if (g_cpu.A & 0x80) { ldx(0x8D08); stx_abs(0xB1D2); return; }
    lda8((abs8(0x5E4B + g_cpu.X) ^ abs8(0x5E50 + g_cpu.X)) & abs8(0x5E4B + g_cpu.X));
    g_cpu._flag_Z = !(g_cpu.A & 0x04);
    if (g_cpu._flag_Z) return;
    ldx(0x8D1E); stx_abs(0xB1D2);
}
/* C1:7693  A2B0/A2B2 = midpoint of object X and object A014[$84]. */
static void midpoint_x(void)
{
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A); dp8_set(0x81, 0);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0x82, (u8)g_cpu.A); dp8_set(0x83, 0);
    ldy(dp_x(0x84));
    lda8(abs8(0xA014 + g_cpu.Y));
    tax();
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0x84, (u8)g_cpu.A); dp8_set(0x85, 0);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0x86, (u8)g_cpu.A); dp8_set(0x87, 0);
    dc_set_mx(0, g_cpu.x_flag);
    for (int k = 0; k < 2; k++) {
        g_cpu._flag_C = 0;
        u16 v = dc_adc16(dp16(0x80 + k * 2), dp16(0x84 + k * 2));
        g_cpu._flag_C = v & 1;
        lda16(v >> 1);
        dp16_set(0x80 + k * 2, g_cpu.A);
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x80)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(dp8(0x82)); abs8_set(0xA2B2, (u8)g_cpu.A);
}
static void midpoint(u16 slot, u16 first)
{
    ldy(slot);
    if (g_cpu.x_flag) dp8_set(0x84, (u8)g_cpu.Y); else dp16_set(0x84, g_cpu.Y);
    lda8(abs8(first));
    tax();
    midpoint_x();
}
DC_ENTRY(C1768E, RTS) { midpoint(2, 0xA014); }
DC_ENTRY(C1767E, RTS) { midpoint(3, 0xA014); }
DC_ENTRY(C17686, RTS) { midpoint(1, 0xA014); }
DC_ENTRY(C17914, RTS) { midpoint(2, 0xA015); }
/* C1:D0ED  Spends Y gold-like units of 2C53 (borrowing one from 2C55):
 * A = FF when paid (or Y = 0), 00 when short. */
DC_ENTRY(C1D0ED, RTS)
{
    sty_abs(0xAEF7);
    cpy(0);
    if (!g_cpu._flag_Z) {
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(abs16(0x2C53), abs16(0xAEF7));
        abs16_set(0xAEF9, g_cpu.A);
        if (!g_cpu._flag_C) {
            lda16(0);
            dc_set_mx(1, g_cpu.x_flag);
            lda8(abs8(0x2C55));
            if (g_cpu._flag_Z) { lda8(0x00); return; }
            dec_abs(0x2C55);
        }
        dc_set_mx(0, g_cpu.x_flag);
        lda16(abs16(0xAEF9));
        abs16_set(0x2C53, g_cpu.A);
        lda16(0);
        dc_set_mx(1, g_cpu.x_flag);
    }
    lda8(0xFF);
}
/* B1BE entry of record byte `src` (FF stays FF). */
static void b1be_of(u16 src, u8 o, u16 save)
{
    lda8(abs8(src));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) { tax(); lda8(abs8(0xB1BE + g_cpu.X)); }
    dp8_set(o, (u8)g_cpu.A);
    abs8_set(save, (u8)g_cpu.A);
}
DC_ENTRY(C1C74C, RTS)
{
    lda8(abs8(0xB18B));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (!g_cpu._flag_C) {
        lda8(abs8(0xB18C));
        dc_C1CB93_glue();
        lda8(abs8(0xB2EA));
        tax();
        lda8(abs8(0xB1BE + g_cpu.X));
        dp8_set(0x00, (u8)g_cpu.A);
        b1be_of(0xB2EB, 0x02, 0xAE97);
        b1be_of(0xB2EC, 0x04, 0xAE98);
    }
    lda8(abs8(0xB2AE)); dp8_set(0x06, (u8)g_cpu.A);
    lda8(abs8(0xB18B)); dp8_set(0x0C, (u8)g_cpu.A);
}
DC_ENTRY(C1764A, RTS)
{
    lda8((u8)g_cpu.Y);
    sub_a(0x09);
    tay();
    lda8(abs8(0xA014 + g_cpu.Y));
    tax();
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0C + g_cpu.X), abs8(0x9708 + g_cpu.X)));
    abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x9713 + g_cpu.X));
    int neg = g_cpu._flag_N;
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), abs8(0x9713 + g_cpu.X)));
    if (neg) { if (!g_cpu._flag_C) tdc(); }
    else if (g_cpu._flag_C) lda8(0xFF);
    abs8_set(0xA2B2, (u8)g_cpu.A);
}
DC_ENTRY(C1EBF8, RTS)
{
    tdc();
    lda8(abs8(0xB202));
    abs8_set(0xB203, (u8)g_cpu.A);
    if (g_cpu.A & 0x80) {
        dc_set_mx(0, g_cpu.x_flag);
        lda16((u16)((abs16(0xAD89) ^ 0xFFFF) + 1));
        abs16_set(0xAD89, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
    lda8(abs8(0xB2C7));
    tax(); stx_dp(0x28);
    ldx(0x002C); stx_dp(0x2A);
    dc_C1C90B_glue();
    lda8((u8)g_cpu.Y);
    dc_set_a8(asl8(asl8((u8)g_cpu.A)));
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.A, dp8(0x2C)));
    tay();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xAD89));
    abs16_set(0xB328 + g_cpu.Y, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0xB203));
    abs8_set(0xB32B + g_cpu.Y, (u8)g_cpu.A);
}
/* C1:AA6B loop from its middle (AA7B): AECC = the usable slot $0E.. (not B18B)
 * with the smallest nonzero word at FD:xxxx+3 below $02. */
DC_ENTRY(C1AA7B, RTS)
{
    goto body;
    for (;;) {
        lda8(dp8(0x0E));
        tax();
        lda8(abs8(0xAEFF + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) goto next;
        lda8((u8)g_cpu.X);
        dc_cmp8((u8)g_cpu.A, abs8(0xB18B));
        if (g_cpu._flag_Z) goto next;
    body:
        lda8(dp8(0x0E));
        dc_set_a8(asl8((u8)g_cpu.A));
        tax();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(bus16(0xFDA80B + g_cpu.X));
        tax();
        ldy(abs16(0x0003 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            cpy(dp16(0x02));
            if (!g_cpu._flag_C) {
                dp16_set(0x02, g_cpu.Y);
                dc_set_mx(1, g_cpu.x_flag);
                lda8(dp8(0x0E));
                abs8_set(0xAECC, (u8)g_cpu.A);
            }
        }
    next:
        dc_set_mx(1, g_cpu.x_flag);
        tdc();
        dp8_set(0x0E, (u8)(dp8(0x0E) + 1));
        lda8(dp8(0x0E));
        dc_cmp8((u8)g_cpu.A, 0x0B);
        if (g_cpu._flag_C) break;
    }
    lda8(0x01);
    abs8_set(0xAECB, 0x01);
    dp8_set(0x03, 0);
}
DC_ENTRY(C1EC39, RTS)
{
    tdc(); tax(); tay();
    do {
        if ((abs8(0xB32B + g_cpu.Y) | abs8(0xB357 + g_cpu.Y) | abs8(0xB383 + g_cpu.Y)) & 0x20) {
            tdc();
            static const u16 z[] = { 0xB328, 0xB329, 0xB354, 0xB355, 0xB380, 0xB381 };
            for (int k = 0; k < 6; k++) abs8_set(z[k] + g_cpu.Y, (u8)g_cpu.A);
            cpx(0x0003);
            if (g_cpu._flag_C) {
                abs8_set(0xAEB2, (u8)g_cpu.A);
                abs8_set(0xAEB0 + g_cpu.X, (u8)g_cpu.A);
                abs8_set(0xAE82 + g_cpu.X, (u8)g_cpu.A);
            }
        }
        inx();
        iny(); iny(); iny(); iny();
        cpy(0x002C);
    } while (!g_cpu._flag_C);
}
/* C1:F908  24-bit 0013..0015,X += B28C, capped at 98967F (9,999,999). */
DC_ENTRY(C1F908, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0x0013 + g_cpu.X), abs16(0xB28C)));
    abs16_set(0x0013 + g_cpu.X, g_cpu.A);
    if (g_cpu._flag_C) {
        dc_set_mx(1, g_cpu.x_flag);
        inc_abs(0x0015 + g_cpu.X);
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0x0015 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0x98);
    if (g_cpu._flag_C) {
        lda8(0x98);
        abs8_set(0x0015 + g_cpu.X, 0x98);
        dc_set_mx(0, g_cpu.x_flag);
        lda16(abs16(0x0013 + g_cpu.X));
        dc_cmp16(g_cpu.A, 0x9680);
        if (g_cpu._flag_C) { lda16(0x967F); abs16_set(0x0013 + g_cpu.X, 0x967F); }
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:0AD3  1CDB.. (14 entries) = 1 where the 94D0 list (from CC:F38C) holds an item, else 0. */
DC_ENTRY(C10AD3, RTS)
{
    tdc(); tay();
    lda8(0x14); dp8_set(0x80, 0x14);
    lda8(abs8(0x95D5));
    tax();
    lda8(bus8(0xCCF38C + g_cpu.X));
    tax();
    do {
        lda8(abs8(0x94D0 + g_cpu.X));
        if ((u8)g_cpu.A >= 0xFB) { dc_cmp8((u8)g_cpu.A, (u8)g_cpu.A); tdc(); }
        else { dc_cmp8((u8)g_cpu.A, 0xFB); lda8(0x01); }
        abs8_set(0x1CDB + g_cpu.Y, (u8)g_cpu.A);
        iny(); inx();
        u8 n = (u8)(dp8(0x80) - 1); dp8_set(0x80, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
}
/* C1:C92A  2C = (2E:28) / 2A (32 / 16 bits), remainder 32. */
static void rol_dp16(u8 o)
{
    u16 v = dp16(o);
    u16 r = (u16)(v << 1 | g_cpu._flag_C);
    g_cpu._flag_C = v >> 15;
    dp16_set(o, r);
    dc_set_flags_nz16(r);
}
DC_ENTRY(C1C92A, RTS)
{
    php();
    dc_set_mx(0, 0);
    dp16_set(0x2C, 0);
    dp16_set(0x32, 0);
    lda16(dp16(0x28));
    if (!g_cpu._flag_Z) lda16(dp16(0x2A));
    if (!g_cpu._flag_Z) {
        ldx(0x0020);
        do {
            rol_dp16(0x28);
            rol_dp16(0x2E);
            rol_dp16(0x32);
            g_cpu._flag_C = 1;
            lda16(dc_sbc16(dp16(0x32), dp16(0x2A)));
            dp16_set(0x32, g_cpu.A);
            if (!g_cpu._flag_C) {
                lda16(dc_adc16(dp16(0x32), dp16(0x2A)));
                dp16_set(0x32, g_cpu.A);
                g_cpu._flag_C = 0;
            }
            rol_dp16(0x2C);
            dex();
        } while (!g_cpu._flag_Z);
    }
    tdc();
    dc_set_mx(1, 0);
    plp();
}
/* C1:8A05 / 8876  One turn of a party member's tech counter: B071/B045,Y
 * counts down from 0A and clears the status bit when it runs out. */
static void tech_tick(u16 sel, u16 flag, u16 src, u16 dst, u16 mask16, u16 cnt, u16 st, u8 stmask)
{
    tdc();
    lda8(abs8(sel));
    if (g_cpu._flag_Z) lda8(abs8(0xAEC7));
    lda8((u8)(g_cpu.A - 1));
    tax();
    lda8(abs8(0xB163 + g_cpu.X));
    tay();
    lda8(0x01); abs8_set(flag + g_cpu.Y, 0x01);
    lda8(abs8(src + g_cpu.Y)); abs8_set(dst + g_cpu.Y, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xB186) & mask16);
    abs16_set(0xB186, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8((u8)(abs8(cnt + g_cpu.Y) - 1));
    abs8_set(cnt + g_cpu.Y, (u8)g_cpu.A);
    if (!g_cpu._flag_Z) return;
    lda8(0x0A); abs8_set(cnt + g_cpu.Y, 0x0A);
    lda8((u8)g_cpu.Y);
    dc_set_mx(0, g_cpu.x_flag);
    u16 a = (u16)(g_cpu.A << 8 | g_cpu.A >> 8);
    g_cpu._flag_C = a & 1;
    lda16(a >> 1);
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(st + g_cpu.X) & stmask); abs8_set(st + g_cpu.X, (u8)g_cpu.A);
    tdc();
    abs8_set(flag + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C18A05, RTS) { tech_tick(0xB17D, 0xAFE2, 0xB100, 0xAF53, 0x1EFF, 0xB071, 0x5E4D, 0xBF); }
DC_ENTRY(C18876, RTS) { ldx(0); tech_tick(0xB179, 0xAFB6, 0xB0D4, 0xAF27, 0x0FFF, 0xB045, 0x5E4B, 0x7F); }
/* C1:0929  Copies 6 rows of 0E (or 0C, skipping 2) bytes of D1:59FC to 0B40+$82, rows 40 apart. */
DC_ENTRY(C10929, RTS)
{
    tdc(); tax();
    lda8(0x06); dp8_set(0x81, 0x06);
    do {
        lda8(dp8(0x84));
        if (g_cpu._flag_Z) lda8(0x0E);
        else { inx(); inx(); lda8(0x0C); }
        dp8_set(0x80, (u8)g_cpu.A);
        ldy(dp_x(0x82));
        do {
            lda8(bus8(0xD159FC + g_cpu.X));
            abs8_set(0x0B40 + g_cpu.Y, (u8)g_cpu.A);
            inx(); iny();
            u8 n = (u8)(dp8(0x80) - 1); dp8_set(0x80, n); dc_set_flags_nz8(n);
        } while (!g_cpu._flag_Z);
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        dp16_set(0x82, dc_adc16(dp16(0x82), 0x0040));
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        u8 n = (u8)(dp8(0x81) - 1); dp8_set(0x81, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
}
/* C1:56CC  Script call: saves the return position (+2) for slot A00E and
 * jumps to entry n of the CE:0000 table. */
DC_ENTRY(C156CC, RTS)
{
    lda8(script_byte());
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    lda8(abs8(0xA00E));
    tay();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    abs16_set(0xA569 + g_cpu.Y, dc_adc16(0x0002, dp16(0xE6)));
    abs16_set(0xA56B + g_cpu.Y, dc_adc16(dp16(0xE8), 0));
    dp16_set(0xE6, bus16(0xCE0000 + g_cpu.X));
    dp16_set(0xE8, 0x00CE);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    inc_abs(0xA00F);
    script_advance();
}
/* C1:CBC3 ..  Scales a stat byte of the record at $04 by the element of slot A. */
static void scale_stat(u16 o)
{
    tax();
    lda8(abs8(0xB1BE + g_cpu.X));
    u16 src = 0xB3BC, off = (u16)(o + 3);
    if (g_cpu._flag_Z) { src = 0xB3BA; off = o; }
    else { dc_cmp8((u8)g_cpu.A, 0x01); if (g_cpu._flag_Z) { src = 0xB3BB; off = (u16)(o + 2); } }
    lda8(abs8(src));
    dc_C1CBF6_glue();
    lda8((u8)g_cpu.Y);
    ldx(dp_x(0x04));
    abs8_set(off + g_cpu.X, (u8)g_cpu.A);
}
DC_ENTRY(C1CBC3, RTS) { scale_stat(0x0003); }
DC_ENTRY(C1CC0B, RTS) { scale_stat(0x0049); }
DC_ENTRY(C1CC3E, RTS) { scale_stat(0x0081); }
DC_ENTRY(C1F4DF, RTS)
{
    lda8(0x80); dp8_set(0x0A, 0x80);
    ldx(dp_x(0x02));
    lda8(abs8(0x0000 + g_cpu.X));
    while (!g_cpu._flag_Z) {
        u8 v = dp8(0x0A); g_cpu._flag_C = v & 1; dp8_set(0x0A, v >> 1);
        lda8((u8)(g_cpu.A - 1));
    }
    ldx(dp_x(0x02));
    lda8(abs8(0x0007 + g_cpu.X) | dp8(0x0A)); abs8_set(0x0007 + g_cpu.X, (u8)g_cpu.A);
    ldx(abs_x(0xB1F4));
    lda8(abs8(0xB2B3 + g_cpu.X) | 0x08); abs8_set(0xB2B3 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xB2D1));
    tax();
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8(abs8(0x2830 + g_cpu.X), dp8(0x3B)));
    ldx(abs_x(0xB3CC));
    abs8_set(0xB3CE + g_cpu.X, (u8)g_cpu.A);
    inc_abs(0xB3CC);
    ldx(dp_x(0x02));
    lda8((u8)(abs8(0x0000 + g_cpu.X) + 1));
    abs8_set(0x0000 + g_cpu.X, (u8)g_cpu.A);
    ldx(dp_x(0x04));
    dc_cmp8((u8)g_cpu.A, bus8(0xCC0001 + g_cpu.X));
    if (!g_cpu._flag_C) return;
    ldx(dp_x(0x00));
    lda8(abs8(0x5E2F + g_cpu.X) | 0x10); abs8_set(0x5E2F + g_cpu.X, (u8)g_cpu.A);
}
/* C1:1583  (entered with Z and A from the caller) */
DC_ENTRY(C11583, RTS)
{
    if (!g_cpu._flag_Z) {
        lda8((u8)(g_cpu.A - 1));
        abs8_set(0x95DB, (u8)g_cpu.A);
        abs8_set(0xA09A, 0);
        abs8_set(0xA86B, 0);
        lda8((u8)(abs8(0x95DB) - 1));
        if (g_cpu._flag_Z) {
            abs8_set(0xA869, 0);
            inc_abs(0xA86A);
        } else {
            tdc(); tax();
            do {
                lda8(bus8(0xD15BD0 + g_cpu.X));
                abs8_set(0x0B40 + g_cpu.X, (u8)g_cpu.A);
                inx();
                cpx(0x0180);
            } while (!g_cpu._flag_Z);
            lda8(0xFF); abs8_set(0x9920, 0xFF);
            inc_abs(0x99E2);
        }
    }
    lda8(abs8(0x95DB));
    if (g_cpu._flag_Z) inc_abs(0xA43F);
    lda8(abs8(0x0900) | 0x55); abs8_set(0x0900, (u8)g_cpu.A);
    lda8(0xF0);
    abs8_set(0x0701, 0xF0); abs8_set(0x0705, 0xF0); abs8_set(0x0709, 0xF0); abs8_set(0x070D, 0xF0);
    dp8_set(0xEE, 0); dp8_set(0xEF, 0);
}
/* C1:D005  Adds one of item Y to the inventory (first free slot when new):
 * A = new count, 00 when Y = 0 or already 99. */
DC_ENTRY(C1D005, RTS)
{
    sty_abs(0xAEF7);
    cpy(0);
    if (g_cpu._flag_Z) { lda8(0x00); return; }
    ldx(0);
    stx_abs(0xAEF9);
    abs8_set(0xAEFB, 0);
    do {
        lda8(abs8(0x2400 + g_cpu.X));
        if (g_cpu._flag_Z) {
            lda8(abs8(0xAEFB));
            if (g_cpu._flag_Z) { stx_abs(0xAEF9); inc_abs(0xAEFB); }
        }
        lda8(abs8(0x2400 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, abs8(0xAEF7));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x2500 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0x63);
            if (g_cpu._flag_C) { lda8(0x00); return; }
            lda8((u8)(g_cpu.A + 1));
            abs8_set(0x2500 + g_cpu.X, (u8)g_cpu.A);
            return;
        }
        inx();
        cpx(0x0100);
    } while (!g_cpu._flag_C);
    ldx(abs_x(0xAEF9));
    lda8(abs8(0xAEF7));
    abs8_set(0x2400 + g_cpu.X, (u8)g_cpu.A);
    lda8((u8)(abs8(0x2500 + g_cpu.X) + 1));
    abs8_set(0x2500 + g_cpu.X, (u8)g_cpu.A);
}

/* ---- batch 5 --------------------------------------------------------------- */
DC_ENTRY(C1FDC3, RTL) { dc_C1D005_glue(); }
/* 01:D005  The same routine through the bank 01 mirror of C1 (seen at runtime as a JSR
 * from 01:F01E, i.e. C1:F007.. running with PB = 01); it uses no PB-relative data. */
DC_ENTRY(01D005, RTS) { dc_C1D005_glue(); }
/* A = 44 + A / 18 (2C of the C92A divide). */
DC_ENTRY(C1FDAE, RTS)
{
    tax(); stx_dp(0x28);
    ldx(0x0018); stx_dp(0x2A);
    dc_C1C92A_glue();
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0x2C), 0x44));
}
/* AD89 = AD89 * $1C / $1E. */
DC_ENTRY(C1EDBA, RTS)
{
    tdc();
    ldx(abs_x(0xAD89)); stx_dp(0x28);
    lda8(dp8(0x1C)); tax(); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    lda8(dp8(0x1E)); tax(); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C));
    stx_abs(0xAD89);
}
/* C1:B488  B1D2 = start of the string after the one at CC:xxxx (entry A of CC:8B08). */
DC_ENTRY(C1B488, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = g_cpu.A >> 15;
    lda16((u16)(g_cpu.A << 1));
    tax();
    lda16(bus16(0xCC8B08 + g_cpu.X));
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    dc_C1B4AA_glue();
    ldx(abs_x(0xB1D2));
    for (;;) {
        lda8(bus8(0xCC0000 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) break;
        inx();
    }
    inx();
    stx_abs(0xB1D2);
}
/* 16-bit helpers for the C02A..C1DC scalers. */
static void lsr16(int n) { for (int k = 0; k < n; k++) { g_cpu._flag_C = g_cpu.A & 1; lda16(g_cpu.A >> 1); } }
static void y_minus_08(void)                     /* TYA; SEC; SBC $08 */
{
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(g_cpu.Y, dp16(0x08)));
}
static void a08_shift(int n) { lda16(dp16(0x08)); lsr16(n); dp16_set(0x08, g_cpu.A); }
static void xba16(void) { lda16((u16)(g_cpu.A << 8 | g_cpu.A >> 8)); dc_set_flags_nz8((u8)g_cpu.A); }
/* C1:C1B2  $30 = v - v/8 - v/16 - v/128 with v = A/8 (about 0.82 * A/8). */
DC_ENTRY(C1C1B2, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lsr16(3);
    tay();
    lsr16(3);
    dp16_set(0x08, g_cpu.A);
    y_minus_08(); tay();
    a08_shift(1);
    y_minus_08(); tay();
    a08_shift(3);
    y_minus_08();
    dp16_set(0x30, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:C063 / C02A  $30 = high byte of the same scaling of (A swapped) / 2 or / 4. */
static void scale_hi(int pre, int post)
{
    dc_set_mx(0, g_cpu.x_flag);
    xba16();
    lsr16(pre);
    tay();
    lsr16(post);
    dp16_set(0x08, g_cpu.A);
    y_minus_08(); tay();
    a08_shift(1);
    y_minus_08(); tay();
    a08_shift(3);
    y_minus_08(); tay();
    a08_shift(1);
    y_minus_08();
    lda16(g_cpu.A & 0xFF00);
    xba16();
    dp16_set(0x30, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(C1C063, RTS) { scale_hi(1, 2); }
DC_ENTRY(C1C02A, RTS) { scale_hi(2, 3); }
/* C1:30E1  Effect 20 for slot $94; party slots also get 4 bytes of CC:F77B. */
DC_ENTRY(C130E1, RTS)
{
    effect_restart(0x20, 0xCCF5FE);
    ldx(dp_x(0x94));
    inc_abs(0xA478 + g_cpu.X);
    cpx(0x0003);
    if (g_cpu._flag_C) return;
    lda8(abs8(0x993C + g_cpu.X));
    asl_a(4);
    tay();
    if (g_cpu.x_flag) dp8_set(0x80, (u8)g_cpu.Y); else dp16_set(0x80, g_cpu.Y);
    ldx(0x0018);
    do {
        lda8(abs8(0x2102 + g_cpu.Y));
        abs8_set(0x2302 + g_cpu.Y, (u8)g_cpu.A);
        iny();
        dex();
    } while (!g_cpu._flag_Z);
    ldy(dp_x(0x80));
    for (int k = 0; k < 4; k++) {
        lda8(bus8(0xCCF77B + k));
        abs8_set(0x2306 + k + g_cpu.Y, (u8)g_cpu.A);
    }
}
/* C1:BD6F  ATB speed of slot X: halved with status bit 7 of 5E4D / 5E52. */
DC_ENTRY(C1BD6F, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(g_cpu.X);
    xba16();
    lsr16(1);
    tay();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0x5E4D + g_cpu.Y) | abs8(0x5E52 + g_cpu.Y));
    g_cpu._flag_Z = !(g_cpu.A & 0x80);
    dc_C1BD80_glue();
}
/* Applies fn to A5D8 of the target (A010 < 4) or of every member of the
 * A015 list (A010 = 4); then the 1-byte command is done. */
static void each_target(void (*fn)(u16 x))
{
    ldx(abs_x(0xA010));
    cpx(0x0008);
    if (!g_cpu._flag_C) {
        cpx(0x0004);
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xA014 + g_cpu.X));
            tax();
            fn(g_cpu.X);
        } else {
            tdc(); tay();
            for (;;) {
                lda8(abs8(0xA015 + g_cpu.Y));
                if (g_cpu._flag_N) break;
                tax();
                fn(g_cpu.X);
                iny();
            }
        }
    }
    lda8(0x01);
    script_advance();
}
static void clr_a5d8(u16 x) { abs8_set(0xA5D8 + x, 0); }
static void inc_a5d8(u16 x) { inc_abs(0xA5D8 + x); }
DC_ENTRY(C1702B, RTS) { each_target(clr_a5d8); }
DC_ENTRY(C16FFC, RTS) { each_target(inc_a5d8); }
/* C1:6063  Records target n of the script for attacker A2B6 (in AB4F and the
 * A3DF table at A2B6 * 0B + target), tagged with $8E. */
static void note_target(void)
{
    lda8(script_byte());
    tax();
    lda8(abs8(0xA014 + g_cpu.X));
    dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0xA2B6));
    dp8_set(0xAD, (u8)g_cpu.A);
    tax();
    lda8(dp8(0x80));
    abs8_set(0xAB4F + g_cpu.X, (u8)g_cpu.A);
    lda8(0x0B);
    dp8_set(0xAE, 0x0B);
    dc_C10089_glue();
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xAF), dp8(0x80)));
    tax();
    lda8((abs8(0xA2B6) & 0x3F) | dp8(0x8E));
    abs8_set(0xA3DF + g_cpu.X, (u8)g_cpu.A);
    lda8(0x02);
    script_advance();
}
DC_ENTRY(C16059, RTS) { lda8(0x80); dp8_set(0x8E, 0x80); note_target(); }
/* C1:D0A2  2C53..2C55 (gold) += Y, capped at 9,999,999: A = 00FF, or 0000 when capped. */
DC_ENTRY(C1D0A2, RTS)
{
    sty_abs(0xAEF7);
    cpy(0);
    if (!g_cpu._flag_Z) {
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(abs16(0x2C53), abs16(0xAEF7)));
        abs16_set(0x2C53, g_cpu.A);
        if (g_cpu._flag_C) {
            lda16(0);
            dc_set_mx(1, g_cpu.x_flag);
            inc_abs(0x2C55);
        }
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x2C55));
        dc_cmp8((u8)g_cpu.A, 0x98);
        if (g_cpu._flag_C) {
            lda8(0x98);
            abs8_set(0x2C55, 0x98);
            dc_set_mx(0, g_cpu.x_flag);
            lda16(abs16(0x2C53));
            dc_cmp16(g_cpu.A, 0x9680);
            if (g_cpu._flag_C) {
                lda16(0x967F);
                abs16_set(0x2C53, 0x967F);
                lda16(0);
                dc_set_mx(1, g_cpu.x_flag);
                return;
            }
        }
    }
    dc_set_mx(1, g_cpu.x_flag);
    lda8(0x00);
    lda16((u16)(g_cpu.A << 8 | g_cpu.A >> 8));
    lda8(0xFF);
}
/* C1:B442  Copies the 50-byte battle record of each party member (5E2D +
 * slot * 80) to 2600 + character * 50. */
DC_ENTRY(C1B442, RTS)
{
    tdc(); tax(); stx_dp(0x00);
    do {
        lda8(abs8(0x2980 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            tax(); stx_dp(0x2A);
            ldx(0x0050); stx_dp(0x28);
            dc_C1C90B_glue();
            ldx(dp_x(0x2C)); stx_dp(0x02);
            dc_set_mx(0, g_cpu.x_flag);
            lda16(dp16(0x00));
            xba16();
            lsr16(1);
            dp16_set(0x04, g_cpu.A);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            tay();
            do {
                ldx(dp_x(0x04));
                lda8(abs8(0x5E2D + g_cpu.X));
                inx(); stx_dp(0x04);
                ldx(dp_x(0x02));
                abs8_set(0x2600 + g_cpu.X, (u8)g_cpu.A);
                inx(); stx_dp(0x02);
                iny();
                cpy(0x0050);
            } while (!g_cpu._flag_C);
        }
        dp8_set(0x00, (u8)(dp8(0x00) + 1));
        ldx(dp_x(0x00));
        cpx(0x0003);
    } while (!g_cpu._flag_C);
}
/* C1:4F1A  Copies 98BF / 973F to A324 / A2FE for the target(s). */
DC_ENTRY(C14F1A, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax();
            lda8(abs8(0xA014 + g_cpu.X));
            tax();
            lda8(abs8(0x98BF + g_cpu.X)); abs8_set(0xA324 + g_cpu.X, (u8)g_cpu.A);
            lda8(abs8(0x973F + g_cpu.X)); abs8_set(0xA2FE + g_cpu.X, (u8)g_cpu.A);
        } else {
            tdc(); tax();
            for (;;) {
                lda8(abs8(0xA018 + g_cpu.X));
                if (g_cpu._flag_N) break;
                tay();
                lda8(abs8(0x973F + g_cpu.Y)); abs8_set(0xA2FE + g_cpu.Y, (u8)g_cpu.A);
                lda8(abs8(0x98BF + g_cpu.Y)); abs8_set(0xA324 + g_cpu.Y, (u8)g_cpu.A);
                inx();
            }
        }
    }
    lda8(0x01);
    script_advance();
}
/* C1:AE70 / AE7A  Adds tech AF1E to the menu list AECC when its entry (FD:A80B
 * by X, + $0A) has a bit of AF1F, tagging 40 / 80 for the 20 / 10 flags. */
static void tech_add(void)
{
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) {
        dc_set_mx(0, g_cpu.x_flag);
        lda16(bus16(0xFDA80B + g_cpu.X));
        dp16_set(0x0C, g_cpu.A);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, dp16(0x0A)));
        tax();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x0000 + g_cpu.X));
        u8 m = abs8(0xAF1F);
        g_cpu._flag_N = m >> 7; g_cpu._flag_V = m >> 6 & 1; g_cpu._flag_Z = ((u8)g_cpu.A & m) == 0;
        if (!g_cpu._flag_Z) {
            ldx(dp_x(0x0C));
            lda8(abs8(0x001D + g_cpu.X));
            if (!g_cpu._flag_N) {
                tdc();
                lda8(abs8(0xAECB));
                tay();
                lda8(abs8(0xAF1E));
                abs8_set(0xAECC + g_cpu.Y, (u8)g_cpu.A);
                inc_abs(0xAECB);
                ldx(dp_x(0x0C));
                lda8(abs8(0x0020 + g_cpu.X));
                if (g_cpu.A & 0x20) {
                    lda8(abs8(0xAECC + g_cpu.Y) | 0x40); abs8_set(0xAECC + g_cpu.Y, (u8)g_cpu.A);
                    inc_abs(0xAF20);
                }
                lda8(abs8(0x0020 + g_cpu.X));
                g_cpu._flag_Z = !(g_cpu.A & 0x10);
                if (!g_cpu._flag_Z) {
                    lda8(abs8(0xAECC + g_cpu.Y) | 0x80); abs8_set(0xAECC + g_cpu.Y, (u8)g_cpu.A);
                    inc_abs(0xAF21);
                }
            }
        }
    }
    dp8_set(0x0D, 0);
}
DC_ENTRY(C1AE7A, RTS) { tech_add(); }
DC_ENTRY(C1AE70, RTS)
{
    tdc();
    dp8_set(0x0B, 0);
    lda8(abs8(0xAF1E));
    tay();
    lda8(abs8(0xAEFF + g_cpu.Y));
    tech_add();
}
/* C1:B6D1  Clears bit 6 of menu entry $14 (records of 7) and counts one use of
 * its item in the 1580 list. */
DC_ENTRY(C1B6D1, RTS)
{
    phx();
    tdc();
    lda8(dp8(0x14));
    dc_set_a8(asl8(asl8(asl8((u8)g_cpu.A))));
    g_cpu._flag_C = 1;
    dc_set_a8(dc_sbc8((u8)g_cpu.A, dp8(0x14)));
    tax();
    lda8(abs8(0x93EF + g_cpu.X));
    if (g_cpu.A & 0x40) {
        lda8((u8)g_cpu.A & 0xBF);
        abs8_set(0x93EF + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x93F4 + g_cpu.X));
        dp8_set(0x0E, (u8)g_cpu.A);
        tdc(); tax(); tay();
        do {
            lda8(abs8(0x1580 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, dp8(0x0E));
            if (g_cpu._flag_Z) {
                lda8((u8)(abs8(0x1583 + g_cpu.X) + 1));
                abs8_set(0x1583 + g_cpu.X, (u8)g_cpu.A);
                break;
            }
            inx(); inx(); inx(); inx(); inx();
            iny();
            cpy(0x0100);
        } while (!g_cpu._flag_C);
    }
    plx();
}
/* C1:20E0  Target list 99C0 = the party slots that can act (bit 7 of 5E4A..). */
DC_ENTRY(C120E0, RTS)
{
    inc_abs(0x960A);
    tdc(); tax();
    for (int k = 0; k < 3; k++) {
        lda8(abs8(0x96F5 + k));
        if (g_cpu._flag_Z) continue;
        lda8(abs8(0xA09B + k));
        if (!g_cpu._flag_Z) continue;
        lda8(abs8(0x5E4A + k * 0x80));
        if (!g_cpu._flag_N) continue;
        if (k == 0) abs8_set(0x99C0, 0);
        else { lda8((u8)k); abs8_set(0x99C0 + g_cpu.X, (u8)k); }
        if (k < 2) inx();
    }
    lda8(abs8(0x99C0));
    if (g_cpu._flag_N) { lda8(0x80); abs8_set(0x9613, 0x80); }
    lda8(abs8(0x9614));
    tax();
    lda8(abs8(0x99C0 + g_cpu.X));
    abs8_set(0xA62D, (u8)g_cpu.A);
}
/* A * 12 (ASL; ASL; STA $80; ASL; CLC; ADC $80). */
static void times12_at(u8 tmp)
{
    dc_set_a8(asl8(asl8((u8)g_cpu.A)));
    dp8_set(tmp, (u8)g_cpu.A);
    dc_set_a8(asl8((u8)g_cpu.A));
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.A, dp8(tmp)));
}
static void times12(void) { times12_at(0x80); }
/* C1:0872  Clears the 2x6 tilemap cells of object 95D5 and draws tiles 60..63
 * for the A6DD entry when A43F is set. */
DC_ENTRY(C10872, RTS)
{
    lda8(abs8(0x95D5));
    times12();
    tax();
    static const u16 z[] = { 0x0CC0, 0x0D00, 0x0D40, 0x0D80, 0x0DC0, 0x0E00, 0x0CC2, 0x0D02, 0x0D42, 0x0D82, 0x0DC2, 0x0E02 };
    for (int k = 0; k < 12; k++) abs8_set(z[k] + g_cpu.X, 0);
    lda8(abs8(0xA43F));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xA6DD));
        tax();
        times12();
        tay();
        if (g_cpu.x_flag) dp8_set(0x80, (u8)g_cpu.Y); else dp16_set(0x80, g_cpu.Y);
        lda8(abs8(0xA6D9 + g_cpu.X));
        tax();
        lda8(abs8(0x95DC + g_cpu.X));
        dc_set_a8(asl8((u8)g_cpu.A));
        tax();
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(bus16(0xCCFADD + g_cpu.X), dp16(0x80)));
        tax();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x9609));
        if (g_cpu._flag_Z) {
            static const u16 t[] = { 0x0CC0, 0x0CC2, 0x0D00, 0x0D02 };
            for (int k = 0; k < 4; k++) { lda8((u8)(0x60 + k)); abs8_set(t[k] + g_cpu.X, (u8)g_cpu.A); }
        }
    }
    abs8_set(0xA43F, 0);
}
/* Subtracts n from 9499 until it borrows: X = quotient. */
static void digits_div(u16 n, int store)
{
    tdc(); tax();
    do {
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(abs16(0x9499), n));
        abs16_set(0x9499, g_cpu.A);
        inx();
    } while (g_cpu._flag_C);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0x9499), n));
    if (store) abs16_set(0x9499, g_cpu.A);
    dex();
    phx();
}
/* C1:011F  9499 (< 1000) to three digit tiles 949D..949F, 949C = FF. */
DC_ENTRY(C1011F, RTS)
{
    phx();
    digits_div(0x0064, 1);
    digits_div(0x000A, 0);
    dc_set_mx(1, g_cpu.x_flag);
    tax();
    lda8(bus8(0xCCF903 + g_cpu.X)); abs8_set(0x949F, (u8)g_cpu.A);
    plx();
    lda8(bus8(0xCCF903 + g_cpu.X)); abs8_set(0x949E, (u8)g_cpu.A);
    plx();
    lda8(bus8(0xCCF903 + g_cpu.X)); abs8_set(0x949D, (u8)g_cpu.A);
    lda8(0xFF); abs8_set(0x949C, 0xFF);
    plx();
}
/* C1:01A9  Splits the 16 name characters of 94A0 into upper tiles 94A0 and
 * lower tiles 94B0 (dakuten-style marks 71 / 72 for codes 40..72). */
DC_ENTRY(C101A9, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.X = 0x94A0; g_cpu.Y = 0x94C0; g_cpu.A = 0x000F;
    dc_mvn(0x7E, 0x7E);
    ldx(0x001E);
    do {
        abs16_set(0x94A0 + g_cpu.X, g_cpu.A);
        dex(); dex();
    } while (!g_cpu._flag_N);
    tdc(); tax();
    dc_set_mx(1, g_cpu.x_flag);
    do {
        lda8(abs8(0x94C0 + g_cpu.X));
        if (g_cpu._flag_Z) return;
        u8 c = (u8)g_cpu.A;
        if (c >= 0x73 || c < 0x40) {
            dc_cmp8(c, c >= 0x73 ? 0x73 : 0x40);
            abs8_set(0x94A0 + g_cpu.X, c);
            lda8(0xFF);
        } else {
            g_cpu._flag_C = 0;
            lda8(dc_adc8(c, c >= 0x69 ? 0x17 : 0x40));
            abs8_set(0x94A0 + g_cpu.X, (u8)g_cpu.A);
            lda8(c >= 0x69 ? 0x72 : 0x71);
        }
        abs8_set(0x94B0 + g_cpu.X, (u8)g_cpu.A);
        inx();
        cpx(0x0010);
    } while (!g_cpu._flag_Z);
}
/* C1:CF69  Adds the low nibble of CC:0005,$08 to stats 3F..42 of record $00
 * selected by its top 4 bits. */
DC_ENTRY(C1CF69, RTS)
{
    tdc();
    ldx(dp_x(0x08));
    lda8(bus8(0xCC0005 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    dp8_set(0x04, (u8)g_cpu.A);
    lda8((u8)g_cpu.A & 0x0F);
    dp8_set(0x0A, (u8)g_cpu.A);
    lda8(dp8(0x04));
    for (int k = 0; k < 4; k++) {
        if (k) { u8 v = dp8(0x04); g_cpu._flag_C = v >> 7; dp8_set(0x04, (u8)(v << 1)); lda8(dp8(0x04)); }
        if (!g_cpu._flag_N) continue;
        ldx(dp_x(0x00));
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x003F + k + g_cpu.X), dp8(0x0A)));
        abs8_set(0x003F + k + g_cpu.X, (u8)g_cpu.A);
    }
}
/* C1:6258  Sets A5AA of the target(s), or A5B5 of the attacker, to the operand. */
DC_ENTRY(C16258, RTS)
{
    lda8(script_byte());
    dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldx(abs_x(0xA2B6));
        lda8(dp8(0x80));
        abs8_set(0xA5B5 + g_cpu.X, (u8)g_cpu.A);
    } else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax();
            lda8(abs8(0xA014 + g_cpu.X));
            tax();
            lda8(dp8(0x80));
            abs8_set(0xA5AA + g_cpu.X, (u8)g_cpu.A);
        } else {
            tdc(); tax();
            for (;;) {
                lda8(abs8(0xA018 + g_cpu.X));
                if (g_cpu._flag_N) break;
                tay();
                lda8(dp8(0x80));
                abs8_set(0xA5AA + g_cpu.Y, (u8)g_cpu.A);
                inx();
            }
        }
    }
    lda8(0x02);
    script_advance();
}

/* ---- batch 6 --------------------------------------------------------------- */
DC_ENTRY(C1FDCF, RTL) { dc_C1B6D1_glue(); }
DC_ENTRY(C1E63B, RTS)
{
    tdc();
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E4B + g_cpu.X));
    if (!(g_cpu.A & 0x06)) { g_cpu._flag_Z = 1; return; }
    lda8((u8)g_cpu.A & 0xF9);
    abs8_set(0x5E4B + g_cpu.X, (u8)g_cpu.A);
    tdc();
    lda8(abs8(0xB1FD));
    tax();
    lda8(0x01); abs8_set(0xB03A + g_cpu.X, 0x01);
    lda8(abs8(0xB158 + g_cpu.X)); abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A);
    dc_C1BD6F_glue();
}
/* C1:AF22 / AF79  A = level-scaled value of FD:BA61 for tech X (8-bit index),
 * counter at `cnt`; 2C is preserved. */
static void tech_level(u16 cnt, int cnt_dp)
{
    dc_set_mx(1, 1);
    dp8_set(0x25, (u8)g_cpu.X);
    dc_set_mx(1, 0);
    ldx(dp16(0x2C));
    dc_push16(g_cpu.X);
    dc_set_mx(1, 1);
    ldx(dp8(0x25));
    dc_cmp8((u8)g_cpu.X, 0xFF);
    if (!g_cpu._flag_Z) {
        abs8_set(0xB31E, (u8)g_cpu.A);
        lda8(abs8(0x2989));
        if (g_cpu.A & 0x20) lda8((u8)g_cpu.X);
        else {
            lda8(abs8(0xB31E));
            dc_cmp8((u8)g_cpu.A, 0x00);
            if (!g_cpu._flag_Z) {
                dc_cmp8((u8)g_cpu.A, dp8(0x25));
                if (!g_cpu._flag_Z) {
                    ldx(cnt_dp ? dp8((u8)cnt) : abs8(cnt));
                    g_cpu._flag_C = 1;
                    lda8(dc_sbc8((u8)g_cpu.A, dp8(0x25)));
                    dc_cmp8((u8)g_cpu.A, 0xFF);
                    if (g_cpu._flag_Z) lda8(bus8(0xFDBA61 + g_cpu.X));
                    else {
                        dp8_set(0x2A, (u8)g_cpu.A);
                        dp8_set(0x2B, 0);
                        lda8(bus8(0xFDBA61 + g_cpu.X));
                        tax();
                        dp8_set(0x28, (u8)g_cpu.X);
                        dp8_set(0x29, 0);
                        dc_C1C92A_glue();
                        g_cpu._flag_C = 0;
                        lda8(dc_adc8(dp8(0x32), dp8(0x25)));
                        if (cnt_dp) { u8 v = (u8)(dp8((u8)cnt) + 1); dp8_set((u8)cnt, v); dc_set_flags_nz8(v); }
                        else inc_abs(cnt);
                    }
                }
            }
        }
    }
    dc_set_mx(1, 0);
    ldx(dc_pull16());
    dp16_set(0x2C, g_cpu.X);
}
DC_ENTRY(C1AF22, RTS) { tech_level(0x26, 1); }
DC_ENTRY(C1AF79, RTS) { tech_level(0xB3E6, 0); }
/* C1:1AA5  Cursor sprites 1.. (from slot Y) over the targets listed in A62D
 * from 960B, up to the end of the group of 3. */
DC_ENTRY(C11AA5, RTS)
{
    if (g_cpu.x_flag) dp8_set(0x82, (u8)g_cpu.Y); else dp16_set(0x82, g_cpu.Y);
    lda8(abs8(0x960B));
    tay();
    if (g_cpu.x_flag) dp8_set(0x80, (u8)g_cpu.Y); else dp16_set(0x80, g_cpu.Y);
    for (;;) {
        ldy(dp_x(0x80));
        lda8(abs8(0xA62D + g_cpu.Y));
        if (!g_cpu._flag_N) {
            tax();
            lda8(dp8(0x82));
            dc_set_a8(asl8(asl8((u8)g_cpu.A)));
            tay();
            g_cpu._flag_C = 0;
            lda8(dc_adc8(abs8(0x1D0C + g_cpu.X), abs8(0x9708 + g_cpu.X)));
            g_cpu._flag_C = 1;
            lda8(dc_sbc8((u8)g_cpu.A, 0x10));
            abs8_set(0x0700 + g_cpu.Y, (u8)g_cpu.A);
            g_cpu._flag_C = 0;
            lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), abs8(0x9713 + g_cpu.X)));
            abs8_set(0x0701 + g_cpu.Y, (u8)g_cpu.A);
            ldx(dp_x(0x82));
            lda8(abs8(0x0900) & bus8(0xCCFB8D + g_cpu.X));
            abs8_set(0x0900, (u8)g_cpu.A);
        }
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
        lda8(dp8(0x80));
        if ((u8)g_cpu.A == 0x03 || (u8)g_cpu.A == 0x06) break;
        dp8_set(0x82, (u8)(dp8(0x82) + 1));
        lda8(dp8(0x82));
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (g_cpu._flag_Z) break;
    }
    lda8(dp8(0x80));
    dc_cmp8((u8)g_cpu.A, 0x06);
    if (g_cpu._flag_Z) tdc();
    abs8_set(0x960B, (u8)g_cpu.A);
    dp8_set(0xEE, 0); dp8_set(0xEF, 0);
}
/* C1:41BE  A119..A11B = per party member, the count of the entries at AD9C
 * (type 3) whose amount reaches its HP 5E30. */
DC_ENTRY(C141BE, RTS)
{
    tdc(); tax(); tay();
    for (int o = 0x80; o <= 0x84; o += 2) dp16_set((u8)o, g_cpu.Y);
    dc_set_mx(0, g_cpu.x_flag);
    do {
        lda16(dp16(0x80));
        xba16();
        lsr16(1);
        tax();
        lda16(abs16(0xAD9E + g_cpu.Y));
        if (!g_cpu._flag_Z) {
            dc_cmp16(g_cpu.A, 0x0003);
            if (g_cpu._flag_Z) {
                lda16(abs16(0x5E30 + g_cpu.X));
                if (!g_cpu._flag_Z) {
                    lda16(abs16(0xAD9C + g_cpu.Y));
                    if (!g_cpu._flag_Z) {
                        lda16((u16)((g_cpu.A ^ 0xFFFF) + 1));
                        g_cpu._flag_C = 0;
                        lda16(dc_adc16(g_cpu.A, abs16(0x5E30 + g_cpu.X)));
                        if (g_cpu._flag_Z || !g_cpu._flag_C) {
                            ldx(dp16(0x80));
                            u16 v = (u16)(dp16((u8)(0x82 + g_cpu.X)) + 1);
                            dp16_set((u8)(0x82 + g_cpu.X), v);
                            dc_set_flags_nz16(v);
                        }
                    }
                }
            }
        }
        iny(); iny(); iny(); iny();
        dp16_set(0x80, (u16)(dp16(0x80) + 1));
        lda16(dp16(0x80));
        dc_cmp16(g_cpu.A, 0x0003);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    for (int k = 0; k < 3; k++) { lda8(dp8((u8)(0x82 + k))); abs8_set(0xA119 + k, (u8)g_cpu.A); }
}
/* C1:DE9E  $16 = $16 * 5 / 20 + 1 (status 05) or * 15 / 20 + 1 (status 40). */
DC_ENTRY(C1DE9E, RTS)
{
    ldx(abs_x(0xB1F4));
    lda8(abs8(0x5E4B + g_cpu.X));
    u8 k;
    if (g_cpu.A & 0x05) k = 0x05;
    else {
        lda8(abs8(0x5E4B + g_cpu.X));
        g_cpu._flag_Z = !(g_cpu.A & 0x40);
        if (g_cpu._flag_Z) return;
        k = 0x0F;
    }
    tdc();
    lda8(dp8(0x16)); tax(); stx_dp(0x28);
    ldx(a_with(k)); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    ldx(0x0014); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C));
    inx();
    stx_dp(0x16);
}
/* C1:2045  Target list 99C0 from the slots X..$80-1 that can be chosen
 * (960F, the current one, goes first); with 960C bit 7 the whole list is
 * copied to A62D. */
static void target_list(u16 end, u16 start, int inc_960a, int all)
{
    ldx(end); stx_dp(0x80);
    ldx(start);
    if (all) { lda8(0x80); abs8_set(0x960C, 0x80); }
    if (inc_960a) inc_abs(0x960A);
    tdc(); tay();
    do {
        lda8(abs8(0x96F5 + g_cpu.X));
        if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x9FF7 + g_cpu.X));
        if (g_cpu._flag_N) goto next;
        lda8(abs8(0xA0A8 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0x960D) & 0x7F);
            dc_cmp8((u8)g_cpu.A, 0x04);
            if (!g_cpu._flag_Z) goto next;
        }
        lda8(abs8(0xA09B + g_cpu.X));
        if (!g_cpu._flag_Z) goto next;
        cpx(abs_x(0x960F));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x960F));
            abs8_set(0x99C0, (u8)g_cpu.A);
        } else {
            lda8((u8)g_cpu.X);
            abs8_set(0x99C1 + g_cpu.Y, (u8)g_cpu.A);
            iny();
        }
    next:
        inx();
        cpx(dp_x(0x80));
    } while (!g_cpu._flag_Z);
    dc_C127C5_glue();
    lda8(abs8(0x9614));
    tax();
    lda8(abs8(0x99C0 + g_cpu.X));
    abs8_set(0xA62D, (u8)g_cpu.A);
    lda8(abs8(0x960C));
    if (!g_cpu._flag_N) return;
    ldx(0x000A);
    do {
        lda8(abs8(0x99C0 + g_cpu.X));
        abs8_set(0xA62D + g_cpu.X, (u8)g_cpu.A);
        dex();
    } while (!g_cpu._flag_N);
}
DC_ENTRY(C1203A, RTS) { target_list(0x0003, 0x0000, 1, 0); }
DC_ENTRY(C1209A, RTS) { target_list(0x0003, 0x0000, 0, 1); }
DC_ENTRY(C120A9, RTS) { target_list(0x000B, 0x0003, 1, 0); }
DC_ENTRY(C120B6, RTS) { target_list(0x000B, 0x0003, 0, 1); }
DC_ENTRY(C120C6, RTS) { target_list(0x000B, 0x0000, 0, 1); }
/* C1:AE21  Keeps one entry of the AECC list: the one marked 80 (with a
 * match in B163 when there are several); AF24 counts when none. */
DC_ENTRY(C1AE21, RTS)
{
    lda8(abs8(0xAECB));
    if (g_cpu._flag_Z) { inc_abs(0xAF24); return; }
    dc_cmp8((u8)g_cpu.A, 0x01);
    if (!g_cpu._flag_Z) {
        tdc(); tax(); tay();
        for (;;) {
            lda8(abs8(0xAECC + g_cpu.X) & 0x0F);
            dc_cmp8((u8)g_cpu.A, abs8(0xB163 + g_cpu.Y));
            if (g_cpu._flag_Z) {
                lda8(abs8(0xAECC + g_cpu.Y));
                if (g_cpu.A & 0x80) break;
            }
            iny();
            lda8((u8)g_cpu.Y);
            dc_cmp8((u8)g_cpu.A, 0x0B);
            if (g_cpu._flag_C) {
                tdc(); tay();
                inx();
                lda8((u8)g_cpu.X);
                dc_cmp8((u8)g_cpu.A, 0x0B);
                if (g_cpu._flag_C) { inc_abs(0xAF24); return; }
            }
        }
    } else {
        tdc(); tax();
        for (;;) {
            lda8(abs8(0xAECC + g_cpu.X));
            if (g_cpu.A & 0x80) break;
            inx();
            lda8((u8)g_cpu.X);
            dc_cmp8((u8)g_cpu.A, abs8(0xAECB));
            if (g_cpu._flag_Z) break;
        }
    }
    lda8((u8)g_cpu.A & 0x7F);
    abs8_set(0xAECC, (u8)g_cpu.A);
    lda8(0x01);
    abs8_set(0xAECB, 0x01);
}
/* C1:561F  Places the target(s) (or the attacker's A240/A250 entry) at 80/81. */
static void place_targets(void)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        lda8(abs8(0xA2B6));
        dc_set_a8(asl8((u8)g_cpu.A));
        tax();
        lda8(dp8(0x80)); abs8_set(0xA240 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xA241 + g_cpu.X, 0);
        lda8(dp8(0x81)); abs8_set(0xA250 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xA251 + g_cpu.X, 0);
    } else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax();
            lda8(abs8(0xA014 + g_cpu.X));
            tax();
            lda8(dp8(0x80)); abs8_set(0x1D0C + g_cpu.X, (u8)g_cpu.A);
            lda8(dp8(0x81)); abs8_set(0x1D23 + g_cpu.X, (u8)g_cpu.A);
        } else {
            tdc(); tay();
            for (;;) {
                lda8(abs8(0xA018 + g_cpu.Y));
                if (g_cpu._flag_N) break;
                tax();
                lda8(dp8(0x80)); abs8_set(0x1D0C + g_cpu.X, (u8)g_cpu.A);
                lda8(dp8(0x81)); abs8_set(0x1D23 + g_cpu.X, (u8)g_cpu.A);
                iny();
            }
        }
    }
    lda8(dp8(0x8E));
    script_advance();
}
DC_ENTRY(C1566C, RTS)
{
    lda8(0x01); dp8_set(0x8E, 0x01);
    lda8(abs8(0x5D9E)); dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0x5D9F)); dp8_set(0x81, (u8)g_cpu.A);
    place_targets();
}
/* C1:76D8  A2B0/A2B2 = (sum of the 3 targets' X) / 3 twice (the Y sum is
 * computed but the X sum is divided both times). */
DC_ENTRY(C176D8, RTS)
{
    for (int k = 0; k < 3; k++) {
        lda8(abs8(0xA014 + k));
        tax();
        lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0x80 + k * 4, (u8)g_cpu.A); dp8_set(0x81 + k * 4, 0);
        lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0x82 + k * 4, (u8)g_cpu.A); dp8_set(0x83 + k * 4, 0);
    }
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0x80), dp16(0x84)));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x88)));
    dp16_set(0x80, g_cpu.A);
    lda16(dc_adc16(dp16(0x82), dp16(0x86)));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x8A)));
    dp16_set(0x82, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    for (int k = 0; k < 2; k++) {
        lda8(dp8(0x80)); dp8_set(0xB1, (u8)g_cpu.A);
        lda8(dp8(0x81)); dp8_set(0xB2, (u8)g_cpu.A);
        lda8(0x03); dp8_set(0xB3, 0x03);
        dc_C100D7_glue();
        lda8(dp8(0xB5));
        abs8_set(k ? 0xA2B2 : 0xA2B0, (u8)g_cpu.A);
    }
}
/* C1:285A  Hit box of object X: 975A/9770 = X -/+ 971E (clamped 0..FF),
 * 9765 = Y - 9729, 977B = Y (+8 unless 2989 bit 2). */
DC_ENTRY(C1285A, RTS)
{
    g_cpu._flag_C = 1;
    lda8(abs8(0xA039 + g_cpu.X));
    int neg = g_cpu._flag_N;
    lda8(dc_sbc8((u8)g_cpu.A, abs8(0x971E + g_cpu.X)));
    if (!neg && g_cpu._flag_N) tdc();
    abs8_set(0x975A + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(abs8(0xA039 + g_cpu.X));
    neg = g_cpu._flag_N;
    lda8(dc_adc8((u8)g_cpu.A, abs8(0x971E + g_cpu.X)));
    if (neg && !g_cpu._flag_N) lda8(0xFF);
    abs8_set(0x9770 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 1;
    lda8(abs8(0xA050 + g_cpu.X));
    neg = g_cpu._flag_N;
    lda8(dc_sbc8((u8)g_cpu.A, abs8(0x9729 + g_cpu.X)));
    if (!neg && g_cpu._flag_N) tdc();
    abs8_set(0x9765 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x2989) & 0x04);
    if (!g_cpu._flag_Z) lda8(abs8(0xA050 + g_cpu.X));
    else { g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA050 + g_cpu.X), 0x08)); }
    abs8_set(0x977B + g_cpu.X, (u8)g_cpu.A);
}
/* C1:48EC  Indexes the enemy AI script at CE:(9885): A280 = start of each of
 * the 16 sections, A2A0 = their entry counts. */
DC_ENTRY(C148EC, RTS)
{
    ldx(abs_x(0x9885));
    stx_abs(0xA280);
    tdc(); tay();
    dp16_set(0x80, g_cpu.Y);
    dp16_set(0x82, g_cpu.Y);
    for (;;) {
        lda8(bus8(0xCE0000 + g_cpu.X) & 0xE0);
        if (g_cpu._flag_Z) goto skip;
        if (!g_cpu._flag_N) {
            lda8((u8)g_cpu.A & 0x40);
            if (!g_cpu._flag_Z) {
                inx(); inx(); inx(); inx();
                dp8_set(0x80, (u8)(dp8(0x80) + 1));
                lda8(dp8(0x82)); tay();
                lda8(dp8(0x80));
                abs8_set(0xA2A0 + g_cpu.Y, (u8)g_cpu.A);
                dp8_set(0x80, 0);
                continue;
            }
            lda8(dp8(0x82)); tay();
            tdc();
            abs8_set(0xA2A0 + g_cpu.Y, (u8)g_cpu.A);
        }
        inx(); inx(); inx(); inx();
        dp8_set(0x82, (u8)(dp8(0x82) + 1));
        lda8(dp8(0x82));
        dc_cmp8((u8)g_cpu.A, 0x10);
        if (g_cpu._flag_Z) return;
        dc_set_a8(asl8((u8)g_cpu.A));
        tay();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(g_cpu.X);
        abs16_set(0xA280 + g_cpu.Y, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x80, 0);
    skip:
        inx(); inx(); inx(); inx();
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
    }
}
/* C1:CC71  HP 5E34 / 5EB4 / 5F34 of party slot B1BE[A] -= the scaled B3BA.. value. */
DC_ENTRY(C1CC71, RTS)
{
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    tax();
    lda8(abs8(0xB1BE + g_cpu.X));
    u16 src = 0xB3BC, hp = 0x5F34;
    if (g_cpu._flag_Z) { src = 0xB3BA; hp = 0x5E34; }
    else { dc_cmp8((u8)g_cpu.A, 0x01); if (g_cpu._flag_Z) { src = 0xB3BB; hp = 0x5EB4; } }
    lda8(abs8(src));
    dc_C1CBF6_glue();
    lda8((u8)g_cpu.Y);
    dc_set_mx(0, g_cpu.x_flag);
    dp16_set(0x0E, g_cpu.A);
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(abs16(hp), dp16(0x0E)));
    abs16_set(hp, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:5E87  993C (sprite set) of the target(s) = 9888; an enemy target with
 * 99D2 set also copies 2000.. from 99D3 * 2 to 2200. */
static void sprite_restore(void);
DC_ENTRY(C15E87, RTS)
{
    lda8(0x01); dp8_set(0x8E, 0x01);
    sprite_restore();
}
/* C1:5E8B  (advance by $8E) */
static void sprite_restore(void)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x04);
    if (g_cpu._flag_C) {
        tdc(); tax();
        for (;;) {
            lda8(abs8(0xA018 + g_cpu.X));
            if (g_cpu._flag_N) break;
            tay();
            lda8(abs8(0x9888 + g_cpu.Y)); abs8_set(0x993C + g_cpu.Y, (u8)g_cpu.A);
            inx();
        }
    } else {
        tax();
        lda8(abs8(0xA014 + g_cpu.X));
        tax();
        dc_cmp8((u8)g_cpu.A, 0x03);
        int big = g_cpu._flag_C;
        if (big) { lda8(abs8(0x99D2)); big = !g_cpu._flag_Z; }
        lda8(abs8(0x9888 + g_cpu.X)); abs8_set(0x993C + g_cpu.X, (u8)g_cpu.A);
        if (big) {
            lda8(abs8(0x99D3));
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = g_cpu.A >> 15;
            lda16((u16)(g_cpu.A << 1));
            tax();
            do {
                lda16(abs16(0x2000 + g_cpu.X));
                abs16_set(0x2200 + g_cpu.X, g_cpu.A);
                inx(); inx();
                cpx(0x0100);
            } while (!g_cpu._flag_Z);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
        }
    }
    lda8(dp8(0x8E));
    script_advance();
}
/* C1:4F54  A2FE / A324 (or A309 / A32F for the attacker) = CC:F5BC / F4BC
 * entry 9895 - 8. */
static void stat_pair(u16 lo, u16 hi)
{
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(abs8(0x9895), 0x08));
    tax();
    lda8(bus8(0xCCF5BC + g_cpu.X)); abs8_set(lo + g_cpu.Y, (u8)g_cpu.A);
    lda8(bus8(0xCCF4BC + g_cpu.X)); abs8_set(hi + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C14F54, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldy(abs_x(0xA2B6));
        stat_pair(0xA309, 0xA32F);
    } else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax();
            lda8(abs8(0xA014 + g_cpu.X));
            tay();
            stat_pair(0xA2FE, 0xA324);
        } else {
            tdc(); tay();
            if (g_cpu.x_flag) dp8_set(0x80, (u8)g_cpu.Y); else dp16_set(0x80, g_cpu.Y);
            for (;;) {
                ldy(dp_x(0x80));
                lda8(abs8(0xA018 + g_cpu.Y));
                if (g_cpu._flag_N) break;
                tay();
                stat_pair(0xA2FE, 0xA324);
                dp8_set(0x80, (u8)(dp8(0x80) + 1));
            }
        }
    }
    lda8(0x01);
    script_advance();
}
/* X = word FD:A849 entry A (the record of battle slot A). */
static void fd_a849(void)
{
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xFDA849 + g_cpu.X));
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:BCE1  Resets the ATB of party slot B18B, then switches B18B to B3F4,
 * clearing its record and releasing the slots linked from byte 5. */
DC_ENTRY(C1BCE1, RTS)
{
    lda8(abs8(0xB18B));
    tax();
    lda8(abs8(0xB158 + g_cpu.X)); abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A);
    dc_C1BD6F_glue();
    lda8(abs8(0xB03A + g_cpu.X));
    if (!g_cpu._flag_Z) { lda8(0x01); abs8_set(0xB03A + g_cpu.X, 0x01); }
    tdc();
    lda8(abs8(0xB3F4));
    abs8_set(0xB18B, (u8)g_cpu.A);
    fd_a849();
    abs8_set(0x0001 + g_cpu.X, 0); abs8_set(0x0002 + g_cpu.X, 0);
    abs8_set(0x0003 + g_cpu.X, 0); abs8_set(0x0006 + g_cpu.X, 0);
    lda8(abs8(0x0005 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) {
        dp8_set(0x28, (u8)g_cpu.A);
        lda8((u8)g_cpu.A & 0x0F);
        fd_a849();
        abs8_set(0x0001 + g_cpu.X, 0);
        lda8(0xFF); abs8_set(0x0005 + g_cpu.X, 0xFF);
        lda8(dp8(0x28));
        lsr_a(4);
        lda8((u8)g_cpu.A & 0x0F);
        dc_cmp8((u8)g_cpu.A, 0x0F);
        if (!g_cpu._flag_Z) {
            fd_a849();
            abs8_set(0x0001 + g_cpu.X, 0);
            lda8(0xFF); abs8_set(0x0005 + g_cpu.X, 0xFF);
        }
    }
    tdc();
    lda8(abs8(0xB18B));
    fd_a849();
    lda8(0xFF); abs8_set(0x0005 + g_cpu.X, 0xFF);
    abs8_set(0xAF22, 0);
}

/* ---- batch 7 --------------------------------------------------------------- */
DC_ENTRY(C1FDCB, RTL) { dc_C1AF22_glue(); }
DC_ENTRY(C1E76E, RTS)
{
    phx();
    tdc(); tax();
    lda8((u8)(dp8(0x1A) + 1));
    dc_C1AF22_glue();
    dp8_set(0x1A, (u8)g_cpu.A);
    plx();
}
/* Y's box against X's: the rows/columns 9765..977B overlap (after the
 * columns were found to overlap). */
static int rows_overlap(void)
{
    lda8(abs8(0x9765 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, abs8(0x9765 + g_cpu.Y));
    if (!g_cpu._flag_C) {
        lda8(abs8(0x977B + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, abs8(0x9765 + g_cpu.Y));
        return g_cpu._flag_C;
    }
    dc_cmp8((u8)g_cpu.A, abs8(0x977B + g_cpu.Y));
    return g_cpu._flag_Z || !g_cpu._flag_C;
}
/* C1:28B0  A = 80 (party) / 81 (enemy) when the box of object $80 touches
 * another active object's box, else 0. */
DC_ENTRY(C128B0, RTS)
{
    tdc(); tay();
    ldx(dp_x(0x80));
    do {
        cpy(dp_x(0x80));
        if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x96F5 + g_cpu.Y));
        if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x9FF7 + g_cpu.Y));
        if (g_cpu._flag_N) goto next;
        lda8(abs8(0xA5CD + g_cpu.Y));
        if (g_cpu._flag_N) goto next;
        lda8(abs8(0x975A + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, abs8(0x975A + g_cpu.Y));
        if (!g_cpu._flag_C) {
            lda8(abs8(0x9770 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, abs8(0x975A + g_cpu.Y));
            if (!g_cpu._flag_C) goto next;
        } else {
            dc_cmp8((u8)g_cpu.A, abs8(0x9770 + g_cpu.Y));
            if (!g_cpu._flag_Z && g_cpu._flag_C) goto next;
        }
        if (rows_overlap()) {
            lda8(0x80);
            cpy(0x0003);
            if (g_cpu._flag_C) lda8(0x81);
            return;
        }
    next:
        iny();
        cpy(0x000B);
    } while (!g_cpu._flag_Z);
    tdc();
}
DC_ENTRY(C17C2B, RTS)
{
    lda8(abs8(0x1D0C + g_cpu.X)); abs8_set(0xA039 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); abs8_set(0xA050 + g_cpu.X, (u8)g_cpu.A);
    dc_C1285A_glue();
    dc_C128B0_glue();
}
DC_ENTRY(C1283D, RTS)                            /* hit boxes of all active objects */
{
    tdc(); tax();
    do {
        lda8(abs8(0x96F5 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0x1D0C + g_cpu.X)); abs8_set(0xA039 + g_cpu.X, (u8)g_cpu.A);
            lda8(abs8(0x1D23 + g_cpu.X)); abs8_set(0xA050 + g_cpu.X, (u8)g_cpu.A);
            dc_C1285A_glue();
        }
        inx();
        cpx(0x000B);
    } while (!g_cpu._flag_Z);
}
/* C1:ED89  AD89 *= $1E (critical) when the level roll (AF79 of 64) is at most 5E72. */
DC_ENTRY(C1ED89, RTS)
{
    ldx(abs_x(0xB1F4));
    lda8(abs8(0x5E72 + g_cpu.X));
    dp8_set(0x1C, (u8)g_cpu.A);
    tdc(); tax();
    lda8(0x64);
    dc_C1AF79_glue();
    dc_cmp8((u8)g_cpu.A, dp8(0x1C));
    if (!g_cpu._flag_Z && g_cpu._flag_C) return;
    ldx(abs_x(0xAD89)); stx_dp(0x28);
    lda8(dp8(0x1E)); tax(); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_abs(0xAD89);
    lda8(abs8(0xAE9B) | 0x80);
    abs8_set(0xAE9B, (u8)g_cpu.A);
}
/* C1:DC64  Hit roll: $16 = 1 when a 1..100 roll is below $16 * 100 / $18 (capped 99%). */
DC_ENTRY(C1DC64, RTS)
{
    lda8(dp8(0x16));
    dc_cmp8((u8)g_cpu.A, dp8(0x18));
    if (g_cpu._flag_C) {
        lda8(0x63); dp8_set(0x16, 0x63);
        lda8(0x64); dp8_set(0x18, 0x64);
    }
    tdc();
    lda8(dp8(0x16)); tax(); stx_dp(0x28);
    ldx(0x0064); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    lda8(dp8(0x18)); tax(); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C)); stx_dp(0x16);
    tdc(); tax();
    lda8(0x64);
    dc_C1AF22_glue();
    dp8_set(0x18, (u8)g_cpu.A);
    lda8(dp8(0x16));
    dc_cmp8((u8)g_cpu.A, dp8(0x18));
    if (g_cpu._flag_C) { lda8(0x01); dp8_set(0x16, 0x01); }
    else { tdc(); dp8_set(0x16, (u8)g_cpu.A); }
    tdc();
    abs8_set(0xAE4F, (u8)g_cpu.A);
}
/* C1:2926  A = FF when a tile of the 16x16 map 7BFD under the box of $80 is
 * blocked (bit 6, or bit 7 unless 9875), else 0. */
DC_ENTRY(C12926, RTS)
{
    ldx(dp_x(0x80));
    lda8(abs8(0x9765 + g_cpu.X)); lsr_a(4); dp8_set(0x82, (u8)g_cpu.A);
    lda8(abs8(0x975A + g_cpu.X)); lsr_a(4); dp8_set(0x83, (u8)g_cpu.A);
    lda8(abs8(0x977B + g_cpu.X)); lsr_a(4); lda8((u8)(g_cpu.A + 1)); dp8_set(0x84, (u8)g_cpu.A);
    lda8(abs8(0x9770 + g_cpu.X)); lsr_a(4); lda8((u8)(g_cpu.A + 1)); dp8_set(0x85, (u8)g_cpu.A);
    do {
        dp8_set(0x86, 0);
        do {
            lda8(dp8(0x82));
            lda8(asl8(asl8(asl8(asl8((u8)g_cpu.A)))));
            g_cpu._flag_C = 0;
            lda8(dc_adc8((u8)g_cpu.A, dp8(0x86)));
            lda8(dc_adc8((u8)g_cpu.A, dp8(0x83)));
            tay();
            lda8(abs8(0x7BFD + g_cpu.Y) & 0x40);
            if (!g_cpu._flag_Z) goto hit;
            lda8(abs8(0x9875));
            if (g_cpu._flag_Z) {
                lda8(abs8(0x7BFD + g_cpu.Y));
                if (g_cpu._flag_N) goto hit;
            }
            dp8_set(0x86, (u8)(dp8(0x86) + 1));
            g_cpu._flag_C = 0;
            lda8(dc_adc8(dp8(0x86), dp8(0x83)));
            dc_cmp8((u8)g_cpu.A, dp8(0x85));
        } while (!g_cpu._flag_C);
        dp8_set(0x82, (u8)(dp8(0x82) + 1));
        lda8(dp8(0x82));
        dc_cmp8((u8)g_cpu.A, dp8(0x84));
    } while (!g_cpu._flag_C);
    tdc();
    return;
hit:
    tdc();
    lda8((u8)(g_cpu.A - 1));
}
/* C1:EF39  AD89 = AD89 * 4 / (element resistance 5E6C of the element in B190). */
DC_ENTRY(C1EF39, RTS)
{
    tdc();
    lda8(abs8(0xB190));
    dp8_set(0x2C, (u8)g_cpu.A);
    u8 a = (u8)g_cpu.A, e;
    if (a & 0x80) e = 0xFF;
    else if (a & 0x40) e = 1;
    else if (a & 0x20) e = 2;
    else if (a & 0x10) e = 3;
    else { g_cpu._flag_Z = 1; return; }
    if (e == 0xFF) tdc(); else lda8(e);
    abs8_set(0xB2CD, (u8)g_cpu.A);
    lda8(0x04); abs8_set(0xB2CF, 0x04);
    lda8(abs8(0xB2CD));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, abs16(0xB1F6)));
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0x5E6C + g_cpu.X));
    abs8_set(0xB2CE, (u8)g_cpu.A);
    if (g_cpu.A & 0x80) { lda8(abs8(0xB1FC) | 0x40); abs8_set(0xB1FC, (u8)g_cpu.A); }
    lda8(abs8(0xB2CE) & 0x3F);
    abs8_set(0xB2CE, (u8)g_cpu.A);
    lda8(abs8(0x5E6C + g_cpu.X));
    if (g_cpu.A & 0x40) {
        g_cpu._flag_C = 1;
        lda8(dc_sbc8(abs8(0xB2CE), 0x02));
        abs8_set(0xB2CE, (u8)g_cpu.A);
    }
    ldx(abs_x(0xAD89)); stx_dp(0x28);
    lda8(abs8(0xB2CF)); tax();
    dp8_set(0x2A, (u8)g_cpu.A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    lda8(abs8(0xB2CE)); tax(); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C));
    stx_abs(0xAD89);
}
/* C1:2332  Target list 99C0: object 9605 first, then the others (party or
 * enemies by 9604) within 20 rows of it. */
DC_ENTRY(C12332, RTS)
{
    lda8(abs8(0x9605));
    tax(); stx_dp(0x88);
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(abs8(0x1D23 + g_cpu.X), 0x20));
    if (!g_cpu._flag_C) tdc();
    dp8_set(0x85, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), 0x20));
    if (g_cpu._flag_C) lda8(0xFF);
    dp8_set(0x87, (u8)g_cpu.A);
    dc_C127D9_glue();
    ldx(0x000B); stx_dp(0x90);
    ldx(0x0003); stx_dp(0x8E);
    lda8(abs8(0x9604));
    if (!g_cpu._flag_Z) { stx_dp(0x90); tdc(); tax(); stx_dp(0x8E); }
    tdc(); tay();
    do {
        ldx(dp_x(0x8E));
        cpx(dp_x(0x88));
        if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x96F5 + g_cpu.X)); if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x9FF7 + g_cpu.X)); if (g_cpu._flag_N) goto next;
        lda8(abs8(0xA0A8 + g_cpu.X)); if (!g_cpu._flag_Z) goto next;
        lda8(abs8(0xA09B + g_cpu.X)); if (!g_cpu._flag_Z) goto next;
        lda8(abs8(0x1D23 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, dp8(0x85));
        if (!g_cpu._flag_C) goto next;
        dc_cmp8((u8)g_cpu.A, dp8(0x87));
        if (!g_cpu._flag_Z && g_cpu._flag_C) goto next;
        lda8(dp8(0x8E));
        abs8_set(0x99C1 + g_cpu.Y, (u8)g_cpu.A);
        iny();
    next:
        dp8_set(0x8E, (u8)(dp8(0x8E) + 1));
        lda8(dp8(0x8E));
        dc_cmp8((u8)g_cpu.A, dp8(0x90));
    } while (!g_cpu._flag_Z);
    lda8(dp8(0x88));
    abs8_set(0x99C0, (u8)g_cpu.A);
}
/* C1:F45F  Steal-style HP/MP drain of 5E5A,$00 by B2DB when the tech allows it. */
DC_ENTRY(C1F45F, RTS)
{
    tdc();
    lda8(abs8(0xB2D1));
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    lda8(bus8(0xCC276A + g_cpu.X));
    dp8_set(0x3B, (u8)g_cpu.A);
    lda8(abs8(0xB2D1));
    tax();
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x2830 + g_cpu.X), dp8(0x3B)));
    tax(); stx_dp(0x28);
    ldx(0x000B); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C));
    lda8(bus8(0xCC1BEB + g_cpu.X) & 0x80);
    if (!g_cpu._flag_Z) {
        tdc();
        lda8(abs8(0xB2D1));
        tay();
        lda8(0x01);
        for (;;) {
            cpy(0);
            if (g_cpu._flag_Z) break;
            lda8(asl8((u8)g_cpu.A));
            ldy((u16)(g_cpu.Y - 1));
        }
        lda8((u8)g_cpu.A & bus8(0x7F01E0));
        if (g_cpu._flag_Z) return;
    }
    lda8(abs8(0xB2AF) | 0x10);
    abs8_set(0xB2AF, (u8)g_cpu.A);
    tdc();
    ldx(dp_x(0x00));
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0x5E5A + g_cpu.X));
    dc_cmp16(g_cpu.A, abs16(0xB2DB));
    if (g_cpu._flag_Z || !g_cpu._flag_C) {
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(0x00);
        abs8_set(0xB2DA, 0);
        dc_set_mx(0, g_cpu.x_flag);
        lda16(abs16(0xB2DB));
        ldx(dp_x(0x00));
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(g_cpu.A, abs16(0x5E5A + g_cpu.X)));
    } else {
        lda16(abs16(0x5E5A + g_cpu.X));
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(g_cpu.A, abs16(0xB2DB)));
    }
    abs16_set(0x5E5A + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:53B0  A = 1 when the angle $82 lies in the half turn after $DB (by
 * quadrants of 40), else 0. */
DC_ENTRY(C153B0, RTS)
{
    lda8(dp8(0xDB)); lsr_a(6); dp8_set(0x86, (u8)g_cpu.A);
    lda8(dp8(0x82)); lsr_a(6); dp8_set(0x87, (u8)g_cpu.A);
    dc_cmp8((u8)g_cpu.A, dp8(0x86));
    int r;   /* 0: 5403, 1: 5406, 2: 53FA */
    if (g_cpu._flag_Z) {
        lda8(dp8(0x82));
        dc_cmp8((u8)g_cpu.A, dp8(0xDB));
        r = g_cpu._flag_C ? 1 : 0;
    } else {
        u8 q = dp8(0x87), p = dp8(0x86);
        lda8(q);
        if (q == 0) { lda8(p); r = (p == 1) ? 0 : (p == 2) ? 2 : 1; if (p == 1 || p == 2) lda8(0); else lda8((u8)(p - 2)); }
        else if (q == 1) { lda8(p); if (p == 0) r = 1; else { lda8((u8)(p - 2)); r = (p == 2) ? 0 : 2; } }
        else if (q == 2) { lda8(p); if (p == 0) r = 2; else { lda8((u8)(p - 1)); r = (p == 1) ? 1 : 0; } }
        else { lda8(p); if (p == 0) r = 0; else { lda8((u8)(p - 1)); r = (p == 1) ? 2 : 1; } }
    }
    if (r == 2) {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0x82), 0x80));
        dc_cmp8((u8)g_cpu.A, dp8(0xDB));
        r = g_cpu._flag_C ? 0 : 1;
    }
    if (r == 0) tdc(); else lda8(0x01);
}
/* C1:1808  Cursor sprites over the attacker 95D5 and the target(s) in 9EE7. */
DC_ENTRY(C11808, RTS)
{
    lda8(abs8(0x9EE7));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) { dc_C118AA_glue(); return; }
    lda8(abs8(0x0900) | 0x55); abs8_set(0x0900, (u8)g_cpu.A);
    lda8(0xF0);
    abs8_set(0x0701, 0xF0); abs8_set(0x0705, 0xF0); abs8_set(0x0709, 0xF0); abs8_set(0x070D, 0xF0);
    lda8(0x32);
    abs8_set(0x0703, 0x32); abs8_set(0x0707, 0x32); abs8_set(0x070B, 0x32);
    abs8_set(0x0702, 0); abs8_set(0x0706, 0); abs8_set(0x070A, 0);
    lda8(abs8(0x95D5));
    for (int k = 0; k < 3; k++) {
        if (k == 1) { lda8(abs8(0x9EE7) & 0x0F); }
        if (k == 2) {
            lda8(abs8(0x0900) & 0xFE & 0xFB); abs8_set(0x0900, (u8)g_cpu.A);
            lda8(abs8(0x9EE7) & 0xF0);
            dc_cmp8((u8)g_cpu.A, 0xF0);
            if (g_cpu._flag_Z) break;
            lsr_a(4);
        }
        tax();
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D0C + g_cpu.X), abs8(0x9708 + g_cpu.X)));
        g_cpu._flag_C = 1;
        lda8(dc_sbc8((u8)g_cpu.A, 0x10));
        abs8_set(0x0700 + k * 4, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), abs8(0x9713 + g_cpu.X)));
        abs8_set(0x0701 + k * 4, (u8)g_cpu.A);
        if (k == 2) { lda8(abs8(0x0900) & 0xEF); abs8_set(0x0900, (u8)g_cpu.A); }
    }
    dp8_set(0xEE, 0); dp8_set(0xEF, 0);
}
/* C1:7920  A2B0/A2B2 = weighted mix (weights CC:FB7F by command Y - 41, /8)
 * of target A014's position and the attacker's A240/A250. */
DC_ENTRY(C17920, RTS)
{
    ldx(dp_x(0x86));
    phx();
    lda8((u8)g_cpu.Y);
    sub_a(0x41);
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    lda8(bus8(0xCCFB7F + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A); dp8_set(0xAE, (u8)g_cpu.A);
    lda8(bus8(0xCCFB80 + g_cpu.X)); dp8_set(0x81, (u8)g_cpu.A);
    lda8(abs8(0xA014));
    tax();
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xAD, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0x84, (u8)g_cpu.A);
    for (int k = 0; k < 2; k++) {
        if (k) {
            lda8(dp8(0x84)); dp8_set(0xAD, (u8)g_cpu.A);
            lda8(dp8(0x80)); dp8_set(0xAE, (u8)g_cpu.A);
        }
        dc_C10089_glue();
        ldx(dp_x(0xAF)); stx_dp(0x82);
        if (!k) {
            lda8(abs8(0xA240)); dp8_set(0xAD, (u8)g_cpu.A);
            lda8(abs8(0xA250)); dp8_set(0x85, (u8)g_cpu.A);
        } else {
            lda8(dp8(0x85)); dp8_set(0xAD, (u8)g_cpu.A);
        }
        lda8(dp8(0x81)); dp8_set(0xAE, (u8)g_cpu.A);
        dc_C10089_glue();
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0xAF), dp16(0x82)));
        lsr_a(3);
        dp16_set(0x82, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(dp8(0x82));
        abs8_set(k ? 0xA2B2 : 0xA2B0, (u8)g_cpu.A);
    }
    plx();
    stx_dp(0x86);
}
/* C1:2B49  9872 = FF when the squared distance between objects X and Y
 * exceeds the word in 9893 (set by the entry points), else 0. */
static u16 abs_diff16(void)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(dp16(0x80), dp16(0x82)));
    if (g_cpu._flag_N) lda16((u16)((g_cpu.A ^ 0xFFFF) + 1));
    return g_cpu.A;
}
static void dist_check_xy(u8 lo, u8 hi, int load_xy)
{
    lda8(lo); abs8_set(0x9893, lo);
    lda8(hi); abs8_set(0x9894, hi);
    if (load_xy) {
        lda8(abs8(0x986F)); tax();
        lda8(abs8(0x9870)); tay();
    }
    lda8(abs8(0xA039 + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A); dp8_set(0x81, 0);
    lda8(abs8(0xA039 + g_cpu.Y)); dp8_set(0x82, (u8)g_cpu.A); dp8_set(0x83, 0);
    dp16_set(0xAD, abs_diff16());
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0xAD)); dp8_set(0xAE, (u8)g_cpu.A);
    lda8(abs8(0xA050 + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A); dp8_set(0x81, 0);
    lda8(abs8(0xA050 + g_cpu.Y)); dp8_set(0x82, (u8)g_cpu.A); dp8_set(0x83, 0);
    dp16_set(0x80, abs_diff16());
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    dc_C10089_glue();
    ldx(dp_x(0xAF)); stx_dp(0x82);
    lda8(dp8(0x80)); dp8_set(0xAD, (u8)g_cpu.A); dp8_set(0xAE, (u8)g_cpu.A);
    dc_C10089_glue();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0x82), dp16(0xAF)));
    dp16_set(0xAF, g_cpu.A);
    dc_cmp16(g_cpu.A, abs16(0x9893));
    int far = !g_cpu._flag_Z && g_cpu._flag_C;
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    if (far) lda8(0xFF);
    abs8_set(0x9872, (u8)g_cpu.A);
}
static void dist_check(u8 lo, u8 hi) { dist_check_xy(lo, hi, 0); }
DC_ENTRY(C12AE3, RTS) { dist_check(0x40, 0x06); }
DC_ENTRY(C12AEF, RTS) { dist_check(0x00, 0x04); }
DC_ENTRY(C12AFB, RTS) { dist_check(0x00, 0x10); }
DC_ENTRY(C12B07, RTS) { dist_check(0x10, 0x00); }
DC_ENTRY(C12B13, RTS) { dist_check(0x10, 0x00); }

/* ---- batch 8 --------------------------------------------------------------- */
DC_ENTRY(C12B1F, RTS) { dist_check(0x00, 0x01); }
DC_ENTRY(C12B37, RTS) { dist_check_xy(0x00, 0x04, 1); }
DC_ENTRY(C12B2B, RTS) { dist_check_xy(0x00, 0x09, 1); }
/* C1:2C2A  Pushes object 986F outward along angle $DB (radius $AE growing by
 * 9734) until it is clear of 9870 (2AEF), the map and the other boxes;
 * then restores its position from 8A/8B. */
DC_ENTRY(C12C2A, RTS)
{
    for (;;) {
        lda8(abs8(0x986F)); tax();
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x9734 + g_cpu.X), dp8(0xAE)));
        dp8_set(0xAE, (u8)g_cpu.A);
        lda8(dp8(0xDB));
        dc_C101F9_glue();
        dp8_set(0x8C, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0xDB), 0x40));
        dc_C101F9_glue();
        dp8_set(0x8E, (u8)g_cpu.A);
        lda8(abs8(0x986F)); tay();
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D23 + g_cpu.Y), dp8(0x8C))); abs8_set(0xA050 + g_cpu.Y, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D0C + g_cpu.Y), dp8(0x8E))); abs8_set(0xA039 + g_cpu.Y, (u8)g_cpu.A);
        dc_C12AEF_glue();
        lda8(abs8(0x9872));
        if (!g_cpu._flag_N) break;
        lda8(abs8(0x986F)); tax(); stx_dp(0x80);
        dc_C1285A_glue();
        abs8_set(0x9875, 0);
        dc_C12926_glue();
        if (g_cpu._flag_N) { lda8(0xFF); abs8_set(0x9872, 0xFF); break; }
        lda8(abs8(0x9871));
        if (!g_cpu._flag_Z) {
            dc_C128B0_glue();
            if (g_cpu._flag_N) { lda8(0xFF); abs8_set(0x9872, 0xFF); break; }
        }
        lda8(abs8(0x986F)); tax();
        lda8(dp8(0x8A)); abs8_set(0x1D0C + g_cpu.X, (u8)g_cpu.A);
        lda8(dp8(0x8B)); abs8_set(0x1D23 + g_cpu.X, (u8)g_cpu.A);
    }
    lda8(abs8(0x986F)); tax();
    lda8(dp8(0x8A)); abs8_set(0x1D0C + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x8B)); abs8_set(0x1D23 + g_cpu.X, (u8)g_cpu.A);
    dc_C1285A_glue();
}
/* C1:6549  Moves D5/D6 a quarter of the way from D3/D4 (D7..DA = signed deltas). */
DC_ENTRY(C16549, RTS)
{
    dp8_set(0x82, 0); dp8_set(0x83, 0);
    for (int k = 0; k < 2; k++) {
        g_cpu._flag_C = 1;
        lda8(dc_sbc8(dp8(0xD5 + k), dp8(0xD3 + k)));
        dp8_set(0xD7 + k * 2, (u8)g_cpu.A);
        lda8(dc_sbc8(0x00, 0x00));
        dp8_set(0xD8 + k * 2, (u8)g_cpu.A);
        if (!g_cpu._flag_C) dp8_set(0x82 + k, (u8)(dp8(0x82 + k) + 1));
    }
    for (int k = 0; k < 2; k++) {
        lda8(dp8(0xD7 + k * 2) ^ dp8(0xD8 + k * 2));
        g_cpu._flag_C = 1;
        lda8(dc_sbc8((u8)g_cpu.A, dp8(0xD8 + k * 2)));
        dp8_set(0xDE + k * 2, (u8)g_cpu.A);
        dp8_set(0xDF + k * 2, 0);
    }
    for (int k = 0; k < 2; k++) { lda8(dp8(0xDE + k * 2)); lsr_a(2); dp8_set(0xDE + k * 2, (u8)g_cpu.A); }
    for (int k = 0; k < 2; k++) {
        lda8(dp8(0x82 + k));
        if (g_cpu._flag_Z) { g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0xD3 + k), dp8(0xDE + k * 2))); }
        else { g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0xD3 + k), dp8(0xDE + k * 2))); }
        dp8_set(0xD5 + k, (u8)g_cpu.A);
    }
}
/* C1:F7B9  Gives B28C experience to character B285: levels up (to 99) while
 * it covers the remaining exp 002B (next from CC:2632). */
DC_ENTRY(C1F7B9, RTS)
{
    lda8(0xFF); abs8_set(0xB28B, 0xFF);
    ldx(abs_x(0xB285));
    lda8(abs8(0x001D + g_cpu.X) & 0x80);
    if (!g_cpu._flag_Z) goto tail;
    ldx(abs_x(0xB285));
    lda8(abs8(0x0012 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0x63);
    if (g_cpu._flag_C) {
        tdc(); tax(); stx_abs(0xB28C);
        lda8(abs8(0xB2AF) | 0x80); abs8_set(0xB2AF, (u8)g_cpu.A);
        return;
    }
    lda8(abs8(0xB2AF) | 0x80); abs8_set(0xB2AF, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    ldx(abs_x(0xB285));
    lda16(abs16(0xB28C));
    dc_cmp16(g_cpu.A, abs16(0x002B + g_cpu.X));
    if (g_cpu._flag_C) {
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(0x00); abs8_set(0xB28B, 0);
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(abs16(0xB28C), abs16(0x002B + g_cpu.X)));
        abs16_set(0xB28C, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8((u8)(abs8(0x0012 + g_cpu.X) + 1));
        abs8_set(0x0012 + g_cpu.X, (u8)g_cpu.A);
        ldx(abs_x(0xB1F4));
        lda8(abs8(0xB2B0 + g_cpu.X) | 0x80); abs8_set(0xB2B0 + g_cpu.X, (u8)g_cpu.A);
        ldx(abs_x(0xB285));
        lda8(0x63);
        dc_cmp8(0x63, abs8(0x0012 + g_cpu.X));
        if (!g_cpu._flag_C) { abs8_set(0x0012 + g_cpu.X, 0x63); ldx(abs_x(0xB1F4)); }
        ldx(abs_x(0xB285));
        lda8(abs8(0x0012 + g_cpu.X));
        dc_set_a8(asl8((u8)g_cpu.A));
        tax();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(bus16(0xCC2632 + g_cpu.X));
        ldx(abs_x(0xB285));
        abs16_set(0x002B + g_cpu.X, g_cpu.A);
        ldx(abs_x(0xB1F4));
        u16 v = (u16)(abs16(0xB311 + g_cpu.X) + 1);
        abs16_set(0xB311 + g_cpu.X, v);
        dc_set_flags_nz16(v);
    } else {
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(abs16(0x002B + g_cpu.X), abs16(0xB28C)));
        abs16_set(0x002B + g_cpu.X, g_cpu.A);
        abs16_set(0xB28C, 0);
    }
tail:
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(abs_x(0xB285));
    lda8(abs8(0x0012 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0x63);
    if (!g_cpu._flag_C) return;
    tdc();
    abs8_set(0x002B + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x002C + g_cpu.X, (u8)g_cpu.A);
}
/* C1:EB68  Adds the AD9C damage entries ($1A rows of 2C bytes) into AD70
 * (keeping status 5) and caps each total at 9999. */
DC_ENTRY(C1EB68, RTS)
{
    tdc(); tax(); stx_dp(0x10); stx_dp(0x12);
    lda8(0x0C); dp8_set(0x10, 0x0C);
    lda8(dp8(0x1A)); dp8_set(0x12, (u8)g_cpu.A);
    do {
        do {
            dc_set_mx(0, g_cpu.x_flag);
            dp16_set(0x28, dp16(0x12));
            dp16_set(0x2A, 0x002C);
            lda16(0x002C);
            dc_C1C90B_glue();
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = 0;
            lda16(dc_adc16(dp16(0x2C), dp16(0x10)));
            tax();
            g_cpu._flag_C = 0;
            lda16(dc_adc16(abs16(0xAD9C + g_cpu.X), abs16(0xAD70 + g_cpu.X)));
            abs16_set(0xAD70 + g_cpu.X, g_cpu.A);
            lda16(abs16(0xAD9E + g_cpu.X));
            dc_cmp16(g_cpu.A, 0x0000);
            if (!g_cpu._flag_Z) {
                lda16(abs16(0xAD9E + g_cpu.X));
                dc_cmp16(g_cpu.A, 0x0005);
                int st = g_cpu._flag_Z;
                if (!st) {
                    lda16(abs16(0xAD72 + g_cpu.X));
                    dc_cmp16(g_cpu.A, 0x0005);
                    if (!g_cpu._flag_Z) { lda16(abs16(0xAD9E + g_cpu.X)); st = 1; }
                }
                if (st) abs16_set(0xAD72 + g_cpu.X, g_cpu.A);
            }
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            u8 n = (u8)(dp8(0x12) - 1); dp8_set(0x12, n);
            lda8(n);
        } while (!g_cpu._flag_Z);
        lda8(dp8(0x1A)); dp8_set(0x12, (u8)g_cpu.A);
        dp8_set(0x10, (u8)(dp8(0x10) + 4));
        lda8(dp8(0x10));
        dc_cmp8((u8)g_cpu.A, 0x2C);
    } while (!g_cpu._flag_C);
    ldx(0x000C); stx_dp(0x10);
    do {
        ldx(dp_x(0x10));
        dc_set_mx(0, g_cpu.x_flag);
        lda16(abs16(0xAD9C + g_cpu.X));
        dc_cmp16(g_cpu.A, 0x270F);
        if (g_cpu._flag_C) { lda16(0x270F); abs16_set(0xAD9C + g_cpu.X, 0x270F); }
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x10, (u8)(dp8(0x10) + 4));
        lda8(dp8(0x10));
        dc_cmp8((u8)g_cpu.A, 0x2C);
    } while (!g_cpu._flag_C);
}
/* C1:E8E2  Marks the slot's damage entry as status 5 (by the 80 / 40 bits of
 * $1A filtered by $1C) and records it through EBF8. */
static void mark_status5(void)
{
    dc_C1E89F_glue();
    lda8(dp8(0x0E));
    tax();
    lda8(0x05); abs8_set(0xAD9E + g_cpu.X, 0x05);
    tdc();
    abs8_set(0xAD9C + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0xAD9D + g_cpu.X, (u8)g_cpu.A);
    tdc();
    lda8(abs8(0xB1FD));
    tay();
    lda8(0x20); abs8_set(0xB202, 0x20);
    dc_C1EBF8_glue();
}
DC_ENTRY(C1E8E2, RTS)
{
    tdc();
    lda8(dp8(0x1C));
    dc_cmp8((u8)g_cpu.A, 0x01);
    lda8(dp8(0x1A) & (g_cpu._flag_Z ? 0x40 : 0x80));
    dp8_set(0x1A, (u8)g_cpu.A);
    tdc();
    lda8(dp8(0x1A));
    if (g_cpu.A & 0xC0) {
        mark_status5();
        lda8(abs8(0xAE94) | 0x40);
        abs8_set(0xAE94, (u8)g_cpu.A);
    } else mark_status5();
}
/* C1:F871  Level-up bonuses for character B285: max HP (0005, cap 999) and
 * max MP (0009, cap 99) from the first CC:258A / 25C2 row at or above its level. */
static void level_bonus(u32 tab, u16 field, u16 cap, int reset)
{
    for (;;) {
        tdc();
        ldx(abs_x(0xB285));
        lda8(abs8(0x0000 + g_cpu.X));
        lda8(asl8(asl8(asl8((u8)g_cpu.A))));
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, dp8(0x10)));
        tax(); stx_dp(0x00);
        lda8(bus8(tab + g_cpu.X));
        ldx(abs_x(0xB285));
        dc_cmp8((u8)g_cpu.A, abs8(0x0012 + g_cpu.X));
        if (g_cpu._flag_C) break;
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x10, (u8)(dp8(0x10) + 1));
        dp8_set(0x10, (u8)(dp8(0x10) + 1));
    }
    tdc();
    ldx(dp_x(0x00));
    lda8(bus8(tab + 1 + g_cpu.X));
    dc_set_mx(0, g_cpu.x_flag);
    ldx(abs_x(0xB285));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, abs16(field + g_cpu.X)));
    abs16_set(field + g_cpu.X, g_cpu.A);
    dc_cmp16(g_cpu.A, cap);
    if (g_cpu._flag_C) { lda16(cap); abs16_set(field + g_cpu.X, cap); }
    if (reset) { tdc(); tax(); stx_dp(0x10); }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(C1F871, RTS)
{
    tdc(); tax(); stx_dp(0x10);
    level_bonus(0xCC258A, 0x0005, 0x03E7, 1);
    level_bonus(0xCC25C2, 0x0009, 0x0063, 0);
}
/* C1:774E  A2B0/A2B2 = position of the target (party: object + polar offset
 * by A34A, radius 18; enemy slot: A240/A250 + offset by A355). */
DC_ENTRY(C1774E, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        tax();
        lda8(abs8(0xA014 + g_cpu.X));
        tax(); stx_dp(0x80);
        lda8(0x18); dp8_set(0xAE, 0x18);
        lda8(abs8(0xA34A + g_cpu.X));
        dc_C101F9_glue();
        dp8_set(0x82, (u8)g_cpu.A);
        ldx(dp_x(0x80));
        lda8(0x18); dp8_set(0xAE, 0x18);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA34A + g_cpu.X), 0x40));
        dc_C101F9_glue();
        dp8_set(0x83, (u8)g_cpu.A);
        ldx(dp_x(0x80));
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), dp8(0x82))); abs8_set(0xA2B2, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D0C + g_cpu.X), dp8(0x83))); abs8_set(0xA2B0, (u8)g_cpu.A);
        return;
    }
    g_cpu._flag_C = 1;
    lda8(dc_sbc8((u8)g_cpu.A, 0x08));
    tax(); stx_dp(0x80);
    lda8(0x18); dp8_set(0xAE, 0x18);
    lda8(abs8(0xA355 + g_cpu.X));
    dc_C101F9_glue();
    dp8_set(0x82, (u8)g_cpu.A); dp8_set(0x83, 0);
    ldx(dp_x(0x80));
    lda8(0x18); dp8_set(0xAE, 0x18);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0xA355 + g_cpu.X), 0x40));
    dc_C101F9_glue();
    dp8_set(0x84, (u8)g_cpu.A); dp8_set(0x85, 0);
    lda8(dp8(0x80));
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0xA250 + g_cpu.X), dp16(0x82))); abs16_set(0xA2B2, g_cpu.A);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0xA240 + g_cpu.X), dp16(0x84))); abs16_set(0xA2B0, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:B4E9  Writes the battle item list 1580 (5-byte entries: item, .., count
 * at +3) back into the inventory 2400/2500 after the menus' bit-6 clean-up. */
DC_ENTRY(C1B4E9, RTS)
{
    for (int k = 0; k < 3; k++) { lda8((u8)k); dp8_set(0x14, (u8)k); dc_C1B6D1_glue(); }
    tdc(); tax(); stx_dp(0x00); stx_dp(0x02);
    for (;;) {
        ldx(dp_x(0x02));
        lda8(abs8(0x1580 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            ldx(0);
            int found = 0;
            do {
                dc_cmp8((u8)g_cpu.A, abs8(0x2400 + g_cpu.X));
                if (g_cpu._flag_Z) { found = 1; break; }
                inx();
                cpx(0x0100);
            } while (!g_cpu._flag_C);
            if (found) {
                stx_dp(0x00);
                ldx(dp_x(0x02));
                lda8(abs8(0x1583 + g_cpu.X));
                if (g_cpu._flag_Z) { ldx(dp_x(0x00)); abs8_set(0x2400 + g_cpu.X, (u8)g_cpu.A); }
                ldx(dp_x(0x00));
                abs8_set(0x2500 + g_cpu.X, (u8)g_cpu.A);
            } else {
                tdc(); tax();
                for (;;) {
                    lda8(abs8(0x2400 + g_cpu.X));
                    dc_cmp8((u8)g_cpu.A, 0x00);
                    if (g_cpu._flag_Z) break;
                    inx();
                    cpx(0x0100);
                    if (g_cpu._flag_Z) goto done;
                }
                stx_dp(0x04);
                ldx(dp_x(0x02)); lda8(abs8(0x1580 + g_cpu.X));
                ldx(dp_x(0x04)); abs8_set(0x2400 + g_cpu.X, (u8)g_cpu.A);
                ldx(dp_x(0x02)); lda8(abs8(0x1583 + g_cpu.X));
                ldx(dp_x(0x04)); abs8_set(0x2500 + g_cpu.X, (u8)g_cpu.A);
            }
        }
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0x02), 0x0005));
        dp16_set(0x02, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        ldx(dp_x(0x02));
        cpx(0x0500);
        if (g_cpu._flag_Z) break;
    }
done:
    dp8_set(0x03, 0);
    dp8_set(0x05, 0);
}
/* C1:F2DB  B2D3..B2D5 = bit masks (80 >> slot) of the 3 AEFF entries, sorted;
 * B2D6..B2D9 = the pair and triple unions. */
static void sort2(u16 a, u16 b)
{
    lda8(abs8(a));
    dc_cmp8((u8)g_cpu.A, abs8(b));
    if (!g_cpu._flag_C) return;
    lda8(abs8(a)); tax();
    lda8(abs8(b)); abs8_set(a, (u8)g_cpu.A);
    lda8((u8)g_cpu.X); abs8_set(b, (u8)g_cpu.A);
}
DC_ENTRY(C1F2DB, RTS)
{
    tdc();
    for (int k = 0; k < 3; k++) abs8_set(0xB2D3 + k, (u8)g_cpu.A);
    for (int k = 0; k < 3; k++) {
        lda8(0x80); dp8_set(0x0A, 0x80);
        lda8(abs8(0xAEFF + k));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) continue;
        for (;;) {
            dc_cmp8((u8)g_cpu.A, 0x00);
            if (g_cpu._flag_Z) break;
            u8 v = dp8(0x0A); g_cpu._flag_C = v & 1; dp8_set(0x0A, v >> 1);
            lda8((u8)(g_cpu.A - 1));
        }
        lda8(dp8(0x0A));
        abs8_set(0xB2D3 + k, (u8)g_cpu.A);
    }
    sort2(0xB2D3, 0xB2D4);
    sort2(0xB2D3, 0xB2D5);
    sort2(0xB2D4, 0xB2D5);
    lda8(abs8(0xB2D3) | abs8(0xB2D4)); abs8_set(0xB2D6, (u8)g_cpu.A);
    lda8(abs8(0xB2D3) | abs8(0xB2D5)); abs8_set(0xB2D7, (u8)g_cpu.A);
    lda8(abs8(0xB2D4) | abs8(0xB2D5)); abs8_set(0xB2D8, (u8)g_cpu.A);
    lda8(abs8(0xB2D3) | abs8(0xB2D4) | abs8(0xB2D5)); abs8_set(0xB2D9, (u8)g_cpu.A);
}
/* C1:06F0  Draws the ATB gauge of party slot $80: 4 cells at CC:FA35 + 1A,
 * filled in eighths (tiles 67..6F) by 99DD / 9F22. */
DC_ENTRY(C106F0, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x80));
    g_cpu._flag_C = g_cpu.A >> 15;
    lda16((u16)(g_cpu.A << 1));
    tax();
    g_cpu._flag_C = 0;
    lda16(dc_adc16(bus16(0xCCFA35 + g_cpu.X), 0x001A));
    tay();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0x80));
    lda8(abs8(0x99DD + g_cpu.X)); dp8_set(0xAD, (u8)g_cpu.A); dp8_set(0xAE, 0);
    lda8(abs8(0x9F22 + g_cpu.X)); dp8_set(0xB3, (u8)g_cpu.A); dp8_set(0xB4, 0);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0xAD));
    asl_a(8);
    dp16_set(0xB1, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    dc_C100D7_glue();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0xB5));
    lsr_a(3);
    dp16_set(0xB5, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(0x20, dp8(0xB5)));
    dp8_set(0x82, (u8)g_cpu.A);
    lsr_a(3); dp8_set(0x83, (u8)g_cpu.A);
    asl_a(3); dp8_set(0x86, (u8)g_cpu.A);
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(dp8(0x82), dp8(0x86)));
    dp8_set(0x82, (u8)g_cpu.A);
    lda8(0x67);
    for (int k = 0; k < 4; k++) abs8_set(0x0CC0 + k * 2 + g_cpu.Y, 0x67);
    ldx(dp_x(0x80));
    lda8(abs8(0x99DD + g_cpu.X));
    lda8(g_cpu._flag_Z ? 0x29 : 0x2D);
    for (int k = 0; k < 4; k++) abs8_set(0x0CC1 + k * 2 + g_cpu.Y, (u8)g_cpu.A);
    for (;;) {
        lda8(dp8(0x83));
        if (g_cpu._flag_Z) break;
        lda8(0x6F); abs8_set(0x0CC0 + g_cpu.Y, 0x6F);
        iny(); iny();
        u8 n = (u8)(dp8(0x83) - 1); dp8_set(0x83, n); dc_set_flags_nz8(n);
    }
    g_cpu._flag_C = 0;
    lda8(dp8(0x82));
    if (g_cpu._flag_Z) return;
    lda8(dc_adc8((u8)g_cpu.A, 0x67));
    abs8_set(0x0CC0 + g_cpu.Y, (u8)g_cpu.A);
}
/* C1:5D1D  Copies a palette (6 or 14 bytes from D1:4100 / D1:4400) into the
 * party/enemy palette buffer 2302 + slot*8, from the CD:xxxx entry of the operand. */
DC_ENTRY(C15D1D, RTS)
{
    lda8(script_byte());
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = g_cpu.A >> 15;
    lda16((u16)(g_cpu.A << 1));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, abs16(0x9882)));
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(bus8(0xCD0001 + g_cpu.X));
    dp8_set(0x82, (u8)g_cpu.A);
    lda8((u8)g_cpu.A & 0x07);
    lda8(asl8(asl8(asl8((u8)g_cpu.A))));
    tay();
    lda8(dp8(0x82));
    int big = g_cpu._flag_N;
    lda8(bus8(0xCD0000 + g_cpu.X));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = g_cpu.A >> 15;
    lda16((u16)(g_cpu.A << 1));
    dp16_set(0x80, g_cpu.A);
    g_cpu._flag_C = g_cpu.A >> 15;
    lda16((u16)(g_cpu.A << 1));
    if (big) {
        dp16_set(0x82, g_cpu.A);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, dp16(0x80)));
        dp16_set(0x84, g_cpu.A);
        lda16(dp16(0x82));
        g_cpu._flag_C = g_cpu.A >> 15;
        lda16((u16)(g_cpu.A << 1));
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, dp16(0x84)));
    } else {
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, dp16(0x80)));
    }
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    u8 n = big ? 0x0E : 0x06;
    u32 src = big ? 0xD14400 : 0xD14100;
    lda8(n); dp8_set(0x82, n);
    do {
        lda8(bus8(src + g_cpu.X));
        abs8_set(0x2302 + g_cpu.Y, (u8)g_cpu.A);
        inx(); iny();
        u8 c = (u8)(dp8(0x82) - 1); dp8_set(0x82, c); dc_set_flags_nz8(c);
    } while (!g_cpu._flag_Z);
    inc_abs(0xA00F);
    lda8(0x02);
    script_advance();
}
/* C1:C96A  Builds the tech menu entries (7 bytes at FD:A84F[$06]) of party
 * slot $06 for the techs it has learned (bits of 2837). */
DC_ENTRY(C1C96A, RTS)
{
    tdc(); tax();
    stx_dp(0x00); stx_dp(0x08); stx_dp(0x0E);
    lda8(dp8(0x06));
    tax();
    lda8(abs8(0xAEFF + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    dp8_set(0x00, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xFDA89D + g_cpu.X)); dp8_set(0x08, (u8)g_cpu.A);
    lda8(bus8(0xFDA89E + g_cpu.X)); dp8_set(0x0E, (u8)g_cpu.A);
    lda8(dp8(0x08));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = g_cpu.A >> 15;
    lda16((u16)(g_cpu.A << 1));
    dp16_set(0x02, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x06));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = g_cpu.A >> 15;
    lda16((u16)(g_cpu.A << 1));
    tax();
    lda16(bus16(0xFDA84F + g_cpu.X));
    dp16_set(0x04, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0x00));
    lda8(abs8(0x2837 + g_cpu.X));
    dp8_set(0x0C, (u8)g_cpu.A);
    do {
        lda8(dp8(0x0C) & 0x80);
        if (!g_cpu._flag_Z) {
            lda8(dp8(0x08));
            dc_C1CB93_glue();
            tdc();
            ldx(dp_x(0x02));
            lda8(bus8(0xCC1ACC + g_cpu.X) & 0x3F);
            ldx(dp_x(0x04)); abs8_set(0x0002 + g_cpu.X, (u8)g_cpu.A);
            ldx(dp_x(0x02));
            lda8(bus8(0xCC1ACB + g_cpu.X));
            ldx(dp_x(0x04)); abs8_set(0x0001 + g_cpu.X, (u8)g_cpu.A);
            ldx(dp_x(0x04));
            lda8(dp8(0x08)); abs8_set(0x0000 + g_cpu.X, (u8)g_cpu.A);
            ldx(dp_x(0x04));
            lda8(0xFF);
            abs8_set(0x0003 + g_cpu.X, 0xFF); abs8_set(0x0005 + g_cpu.X, 0xFF); abs8_set(0x0006 + g_cpu.X, 0xFF);
            tdc();
            lda8(dp8(0x08));
            dc_C1CB48_glue();
            tay();
            tdc();
            lda8(abs8(0xB2EA));
            dc_C1CBC3_glue();
            lda8(0xFF);
            ldx(dp_x(0x04));
            abs8_set(0x0004 + g_cpu.X, 0xFF);
        }
        lda8(asl8(dp8(0x0C)));
        dp8_set(0x0C, (u8)g_cpu.A);
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0x04), 0x0007));
        dp16_set(0x04, g_cpu.A);
        dp16_set(0x02, (u16)(dp16(0x02) + 2));
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x08, (u8)(dp8(0x08) + 1));
        lda8(dp8(0x08));
        dc_cmp8((u8)g_cpu.A, dp8(0x0E));
    } while (!g_cpu._flag_C);
}
/* C1:1BAA  Removes the actor $A1 from the turn queues (95D6.. when waiting,
 * or the ready list A6D9 / current 95D5). */
DC_ENTRY(C11BAA, RTS)
{
    tdc(); tax();
    lda8(dp8(0xA1));
    dp8_set(0x80, (u8)g_cpu.A);
    tax();
    lda8(abs8(0xA6D9 + g_cpu.X));
    if (g_cpu._flag_N) {
        tdc(); tax();
        lda8(dp8(0x80));
        for (;;) {
            dc_cmp8((u8)g_cpu.A, abs8(0x95D6 + g_cpu.X));
            if (g_cpu._flag_Z) break;
            inx();
            cpx(0x0003);
            if (g_cpu._flag_Z) return;
        }
        do {
            lda8(abs8(0x95D7 + g_cpu.X));
            abs8_set(0x95D6 + g_cpu.X, (u8)g_cpu.A);
            inx();
            cpx(0x0003);
        } while (!g_cpu._flag_C);
        dec_abs(0x95DA);
        return;
    }
    dec_abs(0xA6DE);
    dc_cmp8((u8)g_cpu.A, abs8(0x95D5));
    if (!g_cpu._flag_Z) {
        lda8(dp8(0xA1)); tax();
        lda8(0xFF); abs8_set(0xA6D9 + g_cpu.X, 0xFF);
        lda8(0xFE); abs8_set(0xA6DF, 0xFE);
    } else {
        lda8(abs8(0xA86B));
        if (!g_cpu._flag_Z) {
            lda8((u8)(g_cpu.A - 1));
            abs8_set(0x95DB, (u8)g_cpu.A);
            abs8_set(0xA86B, 0);
            abs8_set(0xA09A, 0);
        }
        static const u16 z[] = { 0x9609, 0x960E, 0x9614, 0x95DB, 0x99E0 };
        for (int k = 0; k < 5; k++) abs8_set(z[k], 0);
        lda8(0xFE); abs8_set(0xA6DF, 0xFE);
        lda8(dp8(0xA1)); tax();
        lda8(0xFF); abs8_set(0xA6D9 + g_cpu.X, 0xFF);
        tdc(); tax();
        for (;;) {
            lda8(abs8(0xA6D9 + g_cpu.X));
            abs8_set(0xA6DD, (u8)g_cpu.A);
            abs8_set(0x95D5, (u8)g_cpu.A);
            if (!g_cpu._flag_N) break;
            inx();
            cpx(0x0003);
            if (g_cpu._flag_Z) {
                lda8(0xFF);
                abs8_set(0xA6DD, 0xFF);
                abs8_set(0x95D5, 0xFF);
                break;
            }
        }
    }
    dc_C11C3A_glue();
    abs8_set(0xA862, 0);
}
/* C1:0785  Draws the 7x6 tile window of party slot A (normal / grayed CC:FA41 / FA6B). */
DC_ENTRY(C10785, RTS)
{
    dp8_set(0x80, (u8)g_cpu.A);
    times12_at(0x82);
    tax();
    stx_dp(0x82);
    u32 src = 0xCCFA41;
    lda8(abs8(0x95F1));
    if (g_cpu._flag_Z) goto draw;
    lda8(dp8(0x80)); tax();
    lda8(abs8(0x9F25 + g_cpu.X) | abs8(0x9F28 + g_cpu.X));
    if (g_cpu._flag_Z) goto draw;
    src = 0xCCFA6B;
    lda8(abs8(0xA117));
    if (g_cpu._flag_Z) goto draw;
    lda8(abs8(0xA6DE));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_Z) goto draw;
    lda8(dp8(0x80));
    dc_cmp8((u8)g_cpu.A, abs8(0xA115));
    if (g_cpu._flag_Z) { src = 0xCCFA41; goto draw; }
    ldx(abs_x(0xA115));
    lda8(abs8(0xA6D9 + g_cpu.X));
    if (!g_cpu._flag_N) src = 0xCCFA41;
draw:
    tdc(); tax();
    lda8(0x06); dp8_set(0x80, 0x06);
    do {
        lda8(0x07); dp8_set(0x81, 0x07);
        ldy(dp_x(0x82));
        do {
            lda8(bus8(src + g_cpu.X)); abs8_set(0x0CC0 + g_cpu.Y, (u8)g_cpu.A);
            lda8(0x29); abs8_set(0x0CC1 + g_cpu.Y, 0x29);
            iny(); iny(); inx();
            u8 n = (u8)(dp8(0x81) - 1); dp8_set(0x81, n); dc_set_flags_nz8(n);
        } while (!g_cpu._flag_Z);
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0x82), 0x0040));
        dp16_set(0x82, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        u8 n = (u8)(dp8(0x80) - 1); dp8_set(0x80, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
}

/* ---- batch 9 --------------------------------------------------------------- */
/* C1:29B2 / 29FB / 2A43 / 2A98  Scan slots $84..end-1 (party 0..2 or enemies
 * 3..10) for the nearest / farthest live target (distance $AF from 2B37);
 * 9873 = the pick, $86 = its distance. skip_self skips 986F. */
static void scan_targets(u8 end, int farthest, int skip_self)
{
    for (;;) {
        lda8(dp8(0x84));
        if (skip_self) dc_cmp8((u8)g_cpu.A, dp8(0x8A));
        if (!skip_self || !g_cpu._flag_Z) {
            abs8_set(0x9870, (u8)g_cpu.A);
            tax();
            lda8(abs8(0x96F5 + g_cpu.X));
            if (g_cpu._flag_Z) goto next;
            lda8(abs8(0x9FF7 + g_cpu.X));
            if (g_cpu._flag_N) goto next;
            lda8((u8)g_cpu.X);
            dc_set_a8(asl8((u8)g_cpu.A));
            tax();
            dc_set_mx(0, g_cpu.x_flag);
            lda16(bus16(0xCCF8ED + g_cpu.X));
            tax();
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            lda8(abs8(0x5E4A + g_cpu.X));
            if (g_cpu._flag_N) goto next;
            dc_C12B37_glue();
            dc_set_mx(0, g_cpu.x_flag);
            if (farthest) dc_cmp16(dp16(0xAF), dp16(0x86));
            else dc_cmp16(dp16(0x86), dp16(0xAF));
            lda16(farthest ? dp16(0xAF) : dp16(0x86));
            if (g_cpu._flag_C) {
                lda16(dp16(0xAF)); dp16_set(0x86, g_cpu.A);
                lda16(dp16(0x84)); abs16_set(0x9873, g_cpu.A);
            }
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
        }
    next:
        dp8_set(0x84, (u8)(dp8(0x84) + 1));
        lda8(dp8(0x84));
        dc_cmp8((u8)g_cpu.A, end);
        if (g_cpu._flag_Z) return;
    }
}
DC_ENTRY(C129B2, RTS) { tdc(); tax(); stx_dp(0x84); dex(); stx_dp(0x86); scan_targets(0x03, 0, 0); }
DC_ENTRY(C129FB, RTS) { tdc(); tax(); stx_dp(0x84); stx_dp(0x86); scan_targets(0x03, 1, 0); }
DC_ENTRY(C12A98, RTS) { tdc(); tax(); stx_dp(0x86); ldx(0x0003); stx_dp(0x84); scan_targets(0x0B, 1, 0); }
DC_ENTRY(C12A43, RTS)
{
    ldx(0xFFFF); stx_dp(0x86);
    ldx(0x0003); stx_dp(0x84);
    lda8(abs8(0x986F)); dp8_set(0x8A, (u8)g_cpu.A);
    scan_targets(0x0B, 0, 1);
}
/* C1:0222  $DB = angle (0..FF) of the vector D5/D6 -> D3/D4, from the
 * C0:F300 arctangent table of the |dx|/8, |dy|/8 octant. */
DC_ENTRY(C10222, RTS)
{
    for (int k = 0; k < 2; k++) {
        g_cpu._flag_C = 1;
        lda8(dc_sbc8(dp8(0xD3 + k), dp8(0xD5 + k)));
        dp8_set(0xD7 + k * 2, (u8)g_cpu.A);
        lda8(dc_sbc8(0x00, 0x00));
        dp8_set(0xD8 + k * 2, (u8)g_cpu.A);
    }
    for (int k = 0; k < 2; k++) {
        lda8(dp8(0xD7 + k * 2) ^ dp8(0xD8 + k * 2));
        g_cpu._flag_C = 1;
        lda8(dc_sbc8((u8)g_cpu.A, dp8(0xD8 + k * 2)));
        dp8_set(0xDE + k * 2, (u8)g_cpu.A);
        dp8_set(0xDF + k * 2, 0);
    }
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0xDE)); lsr16(3); dp16_set(0xDC, g_cpu.A);
    lda16(dp16(0xE0) & 0xFFF8);
    asl_a(2);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0xDC)));
    dp16_set(0xE2, g_cpu.A);
    asl_a(1);
    tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0xE2));
    lda8(bus8(0xC0F300 + g_cpu.X));
    dp8_set(0xDB, (u8)g_cpu.A);
    lda8(dp8(0xD8));
    if (!g_cpu._flag_N) {
        lda8(dp8(0xDA));
        if (!g_cpu._flag_N) { g_cpu._flag_C = 0; lda8(dc_adc8(0x80, dp8(0xDB))); }
        else { g_cpu._flag_C = 1; lda8(dc_sbc8(0x80, dp8(0xDB))); }
        dp8_set(0xDB, (u8)g_cpu.A);
        return;
    }
    lda8(dp8(0xDA));
    if (!g_cpu._flag_N) {
        tdc();
        g_cpu._flag_C = 1;
        lda8(dc_sbc8((u8)g_cpu.A, dp8(0xDB)));
        dp8_set(0xDB, (u8)g_cpu.A);
    }
    lda8(dp8(0xDB));
}
/* C1:E006  Starts the stat-change timers of slot B1FD for effect type $16
 * by the bits of $18 (AF27.. = duration from B0D4.., AFB6.. = 1). */
static void timer_on(u16 src, u16 dst, u16 flag)
{
    lda8(abs8(src + g_cpu.X)); abs8_set(dst + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); abs8_set(flag + g_cpu.X, 0x01);
}
DC_ENTRY(C1E006, RTS)
{
    lda8(abs8(0xB1FD));
    tax();
    dc_set_mx(0, g_cpu.x_flag);
    xba16();
    lsr16(1);
    tay();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x16));
    u8 t = (u8)g_cpu.A;
    dc_cmp8(t, 0x01);
    u8 b = 0;
    if (t == 0x01) {
        lda8(dp8(0x18)); b = (u8)g_cpu.A;
        g_cpu._flag_Z = !(b & 0xF0);
        if (!(b & 0xF0)) return;
        if (b & 0x80) { g_cpu._flag_Z = 0; timer_on(0xB0D4, 0xAF27, 0xAFB6); return; }
        if (b & 0x40) {
            lda8(abs8(0xB0DF + g_cpu.X));
            g_cpu._flag_C = 0;
            lda8(dc_adc8((u8)g_cpu.A, abs8(0x5E64 + g_cpu.Y)));
            abs8_set(0xAF32 + g_cpu.X, (u8)g_cpu.A);
            lda8(0x01); abs8_set(0xAFC1 + g_cpu.X, 0x01);
            return;
        }
        if (b & 0x20) { timer_on(0xB0EA, 0xAF3D, 0xAFCC); return; }
        g_cpu._flag_Z = !(b & 0x10);
        if (b & 0x10) timer_on(0xB12C, 0xAF7F, 0xB00E);
        return;
    }
    dc_cmp8(t, 0x02);
    if (t == 0x02) { lda8(dp8(0x18)); g_cpu._flag_Z = !(g_cpu.A & 0xC0); return; }
    dc_cmp8(t, 0x03);
    if (t == 0x03) {
        lda8(dp8(0x18)); b = (u8)g_cpu.A;
        g_cpu._flag_Z = !(b & 0xC0);
        if (!(b & 0xC0)) return;
        if (b & 0x80) { timer_on(0xB0F5, 0xAF48, 0xAFD7); return; }
        timer_on(0xB100, 0xAF53, 0xAFE2);
        return;
    }
    dc_cmp8(t, 0x04);
    if (t != 0x04) return;
    lda8(dp8(0x18)); b = (u8)g_cpu.A;
    g_cpu._flag_Z = !(b & 0x60);
    if (!(b & 0x60)) return;
    if (b & 0x40) { timer_on(0xB10B, 0xAF5E, 0xAFED); return; }
    timer_on(0xB137, 0xAF8A, 0xB019);
}
/* C1:6AF0  Script command: walk forward (party slot / all party / enemy A2B6)
 * for operand steps along the facing angle until the map (7BFD) or another
 * box blocks; advances 0 while walking, 2 when done. */
static void sty_dp(u8 o) { if (g_cpu.x_flag) dp8_set(o, (u8)g_cpu.Y); else dp16_set(o, g_cpu.Y); }
static void phy(void) { if (g_cpu.x_flag) dc_push8((u8)g_cpu.Y); else dc_push16(g_cpu.Y); }
static void ply(void) { ldy(g_cpu.x_flag ? dc_pull8() : dc_pull16()); }
static void walk_init_party(void)
{
    lda8(abs8(0xA5BD)); abs8_set(0xA639 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA5AA + g_cpu.Y)); abs8_set(0xA34A + g_cpu.Y, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set(0x96DB + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0xA35D + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0xA370 + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA2EB + g_cpu.Y, 0x01);
    lda8(0x02); abs8_set(0xA86D + g_cpu.Y, 0x02);
    tdc();
    abs8_set(0xA311 + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA19D + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA337 + g_cpu.Y, 0x01);
}
static void walk_init_enemy(void)
{
    lda8(abs8(0xA5BD)); abs8_set(0xA644 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA5B5 + g_cpu.Y)); abs8_set(0xA355 + g_cpu.Y, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set(0xA230 + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA2F6 + g_cpu.Y, 0x01);
    lda8(0x02); abs8_set(0xA878 + g_cpu.Y, 0x02);
    tdc();
    abs8_set(0xA31C + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA1E0 + g_cpu.Y, (u8)g_cpu.A);
    lda8((u8)g_cpu.Y);
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    lda8(abs8(0xA240 + g_cpu.X)); abs8_set(0xA368 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA250 + g_cpu.X)); abs8_set(0xA37B + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA342 + g_cpu.Y, 0x01);
}
/* One step of party slot Y; 1 = keep walking. */
static int walk_step_party(void)
{
    lda8(abs8(0xA19D + g_cpu.Y));
    if (!g_cpu._flag_Z) return 0;
    lda8(0x10); dp8_set(0xAE, 0x10);
    lda8(abs8(0xA34A + g_cpu.Y));
    dc_C101F9_glue();
    dp8_set(0x82, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0xA34A + g_cpu.Y), 0x40));
    dc_C101F9_glue();
    dp8_set(0x83, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D23 + g_cpu.Y), dp8(0x82)));
    lda8((u8)g_cpu.A & 0xF0); dp8_set(0x82, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0C + g_cpu.Y), dp8(0x83)));
    dc_C1011A_glue();
    lda8((u8)g_cpu.A | dp8(0x82));
    tax();
    lda8(abs8(0x7BFD + g_cpu.X) & 0xC0);
    if (!g_cpu._flag_Z) return 0;
    sty_dp(0x80);
    phy();
    ldx(g_cpu.Y);
    dc_C17C2B_glue();
    dp8_set(0x80, (u8)g_cpu.A);
    ply();
    lda8(dp8(0x80));
    if (g_cpu._flag_N) return 0;
    ldx(g_cpu.Y);
    dec_abs(0xA639 + g_cpu.X);
    return !g_cpu._flag_Z;
}
/* C1:6B00  Walk command for party slot A (0-2) or, with A = 4, the whole
 * party list A018: starts each walker, steps it; advances 0 while walking, 2 once done. */
static void walk_cmd_party(void)
{
    dc_cmp8((u8)g_cpu.A, 0x04);
    if (!g_cpu._flag_Z) {
        tax();
        lda8(abs8(0xA014 + g_cpu.X));
        tay();
        lda8(abs8(0xA337 + g_cpu.Y));
        if (g_cpu._flag_Z) walk_init_party();
        if (walk_step_party()) { lda8(0x00); script_advance(); return; }
        tdc(); abs8_set(0xA337 + g_cpu.Y, (u8)g_cpu.A);
        lda8(0x02); script_advance();
        return;
    }
    tdc(); tax(); stx_dp(0x80);
    for (;;) {
        ldx(dp_x(0x80));
        lda8(abs8(0xA018 + g_cpu.X));
        if (g_cpu._flag_N) break;
        tay();
        lda8(abs8(0xA337 + g_cpu.Y));
        if (!g_cpu._flag_Z) break;
        walk_init_party();
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
    }
    dp8_set(0x84, 0);
    for (;;) {
        ldx(dp_x(0x80));
        lda8(abs8(0xA018 + g_cpu.X));
        if (g_cpu._flag_N) break;
        tay();
        if (walk_step_party()) break;
        tdc(); abs8_set(0xA337 + g_cpu.Y, (u8)g_cpu.A);
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
    }
    lda8(dp8(0x84));
    lda8(g_cpu._flag_Z ? 0x02 : 0x00);
    script_advance();
    return;
}
DC_ENTRY(C16B00, RTS) { walk_cmd_party(); }
DC_ENTRY(C16AF0, RTS)
{
    lda8(script_byte());
    abs8_set(0xA5BD, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) { walk_cmd_party(); return; }
    ldy(abs_x(0xA2B6));
    lda8(abs8(0xA342 + g_cpu.Y));
    if (g_cpu._flag_Z) walk_init_enemy();
    ldx(abs_x(0xA2B6));
    lda8(abs8(0xA1E0 + g_cpu.X));
    if (g_cpu._flag_Z) {
        lda8(0x10); dp8_set(0xAE, 0x10);
        lda8(abs8(0xA355 + g_cpu.X));
        dc_C101F9_glue();
        dp8_set(0x82, (u8)g_cpu.A);
        ldx(abs_x(0xA2B6));
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA355 + g_cpu.X), 0x40));
        dc_C101F9_glue();
        dp8_set(0x83, (u8)g_cpu.A);
        lda8((u8)g_cpu.Y);
        dc_set_a8(asl8((u8)g_cpu.A));
        tay();
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA250 + g_cpu.Y), dp8(0x82)));
        lda8((u8)g_cpu.A & 0xF0); dp8_set(0x82, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA240 + g_cpu.Y), dp8(0x83)));
        dc_C1011A_glue();
        lda8((u8)g_cpu.A | dp8(0x82));
        tax();
        lda8(abs8(0x7BFD + g_cpu.X) & 0xC0);
        if (g_cpu._flag_Z) {
            ldx(abs_x(0xA2B6));
            dec_abs(0xA644 + g_cpu.X);
            if (!g_cpu._flag_Z) { lda8(0x00); script_advance(); return; }
        }
    }
    ldx(abs_x(0xA2B6));
    abs8_set(0xA342 + g_cpu.X, 0);
    lda8(0x02); script_advance();
}
/* C1:081E  Redraws the three party windows (0929 at columns 0/C/E/18/1A by
 * which slots are present), the active one (08E8), then 0872. */
DC_ENTRY(C1081E, RTS)
{
    dp8_set(0x84, 0);
    lda8(abs8(0xA6D9));
    if (!g_cpu._flag_N) { ldx(0x0000); stx_dp(0x82); dc_C10929_glue(); }
    for (int k = 0; k < 2; k++) {
        lda8(abs8(0xA6DA + k));
        if (g_cpu._flag_N) continue;
        lda8(abs8(0xA6D9 + k));
        if (!g_cpu._flag_N) { lda8(0x01); dp8_set(0x84, 0x01); ldx(k ? 0x001A : 0x000E); }
        else ldx(k ? 0x0018 : 0x000C);
        stx_dp(0x82);
        dc_C10929_glue();
        if (k == 0) dp8_set(0x84, 0);
    }
    lda8(abs8(0xA6DD));
    tax();
    lda8(abs8(0xA6D9 + g_cpu.X));
    dc_C108E8_glue();
    inc_abs(0xA43F);
    dc_C10872_glue();
}
/* C1:C82D  Picks the enemy (or, on B18B >= 3, party) target slot B2AE for the
 * auto battle cursor; AF23 = 1 when there is none. */
static void pick_slot(u8 a0a)
{
    lda8((u8)g_cpu.X);
    abs8_set(0xB2AE, (u8)g_cpu.A);
    abs8_set(0xAD8E, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xAD8D, 0x01);
    dp8_set(0x0A, a0a);
}
DC_ENTRY(C1C82D, RTS)
{
    tdc();
    abs8_set(0xAF23, 0);
    lda8(abs8(0xB18B));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (!g_cpu._flag_C) {
        ldx(0x0003);
        for (;;) {
            lda8(abs8(0xAEFF + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (!g_cpu._flag_Z) break;
            inx();
            cpx(0x000B);
            if (g_cpu._flag_C) goto none;
        }
        tdc();
        lda8(abs8(0xB2AE));
        tax();
        lda8(abs8(0xAEFF + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) goto ok;
        for (;;) {
            lda8(abs8(0x2989));
            if (g_cpu.A & 0x20) break;
            tdc();
            ldx(0x0003);
            lda8(0x0B);
            dc_C1AF22_glue();
            tax();
            lda8(abs8(0xAEFF + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (!g_cpu._flag_Z) { pick_slot(0); goto ok; }
        }
        tdc();
        abs8_set(0xAD8D, (u8)g_cpu.A);
        ldx(0x0002);
        for (;;) {
            inx();
            cpx(0x000B);
            if (g_cpu._flag_Z) goto ok;
            lda8(abs8(0xAEFF + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (!g_cpu._flag_Z) { pick_slot(0); goto ok; }
        }
    }
    for (int k = 0; k < 3; k++) {
        lda8(abs8(0xAEFF + k));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) break;
        if (k == 2) goto none;
    }
    tdc();
    lda8(abs8(0xB2AE));
    tax();
    lda8(abs8(0xAEFF + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) goto ok;
    do {
        tdc(); tax();
        lda8(0x03);
        dc_C1AF22_glue();
        tax();
        lda8(abs8(0xAEFF + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
    } while (g_cpu._flag_Z);
    pick_slot(0x01);
ok:
    abs8_set(0xAF23, 0);
    return;
none:
    lda8(0x01); abs8_set(0xAF23, 0x01);
}

/* ---- batch 10 -------------------------------------------------------------- */
/* |a - b| of two bytes (SEC; SBC; BPL; EOR #FF; INC). */
static void abs_diff8(u8 a, u8 b)
{
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(a, b));
    if (g_cpu._flag_N) { lda8((u8)g_cpu.A ^ 0xFF); lda8((u8)(g_cpu.A + 1)); }
}
/* $AF = dx^2 + dy^2 (0089 squares $AD * $AE) between object $80/$81 and object Y. */
static void sq_dist(void)
{
    abs_diff8(dp8(0x80), abs8(0x1D0C + g_cpu.Y));
    dp8_set(0xAD, (u8)g_cpu.A); dp8_set(0xAE, (u8)g_cpu.A);
    dc_C10089_glue();
}
/* C1:2CF3  9873 = |d(9870, 986F)^2 - d(9870, 9871)^2| / 256. */
DC_ENTRY(C12CF3, RTS)
{
    lda8(abs8(0x9870)); tax();
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0x81, (u8)g_cpu.A);
    for (int k = 0; k < 2; k++) {
        lda8(abs8(k ? 0x9871 : 0x986F)); tay();
        sq_dist();
        ldx(dp_x(0xAF)); stx_dp(k ? 0x84 : 0x82);
        abs_diff8(dp8(0x81), abs8(0x1D23 + g_cpu.Y));
        dp8_set(0xAD, (u8)g_cpu.A); dp8_set(0xAE, (u8)g_cpu.A);
        dc_C10089_glue();
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0xAF), dp16(k ? 0x84 : 0x82)));
        if (!k) { dp16_set(0x82, g_cpu.A); tdc(); dc_set_mx(1, g_cpu.x_flag); }
    }
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(g_cpu.A, dp16(0x82)));
    if (g_cpu._flag_N) { lda16(g_cpu.A ^ 0xFFFF); lda16((u16)(g_cpu.A + 1)); }
    dc_C10116_glue();
    dp16_set(0x80, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x80));
    abs8_set(0x9873, (u8)g_cpu.A);
}
/* C1:65BA  A880/A882 = unit step (8.8) of the vector D3/D4 -> D5/D6 along
 * its major axis (0100) and minor axis (minor*256/major); $8C = y is major. */
DC_ENTRY(C165BA, RTS)
{
    tdc(); tax();
    stx_dp(0x82); stx_dp(0x84); stx_dp(0x8C);
    for (int k = 0; k < 2; k++) {
        g_cpu._flag_C = 1;
        lda8(dc_sbc8(dp8(0xD5 + k), dp8(0xD3 + k)));
        dp8_set(0xD7 + k * 2, (u8)g_cpu.A);
        lda8(dc_sbc8(0x00, 0x00));
        dp8_set(0xD8 + k * 2, (u8)g_cpu.A);
        if (!g_cpu._flag_C) dp8_set(0x82 + k, (u8)(dp8(0x82 + k) + 1));
    }
    for (int k = 0; k < 2; k++) {
        lda8(dp8(0xD7 + k * 2) ^ dp8(0xD8 + k * 2));
        g_cpu._flag_C = 1;
        lda8(dc_sbc8((u8)g_cpu.A, dp8(0xD8 + k * 2)));
        dp8_set(0xDE + k * 2, (u8)g_cpu.A);
        dp8_set(0xDF + k * 2, 0);
    }
    lda8(dp8(0xDE));
    dc_cmp8((u8)g_cpu.A, dp8(0xE0));
    int ymaj = !g_cpu._flag_C;
    if (ymaj) { dp8_set(0x8C, (u8)(dp8(0x8C) + 1)); lda8(dp8(0xE0)); }
    dp8_set(0xB3, (u8)g_cpu.A);
    abs8_set(0xAAFE, (u8)g_cpu.A);
    lsr_a(1);
    dp8_set(0x84, (u8)g_cpu.A);
    lda8(dp8(ymaj ? 0xDE : 0xE0));
    dc_set_mx(0, g_cpu.x_flag);
    dc_C1010D_glue();
    dp16_set(0xB1, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    dc_C100D7_glue();
    ldx(dp_x(0xB5)); stx_abs(ymaj ? 0xA880 : 0xA882);
    ldx(0x0100); stx_abs(ymaj ? 0xA882 : 0xA880);
    for (int k = 0; k < 2; k++) {
        lda8(dp8(0x82 + k));
        if (g_cpu._flag_Z) continue;
        dc_set_mx(0, g_cpu.x_flag);
        u16 a = k ? 0xA882 : 0xA880;
        lda16(abs16(a) ^ 0xFFFF);
        lda16((u16)(g_cpu.A + 1));
        abs16_set(a, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
}
/* C1:09B0  Draws battle item entry $80 (1580 list) at column $82: name
 * (CC:0B5E + 11*item), gray when unusable, and its count. */
DC_ENTRY(C109B0, RTS)
{
    ldx(dp_x(0x80));
    lda8(abs8(0x1583 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    lda8(abs8(0x1580 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    dc_set_mx(0, g_cpu.x_flag);
    dp16_set(0x84, g_cpu.A);
    asl_a(2);
    dp16_set(0x8E, g_cpu.A);
    asl_a(1);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x8E)));
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(g_cpu.A, dp16(0x84)));
    lda16(dc_adc16(g_cpu.A, 0x0B5E));
    tax();
    tdc();
    ldy(0x94A0);
    lda16(0x000A);
    dc_mvn(0x7E, 0xCC);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0x0000 + g_cpu.Y, (u8)g_cpu.A);
    dc_C101A9_glue();
    ldx(dp_x(0x80));
    lda8(abs8(0x1582 + g_cpu.X));
    u8 pal = 0x29;
    if (g_cpu._flag_N) {
        lda8(abs8(0x1584 + g_cpu.X));
        ldx(dp_x(0x96));
        lda8((u8)g_cpu.A & bus8(0xCCF9FB + g_cpu.X));
        if (g_cpu._flag_Z) pal = 0x2D;
    }
    lda8(pal);
    dp8_set(0x98, pal);
    tdc(); tay();
    ldx(dp_x(0x82));
    do {
        lda8(abs8(0x94A0 + g_cpu.Y)); abs8_set(0x0EC6 + g_cpu.X, (u8)g_cpu.A); inx();
        lda8(dp8(0x98)); abs8_set(0x0EC6 + g_cpu.X, (u8)g_cpu.A); inx();
        iny(); cpy(0x000A);
    } while (!g_cpu._flag_Z);
    lda8(0x5F); abs8_set(0x0EC8 + g_cpu.X, 0x5F);
    lda8(dp8(0x98)); abs8_set(0x0EC9 + g_cpu.X, (u8)g_cpu.A);
    tdc(); tay();
    ldx(dp_x(0x82));
    do {
        lda8(abs8(0x94B0 + g_cpu.Y)); abs8_set(0x0E86 + g_cpu.X, (u8)g_cpu.A); inx();
        lda8(dp8(0x98)); abs8_set(0x0E86 + g_cpu.X, (u8)g_cpu.A); inx();
        iny(); cpy(0x000B);
    } while (!g_cpu._flag_Z);
    ldx(dp_x(0x80));
    lda8(abs8(0x1583 + g_cpu.X));
    dc_set_mx(0, g_cpu.x_flag);
    abs16_set(0x9499, g_cpu.A);
    dc_C10174_glue();
    dc_C1104E_glue();
    ldx(dp_x(0x82));
    static const u16 at[] = { 0x0EDE, 0x0EE0, 0x0EEC, 0x0EEE, 0x0EF0, 0x0EF2 };
    for (int k = 0; k < 6; k++) {
        u8 v = k == 0 ? abs8(0x949E) : k == 1 ? abs8(0x949F) : (u8)(0xA8CDBEC6u >> (8 * (5 - k)));
        lda8(v); abs8_set(at[k] + g_cpu.X, v);
        lda8(dp8(0x98)); abs8_set(at[k] + 1 + g_cpu.X, (u8)g_cpu.A);
    }
}
/* C1:2F97  Picks the battle action ($A440 / A441[$94]) of party slot $94 from
 * the status table CC:F77F (status mask, action, fallback); 12 for the
 * 5E2F bit-0 state. */
DC_ENTRY(C12F97, RTS)
{
    ldx(dp_x(0x94));
    lda8(abs8(0xA09B + g_cpu.X));
    lda8(g_cpu._flag_Z ? 0x03 : 0x00);
    dp8_set(0x84, (u8)g_cpu.A);
    lda8(0xFF); abs8_set(0xA441 + g_cpu.X, 0xFF);
    lda8(dp8(0x94));
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xCCF8ED + g_cpu.X));
    tay();
    sty_dp(0xA2);
    tdc();
    dp16_set(0x80, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0x94));
    lda8(abs8(0xA119 + g_cpu.X));
    int set = !g_cpu._flag_Z;
    if (!set) { lda8(abs8(0x5E4A + g_cpu.Y)); set = g_cpu._flag_N; }
    if (set) {
        lda8(0x01); abs8_set(0xA0A8 + g_cpu.X, 0x01);
        lda8(0xFF); abs8_set(0x968C + g_cpu.X, 0xFF);
    }
    tdc(); tax();
    for (;;) {
        lda8(bus8(0xCCF77F + g_cpu.X));
        if (g_cpu._flag_N) break;
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, dp16(0xA2)));
        tay();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x5E4A + g_cpu.Y) & bus8(0xCCF780 + g_cpu.X));
        if (g_cpu._flag_Z) {
            lda8((u8)g_cpu.X);
            if (!g_cpu._flag_Z) goto next;
            ldx(dp_x(0x94));
            lda8(abs8(0xA119 + g_cpu.X));
            int hit = !g_cpu._flag_Z;
            tdc(); tax();
            if (!hit) goto next;
        }
        lda8(bus8(0xCCF782 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) {
            lda8(abs8(0xA0A7));
            if (!g_cpu._flag_Z) goto next;
            lda8(0x03);
        }
        dp8_set(0x84, (u8)g_cpu.A);
        ldy(dp_x(0x94));
        lda8(bus8(0xCCF781 + g_cpu.X));
        if (!g_cpu._flag_N) {
            abs8_set(0xA441 + g_cpu.Y, (u8)g_cpu.A);
            dc_cmp8((u8)g_cpu.A, abs8(0xA44C + g_cpu.Y));
            if (!g_cpu._flag_Z) { lda8(0xFF); abs8_set(0x968C + g_cpu.Y, 0xFF); }
        }
        goto out;
    next:
        inx(); inx(); inx(); inx();
    }
    ldy(dp_x(0xA2));
    lda8(abs8(0x5E2F + g_cpu.Y) & 0x01);
    if (g_cpu._flag_Z) goto out;
    ldy(dp_x(0x94));
    lda8(abs8(0xA09B + g_cpu.Y));
    if (!g_cpu._flag_Z) goto out;
    lda8(0x80); abs8_set(0xA441 + g_cpu.Y, 0x80);
    lda8(0x12);
    dc_cmp8(0x12, abs8(0xA44C + g_cpu.Y));
    if (!g_cpu._flag_Z) {
        cpy(0x0003);
        if (!g_cpu._flag_C) { lda8(0xFF); abs8_set(0x968C + g_cpu.Y, 0xFF); }
    }
    lda8(0x12);
    abs8_set(0xA440, (u8)g_cpu.A);
    return;
out:
    lda8(dp8(0x84));
    abs8_set(0xA440, (u8)g_cpu.A);
}
/* C1:F533  Level-up tech learning: for each of the B2E0 techs (bit B2DF of
 * [$00]) whose requirements at [$04] are met by the 2830 levels (B2DE mask),
 * marks it learned and notes dual/triple techs (B305.. / B310). */
DC_ENTRY(C1F533, RTS)
{
    tdc(); tax(); stx_dp(0x10); stx_dp(0x12);
    lda8(abs8(0xB2DE)); dp8_set(0x0C, (u8)g_cpu.A);
    do {
        lda8(asl8(dp8(0x0C)));
        dp8_set(0x0C, (u8)g_cpu.A);
        if (g_cpu._flag_C) {
            ldx(dp_x(0x12));
            lda8(abs8(0x2830 + g_cpu.X));
            ldx(dp_x(0x10));
            abs8_set(0xB2E1 + g_cpu.X, (u8)g_cpu.A);
            dp8_set(0x10, (u8)(dp8(0x10) + 1));
        }
        dp8_set(0x12, (u8)(dp8(0x12) + 1));
        lda8(dp8(0x12));
        dc_cmp8((u8)g_cpu.A, 0x07);
    } while (!g_cpu._flag_C);
    ldx(dp_x(0x10));
    lda8(0xFF); abs8_set(0xB2E1 + g_cpu.X, 0xFF);
    lda8(0x80); abs8_set(0xB2DF, 0x80);
    do {
        ldx(dp_x(0x00));
        lda8(abs8(0x0000 + g_cpu.X) & abs8(0xB2DF));
        if (!g_cpu._flag_Z) goto next;
        tdc(); tax(); stx_dp(0x0E); stx_dp(0x10);
        lda8(0x00); abs8_set(0xB2E9, 0);
        for (;;) {
            ldx(dp_x(0x10));
            lda8(abs8(0xB2E1 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (g_cpu._flag_Z) break;
            dp8_set(0x0E, (u8)g_cpu.A);
            dc_set_mx(0, g_cpu.x_flag);
            lda16(dp16(0x04));
            g_cpu._flag_C = 0;
            lda16(dc_adc16(g_cpu.A, dp16(0x10)));
            tax();
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            lda8(dp8(0x0E));
            dc_cmp8((u8)g_cpu.A, bus8(0xCC0000 + g_cpu.X));
            if (!g_cpu._flag_C) { lda8(0xFF); abs8_set(0xB2E9, 0xFF); goto next; }
            dp8_set(0x10, (u8)(dp8(0x10) + 1));
        }
        tdc();
        lda8(abs8(0xB304));
        tax(); stx_dp(0x28);
        ldx(0x000B); stx_dp(0x2A);
        dc_C1C90B_glue();
        ldx(dp_x(0x2C));
        lda8(bus8(0xCC1BEB + g_cpu.X) & 0x80);
        if (!g_cpu._flag_Z) goto next;
        ldx(dp_x(0x00));
        lda8(abs8(0x0000 + g_cpu.X) | abs8(0xB2DF));
        abs8_set(0x0000 + g_cpu.X, (u8)g_cpu.A);
        lda8(dp8(0x10));
        dc_cmp8((u8)g_cpu.A, 0x02);
        if (g_cpu._flag_Z) {
            lda8(abs8(0xB30F)); tax();
            lda8(abs8(0xB304)); abs8_set(0xB305 + g_cpu.X, (u8)g_cpu.A);
            inc_abs(0xB30F);
            ldx(abs_x(0xB1F4));
            lda8(abs8(0xB2B3 + g_cpu.X) | 0x04); abs8_set(0xB2B3 + g_cpu.X, (u8)g_cpu.A);
            goto next;
        }
        lda8(dp8(0x10));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (g_cpu._flag_Z) {
            lda8(abs8(0xB304)); abs8_set(0xB310, (u8)g_cpu.A);
            ldx(abs_x(0xB1F4));
            lda8(abs8(0xB2B3 + g_cpu.X) | 0x02); abs8_set(0xB2B3 + g_cpu.X, (u8)g_cpu.A);
        }
    next:
        inc_abs(0xB304);
        dc_set_mx(0, g_cpu.x_flag);
        u16 v = (u16)(dp16(0x04) + 3); dp16_set(0x04, v); dc_set_flags_nz16(v);
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0xB2DF));
        lsr_a(1);
        abs8_set(0xB2DF, (u8)g_cpu.A);
        dec_abs(0xB2E0);
        lda8(abs8(0xB2E0));
        dc_cmp8((u8)g_cpu.A, 0x00);
    } while (!g_cpu._flag_Z);
}
/* C1:F39A  Applies the stat growth of each level gained (F45F finds them):
 * the CC:26FA increase is subtracted from the 5E5A exp counter; then adds
 * B2DB exp to 5E43 (24-bit, capped at 9,999,999). */
DC_ENTRY(C1F39A, RTS)
{
    ldx(abs_x(0xB2DB)); stx_dp(0x30);
    for (;;) {
        lda8(0xFF); abs8_set(0xB2DA, 0xFF);
        dc_C1F45F_glue();
        tdc();
        lda8(abs8(0xB2DA));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) break;
        lda8(abs8(0xB2D1));
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, 0x2830)); dp16_set(0x02, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0xB2D1));
        dc_set_a8(asl8((u8)g_cpu.A));
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, 0x276A)); dp16_set(0x04, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        ldx(dp_x(0x00));
        lda8(abs8(0x5E2F + g_cpu.X) & 0x10);
        if (!g_cpu._flag_Z) break;
        dc_C1F4DF_glue();
        tdc();
        lda8(abs8(0xB2D1));
        dc_set_mx(0, g_cpu.x_flag);
        asl_a(4);
        dp16_set(0x0A, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        ldx(dp_x(0x02));
        lda8(abs8(0x0000 + g_cpu.X));
        dc_set_a8(asl8((u8)g_cpu.A));
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, dp16(0x0A)));
        tax();
        lda16(bus16(0xCC26FA + g_cpu.X));
        ldx(dp_x(0x00));
        dc_cmp16(g_cpu.A, abs16(0x5E5A + g_cpu.X));
        if (!g_cpu._flag_C) {
            dp16_set(0x28, g_cpu.A);
            lda16(abs16(0x5E5A + g_cpu.X)); abs16_set(0xB2DB, g_cpu.A);
            lda16(dp16(0x28)); abs16_set(0x5E5A + g_cpu.X, g_cpu.A);
        } else {
            g_cpu._flag_C = 1;
            lda16(dc_sbc16(g_cpu.A, abs16(0x5E5A + g_cpu.X)));
            abs16_set(0x5E5A + g_cpu.X, g_cpu.A);
        }
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
    ldx(dp_x(0x30)); stx_abs(0xB2DB);
    tdc();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xB2DB));
    ldx(dp_x(0x00));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, abs16(0x5E43 + g_cpu.X)));
    abs16_set(0x5E43 + g_cpu.X, g_cpu.A);
    if (g_cpu._flag_C) { dc_set_mx(1, g_cpu.x_flag); inc_abs(0x5E45 + g_cpu.X); }
    dc_set_mx(1, g_cpu.x_flag);
    tdc();
    dc_set_mx(0, g_cpu.x_flag);
    ldx(dp_x(0x00));
    lda16(abs16(0x5E43 + g_cpu.X));
    dc_cmp16(g_cpu.A, 0x967F);
    if (g_cpu._flag_C) {
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x5E45 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0x98);
        if (g_cpu._flag_C) {
            lda8(0x7F); abs8_set(0x5E43 + g_cpu.X, 0x7F);
            lda8(0x96); abs8_set(0x5E44 + g_cpu.X, 0x96);
            lda8(0x98); abs8_set(0x5E45 + g_cpu.X, 0x98);
        }
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}

/* ---- batch 11 -------------------------------------------------------------- */
/* Enemy $9C tries one 8-pixel step along angle $DB: A03C/A053 = target,
 * 98A7 = moving; on collision (2926 / 28B0) stops and counts 9829.
 * Returns with X = $9C + 3 in $80. */
static void enemy_step_try(void)
{
    lda8(0x08); dp8_set(0xAE, 0x08);
    lda8(dp8(0xDB));
    dc_C101F9_glue();
    dp8_set(0x8C, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    dp8_set(0x8E, (u8)g_cpu.A);
}
static void enemy_step_place(void)
{
    lda8(0x01); abs8_set(0x98A7 + g_cpu.X, 0x01);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D26 + g_cpu.X), dp8(0x8C))); abs8_set(0xA053 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0F + g_cpu.X), dp8(0x8E))); abs8_set(0xA03C + g_cpu.X, (u8)g_cpu.A);
    inx(); inx(); inx();
    stx_dp(0x80);
    dc_C1285A_glue();
    abs8_set(0x9875, 0);
    dc_C12926_glue();
}
/* C1:3F5A  Enemy $9C steps in the direction of movement type A2B5 (11..17). */
DC_ENTRY(C13F5A, RTS)
{
    ldx(dp_x(0x9C));
    lda8(abs8(0x1D0F + g_cpu.X)); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D26 + g_cpu.X)); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
    lda8(0xFF); dp8_set(0x80, 0xFF);
    lda8(abs8(0xA2B5));
    u8 t = (u8)g_cpu.A;
    static const u8 ang[] = { 0xC0, 0x40, 0x00, 0x80, 0x20, 0x60, 0xA0 };
    static const s8 face[] = { -1, -1, -1, -1, 3, 1, 2 };
    for (u8 k = 0; k < 7; k++) {
        dc_cmp8(t, (u8)(0x11 + k));
        if (g_cpu._flag_Z) {
            if (face[k] >= 0) { lda8((u8)face[k]); dp8_set(0x80, (u8)face[k]); }
            if (k == 2) tdc(); else lda8(ang[k]);
            goto set;
        }
    }
    dp8_set(0x80, 0);
    lda8(0xE0);
set:
    abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
    dp8_set(0xDB, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    ldx(dp_x(0x9C));
    abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x80));
    if (!g_cpu._flag_N) abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
    enemy_step_try();
    ldx(dp_x(0x9C));
    enemy_step_place();
    int fail = g_cpu._flag_N;
    if (!fail) { dc_C128B0_glue(); fail = g_cpu._flag_N; }
    if (fail) {
        ldx(dp_x(0x9C));
        abs8_set(0x98A7 + g_cpu.X, 0);
        inc_abs(0x9829 + g_cpu.X);
    }
    ldx(dp_x(0x9C));
    inx(); inx(); inx();
    stx_dp(0x80);
}
/* C1:3959  Enemy $9C steps toward object 9752 along one axis (A2B5 = 8:
 * vertical); done (9829) when blocked or aligned in 32-pixel cells. */
DC_ENTRY(C13959, RTS)
{
    ldx(dp_x(0x9C));
    abs8_set(0xA5FD + g_cpu.X, 0);
    lda8(abs8(0x1D0F + g_cpu.X)); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D26 + g_cpu.X)); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x9752 + g_cpu.X)); tay();
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_Z) {
        lda8(abs8(0x1D23 + g_cpu.Y));
        dc_cmp8((u8)g_cpu.A, abs8(0x1D26 + g_cpu.X));
        lda8(g_cpu._flag_C ? 0x40 : 0xC0);
    } else {
        lda8(abs8(0x1D0C + g_cpu.Y));
        dc_cmp8((u8)g_cpu.A, abs8(0x1D0F + g_cpu.X));
        if (g_cpu._flag_C) tdc(); else lda8(0x80);
    }
    dp8_set(0xDB, (u8)g_cpu.A);
    lda8(0x08); dp8_set(0xAE, 0x08);
    ldx(dp_x(0x9C));
    lda8(dp8(0xDB));
    abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
    dc_C101F9_glue();
    dp8_set(0x8C, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    dp8_set(0x8E, (u8)g_cpu.A);
    lda8(dp8(0xDB));
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    ldx(dp_x(0x9C));
    abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
    enemy_step_place();
    if (g_cpu._flag_N) goto done;
    dc_C128B0_glue();
    if (g_cpu._flag_N) goto done;
    ldx(dp_x(0x9C));
    lda8(abs8(0x9752 + g_cpu.X)); tay();
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x08);
    {
        int vert = g_cpu._flag_Z;
        lda8(abs8((vert ? 0x1D23 : 0x1D0C) + g_cpu.Y));
        dc_C10119_glue();
        dp8_set(0x80, (u8)g_cpu.A);
        lda8(abs8((vert ? 0x1D26 : 0x1D0F) + g_cpu.X));
        dc_C10119_glue();
        dc_cmp8((u8)g_cpu.A, dp8(0x80));
        if (!g_cpu._flag_Z) goto out;
    }
done:
    ldx(dp_x(0x9C));
    inc_abs(0x9829 + g_cpu.X);
    lda8(abs8(0x5E0D + g_cpu.X)); abs8_set(0x5E1D + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x9752 + g_cpu.X)); tay();
    lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0x9831 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0x9839 + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x98A7 + g_cpu.X, 0);
out:
    inx(); inx(); inx();
    stx_dp(0x80);
}
/* C1:5DD3  Script command: loads the 24-byte palette entry (D1:4D90 + 24*n)
 * into 2382 and, when a target has A08C set under 99D2, repeats its colors
 * over 2200 from 99D3*2. */
static void pal_load(void);
DC_ENTRY(C15DD3, RTS)
{
    lda8(script_byte());
    dp8_set(0x80, (u8)g_cpu.A);
    lda8(0x02); dp8_set(0x8E, 0x02);
    pal_load();
}
/* C1:5DDC  (palette entry $80, advance by $8E) */
static void pal_load(void)
{
    dp8_set(0x82, 0);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x04);
    if (!g_cpu._flag_C) {
        tax();
        lda8(abs8(0xA014 + g_cpu.X));
        tax();
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (g_cpu._flag_C) {
            lda8(abs8(0x99D2));
            if (!g_cpu._flag_Z) {
                lda8(abs8(0xA08C + g_cpu.X));
                if (!g_cpu._flag_Z) dp8_set(0x82, (u8)(dp8(0x82) + 1));
            }
        }
        lda8(0x08); abs8_set(0x993C + g_cpu.X, 0x08);
    } else {
        tdc(); tax();
        for (;;) {
            lda8(abs8(0xA018 + g_cpu.X));
            if (g_cpu._flag_N) break;
            tay();
            lda8(abs8(0x99D2));
            if (!g_cpu._flag_Z) {
                lda8(abs8(0xA08C + g_cpu.Y));
                if (!g_cpu._flag_Z) dp8_set(0x82, (u8)(dp8(0x82) + 1));
            }
            lda8(0x08); abs8_set(0x993C + g_cpu.Y, 0x08);
            inx();
        }
    }
    lda8(dp8(0x80));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(3);
    dp16_set(0x80, g_cpu.A);
    asl_a(1);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x80)));
    tax();
    stx_dp(0x84);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    tdc(); tay();
    do {
        lda8(bus8(0xD14D90 + g_cpu.X)); abs8_set(0x2382 + g_cpu.Y, (u8)g_cpu.A);
        inx(); iny();
        cpy(0x0018);
    } while (!g_cpu._flag_Z);
    lda8(0xFF);
    for (int k = 0; k < 6; k++) abs8_set(0x2382 + k + g_cpu.Y, 0xFF);
    lda8(dp8(0x82));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0x99D3));
        dc_set_mx(0, g_cpu.x_flag);
        asl_a(1);
        tay();
        for (;;) {
            ldx(dp_x(0x84));
            lda16(0x000C); dp16_set(0x86, 0x000C);
            for (;;) {
                lda16(bus16(0xD14D90 + g_cpu.X)); abs16_set(0x2200 + g_cpu.Y, g_cpu.A);
                inx(); inx(); iny(); iny();
                cpy(0x0100);
                if (g_cpu._flag_Z) goto end;
                u16 n = (u16)(dp16(0x86) - 1); dp16_set(0x86, n); dc_set_flags_nz16(n);
                if (g_cpu._flag_Z) break;
            }
        }
    end:
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
    inc_abs(0xA00F);
    lda8(dp8(0x8E));
    script_advance();
}
/* C1:69C3  Script command: starts the walk of the actor(s) (operand = steps)
 * without moving yet; advances 2 once the walk has ended (A19D / A1E0). */
DC_ENTRY(C169C3, RTS)
{
    lda8(script_byte());
    abs8_set(0xA5BD, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax();
            lda8(abs8(0xA014 + g_cpu.X));
            tay();
            lda8(abs8(0xA337 + g_cpu.Y));
            if (g_cpu._flag_Z) { walk_init_party(); goto adv0; }
            lda8(abs8(0xA19D + g_cpu.Y));
            if (g_cpu._flag_Z) goto adv0;
            ldx(g_cpu.Y);
            abs8_set(0xA337 + g_cpu.X, 0);
            goto adv2;
        }
        tdc(); tax(); stx_dp(0x80);
        for (;;) {
            ldx(dp_x(0x80));
            lda8(abs8(0xA018 + g_cpu.X));
            if (g_cpu._flag_N) goto adv0;
            tay();
            lda8(abs8(0xA337 + g_cpu.Y));
            if (!g_cpu._flag_Z) break;
            walk_init_party();
            dp8_set(0x80, (u8)(dp8(0x80) + 1));
        }
        tdc(); tay(); sty_dp(0x84);
        for (;;) {
            lda8(abs8(0xA018 + g_cpu.Y));
            if (g_cpu._flag_N) break;
            tax();
            lda8(abs8(0xA19D + g_cpu.X));
            if (!g_cpu._flag_Z) abs8_set(0xA337 + g_cpu.X, 0);
            else dp8_set(0x84, (u8)(dp8(0x84) + 1));
            iny();
        }
        lda8(dp8(0x84));
        if (g_cpu._flag_Z) goto adv2;
        goto adv0;
    }
    ldy(abs_x(0xA2B6));
    lda8(abs8(0xA342 + g_cpu.Y));
    if (g_cpu._flag_Z) { walk_init_enemy(); goto adv0; }
    lda8(abs8(0xA1E0 + g_cpu.Y));
    if (g_cpu._flag_Z) goto adv0;
    ldx(g_cpu.Y);
    abs8_set(0xA342 + g_cpu.X, 0);
adv2:
    lda8(0x02); script_advance();
    return;
adv0:
    lda8(0x00); script_advance();
}
/* C1:2701  99C0.. = the live party (9604 = 0) or enemy slots within range
 * 9607 (CC:FB6F distance weights, 16-pixel cells) of object 9605. */
DC_ENTRY(C12701, RTS)
{
    lda8(abs8(0x9605)); tax();
    lda8(abs8(0x1D0C + g_cpu.X)); dc_C1011A_glue(); tay(); sty_dp(0x80);
    lda8(abs8(0x1D23 + g_cpu.X)); dc_C1011A_glue(); tay(); sty_dp(0x82);
    dc_C127D9_glue();
    ldx(0x000B); stx_dp(0x90);
    ldx(0x0003); stx_dp(0x8E);
    lda8(abs8(0x9604));
    if (!g_cpu._flag_Z) { stx_dp(0x90); tdc(); tax(); stx_dp(0x8E); }
    lda8(abs8(0x9605)); tax(); stx_dp(0x92);
    tdc(); tay();
    do {
        ldx(dp_x(0x8E));
        lda8(abs8(0x96F5 + g_cpu.X)); if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x9FF7 + g_cpu.X)); if (g_cpu._flag_N) goto next;
        lda8(abs8(0xA0A8 + g_cpu.X)); if (!g_cpu._flag_Z) goto next;
        lda8(abs8(0xA09B + g_cpu.X)); if (!g_cpu._flag_Z) goto next;
        cpx(dp_x(0x92)); if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x1D0C + g_cpu.X)); dc_C1011A_glue(); dp8_set(0x84, (u8)g_cpu.A); dp8_set(0x85, 0);
        lda8(abs8(0x1D23 + g_cpu.X)); dc_C1011A_glue(); dp8_set(0x86, (u8)g_cpu.A); dp8_set(0x87, 0);
        dc_set_mx(0, g_cpu.x_flag);
        for (int k = 0; k < 2; k++) {
            g_cpu._flag_C = 1;
            lda16(dc_sbc16(dp16(0x84 + 2 * k), dp16(0x80 + 2 * k)));
            if (g_cpu._flag_N) { lda16(g_cpu.A ^ 0xFFFF); lda16((u16)(g_cpu.A + 1)); }
            dp16_set(0x84 + 2 * k, g_cpu.A);
        }
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(dp8(0x84)); tax();
        lda8(bus8(0xCCFB6F + g_cpu.X)); dp8_set(0x84, (u8)g_cpu.A);
        lda8(dp8(0x86)); tax();
        lda8(bus8(0xCCFB6F + g_cpu.X));
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, dp8(0x84)));
        dc_cmp8((u8)g_cpu.A, abs8(0x9607));
        if (g_cpu._flag_Z || !g_cpu._flag_C) {
            g_cpu._flag_C = 0;
            lda8(dp8(0x8E));
            abs8_set(0x99C1 + g_cpu.Y, (u8)g_cpu.A);
            iny();
        }
    next:
        dp8_set(0x8E, (u8)(dp8(0x8E) + 1));
        lda8(dp8(0x8E));
        dc_cmp8((u8)g_cpu.A, dp8(0x90));
    } while (!g_cpu._flag_Z);
    lda8(abs8(0x9604));
    int z = g_cpu._flag_Z;
    lda8(dp8(0x92));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (z ? g_cpu._flag_C : !g_cpu._flag_C) { lda8(dp8(0x92)); abs8_set(0x99C0, (u8)g_cpu.A); }
    lda8(abs8(0x99C0));
    if (!g_cpu._flag_N) return;
    tdc(); tax();
    do {
        lda8(abs8(0x99C1 + g_cpu.X)); abs8_set(0x99C0 + g_cpu.X, (u8)g_cpu.A);
        inx();
        cpx(0x000B);
    } while (!g_cpu._flag_Z);
}

/* ---- batch 12 -------------------------------------------------------------- */
/* C1:B0B6  Mirrors the status bits of character record X (5E4A..5E53) into
 * the per-slot status flags of battle slot Y (bit 0 of B024, AFC1, ...). */
static void status_bit(u16 dst, u8 src, u8 mask)
{
    lda8(abs8(dst + g_cpu.Y) & 0xFE); abs8_set(dst + g_cpu.Y, (u8)g_cpu.A);
    lda8(src);
    g_cpu._flag_Z = !(src & mask);
    if (src & mask) { lda8(abs8(dst + g_cpu.Y) | 0x01); abs8_set(dst + g_cpu.Y, (u8)g_cpu.A); }
}
DC_ENTRY(C1B0B6, RTS)
{
    u16 x = g_cpu.X;
    lda8(abs8(0x5E4A + x));
    if (!(g_cpu.A & 0x80)) {
        lda8(abs8(0x5E4B + x));
        if (g_cpu.A & 0x02) {
            lda8(0x00); abs8_set(0xB03A + g_cpu.Y, 0);
            cpy(0x0003);
            if (g_cpu._flag_C) { tdc(); abs8_set(0x5E0A + g_cpu.Y, (u8)g_cpu.A); }
        }
        u8 e = abs8(0x5E4E + x) | abs8(0x5E53 + x);
        u8 d = abs8(0x5E4D + x) | abs8(0x5E52 + x);
        status_bit(0xB024, e, 0x04);
        status_bit(0xAFC1, abs8(0x5E4B + x), 0x40);
        status_bit(0xB00E, abs8(0x5E4B + x), 0x10);
        status_bit(0xB019, e, 0x20);
        status_bit(0xB02F, d, 0x02);
        status_bit(0xAFCC, abs8(0x5E4B + x), 0x20);
        status_bit(0xAFED, e, 0x40);
        status_bit(0xAFD7, d, 0x80);
        status_bit(0xAFE2, d, 0x40);
        status_bit(0xAFB6, abs8(0x5E4B + x), 0x80);
    }
    cpy(0x0003);
    if (!g_cpu._flag_C) {
        lda8(0x00); abs8_set(0xA0D1 + g_cpu.Y, 0);
        lda8(abs8(0x5E4A + x));
        if (!(g_cpu.A & 0x80)) {
            lda8(abs8(0x5E4B + x));
            if (g_cpu.A & 0x08) { lda8(0x01); abs8_set(0xA0D1 + g_cpu.Y, 0x01); return; }
        }
    }
    lda8(abs8(0x5E4B + x) & 0x04);
    if (!g_cpu._flag_Z) {
        dc_cmp8((u8)g_cpu.A, abs8(0xAE7A + g_cpu.Y));
        if (!g_cpu._flag_Z) {
            abs8_set(0xAE7A + g_cpu.Y, (u8)g_cpu.A);
            lda8(0xFF); abs8_set(0xB268 + g_cpu.Y, 0xFF);
        }
    } else {
        lda8((abs8(0x5E4B + x) ^ abs8(0xAE7A + g_cpu.Y)) & 0x04);
        if (!g_cpu._flag_Z) {
            lda8(0xFF); abs8_set(0xB268 + g_cpu.Y, 0xFF);
            lda8(0x00); abs8_set(0xAE7A + g_cpu.Y, 0);
        }
    }
    lda8(abs8(0x5E4B + x) & 0x80);
    if (!g_cpu._flag_Z) { lda8(0x04); abs8_set(0x5E0A + g_cpu.Y, 0x04); }
}
/* C1:780B  A2B0/A2B2 = point at fraction (CC:FB7F[Y]) between actor A014 /
 * A015 and A017 (Y >= 39: their hit points 9708 / 9713). */
static void tech_point(void)
{
    cpy(0x0039);
    if (g_cpu._flag_C) {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D0C + g_cpu.X), abs8(0x9708 + g_cpu.X)));
        dp8_set(0xAD, (u8)g_cpu.A);
        lda8(abs8(0x9713 + g_cpu.X));
        g_cpu._flag_C = 0;
        if (g_cpu._flag_N) {
            lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), abs8(0x9713 + g_cpu.X)));
            if (!g_cpu._flag_C) tdc();
        } else {
            lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), abs8(0x9713 + g_cpu.X)));
            if (g_cpu._flag_C) lda8(0xFF);
        }
    } else {
        lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xAD, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.X));
    }
}
static void tech_mix(void)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0xAF), dp16(0x82)));
    dc_C1011B_glue();
    dp16_set(0x82, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(C1780B, RTS)
{
    lda8(abs8(0xA014)); dp8_set(0x82, (u8)g_cpu.A);
    cpy(0x0032);
    if (!g_cpu._flag_C) { lda8((u8)g_cpu.Y); sub_a(0x24); }
    else {
        cpy(0x0039);
        if (!g_cpu._flag_C) { lda8(abs8(0xA015)); dp8_set(0x82, (u8)g_cpu.A); lda8((u8)g_cpu.Y); sub_a(0x32); }
        else { lda8((u8)g_cpu.Y); sub_a(0x39); }
    }
    dc_set_a8(asl8((u8)g_cpu.A));
    tax();
    lda8(bus8(0xCCFB7F + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A); dp8_set(0xAE, (u8)g_cpu.A);
    lda8(bus8(0xCCFB80 + g_cpu.X)); dp8_set(0x81, (u8)g_cpu.A);
    lda8(dp8(0x82)); tax();
    tech_point();
    dp8_set(0x84, (u8)g_cpu.A);
    dc_C10089_glue();
    ldx(dp_x(0xAF)); stx_dp(0x82);
    lda8(abs8(0xA017)); tax();
    tech_point();
    dp8_set(0x85, (u8)g_cpu.A);
    lda8(dp8(0x81)); dp8_set(0xAE, (u8)g_cpu.A);
    dc_C10089_glue();
    tech_mix();
    lda8(dp8(0x82)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(dp8(0x84)); dp8_set(0xAD, (u8)g_cpu.A);
    lda8(dp8(0x80)); dp8_set(0xAE, (u8)g_cpu.A);
    dc_C10089_glue();
    ldx(dp_x(0xAF)); stx_dp(0x82);
    lda8(dp8(0x85)); dp8_set(0xAD, (u8)g_cpu.A);
    lda8(dp8(0x81)); dp8_set(0xAE, (u8)g_cpu.A);
    dc_C10089_glue();
    tech_mix();
    lda8(dp8(0x82)); abs8_set(0xA2B2, (u8)g_cpu.A);
}
/* C1:4D04 / 4D08  Script command: sets the target point of the actor(s) to
 * their position + 4*operand ($8E = 2 waits, 1 does not advance). */
static void aim_party(int group)
{
    dp8_set(0x8A, (u8)g_cpu.A);
    dc_set_a8(asl8((u8)g_cpu.A));
    tay();
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0x8A)));
    tax();
    lda8(abs8(0x1C99 + g_cpu.X)); abs8_set(0xA1E8 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1C9A + g_cpu.X)); abs8_set(0xA1E9 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1CBA + g_cpu.X)); abs8_set(0xA1FE + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1CBB + g_cpu.X)); abs8_set(0xA1FF + g_cpu.Y, (u8)g_cpu.A);
    lda8(dp8(0x80));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(2);
    dp16_set(0x82, g_cpu.A);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, abs16(0xA1FE + g_cpu.Y)));
    abs16_set(0xA1FE + g_cpu.Y, g_cpu.A);
    if (group) tax();
    g_cpu._flag_C = 0;
    lda16(dp16(0x82));
    lda16(dc_adc16(g_cpu.A, abs16(0xA1E8 + g_cpu.Y)));
    abs16_set(0xA1E8 + g_cpu.Y, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x8A)); tay();
    lda8(dp8(0x8E)); abs8_set(0xA124 + g_cpu.Y, (u8)g_cpu.A);
    tdc();
    abs8_set(0xA12F + g_cpu.Y, (u8)g_cpu.A);
    lda8((u8)(g_cpu.A - 1));
    abs8_set(0xA13A + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA214 + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C14D08, RTS)
{
    lda8(script_byte());
    dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            ldx(abs_x(0xA010));
            lda8(abs8(0xA014 + g_cpu.X));
            aim_party(0);
        } else {
            tdc(); tax(); stx_dp(0x8C);
            for (;;) {
                ldx(dp_x(0x8C));
                lda8(abs8(0xA018 + g_cpu.X));
                if (g_cpu._flag_N) break;
                aim_party(1);
                dp8_set(0x8C, (u8)(dp8(0x8C) + 1));
            }
        }
    } else {
        ldy(abs_x(0xA2B6));
        lda8(dp8(0x80)); tax();
        lda8(abs8(0xA2A0 + g_cpu.X)); abs8_set(0xA238 + g_cpu.Y, (u8)g_cpu.A);
        lda8(abs8(0xA2B6));
        dc_set_a8(asl8((u8)g_cpu.A));
        tay();
        lda8(dp8(0x80));
        dc_set_a8(asl8((u8)g_cpu.A));
        tax();
        lda8(abs8(0xA280 + g_cpu.X)); abs8_set(0xA220 + g_cpu.Y, (u8)g_cpu.A);
        lda8(abs8(0xA281 + g_cpu.X)); abs8_set(0xA221 + g_cpu.Y, (u8)g_cpu.A);
        ldy(abs_x(0xA2B6));
        lda8(dp8(0x8E)); abs8_set(0xA1A8 + g_cpu.Y, (u8)g_cpu.A);
        tdc();
        abs8_set(0xA1B0 + g_cpu.Y, (u8)g_cpu.A);
        lda8((u8)(g_cpu.A - 1));
        abs8_set(0xA1B8 + g_cpu.Y, (u8)g_cpu.A);
        abs8_set(0xA2B8 + g_cpu.Y, (u8)g_cpu.A);
    }
    lda8(dp8(0x8E));
    dc_cmp8((u8)g_cpu.A, 0x01);
    if (g_cpu._flag_Z) return;
    op_done(0x02);
}
DC_ENTRY(C14D04, RTS) { lda8(0x02); dp8_set(0x8E, 0x02); dc_C14D08_glue(); }

/* ---- batch 13 -------------------------------------------------------------- */
/* C1:CA1A  Builds the dual-tech menu entries (39..65) of party slot $06 for
 * the learned combos (283E bits) whose two partners are present and able. */
DC_ENTRY(C1CA1A, RTS)
{
    tdc(); tax();
    stx_dp(0x08); stx_dp(0x00); stx_dp(0x10); stx_dp(0x0E); stx_dp(0x14);
    lda8(dp8(0x06)); tax();
    lda8(abs8(0xAEFF + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    lda8(0x39); dp8_set(0x08, 0x39);
    lda8(0x66); dp8_set(0x0E, 0x66);
    lda8(dp8(0x08));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(1); dp16_set(0x02, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x06));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(1); tax();
    lda16(bus16(0xFDA84F + g_cpu.X)); dp16_set(0x04, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0x00));
    lda8(abs8(0x283E + g_cpu.X)); dp8_set(0x0C, (u8)g_cpu.A);
    for (;;) {
        lda8(dp8(0x0C) & 0x80);
        if (g_cpu._flag_Z) goto next;
        lda8(dp8(0x08));
        dc_C1CB93_glue();
        for (int k = 0; k < 2; k++) {
            tdc();
            lda8(abs8(0xB2EA + k));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (g_cpu._flag_Z) goto next;
            tax();
            lda8(abs8(0xB1BE + g_cpu.X));
            if (g_cpu._flag_N) goto next;
        }
        lda8(abs8(0xB2EA)); tax();
        lda8(abs8(0xB1BE + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, dp8(0x06));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xB2EB)); tax();
            lda8(abs8(0xB1BE + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, dp8(0x06));
            if (!g_cpu._flag_Z) goto next;
        }
        tdc();
        ldx(dp_x(0x02));
        lda8(bus8(0xCC1ACC + g_cpu.X) & 0x3F);
        ldx(dp_x(0x04)); abs8_set(0x0048 + g_cpu.X, (u8)g_cpu.A);
        ldx(dp_x(0x02));
        lda8(bus8(0xCC1ACB + g_cpu.X));
        ldx(dp_x(0x04)); abs8_set(0x0047 + g_cpu.X, (u8)g_cpu.A);
        ldx(dp_x(0x04));
        lda8(dp8(0x08)); abs8_set(0x0046 + g_cpu.X, (u8)g_cpu.A);
        ldx(dp_x(0x04));
        lda8(0xFF);
        abs8_set(0x0049 + g_cpu.X, 0xFF); abs8_set(0x004B + g_cpu.X, 0xFF); abs8_set(0x004C + g_cpu.X, 0xFF);
        tdc(); lda8(dp8(0x08)); dc_C1CB48_glue(); tay();
        tdc(); lda8(abs8(0xB2EA)); dc_C1CC0B_glue();
        tdc(); lda8(dp8(0x08)); dc_C1CB61_glue(); tay();
        tdc(); lda8(abs8(0xB2EB)); dc_C1CC0B_glue();
        lda8(abs8(0xB2EB)); tax();
        lda8(abs8(0xB1BE + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, dp8(0x06));
        if (g_cpu._flag_Z) { lda8(abs8(0xB2EA)); tax(); lda8(abs8(0xB1BE + g_cpu.X)); }
        lda8((u8)g_cpu.A | 0xF0);
        ldx(dp_x(0x04)); abs8_set(0x004A + g_cpu.X, (u8)g_cpu.A);
        dp8_set(0x14, (u8)(dp8(0x14) + 1));
        lda8(dp8(0x14));
        dc_cmp8((u8)g_cpu.A, 0x06);
        if (g_cpu._flag_C) return;
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0x04), 0x0007)); dp16_set(0x04, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    next:
        lda8(asl8(dp8(0x0C))); dp8_set(0x0C, (u8)g_cpu.A);
        dp8_set(0x08, (u8)(dp8(0x08) + 1));
        dp8_set(0x10, (u8)(dp8(0x10) + 1));
        lda8(dp8(0x10));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (g_cpu._flag_C) {
            ldx(dp_x(0x00)); inx(); stx_dp(0x00);
            lda8(abs8(0x283E + g_cpu.X)); dp8_set(0x0C, (u8)g_cpu.A);
            dp8_set(0x10, 0);
        }
        dc_set_mx(0, g_cpu.x_flag);
        dp16_set(0x02, (u16)(dp16(0x02) + 2));
        lda16(dp16(0x0A));
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, 0x000C)); dp16_set(0x0A, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(dp8(0x08));
        dc_cmp8((u8)g_cpu.A, dp8(0x0E));
        if (g_cpu._flag_C) return;
    }
}
/* C1:E65F  Damage $18 scaling: special values 3E / 3C from the attacker's
 * 5E2D stat, then x16/20 or x18/20 by attacker status 5E4B, then the AE4E
 * multiplier /4; capped at 255. */
static void scale18(u16 mul, u16 div)
{
    tdc();
    lda8(dp8(0x18)); tax(); stx_dp(0x28);
    ldx(mul); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    ldx(div); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C)); stx_dp(0x18);
}
DC_ENTRY(C1E65F, RTS)
{
    lda8(dp8(0x18));
    dc_cmp8((u8)g_cpu.A, 0x3E);
    if (g_cpu._flag_Z) {
        dc_set_mx(0, g_cpu.x_flag);
        lda16(dp16(0x18));
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, abs16(0xB1F6)));
        tax(); tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x5E2D + g_cpu.X)); dp8_set(0x18, (u8)g_cpu.A);
    } else {
        dc_cmp8((u8)g_cpu.A, 0x3C);
        if (g_cpu._flag_Z) {
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = 0;
            lda16(dp16(0x18));
            lda16(dc_adc16(g_cpu.A, abs16(0xB1F6)));
            tax(); tdc();
            dp16_set(0x28, g_cpu.A); dp16_set(0x2A, g_cpu.A);
            dc_set_mx(1, g_cpu.x_flag);
            lda8(abs8(0x5E2D + g_cpu.X)); dp8_set(0x28, (u8)g_cpu.A);
            ldx(0x0005); stx_dp(0x2A);
            dc_C1C90B_glue();
            ldx(dp_x(0x2C)); stx_dp(0x28);
            ldx(0x0002); stx_dp(0x2A);
            dc_C1C92A_glue();
            ldx(dp_x(0x2C)); stx_dp(0x18);
        }
    }
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E4B + g_cpu.X)); dp8_set(0x0E, (u8)g_cpu.A);
    lda8((u8)g_cpu.A & 0x82);
    if (!g_cpu._flag_Z) scale18(0x0010, 0x0014);
    else {
        lda8(dp8(0x0E) & 0x40); if (!g_cpu._flag_Z) scale18(0x0012, 0x0014);
        lda8(dp8(0x0E) & 0x05); if (!g_cpu._flag_Z) scale18(0x0012, 0x0014);
        lda8(dp8(0x0E) & 0x20); if (!g_cpu._flag_Z) scale18(0x0012, 0x0014);
    }
    lda8(abs8(0xAE4E));
    if (g_cpu.A & 0x03) {
        lda8((u8)g_cpu.A & 0x03);
        tax();
        dp8_set(0x2A, (u8)g_cpu.A);
        lda8(dp8(0x18)); dp8_set(0x28, (u8)g_cpu.A);
        dc_C1C90B_glue();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(dp16(0x2C)); lsr16(2); dp16_set(0x18, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
    ldx(dp_x(0x18));
    if (!g_cpu._flag_N) { cpx(0x00FF); if (!g_cpu._flag_C) return; }
    ldx(0x00FF); stx_dp(0x18);
}
/* C1:0B0B  Draws tech/item name slot $80 of the list (94D0) into the
 * 9A29.. window: blank, the "---" strings of CC:FB4B, or the name
 * (CC:15C4 + 11*n) with its 4-cell MP column cleared. */
static void name_row(u16 dst, u16 src, u16 n)
{
    tdc(); tay();
    ldx(dp_x(0x82));
    do {
        lda8(abs8(src + g_cpu.Y)); abs8_set(dst + g_cpu.X, (u8)g_cpu.A); inx();
        lda8(0x2D); abs8_set(dst + g_cpu.X, 0x2D); inx();
        iny(); cpy(n);
    } while (!g_cpu._flag_Z);
}
static void blank_row(u16 dst)
{
    tdc(); tay();
    ldx(dp_x(0x82));
    do {
        lda8(0xFF); abs8_set(dst + g_cpu.X, 0xFF); inx();
        lda8(0x2D); abs8_set(dst + g_cpu.X, 0x2D); inx();
        iny(); cpy(0x0004);
    } while (!g_cpu._flag_Z);
}
DC_ENTRY(C10B0B, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    ldx(0x001E);
    lda16(0xFFFF);
    do { abs16_set(0x94A0 + g_cpu.X, 0xFFFF); dex(); dex(); } while (!g_cpu._flag_N);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0x80));
    lda8(abs8(0x94D0 + g_cpu.X));
    u8 v = (u8)g_cpu.A;
    dc_cmp8(v, 0xFF);
    if (v == 0xFF) goto short_row;
    dc_cmp8(v, 0xFE);
    if (v == 0xFE) goto short_row;
    dc_cmp8(v, 0xFD);
    if (v == 0xFD || v == 0xFC) {
        if (v == 0xFC) dc_cmp8(v, 0xFC);
        ldx(v == 0xFD ? 0x0012 : 0x0000);
        tdc(); tay();
        do {
            lda8(bus8(0xCCFB4B + g_cpu.X)); abs8_set(0x94A0 + g_cpu.Y, (u8)g_cpu.A);
            inx(); iny(); cpy(0x0012);
        } while (!g_cpu._flag_Z);
        goto short_row;
    }
    dc_cmp8(v, 0xFC);
    dc_cmp8(v, 0xFB);
    if (v == 0xFB) { ldx(dp_x(0x88)); goto long_row; }
    ldx(dp_x(0x88));
    dc_set_mx(0, g_cpu.x_flag);
    dp16_set(0x8E, g_cpu.A);
    asl_a(2); dp16_set(0x90, g_cpu.A);
    asl_a(1);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x90)));
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(g_cpu.A, dp16(0x8E)));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, 0x15C4));
    tax();
    ldy(0x94A0);
    lda16(0x000A);
    dc_mvn(0x7E, 0xCC);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0x0000 + g_cpu.Y, (u8)g_cpu.A);
    dc_C101A9_glue();
long_row:
    dp8_set(0x88, (u8)(dp8(0x88) + 1));
    dc_C10BD3_glue();
    name_row(0x9A2F, 0x94A0, 0x000B);
    name_row(0x99EF, 0x94B0, 0x000B);
    blank_row(0x9A45);
    blank_row(0x9A05);
    return;
short_row:
    ldx(dp_x(0x88));
    dp8_set(0x88, (u8)(dp8(0x88) + 1));
    dc_C10BD3_glue();
    name_row(0x9A29, 0x94A0, 0x0012);
}
/* C1:E77B  Healing amount of tech/item effect: 5E2F[$1A] * $18 / 20 plus a
 * random 0..(that/10 + 2), at least 1, capped by max HP / MP (type $1A = 7 / 3);
 * written for the target ($10) and the user ($0E) with the EBF8 popups. */
DC_ENTRY(C1E77B, RTS)
{
    tdc();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0xB1F6), dp16(0x1A)));
    tax();
    lda16(abs16(0x5E2F + g_cpu.X)); dp16_set(0x28, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x18)); tax(); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    lda8(0x14); tax(); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C)); stx_dp(0x30);
    tdc(); tax();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    ldx(0x000A); stx_dp(0x2A);
    dc_C1C92A_glue();
    lda8(dp8(0x2C));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, 0x03));
    ldx(0x0000);
    dc_C1AF22_glue();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x30)));
    dc_cmp16(g_cpu.A, 0x0000);
    if (g_cpu._flag_Z) lda16((u16)(g_cpu.A + 1));
    dp16_set(0x00, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x1A));
    for (int k = 0; k < 2; k++) {
        u8 t = k ? 0x03 : 0x07;
        dc_cmp8((u8)g_cpu.A, t);
        if (!g_cpu._flag_Z) continue;
        lda8(k ? 0x80 : 0xC0); abs8_set(0xB202, (u8)g_cpu.A);
        dc_set_mx(0, g_cpu.x_flag);
        ldy(abs_x(0xB1F6));
        lda16(abs16((k ? 0x5E30 : 0x5E34) + g_cpu.Y));
        dc_cmp16(g_cpu.A, dp16(0x00));
        if (!g_cpu._flag_C) dp16_set(0x00, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        break;
    }
    dc_C1E8C0_glue();
    tdc();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x00)); ldx(dp_x(0x10)); abs16_set(0xAD9C + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0xB18B)); tay();
    ldx(dp_x(0x00)); stx_abs(0xAD89);
    dc_C1EBF8_glue();
    dc_C1E89F_glue();
    tdc();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x00)); ldx(dp_x(0x0E)); abs16_set(0xAD9C + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0xB1FD)); tay();
    lda8(0x40); abs8_set(0xB202, 0x40);
    lda8(dp8(0x1A));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_Z) { lda8(0x00); abs8_set(0xB202, 0); }
    ldx(dp_x(0x00)); stx_abs(0xAD89);
    dc_C1EBF8_glue();
    for (int k = 0; k < 2; k++) {
        lda8(abs8(0x001A));
        dc_cmp8((u8)g_cpu.A, 0x03);
        u8 v = g_cpu._flag_Z ? (k ? 0x01 : 0x03) : (k ? 0x02 : 0x04);
        lda8(v);
        dc_set_mx(0, g_cpu.x_flag);
        ldx(dp_x(k ? 0x10 : 0x0E));
        abs16_set(0xAD9E + g_cpu.X, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
    dc_C1E63B_glue();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}

/* ---- batch 14 -------------------------------------------------------------- */
/* C1:224B  Target list = the party members within 16 of object 960F
 * (2701), copied to A62D with 960C = 80. */
DC_ENTRY(C1224B, RTS)
{
    abs8_set(0x9604, 0);
    lda8(abs8(0x960F)); abs8_set(0x9605, (u8)g_cpu.A);
    lda8(0x10); abs8_set(0x9607, 0x10);
    dc_C12701_glue();
    lda8(0x80); abs8_set(0x960C, 0x80);
    ldx(0x000A);
    do {
        lda8(abs8(0x99C0 + g_cpu.X)); abs8_set(0xA62D + g_cpu.X, (u8)g_cpu.A);
        dex();
    } while (!g_cpu._flag_N);
}
/* C1:7612 / 75E3  A2B4 = facing (C0:F700 of the angle) from point $80/$81
 * toward the screen center 80/70, or toward actor A014[Y-0A]. */
static void face_point(void)
{
    dc_C10222_glue();
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    abs8_set(0xA2B4, (u8)g_cpu.A);
}
DC_ENTRY(C17612, RTS)
{
    lda8(dp8(0x80)); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(dp8(0x81)); dp8_set(0xD4, (u8)g_cpu.A);
    lda8(0x80); dp8_set(0xD5, 0x80);
    lda8(0x70); dp8_set(0xD6, 0x70);
    face_point();
}
DC_ENTRY(C175E3, RTS)
{
    lda8(dp8(0x80)); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(dp8(0x81)); dp8_set(0xD4, (u8)g_cpu.A);
    lda8((u8)g_cpu.Y); sub_a(0x0A);
    tax();
    lda8(abs8(0xA014 + g_cpu.X)); tax();
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0xD6, (u8)g_cpu.A);
    face_point();
}
/* C1:4021  Enemy $9C turns toward the screen center and starts moving. */
DC_ENTRY(C14021, RTS)
{
    ldx(dp_x(0x9C));
    lda8(abs8(0x1D0F + g_cpu.X)); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(abs8(0x1D26 + g_cpu.X)); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A); dp8_set(0xD4, (u8)g_cpu.A);
    lda8(0x80); dp8_set(0xD5, 0x80);
    lda8(0x70); dp8_set(0xD6, 0x70);
    dc_C10222_glue();
    ldx(dp_x(0x9C));
    abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    ldx(dp_x(0x9C));
    abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0x98A7 + g_cpu.X, 0x01);
    inx(); inx(); inx();
    stx_dp(0x80);
}
/* C1:0A88  Draws the two-column tech list page of party slot 95D5 (0B0B per
 * entry, rows 80 apart). */
DC_ENTRY(C10A88, RTS)
{
    lda8(abs8(0x95D5)); tax();
    lda8(bus8(0xCCF38C + g_cpu.X)); dp8_set(0xAF, (u8)g_cpu.A);
    lda8(abs8(0x95EB + g_cpu.X)); dp8_set(0x88, (u8)g_cpu.A); dp8_set(0x86, (u8)g_cpu.A);
    lda8(0x02); dp8_set(0x87, 0x02);
    dp8_set(0x89, 0);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0x88), dp8(0xAF)));
    dp8_set(0x80, (u8)g_cpu.A); dp8_set(0x81, 0);
    tdc();
    dp8_set(0x82, (u8)g_cpu.A); dp8_set(0x83, (u8)g_cpu.A);
    do {
        for (int k = 0; k < 2; k++) {
            dc_C10B0B_glue();
            if (!k) dp8_set(0x80, (u8)(dp8(0x80) + 1));
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = 0;
            lda16(dc_adc16(dp16(0x82), 0x0080));
            dp16_set(0x82, g_cpu.A);
            if (!k) tdc();
            dc_set_mx(1, g_cpu.x_flag);
        }
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
        u8 n = (u8)(dp8(0x87) - 1); dp8_set(0x87, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
}
/* C1:095D  Clears the item window (0E80, 0180 bytes) and draws 3 entries
 * from list position $80. */
DC_ENTRY(C1095D, RTS)
{
    lda8(abs8(0x95D5)); tax(); stx_dp(0x96);
    ldx(0x0180);
    dc_set_mx(0, g_cpu.x_flag);
    tdc(); tay();
    dc_set_mx(1, g_cpu.x_flag);
    do {
        abs8_set(0x0E80 + g_cpu.Y, (u8)g_cpu.A);
        iny(); dex();
    } while (!g_cpu._flag_Z);
    dp8_set(0x81, 0);
    dc_set_mx(0, g_cpu.x_flag);
    tdc(); dp16_set(0x82, g_cpu.A);
    lda16(dp16(0x80)); dp16_set(0x84, g_cpu.A);
    asl_a(2);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x84)));
    dp16_set(0x80, g_cpu.A);
    tax();
    tdc(); dp16_set(0x86, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
    do {
        dc_C109B0_glue();
        dc_set_mx(0, g_cpu.x_flag);
        lda16((u16)(dp16(0x86) + 1));
        asl_a(7);
        dp16_set(0x82, g_cpu.A);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0x80), 0x0005));
        dp16_set(0x80, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x86, (u8)(dp8(0x86) + 1));
        lda8(dp8(0x86));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
}

/* ---- batch 15 -------------------------------------------------------------- */
/* Walk timing for slot Y at speed tab[Y]: steps[Y] = major distance / speed,
 * A880/A882 = unit steps scaled by the speed (00A7). */
static void walk_steps(u16 tab, u16 steps)
{
    lda8(dp8(0x8C));
    lda8(dp8(g_cpu._flag_Z ? 0xDE : 0xE0));
    dp8_set(0xB1, (u8)g_cpu.A); dp8_set(0xB2, 0);
    lda8(abs8(tab + g_cpu.Y)); dp8_set(0xB3, (u8)g_cpu.A);
    dc_C100D7_glue();
    lda8(dp8(0xB5)); abs8_set(steps + g_cpu.Y, (u8)g_cpu.A);
    for (int k = 0; k < 2; k++) {
        u16 v = k ? 0xA882 : 0xA880;
        lda8(abs8(v)); dp8_set(0xA7, (u8)g_cpu.A);
        lda8(abs8(v + 1)); dp8_set(0xA8, (u8)g_cpu.A);
        lda8(abs8(tab + g_cpu.Y)); dp8_set(0xA5, (u8)g_cpu.A);
        dc_C100A7_glue();
        ldx(dp_x(0xA9)); stx_abs(v);
    }
}
/* C1:5135  Starts party slot X walking to point A2B0/A2B2 at speed A324:
 * per-frame 8.8 steps A880/A882 scaled by the speed, step count ABEB. */
DC_ENTRY(C15135, RTS)
{
    stx_dp(0x86);
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A); abs8_set(0xA35D + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A); abs8_set(0xA370 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xA2B0)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); dp8_set(0xD6, (u8)g_cpu.A);
    dc_C10222_glue();
    dc_C165BA_glue();
    ldy(dp_x(0x86));
    walk_steps(0xA324, 0xABEB);
    ldy(dp_x(0x86));
    lda8(dp8(0xDB)); tax();
    lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set(0x96DB + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B0)); abs8_set(0xA383 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); abs8_set(0xA396 + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA2EB + g_cpu.Y, 0x01);
    tdc();
    abs8_set(0xA19D + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA86D + g_cpu.Y, (u8)g_cpu.A);
    phy();
    lda8((u8)g_cpu.Y);
    dc_set_a8(asl8((u8)g_cpu.A)); tay();
    dc_set_a8(asl8((u8)g_cpu.A)); tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xA880)); abs16_set(0xA888 + g_cpu.Y, g_cpu.A);
    lda16(abs16(0xA882)); abs16_set(0xA8AE + g_cpu.Y, g_cpu.A);
    tdc();
    abs16_set(0xA8D4 + g_cpu.Y, g_cpu.A);
    abs16_set(0xA8FA + g_cpu.Y, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
    ply();
    lda8(0x01); abs8_set(0xA337 + g_cpu.Y, 0x01);
}
/* C1:38BF  Enemy $9C steps 8 pixels toward object 9752; when blocked it
 * counts 9829 and takes the target's position as its goal. */
DC_ENTRY(C138BF, RTS)
{
    ldx(dp_x(0x9C));
    abs8_set(0xA5FD + g_cpu.X, 0);
    lda8(abs8(0x1D0F + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D26 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x9752 + g_cpu.X)); tay();
    lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD6, (u8)g_cpu.A);
    lda8(0x08); dp8_set(0xAE, 0x08);
    dc_C10222_glue();
    ldx(dp_x(0x9C));
    abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
    dc_C101F9_glue();
    dp8_set(0x8C, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    dp8_set(0x8E, (u8)g_cpu.A);
    lda8(dp8(0xDB)); tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    ldx(dp_x(0x9C));
    abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
    enemy_step_place();
    int fail = g_cpu._flag_N;
    if (!fail) { dc_C128B0_glue(); fail = g_cpu._flag_N; }
    if (fail) {
        ldx(dp_x(0x9C));
        inc_abs(0x9829 + g_cpu.X);
        lda8(abs8(0x5E0D + g_cpu.X)); abs8_set(0x5E1D + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x9752 + g_cpu.X)); tay();
        lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0x9831 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0x9839 + g_cpu.X, (u8)g_cpu.A);
        abs8_set(0x98A7 + g_cpu.X, 0);
    }
    inx(); inx(); inx();
    stx_dp(0x80);
}
/* C1:2C02  Pushes object 986F away from 9870 (angle from 9870 to 986F) until
 * it is clear (2C2A). */
DC_ENTRY(C12C02, RTS)
{
    abs8_set(0x9872, 0);
    dp8_set(0xAE, 0);
    lda8(abs8(0x986F)); tay();
    lda8(abs8(0x9870)); tax();
    lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0x8A, (u8)g_cpu.A); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0x8B, (u8)g_cpu.A); dp8_set(0xD4, (u8)g_cpu.A);
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0xD6, (u8)g_cpu.A);
    dc_C10222_glue();
    dc_C12C2A_glue();
}
/* C1:4EA2  Script command: aim (4D08 with $8E = 1) the first time, then wait
 * until the move counter A124 / A1A8 runs out. */
DC_ENTRY(C14EA2, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldx(abs_x(0xA2B6));
        lda8(abs8(0xA1C8 + g_cpu.X));
        if (g_cpu._flag_Z) {
            inc_abs(0xA1C8 + g_cpu.X);
            lda8(0x01); dp8_set(0x8E, 0x01);
            dc_C14D08_glue();
            tdc();
            script_advance();
            return;
        }
        lda8(abs8(0xA1A8 + g_cpu.X));
        if (!g_cpu._flag_Z) return;
        abs8_set(0xA1C8 + g_cpu.X, 0);
        lda8(0x02); script_advance();
        return;
    }
    dc_cmp8((u8)g_cpu.A, 0x04);
    int group = g_cpu._flag_Z;
    if (group) lda8(abs8(0xA018));
    else { tax(); lda8(abs8(0xA014 + g_cpu.X)); }
    tax();
    lda8(abs8(0xA150 + g_cpu.X));
    if (g_cpu._flag_Z) {
        inc_abs(0xA150 + g_cpu.X);
        lda8(0x01); dp8_set(0x8E, 0x01);
        dc_C14D08_glue();
        return;
    }
    if (group) { lda8(abs8(0xA018)); tax(); }
    lda8(abs8(0xA124 + g_cpu.X));
    if (!g_cpu._flag_Z) return;
    abs8_set(0xA150 + g_cpu.X, 0);
    lda8(0x02); script_advance();
}
/* C1:DBAA  Applies status effect $18 of group $16 to the target B1F6
 * (5E4A + group), unless immune (5E4F); group 0..2 also updates the haste /
 * slow pair, $40 = 80 marks an immune hit of group 1. */
DC_ENTRY(C1DBAA, RTS)
{
    tdc();
    lda8(dp8(0x16));
    u8 v = (u8)g_cpu.A;
    dc_cmp8(v, 0xFF);
    if (v == 0xFF) return;
    if (v <= 0x02) {
        dc_cmp8(v, 0x00);
        if (v) dc_cmp8(v, 0x01);
        if (v == 2) dc_cmp8(v, 0x02);
        dc_set_mx(0, g_cpu.x_flag);
        lda16(dp16(0x16));
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, abs16(0xB1F6)));
        tax(); tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x5E4F + g_cpu.X) & dp8(0x18));
        if (!g_cpu._flag_Z) {
            lda8(dp8(0x16));
            dc_cmp8((u8)g_cpu.A, 0x01);
            if (g_cpu._flag_Z) { lda8(0x80); dp8_set(0x40, 0x80); return; }
            goto clear;
        }
        lda8(abs8(0x5E4A + g_cpu.X) | dp8(0x18)); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
        dc_C1E006_glue();
        ldx(abs_x(0xB1F6));
        lda8(abs8(0x5E4B + g_cpu.X));
        if (g_cpu.A & 0x20) {
            lda8(abs8(0x5E52 + g_cpu.X));
            if (!(g_cpu.A & 0x80)) {
                lda8(abs8(0x5E4D + g_cpu.X) & 0x7F); abs8_set(0x5E4D + g_cpu.X, (u8)g_cpu.A);
                goto clear;
            }
        }
        lda8(abs8(0x5E4B + g_cpu.X) & 0xDF); abs8_set(0x5E4B + g_cpu.X, (u8)g_cpu.A);
        goto clear;
    }
    dc_cmp8(v, 0x00); dc_cmp8(v, 0x01); dc_cmp8(v, 0x02);
    dc_cmp8(v, 0x03);
    if (v == 0x03) {
        lda8(dp8(0x18));
        if (g_cpu.A & 0x80) {
            ldx(abs_x(0xB1F6));
            lda8(abs8(0x5E4B + g_cpu.X) & 0xDF); abs8_set(0x5E4B + g_cpu.X, (u8)g_cpu.A);
        }
    } else {
        dc_cmp8(v, 0x04);
        if (v != 0x04) {
            dc_cmp8(v, 0x05);
            if (v != 0x05) goto clear;
        }
    }
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xB1F6));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x16)));
    tax(); tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0x5E4A + g_cpu.X) | dp8(0x18)); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
    if (v != 0x05) dc_C1E006_glue();
clear:
    tdc();
    dp8_set(0x16, (u8)g_cpu.A);
}

/* ---- batch 16 -------------------------------------------------------------- */
/* C1:F205  After battle: applies the exp growth of each present party member
 * (F39A), then learns the dual/triple techs (CC:2778 table of combos per
 * party pair B2D6..B2D9, F533). */
DC_ENTRY(C1F205, RTS)
{
    tdc(); tax();
    lda8(0xFF);
    do { abs8_set(0xB305 + g_cpu.X, 0xFF); inx(); cpx(0x000A); } while (!g_cpu._flag_C);
    abs8_set(0xB310, 0xFF);
    abs8_set(0xB30F, 0);
    tdc();
    abs8_set(0xB2D0, (u8)g_cpu.A);
    do {
        tax();
        lda8(abs8(0xAEFF + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xB2D0));
            asl_a(3);
            tax(); stx_abs(0xB3CC);
            lda8(abs8(0xB2D0));
            dc_set_mx(0, g_cpu.x_flag);
            abs16_set(0xB1F4, g_cpu.A);
            xba16();
            lsr16(1);
            tax();
            dp16_set(0x00, g_cpu.A);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            lda8(abs8(0x5E2F + g_cpu.X) & 0x10);
            if (g_cpu._flag_Z) {
                lda8(abs8(0x5E2D + g_cpu.X)); abs8_set(0xB2D1, (u8)g_cpu.A);
                dc_C1F39A_glue();
            }
        }
        inc_abs(0xB2D0);
        lda8(abs8(0xB2D0));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_C);
    dc_C1F2DB_glue();
    tdc();
    abs8_set(0xB2D0, (u8)g_cpu.A);
    do {
        tdc();
        ldx(0x2778); stx_dp(0x02);
        abs8_set(0xB2D2, 0);
        for (;;) {
            ldx(dp_x(0x02));
            lda8(bus8(0xCC0000 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (g_cpu._flag_Z) break;
            dp8_set(0x08, (u8)g_cpu.A);
            lda8(abs8(0xB2D0)); tay();
            lda8(dp8(0x08));
            dc_cmp8((u8)g_cpu.A, abs8(0xB2D6 + g_cpu.Y));
            if (!g_cpu._flag_Z) {
                inx(); inx(); inx(); inx(); inx();
                stx_dp(0x02);
                inc_abs(0xB2D2);
                continue;
            }
            ldx(dp_x(0x02));
            lda8(bus8(0xCC0001 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (g_cpu._flag_Z) break;
            tdc();
            lda8(abs8(0xB2D2));
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = 0;
            lda16(dc_adc16(g_cpu.A, 0x283E)); dp16_set(0x00, g_cpu.A);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            lda8(abs8(0xB2D0)); tax();
            lda8(abs8(0xB2D6 + g_cpu.X)); abs8_set(0xB2DE, (u8)g_cpu.A);
            ldx(dp_x(0x02));
            lda8(bus8(0xCC0001 + g_cpu.X)); abs8_set(0xB304, (u8)g_cpu.A);
            ldx(dp_x(0x02));
            lda8(bus8(0xCC0002 + g_cpu.X)); abs8_set(0xB2E0, (u8)g_cpu.A);
            ldx(dp_x(0x02));
            dc_set_mx(0, g_cpu.x_flag);
            lda16(bus16(0xCC0003 + g_cpu.X)); dp16_set(0x04, g_cpu.A);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            dc_C1F533_glue();
            break;
        }
        inc_abs(0xB2D0);
        lda8(abs8(0xB2D0));
        dc_cmp8((u8)g_cpu.A, 0x04);
    } while (!g_cpu._flag_C);
}
/* C1:577D  Script command: aim by the second operand (4D08) and wait $80
 * frames-worth of the move counter; advances 3 when done. */
DC_ENTRY(C1577D, RTS)
{
    sty_dp(0x82);
    iny();
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    ldy(dp_x(0x82));
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldx(abs_x(0xA2B6));
        lda8(abs8(0xA1C8 + g_cpu.X));
        if (g_cpu._flag_Z) {
            lda8(dp8(0x80)); abs8_set(0xA1C8 + g_cpu.X, (u8)g_cpu.A);
            lda8(0x02); dp8_set(0x8E, 0x02);
            dc_C14D08_glue();
            tdc();
            script_advance();
            return;
        }
        lda8(abs8(0xA1A8 + g_cpu.X));
        if (!g_cpu._flag_Z) return;
        abs8_set(0xA1C8 + g_cpu.X, 0);
        lda8(0x03); script_advance();
        return;
    }
    dc_cmp8((u8)g_cpu.A, 0x04);
    int group = g_cpu._flag_Z;
    if (group) lda8(abs8(0xA018));
    else { tax(); lda8(abs8(0xA014 + g_cpu.X)); }
    tax();
    lda8(abs8(0xA150 + g_cpu.X));
    if (g_cpu._flag_Z) {
        lda8(dp8(0x80)); abs8_set(0xA150 + g_cpu.X, (u8)g_cpu.A);
        lda8(0x01); dp8_set(0x8E, 0x01);
        dc_C14D08_glue();
        return;
    }
    if (!group) {
        lda8(abs8(0xA124 + g_cpu.X));
        if (!g_cpu._flag_Z) return;
        abs8_set(0xA150 + g_cpu.X, 0);
        lda8(0x03); script_advance();
        return;
    }
    tdc(); tay();
    for (;;) {
        lda8(abs8(0xA018 + g_cpu.Y));
        if (g_cpu._flag_N) break;
        tax();
        lda8(abs8(0xA124 + g_cpu.X));
        if (!g_cpu._flag_Z) return;
        iny();
    }
    lda8(0x03); script_advance();
}
/* C1:4E22  Script command: like 4EA2 but flagged in A145 / A1C0 (enemies
 * also start their move animation A1D8). */
DC_ENTRY(C14E22, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldx(abs_x(0xA2B6));
        lda8(abs8(0xA1C0 + g_cpu.X));
        if (g_cpu._flag_Z) {
            inc_abs(0xA1C0 + g_cpu.X);
            lda8(0x01); dp8_set(0x8E, 0x01);
            dc_C14D08_glue();
            lda8(0x01); dp8_set(0x8E, 0x01);
            ldx(abs_x(0xA2B6));
            inc_abs(0xA1D8 + g_cpu.X);
            lda8(dp8(0x8E));
            if (g_cpu._flag_Z) { lda8(0x01); script_advance(); }
            return;
        }
        lda8(abs8(0xA1A8 + g_cpu.X));
        if (!g_cpu._flag_Z) return;
        abs8_set(0xA1C0 + g_cpu.X, 0);
        abs8_set(0xA1D8 + g_cpu.X, 0);
        lda8(0x02); script_advance();
        return;
    }
    dc_cmp8((u8)g_cpu.A, 0x04);
    int group = g_cpu._flag_Z;
    if (group) lda8(abs8(0xA018));
    else { tax(); lda8(abs8(0xA014 + g_cpu.X)); }
    tax();
    lda8(abs8(0xA145 + g_cpu.X));
    if (g_cpu._flag_Z) {
        inc_abs(0xA145 + g_cpu.X);
        lda8(0x01); dp8_set(0x8E, 0x01);
        dc_C14D08_glue();
        return;
    }
    if (!group) {
        lda8(abs8(0xA124 + g_cpu.X));
        if (!g_cpu._flag_Z) return;
        abs8_set(0xA145 + g_cpu.X, 0);
        lda8(0x02); script_advance();
        return;
    }
    tdc(); tay();
    for (;;) {
        lda8(abs8(0xA018 + g_cpu.Y));
        if (g_cpu._flag_N) break;
        tax();
        lda8(abs8(0xA124 + g_cpu.X));
        if (!g_cpu._flag_Z) return;
        iny();
    }
    lda8(0x02); script_advance();
}
/* C1:379A  Enemy $9C steps toward object 9752 (sliding along the axis when
 * blocked, 9791); arrives when within the A2B5 range (2AEF / 2AFB / 2B07). */
DC_ENTRY(C1379A, RTS)
{
    ldx(dp_x(0x9C));
    abs8_set(0xA5FD + g_cpu.X, 0);
    lda8(abs8(0x9791 + g_cpu.X));
    if (!g_cpu._flag_Z) { lda8(abs8(0x9786 + g_cpu.X)); dp8_set(0xDB, (u8)g_cpu.A); }
    else {
        lda8(abs8(0x1D0F + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D26 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x9752 + g_cpu.X)); tay();
        lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0xD5, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD6, (u8)g_cpu.A);
        dc_C10222_glue();
    }
    lda8(0x08); dp8_set(0xAE, 0x08);
    ldx(dp_x(0x9C));
    lda8(dp8(0xDB)); abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
    dc_C101F9_glue();
    dp8_set(0x8C, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    dp8_set(0x8E, (u8)g_cpu.A);
    lda8(dp8(0xDB)); tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    ldx(dp_x(0x9C));
    abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x9752 + g_cpu.X)); tay();
    lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0xA039 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0xA050 + g_cpu.Y, (u8)g_cpu.A);
    lda8((u8)g_cpu.X);
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, 0x03));
    tax();
    lda8(abs8(0x1D0C + g_cpu.X)); abs8_set(0xA039 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); abs8_set(0xA050 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xA2B5));
    int arrive = 1;
    for (int k = 0; k < 3; k++) {
        lda8((u8)(g_cpu.A - 1));
        if (!g_cpu._flag_Z) continue;
        if (k == 0) dc_C12AEF_glue(); else if (k == 1) dc_C12AFB_glue(); else dc_C12B07_glue();
        lda8(abs8(0x9872));
        arrive = !g_cpu._flag_N;
        break;
    }
    if (arrive) {
        ldx(dp_x(0x9C));
        inc_abs(0x9829 + g_cpu.X);
        lda8(abs8(0x5E0D + g_cpu.X)); abs8_set(0x5E1D + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x9752 + g_cpu.X)); tay();
        lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0x9831 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0x9839 + g_cpu.X, (u8)g_cpu.A);
        abs8_set(0x98A7 + g_cpu.X, 0);
    } else {
        ldx(dp_x(0x9C));
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D26 + g_cpu.X), dp8(0x8C))); abs8_set(0xA053 + g_cpu.X, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D0F + g_cpu.X), dp8(0x8E))); abs8_set(0xA03C + g_cpu.X, (u8)g_cpu.A);
        inx(); inx(); inx();
        stx_dp(0x80);
        dc_C1285A_glue();
        abs8_set(0x9875, 0);
        lda8(abs8(0xA2B5));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (g_cpu._flag_Z) { lda8(0x01); abs8_set(0x9875, 0x01); }
        dc_C12926_glue();
        int blocked = g_cpu._flag_N;
        if (!blocked) {
            lda8(abs8(0xA2B5));
            dc_cmp8((u8)g_cpu.A, 0x03);
            if (!g_cpu._flag_Z) { dc_C128B0_glue(); blocked = g_cpu._flag_N; }
        }
        ldx(dp_x(0x9C));
        if (blocked) {
            g_cpu._flag_C = 0;
            lda8(dc_adc8(dp8(0xDB), 0x40));
            lsr_a(6);
            asl_a(6);
            dp8_set(0xDB, (u8)g_cpu.A);
            lda8(dp8(0xDB)); abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
            inc_abs(0x9791 + g_cpu.X);
            abs8_set(0x98A7 + g_cpu.X, 0);
        } else {
            abs8_set(0x9791 + g_cpu.X, 0);
            lda8(0x01); abs8_set(0x98A7 + g_cpu.X, 0x01);
        }
    }
    inx(); inx(); inx();
    stx_dp(0x80);
}

/* ---- batch 17 -------------------------------------------------------------- */
/* C1:3E33  Enemy $9C circles (zig-zag) around its starting point: angle A615
 * swings by 10 per step, radius A625 (20, halved for A2B5 = 0A); reverses
 * when blocked. */
DC_ENTRY(C13E33, RTS)
{
    ldx(dp_x(0x9C));
    lda8(abs8(0x1D0F + g_cpu.X)); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D26 + g_cpu.X)); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xA5FD + g_cpu.X));
    if (g_cpu._flag_Z) {
        inc_abs(0xA5FD + g_cpu.X);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1D26 + g_cpu.X), 0x10)); abs8_set(0xA60D + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D0F + g_cpu.X)); abs8_set(0xA605 + g_cpu.X, (u8)g_cpu.A);
        lda8(0xC0); abs8_set(0xA615 + g_cpu.X, 0xC0);
        abs8_set(0xA61D + g_cpu.X, 0);
    }
    lda8(abs8(0xA61D + g_cpu.X));
    lda8(g_cpu._flag_Z ? 0x10 : 0xF0);
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, abs8(0xA615 + g_cpu.X)));
    abs8_set(0xA615 + g_cpu.X, (u8)g_cpu.A);
    dp8_set(0xDB, (u8)g_cpu.A);
    lda8(0x20); abs8_set(0xA625, 0x20);
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x0A);
    if (g_cpu._flag_Z) {
        u8 v = abs8(0xA625); g_cpu._flag_C = v & 1; v >>= 1; abs8_set(0xA625, v); dc_set_flags_nz8(v);
    }
    lda8(abs8(0xA625)); dp8_set(0xAE, (u8)g_cpu.A);
    lda8(dp8(0xDB));
    dc_C101F9_glue();
    ldx(dp_x(0x9C));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, abs8(0xA60D + g_cpu.X))); dp8_set(0xD6, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0xA625), 0x20)); dp8_set(0xAE, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    ldx(dp_x(0x9C));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, abs8(0xA605 + g_cpu.X))); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0x1D0F + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(abs8(0x1D26 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A);
    lda8(0x08); dp8_set(0xAE, 0x08);
    dc_C10222_glue();
    ldx(dp_x(0x9C));
    abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
    dc_C101F9_glue();
    dp8_set(0x8C, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    dp8_set(0x8E, (u8)g_cpu.A);
    lda8(dp8(0xDB)); tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    ldx(dp_x(0x9C));
    abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
    ldx(dp_x(0x9C));
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D26 + g_cpu.X), dp8(0x8C))); abs8_set(0xA053 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0F + g_cpu.X), dp8(0x8E))); abs8_set(0xA03C + g_cpu.X, (u8)g_cpu.A);
    inx(); inx(); inx();
    stx_dp(0x80);
    dc_C1285A_glue();
    abs8_set(0x9875, 0);
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x0C);
    if (g_cpu._flag_Z) { lda8(0x01); abs8_set(0x9875, 0x01); }
    dc_C12926_glue();
    int blocked = g_cpu._flag_N;
    if (!blocked) {
        lda8(abs8(0xA2B5));
        dc_cmp8((u8)g_cpu.A, 0x0C);
        if (!g_cpu._flag_Z) { dc_C128B0_glue(); blocked = g_cpu._flag_N; }
    }
    if (blocked) {
        lda8(0x58); dp8_set(0x82, 0x58);
        lda8(abs8(0xA2B5));
        dc_cmp8((u8)g_cpu.A, 0x0A);
        if (g_cpu._flag_Z) { lda8(0x30); dp8_set(0x82, 0x30); }
        ldx(dp_x(0x9C));
        lda8(abs8(0xA61D + g_cpu.X));
        if (g_cpu._flag_Z) { g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0xA615 + g_cpu.X), dp8(0x82))); }
        else { g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA615 + g_cpu.X), dp8(0x82))); }
        abs8_set(0xA615 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0xA61D + g_cpu.X) ^ 0x01); abs8_set(0xA61D + g_cpu.X, (u8)g_cpu.A);
    } else {
        ldx(dp_x(0x9C));
        lda8(0x01); abs8_set(0x98A7 + g_cpu.X, 0x01);
    }
    ldx(dp_x(0x9C));
    inx(); inx(); inx();
    stx_dp(0x80);
}
/* C1:DA37  HP (bit 80 of $1A) / MP (bit 40) restore of $16 * $18 (+ random
 * 0..$16 when /50 or /10 leaves a remainder; $18 = 0F: full) with popups. */
static void restore_amount(u16 full, u16 a28_imm, u16 div)
{
    lda8(dp8(0x18));
    dc_cmp8((u8)g_cpu.A, 0x0F);
    if (g_cpu._flag_Z) {
        ldx(abs_x(0xB1F6));
        dc_set_mx(0, g_cpu.x_flag);
        lda16(abs16(full + g_cpu.X)); dp16_set(0x00, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        return;
    }
    if (a28_imm) ldx(a28_imm); else ldx(dp_x(0x16));
    stx_dp(0x28);
    ldx(dp_x(0x18)); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x00);
    ldx(dp_x(0x00)); stx_dp(0x28);
    ldx(div); stx_dp(0x2A);
    dc_C1C92A_glue();
    lda8(dp8(0x32));
    dc_cmp8((u8)g_cpu.A, 0x00);
    if (g_cpu._flag_Z) return;
    tdc(); tax();
    lda8(dp8(0x16));
    dc_C1AF22_glue();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x00))); dp16_set(0x00, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
static void restore_popup(u8 b202)
{
    ldy(g_cpu.X);
    ldx(dp_x(0x00)); stx_abs(0xAD89);
    lda8(b202); abs8_set(0xB202, b202);
    dc_C1EBF8_glue();
    dc_C1E89F_glue();
    lda8(dp8(0x0E)); tax();
    lda8(dp8(0x00)); abs8_set(0xAD9C + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x01)); abs8_set(0xAD9D + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x04)); abs8_set(0xAD9E + g_cpu.X, (u8)g_cpu.A);
}
static void restore_target(u8 kind)
{
    lda8(abs8(0xAD8D));
    lda8((u8)(g_cpu.A - 1));
    tax();
    lda8(abs8(0xAD8E + g_cpu.X)); tax();
    lda8(kind); dp8_set(0x04, kind);
}
DC_ENTRY(C1DA37, RTS)
{
    lda8(dp8(0x1A) & 0x80);
    if (!g_cpu._flag_Z) {
        restore_amount(0x5E32, 0, 0x0032);
        restore_target(0x01);
        lda8(abs8(0xB1FD));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (!g_cpu._flag_C) {
            ldy(dp_x(0x00));
            cpy(0x03E7);
            if (g_cpu._flag_C) { ldy(0x03E7); sty_dp(0x00); }
        }
        restore_popup(0x80);
        lda8(0x01); abs8_set(0xAD9B, 0x01);
    }
    inc_abs(0xB2C7);
    lda8(dp8(0x1A) & 0x40);
    if (!g_cpu._flag_Z) {
        restore_amount(0x5E36, 0x000A, 0x000A);
        restore_target(0x02);
        restore_popup(0xC0);
    }
    abs8_set(0xAD9B, 0);
    dec_abs(0xB2C7);
}
/* C1:DEF0  Physical attack power $18 adjusted by attacker status (5E4D/5E52
 * bit 40: x2.5/2, bit 01: x2; 5E4B bits: x4/20, x10/20, x15/20 ...) and the
 * level difference bonus (5E3F) capped at 255. */
static void atk_scale(u8 mul)
{
    tdc();
    lda8(dp8(0x18)); tax(); stx_dp(0x28);
    lda8(mul); tax(); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    ldx(0x0014); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C)); inx(); stx_dp(0x18);
}
DC_ENTRY(C1DEF0, RTS)
{
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E4D + g_cpu.X) | abs8(0x5E52 + g_cpu.X));
    if (g_cpu.A & 0x40) {
        ldx(dp_x(0x18)); stx_dp(0x28);
        lda8(0x05); tax(); stx_dp(0x2A);
        dc_C1C90B_glue();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(dp16(0x2C)); lsr16(1);
        dc_cmp16(g_cpu.A, 0x00FF);
        if (g_cpu._flag_C) lda16(0x00FF);
        dp16_set(0x18, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    } else if (g_cpu.A & 0x01) {
        ldx(dp_x(0x18)); stx_dp(0x28);
        lda8(0x02); tax(); stx_dp(0x2A);
        dc_C1C90B_glue();
        ldx(dp_x(0x2C)); stx_dp(0x18);
    } else {
        ldx(abs_x(0xB1F6));
        lda8(abs8(0x5E4B + g_cpu.X)); dp8_set(0x0E, (u8)g_cpu.A);
        if (g_cpu.A & 0x82) atk_scale(0x04);
        else if (dp8(0x0E) & 0x05) { lda8(dp8(0x0E)); atk_scale(0x0A); }
        else {
            lda8(dp8(0x0E));
            if (g_cpu.A & 0x20) atk_scale(0x0F);
            lda8(dp8(0x0E));
            if (g_cpu.A & 0x40) atk_scale(0x0A);
        }
    }
    ldx(abs_x(0xB1F4));
    lda8(abs8(0x5E3F + g_cpu.X));
    ldx(abs_x(0xB1F6));
    dc_cmp8((u8)g_cpu.A, abs8(0x5E3F + g_cpu.X));
    if (!g_cpu._flag_C) return;
    g_cpu._flag_C = 1;
    lda8(dc_sbc8((u8)g_cpu.A, abs8(0x5E3F + g_cpu.X)));
    tax(); stx_dp(0x28);
    ldx(0x0014); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    ldx(0x0002);
    dc_C1C92A_glue();
    lda8(dp8(0x2C));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0x18)));
    if (g_cpu._flag_C) { lda8(0xFF); dp8_set(0x18, 0xFF); }
}

/* ---- batch 18 -------------------------------------------------------------- */
/* C1:CCCB  Triple-tech menu entry (66..74) of party slot $06: the first
 * learned combo (284D bits) whose three members are present and able. */
DC_ENTRY(C1CCCB, RTS)
{
    tdc(); tax();
    stx_dp(0x08); stx_dp(0x0E); stx_dp(0x00); stx_dp(0x10); stx_dp(0x14);
    lda8(dp8(0x06)); tax();
    lda8(abs8(0xAEFF + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    tax();
    lda8(0x66); dp8_set(0x08, 0x66);
    lda8(0x75); dp8_set(0x0E, 0x75);
    lda8(dp8(0x08));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(1); dp16_set(0x02, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x06));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(1); tax();
    lda16(bus16(0xFDA84F + g_cpu.X)); dp16_set(0x04, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0x00));
    lda8(abs8(0x284D + g_cpu.X)); dp8_set(0x0C, (u8)g_cpu.A);
    for (;;) {
        lda8(dp8(0x0C) & 0x80);
        if (g_cpu._flag_Z) goto next;
        lda8(dp8(0x08));
        dc_C1CB93_glue();
        for (int k = 0; k < 3; k++) {
            if (k < 2) tdc();
            lda8(abs8(0xB2EA + k));
            dc_cmp8((u8)g_cpu.A, 0xFF);
            if (g_cpu._flag_Z) goto next;
            tax();
            lda8(abs8(0xB1BE + g_cpu.X));
            if (g_cpu._flag_N) goto next;
        }
        ldx(dp_x(0x02));
        lda8(bus8(0xCC1ACC + g_cpu.X) & 0x3F);
        ldx(dp_x(0x04)); abs8_set(0x0080 + g_cpu.X, (u8)g_cpu.A);
        ldx(dp_x(0x02));
        lda8(bus8(0xCC1ACB + g_cpu.X));
        ldx(dp_x(0x04)); abs8_set(0x007F + g_cpu.X, (u8)g_cpu.A);
        ldx(dp_x(0x04));
        lda8(dp8(0x08)); abs8_set(0x007E + g_cpu.X, (u8)g_cpu.A);
        ldx(dp_x(0x04));
        lda8(0xFF);
        abs8_set(0x0081 + g_cpu.X, 0xFF); abs8_set(0x0083 + g_cpu.X, 0xFF); abs8_set(0x0084 + g_cpu.X, 0xFF);
        tdc(); lda8(dp8(0x08)); dc_C1CB48_glue(); tay();
        tdc(); lda8(abs8(0xB2EA)); dc_C1CC3E_glue();
        tdc(); lda8(dp8(0x08)); dc_C1CB61_glue(); tay();
        tdc(); lda8(abs8(0xB2EB)); dc_C1CC3E_glue();
        tdc(); lda8(dp8(0x08)); dc_C1CB7A_glue(); tay();
        tdc(); lda8(abs8(0xB2EC)); dc_C1CC3E_glue();
        for (int k = 0; k < 2; k++) {
            lda8(abs8(k ? 0xB2EB : 0xB2EC)); tax();
            lda8(abs8(0xB1BE + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, dp8(0x06));
            if (g_cpu._flag_Z) { lda8(abs8(0xB2EA)); tax(); lda8(abs8(0xB1BE + g_cpu.X)); }
            if (!k) { asl_a(4); dp8_set(0x00, (u8)g_cpu.A); }
        }
        lda8((u8)g_cpu.A | dp8(0x00));
        ldx(dp_x(0x04)); abs8_set(0x0082 + g_cpu.X, (u8)g_cpu.A);
        return;
    next:
        dc_set_mx(0, g_cpu.x_flag);
        lda16(dp16(0x04));
        g_cpu._flag_C = 0;
        lda16(dc_adc16(g_cpu.A, 0x0007));
        dp16_set(0x02, (u16)(dp16(0x02) + 2));
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x08, (u8)(dp8(0x08) + 1));
        dp8_set(0x10, (u8)(dp8(0x10) + 1));
        lda8(dp8(0x10));
        dc_cmp8((u8)g_cpu.A, 0x01);
        if (g_cpu._flag_C) {
            ldx(dp_x(0x00)); inx(); stx_dp(0x00);
            lda8(abs8(0x284D + g_cpu.X)); dp8_set(0x0C, (u8)g_cpu.A);
            dp8_set(0x10, 0);
        }
        lda8(dp8(0x08));
        dc_cmp8((u8)g_cpu.A, dp8(0x0E));
        if (g_cpu._flag_C) return;
    }
}
/* C1:05A7  Redraws the HP (and MP) digits of party slot A6E0 (round robin)
 * in the battle status window, grayed at 1/8 HP (A10F), then the ATB gauges
 * (06F0) when 9F20 is set. */
static void hp_digits_attr(u16 at, int n)
{
    ldx(0x0029);
    lda8(abs8(0xA10F));
    if (!g_cpu._flag_Z) ldx(0x002D);
    lda8((u8)g_cpu.X);
    for (int k = 0; k < n; k++) abs8_set(at + 2 * k + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C105A7, RTS)
{
    lda8(abs8(0xA6DB));
    if (!g_cpu._flag_N) {
        lda8(abs8(0xA09A));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x9F20));
            if (!g_cpu._flag_Z) { lda8((u8)(g_cpu.A - 1)); if (g_cpu._flag_Z) goto gauges; }
        }
    }
    inc_abs(0xA6E0);
    lda8(abs8(0xA6E0));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_C) abs8_set(0xA6E0, 0);
    lda8(abs8(0xA6E0)); tax(); stx_dp(0x80);
    lda8(abs8(0x96F5 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dp16(0x80)); asl_a(1); tax();
    lda16(abs16(0x9F20));
    if (g_cpu._flag_Z) lda16(bus16(0xCCFA2F + g_cpu.X));
    else {
        lda16((u16)(g_cpu.A - 1));
        lda16(bus16((g_cpu._flag_Z ? 0xCCFA35 : 0xCCFA3B) + g_cpu.X));
    }
    tay(); sty_dp(0x86);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0xA10F, 0);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x80)); asl_a(1); tax();
    lda16(bus16(0xCCF8ED + g_cpu.X)); dp16_set(0xA2, g_cpu.A);
    tax();
    lda16(abs16(0x5E30 + g_cpu.X)); abs16_set(0x9499, g_cpu.A);
    int gray = g_cpu._flag_Z;
    if (!gray) {
        lda16(abs16(0x5E32 + g_cpu.X));
        dc_C1011B_glue();
        dc_cmp16(g_cpu.A, abs16(0x9499));
        if (g_cpu._flag_Z || g_cpu._flag_C) { lda16(abs16(0xA110)); gray = !g_cpu._flag_Z; }
    }
    if (gray) { u16 v = (u16)(abs16(0xA10F) + 1); abs16_set(0xA10F, v); dc_set_flags_nz16(v); }
    dc_C1011F_glue();
    dc_C1104E_glue();
    lda8(abs8(0x949D)); abs8_set(0x0CC0 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x949E)); abs8_set(0x0CC2 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x949F)); abs8_set(0x0CC4 + g_cpu.Y, (u8)g_cpu.A);
    hp_digits_attr(0x0CC1, 4);
    lda8(abs8(0x9F20));
    if (g_cpu._flag_Z) {
        lda8((u8)g_cpu.X);
        for (int k = 0; k < 3; k++) abs8_set(0x0CC9 + 2 * k + g_cpu.Y, (u8)g_cpu.A);
    }
    dc_set_mx(0, g_cpu.x_flag);
    ldx(dp_x(0xA2));
    lda16(abs16(0x5E34 + g_cpu.X)); abs16_set(0x9499, g_cpu.A);
    lda16(abs16(0x9F20));
    int wide = g_cpu._flag_Z;
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0x86), wide ? 0x0010 : 0x0008));
    tay();
    dc_C10174_glue();
    lda8(abs8(0x9F20));
    dc_C1104E_glue();
    u16 o = wide ? 0x0CC2 : 0x0CC0;
    lda8(abs8(0x949E)); abs8_set(o + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x949F)); abs8_set(o + 2 + g_cpu.Y, (u8)g_cpu.A);
    hp_digits_attr(o + 1, 2);
gauges:
    lda8(abs8(0x9F20));
    if (g_cpu._flag_Z) return;
    tdc(); tax(); stx_dp(0x80);
    do {
        ldx(dp_x(0x80));
        lda8(abs8(0x96F5 + g_cpu.X));
        if (!g_cpu._flag_Z) dc_C106F0_glue();
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
        lda8(dp8(0x80));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
}
/* C1:0EE1  Grays (2D) or enables (29) the three visible battle menu rows of
 * party slot 95D5 by tech MP cost (1A83/85/86 vs 5E34/5EB4/5F34), required
 * partners (103E), silence (A0D1) and the 74 / 42 special case. */
DC_ENTRY(C10EE1, RTS)
{
    lda8(abs8(0x95D5));
    dc_set_a8(asl8((u8)g_cpu.A)); tax();
    lda8(bus8(0xCCF395 + g_cpu.X)); dp8_set(0xAF, (u8)g_cpu.A);
    lda8(bus8(0xCCF396 + g_cpu.X)); dp8_set(0xB0, (u8)g_cpu.A);
    lda8(abs8(0x95D5)); tax(); tay();
    lda8(bus8(0xCCF38C + g_cpu.X)); dp8_set(0x86, (u8)g_cpu.A);
    lda8(abs8(0x95EB + g_cpu.Y));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0x86)));
    tax(); stx_dp(0x86);
    static const u16 mp[] = { 0x5E34, 0x5E35, 0x5EB4, 0x5EB5, 0x5F34, 0x5F35 };
    for (int k = 0; k < 6; k++) { lda8(abs8(mp[k])); dp8_set((u8)(0x96 + k), (u8)g_cpu.A); }
    tdc(); tax(); stx_dp(0x82);
    do {
        lda8(dp8(0x82));
        dc_set_a8(asl8((u8)g_cpu.A)); tax();
        lda8(bus8(0xCCF39B + g_cpu.X)); dp8_set(0x84, (u8)g_cpu.A);
        lda8(bus8(0xCCF39C + g_cpu.X)); dp8_set(0x85, (u8)g_cpu.A);
        lda8(0x29); dp8_set(0x94, 0x29);
        ldx(dp_x(0x86));
        lda8(abs8(0x94D0 + g_cpu.X) & 0xF0);
        dc_cmp8((u8)g_cpu.A, 0xF0);
        if (g_cpu._flag_Z) {
            lda8(abs8(0x94D0 + g_cpu.X));
            u8 v = (u8)g_cpu.A;
            dc_cmp8(v, 0xFC);
            int gray = 0;
            if (v == 0xFC) { lda8(abs8(0xA6DE)); dc_cmp8((u8)g_cpu.A, 0x02); gray = !g_cpu._flag_C; }
            else {
                dc_cmp8(v, 0xFD);
                if (v == 0xFD) { lda8(abs8(0xA6DE)); dc_cmp8((u8)g_cpu.A, 0x03); gray = !g_cpu._flag_C; }
            }
            if (gray) { lda8(0x2D); dp8_set(0x94, 0x2D); }
            goto draw;
        }
        lda8(abs8(0x9551 + g_cpu.X)); tax();
        lda8(bus8(0xCCF3A1 + g_cpu.X));
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, dp8(0xAF))); dp8_set(0x88, (u8)g_cpu.A);
        lda8(dc_adc8(dp8(0xB0), 0x00)); dp8_set(0x89, (u8)g_cpu.A);
        ldx(dp_x(0x88));
        {
            static const u8 req[] = { 0x03, 0x05, 0x06 };
            for (int k = 0; k < 3; k++) {
                lda8(abs8(0x1A80 + req[k] + g_cpu.X));
                if (g_cpu._flag_N) continue;
                g_cpu._flag_C = 1;
                dc_sbc8(dp8((u8)(0x96 + 2 * k)), abs8(0x1A80 + req[k] + g_cpu.X));
                lda8(dc_sbc8(dp8((u8)(0x97 + 2 * k)), 0x00));
                if (!g_cpu._flag_C) goto fail;
            }
        }
        lda8(abs8(0x1A84 + g_cpu.X)); dp8_set(0x81, (u8)g_cpu.A);
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            lda8((u8)g_cpu.A & 0x0F); dp8_set(0x80, (u8)g_cpu.A);
            dc_C1103E_glue();
            if (!g_cpu._flag_Z) goto fail;
            lda8(dp8(0x81) & 0xF0);
            dc_cmp8((u8)g_cpu.A, 0xF0);
            if (!g_cpu._flag_Z) {
                dc_C1011A_glue();
                dp8_set(0x80, (u8)g_cpu.A);
                dc_C1103E_glue();
                if (!g_cpu._flag_Z) goto fail;
            }
        }
        lda8(abs8(0x95D5)); tax();
        lda8(abs8(0xA0D1 + g_cpu.X));
        if (!g_cpu._flag_Z) goto fail;
        ldx(dp_x(0x88));
        lda8(abs8(0x1A80 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0x74);
        if (g_cpu._flag_Z) {
            ldx(abs_x(0xA0FF));
            lda8(abs8(0x1C48 + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, 0x42);
            if (!g_cpu._flag_Z) goto fail;
        }
        ldx(dp_x(0x88));
        lda8(abs8(0x1A82 + g_cpu.X) & 0x7F); abs8_set(0x1A82 + g_cpu.X, (u8)g_cpu.A);
        lda8(0x29); dp8_set(0x94, 0x29);
        goto draw;
    fail:
        ldx(dp_x(0x88));
        lda8(abs8(0x1A82 + g_cpu.X) | 0x80); abs8_set(0x1A82 + g_cpu.X, (u8)g_cpu.A);
        lda8(0x2D); dp8_set(0x94, 0x2D);
    draw:
        for (int k = 0; k < 2; k++) {
            ldx(dp_x(0x84));
            ldy(0x0012);
            lda8(dp8(0x94));
            do {
                abs8_set((k ? 0xA722 : 0xA6E2) + g_cpu.X, (u8)g_cpu.A);
                inx(); inx();
                ldy((u16)(g_cpu.Y - 1));
            } while (!g_cpu._flag_Z);
        }
        dp8_set(0x86, (u8)(dp8(0x86) + 1));
        dp8_set(0x82, (u8)(dp8(0x82) + 1));
        lda8(dp8(0x82));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
    lda8(0x01); abs8_set(0xA099, 0x01);
}

/* ---- batch 19 -------------------------------------------------------------- */
/* C1:17A1 / 17B3 / 17BF / 17D1  Item list page up / redraw / page down
 * (95E6 = first row, up to FA), then 991F/9920 = FF. */
static void item_page_done(void)
{
    dc_C1095D_glue();
    lda8(0xFF); abs8_set(0x991F, 0xFF); abs8_set(0x9920, 0xFF);
}
DC_ENTRY(C117B3, RTS) { item_page_done(); }
DC_ENTRY(C117D1, RTS) { item_page_done(); }
DC_ENTRY(C117BF, RTS)
{
    lda8(abs8(0x95E6));
    dc_cmp8((u8)g_cpu.A, 0xFA);
    if (!g_cpu._flag_C) {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x95E6), 0x01));
        abs8_set(0x95E6, (u8)g_cpu.A);
        dp8_set(0x80, (u8)g_cpu.A);
        dc_C1095D_glue();
    }
    lda8(0xFF); abs8_set(0x991F, 0xFF); abs8_set(0x9920, 0xFF);
}
DC_ENTRY(C117A1, RTS)
{
    lda8(abs8(0x95E6));
    if (!g_cpu._flag_Z) {
        g_cpu._flag_C = 1;
        lda8(dc_sbc8(abs8(0x95E6), 0x01));
        abs8_set(0x95E6, (u8)g_cpu.A);
    }
    dp8_set(0x80, (u8)g_cpu.A);
    item_page_done();
}
/* C1:D221 / D23E  Applies an item / tech effect to each of the AD8D targets
 * (E9B8 picks the next; D132 + DA37 restore HP/MP, D14F + DB5E statuses). */
DC_ENTRY(C1D221, RTS)
{
    dc_C1E9A3_glue();
    do {
        dc_C1E9B8_glue();
        dc_C1D132_glue();
        dc_C1DA37_glue();
        dc_C1D508_glue();
        dec_abs(0xAD8D);
        lda8(abs8(0xAD8D));
    } while (!g_cpu._flag_Z);
    lda8(0x00); abs8_set(0xB200, 0);
}
DC_ENTRY(C1D23E, RTS)
{
    dc_C1E9A3_glue();
    do {
        dc_C1E9B8_glue();
        dc_C1D14F_glue();
        dc_C1DB5E_glue();
        lda8(dp8(0x16));
        if (g_cpu.A & 0x40) { dc_C1D132_glue(); dc_C1DA37_glue(); }
        dc_C1D508_glue();
        dec_abs(0xAD8D);
        lda8(abs8(0xAD8D));
    } while (!g_cpu._flag_Z);
    lda8(0x00); abs8_set(0xB200, 0);
}
/* C1:E976  With chance $16 in 100: applies status group 1 (DBAA) and, when
 * the user has a 5E73 follow-up, queues it in B29A[B1FD]. */
DC_ENTRY(C1E976, RTS)
{
    tdc(); tax();
    lda8(0x64);
    dc_C1AF22_glue();
    dp8_set(0x0E, (u8)g_cpu.A);
    lda8(dp8(0x16));
    dc_cmp8((u8)g_cpu.A, dp8(0x0E));
    if (!g_cpu._flag_C) return;
    lda8(0x01); dp8_set(0x16, 0x01);
    dc_C1DBAA_glue();
    ldx(abs_x(0xB1F4));
    lda8(abs8(0x5E73 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    dp8_set(0x0A, (u8)g_cpu.A);
    tdc();
    lda8(abs8(0xB1FD)); tax();
    lda8(dp8(0x0A)); abs8_set(0xB29A + g_cpu.X, (u8)g_cpu.A);
}

/* ---- batch 20 -------------------------------------------------------------- */
/* Enemy A2B6 (X on entry) starts walking to A2B0/A2B2: angle and 8.8 steps
 * (0222/65BA), step count ABF6 at speed A32F, facing A230, goal A38E/A3A1,
 * start A368/A37B, step accumulators A89E.. (index = enemy*2 in X). */
static void enemy_walk_setup(void)
{
    lda8((u8)g_cpu.X);
    dc_set_a8(asl8((u8)g_cpu.A)); tax();
    lda8(abs8(0xA240 + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(abs8(0xA250 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A);
    lda8(abs8(0xA2B0)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); dp8_set(0xD6, (u8)g_cpu.A);
    dc_C10222_glue();
    dc_C165BA_glue();
    ldy(abs_x(0xA2B6));
    walk_steps(0xA32F, 0xABF6);
    ldy(abs_x(0xA2B6));
    lda8(dp8(0xDB));
    abs8_set(0xA355 + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA5B5 + g_cpu.Y, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set(0xA230 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B0)); abs8_set(0xA38E + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); abs8_set(0xA3A1 + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA2F6 + g_cpu.Y, 0x01);
    tdc();
    abs8_set(0xA878 + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA31C + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA1E0 + g_cpu.Y, (u8)g_cpu.A);
    lda8((u8)g_cpu.Y);
    dc_set_a8(asl8((u8)g_cpu.A)); tax();
    lda8(abs8(0xA240 + g_cpu.X)); abs8_set(0xA368 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA250 + g_cpu.X)); abs8_set(0xA37B + g_cpu.Y, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xA880)); abs16_set(0xA89E + g_cpu.X, g_cpu.A);
    lda16(abs16(0xA882)); abs16_set(0xA8C4 + g_cpu.X, g_cpu.A);
    tdc();
    abs16_set(0xA8EA + g_cpu.X, g_cpu.A);
    abs16_set(0xA910 + g_cpu.X, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:4FBF / 4FD1  Script command: walk the actor(s) to point A2B0/A2B2
 * (operands, or 5D9E/5D9F); advances 0 while walking, A3A9[A010] = $8E
 * (the command length) once arrived. */
static void walk_to_point(void)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax();
            lda8(abs8(0xA014 + g_cpu.X)); tax();
            lda8(abs8(0xA337 + g_cpu.X));
            if (g_cpu._flag_Z) {
                dc_C15135_glue();
                ldx(abs_x(0xA010));
                lda8(dp8(0x8E)); abs8_set(0xA3A9 + g_cpu.X, (u8)g_cpu.A);
                goto adv0;
            }
            lda8(abs8(0xA19D + g_cpu.X));
            if (g_cpu._flag_Z) goto adv0;
            abs8_set(0xA337 + g_cpu.X, 0);
            goto done;
        }
        tdc(); tax(); stx_dp(0x80);
        for (;;) {
            ldx(dp_x(0x80));
            lda8(abs8(0xA018 + g_cpu.X));
            if (g_cpu._flag_N) break;
            tax();
            dc_C15135_glue();
            dp8_set(0x80, (u8)(dp8(0x80) + 1));
        }
        ldx(abs_x(0xA010));
        lda8(dp8(0x8E)); abs8_set(0xA3A9 + g_cpu.X, (u8)g_cpu.A);
        goto done;
    }
    ldx(abs_x(0xA2B6));
    lda8(abs8(0xA342 + g_cpu.X));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xA1E0 + g_cpu.X));
        if (g_cpu._flag_Z) goto adv0;
        abs8_set(0xA342 + g_cpu.X, 0);
        goto done;
    }
    enemy_walk_setup();
    ldx(abs_x(0xA010));
    lda8(dp8(0x8E)); abs8_set(0xA3A9 + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA342 + g_cpu.Y, 0x01);
adv0:
    lda8(0x00); script_advance();
    return;
done:
    ldx(abs_x(0xA010));
    lda8(abs8(0xA3A9 + g_cpu.X));
    script_advance();
}
DC_ENTRY(C14FBF, RTS)
{
    lda8(script_byte()); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(script_byte()); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x03); dp8_set(0x8E, 0x03);
    walk_to_point();
}
DC_ENTRY(C14FD1, RTS)
{
    lda8(abs8(0x5D9E)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x5D9F)); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x01); dp8_set(0x8E, 0x01);
    walk_to_point();
}
/* C1:D267  Applies a status attack to each of the AD8D targets: hit chance
 * = attacker 5E66 / evasion-ish $1C + $1A vs a 0..99 roll (always when
 * AE4D bit 20), then DBAA; B200 = 80 when it missed / was blocked. */
DC_ENTRY(C1D267, RTS)
{
    dc_C1E9A3_glue();
    do {
        dc_C1E9B8_glue();
        lda8(abs8(0xAE4D));
        int roll = !(g_cpu.A & 0x20);
        int miss = 0;
        if (roll) {
            dc_C1D1A4_glue();
            ldx(dp_x(0x16)); stx_dp(0x1A);
            ldx(dp_x(0x18)); stx_dp(0x1C);
            tdc();
            ldx(abs_x(0xB1F4));
            lda8(abs8(0x5E66 + g_cpu.X));
            tax(); stx_abs(0x0028);
            lda8(dp8(0x1C));
            tax(); abs8_set(0x002A, (u8)g_cpu.A);
            dc_C1C92A_glue();
            lda8(dp8(0x2C));
            g_cpu._flag_C = 0;
            lda8(dc_adc8((u8)g_cpu.A, dp8(0x1A)));
            dp8_set(0x16, (u8)g_cpu.A);
            tdc(); tax();
            lda8(0x64);
            dc_C1AF22_glue();
            dc_cmp8((u8)g_cpu.A, dp8(0x16));
            miss = !g_cpu._flag_Z && g_cpu._flag_C;
        }
        if (!miss) {
            dc_C1D14F_glue();
            dc_C1DBAA_glue();
            lda8(dp8(0x16));
            dc_cmp8((u8)g_cpu.A, 0x80);
            miss = g_cpu._flag_Z;
            if (!miss) { lda8(0x00); abs8_set(0xB200, 0); }
        }
        if (miss) {
            dc_C1D1B5_glue();
            dc_C1E8E2_glue();
            lda8(0x80); abs8_set(0xB200, 0x80);
        }
        dc_C1D508_glue();
        dec_abs(0xAD8D);
        lda8(abs8(0xAD8D));
    } while (!g_cpu._flag_Z);
}

/* C1:3234  Per-frame animation for the three battle slots of group A (1: party
 * 0-2, 2: enemies 3-5, 3: 6-7): picks the slot order 992D..992F (the first
 * pending 9925.. request rotates it), then for each slot reloads its 8 frame
 * ids/delays from the pose tables when pose 5E0D/96DE changed, and counts the
 * frame delay 9694 down to the next frame (967B), handing the frame to 1C4A.
 * Finally steps $A4 towards 993B (below 3). */
static const u8 c13234_rot[3][4][3] = {
    { { 0, 1, 2 }, { 1, 2, 0 }, { 2, 0, 1 }, { 0, 1, 2 } },
    { { 3, 4, 5 }, { 4, 5, 3 }, { 5, 3, 4 }, { 3, 4, 5 } },
    { { 6, 7, 0xFF }, { 7, 6, 0xFF }, { 6, 7, 0xFF }, { 6, 7, 0xFF } },
};
static int c13234_next_frame(void)                /* Y = $9C*8 + 969C; wraps to 0 on a 0 delay */
{
    lda8(dp8(0x9C)); dc_C10112_glue(); dp8_set(0x80, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, abs8(0x969C + g_cpu.X))); tay();
    lda8(abs8(0x9799 + g_cpu.Y));
    if (!g_cpu._flag_Z) return 0;
    abs8_set(0x969C + g_cpu.X, 0); lda8(dp8(0x80)); tay();
    return 1;
}
DC_ENTRY(C13234, RTS)
{
    abs8_set(0x9841, 0);
    int grp = -1;
    for (int k = 0; k < 3; k++) {
        lda8((u8)(g_cpu.A - 1));
        if (g_cpu._flag_Z) { grp = k; break; }
    }
    if (grp < 0) return;
    dp8_set(0xA4, (u8)(dp8(0xA4) - 1));
    static const u16 req[3] = { 0x9925, 0x9928, 0x992B };
    int nreq = grp == 2 ? 2 : 3, which = 3;
    for (int k = 0; k < nreq; k++)
        if (abs8(req[grp] + k)) { abs8_set(req[grp] + k, 0); which = k; break; }
    const u8 *o = c13234_rot[grp][which];
    for (int k = 0; k < 3; k++) abs8_set(0x992D + k, o[k]);
    dc_set_a8(o[2]);
    if (grp == 0) g_cpu.A &= 0x00FF;                 /* every group-1 path went through TDC */
    do {
        lda8(abs8(0x9841)); tax();
        lda8(abs8(0x992D + g_cpu.X));
        if (g_cpu._flag_N) goto next;
        tax(); stx_dp(0x9C);
        lda8(abs8(0x96F8 + g_cpu.X));
        if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x974A + g_cpu.X)); dc_cmp8((u8)g_cpu.A, abs8(0x5E0D + g_cpu.X));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x96DE + g_cpu.X)); dc_cmp8((u8)g_cpu.A, abs8(0x96E9 + g_cpu.X));
        }
        if (!g_cpu._flag_Z) {
            lda8(abs8(0x5E0D + g_cpu.X)); abs8_set(0x974A + g_cpu.X, (u8)g_cpu.A);
            lda8(abs8(0x96DE + g_cpu.X)); abs8_set(0x96E9 + g_cpu.X, (u8)g_cpu.A);
            lda8((u8)g_cpu.X);
            dc_set_a8(asl8((u8)g_cpu.A));
            g_cpu._flag_C = 0;
            lda8(dc_adc8((u8)g_cpu.A, dp8(0x9C))); tax();
            for (int k = 0; k < 3; k++) {
                lda8(abs8(0x1CC3 + k + g_cpu.X)); dp8_set(0xC3 + k, (u8)g_cpu.A);
            }
            for (int k = 0; k < 3; k++) {
                lda8(abs8(0x1CA2 + k + g_cpu.X)); dp8_set(0xC0 + k, (u8)g_cpu.A);
            }
            ldx(dp_x(0x9C));
            lda8(abs8(0x96DE + g_cpu.X)); dp8_set(0xA5, (u8)g_cpu.A);
            lda8(abs8(0x9671 + g_cpu.X)); dp8_set(0xA7, (u8)g_cpu.A);
            lda8(abs8(0xA2CB + g_cpu.X)); dp8_set(0xA8, (u8)g_cpu.A);
            dc_C100A7_glue();
            ldx(dp_x(0x9C));
            lda8(abs8(0x974A + g_cpu.X)); tax();
            lda8(bus8(0xCCFB91 + g_cpu.X));
            if (g_cpu._flag_Z) lda8(0x0C);
            else if ((u8)(g_cpu.A - 1) == 0) {
                ldx(dp_x(0x9C)); abs8_set(0x9829 + g_cpu.X, 0); lda8(0x04);
            } else lda8(0x18);
            dp8_set(0x86, (u8)g_cpu.A);
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = 0;
            lda16(dc_adc16(g_cpu.A, dp16(0xA9))); dp16_set(0x82, g_cpu.A);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            lda8(0x08); dp8_set(0x80, 0x08);
            ldx(dp_x(0x9C));
            lda8(bus8(0xCCF82F + g_cpu.X)); tax();
            phx();
            ldy(dp_x(0x82));
            do {                                        /* frame ids */
                lda8(bus8(lp(0xC0) + g_cpu.Y)); abs8_set(0x97D9 + g_cpu.X, (u8)g_cpu.A);
                iny(); inx();
                dp8_set(0x80, (u8)(dp8(0x80) - 1));
            } while (dp8(0x80));
            lda8(0x08); dp8_set(0x80, 0x08);
            plx(); stx_dp(0x84);
            lda8(dp8(0x86)); tay();
            do {                                        /* frame delays */
                lda8(bus8(lp(0xC3) + g_cpu.Y)); tax();
                lda8(bus8(0xCCFBCE + g_cpu.X));
                ldx(dp_x(0x84));
                abs8_set(0x9799 + g_cpu.X, (u8)g_cpu.A);
                iny();
                dp8_set(0x84, (u8)(dp8(0x84) + 1));
                dp8_set(0x80, (u8)(dp8(0x80) - 1));
            } while (dp8(0x80));
        }
        ldx(dp_x(0x9C));
        dec_abs(0x9694 + g_cpu.X);
        if (!g_cpu._flag_Z) goto next;
        c13234_next_frame();
        lda8(abs8(0x97D9 + g_cpu.Y)); abs8_set(0x967B, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dp8(0x9C));
        lda8(dc_adc8((u8)g_cpu.A, 0x03)); abs8_set(0x9679, (u8)g_cpu.A);
        tax();
        lda8(abs8(0x9912));
        int hold = 0;
        if (!g_cpu._flag_Z) {
            lda8((u8)(g_cpu.A - 1));
            if (!g_cpu._flag_Z) hold = 1;
            else {
                lda8(abs8(0x967C + g_cpu.X));
                if (!g_cpu._flag_Z) hold = 1;
                else {
                    lda8(abs8(0x9679));
                    dc_cmp8((u8)g_cpu.A, 0x04);
                    if (!g_cpu._flag_Z) { dc_cmp8((u8)g_cpu.A, 0x06); hold = !g_cpu._flag_C; }
                }
            }
        }
        if (hold) {
            inc_abs(0x9922 + g_cpu.X);
            ldx(dp_x(0x9C));
            inc_abs(0x9694 + g_cpu.X);
            goto next;
        }
        lda8(abs8(0x967C + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (!g_cpu._flag_C) dc_jsr(0xC11C4A, 0xC1345D);
        ldx(dp_x(0x9C));
        inc_abs(0x969C + g_cpu.X);
        lda8(abs8(0x969C + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (g_cpu._flag_Z) abs8_set(0x969C + g_cpu.X, 0);
        if (c13234_next_frame()) lda8(abs8(0x9799 + g_cpu.Y));
        abs8_set(0x9694 + g_cpu.X, (u8)g_cpu.A);
    next:
        inc_abs(0x9841);
        lda8(abs8(0x9841));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
    lda8(dp8(0xA4));
    dc_cmp8((u8)g_cpu.A, abs8(0x993B));
    if (g_cpu._flag_Z) return;
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_C) return;
    u8 a4 = (u8)(dp8(0xA4) + 1); dp8_set(0xA4, a4); dc_set_flags_nz8(a4);
}

/* C1:4A71  Shortest path on the 16x16 battle grid 7CFD (cells: 00 free, FF
 * the goal, C0+ blocked): floods outward from the start cell $80/$81
 * (marked 20) with step numbers $94 = 1, 2, .. using the frontier lists
 * 7DFD / 7EFD; once a step reaches the goal ($92 != 0) walks back from the
 * goal ($82/$83) marking the path cells E0|step. $92 = 80: no path within
 * 1E steps. Runs with 8-bit X/Y and returns with 16-bit X/Y. */
static const u8 grid_dir[4] = { 0xF0, 0x10, 0xFF, 0x01 };    /* up, down, left, right */
static u8 grid_cell(u8 o, u8 d)                  /* CLC; LDA o; ADC d -> X */
{
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(o), d));
    return (u8)g_cpu.A;
}
DC_ENTRY(C14A71, RTS)
{
    tdc(); tax(); tay(); inx();
    stx_dp(0x94);
    dc_set_mx(1, 1);
    lda8(dp8(0x81) & 0xF0); dp8_set(0x92, (u8)g_cpu.A);
    lda8(dp8(0x80)); dc_C1011A_glue();
    lda8((u8)g_cpu.A | dp8(0x92)); tax();
    lda8(0x20); abs8_set(0x7CFD + g_cpu.X, 0x20);
    stx_abs(0x7DFD); abs8_set(0x7DFE, 0);
    stx_dp(0x8E); stx_abs(0xA513);
    lda8(dp8(0x83) & 0xF0); dp8_set(0x92, (u8)g_cpu.A);
    lda8(dp8(0x82)); dc_C1011A_glue();
    lda8((u8)g_cpu.A | dp8(0x92)); tax();
    cpx(abs8(0xA513));
    if (g_cpu._flag_Z) { sub_a(0x10); tax(); }
    lda8(0xFF); abs8_set(0x7CFD + g_cpu.X, 0xFF);
    stx_dp(0x90); stx_abs(0xA515);
    dp8_set(0x92, 0);
    for (;;) {                                  /* one flood step */
        dp8_set(0x84, 0);
        dp8_set(0x80, 0);
        for (;;) {
            ldy(dp8(0x80));
            lda8(abs8(0x7DFD + g_cpu.Y));
            if (g_cpu._flag_Z) break;
            dp8_set(0x82, (u8)g_cpu.A);
            for (int k = 0; k < 4; k++) {
                ldx(grid_cell(0x82, grid_dir[k]));
                lda8(abs8(0x7CFD + g_cpu.X));
                if (g_cpu._flag_Z) {
                    lda8(dp8(0x94)); abs8_set(0x7CFD + g_cpu.X, (u8)g_cpu.A);
                    lda8((u8)g_cpu.X);
                    ldx(dp8(0x84));
                    abs8_set(0x7EFD + g_cpu.X, (u8)g_cpu.A);
                    dp8_set(0x84, (u8)(dp8(0x84) + 1));
                } else {
                    dc_cmp8((u8)g_cpu.A, 0xFF);
                    if (g_cpu._flag_Z) dp8_set(0x92, (u8)(dp8(0x92) + 1));
                }
            }
            dp8_set(0x80, (u8)(dp8(0x80) + 1));
        }
        lda8(dp8(0x92));
        if (!g_cpu._flag_Z) break;
        ldx(dp8(0x84));
        abs8_set(0x7EFD + g_cpu.X, 0);
        tdc(); tax();
        for (;;) {                              /* next frontier */
            lda8(abs8(0x7EFD + g_cpu.X));
            if (g_cpu._flag_Z) break;
            abs8_set(0x7DFD + g_cpu.X, (u8)g_cpu.A);
            inx();
        }
        abs8_set(0x7DFD + g_cpu.X, 0);
        dp8_set(0x94, (u8)(dp8(0x94) + 1));
        lda8(dp8(0x94));
        dc_cmp8((u8)g_cpu.A, 0x1F);
        if (g_cpu._flag_Z) {
            lda8(0x80); dp8_set(0x92, 0x80);
            dc_set_mx(1, 0);
            return;
        }
    }
    ldx(dp8(0x8E));
    abs8_set(0x7CFD + g_cpu.X, 0);
    dp8_set(0x94, (u8)(dp8(0x94) - 1));
    ldy(dp8(0x90));
    do {                                        /* trace back from the goal */
        for (int k = 0; k < 4; k++) {
            ldy(grid_cell(0x90, grid_dir[k]));
            lda8(abs8(0x7CFD + g_cpu.Y));
            if (k == 3) break;
            dc_cmp8((u8)g_cpu.A, dp8(0x94));
            if (g_cpu._flag_Z) break;
        }
        lda8((u8)g_cpu.A | 0xE0); abs8_set(0x7CFD + g_cpu.Y, (u8)g_cpu.A);
        dp8_set(0x90, (u8)g_cpu.Y);
        u8 s = (u8)(dp8(0x94) - 1); dp8_set(0x94, s); dc_set_flags_nz8(s);
    } while (!g_cpu._flag_N);
    dc_set_mx(1, 0);
}

/* C1:BDB3  Restarts the ATB gauges of the party slots B2EA..B2EC (FF ends):
 * first clears the first slot's menu record (FDA849), then for each slot
 * AFAB = base speed B158 (BD6F adjusts it), scaled up by the battle-speed
 * table CC2BDC / CC2BEB nibble: AFAB += AFAB * n / 10 (FF cap), copied to
 * 9F22 (and 99DD); a pending B03A turn is reset, clearing 93EE. */
static void atb_restart(u16 list, u32 tab, int hi_nibble, int set_2a, int both)
{
    lda8(abs8(list)); tax();
    lda8(abs8(0xB1BE + g_cpu.X)); tax();
    lda8(0x01); abs8_set(0xB188 + g_cpu.X, 0x01);
    abs8_set(0xB188 + g_cpu.X, 0);
    lda8(abs8(0xB158 + g_cpu.X)); abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A);
    dc_C1BD6F_glue();
    ldy(g_cpu.X);
    tdc();
    lda8(abs8(0xB18C)); tax();
    lda8(bus8(tab + g_cpu.X));
    if (hi_nibble) lsr_a(4);
    lda8((u8)g_cpu.A & 0x0F); tax();
    lda8(abs8(0xAFAB + g_cpu.Y));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) {
        tdc();
        stx_dp(0x28);
        lda8(abs8(0xAFAB + g_cpu.Y)); tax(); stx_dp(0x2A);
        dc_C1C90B_glue();
        ldx(dp_x(0x2C)); stx_dp(0x28);
        ldx(0x000A);
        if (set_2a) stx_dp(0x2A);
        dc_C1C92A_glue();
        lda8(dp8(0x2C));
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, abs8(0xAFAB + g_cpu.Y)));
        if (g_cpu._flag_C) lda8(0xFF);
        abs8_set(0xAFAB + g_cpu.Y, (u8)g_cpu.A);
    }
    if (both) abs8_set(0x99DD + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0x9F22 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xB03A + g_cpu.Y));
    if (g_cpu._flag_Z) return;
    lda8(0x01); abs8_set(0xB03A + g_cpu.Y, 0x01);
    tdc(); abs8_set(0xB188 + g_cpu.Y, (u8)g_cpu.A);
    sty_dp(0x2A);
    ldy(0x0007); sty_dp(0x28);
    dc_C1C90B_glue();
    ldy(dp_x(0x2C));
    tdc(); abs8_set(0x93EE + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C1BDB3, RTS)
{
    tdc();
    lda8(abs8(0xB2EA)); tax();
    lda8(abs8(0xB1BE + g_cpu.X));
    dc_set_a8(asl8((u8)g_cpu.A)); tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xFDA849 + g_cpu.X)); tax();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    static const u8 clr[5] = { 0, 1, 2, 3, 6 };
    for (int k = 0; k < 5; k++) abs8_set(g_cpu.X + clr[k], 0);
    atb_restart(0xB2EA, 0xCC2BDC, 1, 0, 0);
    for (int k = 1; k < 3; k++) {
        lda8(abs8(0xB2EA + k));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) return;
        atb_restart(0xB2EA + k, k == 1 ? 0xCC2BDC : 0xCC2BEB, 0, 1, 1);
    }
}

/* C1:F623  Recomputes a character's stats from its level: unless B28B, for
 * the record at B285 (character id +00, level +12) and the 7-byte growth row
 * CC:25FA + id*7, each stat = base + row byte * (level-1) / 100; all seven
 * capped at 99, stat +0D at 50. */
DC_ENTRY(C1F623, RTS)
{
    static const u8 row[6] = { 0, 1, 3, 4, 5, 6 };
    static const u8 base[6] = { 0x2F, 0x30, 0x32, 0x33, 0x34, 0x35 };
    static const u8 dst[6] = { 0x0B, 0x0C, 0x0F, 0x10, 0x0E, 0x11 };
    dc_C1F7B9_glue();
    lda8(abs8(0xB28B));
    dc_cmp8((u8)g_cpu.A, 0x00);
    if (!g_cpu._flag_Z) return;
    dc_C1F871_glue();
    tdc();
    ldx(abs_x(0xB285));
    lda8(abs8(g_cpu.X)); tax(); stx_dp(0x28);
    ldx(0x0007); stx_dp(0x2A);
    dc_C1C90B_glue();
    lda8(dp8(0x2C));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, 0x25FA)); abs16_set(0xB294, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    for (int k = 0; k < 6; k++) {
        ldx(abs_x(0xB294));
        lda8(bus8(0xCC0000 + row[k] + g_cpu.X)); tax(); stx_dp(0x28);
        ldx(abs_x(0xB285));
        lda8((u8)(abs8(0x0012 + g_cpu.X) - 1)); tax(); stx_dp(0x2A);
        dc_C1C90B_glue();
        ldx(dp_x(0x2C)); stx_dp(0x28);
        ldx(0x0064); stx_dp(0x2A);
        dc_C1C92A_glue();
        ldx(abs_x(0xB285));
        lda8(abs8(base[k] + g_cpu.X)); tax(); stx_dp(0x28);
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, dp8(0x2C)));
        ldx(abs_x(0xB285));
        abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A);
    }
    tdc(); tay();
    ldx(abs_x(0xB285));
    do {
        lda8(abs8(0x000B + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0x63);
        if (g_cpu._flag_C) { lda8(0x63); abs8_set(0x000B + g_cpu.X, 0x63); }
        inx(); iny();
        cpy(0x0007);
    } while (!g_cpu._flag_C);
    lda8(0x32);
    ldx(abs_x(0xB285));
    dc_cmp8((u8)g_cpu.A, abs8(0x000D + g_cpu.X));
    if (!g_cpu._flag_C) abs8_set(0x000D + g_cpu.X, 0x32);
}

/* Heading $DB from target 9752 (D3/D4) to the spot beside it at distance
 * $AE (0222 angle), the spot kept in A5E4/A5EC; flipped by 80 when it
 * points away from the enemy's side. Shared by 3A3D and 3BC2. */
static void spot_heading(void)
{
    dc_C10222_glue();
    dc_C101F9_glue();
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0xD4)));
    ldx(dp_x(0x9C));
    dp8_set(0x82, (u8)g_cpu.A); abs8_set(0xA5EC + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0xD3)));
    ldx(dp_x(0x9C));
    dp8_set(0x80, (u8)g_cpu.A); abs8_set(0xA5E4 + g_cpu.X, (u8)g_cpu.A);
    ldx(dp_x(0x9C));
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(abs8(0x1D0F + g_cpu.X), dp8(0x80)));
    tdc();
    lda8(dc_sbc8((u8)g_cpu.A, 0x00)); dp8_set(0x81, (u8)g_cpu.A);   /* FF: target is to the right */
    lda8(dc_sbc8(abs8(0x1D26 + g_cpu.X), dp8(0x82)));
    tdc();
    lda8(dc_sbc8((u8)g_cpu.A, 0x00)); dp8_set(0x83, (u8)g_cpu.A);
    lda8(dp8(0xDB)); dc_C10118_glue();
    dp8_set(0x84, (u8)g_cpu.A);
    int flip;
    u8 q = dp8(0x84), r81 = dp8(0x81), r83 = dp8(0x83);
    if (!(q & 2)) {
        if (q == 0) flip = r81 == 0 || r83 == 0;
        else flip = (r81 & 0x80) || r83 == 0;
    } else if (!(q & 1)) flip = (r81 & 0x80) || (r83 & 0x80);
    else flip = r81 == 0 || r83 != 0;
    if (flip) {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0xDB), 0x80)); dp8_set(0xDB, (u8)g_cpu.A);
    }
}
/* $DB -> 9786, step 8 along it ($8C/$8E), facing 96DE. */
static void step_heading(void)
{
    lda8(0x08); dp8_set(0xAE, 0x08);
    ldx(dp_x(0x9C));
    lda8(dp8(0xDB)); abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
    dc_C101F9_glue();
    dp8_set(0x8C, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xDB), 0x40));
    dc_C101F9_glue();
    dp8_set(0x8E, (u8)g_cpu.A);
    lda8(dp8(0xDB)); tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    ldx(dp_x(0x9C));
    abs8_set(0x96DE + g_cpu.X, (u8)g_cpu.A);
}
/* C1:3A3D  One step of enemy $9C backing away: unless already turned (9791,
 * keeps heading 9786), heads from target 9752's spot towards its own (40,
 * or 80 for formation A2B5 = 10, along heading $DB from 0222), flipping the
 * heading when it points the wrong way; then steps 8 along it. A blocked
 * step (2B13 / 285A..28B0) snaps the heading to the next quarter turn. */
DC_ENTRY(C13A3D, RTS)
{
    ldx(dp_x(0x9C));
    abs8_set(0xA5FD + g_cpu.X, 0);
    lda8(abs8(0x9791 + g_cpu.X));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0x9786 + g_cpu.X)); dp8_set(0xDB, (u8)g_cpu.A);
        goto step;
    }
    lda8(abs8(0x9752 + g_cpu.X)); tay();
    lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD4, (u8)g_cpu.A);
    lda8(abs8(0x1D0F + g_cpu.X)); dp8_set(0xD5, (u8)g_cpu.A); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D26 + g_cpu.X)); dp8_set(0xD6, (u8)g_cpu.A); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
    lda8(0x40); dp8_set(0xAE, 0x40);
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x10);
    if (g_cpu._flag_Z) { lda8(0x80); dp8_set(0xAE, 0x80); }
    spot_heading();
step:
    step_heading();
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x10);
    int blocked = 1;
    if (!g_cpu._flag_Z) {
        ldy(0x000B);
        lda8(abs8(0xA5EC + g_cpu.X)); abs8_set(0xA050 + g_cpu.Y, (u8)g_cpu.A);
        lda8(abs8(0xA5E4 + g_cpu.X)); abs8_set(0xA039 + g_cpu.Y, (u8)g_cpu.A);
        lda8((u8)g_cpu.X);
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, 0x03)); tax();
        dc_C12B13_glue();
        lda8(abs8(0x9872));
        if (!g_cpu._flag_N) {
            ldx(dp_x(0x9C));
            inc_abs(0x9829 + g_cpu.X);
            lda8(abs8(0x5E0D + g_cpu.X)); abs8_set(0x5E1D + g_cpu.X, (u8)g_cpu.A);
            lda8(abs8(0x9752 + g_cpu.X)); tay();
            lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0x9831 + g_cpu.X, (u8)g_cpu.A);
            lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0x9839 + g_cpu.X, (u8)g_cpu.A);
            abs8_set(0x98A7 + g_cpu.X, 0);
            goto done;
        }
    }
    ldx(dp_x(0x9C));
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D26 + g_cpu.X), dp8(0x8C))); abs8_set(0xA053 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0F + g_cpu.X), dp8(0x8E))); abs8_set(0xA03C + g_cpu.X, (u8)g_cpu.A);
    inx(); inx(); inx();
    stx_dp(0x80);
    dc_C1285A_glue();
    abs8_set(0x9875, 0);
    dc_C12926_glue();
    if (!g_cpu._flag_N) {
        lda8(abs8(0xA2B5));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (g_cpu._flag_Z) blocked = 0;
        else { dc_C128B0_glue(); blocked = g_cpu._flag_N; }
    }
    ldx(dp_x(0x9C));
    if (blocked) {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0xDB), 0x40));
        lsr_a(6); asl_a(6);
        dp8_set(0xDB, (u8)g_cpu.A);
        lda8(dp8(0xDB)); abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
        inc_abs(0x9791 + g_cpu.X);
        abs8_set(0x98A7 + g_cpu.X, 0);
    } else {
        abs8_set(0x9791 + g_cpu.X, 0);
        lda8(0x01); abs8_set(0x98A7 + g_cpu.X, 0x01);
    }
done:
    inx(); inx(); inx();
    stx_dp(0x80);
}

/* C1:23A4  Target list of enemies inside the triangle of the three party
 * members (1D0C/1D23 .. 1D0E/1D25): orders the corners P0/P1/P2 ($80..$85),
 * takes each corner's angles to the other two ($86..$8B, $92..$94 = 1 when
 * that span wraps past 0), then lists in 99C1 every live, targetable enemy
 * (3..0A) whose angle from each corner lies within that corner's span. */
static void swap_dp(u8 a, u8 b)                  /* PHA (old a); LDA b; STA a; PLA; STA b */
{
    u8 t = dp8(a);
    dc_push8(t); dp8_set(a, dp8(b)); dp8_set(b, dc_pull8());
}
static void swap_pt(u8 a, u8 b, int y_first)
{
    if (y_first) { swap_dp((u8)(a + 1), (u8)(b + 1)); swap_dp(a, b); }
    else { swap_dp(a, b); swap_dp((u8)(a + 1), (u8)(b + 1)); }
}
static void angle_from(u8 p)                     /* $DB = angle from point p to D5/D6 */
{
    lda8(dp8(p)); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(dp8((u8)(p + 1))); dp8_set(0xD4, (u8)g_cpu.A);
    dc_C10222_glue();
}
static int in_span(u8 lo, u8 hi, u8 wrap)        /* $DB within [lo, hi], or outside (hi, lo) when wrapped */
{
    u8 db = dp8(0xDB), l = dp8(lo), h = dp8(hi);
    if (!dp8(wrap)) {
        dc_cmp8(db, l);
        if (!g_cpu._flag_C) return 0;
        dc_cmp8(h, db);
        return g_cpu._flag_C;
    }
    dc_cmp8(h, db);
    if (g_cpu._flag_C) return 1;
    dc_cmp8(db, l);
    return g_cpu._flag_C;
}
DC_ENTRY(C123A4, RTS)
{
    tdc(); tax();
    stx_dp(0x92); stx_dp(0x94);
    for (int k = 0; k < 3; k++) {
        dp8_set(0x80 + 2 * k, abs8(0x1D0C + k));
        dp8_set(0x81 + 2 * k, abs8(0x1D23 + k));
    }
    if (dp8(0x84) >= dp8(0x80)) swap_pt(0x84, 0x80, 0);
    if (dp8(0x84) >= dp8(0x82)) swap_pt(0x84, 0x82, 0);
    if (dp8(0x80) >= dp8(0x82)) swap_pt(0x80, 0x82, 0);
    if (dp8(0x84) == dp8(0x80)) {
        if (dp8(0x85) < dp8(0x81)) swap_pt(0x84, 0x80, 1);
    } else if (dp8(0x80) == dp8(0x82)) {
        if (dp8(0x83) < dp8(0x81)) swap_pt(0x82, 0x80, 1);
    } else if (dp8(0x81) > dp8(0x83) || dp8(0x81) > dp8(0x85)) swap_pt(0x84, 0x82, 0);
    static const u8 from[6] = { 0x80, 0x80, 0x82, 0x82, 0x84, 0x84 };
    static const u8 to[6] = { 0x82, 0x84, 0x84, 0x80, 0x80, 0x82 };
    for (int k = 0; k < 6; k++) {
        dp8_set(0xD3, dp8(from[k])); dp8_set(0xD4, dp8((u8)(from[k] + 1)));
        dp8_set(0xD5, dp8(to[k])); dp8_set(0xD6, dp8((u8)(to[k] + 1)));
        dc_C10222_glue();
        dp8_set(0x86 + k, (u8)g_cpu.A);
    }
    for (int k = 0; k < 3; k++)
        if (dp8(0x87 + 2 * k) < dp8(0x86 + 2 * k)) dp8_set(0x92 + k, (u8)(dp8(0x92 + k) + 1));
    dc_C127D9_glue();
    ldx(0x000B); stx_dp(0x90);
    ldx(0x0003); stx_dp(0x8E);
    tdc(); tay();
    do {
        ldx(dp_x(0x8E));
        if (abs8(0x96F5 + g_cpu.X) && !(abs8(0x9FF7 + g_cpu.X) & 0x80)
            && !abs8(0xA0A8 + g_cpu.X) && !abs8(0xA09B + g_cpu.X)) {
            lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xD5, (u8)g_cpu.A);
            lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0xD6, (u8)g_cpu.A);
            angle_from(0x80);
            if (in_span(0x86, 0x87, 0x92)) {
                angle_from(0x82);
                if (in_span(0x88, 0x89, 0x93)) {
                    angle_from(0x84);
                    if (in_span(0x8A, 0x8B, 0x94)) {
                        lda8(dp8(0x8E)); abs8_set(0x99C1 + g_cpu.Y, (u8)g_cpu.A);
                        iny();
                    }
                }
            }
        }
        dp8_set(0x8E, (u8)(dp8(0x8E) + 1));
        lda8(dp8(0x8E));
        dc_cmp8((u8)g_cpu.A, dp8(0x90));
    } while (!g_cpu._flag_Z);
    dc_C127C5_glue();
}

/* C1:25A3  Target list of enemies in the strip between two battlers 9605 and
 * 9606: corner 80/81 is beside 9605 (offset 9607*8 across the line between
 * them), corner 82/83 beside 9605 or (9608) 9606; enemies (3..0A, or all
 * slots with 9604) inside both corners' quarter-turn spans go to 99C1. */
DC_ENTRY(C125A3, RTS)
{
    lda8(abs8(0x9607)); asl_a(3); dp8_set(0xAE, (u8)g_cpu.A);
    lda8(abs8(0x9605)); tax();
    lda8(abs8(0x9606)); tay();
    lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A);
    lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD6, (u8)g_cpu.A);
    dc_C10222_glue();
    dc_C101F9_glue();
    dp8_set(0x80, (u8)g_cpu.A);
    static const u8 turn[3] = { 0xC0, 0x80, 0x40 };
    for (int k = 0; k < 3; k++) {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0xDB), turn[k]));
        dc_C101F9_glue();
        dp8_set(0x81 + k, (u8)g_cpu.A);
    }
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0xD3), dp8(0x80))); dp8_set(0x80, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0xD4), dp8(0x81))); dp8_set(0x81, (u8)g_cpu.A);
    lda8(dp8(0xDB)); dp8_set(0x84, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0xDB), 0x40)); dp8_set(0x86, (u8)g_cpu.A);
    lda8(abs8(0x9608));
    int far = !g_cpu._flag_Z;
    u8 ox = far ? 0xD5 : 0xD3;
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8(ox), dp8(0x82))); dp8_set(0x82, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8((u8)(ox + 1)), dp8(0x83))); dp8_set(0x83, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0xDB), far ? 0x80 : 0xC0)); dp8_set(0x88, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    if (far) lda8(dc_adc8(dp8(0xDB), 0xC0)); else lda8(dp8(0xDB));
    dp8_set(0x8A, (u8)g_cpu.A);
    dp8_set(0x8C, 0); dp8_set(0x8D, 0);
    if (dp8(0x84) >= 0xC0) dp8_set(0x8C, 1);
    if (dp8(0x88) >= 0xC0) dp8_set(0x8D, 1);
    dc_C127D9_glue();
    ldx(0x000B); stx_dp(0x90);
    ldx(0x0003); stx_dp(0x8E);
    lda8(abs8(0x9604));
    if (!g_cpu._flag_Z) { stx_dp(0x90); tdc(); tax(); stx_dp(0x8E); }
    lda8(abs8(0x9606)); tax(); stx_dp(0x92);
    tdc(); tay();
    do {
        ldx(dp_x(0x8E));
        if (abs8(0x96F5 + g_cpu.X) && !(abs8(0x9FF7 + g_cpu.X) & 0x80)
            && !abs8(0xA0A8 + g_cpu.X) && !abs8(0xA09B + g_cpu.X)
            && g_cpu.X != dp_x(0x92)) {
            lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xD5, (u8)g_cpu.A);
            lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0xD6, (u8)g_cpu.A);
            angle_from(0x80);
            if (in_span(0x84, 0x86, 0x8C)) {
                angle_from(0x82);
                if (in_span(0x88, 0x8A, 0x8D)) {
                    lda8(dp8(0x8E)); abs8_set(0x99C1 + g_cpu.Y, (u8)g_cpu.A);
                    iny();
                }
            }
        }
        dp8_set(0x8E, (u8)(dp8(0x8E) + 1));
        lda8(dp8(0x8E));
        dc_cmp8((u8)g_cpu.A, dp8(0x90));
    } while (!g_cpu._flag_Z);
    lda8(abs8(0x9604));                         /* 27AD: the other battler leads the list */
    int lead = g_cpu._flag_Z ? 1 : 0;
    lda8(dp8(0x92));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_C == lead) { lda8(dp8(0x92)); abs8_set(0x99C0, (u8)g_cpu.A); }
    dc_C127C5_glue();
}

/* C1:62AA  Leap to A2B0/A2B2 (shared by 672E / 674A, $8E = command length,
 * $8F = arc flag): starts party slot A014[A010] (or enemy A2B6 for A010 >= 8)
 * moving there at speed A324/A32F like walk_steps, plus a vertical arc of
 * height $90 (7FFF / FFFF per step, 00D7) over $84 steps; a 9C/9D action
 * (9895) aims via 6549 and takes 4x as many steps. Advances 0 while moving,
 * A3A9[A010] when done (A010 = 4 just advances). */
typedef struct {
    u16 speed, steps, arc, arcflag, dst_x, dst_y, run, busy, done, done2;
    u16 sx, sy, len, h, h0, z, t0, t1, h1, h2, end;
} LeapSlots;
static const LeapSlots leap_party = { 0xA324, 0xABFE, 0xAA04, 0xAA2A, 0xA383, 0xA396, 0xA2EB, 0xA86D, 0xA19D, 0xA337,
    0xA888, 0xA8AE, 0xA9B8, 0xA920, 0xAA3D, 0xA922, 0xA8D4, 0xA8FA, 0xA96C, 0xA96E, 0xA9DE };
static const LeapSlots leap_enemy = { 0xA32F, 0xAC09, 0xAA1A, 0xAA35, 0xA38E, 0xA3A1, 0xA2F6, 0xA878, 0xA1E0, 0xA342,
    0xA89E, 0xA8C4, 0xA9CE, 0xA94C, 0xAA53, 0xA94E, 0xA8EA, 0xA910, 0xA998, 0xA99A, 0xA9F4 };
static int leap_aim(void) { lda8(abs8(0x9895)); return (u8)g_cpu.A == 0x9C || (u8)g_cpu.A == 0x9D; }
static void leap_slot_y(const LeapSlots *s) { ldy(s == &leap_party ? dp_x(0x80) : abs_x(0xA2B6)); }
static u8 leap_slot8(const LeapSlots *s) { return s == &leap_party ? dp8(0x80) : abs8(0xA2B6); }
static void leap_start(const LeapSlots *s)       /* D3..D6 set */
{
    if (leap_aim()) dc_C16549_glue();
    dc_C10222_glue();
    dc_C165BA_glue();
    leap_slot_y(s);
    lda8(dp8(dp8(0x8C) ? 0xE0 : 0xDE)); dp8_set(0xB1, (u8)g_cpu.A); dp8_set(0xB2, 0);
    lda8(abs8(s->speed + g_cpu.Y)); dp8_set(0xB3, (u8)g_cpu.A);
    dc_C100D7_glue();
    lda8(dp8(0xB5)); abs8_set(s->steps + g_cpu.Y, (u8)g_cpu.A);
    if (leap_aim()) { lda8(abs8(s->steps + g_cpu.Y)); asl_a(2); abs8_set(s->steps + g_cpu.Y, (u8)g_cpu.A); }
    lda8(dp8(0x8F)); abs8_set(s->arcflag + g_cpu.Y, (u8)g_cpu.A);
    for (int k = 0; k < 2; k++) {
        u16 v = k ? 0xA882 : 0xA880;
        lda8(abs8(v)); dp8_set(0xA7, (u8)g_cpu.A);
        lda8(abs8(v + 1)); dp8_set(0xA8, (u8)g_cpu.A);
        lda8(abs8(s->speed + g_cpu.Y)); dp8_set(0xA5, (u8)g_cpu.A);
        dc_C100A7_glue();
        ldx(dp_x(0xA9)); stx_abs(v);
    }
    lda8(leap_slot8(s)); dc_set_a8(asl8((u8)g_cpu.A)); tax();
    lda8(dp8(0x90)); abs8_set(s->arc + g_cpu.X, (u8)g_cpu.A); abs8_set(s->arc + 1 + g_cpu.X, 0);
    dc_cmp8((u8)g_cpu.A, 0x02);
    ldx(g_cpu._flag_Z ? 0x7FFF : 0xFFFF); stx_dp(0xB1);
    lda8(dp8(0x84)); dp8_set(0xB3, (u8)g_cpu.A);
    dc_C100D7_glue();
    leap_slot_y(s);
    lda8(dp8(0xDB));
    if (s == &leap_enemy) abs8_set(0xA355 + g_cpu.Y, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set((s == &leap_enemy ? 0xA230 : 0x96DB) + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B0)); abs8_set(s->dst_x + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); abs8_set(s->dst_y + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(s->run + g_cpu.Y, 0x01); abs8_set(s->busy + g_cpu.Y, 0x01);
    tdc(); abs8_set(s->done + g_cpu.Y, (u8)g_cpu.A);
    phy();
    lda8((u8)g_cpu.Y);
    dc_set_a8(asl8((u8)g_cpu.A)); tay();
    dc_set_a8(asl8((u8)g_cpu.A)); tax();
    dc_set_mx(0, g_cpu.x_flag);
    abs16_set(s->sx + g_cpu.Y, abs16(0xA880));
    abs16_set(s->sy + g_cpu.Y, abs16(0xA882));
    abs16_set(s->len + g_cpu.Y, dp16(0xB5));
    lda16(dp16(0xB1)); abs16_set(s->h + g_cpu.X, g_cpu.A); abs16_set(s->h0 + g_cpu.Y, g_cpu.A);
    tdc();
    abs16_set(s->z + g_cpu.X, 0);
    abs16_set(s->t0 + g_cpu.Y, 0); abs16_set(s->t1 + g_cpu.Y, 0);
    abs16_set(s->h1 + g_cpu.X, 0); abs16_set(s->h2 + g_cpu.X, 0);
    abs16_set(s->end + g_cpu.Y, 0);
    dc_set_mx(1, g_cpu.x_flag);
    ply();
    ldx(abs_x(0xA010));
    lda8(dp8(0x8E)); abs8_set(0xA3A9 + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); abs8_set(s->done2 + g_cpu.Y, 0x01);
}
static void leap_to_point(void)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (g_cpu._flag_Z) goto finished;
        tax();
        lda8(abs8(0xA014 + g_cpu.X)); tax();
        lda8(abs8(0xA337 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xA19D + g_cpu.X));
            if (g_cpu._flag_Z) goto moving;
            abs8_set(0xA337 + g_cpu.X, 0);
            goto finished;
        }
        stx_dp(0x80);
        lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A); abs8_set(0xA35D + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A); abs8_set(0xA370 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0xA2B0)); dp8_set(0xD5, (u8)g_cpu.A);
        lda8(abs8(0xA2B2)); dp8_set(0xD6, (u8)g_cpu.A);
        leap_start(&leap_party);
        goto moving;
    }
    ldx(abs_x(0xA2B6));
    lda8(abs8(0xA342 + g_cpu.X));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xA1E0 + g_cpu.X));
        if (g_cpu._flag_Z) goto moving;
        abs8_set(0xA342 + g_cpu.X, 0);
        goto finished;
    }
    lda8((u8)g_cpu.X); ldy(g_cpu.X);
    dc_set_a8(asl8((u8)g_cpu.A)); tax();
    lda8(abs8(0xA240 + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A); abs8_set(0xA368 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA250 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A); abs8_set(0xA37B + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B0)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); dp8_set(0xD6, (u8)g_cpu.A);
    leap_start(&leap_enemy);
moving:
    lda8(0x00); script_advance();
    return;
finished:
    ldx(abs_x(0xA010));
    lda8(abs8(0xA3A9 + g_cpu.X));
    script_advance();
}
static void leap_to_5d9e(u8 arc)
{
    dp8_set(0x8F, arc);
    lda8(abs8(0x5D9E)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x5D9F)); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x02); dp8_set(0x8E, 0x02);
    leap_to_point();
}
DC_ENTRY(C1672E, RTS) { leap_to_5d9e(0); }       /* leap to the spot 5D9E/5D9F */
DC_ENTRY(C1674A, RTS) { lda8(0x01); leap_to_5d9e(1); }   /* same, arcing */

/* C1:6751  Checks whether the battler can step ahead: sets up party slot
 * A014[A010] / the A018 list / enemy A2B6 facing A34A / A355 (count A639 /
 * A644 = FF), then tests the grid cell 16 units ahead (7BFD & C0 =
 * blocked). Free: A337 / A342 = 1, advances 0 (stay); blocked or already
 * stopped: clears it and advances 1. */
static void probe_init_party(void)              /* Y = slot */
{
    lda8(abs8(0xA5AA + g_cpu.Y)); abs8_set(0xA34A + g_cpu.Y, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set(0x96DB + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0xA35D + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0xA370 + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA2EB + g_cpu.Y, 0x01);
    lda8(0x02); abs8_set(0xA86D + g_cpu.Y, 0x02);
    tdc();
    abs8_set(0xA311 + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0xA19D + g_cpu.Y, (u8)g_cpu.A);
    lda8((u8)(g_cpu.A - 1)); abs8_set(0xA639 + g_cpu.Y, (u8)g_cpu.A);
}
static int probe_party(void)                     /* Y = slot; 1 = free */
{
    lda8(abs8(0xA19D + g_cpu.Y));
    if (!g_cpu._flag_Z) return 0;
    lda8(0x10); dp8_set(0xAE, 0x10);
    lda8(abs8(0xA34A + g_cpu.Y));
    dc_C101F9_glue();
    dp8_set(0x82, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0xA34A + g_cpu.Y), 0x40));
    dc_C101F9_glue();
    dp8_set(0x83, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D23 + g_cpu.Y), dp8(0x82)));
    lda8((u8)g_cpu.A & 0xF0); dp8_set(0x82, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0C + g_cpu.Y), dp8(0x83)));
    dc_C1011A_glue();
    lda8((u8)g_cpu.A | dp8(0x82));
    tax();
    lda8(abs8(0x7BFD + g_cpu.X) & 0xC0);
    return g_cpu._flag_Z;
}
DC_ENTRY(C16751, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax();
            lda8(abs8(0xA014 + g_cpu.X)); tay();
            lda8(abs8(0xA337 + g_cpu.Y));
            if (g_cpu._flag_Z) probe_init_party();
            if (!probe_party()) { tdc(); abs8_set(0xA337 + g_cpu.Y, (u8)g_cpu.A); goto blocked; }
            lda8(0x01); abs8_set(0xA337 + g_cpu.Y, 0x01);
            goto free;
        }
        tdc(); tax(); stx_dp(0x80);
        for (;;) {
            ldx(dp_x(0x80));
            lda8(abs8(0xA018 + g_cpu.X));
            if (g_cpu._flag_N) break;
            tay();
            lda8(abs8(0xA337 + g_cpu.Y));
            if (!g_cpu._flag_Z) break;
            probe_init_party();
            dp8_set(0x80, (u8)(dp8(0x80) + 1));
        }
        dp8_set(0x84, 0);
        for (;;) {
            ldx(dp_x(0x80));
            lda8(abs8(0xA018 + g_cpu.X));
            if (g_cpu._flag_N) break;
            tay();
            if (probe_party()) {
                lda8(0x01); abs8_set(0xA337 + g_cpu.Y, 0x01);
                dp8_set(0x84, (u8)(dp8(0x84) + 1));
            } else { tdc(); abs8_set(0xA337 + g_cpu.Y, (u8)g_cpu.A); }
            dp8_set(0x80, (u8)(dp8(0x80) + 1));
        }
        lda8(dp8(0x84));
        if (g_cpu._flag_Z) goto blocked;
        goto free;
    }
    ldy(abs_x(0xA2B6));
    lda8(abs8(0xA342 + g_cpu.Y));
    if (g_cpu._flag_Z) {
        lda8(abs8(0xA5B5 + g_cpu.Y)); abs8_set(0xA355 + g_cpu.Y, (u8)g_cpu.A);
        tax();
        lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set(0xA230 + g_cpu.Y, (u8)g_cpu.A);
        lda8(0x01); abs8_set(0xA2F6 + g_cpu.Y, 0x01);
        lda8(0x02); abs8_set(0xA878 + g_cpu.Y, 0x02);
        tdc();
        abs8_set(0xA31C + g_cpu.Y, (u8)g_cpu.A);
        abs8_set(0xA1E0 + g_cpu.Y, (u8)g_cpu.A);
        lda8((u8)(g_cpu.A - 1)); abs8_set(0xA644 + g_cpu.Y, (u8)g_cpu.A);
        lda8((u8)g_cpu.Y);
        dc_set_a8(asl8((u8)g_cpu.A)); tax();
        lda8(abs8(0xA240 + g_cpu.X)); abs8_set(0xA368 + g_cpu.Y, (u8)g_cpu.A);
        lda8(abs8(0xA250 + g_cpu.X)); abs8_set(0xA37B + g_cpu.Y, (u8)g_cpu.A);
    }
    ldx(abs_x(0xA2B6));
    lda8(abs8(0xA1E0 + g_cpu.X));
    int free_ = 0;
    if (g_cpu._flag_Z) {
        lda8(0x10); dp8_set(0xAE, 0x10);
        lda8(abs8(0xA355 + g_cpu.X));
        dc_C101F9_glue();
        dp8_set(0x82, (u8)g_cpu.A);
        ldx(abs_x(0xA2B6));
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA355 + g_cpu.X), 0x40));
        dc_C101F9_glue();
        dp8_set(0x83, (u8)g_cpu.A);
        lda8((u8)g_cpu.Y);
        dc_set_a8(asl8((u8)g_cpu.A)); tay();
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA250 + g_cpu.Y), dp8(0x82)));
        lda8((u8)g_cpu.A & 0xF0); dp8_set(0x82, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA240 + g_cpu.Y), dp8(0x83)));
        dc_C1011A_glue();
        lda8((u8)g_cpu.A | dp8(0x82));
        tax();
        lda8(abs8(0x7BFD + g_cpu.X) & 0xC0);
        free_ = g_cpu._flag_Z;
        ldx(abs_x(0xA2B6));
    }
    if (!free_) { abs8_set(0xA342 + g_cpu.X, 0); goto blocked; }
    lda8(0x01); abs8_set(0xA342 + g_cpu.X, 0x01);
free:
    lda8(0x00); script_advance();
    return;
blocked:
    lda8(0x01); script_advance();
}

/* C1:3BC2  One step of enemy $9C closing in on target 9752: heads for the
 * spot A5FC (20, 40 for formation 0E) from the target like 3A3D; when that
 * spot is within reach (2B1F) it circles the target instead (heading
 * +-10 by A5F4, flipping A5F4 when blocked). A blocked approach snaps the
 * heading to the next quarter turn; formation 0D never moves, 0F ignores
 * other battlers (9875 = 1). */
static int enemy_try_move(void)                  /* X = $9C; 1 = blocked */
{
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D26 + g_cpu.X), dp8(0x8C))); abs8_set(0xA053 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0F + g_cpu.X), dp8(0x8E))); abs8_set(0xA03C + g_cpu.X, (u8)g_cpu.A);
    inx(); inx(); inx();
    stx_dp(0x80);
    dc_C1285A_glue();
    abs8_set(0x9875, 0);
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x0F);
    if (g_cpu._flag_Z) { lda8(0x01); abs8_set(0x9875, 0x01); }
    dc_C12926_glue();
    if (g_cpu._flag_N) return 1;
    lda8(abs8(0xA2B5));
    dc_cmp8((u8)g_cpu.A, 0x0F);
    if (g_cpu._flag_Z) return 0;
    dc_C128B0_glue();
    return g_cpu._flag_N;
}
DC_ENTRY(C13BC2, RTS)
{
    ldx(dp_x(0x9C));
    abs8_set(0xA5FD + g_cpu.X, 0);
    lda8(abs8(0x9791 + g_cpu.X));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0x9786 + g_cpu.X)); dp8_set(0xDB, (u8)g_cpu.A);
    } else {
        lda8(abs8(0x9752 + g_cpu.X)); tay();
        lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0xD3, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD4, (u8)g_cpu.A);
        lda8(abs8(0x1D0F + g_cpu.X)); dp8_set(0xD5, (u8)g_cpu.A); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D26 + g_cpu.X)); dp8_set(0xD6, (u8)g_cpu.A); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
        lda8(0x20); abs8_set(0xA5FC, 0x20);
        lda8(abs8(0xA2B5));
        dc_cmp8((u8)g_cpu.A, 0x0E);
        if (g_cpu._flag_Z) { u8 v = abs8(0xA5FC); g_cpu._flag_C = v >> 7; v = (u8)(v << 1); abs8_set(0xA5FC, v); dc_set_flags_nz8(v); }
        lda8(abs8(0xA5FC)); dp8_set(0xAE, (u8)g_cpu.A);
        spot_heading();
    }
    step_heading();
    ldy(0x000B);
    lda8(abs8(0xA5EC + g_cpu.X)); abs8_set(0xA050 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA5E4 + g_cpu.X)); abs8_set(0xA039 + g_cpu.Y, (u8)g_cpu.A);
    lda8((u8)g_cpu.X);
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, 0x03)); tax();
    dc_C12B1F_glue();
    lda8(abs8(0x9872));
    if (g_cpu._flag_N) {                         /* not there yet: approach */
        ldx(dp_x(0x9C));
        lda8(abs8(0xA2B5));
        dc_cmp8((u8)g_cpu.A, 0x0D);
        if (g_cpu._flag_Z) abs8_set(0x98A7 + g_cpu.X, 0);
        else if (enemy_try_move()) {
            ldx(dp_x(0x9C));
            g_cpu._flag_C = 0;
            lda8(dc_adc8(dp8(0xDB), 0x40));
            lsr_a(6); asl_a(6);
            dp8_set(0xDB, (u8)g_cpu.A);
            lda8(dp8(0xDB)); abs8_set(0x9786 + g_cpu.X, (u8)g_cpu.A);
            inc_abs(0x9791 + g_cpu.X);
            abs8_set(0x98A7 + g_cpu.X, 0);
        } else {
            ldx(dp_x(0x9C));
            abs8_set(0x9791 + g_cpu.X, 0);
            lda8(0x01); abs8_set(0x98A7 + g_cpu.X, 0x01);
        }
    } else {                                     /* in reach: circle the target */
        ldx(dp_x(0x9C));
        abs8_set(0x9791 + g_cpu.X, 0);
        lda8(abs8(0x9752 + g_cpu.X)); tay();
        lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0xD3, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD4, (u8)g_cpu.A);
        lda8(abs8(0x1D0F + g_cpu.X)); dp8_set(0xD5, (u8)g_cpu.A); abs8_set(0x98AF + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D26 + g_cpu.X)); dp8_set(0xD6, (u8)g_cpu.A); abs8_set(0x98B7 + g_cpu.X, (u8)g_cpu.A);
        dc_C10222_glue();
        dc_push8((u8)g_cpu.A);
        ldx(dp_x(0x9C));
        lda8(abs8(0xA5F4 + g_cpu.X));
        lda8(g_cpu._flag_Z ? 0x10 : 0xF0);
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, dp8(0xDB))); dp8_set(0xDB, (u8)g_cpu.A);
        lda8(abs8(0xA5FC)); dp8_set(0xAE, (u8)g_cpu.A);
        lda8(dp8(0xDB));
        dc_C101F9_glue();
        ldx(dp_x(0x9C));
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, abs8(0x1D23 + g_cpu.Y))); dp8_set(0xD6, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0xDB), 0x40));
        dc_C101F9_glue();
        ldx(dp_x(0x9C));
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, abs8(0x1D0C + g_cpu.Y))); dp8_set(0xD5, (u8)g_cpu.A);
        lda8(abs8(0x1D0F + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A);
        lda8(abs8(0x1D26 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A);
        dc_C10222_glue();
        lda8(dc_pull8());
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, 0x80));
        dc_cmp8((u8)g_cpu.A, dp8(0xDB));
        if (g_cpu._flag_Z) {                     /* straight across: nudge off the line */
            dp8_set(0xDB, (u8)(dp8(0xDB) + 1));
            lda8(abs8(0xA5FC));
            dc_cmp8((u8)g_cpu.A, 0x20);
            if (g_cpu._flag_Z) dp8_set(0xDB, (u8)(dp8(0xDB) + 1));
        }
        step_heading();
        ldx(dp_x(0x9C));
        if (enemy_try_move()) {
            ldx(dp_x(0x9C));
            lda8(abs8(0xA5F4 + g_cpu.X) ^ 0x01); abs8_set(0xA5F4 + g_cpu.X, (u8)g_cpu.A);
        } else {
            ldx(dp_x(0x9C));
            lda8(0x01); abs8_set(0x98A7 + g_cpu.X, 0x01);
        }
    }
    inx(); inx(); inx();
    stx_dp(0x80);
}

/* C1:0299  Redraws the battle status window into the tile buffer 0CC0
 * (2 bytes per tile, 40 per row): clears it, draws the frame corners for
 * layout 9F20, then per party member (96F5) the name twice (9412.., rows
 * $84 and $84-40), HP "cur/max" and MP, palette 2D for the names and for
 * low HP (A10F: HP <= max/8 with A110, or 0), the gauge ends 5D/5E (06F0)
 * in layouts 1/2; then the enemy name rows (98CC, ACBC) and, while a menu
 * is open (A6DE), the command list (0785 / 081E) or the cursor row (08E8). */
static void row_chars(u16 src, int n, u8 attr)    /* Y = tile offset, X = source index */
{
    dp8_set(0x8E, (u8)n);
    do {
        lda8(abs8(src + g_cpu.X)); abs8_set(0x0CC0 + g_cpu.Y, (u8)g_cpu.A);
        lda8(attr); abs8_set(0x0CC1 + g_cpu.Y, attr);
        iny(); iny(); inx();
        dp8_set(0x8E, (u8)(dp8(0x8E) - 1));
    } while (dp8(0x8E));
    dc_set_flags_nz8(0);
}
static void y_from_84(u16 add, int sub)           /* REP; (C) LDA $84 +- add; TAY */
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = sub;
    lda16(sub ? dc_sbc16(dp16(0x84), add) : dc_adc16(dp16(0x84), add));
    tay();
}
static void y_by_layout(u16 l0, u16 l1, int sub1, u16 l2)   /* m0: LDA 9F20 picks the offset */
{
    lda16(abs16(0x9F20));
    if (g_cpu._flag_Z) { y_from_84(l0, 0); return; }
    lda16((u16)(g_cpu.A - 1));
    if (g_cpu._flag_Z) y_from_84(l1, sub1);
    else y_from_84(l2, 0);
}
static void digits3(u16 at)                       /* 949D..949F -> tiles at Y+at, +2, +4 */
{
    lda8(abs8(0x949D)); abs8_set(at + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x949E)); abs8_set(at + 2 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x949F)); abs8_set(at + 4 + g_cpu.Y, (u8)g_cpu.A);
}
static void low_attr(void)                        /* A = 2D when A10F (low HP) else 29 */
{
    ldx(0x0029);
    lda8(abs8(0xA10F));
    if (!g_cpu._flag_Z) ldx(0x002D);
    dc_set_a8((u8)g_cpu.X); dc_set_flags_nz8((u8)g_cpu.X);
}
DC_ENTRY(C10299, RTS)
{
    tdc(); tax(); tay();
    do {
        tdc(); abs8_set(0x0CC0 + g_cpu.Y, (u8)g_cpu.A);
        lda8(0x29); abs8_set(0x0CC1 + g_cpu.Y, 0x29);
        iny(); iny(); inx();
        cpx(0x00C0);
    } while (!g_cpu._flag_Z);
    static const u16 corner[3][4] = {           /* 64 / 66 / 66 / 65 */
        { 0x0CE8, 0x0CEA, 0x0CFC, 0x0CFA }, { 0x0CDA, 0x0CDC, 0x0CE4, 0x0CE2 }, { 0x0CE6, 0x0CE8, 0x0CF0, 0x0CEE } };
    lda8(abs8(0x9F20));
    int lay = g_cpu._flag_Z ? 0 : (u8)(g_cpu.A - 1) == 0 ? 1 : 2;
    abs8_set(corner[lay][0], 0x64);
    abs8_set(corner[lay][1], 0x66); abs8_set(corner[lay][2], 0x66);
    lda8(0x65); abs8_set(corner[lay][3], 0x65);
    lda8(abs8(0x9F20));
    ldx(lay == 1 ? 0x0068 : 0x005A);
    stx_dp(0x84);
    tdc(); tax(); stx_dp(0x80);
    do {                                         /* party members */
        ldy(dp_x(0x84));
        ldx(dp_x(0x80));
        lda8(abs8(0x96F5 + g_cpu.X));
        if (g_cpu._flag_Z) goto next;
        lda8(bus8(0xCCF837 + g_cpu.X)); tax();
        row_chars(0x9412, 5, 0x2D);
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(dp16(0x84), 0x0040)); tay();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        row_chars(0x9412, 5, 0x2D);
        abs8_set(0xA10F, 0);
        dc_set_mx(0, g_cpu.x_flag);
        lda16(dp16(0x80)); lda16((u16)(g_cpu.A << 1)); tax();
        lda16(bus16(0xCCF8ED + g_cpu.X)); dp16_set(0xA2, g_cpu.A); tax();
        lda16(abs16(0x5E30 + g_cpu.X)); abs16_set(0x9499, g_cpu.A);
        int low = g_cpu._flag_Z;
        if (!low) {
            lda16(abs16(0x5E32 + g_cpu.X));
            dc_C1011B_glue();
            dc_cmp16(g_cpu.A, abs16(0x9499));
            if (g_cpu._flag_Z || g_cpu._flag_C) { lda16(abs16(0xA110)); low = !g_cpu._flag_Z; }
        }
        if (low) { u16 v = (u16)(abs16(0xA10F) + 1); abs16_set(0xA10F, v); dc_set_flags_nz16(v); }
        y_by_layout(0x000E, 0x000E, 1, 0x000C);
        dc_C1011F_glue();                        /* returns with 8-bit A */
        dc_C1104E_glue();
        digits3(0x0CC0);
        low_attr();
        abs8_set(0x0CC1 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x0CC3 + g_cpu.Y, (u8)g_cpu.A);
        abs8_set(0x0CC5 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x0CC7 + g_cpu.Y, (u8)g_cpu.A);
        lda8(abs8(0x9F20));
        if (g_cpu._flag_Z) {
            lda8(0xE0); abs8_set(0x0CC6 + g_cpu.Y, 0xE0);
            dc_set_mx(0, g_cpu.x_flag);
            ldx(dp_x(0xA2));
            lda16(abs16(0x5E32 + g_cpu.X)); abs16_set(0x9499, g_cpu.A);
            y_from_84(0x0016, 0);
            dc_C1011F_glue();
            dc_C1104E_glue();
            digits3(0x0CC0);
            low_attr();
            abs8_set(0x0CC1 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x0CC3 + g_cpu.Y, (u8)g_cpu.A);
            abs8_set(0x0CC5 + g_cpu.Y, (u8)g_cpu.A);
        } else {
            lda8(0x5F); abs8_set(0x0CC6 + g_cpu.Y, 0x5F);
        }
        dc_set_mx(0, g_cpu.x_flag);              /* MP */
        ldx(dp_x(0xA2));
        lda16(abs16(0x5E34 + g_cpu.X)); abs16_set(0x9499, g_cpu.A);
        y_by_layout(0x001E, 0x0006, 1, 0x0014);
        dc_C10174_glue();                        /* returns with 8-bit A */
        lda8(abs8(0x9F20));
        dc_C1104E_glue();
        if (abs8(0x9F20) == 0) {
            lda8(abs8(0x949E)); abs8_set(0x0CC2 + g_cpu.Y, (u8)g_cpu.A);
            lda8(abs8(0x949F)); abs8_set(0x0CC4 + g_cpu.Y, (u8)g_cpu.A);
            low_attr();
            abs8_set(0x0CC3 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x0CC5 + g_cpu.Y, (u8)g_cpu.A);
        } else {
            lda8(abs8(0x949E)); abs8_set(0x0CC0 + g_cpu.Y, (u8)g_cpu.A);
            lda8(abs8(0x949F)); abs8_set(0x0CC2 + g_cpu.Y, (u8)g_cpu.A);
            low_attr();
            abs8_set(0x0CC1 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x0CC3 + g_cpu.Y, (u8)g_cpu.A);
        }
        lda8(abs8(0x9F20));
        if (!g_cpu._flag_Z) {                    /* gauge ends */
            dc_set_mx(0, g_cpu.x_flag);
            lda16((u16)(g_cpu.A - 1));
            y_from_84(g_cpu._flag_Z ? 0x000A : 0x0018, 0);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            lda8(0x5D); abs8_set(0x0CC0 + g_cpu.Y, 0x5D);
            lda8(0x5E); abs8_set(0x0CCA + g_cpu.Y, 0x5E);
            lda8(0x29); abs8_set(0x0CC1 + g_cpu.Y, 0x29); abs8_set(0x0CCB + g_cpu.Y, 0x29);
            dc_C106F0_glue();
        }
    next:
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dp16(0x84), 0x0080)); dp16_set(0x84, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
        lda8(dp8(0x80));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
    ldx(0x0042); stx_dp(0x84);                   /* enemy names */
    tdc(); tax(); stx_dp(0x80);
    do {
        ldx(dp_x(0x80));
        lda8(abs8(0x98CC + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(dp8(0x80)); asl_a(3); dp8_set(0x8E, (u8)g_cpu.A);
            asl_a(1);
            g_cpu._flag_C = 0;
            lda8(dc_adc8((u8)g_cpu.A, dp8(0x8E))); tax();
            ldy(dp_x(0x84));
            row_chars(0xACBC, 11, 0x29);
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = 1;
            lda16(dc_sbc16(dp16(0x84), 0x0040)); tay();
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            inx();
            row_chars(0xACBC, 11, 0x29);
            dc_set_mx(0, g_cpu.x_flag);
            g_cpu._flag_C = 0;
            lda16(dc_adc16(dp16(0x84), 0x0080)); dp16_set(0x84, g_cpu.A);
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
        }
        dp8_set(0x80, (u8)(dp8(0x80) + 1));
        lda8(dp8(0x80));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
    lda8(abs8(0xA6DE));
    if (g_cpu._flag_Z) return;
    lda8(abs8(0xA86B));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xA62D));
        dc_cmp8((u8)g_cpu.A, abs8(0xA0D7));
        if (g_cpu._flag_Z) return;
        lda8(0xFE); abs8_set(0xA6DF, 0xFE);
        lda8(abs8(0xA62D));
        dc_C108E8_glue();
        return;
    }
    lda8(abs8(0x95DB));
    if (!g_cpu._flag_Z) return;
    lda8((u8)(abs8(0xA6DE) - 1)); abs8_set(0x95F1, (u8)g_cpu.A);
    lda8(abs8(0xA6DD)); tax();
    lda8(abs8(0xA6D9 + g_cpu.X));
    if (g_cpu._flag_N) {
        tdc(); tax();
        for (;;) {
            lda8(abs8(0xA6D9 + g_cpu.X));
            if (!g_cpu._flag_N) break;
            inx();
        }
        dc_push8((u8)g_cpu.A);
        lda8((u8)g_cpu.X); abs8_set(0xA6DD, (u8)g_cpu.A);
        lda8(dc_pull8());
    }
    abs8_set(0x95D5, (u8)g_cpu.A);
    tdc(); tax(); stx_dp(0x86);
    do {
        ldx(dp_x(0x86));
        lda8(abs8(0xA6D9 + g_cpu.X));
        if (!g_cpu._flag_N) dc_C10785_glue();
        dp8_set(0x86, (u8)(dp8(0x86) + 1));
        lda8(dp8(0x86));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
    dc_C1081E_glue();
}

/* ---- batch 31: small handlers from the battle animation and effect tables ---- */
/* Animation-script ops (tables B85F / B88D) that do nothing, and two unused
 * entries of the D3xx/D4xx handler list. */
DC_ENTRY(C198C4, RTS) {}
DC_ENTRY(C19960, RTS) {}
DC_ENTRY(C19961, RTS) {}
DC_ENTRY(C19966, RTS) {}
DC_ENTRY(C19978, RTS) {}
DC_ENTRY(C1997D, RTS) {}
DC_ENTRY(C1997E, RTS) {}
DC_ENTRY(C1997F, RTS) {}
DC_ENTRY(C19B46, RTS) {}
DC_ENTRY(C19B47, RTS) {}
DC_ENTRY(C19B8C, RTS) {}
DC_ENTRY(C19C6E, RTS) {}
DC_ENTRY(C19E62, RTS) {}
DC_ENTRY(C1D370, RTS) {}
DC_ENTRY(C1D48F, RTS) {}
/* Animation ops that clear the "effect running" flag AF24. */
DC_ENTRY(C19962, RTS) { abs8_set(0xAF24, 0); }
DC_ENTRY(C19979, RTS) { abs8_set(0xAF24, 0); }
DC_ENTRY(C199B4, RTS) { abs8_set(0xAF24, 0); }
/* C1:9967  AEE4 = second byte of the current animation record (CC:B1D2+1). */
DC_ENTRY(C19967, RTS)
{
    ldx(abs_x(0xB1D2));
    lda8(abs8(0xB252)); asl_a(2); tay();
    lda8(bus8(0xCC0001 + g_cpu.X));
    abs8_set(0xAEE4, (u8)g_cpu.A);
}
/* Table B8BB: pick screen effect n (AECC) and request it (AECB = 1). */
static void fx_pick(u8 n) { lda8(n); abs8_set(0xAECC, n); lda8(0x01); abs8_set(0xAECB, 0x01); }
DC_ENTRY(C1AB4E, RTS) { fx_pick(0x00); }
DC_ENTRY(C1AB59, RTS) { fx_pick(0x01); }
DC_ENTRY(C1AB64, RTS) { fx_pick(0x02); }
DC_ENTRY(C1AB6F, RTS) { fx_pick(0x03); }
DC_ENTRY(C1AB7A, RTS) { fx_pick(0x04); }
DC_ENTRY(C1AB85, RTS) { fx_pick(0x05); }
DC_ENTRY(C1AB90, RTS) { fx_pick(0x06); }
DC_ENTRY(C1AAAB, RTS) { fx_pick(0x03); }
DC_ENTRY(C1AAB6, RTS) { fx_pick(0x04); }
DC_ENTRY(C1AAC1, RTS) { fx_pick(0x05); }
DC_ENTRY(C1AACC, RTS) { fx_pick(0x06); }
DC_ENTRY(C1AAD7, RTS) { fx_pick(0x07); }
DC_ENTRY(C1AAE2, RTS) { fx_pick(0x08); }
DC_ENTRY(C1AAED, RTS) { fx_pick(0x09); }
DC_ENTRY(C1AAF8, RTS) { fx_pick(0x0A); }
/* C1:2329  Clears 9604, re-checks the party triangle (23A4) and resets the
 * target list (27E8). */
DC_ENTRY(C12329, RTS)
{
    abs8_set(0x9604, 0);
    dc_C123A4_glue();
    dc_C127E8_glue();
}
/* C1:5D18  Battle script op: skip 2 bytes. */
DC_ENTRY(C15D18, RTS) { lda8(0x02); script_advance(); }
/* C1:D4F3  Ends the current fighter's special state: E9B8, then clears
 * bit 6 of its status byte 5E4A and B200. */
DC_ENTRY(C1D4F3, RTS)
{
    dc_C1E9B8_glue();
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E4A + g_cpu.X) & 0xBF);
    abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
    lda8(0x00); abs8_set(0xB200, 0);
}
/* Effect table FA61 (item/tech effects, fighter in B1F6, field offset $1C,
 * bit mask $1E). */
DC_ENTRY(C1EFBC, RTS) { dc_C1EDBA_glue(); dc_C1EF39_glue(); }
static void stat_x(void)                         /* X = B1F6 + $1C */
{
    tdc(); lda8(dp8(0x1C));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, abs16(0xB1F6)));
    tax(); tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:F1EF  Sets the bits $1E in the fighter's byte 5E2D+$1C. */
DC_ENTRY(C1F1EF, RTS)
{
    stat_x();
    lda8(abs8(0x5E2D + g_cpu.X) | dp8(0x1E));
    abs8_set(0x5E2D + g_cpu.X, (u8)g_cpu.A);
}
/* C1:F0BD  Clears the bits $1E there ($1E is left inverted). */
DC_ENTRY(C1F0BD, RTS)
{
    lda8(dp8(0x1E) ^ 0xFF); dp8_set(0x1E, (u8)g_cpu.A);
    stat_x();
    lda8(abs8(0x5E2D + g_cpu.X) & dp8(0x1E));
    abs8_set(0x5E2D + g_cpu.X, (u8)g_cpu.A);
}
/* C1:EFC3  $1A = 1 when a random number below 100 (AF22) is under $1C. */
DC_ENTRY(C1EFC3, RTS)
{
    dp8_set(0x1A, 0);
    tdc(); tax();
    lda8(0x64);
    dc_C1AF22_glue();
    dc_cmp8((u8)g_cpu.A, dp8(0x1C));
    if (!g_cpu._flag_C) { dp8_set(0x1A, (u8)(dp8(0x1A) + 1)); dc_set_flags_nz8(dp8(0x1A)); }
}

/* ---- batch 32 ---- */
/* Battle script ops on the counters 5D9C..5D9F. */
DC_ENTRY(C15F9A, RTS) { abs8_set(0xAB4E, 0); abs8_set(0xA08B, 0); lda8(0x01); script_advance(); }
DC_ENTRY(C15965, RTS) { dec_abs(0x5D9C); op_done(0x01); }
DC_ENTRY(C15970, RTS) { dec_abs(0x5D9D); op_done(0x01); }
/* C1:5722  Returns from a script subroutine (56FB) when 5D9F is 0. */
DC_ENTRY(C15722, RTS)
{
    lda8(abs8(0x5D9F));
    if (g_cpu._flag_Z) { dc_C156FB_glue(); return; }
    lda8(0x01); script_advance();
}
/* C1:58FE  5D9C = operand. */
DC_ENTRY(C158FE, RTS)
{
    lda8(script_byte()); abs8_set(0x5D9C, (u8)g_cpu.A);
    op_done(0x02);
}
/* C1:5AAF / 5AC5  Swap 5D9C with 5D9E / 5D9F. */
static void swap_5d9c(u16 b)
{
    u8 t = abs8(0x5D9C);
    lda8(abs8(b)); abs8_set(0x5D9C, (u8)g_cpu.A);
    abs8_set(b, t);
    op_done(0x01);
}
DC_ENTRY(C15AAF, RTS) { swap_5d9c(0x5D9E); }
DC_ENTRY(C15AC5, RTS) { swap_5d9c(0x5D9F); }
/* C1:5FB3  Clears A1D8..A1DF. */
DC_ENTRY(C15FB3, RTS)
{
    for (int k = 7; k >= 0; k--) abs8_set(0xA1D8 + k, 0);
    ldx(0xFFFF);
    lda8(0x01); script_advance();
}
/* C1:AB9B  Effect target = the party member (slot 0/1/2 records 5E30/5EB0/5F30)
 * with the highest current HP; ties go to the later one. $02/$03 = that HP. */
DC_ENTRY(C1AB9B, RTS)
{
    tdc();
    ldy(abs_x(0x5E30)); dp16_set(0x02, g_cpu.Y);
    abs8_set(0xAECC, 0);
    static const u16 hp[2] = { 0x5EB0, 0x5F30 };
    for (int k = 0; k < 2; k++) {
        ldy(abs_x(hp[k]));
        cpy(dp16(0x02));
        if (g_cpu._flag_C) { dp16_set(0x02, g_cpu.Y); lda8((u8)(k + 1)); abs8_set(0xAECC, (u8)(k + 1)); }
    }
    tdc();
    lda8(0x01); abs8_set(0xAECB, 0x01);
    dp8_set(0x03, 0);
}
/* C1:A6ED  Lists the effect targets: every slot 3..10 with AEFF+slot != FF
 * goes to AECC.., count in AECB. */
DC_ENTRY(C1A6ED, RTS)
{
    tdc(); tax();
    for (ldy(0x0003);;) {
        lda8(abs8(0xAEFF + g_cpu.Y));
        if ((u8)g_cpu.A != 0xFF) { lda8((u8)g_cpu.Y); abs8_set(0xAECC + g_cpu.X, (u8)g_cpu.A); inx(); }
        iny();
        cpy(0x000B);
        if (g_cpu._flag_Z) break;
    }
    lda8((u8)g_cpu.X); abs8_set(0xAECB, (u8)g_cpu.A);
}
/* C1:F10D  Effect: HP of the target (B1F6) or, with $1E = 0, of the user
 * (B1F4) set to 0; for the target also marks it in AEB0 and AEB2. */
DC_ENTRY(C1F10D, RTS)
{
    lda8(dp8(0x1E));
    if (g_cpu._flag_Z) ldx(abs_x(0xB1F4));
    else {
        ldx(abs_x(0xB1F6));
        dc_set_mx(0, g_cpu.x_flag);
        lda16(g_cpu.X); asl_a(1);
        lda16((u16)(g_cpu.A << 8 | g_cpu.A >> 8)); dc_set_flags_nz8((u8)g_cpu.A);
        tay(); tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(0x01); abs8_set(0xAEB0 + g_cpu.Y, 0x01); abs8_set(0xAEB2, 0x01);
    }
    tdc();
    abs8_set(0x5E31 + g_cpu.X, (u8)g_cpu.A); abs8_set(0x5E30 + g_cpu.X, (u8)g_cpu.A);
}
/* C1:F1C7  When AE9B bit 7 is set: damage AD89 doubled (C90B with $2A = 2),
 * or 9999 when $1E = 1. */
DC_ENTRY(C1F1C7, RTS)
{
    lda8(abs8(0xAE9B));
    g_cpu._flag_Z = (g_cpu.A & 0x80) == 0;
    if (g_cpu._flag_Z) return;
    lda8(dp8(0x1E));
    dc_cmp8((u8)g_cpu.A, 0x01);
    if (g_cpu._flag_Z) { ldx(0x270F); stx_abs(0xAD89); return; }
    ldx(abs_x(0xAD89)); stx_dp(0x28);
    ldx(0x0002); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_abs(0xAD89);
}

/* ---- batch 33 ---- */
/* Battle script ops on the variables 5D80+n (n = operand) and 5D9C. */
DC_ENTRY(C156BB, RTS) { lda8(script_byte()); tax(); lda8(0xFF); abs8_set(0xA437 + g_cpu.X, 0xFF); op_done(0x02); }
DC_ENTRY(C159FA, RTS) { lda8(script_byte()); tax(); lda8(abs8(0x5D9C)); abs8_set(0x5D80 + g_cpu.X, (u8)g_cpu.A); op_done(0x02); }
DC_ENTRY(C15931, RTS) { lda8(script_byte()); tax(); inc_abs(0x5D80 + g_cpu.X); op_done(0x02); }
DC_ENTRY(C15956, RTS) { lda8(script_byte()); tax(); dec_abs(0x5D80 + g_cpu.X); op_done(0x02); }
/* 5D9C += operand / += 5D80+operand. */
DC_ENTRY(C15991, RTS)
{
    lda8(script_byte());
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, abs8(0x5D9C))); abs8_set(0x5D9C, (u8)g_cpu.A);
    op_done(0x02);
}
DC_ENTRY(C159A3, RTS)
{
    lda8(script_byte()); tax();
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x5D80 + g_cpu.X), abs8(0x5D9C))); abs8_set(0x5D9C, (u8)g_cpu.A);
    op_done(0x02);
}
/* 5D80+b = 5D80+a (two operands). */
DC_ENTRY(C159E4, RTS)
{
    lda8(script_byte()); tax();
    lda8(script_byte()); tay();
    lda8(abs8(0x5D80 + g_cpu.X)); abs8_set(0x5D80 + g_cpu.Y, (u8)g_cpu.A);
    op_done(0x03);
}
/* C1:5769  Ends the fighter's script (4CF0) when A018+operand is negative. */
DC_ENTRY(C15769, RTS)
{
    lda8(script_byte()); tax();
    lda8(abs8(0xA018 + g_cpu.X));
    if (g_cpu._flag_N) { dc_C14CF0_glue(); return; }
    op_done(0x02);
}
/* C1:5FA5  Increments A1D8..A1DF. */
DC_ENTRY(C15FA5, RTS)
{
    for (int k = 7; k >= 0; k--) inc_abs(0xA1D8 + k);
    ldx(0xFFFF);
    lda8(0x01); script_advance();
}
/* C1:5DC1  Clears A3B9 + (command - 65). */
DC_ENTRY(C15DC1, RTS)
{
    lda8(abs8(0x9895)); sub_a(0x65);
    tax();
    abs8_set(0xA3B9 + g_cpu.X, 0);
    op_done(0x01);
}
/* C1:22D3  When a party slot holds character 6 (Magus), sets him up as
 * the target (9605/9607 = 19, 2701) and resets the target list. */
DC_ENTRY(C122D3, RTS)
{
    abs8_set(0x9604, 0);
    tdc(); tax();
    for (;;) {
        lda8(abs8(0x2980 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0x06);
        if (g_cpu._flag_Z) break;
        inx();
        cpx(0x0003);
        if (g_cpu._flag_Z) return;
    }
    lda8((u8)g_cpu.X); abs8_set(0x9605, (u8)g_cpu.A);
    lda8(0x19); abs8_set(0x9607, 0x19);
    dc_C12701_glue();
    dc_C127E8_glue();
}
/* Effect table FA61: chance effects (random 0..99 against $1C or 80). */
static int roll_under(u8 lim)                    /* AF22(100) <= lim */
{
    tdc(); tax();
    lda8(0x64);
    dc_C1AF22_glue();
    dc_cmp8((u8)g_cpu.A, lim);
    return g_cpu._flag_Z || !g_cpu._flag_C;
}
static void add_status(void)                     /* $16 = 1, $18 = $1E, DBAA */
{
    lda8(0x01); dp8_set(0x16, 0x01);
    lda8(dp8(0x1E)); dp8_set(0x18, (u8)g_cpu.A);
    dc_C1DBAA_glue();
}
/* C1:EEF7  Inflicts status $1E with chance $1C%. */
DC_ENTRY(C1EEF7, RTS)
{
    tdc(); tax(); stx_dp(0x16); stx_dp(0x18);
    if (roll_under(dp8(0x1C))) add_status();
}
/* C1:EED0  Same with an 80% chance, and only when the target's 5E47 has the bits $1C. */
DC_ENTRY(C1EED0, RTS)
{
    tdc(); tax(); stx_dp(0x16); stx_dp(0x18);
    if (!roll_under(0x50)) return;
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E47 + g_cpu.X) & dp8(0x1C));
    if (g_cpu._flag_Z) return;
    add_status();
}
/* C1:EF14  With chance $1C%: damage = target HP - 1 and the "hit" bit in B1FC. */
DC_ENTRY(C1EF14, RTS)
{
    if (!roll_under(dp8(0x1C))) return;
    ldx(abs_x(0xB1F6));
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)(abs16(0x5E30 + g_cpu.X) - 1)); abs16_set(0xAD89, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0xB1FC) | 0x01); abs8_set(0xB1FC, (u8)g_cpu.A);
}
/* C1:F095  Unless the target has 5E46 bit 7: with chance $1C%, damage = half its HP. */
DC_ENTRY(C1F095, RTS)
{
    tdc();
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E46 + g_cpu.X) & 0x80);
    if (!g_cpu._flag_Z) return;
    if (!roll_under(dp8(0x1C))) return;
    ldx(abs_x(0xB1F6));
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0x5E30 + g_cpu.X)); lsr_a(1); abs16_set(0xAD89, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:A508  Effect target = the living party member with the lowest HP.
 * The search starts from 7FFF with slot 0 picked (TDC; CMP #0 always
 * matches since D = 0), so slot 0's own HP is never compared. */
DC_ENTRY(C1A508, RTS)
{
    tdc();
    ldy(abs_x(0x5E30));
    dc_cmp8((u8)g_cpu.A, 0x00);
    if (g_cpu._flag_Z) ldy(0x7FFF);
    dp16_set(0x02, g_cpu.Y);
    abs8_set(0xAECC, 0);
    static const u16 hp[2] = { 0x5EB0, 0x5F30 };
    for (int k = 0; k < 2; k++) {
        ldy(abs_x(hp[k]));
        if (g_cpu._flag_Z) continue;
        cpy(dp16(0x02));
        if (!g_cpu._flag_C) { dp16_set(0x02, g_cpu.Y); lda8((u8)(k + 1)); abs8_set(0xAECC, (u8)(k + 1)); }
    }
    tdc();
    lda8(0x01); abs8_set(0xAECB, 0x01);
    dp8_set(0x03, 0);
}
/* C1:A6C3  Lists the enemy slots 3..10 that exist (AEFF+slot != FF),
 * skipping the acting one (B252 + 3); X = count. */
static void list_others(void)
{
    tdc(); dp8_set(0x0B, 0); tax();
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0xB252), 0x03)); dp8_set(0x0A, (u8)g_cpu.A);
    for (ldy(0x0003);;) {
        cpy(dp16(0x0A));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xAEFF + g_cpu.Y));
            if ((u8)g_cpu.A != 0xFF) { lda8((u8)g_cpu.Y); abs8_set(0xAECC + g_cpu.X, (u8)g_cpu.A); inx(); }
        }
        iny();
        cpy(0x000B);
        if (g_cpu._flag_Z) break;
    }
    lda8((u8)g_cpu.X);
}
DC_ENTRY(C1A6C3, RTS) { list_others(); abs8_set(0xAECB, (u8)g_cpu.A); }
/* C1:D490 / D4D6  Repeat for each of the AD8D targets: next target (E9B8),
 * then apply. */
DC_ENTRY(C1D490, RTS)
{
    dc_C1E9A3_glue();
    do {
        dc_C1E9B8_glue(); dc_C1D1D3_glue(); dc_C1DA37_glue(); dc_C1D508_glue();
        dec_abs(0xAD8D);
        lda8(abs8(0xAD8D));
    } while (!g_cpu._flag_Z);
    lda8(0x00); abs8_set(0xB200, 0);
}
DC_ENTRY(C1D4D6, RTS)
{
    dc_C1E9A3_glue();
    do {
        dc_C1E9B8_glue(); dc_C1D20A_glue(); dc_C1DBAA_glue();
        lda8(0x00); abs8_set(0xB200, 0);
        dc_C1D508_glue();
        dec_abs(0xAD8D);
        lda8(abs8(0xAD8D));
    } while (!g_cpu._flag_Z);
}

/* ---- batch 34 ---- */
/* Battle script "wait for the hit signal" ops: while the 16-bit timer is
 * running it counts down and the op repeats; then the fighter (A010; party
 * 0-3 via A014, 4 passes at once, 8+ the enemy in A2B6) raises its "waiting"
 * flag (A337 / A342) and the op ends when the signal (A19D / A1E0) arrives.
 * 7349 also clears the signal. */
static void wait_signal(u16 timer, int clear_signal)
{
    if (timer && (abs8(timer) | abs8(timer + 1))) {
        lda8(abs8(timer) | abs8(timer + 1));
        g_cpu._flag_C = 1;
        lda8(dc_sbc8(abs8(timer), 0x01)); abs8_set(timer, (u8)g_cpu.A);
        lda8(dc_sbc8(abs8(timer + 1), 0x00)); abs8_set(timer + 1, (u8)g_cpu.A);
        lda8(0x00); script_advance(); return;
    }
    if (timer) lda8(0);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    u16 flag, sig;
    if (g_cpu._flag_C) { ldx(abs_x(0xA2B6)); flag = 0xA342; sig = 0xA1E0; }
    else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (g_cpu._flag_Z) { lda8(0x01); script_advance(); return; }
        tax(); lda8(abs8(0xA014 + g_cpu.X)); tax();
        flag = 0xA337; sig = 0xA19D;
    }
    lda8(0x01); abs8_set(flag + g_cpu.X, 0x01);
    lda8(abs8(sig + g_cpu.X));
    if (g_cpu._flag_Z) { lda8(0x00); script_advance(); return; }
    abs8_set(flag + g_cpu.X, 0);
    if (clear_signal) abs8_set(sig + g_cpu.X, 0);
    lda8(0x01); script_advance();
}
DC_ENTRY(C17349, RTS) { wait_signal(0xAAFC, 1); }
DC_ENTRY(C17364, RTS) { wait_signal(0, 1); }
DC_ENTRY(C173AD, RTS) { wait_signal(0xAAFE, 0); }
/* C1:572C  Script "if 5D80+a == b": skips 3 bytes when equal, otherwise
 * stays (advance 0). */
DC_ENTRY(C1572C, RTS)
{
    lda8(script_byte()); tax();
    iny();
    lda8(abs8(0x5D80 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, bus8(lp(0xE6) + g_cpu.Y));
    if (!g_cpu._flag_Z) { tdc(); script_advance(); return; }
    op_done(0x03);
}
/* C1:5CF1 / 5D12  Clears / sets bit 7 of the party fighter's 9FF7 flags
 * (fighters 0-3 only). */
static void set_9ff7_bit7(u8 v)
{
    dp8_set(0x8E, v);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_C) {
            tax(); lda8(abs8(0xA014 + g_cpu.X)); tax();
            lda8((abs8(0x9FF7 + g_cpu.X) & 0x7F) | dp8(0x8E));
            abs8_set(0x9FF7 + g_cpu.X, (u8)g_cpu.A);
        }
    }
    lda8(0x01); script_advance();
}
DC_ENTRY(C15CF1, RTS) { set_9ff7_bit7(0x00); }
DC_ENTRY(C15D12, RTS) { lda8(0x80); set_9ff7_bit7(0x80); }
/* C1:22A4  When a party slot holds character 3 (Robo), makes him the target
 * (9607 = 19 for command 14, else 10) and resets the target list. */
DC_ENTRY(C122A4, RTS)
{
    abs8_set(0x9604, 0);
    tdc(); tax();
    for (;;) {
        lda8(abs8(0x2980 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (g_cpu._flag_Z) break;
        inx();
        cpx(0x0003);
        if (g_cpu._flag_Z) return;
    }
    lda8((u8)g_cpu.X); abs8_set(0x9605, (u8)g_cpu.A);
    lda8(abs8(0x960D) & 0x7F);
    dc_cmp8((u8)g_cpu.A, 0x14);
    lda8(g_cpu._flag_Z ? 0x19 : 0x10);
    abs8_set(0x9607, (u8)g_cpu.A);
    dc_C12701_glue();
    dc_C127E8_glue();
}
/* C1:9C6F  Animation op: the target enemy (B18B) leaves the battle: flag
 * bit 7 in its object, AEFF+slot = FF, AEE3 = 7, B3B8 = 1. */
DC_ENTRY(C19C6F, RTS)
{
    ldx(abs_x(0xB1D2));
    lda8(abs8(0xB252)); asl_a(2); tay();
    lda8(bus8(0xCC0001 + g_cpu.X)); abs8_set(0xAEE4, (u8)g_cpu.A);
    lda8(0x02); abs8_set(0xAEE3, 0x02);
    dc_C1ACF2_glue();
    tdc();
    lda8(abs8(0xB18B)); asl_a(1); tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xFDA80B + g_cpu.X)); tax(); tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0x001D + g_cpu.X) | 0x80); abs8_set(0x001D + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xB18B)); tax();
    lda8(0xFF); abs8_set(0xAEFF + g_cpu.X, 0xFF);
    lda8(0x07); abs8_set(0xAEE3, 0x07);
    lda8(0x01); abs8_set(0xB3B8, 0x01);
}
/* C1:9981  AF24 = 2 when no enemy slot 0-7 is "present but not shown"
 * (AF0D != FF, AF02 == FF, AF15 bit 7 clear). */
DC_ENTRY(C19981, RTS)
{
    tdc(); tax(); stx_dp(0x10); stx_dp(0x0C);
    do {
        ldx(dp_x(0x10));
        if (abs8(0xAF0D + g_cpu.X) != 0xFF && abs8(0xAF02 + g_cpu.X) == 0xFF && !(abs8(0xAF15 + g_cpu.X) & 0x80))
            dp8_set(0x0C, (u8)(dp8(0x0C) + 1));
        dp8_set(0x10, (u8)(dp8(0x10) + 1));
        lda8(dp8(0x10));
        dc_cmp8((u8)g_cpu.A, 0x08);
    } while (!g_cpu._flag_C);
    lda8(dp8(0x0C));
    dc_cmp8((u8)g_cpu.A, 0x00);
    if (g_cpu._flag_Z) { lda8(0x02); abs8_set(0xAF24, 0x02); }
}
/* C1:F0D9  Damage = user HP / 10 * AD89 / $1E (C92A divides, C90B multiplies). */
DC_ENTRY(C1F0D9, RTS)
{
    tdc();
    ldx(abs_x(0xB1F4));
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0x5E30 + g_cpu.X)); dp16_set(0x28, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(0x000A); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x32)); stx_dp(0x28);
    ldx(abs_x(0xAD89)); stx_dp(0x2A);
    dc_C1C90B_glue();
    ldx(dp_x(0x2C)); stx_dp(0x28);
    lda8(dp8(0x1E)); tax(); stx_dp(0x2A);
    dc_C1C92A_glue();
    ldx(dp_x(0x2C)); stx_abs(0xAD89);
}
/* C1:D3F4  Effect that damages the target by the user's HP: D221, AD89 =
 * user HP, EBF8 for the target, then queues the damage number (E89F). */
DC_ENTRY(C1D3F4, RTS)
{
    dc_C1D221_glue();
    ldx(abs_x(0xB1F4));
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0x5E30 + g_cpu.X)); abs16_set(0xAD89, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0xB18B)); tay();
    lda8(0x00); abs8_set(0xB202, 0);
    dc_C1EBF8_glue();
    abs8_set(0xAD9B, 0);
    dc_C1E89F_glue();
    lda8(dp8(0x0E)); tax();
    lda8(abs8(0xAD89)); abs8_set(0xAD9C + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xAD8A)); abs8_set(0xAD9D + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xAD9E + g_cpu.X, 0x01);
    lda8(0x00); abs8_set(0xB200, 0);
}

/* ---- batch 35 ---- */
/* C1:5D93  Script ops 61-64: starts timer n = command - 61 with the three
 * operands (A3C5/A3C9/A3CD + n), on (A3C1/A3B9 = 1, A3BD = FF). */
DC_ENTRY(C15D93, RTS)
{
    lda8(abs8(0x9895)); sub_a(0x61); tax();
    lda8(script_byte()); abs8_set(0xA3C5 + g_cpu.X, (u8)g_cpu.A);
    lda8(script_byte()); abs8_set(0xA3C9 + g_cpu.X, (u8)g_cpu.A);
    lda8(script_byte()); abs8_set(0xA3CD + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xA3C1 + g_cpu.X, 0x01); abs8_set(0xA3B9 + g_cpu.X, 0x01);
    lda8(0xFF); abs8_set(0xA3BD + g_cpu.X, 0xFF);
    op_done(0x04);
}
/* C1:EE9F  Random status from a 1..100 roll (E76E): <40 bit 04, <65 bit 80,
 * <90 bit 02, else 40; applied with DBAA. */
DC_ENTRY(C1EE9F, RTS)
{
    lda8(0x64); dp8_set(0x1A, 0x64);
    dc_C1E76E_glue();
    lda8(0x01); dp8_set(0x16, 0x01);
    lda8(dp8(0x1A));
    static const struct { u8 lim, bit; } t[4] = { { 0x28, 0x04 }, { 0x41, 0x80 }, { 0x5A, 0x02 }, { 0x64, 0x40 } };
    for (int k = 0; k < 4; k++) {
        dc_cmp8((u8)g_cpu.A, t[k].lim);
        if (!g_cpu._flag_C) { lda8(t[k].bit); break; }
    }
    dp8_set(0x18, (u8)g_cpu.A);
    dc_C1DBAA_glue();
}
/* C1:5A3E  5D9E/5D9F = the fighter's grid position (party: 1D0C/1D23 by
 * object, enemy: A240/A250). */
DC_ENTRY(C15A3E, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        tax(); lda8(abs8(0xA014 + g_cpu.X)); tax(); stx_dp(0x82);
        lda8(abs8(0x1D0C + g_cpu.X)); abs8_set(0x5D9E, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.X)); abs8_set(0x5D9F, (u8)g_cpu.A);
    } else {
        lda8(abs8(0xA2B6)); asl_a(1); tax();
        lda8(abs8(0xA240 + g_cpu.X)); abs8_set(0x5D9E, (u8)g_cpu.A);
        lda8(abs8(0xA250 + g_cpu.X)); abs8_set(0x5D9F, (u8)g_cpu.A);
    }
    lda8(0x01); script_advance();
}
/* C1:621B  Screen effect op: operand byte = selector (high nibble) | length n; the n bytes from
 * the operand on go to dp53.. and CD:001B (D1:F67A) runs the effect; advance n + 1. */
DC_ENTRY(C1621B, RTS)
{
    lda8(script_byte() & 0x0F); dp8_set(0x80, (u8)g_cpu.A); dp8_set(0x81, (u8)g_cpu.A);
    tdc(); tax();
    do {
        lda8(bus8(lp(0xE6) + g_cpu.Y)); b0_8_set((u16)(g_cpu.D + 0x53 + g_cpu.X), (u8)g_cpu.A);
        iny(); inx();
        u8 n = (u8)(dp8(0x80) - 1); dp8_set(0x80, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
    dc_jsl(0xCD001B, 0xC16230);
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0x81), 0x01));
    script_advance();
}
/* C1:623C  Script ops 43-80 / 81+: A4E8 + (command - 43 or - 81) = operand + 1. */
DC_ENTRY(C1623C, RTS)
{
    lda8(abs8(0x9895));
    dc_cmp8((u8)g_cpu.A, 0x81);
    sub_a(g_cpu._flag_C ? 0x81 : 0x43);
    tax();
    lda8(script_byte());
    lda8((u8)(g_cpu.A + 1)); abs8_set(0xA4E8 + g_cpu.X, (u8)g_cpu.A);
    lda8(0x02); script_advance();
}
/* C1:5694  Enemy only: its target A437 = party member (operand 0-8, via A014)
 * or enemy (operand - 9) | 40. */
DC_ENTRY(C15694, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        lda8(script_byte());
        dc_cmp8((u8)g_cpu.A, 0x09);
        if (!g_cpu._flag_C) { tax(); lda8(abs8(0xA014 + g_cpu.X)); }
        else { sub_a(0x09); lda8((u8)g_cpu.A | 0x40); }
        ldx(abs_x(0xA2B6));
        abs8_set(0xA437 + g_cpu.X, (u8)g_cpu.A);
    }
    op_done(0x02);
}
/* C1:587D  Clears A4A4/A4AF for the party fighter, or (fighter 4) for every
 * fighter listed in A018.. up to a negative entry. */
DC_ENTRY(C1587D, RTS)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax(); lda8(abs8(0xA014 + g_cpu.X)); tax();
            abs8_set(0xA4A4 + g_cpu.X, 0); abs8_set(0xA4AF + g_cpu.X, 0);
        } else {
            tdc(); tay();
            for (;;) {
                lda8(abs8(0xA018 + g_cpu.Y));
                if (g_cpu._flag_N) break;
                tax();
                abs8_set(0xA4A4 + g_cpu.X, 0); abs8_set(0xA4AF + g_cpu.X, 0);
                iny();
            }
        }
    }
    lda8(0x01); script_advance();
}
/* C1:605F  The enemy (A2B6) targets the party member given by the operand:
 * AB4F+enemy = it, and A3DF + enemy*11 + member = enemy | 40. */
DC_ENTRY(C1605F, RTS)
{
    lda8(0x40); dp8_set(0x8E, 0x40);
    lda8(script_byte()); tax();
    lda8(abs8(0xA014 + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0xA2B6)); dp8_set(0xAD, (u8)g_cpu.A); tax();
    lda8(dp8(0x80)); abs8_set(0xAB4F + g_cpu.X, (u8)g_cpu.A);
    lda8(0x0B); dp8_set(0xAE, 0x0B);
    dc_C10089_glue();
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0xAF), dp8(0x80))); tax();
    lda8((abs8(0xA2B6) & 0x3F) | dp8(0x8E));
    abs8_set(0xA3DF + g_cpu.X, (u8)g_cpu.A);
    lda8(0x02); script_advance();
}
/* C1:5A70  5D80+a += b (signed), clamped to 0..FF. */
DC_ENTRY(C15A70, RTS)
{
    lda8(script_byte()); tax();
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    if (!g_cpu._flag_N) {
        lda8(dc_adc8((u8)g_cpu.A, abs8(0x5D80 + g_cpu.X)));
        if (g_cpu._flag_C) lda8(0xFF);
    } else {
        lda8(dc_adc8(dp8(0x80), abs8(0x5D80 + g_cpu.X)));
        if (!g_cpu._flag_C) tdc();
    }
    abs8_set(0x5D80 + g_cpu.X, (u8)g_cpu.A);
    op_done(0x03);
}
/* C1:D4AD  For each target: D1F9, DB5E, and when $16 has bit 6 also D1D3/DA37. */
DC_ENTRY(C1D4AD, RTS)
{
    dc_C1E9A3_glue();
    do {
        dc_C1E9B8_glue(); dc_C1D1F9_glue(); dc_C1DB5E_glue();
        lda8(dp8(0x16) & 0x40);
        if (!g_cpu._flag_Z) { dc_C1D1D3_glue(); dc_C1DA37_glue(); }
        dc_C1D508_glue();
        dec_abs(0xAD8D);
        lda8(abs8(0xAD8D));
    } while (!g_cpu._flag_Z);
    lda8(0x00); abs8_set(0xB200, 0);
}

/* ---- batch 36 ---- */
/* C1:5EDF  AB4B = operand, AB4A = AB4E = 1; when 99D2 is set, A08B goes up
 * if any listed fighter (A017..) with A08C has 993C = 8. */
DC_ENTRY(C15EDF, RTS)
{
    lda8(script_byte()); abs8_set(0xAB4B, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xAB4A, 0x01); abs8_set(0xAB4E, 0x01);
    lda8(abs8(0x99D2));
    if (!g_cpu._flag_Z) {
        tdc(); tax();
        for (;;) {
            lda8(abs8(0xA017 + g_cpu.X));
            if (g_cpu._flag_N) break;
            tay();
            lda8(abs8(0xA08C + g_cpu.Y));
            if (!g_cpu._flag_Z) {
                lda8(abs8(0x993C + g_cpu.Y));
                dc_cmp8((u8)g_cpu.A, 0x08);
                if (g_cpu._flag_Z) { inc_abs(0xA08B); break; }
            }
            inx();
        }
    }
    lda8(0x02); script_advance();
}
/* The script's fighter (A010): a party member (via A014), or with A010 = 4
 * every fighter in the A018.. list; enemies (8+) are skipped. */
static void each_party_fighter(void (*f)(void))
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) return;
    dc_cmp8((u8)g_cpu.A, 0x04);
    if (!g_cpu._flag_Z) { tax(); lda8(abs8(0xA014 + g_cpu.X)); tax(); f(); return; }
    tdc(); tay();
    for (;;) {
        lda8(abs8(0xA018 + g_cpu.Y));
        if (g_cpu._flag_N) return;
        tax(); f();
        iny();
    }
}
/* C1:58AB  Revives: 96F5+fighter += 1, A0A8 cleared. */
static void revive_x(void) { inc_abs(0x96F5 + g_cpu.X); abs8_set(0xA0A8 + g_cpu.X, 0); }
DC_ENTRY(C158AB, RTS) { each_party_fighter(revive_x); op_done(0x01); }
/* C1:5844 / 5879  9FF7 flags = 1 / 0 (the list form keeps bit 7). */
static int s_single;
static void flag_9ff7(void)
{
    if (s_single) lda8(dp8(0x8E));
    else lda8((abs8(0x9FF7 + g_cpu.X) & 0x80) | dp8(0x8E));
    abs8_set(0x9FF7 + g_cpu.X, (u8)g_cpu.A);
}
static void set_9ff7(u8 v)
{
    dp8_set(0x8E, v);
    s_single = abs8(0xA010) != 0x04;
    each_party_fighter(flag_9ff7);
    lda8(0x01); script_advance();
}
DC_ENTRY(C15844, RTS) { lda8(0x01); set_9ff7(0x01); }
DC_ENTRY(C15879, RTS) { set_9ff7(0x00); }
/* C1:A765 / AA61  Effect target = the enemy (slots 3-10) with the lowest
 * nonzero HP (object field +3); AA61 skips the acting enemy B18B. */
static void lowest_enemy(int skip_self)
{
    ldy(0xFFFF); dp16_set(0x02, g_cpu.Y);
    tdc();
    lda8(0x03); dp8_set(0x0E, 0x03);
    do {
        lda8(dp8(0x0E)); tax();
        lda8(abs8(0xAEFF + g_cpu.X));
        int ok = (u8)g_cpu.A != 0xFF;
        if (ok && skip_self) { lda8((u8)g_cpu.X); dc_cmp8((u8)g_cpu.A, abs8(0xB18B)); ok = !g_cpu._flag_Z; }
        if (ok) {
            lda8(dp8(0x0E)); asl_a(1); tax();
            dc_set_mx(0, g_cpu.x_flag);
            lda16(bus16(0xFDA80B + g_cpu.X)); tax();
            ldy(abs_x((u16)(0x0003 + g_cpu.X)));
            if (!g_cpu._flag_Z) {
                cpy(dp16(0x02));
                if (!g_cpu._flag_C) {
                    dp16_set(0x02, g_cpu.Y);
                    dc_set_mx(1, g_cpu.x_flag);
                    lda8(dp8(0x0E)); abs8_set(0xAECC, (u8)g_cpu.A);
                }
            }
        }
        dc_set_mx(1, g_cpu.x_flag);
        tdc();
        dp8_set(0x0E, (u8)(dp8(0x0E) + 1));
        lda8(dp8(0x0E));
        dc_cmp8((u8)g_cpu.A, 0x0B);
    } while (!g_cpu._flag_C);
    lda8(0x01); abs8_set(0xAECB, 0x01);
    dp8_set(0x03, 0);
}
DC_ENTRY(C1A765, RTS) { lowest_enemy(0); }
DC_ENTRY(C1AA61, RTS) { lowest_enemy(1); }

/* ---- batch 37 ---- */
/* C1:F036  Effect: with $1C bit 7, heals the target by its max HP (AD89 =
 * 5E32, damage number negative); with bit 6, clears its status bytes
 * (5E4A keeps C0, 5E4B = 0, 5E4C keeps C2). */
DC_ENTRY(C1F036, RTS)
{
    lda8(dp8(0x1C));
    if (g_cpu.A & 0x80) {
        ldx(abs_x(0xB1F6));
        dc_set_mx(0, g_cpu.x_flag);
        lda16(abs16(0x5E32 + g_cpu.X)); abs16_set(0xAD89, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0xB1FD)); tay();
        lda8(0x80); abs8_set(0xB202, 0x80);
        dc_C1EBF8_glue();
        abs8_set(0xAD9B, 0);
        dc_C1E89F_glue();
        lda8(dp8(0x0E)); tax();
        dc_set_mx(0, g_cpu.x_flag);
        lda16((u16)-abs16(0xAD89)); abs16_set(0xAD9C + g_cpu.X, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(0x01); abs8_set(0xAD9E + g_cpu.X, 0x01);
    }
    lda8(dp8(0x1C));
    g_cpu._flag_Z = !(g_cpu.A & 0x40);
    if (g_cpu._flag_Z) return;
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E4A + g_cpu.X) & 0xC0); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
    lda8(0);                             abs8_set(0x5E4B + g_cpu.X, 0);
    lda8(abs8(0x5E4C + g_cpu.X) & 0xC2); abs8_set(0x5E4C + g_cpu.X, (u8)g_cpu.A);
}
/* C1:5806 / 5840  A5CD flags of the party fighter(s) = 1 / 0, bit 7 kept. */
static void flag_a5cd(void)
{
    lda8((abs8(0xA5CD + g_cpu.X) & 0x80) | dp8(0x8E));
    abs8_set(0xA5CD + g_cpu.X, (u8)g_cpu.A);
}
DC_ENTRY(C15806, RTS) { lda8(0x01); dp8_set(0x8E, 0x01); each_party_fighter(flag_a5cd); lda8(0x01); script_advance(); }
DC_ENTRY(C15840, RTS) { dp8_set(0x8E, 0x00); each_party_fighter(flag_a5cd); lda8(0x01); script_advance(); }
/* C1:ABC9  Effect target = another enemy at random: the first of the list
 * (A6C3) is replaced by a random pick when there are 2 or more (only AECC+1
 * is cleared to FF: the clear loop ends after one pass). */
DC_ENTRY(C1ABC9, RTS)
{
    list_others();
    dc_cmp8((u8)g_cpu.A, 0x02);
    if (g_cpu._flag_C) {
        ldx(0x0000);
        dc_C1AF22_glue();
        tax();
        lda8(abs8(0xAECC + g_cpu.X)); abs8_set(0xAECC, (u8)g_cpu.A);
        lda8(0xFF);
        ldx(0x0001);
        abs8_set(0xAECC + g_cpu.X, 0xFF); inx();
        cpx(0x0000);
        lda8(0x01);
    }
    abs8_set(0xAECB, (u8)g_cpu.A);
}
/* C1:AB03  Effect target = a random enemy slot 0-7 with AF15 bit 7 (AECB = 0
 * when there is none). */
DC_ENTRY(C1AB03, RTS)
{
    tdc(); tax(); tay();
    do {
        lda8(abs8(0xAF15 + g_cpu.X));
        if (g_cpu.A & 0x80) { lda8((u8)g_cpu.X); abs8_set(0xAECC + g_cpu.Y, (u8)g_cpu.A); iny(); }
        inx();
        cpx(0x0008);
    } while (!g_cpu._flag_C);
    cpy(0x0000);
    if (g_cpu._flag_Z) { tdc(); abs8_set(0xAECB, (u8)g_cpu.A); return; }
    lda8((u8)g_cpu.Y); abs8_set(0xAECB, (u8)g_cpu.A);
    dc_cmp8((u8)g_cpu.A, 0x02);
    if (!g_cpu._flag_C) return;
    tdc(); tax();
    lda8(abs8(0xAECB));
    dc_C1AF22_glue();
    tax();
    lda8(abs8(0xAECC + g_cpu.X)); abs8_set(0xAECC, (u8)g_cpu.A);
    ldx(0x0001);
    lda8(0xFF);
    do { abs8_set(0xAECC + g_cpu.X, 0xFF); inx(); cpx(0x000B); } while (!g_cpu._flag_C);
    lda8(0x01); abs8_set(0xAECB, 0x01);
}

/* ---- batch 38 ---- */
/* C1:753A  Sets the fighter(s)' 99B5 (enemy: AB5F) to the operand. */
static void put_80_99b5(void) { lda8(dp8(0x80)); abs8_set(0x99B5 + g_cpu.Y, (u8)g_cpu.A); }
static void put_9f3b_99b5(void) { lda8(abs8(0x9F3B + g_cpu.Y)); abs8_set(0x99B5 + g_cpu.Y, (u8)g_cpu.A); }
/* Party fighter / list (index in Y for the list form, X for the single
 * one; the list walks X) / enemy, as 753A and 7579 do it. */
static void fighter_99b5(void (*f)(void), u16 enemy_src)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldx(abs_x(0xA2B6));
        lda8(enemy_src ? abs8(enemy_src + g_cpu.X) : dp8(0x80));
        abs8_set(0xAB5F + g_cpu.X, (u8)g_cpu.A);
        return;
    }
    dc_cmp8((u8)g_cpu.A, 0x04);
    if (!g_cpu._flag_Z) {
        tax(); lda8(abs8(0xA014 + g_cpu.X)); tax();
        u16 y = g_cpu.Y; g_cpu.Y = g_cpu.X; f(); g_cpu.Y = y;
        return;
    }
    tdc(); tax();
    for (;;) {
        lda8(abs8(0xA018 + g_cpu.X));
        if (g_cpu._flag_N) return;
        tay(); f();
        inx();
    }
}
DC_ENTRY(C1753A, RTS)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    fighter_99b5(put_80_99b5, 0);
    lda8(0x02); script_advance();
}
/* C1:7579  Restores 99B5 from 9F3B (enemy: AB5F from AB57); reads the
 * operand into $80 but moves on by 1 only. */
DC_ENTRY(C17579, RTS)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    fighter_99b5(put_9f3b_99b5, 0xAB57);
    lda8(0x01); script_advance();
}
/* C1:373B  Per-frame enemy movement for enemy $9C: refreshes 9752 from
 * 5E15 and runs its movement type (974A -> CC:FBAB -> table 3760). */
DC_ENTRY(C1373B, RTS)
{
    static void (*const move[9])(void) = {
        dc_C13772_glue, dc_C1379A_glue, dc_C138BF_glue, dc_C13959_glue, dc_C13A3D_glue,
        dc_C13BC2_glue, dc_C13E33_glue, dc_C13F5A_glue, dc_C14021_glue };
    ldx(dp_x(0x9C));
    lda8(abs8(0x96F8 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    lda8(abs8(0x5E15 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, abs8(0x9752 + g_cpu.X));
    if (!g_cpu._flag_Z) abs8_set(0x9752 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x974A + g_cpu.X));
    if (g_cpu._flag_N) return;
    abs8_set(0xA2B5, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xCCFBAB + g_cpu.X)); asl_a(1); tax();
    if (g_cpu.X < 18) move[g_cpu.X >> 1]();
    else dc_jsr(0xC10000 | bus16(0xC13760 + g_cpu.X), 0xC1375C);
}

/* ---- batch 39 ---- */
/* C1:49FF  Can party member AE91 reach the fighter A017? Positions to
 * D3..D6, distance (2AE3; 9872 < 0 = too far), facing toward it (0222 ->
 * C0:F700 into 96DB), then, for characters that must walk (CC:F63C) or
 * when in range, a path search on the grid (4A71; $92 < 0 = blocked).
 * 9930 = 1 when reachable. */
DC_ENTRY(C149FF, RTS)
{
    lda8(0x01); abs8_set(0x9930, 0x01);
    lda8(abs8(0xAE91)); abs8_set(0x986F, (u8)g_cpu.A); tax();
    lda8(abs8(0xA017)); abs8_set(0x9870, (u8)g_cpu.A); tay();
    dp8_set(0xD3, abs8(0x1D0C + g_cpu.X)); dp8_set(0xD4, abs8(0x1D23 + g_cpu.X));
    dp8_set(0xD5, abs8(0x1D0C + g_cpu.Y)); lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD6, (u8)g_cpu.A);
    dc_C12AE3_glue();
    lda8(abs8(0x9872));
    if (g_cpu._flag_N) abs8_set(0x9930, 0);
    lda8(abs8(0xAE91)); tay();
    dc_C10222_glue();
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X)); abs8_set(0x96DB + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x2980 + g_cpu.Y)); tax();
    lda8(bus8(0xCCF63C + g_cpu.X));
    if (g_cpu._flag_Z) {
        lda8(abs8(0x9930));
        if (g_cpu._flag_Z) return;
    }
    for (int k = 0; k < 4; k++) dp8_set(0x80 + k, dp8(0xD3 + k));
    lda8(dp8(0xD6));
    dc_C14A71_glue();
    lda8(dp8(0x92));
    if (g_cpu._flag_N) { abs8_set(0x9930, 0); return; }
    lda8(0x01); abs8_set(0x9930, 0x01);
}
/* C1:D371  For each target: its bits from D14F; bit 7 -> E77B with $1A = 3
 * and AD9B + 1, bit 6 -> E77B with $1A = 7, $16 = 14. $20 = the first $16. */
DC_ENTRY(C1D371, RTS)
{
    dc_C1D14F_glue();
    lda8(dp8(0x16)); dp8_set(0x20, (u8)g_cpu.A);
    dc_C1E9A3_glue();
    do {
        dc_C1E9B8_glue(); dc_C1D14F_glue();
        lda8(dp8(0x16)); dp8_set(0x1E, (u8)g_cpu.A);
        tdc(); abs8_set(0xAD9B, (u8)g_cpu.A);
        lda8(dp8(0x1E) & 0x80);
        if (!g_cpu._flag_Z) {
            lda8(0x03); dp8_set(0x1A, 0x03);
            dc_C1E77B_glue();
            lda8((u8)(abs8(0xAD9B) + 1)); abs8_set(0xAD9B, (u8)g_cpu.A);
        }
        lda8(dp8(0x1E) & 0x40);
        if (!g_cpu._flag_Z) {
            lda8(0x07); dp8_set(0x1A, 0x07);
            lda8(0x14); dp8_set(0x16, 0x14);
            dc_C1E77B_glue();
        }
        dc_C1D508_glue();
        dec_abs(0xAD8D);
        lda8(abs8(0xAD8D));
    } while (!g_cpu._flag_Z);
    lda8(0x00); abs8_set(0xB200, 0);
}
/* C1:5611  Puts the fighter(s) at grid position (a, b): 1D0C/1D23 for the
 * party, A240/A250 (16-bit) for the enemy. */
static void put_pos(void) { abs8_set(0x1D0C + g_cpu.X, dp8(0x80)); lda8(dp8(0x81)); abs8_set(0x1D23 + g_cpu.X, (u8)g_cpu.A); }
DC_ENTRY(C15611, RTS)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    lda8(script_byte()); dp8_set(0x81, (u8)g_cpu.A);
    lda8(0x03); dp8_set(0x8E, 0x03);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        lda8(abs8(0xA2B6)); asl_a(1); tax();
        lda8(dp8(0x80)); abs8_set(0xA240 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xA241 + g_cpu.X, 0);
        lda8(dp8(0x81)); abs8_set(0xA250 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xA251 + g_cpu.X, 0);
    } else each_party_fighter(put_pos);
    lda8(dp8(0x8E)); script_advance();
}
/* C1:6154  Adds the operand to the fighter(s)' A5AA (enemy: A5B5). The list
 * form walks A015.. (not A018). */
static void add_a5aa(void)
{
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0xA5AA + g_cpu.Y), dp8(0x80))); abs8_set(0xA5AA + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C16154, RTS)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldx(abs_x(0xA2B6));
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0xA5B5 + g_cpu.X), dp8(0x80))); abs8_set(0xA5B5 + g_cpu.X, (u8)g_cpu.A);
    } else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax(); lda8(abs8(0xA014 + g_cpu.X)); tax();
            u16 y = g_cpu.Y; g_cpu.Y = g_cpu.X; add_a5aa(); g_cpu.Y = y;
        } else {
            tdc(); tax();
            for (;;) {
                lda8(abs8(0xA015 + g_cpu.X));
                if (g_cpu._flag_N) break;
                tay(); add_a5aa();
                inx();
            }
        }
    }
    op_done(0x02);
}

/* ---- batch 40 ---- */
/* C1:3714  Each frame (unless 99E0): for every enemy with a movement type
 * whose countdown 9897 runs out, reload it from 989F and move it (373B). */
DC_ENTRY(C13714, RTS)
{
    lda8(abs8(0x99E0));
    if (!g_cpu._flag_Z) return;
    tdc(); tax(); stx_dp(0x9C);
    do {
        ldx(dp_x(0x9C));
        lda8(abs8(0x974A + g_cpu.X));
        if (!g_cpu._flag_Z) {
            dec_abs(0x9897 + g_cpu.X);
            if (g_cpu._flag_Z) {
                lda8(abs8(0x989F + g_cpu.X)); abs8_set(0x9897 + g_cpu.X, (u8)g_cpu.A);
                dc_C1373B_glue();
            }
        }
        dp8_set(0x9C, (u8)(dp8(0x9C) + 1));
        lda8(dp8(0x9C));
        dc_cmp8((u8)g_cpu.A, 0x08);
    } while (!g_cpu._flag_Z);
}
/* C1:691B / 69BA  Starts a party fighter's walk to (a, b) (69BA: (1, 0)):
 * target AB67/AB7D, flags AB72/AB88, the four waypoints AB93.. / AB9E..
 * start at its position, A192 = $8F. Enemies are skipped. */
static void walk_setup(void)
{
    lda8(dp8(0x80)); abs8_set(0xAB67 + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x81)); abs8_set(0xAB7D + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xAB72 + g_cpu.X, 0x01); abs8_set(0xAB88 + g_cpu.X, 0x01);
    lda8(abs8(0x1D0C + g_cpu.X));
    static const u16 px[4] = { 0xAB93, 0xABA9, 0xABBF, 0xABD5 }, py[4] = { 0xAB9E, 0xABB4, 0xABCA, 0xABE0 };
    for (int k = 0; k < 4; k++) abs8_set(px[k] + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x1D23 + g_cpu.X));
    for (int k = 0; k < 4; k++) abs8_set(py[k] + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x8F)); abs8_set(0xA192 + g_cpu.X, (u8)g_cpu.A);
}
static void walk_start(void)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (!g_cpu._flag_C) {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_C) { tax(); lda8(abs8(0xA014 + g_cpu.X)); tax(); walk_setup(); }
        else {
            tdc(); tay();
            for (;;) {
                lda8(abs8(0xA018 + g_cpu.Y));
                if (g_cpu._flag_N) break;
                tax(); walk_setup();
                iny();
            }
        }
    }
    lda8(dp8(0x8E)); script_advance();
}
DC_ENTRY(C1691B, RTS)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    lda8(script_byte()); dp8_set(0x81, (u8)g_cpu.A);
    lda8(0x03); dp8_set(0x8E, 0x03); dp8_set(0x8F, 0x03);
    walk_start();
}
DC_ENTRY(C169BA, RTS)
{
    lda8(0x01); dp8_set(0x8E, 0x01);
    dp8_set(0x8F, 0);
    walk_start();
}
/* C1:6F9A / 6FEF  Steps the fighter(s)' A4AF by b until it equals a (6FEF:
 * a = 0); the op repeats (advance 0) until every listed fighter got there.
 * A010 is read as a 16-bit index here. */
static void step_a4af(void)
{
    ldx(abs_x(0xA010));
    cpx(0x0008);
    if (g_cpu._flag_C) { lda8(dp8(0x8E)); script_advance(); return; }
    cpx(0x0004);
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xA014 + g_cpu.X)); tax();
        lda8(abs8(0xA4AF + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, dp8(0x80));
        if (g_cpu._flag_Z) { lda8(dp8(0x8E)); script_advance(); return; }
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, dp8(0x81))); abs8_set(0xA4AF + g_cpu.X, (u8)g_cpu.A);
    } else {
        tdc(); tay();
        for (;;) {
            lda8(abs8(0xA018 + g_cpu.Y));
            if (g_cpu._flag_N) break;
            tax();
            lda8(abs8(0xA4AF + g_cpu.X));
            dc_cmp8((u8)g_cpu.A, dp8(0x80));
            if (g_cpu._flag_Z) { lda8(dp8(0x8E)); script_advance(); return; }
            g_cpu._flag_C = 0;
            lda8(dc_adc8((u8)g_cpu.A, dp8(0x81))); abs8_set(0xA4AF + g_cpu.X, (u8)g_cpu.A);
            iny();
        }
    }
    tdc(); script_advance();
}
DC_ENTRY(C16F9A, RTS)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    lda8(script_byte()); dp8_set(0x81, (u8)g_cpu.A);
    lda8(0x03); dp8_set(0x8E, 0x03);
    step_a4af();
}
DC_ENTRY(C16FEF, RTS)
{
    lda8(script_byte()); dp8_set(0x81, (u8)g_cpu.A);
    dp8_set(0x80, 0);
    lda8(0x02); dp8_set(0x8E, 0x02);
    step_a4af();
}

/* ---- batch 41 ---- */
/* C1:6093  Copies a fighter's A5AA (operand 0-9 via A014; 10+ = enemy
 * A5B5 + operand - 10) to the script fighter(s) (list form via A015..). */
DC_ENTRY(C16093, RTS)
{
    lda8(script_byte());
    dc_cmp8((u8)g_cpu.A, 0x0A);
    if (!g_cpu._flag_C) { tax(); lda8(abs8(0xA014 + g_cpu.X)); tax(); lda8(abs8(0xA5AA + g_cpu.X)); }
    else { sub_a(0x0A); tax(); lda8(abs8(0xA5B5 + g_cpu.X)); }
    dp8_set(0x80, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) { ldx(abs_x(0xA2B6)); lda8(dp8(0x80)); abs8_set(0xA5B5 + g_cpu.X, (u8)g_cpu.A); }
    else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) { tax(); lda8(abs8(0xA014 + g_cpu.X)); tax(); lda8(dp8(0x80)); abs8_set(0xA5AA + g_cpu.X, (u8)g_cpu.A); }
        else {
            tdc(); tax();
            for (;;) {
                lda8(abs8(0xA015 + g_cpu.X));
                if (g_cpu._flag_N) break;
                tay(); lda8(dp8(0x80)); abs8_set(0xA5AA + g_cpu.Y, (u8)g_cpu.A);
                inx();
            }
        }
    }
    op_done(0x02);
}
/* C1:74B4  Starts a timed move for the party fighter(s): AB1E/AB34 = a/b,
 * AB13 = c + 1, flags AB29/AB3F/A2FE/A2EB = 1, A86D = 6, A337 = 1. */
static void timed_move(void)
{
    lda8(dp8(0x80)); abs8_set(0xAB1E + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x82)); abs8_set(0xAB34 + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x84)); abs8_set(0xAB13 + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01);
    abs8_set(0xAB29 + g_cpu.X, 0x01); abs8_set(0xAB3F + g_cpu.X, 0x01);
    abs8_set(0xA2FE + g_cpu.X, 0x01); abs8_set(0xA2EB + g_cpu.X, 0x01);
    lda8(0x06); abs8_set(0xA86D + g_cpu.X, 0x06);
    lda8(0x01); abs8_set(0xA337 + g_cpu.X, 0x01);
}
DC_ENTRY(C174B4, RTS)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    lda8(script_byte()); dp8_set(0x82, (u8)g_cpu.A);
    lda8(script_byte()); lda8((u8)(g_cpu.A + 1)); dp8_set(0x84, (u8)g_cpu.A);
    each_party_fighter(timed_move);
    lda8(0x04); script_advance();
}
/* C1:F9EB  After battle: for each of the 7 party slots flagged in 29AF
 * (bit 7 first) with B1BE bit 7 set, the character record 2600 + slot*50
 * gains B28C * 15 / 20 experience (F908 + level-ups F623); B28C is kept. */
DC_ENTRY(C1F9EB, RTS)
{
    ldx(0x0003); stx_abs(0xB1F4);
    tdc(); tax(); stx_abs(0xAE79); stx_dp(0x02);
    lda8(abs8(0x29AF)); abs8_set(0xAE79, (u8)g_cpu.A);
    do {
        lda8(abs8(0xAE79));
        if (g_cpu._flag_N) {
            ldx(dp_x(0x02));
            lda8(abs8(0xB1BE + g_cpu.X));
            if (g_cpu._flag_N) {
                stx_dp(0x2A);
                ldy(0x0050); if (g_cpu.x_flag) dp8_set(0x28, (u8)g_cpu.Y); else dp16_set(0x28, g_cpu.Y);
                dc_C1C90B_glue();
                dc_set_mx(0, g_cpu.x_flag);
                g_cpu._flag_C = 0;
                lda16(dc_adc16(dp16(0x2C), 0x2600)); abs16_set(0xB285, g_cpu.A);
                tdc();
                dc_set_mx(1, g_cpu.x_flag);
                ldx(abs_x(0xB28C)); phx(); stx_dp(0x28);
                lda8(0x0F); tax(); stx_dp(0x2A);
                dc_C1C90B_glue();
                ldx(dp_x(0x2C)); stx_dp(0x28);
                ldx(0x0014); stx_dp(0x2A);
                dc_C1C92A_glue();
                ldx(dp_x(0x2C)); stx_abs(0xB28C);
                ldx(abs_x(0xB285));
                dc_C1F908_glue();
                dc_C1F623_glue();
                plx(); stx_abs(0xB28C);
                tdc();
                dc_set_mx(1, g_cpu.x_flag);
            }
        }
        lda8(abs8(0xAE79)); asl_a(1); abs8_set(0xAE79, (u8)g_cpu.A);
        dp8_set(0x02, (u8)(dp8(0x02) + 1));
        lda8(dp8(0x02));
        dc_cmp8((u8)g_cpu.A, 0x07);
    } while (!g_cpu._flag_C);
}

/* ---- batch 42 ---- */
/* C1:F132  Effect: unless the target has 5E46 bit 7, with chance $1C% it
 * dies (HP = 0). Otherwise the target slot B1FD is taken out of the target
 * masks AE95/AE99 (their union saved in AE58) and the damage AD89 is 0.
 * Then, when AE95 is empty, the highest bit of AE99 moves to AE95; with
 * both empty AE94 gets bit 6. */
DC_ENTRY(C1F132, RTS)
{
    tdc();
    ldx(abs_x(0xB1F6));
    lda8(abs8(0x5E46 + g_cpu.X));
    int immune = (g_cpu.A & 0x80) != 0;
    if (!immune && roll_under(dp8(0x1C))) {
        tdc();
        ldx(abs_x(0xB1F6));
        abs8_set(0x5E30 + g_cpu.X, 0); abs8_set(0x5E31 + g_cpu.X, 0);
    } else {
        lda8(abs8(0xB1FD)); tax();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(abs16(0xAE95) | abs16(0xAE99)); abs16_set(0xAE58, g_cpu.A);
        u16 bit = 0x8000;
        for (u16 n = g_cpu.X; n; n--) bit >>= 1;
        ldx(0x0000);
        u16 mask = (u16)~bit;
        dp16_set(0x2C, mask);
        lda16(mask & abs16(0xAE95)); abs16_set(0xAE95, g_cpu.A);
        lda16(mask & abs16(0xAE99)); abs16_set(0xAE99, g_cpu.A);
        tdc(); tax(); stx_abs(0xAD89);
        dc_set_mx(1, g_cpu.x_flag);
    }
    ldx(abs_x(0xAE95));
    cpx(0x0000);
    if (g_cpu._flag_Z) {
        ldx(abs_x(0xAE99));
        cpx(0x0000);
        int found = 0;
        if (!g_cpu._flag_Z) {
            dc_set_mx(0, g_cpu.x_flag);
            for (u16 a = 0x8000; a; a >>= 1)
                if (a & abs16(0xAE99)) {
                    abs16_set(0xAE95, a);
                    lda16(a ^ abs16(0xAE99)); abs16_set(0xAE99, g_cpu.A);
                    found = 1; break;
                }
        }
        if (!found) {
            tdc();
            dc_set_mx(1, g_cpu.x_flag);
            lda8(abs8(0xAE94) | 0x40); abs8_set(0xAE94, (u8)g_cpu.A);
        }
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* C1:720E  Walk to the point AAF9/AAFA: a party fighter starts walking
 * (5135) with its wait flag cleared and AAFE scaled by its speed (7313);
 * an enemy starts its walk the same way (the speed for 7313 is read with
 * the doubled index, A32F + enemy*2). A010 = 4 does nothing. */
DC_ENTRY(C1720E, RTS)
{
    lda8(abs8(0xAAF9)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0xAAFA)); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldx(abs_x(0xA2B6));
        enemy_walk_setup();
        lda8(abs8(0xA32F + g_cpu.X));
        dc_C17313_glue();
    } else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax(); lda8(abs8(0xA014 + g_cpu.X)); tax();
            phx();
            dc_C15135_glue();
            plx();
            abs8_set(0xA337 + g_cpu.X, 0);
            lda8(abs8(0xA324 + g_cpu.X));
            dc_C17313_glue();
        }
    }
    lda8(0x01); script_advance();
}

/* ---- batch 44 ---- */
/* C1:51EE / 5205  Jump to a point: (c, d) from the operands (51EE) or 5D9E/
 * 5D9F (5205), with the arc height $82 from the first operand (FF = the
 * fighter's own A5AA / A5B5). Each fighter that is not already jumping gets
 * its angle (0222), the jump length (53B0) and the jump flags; the op
 * repeats (advance 0) until the fighters signal the landing (A19D / A1E0). */
struct jump_slots { u16 h, own, len, f1, f2, z1, z2, z3, gx, gy, busy; };
static const struct jump_slots jump_party = { 0xAC11, 0xA5AA, 0xAC37, 0xA2EB, 0xA86D, 0xA311, 0xA19D, 0xAC24, 0xA383, 0xA396, 0xA337 };
static const struct jump_slots jump_enemy = { 0xAC1C, 0xA5B5, 0xAC42, 0xA2F6, 0xA878, 0xA31C, 0xA1E0, 0xAC2F, 0xA38E, 0xA3A1, 0xA342 };
static void jump_setup(const struct jump_slots *j)   /* Y = fighter, D3/D4 set */
{
    lda8(abs8(0xA2B0)); dp8_set(0xD5, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); dp8_set(0xD6, (u8)g_cpu.A);
    dc_C10222_glue();
    lda8(dp8(0x82));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) lda8(abs8(j->own + g_cpu.Y));
    abs8_set(j->h + g_cpu.Y, (u8)g_cpu.A);
    dp8_set(0x82, (u8)g_cpu.A);
    dc_C153B0_glue();
    abs8_set(j->len + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(j->f1 + g_cpu.Y, 0x01);
    lda8(0x07); abs8_set(j->f2 + g_cpu.Y, 0x07);
    tdc();
    abs8_set(j->z1 + g_cpu.Y, (u8)g_cpu.A); abs8_set(j->z2 + g_cpu.Y, (u8)g_cpu.A); abs8_set(j->z3 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B0)); abs8_set(j->gx + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); abs8_set(j->gy + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(j->busy + g_cpu.Y, 0x01);
}
static void party_d3(void) { lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0xD3, (u8)g_cpu.A); lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0xD4, (u8)g_cpu.A); }
static void jump_to_point(void)
{
    int done = 0;
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldy(abs_x(0xA2B6));
        lda8(abs8(0xA342 + g_cpu.Y));
        if (g_cpu._flag_Z) {
            lda8((u8)g_cpu.Y); asl_a(1); tax();
            lda8(abs8(0xA240 + g_cpu.X)); dp8_set(0xD3, (u8)g_cpu.A);
            lda8(abs8(0xA250 + g_cpu.X)); dp8_set(0xD4, (u8)g_cpu.A);
            abs8_set(0xA241 + g_cpu.X, 0); abs8_set(0xA251 + g_cpu.X, 0);
            jump_setup(&jump_enemy);
        } else {
            ldx(abs_x(0xA2B6));
            lda8(abs8(0xA1E0 + g_cpu.X));
            if (!g_cpu._flag_Z) { abs8_set(0xA342 + g_cpu.X, 0); done = 1; }
        }
    } else {
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (!g_cpu._flag_Z) {
            tax(); lda8(abs8(0xA014 + g_cpu.X)); tay();
            lda8(abs8(0xA337 + g_cpu.Y));
            if (g_cpu._flag_Z) { party_d3(); jump_setup(&jump_party); }
            else {
                lda8(abs8(0xA19D + g_cpu.Y));
                if (!g_cpu._flag_Z) { tdc(); abs8_set(0xA337 + g_cpu.Y, (u8)g_cpu.A); done = 1; }
            }
        } else {
            tdc(); tax(); stx_dp(0x80);
            for (;;) {
                ldx(dp_x(0x80));
                lda8(abs8(0xA018 + g_cpu.X));
                if (g_cpu._flag_N) break;
                tay();
                lda8(abs8(0xA337 + g_cpu.Y));
                if (!g_cpu._flag_Z) {
                    /* someone is already jumping: the rest of the list lands */
                    dp8_set(0x84, 0);
                    for (;;) {
                        ldx(dp_x(0x80));
                        lda8(abs8(0xA018 + g_cpu.X));
                        if (g_cpu._flag_N) break;
                        tay();
                        lda8(abs8(0xA19D + g_cpu.Y));
                        if (!g_cpu._flag_Z) { tdc(); abs8_set(0xA337 + g_cpu.Y, (u8)g_cpu.A); }
                        else dp8_set(0x84, (u8)(dp8(0x84) + 1));
                        dp8_set(0x80, (u8)(dp8(0x80) + 1));
                    }
                    lda8(dp8(0x84));
                    done = g_cpu._flag_Z;
                    break;
                }
                party_d3(); jump_setup(&jump_party);
                dp8_set(0x80, (u8)(dp8(0x80) + 1));
            }
        }
    }
    if (done) lda8(dp8(0x8E)); else lda8(0x00);
    script_advance();
}
DC_ENTRY(C151EE, RTS)
{
    lda8(script_byte()); dp8_set(0x82, (u8)g_cpu.A);
    lda8(script_byte()); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(script_byte()); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x04); dp8_set(0x8E, 0x04);
    jump_to_point();
}
DC_ENTRY(C15205, RTS)
{
    lda8(script_byte()); dp8_set(0x82, (u8)g_cpu.A);
    lda8(abs8(0x5D9E)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x5D9F)); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x02); dp8_set(0x8E, 0x02);
    jump_to_point();
}

/* ---- batch 45 ---- */
static void sty_dp84(void) { if (g_cpu.x_flag) dp8_set(0x84, (u8)g_cpu.Y); else dp16_set(0x84, g_cpu.Y); }
/* Y = $82 + offset table[X]; returns 0 when that map cell is FF (A = the cell). */
static int path_cell(u32 tab)
{
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0x82), bus8(tab + g_cpu.X))); tay();
    lda8(abs8(0x8AFD + g_cpu.Y));
    return (u8)g_cpu.A != 0xFF;
}
/* C1:5409 / 541B  Party fighter walks to (a, b) / 5D9E-5D9F along the grid
 * path. First call: path search from its position (4A71; the step map
 * 7CFD is saved to 7FFD + CC:F67C[fighter]), state A15B = 1. Each step
 * afterwards (A500 counts down 7 frames): the saved map comes back to 8AFD,
 * and from the current cell A53D the fighter picks the neighbour whose map
 * value has the top bits E0 (trying first the way it came, A548), then
 * nudges diagonally (CC:F454.. offsets) when the cell beyond is open; the
 * new direction A517, facing 96DB and cell A53D are stored. A map value FF
 * ends the walk (A15B = 0) and the op ends with the value saved in A4F0. */
static void path_walk(void)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    int party = !g_cpu._flag_C;
    if (party) { dc_cmp8((u8)g_cpu.A, 0x04); party = !g_cpu._flag_Z; }
    if (!party) goto finish;
    tax(); lda8(abs8(0xA014 + g_cpu.X)); tax(); stx_dp(0x80);
    lda8(abs8(0xA15B + g_cpu.X));
    if (g_cpu._flag_Z) {
        lda8(abs8(0x1D0C + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A); abs8_set(0xA553 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + g_cpu.X)); abs8_set(0xA55E + g_cpu.X, (u8)g_cpu.A); dp8_set(0x81, (u8)g_cpu.A);
        lda8(abs8(0xA2B0)); dp8_set(0x82, (u8)g_cpu.A);
        lda8(abs8(0xA2B2)); dp8_set(0x83, (u8)g_cpu.A);
        inc_abs(0xA166 + g_cpu.X);
        ldx(abs_x(0xA010));
        lda8(dp8(0x8E)); abs8_set(0xA4F0 + g_cpu.X, (u8)g_cpu.A);
        dc_C14A71_glue();
        lda8(abs8(0xA010)); tax();
        lda8(abs8(0xA014 + g_cpu.X)); tax(); stx_dp(0x80);
        lda8(abs8(0xA513)); abs8_set(0xA53D + g_cpu.X, (u8)g_cpu.A);
        lda8(dp8(0x80)); asl_a(1); tax();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(bus16(0xCCF67C + g_cpu.X)); tay(); tdc(); tax();
        dc_set_mx(1, g_cpu.x_flag);
        do {
            lda8(abs8(0x7CFD + g_cpu.X)); abs8_set(0x7FFD + g_cpu.Y, (u8)g_cpu.A);
            iny(); inx();
            cpx(0x0100);
        } while (!g_cpu._flag_Z);
        ldy(dp_x(0x80));
        tdc();
        abs8_set(0xA517 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0xA522 + g_cpu.Y, (u8)g_cpu.A);
        lda8(0xFF); abs8_set(0xA548 + g_cpu.Y, 0xFF); abs8_set(0xA500 + g_cpu.Y, 0xFF);
        lda8(0x01); abs8_set(0xA15B + g_cpu.Y, 0x01);
        goto step;
    }
    ldx(dp_x(0x80));
    lda8(abs8(0xA500 + g_cpu.X));
    if (!g_cpu._flag_N) goto step;
    lda8(0x07); abs8_set(0xA500 + g_cpu.X, 0x07);
    lda8(dp8(0x80)); asl_a(1); tax();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(bus16(0xCCF67C + g_cpu.X), 0x7FFD)); tax();
    ldy(0x8AFD);
    lda16(0x00FF);
    dc_mvn(0x7E, 0x7E);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(dp_x(0x80));
    lda8(abs8(0xA53D + g_cpu.X)); dp8_set(0x82, (u8)g_cpu.A);
    {
        /* the neighbour with a path mark (top bits E0): up F0, down 10,
         * left FF, else right 01, skipping the way back (A548) */
        static const u8 nb[3] = { 0xF0, 0x10, 0xFF };
        u8 d = 3;
        for (u8 k = 0; k < 3; k++) {
            lda8(abs8(0xA548 + g_cpu.X));
            if ((u8)g_cpu.A == k) continue;
            g_cpu._flag_C = 0;
            lda8(dc_adc8(dp8(0x82), nb[k])); tay(); sty_dp84();
            lda8(abs8(0x8AFD + g_cpu.Y) & 0xE0);
            if ((u8)g_cpu.A == 0xE0) { d = k; break; }
        }
        if (d == 3) { g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0x82), 0x01)); tay(); sty_dp84(); }
        if (d == 0) tdc(); else lda8(d);
        asl_a(3); dp8_set(0x85, (u8)g_cpu.A); tax();
        lda8(abs8(0x8AFD + g_cpu.Y));
        if ((u8)g_cpu.A == 0xFF) goto end;
        if (!path_cell(0xCCF454)) goto end;
        if ((u8)(g_cpu.A & 0xE0) != 0xE0) {
            /* the straight cell is not on the path: try the two diagonals */
            int alt = 0;
            if (!path_cell(0xCCF455)) goto end;
            if ((u8)(g_cpu.A & 0xE0) != 0xE0) {
                alt = 1;
                if (!path_cell(0xCCF456)) goto end;
            }
            dp8_set(0x86, (u8)g_cpu.Y);
            if (g_cpu.x_flag) {} else dp16_set(0x86, g_cpu.Y);
            g_cpu._flag_C = 0;
            lda8(dc_adc8(dp8(0x82), bus8((alt ? 0xCCF476 : 0xCCF475) + g_cpu.X))); tay();
            lda8(abs8(0x8AFD + g_cpu.Y) & 0xE0);
            u8 add = g_cpu._flag_Z ? (alt ? 2 : 1) : (alt ? 4 : 3);
            ldy(dp_x(0x86));
            dp8_set(0x85, (u8)(dp8(0x85) + add));
            lda8((u8)g_cpu.Y); dp8_set(0x84, (u8)g_cpu.A);
        }
        lda8(dp8(0x85)); tax();
        ldy(dp_x(0x80));
        lda8(dp8(0x85)); abs8_set(0xA517 + g_cpu.Y, (u8)g_cpu.A);
        lda8(bus8(0xCCF414 + g_cpu.X)); abs8_set(0xA548 + g_cpu.Y, (u8)g_cpu.A);
        lda8(bus8(0xCCF434 + g_cpu.X)); abs8_set(0x96DB + g_cpu.Y, (u8)g_cpu.A);
        lda8(dp8(0x84)); abs8_set(0xA53D + g_cpu.Y, (u8)g_cpu.A);
        goto step;
    }
end:
    ldx(dp_x(0x80));
    abs8_set(0xA15B + g_cpu.X, 0);
    goto finish;
step:
    lda8(0x00); script_advance();
    return;
finish:
    ldx(abs_x(0xA010));
    lda8(abs8(0xA4F0 + g_cpu.X)); script_advance();
}
DC_ENTRY(C15409, RTS)
{
    lda8(script_byte()); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(script_byte()); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x03); dp8_set(0x8E, 0x03);
    path_walk();
}
DC_ENTRY(C1541B, RTS)
{
    lda8(abs8(0x5D9E)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x5D9F)); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x01); dp8_set(0x8E, 0x01);
    path_walk();
}
/* ---- batch 46 ---- */
/* C1:5F13  Script command (palette n, period p): blinks the actor(s) between
 * palette entry n and their normal look. Every actor counts A17C down; when
 * it reaches 0 it reloads p, flips A187 and shows the palette (5DDC) or the
 * normal sprite set (5E8B), advancing 3 then. Without 5D80 only the normal
 * look comes back. Enemies (A010 >= 8) just advance 3. */
DC_ENTRY(C15F13, RTS)
{
    lda8(0x03); dp8_set(0x8E, 0x03);
    lda8(abs8(0x5D80));
    if (g_cpu._flag_Z) { sprite_restore(); return; }
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    lda8(script_byte()); dp8_set(0x92, (u8)g_cpu.A);
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) { lda8(0x03); script_advance(); return; }
    dc_cmp8((u8)g_cpu.A, 0x04);
    if (!g_cpu._flag_Z) {
        tax(); lda8(abs8(0xA014 + g_cpu.X)); tax();
        lda8(abs8(0xA17C + g_cpu.X));
        if (g_cpu._flag_Z) goto flip;
        dec_abs(0xA17C + g_cpu.X);
        goto wait;
    }
    tdc(); tax(); stx_dp(0x90);
    for (;;) {
        ldx(dp_x(0x90));
        lda8(abs8(0xA018 + g_cpu.X));
        if (g_cpu._flag_N) goto wait;
        tax();
        lda8(abs8(0xA17C + g_cpu.X));
        if (g_cpu._flag_Z) goto flip;
        dec_abs(0xA17C + g_cpu.X);
        dp8_set(0x90, (u8)(dp8(0x90) + 1));
    }
flip:
    lda8(dp8(0x92)); abs8_set(0xA17C + g_cpu.X, (u8)g_cpu.A);
    dp8_set(0x8E, 0);
    lda8(abs8(0xA187 + g_cpu.X) ^ 0x01); abs8_set(0xA187 + g_cpu.X, (u8)g_cpu.A);
    if (!g_cpu._flag_Z) pal_load(); else sprite_restore();
    return;
wait:
    tdc(); script_advance();
}
/* ---- batch 47 ---- */
struct arc_slots { u16 busy, dflt, x, xs, y, ys, h, dist, step, len, on, spd, done, vec; };
static const struct arc_slots arc_party = { 0xA337, 0xA5AA, 0xAA74, 0x1D0C, 0xAA87, 0x1D23, 0xA324,
    0xAA9A, 0xAAD3, 0xAAE6, 0xA2EB, 0xA86D, 0xA19D, 0xAAAD };
static const struct arc_slots arc_enemy = { 0xA342, 0xA5B5, 0xAA7F, 0, 0xAA92, 0, 0xA32F,
    0xAAA5, 0xAADE, 0xAAF1, 0xA2F6, 0xA878, 0xA1E0, 0xAAC3 };
/* Sets up the arc of actor Y: target (A2B0, A2B2), height $82 (FF = its
 * default), direction/length $80 (top bit = rising), speed $8F, and the
 * step vector from $84 * 64 / its height (C1:0089, 00D7). */
static void arc_setup(const struct arc_slots *s)
{
    lda8(dp8(0x82));
    if ((u8)g_cpu.A == 0xFF) { lda8(abs8(s->dflt + g_cpu.Y)); dp8_set(0x82, (u8)g_cpu.A); }
    lda8(abs8(0xA2B0)); abs8_set(s->x + g_cpu.Y, (u8)g_cpu.A);
    if (s->xs) abs8_set(s->xs + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0xA2B2)); abs8_set(s->y + g_cpu.Y, (u8)g_cpu.A);
    if (s->ys) abs8_set(s->ys + g_cpu.Y, (u8)g_cpu.A);
    lda8(dp8(0x80));
    if (!g_cpu._flag_N) {
        g_cpu._flag_C = 1;
        lda8(dc_sbc8(dp8(0x82), abs8(s->h + g_cpu.Y))); abs8_set(s->dist + g_cpu.Y, (u8)g_cpu.A);
        lda8(abs8(s->h + g_cpu.Y)); abs8_set(s->step + g_cpu.Y, (u8)g_cpu.A);
    } else {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0x82), abs8(s->h + g_cpu.Y))); abs8_set(s->dist + g_cpu.Y, (u8)g_cpu.A);
        lda8((u8)(-abs8(s->h + g_cpu.Y))); abs8_set(s->step + g_cpu.Y, (u8)g_cpu.A);
    }
    lda8(dp8(0x80) & 0x7F); abs8_set(s->len + g_cpu.Y, (u8)g_cpu.A);
    lda8(0x01); abs8_set(s->on + g_cpu.Y, 0x01);
    lda8(dp8(0x8F)); abs8_set(s->spd + g_cpu.Y, (u8)g_cpu.A);
    tdc(); abs8_set(s->done + g_cpu.Y, (u8)g_cpu.A);
    phy();
    lda8(dp8(0x84)); dp8_set(0xAE, (u8)g_cpu.A);
    lda8(0x40); dp8_set(0xAD, 0x40);
    dc_C10089_glue();
    ldx(dp_x(0xAF)); stx_dp(0xB1);
    lda8(abs8(s->h + g_cpu.Y)); dp8_set(0xB3, (u8)g_cpu.A);
    dc_C100D7_glue();
    lda8((u8)g_cpu.Y); asl_a(1); tay();
    lda8(dp8(0xB5)); abs8_set(s->vec + g_cpu.Y, (u8)g_cpu.A);
    lda8(dp8(0xB6)); abs8_set(s->vec + 1 + g_cpu.Y, (u8)g_cpu.A);
    ply();
    lda8(0x01); abs8_set(s->busy + g_cpu.Y, 0x01);
}
/* C1:6D14  Arc move of the actor(s) to (A2B0, A2B2): starts it on the first
 * call (advance 0), waits until it lands (A19D / A1E0), then advances $8E. */
static void arc_move(void)
{
    lda8(abs8(0xA010));
    dc_cmp8((u8)g_cpu.A, 0x08);
    if (g_cpu._flag_C) {
        ldy(abs_x(0xA2B6));
        lda8(abs8(0xA342 + g_cpu.Y));
        if (g_cpu._flag_Z) { arc_setup(&arc_enemy); goto wait; }
        ldx(abs_x(0xA2B6));
        lda8(abs8(0xA1E0 + g_cpu.X));
        if (g_cpu._flag_Z) goto wait;
        abs8_set(0xA342 + g_cpu.X, 0);
        goto done;
    }
    dc_cmp8((u8)g_cpu.A, 0x04);
    if (!g_cpu._flag_Z) {
        tax(); lda8(abs8(0xA014 + g_cpu.X)); tay();
        lda8(abs8(0xA337 + g_cpu.Y));
        if (g_cpu._flag_Z) { arc_setup(&arc_party); goto wait; }
        lda8(abs8(0xA19D + g_cpu.Y));
        if (g_cpu._flag_Z) goto wait;
        tdc(); abs8_set(0xA337 + g_cpu.Y, (u8)g_cpu.A);
        goto done;
    }
    tdc(); tax(); stx_dp(0x86);
    for (;;) {
        ldx(dp_x(0x86));
        lda8(abs8(0xA018 + g_cpu.X));
        if (g_cpu._flag_N) goto wait;
        tay();
        lda8(abs8(0xA337 + g_cpu.Y));
        if (!g_cpu._flag_Z) break;
        arc_setup(&arc_party);
        dp8_set(0x86, (u8)(dp8(0x86) + 1));
    }
    tdc(); tax(); stx_dp(0x86);
    dp8_set(0x84, 0);
    for (;;) {
        ldx(dp_x(0x86));
        lda8(abs8(0xA018 + g_cpu.X));
        if (g_cpu._flag_N) break;
        tax();
        lda8(abs8(0xA19D + g_cpu.X));
        if (!g_cpu._flag_Z) { abs8_set(0xA337 + g_cpu.X, 0); dp8_set(0x84, (u8)(dp8(0x84) + 1)); }
        dp8_set(0x86, (u8)(dp8(0x86) + 1));
    }
    lda8(dp8(0x84));
    if (g_cpu._flag_Z) goto done;
wait:
    lda8(0x00); script_advance();
    return;
done:
    lda8(dp8(0x8E)); script_advance();
}
static void arc_to_5d9e(void)
{
    lda8(script_byte()); dp8_set(0x80, (u8)g_cpu.A);
    lda8(script_byte()); dp8_set(0x82, (u8)g_cpu.A);
    lda8(script_byte()); dp8_set(0x84, (u8)g_cpu.A);
    lda8(abs8(0x5D9E)); abs8_set(0xA2B0, (u8)g_cpu.A);
    lda8(abs8(0x5D9F)); abs8_set(0xA2B2, (u8)g_cpu.A);
    lda8(0x04); dp8_set(0x8E, 0x04);
}
/* C1:6F31 / 6F75  Script command (length, height, power): arc jump to the
 * point 5D9E-5D9F at speed 3 / 5. */
DC_ENTRY(C16F31, RTS) { arc_to_5d9e(); lda8(0x03); dp8_set(0x8F, 0x03); arc_move(); }
DC_ENTRY(C16F75, RTS) { arc_to_5d9e(); lda8(0x05); dp8_set(0x8F, 0x05); arc_move(); }
/* ---- batch 48 ---- */
/* C1:1C4A / 1C55  Builds the battle window graphics of slot 9679 (kind
 * 967C: 3 = none, from 1C4A) for page 967B, when the page changed (9907).
 * The page's tile map (layout CC:F40A[kind] * page, from the 1C57/1C78
 * source of the slot, CC:F92D) is walked in blocks of CC:F3C2 x F3C3 rows
 * at CC:F3B6 offsets: each map word picks a 32-byte tile (0 = blank), copied
 * into 7E:CC:F941[slot] + offset, mirrored through C0:FD00 when bit 14 is
 * set. The list that follows (CC:F4FC[kind] bytes) goes to 9F47 (word,
 * sign-extended) at CC:F917[slot]; 9912 counts the update. */
static void tile_copy(void)
{
    /* one 32-byte tile from DB:Y to 7E:X, or zeros when the map word is 0 */
    dc_set_mx(0, 0);
    lda16(bus16(lp(0xBA))); dp16_set(0x8C, g_cpu.A);
    lda16(g_cpu.A & 0x07FF);
    if (g_cpu._flag_Z) {
        for (int k = 0; k < 32; k += 2) bus16_set(0x7E0000 + (u16)(g_cpu.X + k), g_cpu.A);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.X, 0x0020)); tax();
    } else {
        asl_a(5);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0xBD))); g_cpu.Y = g_cpu.A; dc_set_flags_nz16(g_cpu.Y);
        lda16(dp16(0x8C) & 0x4000);
        if (g_cpu._flag_Z) {
            for (int k = 0; k < 32; k += 2) {
                lda16(abs16((u32)k + g_cpu.Y));
                bus16_set(0x7E0000 + (u16)(g_cpu.X + k), g_cpu.A);
            }
            g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.X, 0x0020)); tax();
        } else {
            dp16_set(0x8C, g_cpu.Y);
            dp16_set(0xAD, g_cpu.X);
            dc_set_mx(1, 1);
            u32 src = ((u32)g_cpu.DB << 16) + dp16(0x8C);
            for (u8 y = 0; y < 32; y++) {
                g_cpu.Y = y;
                lda8(bus8(src + y)); tax();
                lda8(bus8(0xC0FD00 + g_cpu.X));
                bus8_set(lp(0xAD) + y, (u8)g_cpu.A);
            }
            g_cpu.Y = 32;
            dc_set_mx(0, 0);
            g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0xAD), 0x0020)); tax();
        }
    }
    dp16_set(0xBA, (u16)(dp16(0xBA) + 2));
    tdc();
    dc_set_mx(1, 0);
}
static void win_tiles(void)
{
    ldx(abs16(0x9679)); stx_dp(0x88);
    abs8_set(0x9922 + g_cpu.X, 0);
    lda8(abs8(0x967B));
    dc_cmp8((u8)g_cpu.A, abs8(0x9907 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    abs8_set(0x9907 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x967C + g_cpu.X)); asl_a(1); tax();
    lda8(bus8(0xCCF40A + g_cpu.X)); dp8_set(0xA7, (u8)g_cpu.A);
    lda8(bus8(0xCCF40B + g_cpu.X)); dp8_set(0xA8, (u8)g_cpu.A);
    lda8(abs8(0x967B)); dp8_set(0xA5, (u8)g_cpu.A);
    dc_C100A7_glue();
    ldx(dp16(0x88));
    lda8(bus8(0xCCF92D + g_cpu.X)); tax();
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0x1C78 + g_cpu.X), dp16(0xA9))); dp16_set(0xBA, g_cpu.A);
    lda16(abs16(0x1C57 + g_cpu.X)); dp16_set(0xBD, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    dc_push8(g_cpu.DB);
    lda8(abs8(0x1C7A + g_cpu.X)); dp8_set(0xBC, (u8)g_cpu.A);
    lda8(abs8(0x1C59 + g_cpu.X)); dc_push8((u8)g_cpu.A);
    ldx(dp16(0x88));
    lda8(abs8(0x967C + g_cpu.X)); dp8_set(0xAD, (u8)g_cpu.A);
    dc_cmp8((u8)g_cpu.A, 0x02);
    if (g_cpu._flag_Z) {
        lda8(abs8(0x9679)); dc_cmp8((u8)g_cpu.A, 0x03);
        if (!g_cpu._flag_Z) { lda8(0x05); dp8_set(0xAD, 0x05); }
    }
    lda8(0x0E); dp8_set(0xAE, 0x0E);
    dc_C10089_glue();
    ldx(dp16(0xAF));
    lda8(bus8(0xCCF3C2 + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A); dp8_set(0x81, (u8)g_cpu.A);
    lda8(bus8(0xCCF3C3 + g_cpu.X)); dp8_set(0x85, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.X, 0xF3B6)); dp16_set(0x82, g_cpu.A);
    dp16_set(0x88, (u16)(dp16(0x88) << 1));
    tdc();
    dc_set_mx(1, 0);
    lda8(0xCC); dp8_set(0x84, 0xCC);
    lda8(0x7E); dp8_set(0xAF, 0x7E);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    for (;;) {
        dc_set_mx(0, 0);
        ldx(dp16(0x88));
        g_cpu._flag_C = 0;
        lda16(dc_adc16(bus16(0xCCF941 + g_cpu.X), bus16(lp(0x82)))); tax();
        dp16_set(0x82, (u16)(dp16(0x82) + 2));
        for (;;) {
            tile_copy();
            u8 n = (u8)(dp8(0x80) - 1); dp8_set(0x80, n);
            if (n) continue;
            break;
        }
        u8 r = (u8)(dp8(0x85) - 1); dp8_set(0x85, r);
        if (!r) break;
        lda8(dp8(0x81)); dp8_set(0x80, (u8)g_cpu.A);
    }
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    lda8(abs8(0x9679)); tax();
    lda8(abs8(0x967C + g_cpu.X)); tax();
    lda8(bus8(0xCCF4FC + g_cpu.X)); tax(); stx_dp(0x80);
    ldx(dp16(0x88));
    lda8(bus8(0xCCF917 + g_cpu.X)); dp8_set(0x82, (u8)g_cpu.A);
    lda8(bus8(0xCCF918 + g_cpu.X)); dp8_set(0x83, (u8)g_cpu.A);
    ldx(dp16(0x82));
    ldy(0x0000);
    do {
        lda8(bus8(lp(0xBA) + g_cpu.Y)); abs8_set(0x9F47 + g_cpu.X, (u8)g_cpu.A);
        if (g_cpu._flag_N) lda8(0xFF); else tdc();
        abs8_set(0x9F48 + g_cpu.X, (u8)g_cpu.A);
        inx(); inx(); iny();
        cpy(dp16(0x80));
    } while (!g_cpu._flag_Z);
    inc_abs(0x9912);
}
DC_ENTRY(C11C4A, RTS)
{
    ldx(abs16(0x9679));
    lda8(abs8(0x967C + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (g_cpu._flag_Z) return;
    win_tiles();
}
DC_ENTRY(C11C55, RTS) { win_tiles(); }
/* ---- batch 49 ---- */
/* Damage formulas: every step loads the factors into dp 28/2A and calls
 * C1:C90B (2C = 28 * 2A) or C1:C92A (2C = 28 / 2A). */
static void ax_dp(u8 o) { tax(); stx_dp(o); }
static void mv_dp(u8 src, u8 dst) { ldx(dp16(src)); stx_dp(dst); }
static void li_dp(u16 v, u8 o) { ldx(v); stx_dp(o); }
static void dmg_mul(void) { dc_C1C90B_glue(); }
static void dmg_div(void) { dc_C1C92A_glue(); }
static void ldx_b1f4(void) { ldx(abs16(0xB1F4)); }
/* 2C = AEEF * 1 * dp[src] / k (AEEF = the power multiplier of the tech) */
static void dmg_power(u8 src, u16 k, int tdc_first)
{
    lda8(abs8(0xAEEF)); ax_dp(0x28);
    lda8(0x01); ax_dp(0x2A);
    dmg_mul();
    mv_dp(0x2C, 0x28);
    if (tdc_first) tdc();
    mv_dp(src, 0x2A);
    dmg_mul();
    mv_dp(0x2C, 0x28);
    li_dp(k, 0x2A);
    dmg_div();
}
/* B2BE = C1:AF22(dp[o] + add) (the random spread of the damage) */
static void dmg_spread(u8 o, u8 add)
{
    tdc(); tax();
    lda8(dp8(o));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, add));
    dc_C1AF22_glue();
    abs8_set(0xB2BE, (u8)g_cpu.A);
}
/* 30 = dp 2C + dp 0A (16 bit) */
static void sum_2c_0a(u8 dst)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x2C), dp16(0x0A))); dp16_set(dst, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* 2C = dp 2C / k * m */
static void div_mul(u16 k, u16 m)
{
    mv_dp(0x2C, 0x28); li_dp(k, 0x2A); dmg_div();
    mv_dp(0x2C, 0x28); li_dp(m, 0x2A); dmg_mul();
}
static void mul_div(u16 m, u16 k)
{
    mv_dp(0x2C, 0x28); li_dp(m, 0x2A); dmg_mul();
    mv_dp(0x2C, 0x28); li_dp(k, 0x2A); dmg_div();
}
/* C1:E0F5  Base damage of the current attack by formula $16 (1-7; 8 uses
 * none, every other value the weapon-stat default), with B1F4 = the
 * attacker's stat block offset: from its stats (5E2D..5E70 by offset),
 * the power AEEF and a random spread (B2BE). Then the target's state
 * (5E4E | 5E53, 5E4B) scales it: x1.5, /2*3, /4*5, *8/10 twice, /4.
 * The result goes to $16 and $0A is cleared. */
DC_ENTRY(C1E0F5, RTS)
{
    tdc();
    lda8(dp8(0x16));
    /* the compare chain (its carry reaches the multiply/divide helpers):
     * 1-7 match; anything else past CMP #8 takes the default formula, and
     * 8 itself falls through CMP #9 / #A to no formula at all, so the
     * formula 9 (C1:E4BB) and A (C1:E515) blocks are never reached. */
    u8 f = (u8)g_cpu.A;
    for (u8 k = 1; k <= 8; k++) { dc_cmp8(f, k); if (g_cpu._flag_Z) break; }
    if (f == 0x08) { dc_cmp8(f, 0x09); dc_cmp8(f, 0x0A); }
    switch (f > 0x08 ? 0 : f) {
    case 0x01:
        dc_C1E63B_glue();
        ldx_b1f4(); tdc(); lda8(abs8(0x5E63 + g_cpu.X)); ax_dp(0x28);
        lda8(0x0C); ax_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x0A);
        ldx_b1f4(); lda8(abs8(0x5E70 + g_cpu.X)); ax_dp(0x28);
        lda8(0x05); ax_dp(0x2A); dmg_mul();
        sum_2c_0a(0x30);
        dmg_power(0x30, 0x12, 0);
        mv_dp(0x2C, 0x30);
        tdc(); ldx_b1f4(); lda8(abs8(0x5E63 + g_cpu.X)); ax_dp(0x28);
        li_dp(0x0003, 0x2A); dmg_div();
        dmg_spread(0x2C, 1);
        mv_dp(0x30, 0x2C);
        break;
    case 0x02:
        dc_C1E63B_glue();
        ldx_b1f4(); tdc(); lda8(abs8(0x5E67 + g_cpu.X)); ax_dp(0x28);
        lda8(0x08); ax_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x0A);
        ldx_b1f4(); lda8(abs8(0x5E70 + g_cpu.X)); ax_dp(0x28);
        lda8(0x08); ax_dp(0x2A); dmg_mul();
        sum_2c_0a(0x30);
        dmg_power(0x30, 0x12, 0);
        ldx_b1f4(); lda8(abs8(0x5E67 + g_cpu.X)); dc_C1C063_glue();
        dmg_spread(0x30, 1);
        break;
    case 0x03:
        tdc(); ldx_b1f4(); lda8(abs8(0x5E3F + g_cpu.X)); ax_dp(0x28);
        lda8(0x0C); ax_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x0A);
        tdc(); ldx_b1f4(); lda8(abs8(0x5E66 + g_cpu.X)); ax_dp(0x28);
        lda8(0x0C); ax_dp(0x2A); dmg_mul();
        sum_2c_0a(0x0A);
        lda8(0x01); ax_dp(0x28);
        lda8(abs8(0xAEEF)); ax_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x28); mv_dp(0x0A, 0x2A); dmg_mul();
        mv_dp(0x2C, 0x28); li_dp(0x000F, 0x2A); dmg_div();
        tdc(); ldx_b1f4(); lda8(abs8(0x5E66 + g_cpu.X)); dc_C1C063_glue();
        dmg_spread(0x30, 1);
        break;
    case 0x04:
        dc_C1E63B_glue();
        tdc(); ldx_b1f4(); lda8(abs8(0x5E6A + g_cpu.X)); ax_dp(0x28);
        lda8(0x02); ax_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x0A);
        dmg_power(0x0A, 0x12, 1);
        tdc(); ldx_b1f4(); lda8(abs8(0x5E6A + g_cpu.X)); dc_C1C02A_glue();
        dmg_spread(0x30, 2);
        break;
    case 0x05:
        dc_C1E63B_glue();
        ldx_b1f4(); tdc(); lda8(abs8(0x5E63 + g_cpu.X)); ax_dp(0x28);
        lda8(0x10); ax_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x0A);
        ldx_b1f4(); tdc(); lda8(abs8(0x5E3F + g_cpu.X)); ax_dp(0x28); stx_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x28); li_dp(0x0005, 0x2A); dmg_div();
        sum_2c_0a(0x30);
        dmg_power(0x30, 0x14, 0);
        mv_dp(0x2C, 0x30);
        tdc(); ldx_b1f4(); lda8(abs8(0x5E3F + g_cpu.X)); ax_dp(0x28);
        li_dp(0x0003, 0x2A); dmg_div();
        dmg_spread(0x2C, 1);
        mv_dp(0x30, 0x2C);
        break;
    case 0x06:
        dc_C1E63B_glue();
        ldx_b1f4(); lda8(abs8(0x5E64 + g_cpu.X)); ax_dp(0x28);
        li_dp(0x0001, 0x2A); dmg_mul();
        mv_dp(0x2C, 0x28);
        lda8(abs8(0xAEEF)); ax_dp(0x2A); dmg_mul();
        mv_dp(0x2C, 0x2A);
        ldx_b1f4();
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(abs16(0x5E32 + g_cpu.X), abs16(0x5E30 + g_cpu.X)));
        dp16_set(0x28, g_cpu.A); dp16_set(0x0A, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dmg_mul();
        mv_dp(0x2C, 0x28); li_dp(0x008C, 0x2A); dmg_div();
        dc_set_mx(0, g_cpu.x_flag); lda16(dp16(0x0A)); dc_set_mx(1, g_cpu.x_flag);
        dc_C1C1B2_glue();
        dmg_spread(0x30, 1);
        break;
    case 0x07:
        dc_C1E63B_glue();
        ldx_b1f4(); tdc(); lda8(abs8(0x5E63 + g_cpu.X));
        dc_set_mx(0, g_cpu.x_flag);
        asl_a(1);
        ldx(abs16(0xB1F8));
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0x5E70 + g_cpu.X))); dp16_set(0x30, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        dmg_power(0x30, 0x12, 0);
        ldx_b1f4(); lda8(abs8(0x5E63 + g_cpu.X)); dc_C1C063_glue();
        dmg_spread(0x30, 1);
        break;
    case 0x08:
        break;
    default: {
        /* the stat picked by the weapon kind bits of 5E2E: 80 -> 5E6C,
         * 40 -> 5E6D, 20 -> 5E6E, else 5E6F (16-bit reads); B1F8 + it
         * indexes 5E70 for the damage base (8-bit into $30). */
        dc_C1E63B_glue();
        ldx_b1f4(); tdc(); lda8(abs8(0x5E63 + g_cpu.X));
        dc_set_mx(0, g_cpu.x_flag);
        asl_a(1);
        tdc();
        ldx_b1f4();
        lda16(abs16(0x5E2E + g_cpu.X));
        u16 w = (u16)g_cpu.A;
        u16 src = (w & 0x80) ? 0x5E6C : (w & 0x40) ? 0x5E6D : (w & 0x20) ? 0x5E6E : 0x5E6F;
        lda16(abs16(src + g_cpu.X));
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xB1F8))); tax(); stx_dp(0x30);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0x5E70 + g_cpu.X))); dp8_set(0x30, (u8)g_cpu.A);
        tdc();
        dmg_power(0x30, 0x12, 0);
        ldx_b1f4(); lda8(abs8(0x5E63 + g_cpu.X)); dc_C1C063_glue();
        dmg_spread(0x30, 1);
        break;
    }
    }
    /* the target's state */
    ldx_b1f4();
    u8 st = abs8(0x5E4E + g_cpu.X) | abs8(0x5E53 + g_cpu.X);
    lda8(st);
    if (st & 0x80) {
        dc_set_mx(0, g_cpu.x_flag);
        lda16(dp16(0x2C)); lsr_a(1);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x2C))); dp16_set(0x2C, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    } else {
        if (st & 0x08) div_mul(0x0002, 0x0003);
        else if (st & 0x02) div_mul(0x0004, 0x0005);
        ldx_b1f4(); lda8(abs8(0x5E4B + g_cpu.X));
        if (g_cpu.A & 0x40) mul_div(0x0008, 0x000A);
        ldx_b1f4(); lda8(abs8(0x5E4B + g_cpu.X));
        if (g_cpu.A & 0x20) mul_div(0x0008, 0x000A);
        ldx_b1f4(); lda8(abs8(0x5E4B + g_cpu.X));
        if (g_cpu.A & 0x04) { mv_dp(0x2C, 0x28); li_dp(0x0004, 0x2A); dmg_div(); }
    }
    mv_dp(0x2C, 0x16);
    tdc(); tax(); stx_dp(0x0A);
}
/* ---- batch 50 ---- */
/* C1:34A7  Rebuilds the window graphics of every active slot 0-10 (96F5)
 * with its page A4DC, then sets 9689-968B = 1, 2, 3. 34BD enters the loop
 * at the page store with A = the page. */
static void win_all_from(int mid)
{
    for (;;) {
        if (!mid) {
            ldx(abs16(0x9689));
            lda8(abs8(0x96F5 + g_cpu.X));
            if (g_cpu._flag_Z) goto next;
            lda8(abs8(0x9689)); abs8_set(0x9679, (u8)g_cpu.A);
            lda8(abs8(0xA4DC + g_cpu.X));
        }
        mid = 0;
        abs8_set(0x967B, (u8)g_cpu.A);
        dc_C11C55_glue();
    next:
        inc_abs(0x9689);
        lda8(abs8(0x9689));
        dc_cmp8((u8)g_cpu.A, 0x0B);
        if (g_cpu._flag_Z) break;
    }
    lda8(0x01); abs8_set(0x9689, 0x01);
    lda8(0x02); abs8_set(0x968A, 0x02);
    lda8(0x03); abs8_set(0x968B, 0x03);
}
DC_ENTRY(C134A7, RTS) { tdc(); tax(); stx_abs(0x9689); win_all_from(0); }
DC_ENTRY(C134BD, RTS) { win_all_from(1); }
/* C1:F93E  Stat recompute of the three party members: for each one not
 * absent (AEFF+n top bit clear), clears B311+n, prepares its block
 * (C1:F908) and runs the equipment/status pass (C1:F623) with B1F4 = n and
 * B285/B287/B289 = its block and the other two, repeated while it leaves
 * B28C nonzero (B28C is restored between members); then F205 and F9EB. */
DC_ENTRY(C1F93E, RTS)
{
    static const u16 blk[3] = { 0x5E2D, 0x5EAD, 0x5F2D };
    ldx(abs16(0xB28C)); phx();
    for (int n = 0; n < 3; n++) {
        if (n) { plx(); stx_abs(0xB28C); phx(); }
        abs8_set(0xB311 + n, 0);
        lda8(abs8(0xAEFF + n));
        if (g_cpu._flag_N) continue;
        ldx(blk[n]); dc_C1F908_glue();
        do {
            ldx((u16)n); stx_abs(0xB1F4);
            ldx(blk[n]); stx_abs(0xB285);
            ldx(blk[(n + 1) % 3]); stx_abs(0xB287);
            ldx(blk[(n + 2) % 3]); stx_abs(0xB289);
            dc_C1F623_glue();
            ldx(abs16(0xB28C));
            cpx(0x0000);
        } while (!g_cpu._flag_Z);
    }
    plx(); stx_abs(0xB28C);
    dc_C1F205_glue();
    dc_C1F9EB_glue();
}
/* C1:423A  Enemy action timers, once per tick: A12F[0..10] count down; from slot A21F on
 * (round robin) the first active slot (A124) whose timer ran out acts, unless an action is
 * pending (A028 / A012). Its step A13A advances (or restarts at A150 - 1, which also ends the
 * pattern) through its pattern list E4:(A1FE[slot] + step): 0 ends the list (A124 = 2 loops it
 * from step 0, else the slot goes idle), else that byte is the new timer and the action is
 * E4:(A1E8[slot] + step + C1:00A7(stat 96DB / A214, 966E, A2C8)); window via C1:1C4A, A012 = 2. */
DC_ENTRY(C1423A, RTS)
{
    tdc(); tax();
    do {
        lda8(abs8(0xA12F + g_cpu.X));
        if (!g_cpu._flag_Z) dec_abs(0xA12F + g_cpu.X);
        inx(); cpx(0x000B);
    } while (!g_cpu._flag_Z);
    tdc(); tay();
    lda8(abs8(0xA21F)); tax();
    for (;;) {
        lda8(abs8(0xA124 + g_cpu.X));
        if (!g_cpu._flag_Z) { lda8(abs8(0xA12F + g_cpu.X)); if (g_cpu._flag_Z) break; }
        inx(); cpx(0x000B);
        if (g_cpu._flag_Z) { tdc(); tax(); }
        iny(); cpy(0x000B);
        if (g_cpu._flag_Z) return;
    }
    lda8(abs8(0xA028)); if (!g_cpu._flag_Z) return;
    lda8(abs8(0xA012)); if (!g_cpu._flag_Z) return;
    stx_dp(0x80);
    inx(); cpx(0x000B);
    if (g_cpu._flag_Z) { tdc(); tax(); }
    lda8((u8)g_cpu.X); abs8_set(0xA21F, (u8)g_cpu.A);
    ldx(dp16(0x80));
    lda8(abs8(0x96DB + g_cpu.X)); dc_cmp8((u8)g_cpu.A, abs8(0xA214 + g_cpu.X));
    if (!g_cpu._flag_Z) abs8_set(0xA214 + g_cpu.X, (u8)g_cpu.A); else lda8(abs8(0xA214 + g_cpu.X));
    dp8_set(0xA5, (u8)g_cpu.A);
    lda8(abs8(0x966E + g_cpu.X)); dp8_set(0xA7, (u8)g_cpu.A);
    lda8(abs8(0xA2C8 + g_cpu.X)); dp8_set(0xA8, (u8)g_cpu.A);
    dc_C100A7_glue();
    ldx(dp16(0xA9)); stx_dp(0x82);
    lda8(dp8(0x80)); abs8_set(0x9679, (u8)g_cpu.A); asl_a(1); tay();
    ldx(dp16(0x80));
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA13A + g_cpu.X), 0x01)); abs8_set(0xA13A + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xA150 + g_cpu.X));
    if (!g_cpu._flag_Z) {
        lda8((u8)(g_cpu.A - 1)); abs8_set(0xA13A + g_cpu.X, (u8)g_cpu.A);
        abs8_set(0xA124 + g_cpu.X, 0);
    }
    for (;;) {
        lda8(abs8(0xA13A + g_cpu.X));
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xA1FE + g_cpu.Y))); tax();
        tdc();
        dc_set_mx(1, 0);
        lda8(bus8(0xE40000 + (u32)g_cpu.X));
        if (!g_cpu._flag_Z) break;
        ldx(dp16(0x80));
        lda8(abs8(0xA124 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0x02);
        if (!g_cpu._flag_Z) { abs8_set(0xA124 + g_cpu.X, 0); return; }
        ldx(dp16(0x80));
        tdc(); abs8_set(0xA13A + g_cpu.X, 0);
    }
    ldx(dp16(0x80));
    abs8_set(0xA12F + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xA13A + g_cpu.X));
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x82)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xA1E8 + g_cpu.Y))); tax();
    tdc();
    dc_set_mx(1, 0);
    lda8(bus8(0xE40000 + (u32)g_cpu.X)); abs8_set(0x967B, (u8)g_cpu.A);
    dc_C11C4A_glue();
    lda8(0x02); abs8_set(0xA012, 0x02);
}
/* Next frame of party member dp9C's pose list (8 frames from 9799 / 97D9 + slot * 8, cursor
 * 969C, wraps at 4 or at an empty entry): 9694 = its duration. */
static void pose_next(void)
{
    lda8(dp8(0x9C)); dc_C10112_glue(); dp8_set(0x80, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0x969C + g_cpu.X))); tay();
    lda8(abs8(0x9799 + g_cpu.Y));
    if (g_cpu._flag_Z) {
        abs8_set(0x969C + g_cpu.X, 0);
        lda8(dp8(0x80)); tay();
        lda8(abs8(0x9799 + g_cpu.Y));
    }
    abs8_set(0x9694 + g_cpu.X, (u8)g_cpu.A);
}
/* C1:333D  Party battle poses, for 9841 up to 3 (party slot 992D[9841], skipped when negative or
 * not shown, 96F8). When the pose 5E0D or the stat 96DE changed: reload the slot's 8 frames
 * (sprite ids from [1CA2 + 3 * slot] at 8 * C1:00A7(96DE, 9671, A2CB) + 0C / 04 / 18 by the
 * pose kind CC:FB91, durations through CC:FBCE from [1CC3 + 3 * slot]). Then count the frame
 * down (9694); at 0 show the frame (967B, object 9679 = slot + 3; the window C1:1C4A unless
 * 967C >= 3, or wait a tick while 9912 says the window is busy) and step to the next one.
 * Finally A4 += 1 toward 993B while below 3. */
DC_ENTRY(C1333D, RTS)
{
    do {
        lda8(abs8(0x9841)); tax();
        lda8(abs8(0x992D + g_cpu.X));
        if (g_cpu._flag_N) goto next;
        tax(); stx_dp(0x9C);
        lda8(abs8(0x96F8 + g_cpu.X));
        if (g_cpu._flag_Z) goto next;
        lda8(abs8(0x974A + g_cpu.X)); dc_cmp8((u8)g_cpu.A, abs8(0x5E0D + g_cpu.X));
        bool same = g_cpu._flag_Z;
        if (same) { lda8(abs8(0x96DE + g_cpu.X)); dc_cmp8((u8)g_cpu.A, abs8(0x96E9 + g_cpu.X)); same = g_cpu._flag_Z; }
        if (!same) {
            lda8(abs8(0x5E0D + g_cpu.X)); abs8_set(0x974A + g_cpu.X, (u8)g_cpu.A);
            lda8(abs8(0x96DE + g_cpu.X)); abs8_set(0x96E9 + g_cpu.X, (u8)g_cpu.A);
            lda8((u8)g_cpu.X); asl_a(1); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x9C))); tax();
            static const u8 ptr[6] = { 0xC3, 0xC4, 0xC5, 0xC0, 0xC1, 0xC2 };
            static const u16 src[6] = { 0x1CC3, 0x1CC4, 0x1CC5, 0x1CA2, 0x1CA3, 0x1CA4 };
            for (int k = 0; k < 6; k++) { lda8(abs8(src[k] + g_cpu.X)); dp8_set(ptr[k], (u8)g_cpu.A); }
            ldx(dp16(0x9C));
            lda8(abs8(0x96DE + g_cpu.X)); dp8_set(0xA5, (u8)g_cpu.A);
            lda8(abs8(0x9671 + g_cpu.X)); dp8_set(0xA7, (u8)g_cpu.A);
            lda8(abs8(0xA2CB + g_cpu.X)); dp8_set(0xA8, (u8)g_cpu.A);
            dc_C100A7_glue();
            ldx(dp16(0x9C));
            lda8(abs8(0x974A + g_cpu.X)); tax();
            lda8(bus8(0xCCFB91 + (u32)g_cpu.X));
            if (g_cpu._flag_Z) lda8(0x0C);
            else {
                lda8((u8)(g_cpu.A - 1));
                if (g_cpu._flag_Z) { ldx(dp16(0x9C)); abs8_set(0x9829 + g_cpu.X, 0); lda8(0x04); }
                else lda8(0x18);
            }
            dp8_set(0x86, (u8)g_cpu.A);
            dc_set_mx(0, 0);
            g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0xA9))); dp16_set(0x82, g_cpu.A);
            tdc();
            dc_set_mx(1, 0);
            lda8(0x08); dp8_set(0x80, 0x08);
            ldx(dp16(0x9C));
            lda8(bus8(0xCCF82F + (u32)g_cpu.X)); tax();
            u16 base = g_cpu.X;
            ldy(dp16(0x82));
            do {
                lda8(bus8(lp(0xC0) + g_cpu.Y)); abs8_set(0x97D9 + g_cpu.X, (u8)g_cpu.A);
                iny(); inx();
                u8 n = (u8)(dp8(0x80) - 1); dp8_set(0x80, n); dc_set_flags_nz8(n);
            } while (!g_cpu._flag_Z);
            lda8(0x08); dp8_set(0x80, 0x08);
            ldx(base); stx_dp(0x84);
            lda8(dp8(0x86)); tay();
            do {
                lda8(bus8(lp(0xC3) + g_cpu.Y)); tax();
                lda8(bus8(0xCCFBCE + (u32)g_cpu.X));
                ldx(dp16(0x84)); abs8_set(0x9799 + g_cpu.X, (u8)g_cpu.A);
                iny();
                dp8_set(0x84, (u8)(dp8(0x84) + 1));
                u8 n = (u8)(dp8(0x80) - 1); dp8_set(0x80, n); dc_set_flags_nz8(n);
            } while (!g_cpu._flag_Z);
        }
        ldx(dp16(0x9C));
        dec_abs(0x9694 + g_cpu.X);
        if (!g_cpu._flag_Z) goto next;
        lda8(dp8(0x9C)); dc_C10112_glue(); dp8_set(0x80, (u8)g_cpu.A);
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0x969C + g_cpu.X))); tay();
        lda8(abs8(0x9799 + g_cpu.Y));
        if (g_cpu._flag_Z) { abs8_set(0x969C + g_cpu.X, 0); lda8(dp8(0x80)); tay(); }
        lda8(abs8(0x97D9 + g_cpu.Y)); abs8_set(0x967B, (u8)g_cpu.A);
        g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0x9C), 0x03)); abs8_set(0x9679, (u8)g_cpu.A); tax();
        lda8(abs8(0x9912));
        bool wait = false;
        if (!g_cpu._flag_Z) {
            lda8((u8)(g_cpu.A - 1));
            if (!g_cpu._flag_Z) wait = true;
            else {
                lda8(abs8(0x967C + g_cpu.X));
                if (!g_cpu._flag_Z) wait = true;
                else {
                    lda8(abs8(0x9679)); dc_cmp8((u8)g_cpu.A, 0x04);
                    if (!g_cpu._flag_Z) { dc_cmp8((u8)g_cpu.A, 0x06); wait = !g_cpu._flag_C; }
                }
            }
        }
        if (wait) {
            inc_abs(0x9922 + g_cpu.X);
            ldx(dp16(0x9C)); inc_abs(0x9694 + g_cpu.X);
            goto next;
        }
        lda8(abs8(0x967C + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0x03);
        if (!g_cpu._flag_C) dc_C11C4A_glue();
        ldx(dp16(0x9C));
        inc_abs(0x969C + g_cpu.X);
        lda8(abs8(0x969C + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0x04);
        if (g_cpu._flag_Z) abs8_set(0x969C + g_cpu.X, 0);
        pose_next();
    next:
        inc_abs(0x9841);
        lda8(abs8(0x9841)); dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
    lda8(dp8(0xA4)); dc_cmp8((u8)g_cpu.A, abs8(0x993B));
    if (!g_cpu._flag_Z) { dc_cmp8((u8)g_cpu.A, 0x03); if (!g_cpu._flag_C) { u8 v = (u8)(dp8(0xA4) + 1); dp8_set(0xA4, v); dc_set_flags_nz8(v); } }
}

/* ---- battle item list and party equipment stats ---------------------------- */
/* C1:CDFF  Clears 1580..1A7F and rebuilds the battle item list there from the
 * 256 inventory slots (ids 2400, counts 2500 into dp0E): FD:AF80 adds a slot,
 * FD:B01C passes an empty one. Both advance dp00 / dp08 (it ends when dp08 wraps). */
DC_ENTRY(C1CDFF, RTS)
{
    tdc(); tax();
    do { abs8_set(0x1580 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0500); } while (!g_cpu._flag_Z);
    tdc(); tax();
    stx_dp(0x00); stx_dp(0x04); stx_dp(0x08); stx_dp(0x0A);
    do {
        tdc();
        ldx(dp_x(0x00));
        lda8(abs8(0x2400 + g_cpu.X));
        if (g_cpu._flag_Z) dc_jsl(0xFDB01C, 0xC1CE1C);
        else {
            lda8(abs8(0x2500 + g_cpu.X)); dp8_set(0x0E, (u8)g_cpu.A);
            lda8(abs8(0x2400 + g_cpu.X));
            dc_jsl(0xFDAF80, 0xC1CE2A);
        }
        lda8(dp8(0x08));
    } while (!g_cpu._flag_Z);
}
/* C1:CF15  For party slots 0-2 (record FD:A80B): looks up the item ([06] at the
 * record offset) with FD:B033 and copies dp0A bytes of its CC:0000 record to [08]
 * at the same offset. */
DC_ENTRY(C1CF15, RTS)
{
    tdc(); tax();
    stx_dp(0x02); stx_dp(0x04);
    do {
        ldx(dp_x(0x02));
        dc_set_mx(0, g_cpu.x_flag);
        lda16(g_cpu.X); asl_a(1); tax();
        lda16(bus16(0xFDA80B + g_cpu.X));
        dp16_set(0x00, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        ldy(dp_x(0x00));
        lda8(bus8(dba((u32)dp16(0x06) + g_cpu.Y)));
        dc_jsl(0xFDB033, 0xC1CF2F);
        ldy(dp_x(0x00));
        do {
            lda8(bus8(0xCC0000 + g_cpu.X));
            bus8_set(dba((u32)dp16(0x08) + g_cpu.Y), (u8)g_cpu.A);
            inx(); iny();
            u8 n = (u8)(dp8(0x04) + 1);
            dp8_set(0x04, n);
            lda8(n);
            dc_cmp8(n, dp8(0x0A));
        } while (!g_cpu._flag_C);
        tdc();
        dp8_set(0x04, (u8)g_cpu.A);
        u8 s = (u8)(dp8(0x02) + 1);
        dp8_set(0x02, s);
        lda8(s);
        dc_cmp8(s, 0x03);
    } while (!g_cpu._flag_C);
}
/* One equipped piece: dp08 = its 6-byte CC record (index A * 6 + base), then the
 * stat raises of FD:B141 (table 29D7) and C1:CF69 (2A03). */
static void equip_piece(u16 base, u32 site)
{
    tax(); stx_dp(0x28);
    ldx(a_with(0x06)); stx_dp(0x2A);
    dc_C1C90B_glue();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0x2C), base));
    dp16_set(0x08, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(0x29D7); stx_dp(0x0E);
    dc_jsl(0xFDB141, site);
    ldx(0x2A03); stx_dp(0x0E);
    dc_C1CF69_glue();
}
/* C1:CE3A  Recomputes the stats of party slot dp02 (unless AEFF+slot is FF): the
 * base stats 0B..11 go to the current ones 36..3C, then the weapon (29), armor
 * (28, ids from 5A) and helmet (27, ids from 7B) add their bonuses. */
DC_ENTRY(C1CE3A, RTS)
{
    static const u8 from[] = { 0x0B, 0x0D, 0x0C, 0x0F, 0x10, 0x0E, 0x11 };
    static const u8 to[]   = { 0x36, 0x38, 0x37, 0x3A, 0x3B, 0x39, 0x3C };
    tdc(); tax(); stx_dp(0x0A);
    ldx(dp_x(0x02));
    lda8(abs8(0xAEFF + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    dc_set_mx(0, g_cpu.x_flag);
    lda16(g_cpu.X); asl_a(1); tax();
    lda16(bus16(0xFDA80B + g_cpu.X));
    dp16_set(0x00, g_cpu.A);
    tax(); tdc();
    dc_set_mx(1, g_cpu.x_flag);
    for (int k = 0; k < 7; k++) {
        lda8(abs8(from[k] + g_cpu.X));
        abs8_set(to[k] + g_cpu.X, (u8)g_cpu.A);
    }
    tdc();
    lda8(abs8(0x0029 + g_cpu.X));
    equip_piece(0x06A4, 0xC1CEA4);
    tdc(); ldx(dp_x(0x00));
    lda8(abs8(0x0028 + g_cpu.X)); sub_a(0x5A);
    equip_piece(0x08C0, 0xC1CED6);
    tdc(); ldx(dp_x(0x00));
    lda8(abs8(0x0027 + g_cpu.X)); sub_a(0x7B);
    equip_piece(0x0986, 0xC1CF08);
}

/* ---- animation ops C1:99BE / C1:9B8D ----------------------------------------- */
/* Next byte of the animation record (CC:0000 + ++B1D2). */
static u8 anim_byte(void)
{
    ldx(abs_x(0xB1D2)); inx(); stx_abs(0xB1D2);
    lda8(bus8(0xCC0000 + g_cpu.X));
    return (u8)g_cpu.A;
}
/* C1:99BE  Animation op: the actor AEC8 acts on its own object (5E15 + slot):
 * AD8E / AECC = it, AD8D / AECB = 1 (B3B9 = 1 for a party member), screen flag
 * AE95 (C1:AD09); operand 1 (also dp0E / AEE4) = 1 picks B18C from 5E48 of the
 * object slot+3; operand 2 to AEE5, record pick FD:AB01, then AC89 / ACCE. */
DC_ENTRY(C199BE, RTS)
{
    tdc();
    abs8_set(0xB18C, (u8)g_cpu.A);
    lda8(abs8(0xB1FC) | 0x02); abs8_set(0xB1FC, (u8)g_cpu.A);
    lda8(abs8(0xAEC8)); tax();
    lda8(abs8(0x5E15 + g_cpu.X));
    abs8_set(0xAD8E, (u8)g_cpu.A); abs8_set(0xAECC, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xAD8D, 0x01); abs8_set(0xAECB, 0x01);
    lda8(abs8(0xAD8E));
    dc_cmp8((u8)g_cpu.A, 0x03);
    if (!g_cpu._flag_C) { lda8(0x01); abs8_set(0xB3B9, 0x01); }
    dc_C1AD09_glue();
    u8 op = anim_byte();
    dp8_set(0x0E, op); abs8_set(0xAEE4, op);
    dc_cmp8(op, 0x01);
    if (g_cpu._flag_Z) {
        tdc();
        lda8(abs8(0xAEC8));
        g_cpu._flag_C = 0;
        dc_set_a8(dc_adc8((u8)g_cpu.A, 0x03));
        dc_set_mx(0, g_cpu.x_flag);
        xba16(); lsr_a(1); tax(); tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0x5E48 + g_cpu.X)); abs8_set(0xB18C, (u8)g_cpu.A);
    }
    abs8_set(0xAEE5, anim_byte());
    dc_jsl(0xFDAB01, 0xC19A25);
    g_cpu._flag_C = 0;
    lda8(abs8(0xAEC8));
    dc_C1AC89_glue();
    dc_C1ACCE_glue();
    lda8(0x00); abs8_set(0xB3B8, 0x00);
}
/* X = word of the FD:A80B object table entry for slot A. */
static void obj_of(u8 slot)
{
    tdc(); lda8(slot); asl_a(1); tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xFDA80B + g_cpu.X)); tax();
}
/* C1:9B8D  Animation op (id, AEE4, reload): the target enemy B18B is replaced by
 * the enemy slot 3-10 whose id AF0A is the operand, not shown yet (AEFF differs),
 * linked to itself or to B18B (AE6D) and waiting (AF12 bit 7). It is shown (AEFF =
 * id, AF12 bit 7 off, AECC / AECB, C1:AD09), B18B is hidden (AEFF = FF, AF12 bit
 * 7 on) and their AE6D links swap; then the slot is reloaded (FD:B438) or takes
 * B18B's word 03. No such slot: AF24 = 2. AEE3 = 5, B3B8 = 1. */
DC_ENTRY(C19B8D, RTS)
{
    tdc();
    ldx(abs_x(0xB1D2));
    lda8(bus8(0xCC0001 + g_cpu.X)); abs8_set(0xAECC, (u8)g_cpu.A);
    lda8(bus8(0xCC0002 + g_cpu.X)); abs8_set(0xAEE4, (u8)g_cpu.A);
    lda8(bus8(0xCC0003 + g_cpu.X)); dp8_set(0x12, (u8)g_cpu.A);
    lda8(0x02); abs8_set(0xAEE3, 0x02);
    dc_C1ACF2_glue();
    tdc();
    ldx(0x0003);
    for (;;) {
        lda8(abs8(0xAECC));
        dc_cmp8((u8)g_cpu.A, abs8(0xAF0A + g_cpu.X));
        if (g_cpu._flag_Z) {
            dc_cmp8((u8)g_cpu.A, abs8(0xAEFF + g_cpu.X));
            if (!g_cpu._flag_Z) {
                stx_dp(0x0E);
                lda8(abs8(0xAE6D + g_cpu.X));
                dc_cmp8((u8)g_cpu.A, dp8(0x0E));
                if (!g_cpu._flag_Z) dc_cmp8((u8)g_cpu.A, abs8(0xB18B));
                if (g_cpu._flag_Z) {
                    lda8(abs8(0xAF12 + g_cpu.X));
                    if (g_cpu._flag_N) break;
                }
            }
        }
        inx();
        cpx(0x000B);
        if (g_cpu._flag_C) {
            lda8(0x02); abs8_set(0xAF24, 0x02);
            goto done;
        }
    }
    lda8(abs8(0xAF12 + g_cpu.X) & 0x7F); abs8_set(0xAF12 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xAECC)); abs8_set(0xAEFF + g_cpu.X, (u8)g_cpu.A);
    lda8((u8)g_cpu.X); abs8_set(0xAECC, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0xAECB, 0x01);
    dc_C1AD09_glue();
    lda8(abs8(0xB18B)); tax();
    lda8(0xFF); abs8_set(0xAEFF + g_cpu.X, 0xFF);
    lda8(abs8(0xAECC)); tay();
    lda8(abs8(0xAE6D + g_cpu.Y)); dp8_set(0x0E, (u8)g_cpu.A);
    lda8(abs8(0xAE6D + g_cpu.X)); abs8_set(0xAE6D + g_cpu.Y, (u8)g_cpu.A);
    lda8(dp8(0x0E)); abs8_set(0xAE6D + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xAF12 + g_cpu.X) | 0x80); abs8_set(0xAF12 + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x12));
    if (!g_cpu._flag_Z) {
        tdc();
        lda8(abs8(0xAECC)); sub_a(0x03);
        tax(); stx_dp(0x02);
        dc_jsl(0xFDB438, 0xC19C30);
    } else {
        obj_of(abs8(0xB18B));
        ldy(abs16(0x0003 + g_cpu.X));
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        obj_of(abs8(0xAECC));
        lda16(g_cpu.Y);
        abs16_set(0x0003 + g_cpu.X, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
done:
    lda8(0x05); abs8_set(0xAEE3, 0x05);
    lda8(0x01); abs8_set(0xB3B8, 0x01);
}

/* ---- damage application ------------------------------------------------------ */
/* Subtracts the hits k = 0..2 (B328 + 2C * k, word per target, flags word B32A
 * bit 14 = MP) of target Y whose MP flag equals mp from the word at obj+field. */
static void apply_hits(u16 field, int mp)
{
    for (int k = 0; k < 3; k++) {
        u16 o = (u16)(0x2C * k);
        lda16(abs16(0xB32A + o + g_cpu.Y));
        if (((g_cpu.A & 0x4000) != 0) != mp) continue;
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(abs16(field + g_cpu.X), abs16(0xB328 + o + g_cpu.Y)));
        abs16_set(field + g_cpu.X, g_cpu.A);
    }
}
/* C1:EC7F  Applies the pending damage to the 11 battle objects (5E00 + 80 * n):
 * HP 5E30 (unless the first hit is an MP one) clamped to the max 5E32; reaching 0
 * (or below) knocks out a present object once (5E4A bit 7; an enemy gives its
 * rewards, FD:ABA2) and skips its MP. MP 5E34 is clamped to 0..5E36. Then the
 * damage lists are cleared (FD:ACFD). */
DC_ENTRY(C1EC7F, RTS)
{
    dc_C1EC39_glue();
    tdc(); tax(); tay();
    do {
        lda8(abs8(0xB32B + g_cpu.Y));
        if (!(g_cpu.A & 0x40)) {
            dc_set_mx(0, g_cpu.x_flag);
            apply_hits(0x5E30, 0);
            lda16(abs16(0x5E30 + g_cpu.X));
            if (!g_cpu._flag_N && g_cpu.A != 0) {
                dc_cmp16(g_cpu.A, abs16(0x5E32 + g_cpu.X));
                if (g_cpu._flag_C) lda16(abs16(0x5E32 + g_cpu.X));
                abs16_set(0x5E30 + g_cpu.X, g_cpu.A);
                tdc();
                dc_set_mx(1, g_cpu.x_flag);
            } else {
                tdc();
                dc_set_mx(1, g_cpu.x_flag);
                lda8(abs8(0x5E4A + g_cpu.X));
                if (!(g_cpu.A & 0x80) && abs8(0xAEFF + (g_cpu.Y >> 2)) != 0xFF) {
                    if (g_cpu.Y >= 0x000C) {
                        phx(); dc_push16(g_cpu.Y);
                        dc_set_a8((u8)(g_cpu.Y >> 2));
                        dp8_set(0x0E, (u8)g_cpu.A);
                        dc_jsl(0xFDABA2, 0xC1ECFE);
                        ldy(dc_pull16()); plx();
                    }
                    lda8(abs8(0x5E4A + g_cpu.X) | 0x80); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
                }
                abs16_set(0x5E30 + g_cpu.X, 0);
                goto next;
            }
        }
        dc_set_mx(0, g_cpu.x_flag);
        apply_hits(0x5E34, 1);
        lda16(abs16(0x5E34 + g_cpu.X));
        if (g_cpu._flag_N) tdc();
        else {
            dc_cmp16(g_cpu.A, abs16(0x5E36 + g_cpu.X));
            if (g_cpu._flag_C) lda16(abs16(0x5E36 + g_cpu.X));
        }
        abs16_set(0x5E34 + g_cpu.X, g_cpu.A);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    next:
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.Y, 0x0004)); tay();
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.X, 0x0080)); tax();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        cpx(0x0580);
    } while (!g_cpu._flag_C);
    dc_jsl(0xFDACFD, 0xC1ED84);
}

/* ---- battle setup C1:FA8B ------------------------------------------------------ */
static void inc_dp(u8 o) { u8 v = (u8)(dp8(o) + 1); dp8_set(o, v); dc_set_flags_nz8(v); }
/* Item records copied into each party record by C1:CF15: id field, destination, size. */
static const struct { u8 id, dst, n; } equip_copy[] = {
    { 0x29, 0x43, 6 }, { 0x27, 0x70, 3 }, { 0x28, 0x48, 3 }, { 0x2A, 0x4B, 4 },
};
/* One party slot (dp10 = 2 * slot, dp12 = its record) of the setup's stat pass:
 * 3E = 37 + 48 + 70 (capped at FF), the accessory raise FD:B14D when 4C bit 6 and
 * 4D are set, then 3D = (36, or 3A for characters 1 and 2) + CC:0000 + record. The
 * weapon offset 0262 + 6 * weapon (29) is computed but X is reloaded with the
 * record before the CC read, and the 16-bit sum of two bytes never carries, so the
 * FF cap there is dead (quirks of the original). */
static void setup_stats(void)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xFDA80B + g_cpu.X));
    dp16_set(0x12, g_cpu.A);
    tay(); tdc();
    dp16_set(0x28, g_cpu.A); dp16_set(0x2A, g_cpu.A); dp16_set(0x2C, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0x0037 + g_cpu.Y)); dp8_set(0x28, (u8)g_cpu.A);
    lda8(abs8(0x0048 + g_cpu.Y)); dp8_set(0x2A, (u8)g_cpu.A);
    lda8(abs8(0x0070 + g_cpu.Y));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x28)));
    lda16(dc_adc16(g_cpu.A, dp16(0x2A)));
    dc_cmp16(g_cpu.A, 0x00FF);
    if (g_cpu._flag_C) lda16(0x00FF);
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0x003E + g_cpu.Y, (u8)g_cpu.A);
    tdc();
    lda8(abs8(0x004C + g_cpu.Y));
    if (g_cpu.A & 0x40) {
        lda8(abs8(0x004D + g_cpu.Y));
        if (!g_cpu._flag_Z) {
            sty_dp(0x00);
            ldy(0x29D7); sty_dp(0x0E);
            dc_jsl(0xFDB14D, 0xC1FC92);
            ldy(dp_x(0x00));
        }
    }
    tdc();
    ldx(dp_x(0x12));
    lda8(abs8(0x0029 + g_cpu.X));
    tax(); stx_dp(0x28);
    ldx(a_with(0x06)); stx_dp(0x2A);
    dc_C1C90B_glue();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x2C));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, 0x0262)); tax();
    lda16(dp16(0x10)); lsr_a(1); tay();
    tdc(); dp16_set(0x28, g_cpu.A); dp16_set(0x2A, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0xAEFF + g_cpu.Y));
    dc_cmp8((u8)g_cpu.A, 0x01);
    if (!g_cpu._flag_Z) dc_cmp8((u8)g_cpu.A, 0x02);
    u16 f = g_cpu._flag_Z ? 0x003A : 0x0036;
    ldx(dp_x(0x12));
    lda8(abs8(f + g_cpu.X)); dp8_set(0x28, (u8)g_cpu.A);
    lda8(bus8(0xCC0000 + g_cpu.X)); dp8_set(0x2A, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x28));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, dp16(0x2A)));
    if (g_cpu._flag_C) lda16(0x00FF);
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0x003D + g_cpu.X, (u8)g_cpu.A);
}
/* C1:FA8B  Battle setup: enemy slots (FD:B438 for each of AEC6), the party
 * (Ayla's fists follow her level, FD:AE52 / C1:FDAE), the slot tables AEFF / AF0A
 * and counts AEC5 / AEC7, the party objects and windows, the item list (C1:CDFF),
 * cleared battle state, stats (C1:CE3A, item records, setup_stats), HP copies
 * B316.., and per enemy slot the reward flags AE5D (80 = charm, 40 = drop item).
 * Some stores are dead: FD:B22E already zeroed 5FAD..63AC, FD:B4E7 rewrites B158..,
 * FD:B0D5 clears B044 and the armor copy overwrites the last weapon byte 48. */
DC_ENTRY(C1FA8B, RTS)
{
    static const u32 ae52[] = { 0xC1FAC1, 0xC1FAD9, 0xC1FAF1 };
    static const u32 b3fe[] = { 0xC1FB3D, 0xC1FB47, 0xC1FB51 };
    static const u32 b363[] = { 0xC1FD41, 0xC1FD48, 0xC1FD4F };
    static const u32 tail[] = { 0xFDB0D5, 0xFDB4E7, 0xFDB7EB, 0xFDB555, 0xFDB732, 0xFDACFD };
    dc_set_mx(1, 0);
    dc_jsl(0xFDB2DE, 0xC1FA8F);
    dc_jsl(0xFDB22E, 0xC1FA93);
    tdc(); tax(); tay();
    do { abs8_set(0x5FAD + g_cpu.Y, (u8)g_cpu.A); iny(); cpy(0x0400); } while (!g_cpu._flag_C);
    tdc(); tax(); tay();
    stx_dp(0x02); stx_dp(0x04);
    do {
        ldx(dp_x(0x02));
        dc_jsl(0xFDB438, 0xC1FAAC);
        inc_dp(0x02);
        lda8(abs8(0xAEC6));
        dc_cmp8((u8)g_cpu.A, dp8(0x02));
    } while (!g_cpu._flag_Z);
    dc_jsl(0xFDB121, 0xC1FAB9);
    tdc();
    for (int k = 0; k < 3; k++) {
        u16 r = (u16)(0x80 * k);
        ldy((u16)k);
        dc_jsl(0xFDAE52, ae52[k]);
        lda8(abs8(0x5E2D + r));
        dc_cmp8((u8)g_cpu.A, 0x05);
        if (g_cpu._flag_Z) {
            tdc();
            lda8(abs8(0x5E3F + r));
            dc_C1FDAE_glue();
            abs8_set(0x5E56 + r, (u8)g_cpu.A);
        }
    }
    tdc();
    abs8_set(0xAEC5, (u8)g_cpu.A);
    tax();
    do {
        lda8(0xFF);
        abs8_set(0xAEFF + g_cpu.X, 0xFF); abs8_set(0xAF0A + g_cpu.X, 0xFF);
        lda8(abs8(0xA09B + g_cpu.X));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x2980 + g_cpu.X));
            if (!g_cpu._flag_N) {
                abs8_set(0xAEFF + g_cpu.X, (u8)g_cpu.A); abs8_set(0xAF0A + g_cpu.X, (u8)g_cpu.A);
                inc_abs(0xAEC5);
            }
        }
        inx();
        cpx(0x0003);
    } while (!g_cpu._flag_C);
    lda8(abs8(0xAEC5));
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.A, abs8(0xAEC6)));
    abs8_set(0xAEC7, (u8)g_cpu.A);
    tdc();
    for (int k = 0; k < 3; k++) {
        ldx((u16)k); ldy((u16)(0x80 * k));
        dc_jsl(0xFDB3FE, b3fe[k]);
    }
    dc_jsl(0xFDAE99, 0xC1FB55);
    dc_jsl(0xFDAD09, 0xC1FB59);
    ldx(0x0000); stx_dp(0x06);
    for (int k = 0; k < 3; k++) {
        if (k) inc_dp(0x06);
        dc_C1C96A_glue();
        dc_C1CA1A_glue();
        dc_C1CCCB_glue();
    }
    dc_C1CDFF_glue();
    dc_jsl(0xFDAEF2, 0xC1FB84);
    tdc(); tax();
    do { abs8_set(0x93EE + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0012); } while (!g_cpu._flag_C);
    abs8_set(0xB188, 0); abs8_set(0xB189, 0); abs8_set(0xB18A, 0);
    tdc(); tax();
    abs8_set(0xAD8D, (u8)g_cpu.A);
    lda8(0xFF);
    do { abs8_set(0xAD8E + g_cpu.X, 0xFF); inx(); cpx(0x000C); } while (!g_cpu._flag_Z);
    dc_jsl(0xFDACEE, 0xC1FBAC);
    tdc(); tax();
    abs8_set(0xAEBB, (u8)g_cpu.A);
    static const u16 zw[] = { 0xAEBD, 0xAEBF, 0xAEC1, 0xAEC3, 0xB186 };
    for (int k = 0; k < 5; k++) stx_abs(zw[k]);
    do { abs8_set(0xAF27 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x011E); } while (!g_cpu._flag_Z);
    tdc(); tax();
    lda8(0xFF);
    do {
        abs8_set(0xB163 + g_cpu.X, 0xFF); abs8_set(0xB16E + g_cpu.X, 0xFF);
        abs8_set(0xB179 + g_cpu.X, 0);
        abs8_set(0xB158 + g_cpu.X, 0xFF); abs8_set(0xAFAB + g_cpu.X, 0xFF);
        inx(); cpx(0x000B);
    } while (!g_cpu._flag_Z);
    do { abs8_set(0xB179 + g_cpu.X, 0); inx(); cpx(0x000D); } while (!g_cpu._flag_Z);
    tdc(); tax();
    stx_dp(0x02);
    for (int k = 0; k < 3; k++) {
        if (k) inc_dp(0x02);
        dc_C1CE3A_glue();
    }
    tdc(); tax();
    stx_dp(0x06); stx_dp(0x08); stx_dp(0x0A);
    for (int k = 0; k < 4; k++) {
        lda8(equip_copy[k].id); dp8_set(0x06, equip_copy[k].id);
        lda8(equip_copy[k].dst); dp8_set(0x08, equip_copy[k].dst);
        lda8(equip_copy[k].n); dp8_set(0x0A, equip_copy[k].n);
        dc_C1CF15_glue();
    }
    tdc(); tax();
    stx_dp(0x10);
    abs8_set(0xAE56, (u8)g_cpu.A);
    do {
        setup_stats();
        tdc();
        ldx(dp_x(0x10)); inx(); inx(); stx_dp(0x10);
        cpx(0x0006);
    } while (!g_cpu._flag_C);
    for (int k = 0; k < 6; k++) dc_jsl(tail[k], 0xC1FCFE + 4 * k);
    tdc(); tax();
    dc_C1B093_glue();
    ldx(0x0080); dc_C1B093_glue();
    ldx(0x0100); dc_C1B093_glue();
    dc_set_mx(0, g_cpu.x_flag);
    static const u16 hp_from[] = { 0x5E32, 0x5EB2, 0x5F32 };
    for (int k = 0; k < 3; k++) { lda16(abs16(hp_from[k])); abs16_set(0xB316 + 2 * k, g_cpu.A); }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    for (int k = 0; k < 3; k++) { ldy((u16)k); dc_jsl(0xFDB363, b363[k]); }
    tdc(); tax();
    stx_abs(0xB3C0); stx_abs(0xB3C2); stx_abs(0xB3C4);
    stx_dp(0x00);
    do {
        tdc();
        ldx(dp_x(0x00));
        abs8_set(0xAE5D + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0xAF02 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            tax(); stx_dp(0x28);
            ldx(0x0007); stx_dp(0x2A);
            dc_C1C90B_glue();
            ldx(dp_x(0x2C)); stx_dp(0x02);
            ldx(dp_x(0x02));
            lda8(bus8(0xCC5E05 + g_cpu.X));
            if (!g_cpu._flag_Z) {
                ldx(dp_x(0x00));
                lda8(0x80); abs8_set(0xAE5D + g_cpu.X, 0x80);
            }
            ldx(dp_x(0x02));
            lda8(bus8(0xCC5E04 + g_cpu.X));
            if (!g_cpu._flag_Z) {
                ldx(dp_x(0x00));
                lda8(abs8(0xAE5D + g_cpu.X) | 0x40); abs8_set(0xAE5D + g_cpu.X, (u8)g_cpu.A);
            }
        }
        inc_dp(0x00);
        lda8(dp8(0x00));
        dc_cmp8((u8)g_cpu.A, 0x08);
    } while (!g_cpu._flag_C);
    dc_jsl(0xFDB223, 0xC1FDA5);
    dc_jsl(0xFDAEC4, 0xC1FDA9);
}
DC_ENTRY(C1AC85, RTS) { dc_C1EC7F_glue(); }

/* ---- party window animation C1:369A ------------------------------------------ */
/* C1:369A  Every other call (dpE5 toggles): for party slots 0-2 that are not
 * knocked out (5E4A bit 7, unless 5DDE is set) and animated (96F5), counts down
 * 5DBD; at 0 it steps the frame 5DC1 (stops at 8) and loads its duration from
 * 922D + 8 * slot; a nonzero one redraws the window (9679 = slot, 967B = page
 * 9245.., C1:1C4A). dpA4 is set while it runs. */
DC_ENTRY(C1369A, RTS)
{
    lda8(dp8(0xE5));
    if (g_cpu._flag_Z) { u8 v = (u8)(dp8(0xE5) + 1); dp8_set(0xE5, v); dc_set_flags_nz8(v); return; }
    dp8_set(0xE5, 0);
    lda8(0x01); dp8_set(0xA4, 0x01);
    tdc(); tax();
    stx_abs(0x5DC5);
    do {
        bool live = true;
        lda8(abs8(0x5DDE));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x5DC5));
            dc_jsl(0xCFFD9E, 0xC136B4);
            ldx(dp_x(0xA2));
            lda8(abs8(0x5E4A + g_cpu.X));
            live = !g_cpu._flag_N;
        }
        if (live) {
            ldx(abs_x(0x5DC5));
            lda8(abs8(0x96F5 + g_cpu.X));
            if (!g_cpu._flag_Z) {
                dec_abs(0x5DBD + g_cpu.X);
                if (g_cpu._flag_Z) {
                    inc_abs(0x5DC1 + g_cpu.X);
                    lda8(abs8(0x5DC1 + g_cpu.X));
                    dc_cmp8((u8)g_cpu.A, 0x08);
                    if (!g_cpu._flag_Z) {
                        lda8(abs8(0x5DC1 + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A);
                        lda8(abs8(0x5DC5));
                        dc_set_a8(asl8(asl8(asl8((u8)g_cpu.A))));
                        g_cpu._flag_C = 0;
                        dc_set_a8(dc_adc8((u8)g_cpu.A, dp8(0x80)));
                        tay();
                        lda8(abs8(0x922D + g_cpu.Y));
                        abs8_set(0x5DBD + g_cpu.X, (u8)g_cpu.A);
                        if (!g_cpu._flag_Z) {
                            lda8(abs8(0x5DC5)); abs8_set(0x9679, (u8)g_cpu.A);
                            lda8(abs8(0x9245 + g_cpu.Y)); abs8_set(0x967B, (u8)g_cpu.A);
                            dc_C11C4A_glue();
                        } else lda8(abs8(0x5DDD));
                    }
                }
            }
        }
        inc_abs(0x5DC5);
        lda8(abs8(0x5DC5));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_Z);
    dp8_set(0xA4, 0);
    dp8_set(0xE5, 0);
}

/* ---- battle cursor sprites C1:17DD ---------------------------------------- */
/* OAM entry oam (0700..) on battle object X: x = 1D0C + 9708 - 10, y = 1D23 + 9713. */
static void cursor_at(u32 oam)
{
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D0C + g_cpu.X), abs8(0x9708 + g_cpu.X)));
    sub_a(0x10);
    abs8_set(oam, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x1D23 + g_cpu.X), abs8(0x9713 + g_cpu.X)));
    abs8_set(oam + 1, (u8)g_cpu.A);
}
/* OAM entries 0-3 to tile 32 / attribute 0 (n of them), high bits 0900 |= 55. */
static void cursor_tiles(int n)
{
    lda8(abs8(0x0900) | 0x55); abs8_set(0x0900, (u8)g_cpu.A);
    lda8(0x32);
    for (int k = 0; k < n; k++) abs8_set(0x0703 + 4 * k, 0x32);
    for (int k = 0; k < n; k++) abs8_set(0x0702 + 4 * k, 0);
}
static void cursors_off(void)
{
    lda8(0xF0);
    abs8_set(0x0701, 0xF0); abs8_set(0x0705, 0xF0); abs8_set(0x0709, 0xF0); abs8_set(0x070D, 0xF0);
}
/* Menu cursor (95DB = 1): hand on the actor 95D5 and on the target(s) 9EE7 (low and
 * high nibble, F = none) once the menu has opened (9EE3 >= 39); else C1:1115. */
static void menu_cursor(void)
{
    lda8(abs8(0x9EE3)); dc_cmp8((u8)g_cpu.A, 0x39);
    bool open = g_cpu._flag_C;
    if (open) { lda8(abs8(0x9EE7)); dc_cmp8((u8)g_cpu.A, 0xFF); open = !g_cpu._flag_Z; }
    if (!open) { dc_C11115_glue(); return; }
    lda8(abs8(0x0900) | 0x55); abs8_set(0x0900, (u8)g_cpu.A);
    cursors_off();
    lda8(0x32);
    abs8_set(0x0703, 0x32); abs8_set(0x0707, 0x32); abs8_set(0x070B, 0x32);
    abs8_set(0x0702, 0); abs8_set(0x0706, 0); abs8_set(0x070A, 0);
    lda8(abs8(0x95D5)); tax(); cursor_at(0x0700);
    lda8(abs8(0x9EE7) & 0x0F); tax(); cursor_at(0x0704);
    lda8(abs8(0x0900) & 0xFE); lda8((u8)g_cpu.A & 0xFB); abs8_set(0x0900, (u8)g_cpu.A);
    lda8(abs8(0x9EE7) & 0xF0);
    dc_cmp8((u8)g_cpu.A, 0xF0);
    if (g_cpu._flag_Z) return;
    dc_C1011A_glue();
    tax(); cursor_at(0x0708);
    lda8(abs8(0x0900) & 0xEF); abs8_set(0x0900, (u8)g_cpu.A);
}
/* 0E80.. tile map icon at the CC:FAE9 offset of entry e: 4 tiles of t (attribute a). */
static void page_icon(u8 e, const u8 *t, u8 a)
{
    dc_set_a8(asl8(e)); tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xCCFAE9 + g_cpu.X)); tax(); tdc();
    dc_set_mx(1, g_cpu.x_flag);
    static const u16 at[4] = { 0x0E80, 0x0E82, 0x0EC0, 0x0EC2 };
    for (int k = 0; k < 4; k++) { lda8(t[k]); abs8_set(at[k] + g_cpu.X, t[k]); }
    if (a) { lda8(a); for (int k = 0; k < 4; k++) abs8_set(at[k] + 1 + g_cpu.X, a); }
}
/* Page arrows (95DB >= 2): the scroll marks 0EFC/0FFC for the position 95E6 (0 =
 * top, FA = bottom) and the page icon moved from 991F to 95E5; FD:FD6A uploads
 * the map when anything changed. */
static void page_marks(void)
{
    dp8_set(0x80, 0);
    lda8(abs8(0x95E6)); dc_cmp8((u8)g_cpu.A, abs8(0x9920));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0x95E6));
        u8 a, b, c, d;
        if (g_cpu._flag_Z) { a = 0xFF; b = 0x7F; c = 0xA9; d = 0xA9; }
        else {
            dc_cmp8((u8)g_cpu.A, 0xFA);
            if (g_cpu._flag_Z) { a = 0x7F; b = 0xFF; c = 0x29; d = 0x29; }
            else { a = 0x7F; b = 0x7F; c = 0x29; d = 0xA9; }
        }
        abs8_set(0x0EFC, a); abs8_set(0x0FFC, b); abs8_set(0x0EFD, c); abs8_set(0x0FFD, d);
        lda8(d);
        inc_dp(0x80);
    }
    lda8(abs8(0x95E5)); dc_cmp8((u8)g_cpu.A, abs8(0x991F));
    bool move = !g_cpu._flag_Z;
    if (!move) {
        lda8(abs8(0x95E6)); dc_cmp8((u8)g_cpu.A, abs8(0x9920));
        if (!g_cpu._flag_Z) { abs8_set(0x9920, (u8)g_cpu.A); move = true; }
    }
    if (move) {
        static const u8 blank[4] = { 0xFF, 0xFF, 0xFF, 0xFF }, icon[4] = { 0x60, 0x61, 0x62, 0x63 };
        lda8(abs8(0x991F));
        if (g_cpu._flag_N) tdc();
        page_icon((u8)g_cpu.A, blank, 0);
        lda8(abs8(0x95E5)); abs8_set(0x991F, (u8)g_cpu.A);
        page_icon((u8)g_cpu.A, icon, 0x29);
        inc_dp(0x80);
    }
    lda8(dp8(0x80));
    if (!g_cpu._flag_Z) dc_jsl(0xCFFD6A, 0xC11972);
}
/* Target cursors (9609 set, not while 9613 bit 7): one target A62D, or the list
 * A62D.. from 960B (up to three per frame, wrapping at 6), sprites 0700.. with the
 * CC:FB8D high-bit masks. A single enemy target (3+) outside menus shows its name
 * (CD:0030) or, with 9F34 / 9F29 set, its HP 5E30..33 (CD:002D); A4EE / 960A
 * make dpEC follow the cursor height. */
static void target_cursors(void)
{
    lda8(abs8(0x9613));
    if (g_cpu._flag_N) return;
    lda8((u8)(abs8(0x95DB) - 1));
    if (g_cpu._flag_Z) dc_C10EBA_glue();
    cursor_tiles(4);
    lda8(abs8(0xA62E));
    if (g_cpu._flag_N) {
        lda8(abs8(0xA62D));
        if (g_cpu._flag_N) return;
        abs8_set(0xA64E, (u8)g_cpu.A);
        tax(); cursor_at(0x0700);
        lda8(abs8(0x95DB));
        if (g_cpu._flag_Z) {
            lda8(abs8(0xA64E)); dc_cmp8((u8)g_cpu.A, 0x03);
            if (g_cpu._flag_C) {
                dc_set_a8(asl8((u8)g_cpu.A)); tax();
                lda8(abs8(0x984D + g_cpu.X)); abs8_set(0x0200, (u8)g_cpu.A);
                lda8(bus8(0xCCF8ED + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A);
                lda8(bus8(0xCCF8EE + g_cpu.X)); dp8_set(0x81, (u8)g_cpu.A);
                bool hp = false;
                lda8(abs8(0x9F34));
                if (!g_cpu._flag_Z) { lda8(abs8(0xA64E)); tay(); lda8(abs8(0x9F29 + g_cpu.Y)); hp = !g_cpu._flag_Z; }
                if (hp) {
                    ldx(dp_x(0x80));
                    for (int k = 0; k < 4; k++) { lda8(abs8(0x5E30 + k + g_cpu.X)); abs8_set(0x0201 + k, (u8)g_cpu.A); }
                }
                lda8(abs8(0x0200)); tax();
                lda8(bus8(0xE1DE80 + g_cpu.X));
                if (!g_cpu._flag_Z) lda8(0xFF); else lda8(abs8(0xA64E));
                if (hp) dc_jsl(0xCD002D, 0xC11A47); else dc_jsl(0xCD0030, 0xC11A16);
            }
        }
        lda8(abs8(0x0900) & 0xFE); abs8_set(0x0900, (u8)g_cpu.A);
        lda8(abs8(0xA4EE));
        if (g_cpu._flag_Z) return;
        abs8_set(0xA4EE, 0);
        lda8(abs8(0x960A));
        if (g_cpu._flag_Z) return;
        lda8(dp8(0xEC));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x0701)); dc_cmp8((u8)g_cpu.A, 0x2D);
            if (!g_cpu._flag_C) inc_dp(0xEC);
        } else {
            lda8(abs8(0x0701)); dc_cmp8((u8)g_cpu.A, 0x9C);
            if (g_cpu._flag_C) dp8_set(0xEC, 0);
        }
        return;
    }
    lda8(abs8(0xA62D));
    if (!g_cpu._flag_N) {
        tax(); cursor_at(0x0700);
        lda8(abs8(0x0900) & bus8(0xCCFB8D)); abs8_set(0x0900, (u8)g_cpu.A);
    }
    ldy(0x0001); sty_dp(0x82);
    lda8(abs8(0x960B)); tay(); sty_dp(0x80);
    for (;;) {
        ldy(dp_x(0x80));
        lda8(abs8(0xA62D + g_cpu.Y));
        if (!g_cpu._flag_N) {
            tax();
            lda8(dp8(0x82)); dc_set_a8(asl8(asl8((u8)g_cpu.A))); tay();
            cursor_at(0x0700 + (u32)g_cpu.Y);
            ldx(dp_x(0x82));
            lda8(abs8(0x0900) & bus8(0xCCFB8D + g_cpu.X)); abs8_set(0x0900, (u8)g_cpu.A);
        }
        inc_dp(0x80);
        lda8(dp8(0x80));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (g_cpu._flag_Z) break;
        dc_cmp8((u8)g_cpu.A, 0x06);
        if (g_cpu._flag_Z) break;
        inc_dp(0x82);
        lda8(dp8(0x82));
        dc_cmp8((u8)g_cpu.A, 0x04);
        if (g_cpu._flag_Z) break;
    }
    lda8(dp8(0x80));
    dc_cmp8((u8)g_cpu.A, 0x06);
    if (g_cpu._flag_Z) tdc();
    abs8_set(0x960B, (u8)g_cpu.A);
}
/* C1:17DD  Battle cursor sprites for the frame: hidden while 95D5 bit 7, else the
 * target cursors (9609), the menu cursor (95DB = 1) or the page marks (95DB >= 2). */
DC_ENTRY(C117DD, RTS)
{
    lda8(abs8(0x95D5));
    if (g_cpu._flag_N) {
        lda8(abs8(0x0900) | 0x55); abs8_set(0x0900, (u8)g_cpu.A);
        cursors_off();
    } else {
        lda8(abs8(0x9609));
        if (!g_cpu._flag_Z) target_cursors();
        else {
            lda8(abs8(0x95DB));
            if (!g_cpu._flag_Z) {
                lda8((u8)(g_cpu.A - 1));
                if (g_cpu._flag_Z) menu_cursor(); else page_marks();
            }
        }
    }
    dp8_set(0xEE, 0); dp8_set(0xEF, 0);
}

/* ---- damage numbers C1:5ADB ------------------------------------------------- */
static void inx4(void) { inx(); inx(); inx(); inx(); }
/* Digit sprite k (tile 949C + k) at 922D + X: x = dp94 + offset dp8E + k, y = dp95,
 * attribute dp8C; mask clears its size bits in dp84. */
static void dmg_digit(int k, u8 mask)
{
    lda8(abs8(0x949C + k)); abs8_set(0x922F + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(dp8(0x94), dp8((u8)(0x8E + k)))); abs8_set(0x922D + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x95)); abs8_set(0x922E + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x8C)); abs8_set(0x9230 + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x84) & mask); dp8_set(0x84, (u8)g_cpu.A);
}
static bool blank_digit(int k) { lda8(abs8(0x949C + k)); dc_cmp8((u8)g_cpu.A, 0x90); return g_cpu._flag_Z; }
/* Damage numbers for the 11 objects (sprite rows of 16 bytes from 922D, the first
 * of CC:F829[9895 & 7]): object Y with an entry in AD9E (kind, value AD9C) gets
 * four digit sprites (90 = blank, leading ones skipped; kind 5 shows a fixed
 * word 9A 9B), centred by CC:F5C4 on its position (1D0C / 1D23, + 9708 / 9713
 * while A003 is set); kinds 1 / 2 use attribute 39 (healing), others 31, and all
 * but 1 / 3 set A0DC. */
static void dmg_numbers(void)
{
    abs8_set(0xA3D2, 0);
    inc_abs(0xA3D1);
    lda8(abs8(0x9895) & 0x07); abs8_set(0xA3D3, (u8)g_cpu.A);
    tax();
    lda8(bus8(0xCCF829 + g_cpu.X)); tax(); stx_dp(0x80);
    tdc(); tay();
    sty_dp(0x82); sty_dp(0x92);
    do {
        lda8(0x31); dp8_set(0x8C, 0x31);
        lda8(abs8(0xA003 + g_cpu.Y));
        if (g_cpu._flag_Z) {
            lda8(abs8(0x1D0C + g_cpu.Y)); dp8_set(0x94, (u8)g_cpu.A);
            lda8(abs8(0x1D23 + g_cpu.Y)); dp8_set(0x95, (u8)g_cpu.A);
        } else {
            g_cpu._flag_C = 0;
            lda8(dc_adc8(abs8(0x1D0C + g_cpu.Y), abs8(0x9708 + g_cpu.Y))); dp8_set(0x94, (u8)g_cpu.A);
            g_cpu._flag_C = 0;
            lda8(dc_adc8(abs8(0x1D23 + g_cpu.Y), abs8(0x9713 + g_cpu.Y))); dp8_set(0x95, (u8)g_cpu.A);
        }
        ldx(dp_x(0x80));
        lda8(abs8(0xAD9E + g_cpu.X));
        if (g_cpu._flag_Z) {
            g_cpu._flag_C = 0;
            lda8(dc_adc8(dp8(0x82), 0x10)); dp8_set(0x82, (u8)g_cpu.A);
        } else {
            dc_cmp8((u8)g_cpu.A, 0x05);
            if (g_cpu._flag_Z) {
                lda8(0x90); abs8_set(0x949B, 0x90); abs8_set(0x949C, 0x90); abs8_set(0x949D, 0x90);
                lda8(0x9A); abs8_set(0x949E, 0x9A);
                lda8(0x9B); abs8_set(0x949F, 0x9B);
                lda8(0x02); dp8_set(0x8E, 0x02);
            } else {
                dc_cmp8((u8)g_cpu.A, 0x03);
                if (!g_cpu._flag_Z) {
                    dc_cmp8((u8)g_cpu.A, 0x01);
                    bool heal = g_cpu._flag_Z;
                    if (!heal) {
                        lda8(0x01); abs8_set(0xA0DC + g_cpu.Y, 0x01);
                        lda8(abs8(0xAD9E + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0x02);
                        heal = g_cpu._flag_Z;
                    }
                    if (heal) { lda8(0x39); dp8_set(0x8C, 0x39); }
                }
                dc_set_mx(0, g_cpu.x_flag);
                lda16(abs16(0xAD9C + g_cpu.X)); abs16_set(0x9499, g_cpu.A);
                dc_jsl(0xCFFDAD, 0xC15BB9);
                dc_set_mx(1, g_cpu.x_flag);
                dp8_set(0x8E, 0);
                for (int k = 0; k < 3 && blank_digit(k); k++) dp8_set(0x8E, (u8)(dp8(0x8E) + 1));
            }
            lda8(dp8(0x8E)); dc_set_a8(asl8(asl8((u8)g_cpu.A))); tax();
            for (int k = 0; k < 4; k++) { lda8(bus8(0xCCF5C4 + k + g_cpu.X)); dp8_set((u8)(0x8E + k), (u8)g_cpu.A); }
            lda8(0x55); dp8_set(0x84, 0x55);
            ldx(dp_x(0x82));
            if (!blank_digit(0)) dmg_digit(0, 0xFC);
            inx4();
            if (!blank_digit(1) || !blank_digit(0)) dmg_digit(1, 0xF3);
            inx4();
            if (!blank_digit(2) || !blank_digit(1) || !blank_digit(0)) dmg_digit(2, 0xCF);
            inx4();
            dmg_digit(3, 0x3F);
            inx4();
            stx_dp(0x82);
            lda8(dp8(0x84)); abs8_set(0x92DD + g_cpu.Y, (u8)g_cpu.A);
            lda8(0x01); abs8_set(0xA3D4 + g_cpu.Y, 0x01);
        }
        ldx(dp_x(0x80)); inx4(); stx_dp(0x80);
        iny();
        cpy(0x000B);
    } while (!g_cpu._flag_Z);
    lda8(0x01); abs8_set(0xAC5A, 0x01);
}
/* C1:5ADB  Animation op: damage numbers. Step 0 clears the digit sprites 922D..
 * (F0), their size bits 92DD.. and A3B9..BC and copies the palettes 999F / 99A7 to
 * 2300 / 2380; step 1 builds the numbers (dmg_numbers); later steps wait for AC5A
 * to clear, then the op ends (A3D1 = 0, script + 1). */
DC_ENTRY(C15ADB, RTS)
{
    lda8(abs8(0xA3D1));
    if (g_cpu._flag_Z) {
        inc_abs(0xA3D1);
        ldx(0x00AF); lda8(0xF0);
        do { abs8_set(0x922D + g_cpu.X, 0xF0); dex(); } while (!g_cpu._flag_N);
        ldx(0x000A);
        do { abs8_set(0x92DD + g_cpu.X, 0); dex(); } while (!g_cpu._flag_N);
        abs8_set(0xA3B9, 0); abs8_set(0xA3BA, 0); abs8_set(0xA3BB, 0); abs8_set(0xA3BC, 0);
        tdc(); tay();
        do { lda8(abs8(0x999F + g_cpu.Y)); abs8_set(0x2300 + g_cpu.Y, (u8)g_cpu.A); iny(); cpy(0x0008); } while (!g_cpu._flag_Z);
        tdc(); tay();
        do { lda8(abs8(0x99A7 + g_cpu.Y)); abs8_set(0x2380 + g_cpu.Y, (u8)g_cpu.A); iny(); cpy(0x0008); } while (!g_cpu._flag_Z);
        tdc();
    } else {
        lda8(abs8(0xA3D1));
        dc_cmp8((u8)g_cpu.A, 0x01);
        if (g_cpu._flag_Z) { dmg_numbers(); tdc(); }
        else {
            lda8(abs8(0xAC5A));
            if (!g_cpu._flag_Z) tdc();
            else { abs8_set(0xA3D1, 0); lda8(0x01); }
        }
    }
    script_advance();
}

/* ---- battle window refresh C1:0C2D ------------------------------------------ */
/* Two digits of the word w of party slot k (C1:0174 / 104E) at r / r + 2 of the
 * window map, r + 0A / r + 0C blanked; with 96F5 + k = 0 only r + 6 is blanked. */
static void party_hp(int k, u16 w, u16 r)
{
    lda8(abs8(0x96F5 + k));
    if (g_cpu._flag_Z) { lda8(0xFF); abs8_set(r + 6, 0xFF); return; }
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(w)); abs16_set(0x9499, g_cpu.A);
    dc_C10174_glue();
    dc_C1104E_glue();
    lda8(abs8(0x949E)); abs8_set(r, (u8)g_cpu.A);
    lda8(abs8(0x949F)); abs8_set(r + 2, (u8)g_cpu.A);
    lda8(0xFF); abs8_set(r + 0x0A, 0xFF); abs8_set(r + 0x0C, 0xFF);
}
/* Two digits of the byte e (bit 7 = none; B rides along as the high byte) at r /
 * r + 2, the ones digit alone at r when the tens digit is the blank zero 73. */
static void cost_digits(u16 e, u16 r)
{
    lda8(abs8(e));
    if (g_cpu._flag_N) return;
    dc_set_mx(0, g_cpu.x_flag);
    abs16_set(0x9499, g_cpu.A);
    dc_C10174_glue();
    lda8(abs8(0x949E)); dc_cmp8((u8)g_cpu.A, 0x73);
    if (g_cpu._flag_Z) {
        lda8(0xFF); abs8_set(r + 2, 0xFF);
        lda8(abs8(0x949F)); abs8_set(r, (u8)g_cpu.A);
    } else {
        abs8_set(r, (u8)g_cpu.A);
        lda8(abs8(0x949F)); abs8_set(r + 2, (u8)g_cpu.A);
    }
}
/* C1:0C2D  Battle window refresh (FD:FD9E for the actor 95D5 first).  95DB = 0:
 * redraw the party window when A6DD changed (1C3A + 0299), else 05A7 and, with
 * A43F set, 0872; hidden actor (95D5 bit 7) also gets 1C3A; then FD:FD02 and 99E2++.
 * 95DB = 1 (menu open): 0EE1 / 0A88, the map 99E3.. (every other byte) into
 * A6E1.., six rows of nine CC:FA95 tiles from A70B, the digits of the three party
 * slots, the entry 9EE3..9EE9 (1A80 + CC:F395[actor] + CC:F3A1[9551 item], all FF
 * when the 9551 item is FF) with the digits of 9EE6 / 9EE8 / 9EE9, the 60..63 icon
 * at CC:FAE3[95DF] (unless 9609), CD:0027 while A0DB bit 7, FD:FD36, and the D1:5A50
 * colours into 0B40 when A86A is set.  Other 95DB values do nothing. */
DC_ENTRY(C10C2D, RTS)
{
    lda8(abs8(0x95D5));
    dc_jsl(0xCFFD9E, 0xC10C30);
    lda8(abs8(0x95DB));
    if (g_cpu._flag_Z) {
        lda8(abs8(0x95DB));
        if (!g_cpu._flag_Z) return;
        lda8(abs8(0xA6DD)); dc_cmp8((u8)g_cpu.A, abs8(0xA6DF));
        if (!g_cpu._flag_Z) {
            abs8_set(0xA6DF, (u8)g_cpu.A);
            dc_C11C3A_glue();
            dc_C10299_glue();
        } else {
            dc_C105A7_glue();
            lda8(abs8(0xA43F));
            if (!g_cpu._flag_Z) { lda8(abs8(0xA6DD)); if (!g_cpu._flag_N) dc_C10872_glue(); }
        }
        lda8(abs8(0x95D5));
        if (g_cpu._flag_N) dc_C11C3A_glue();
        dc_jsl(0xCFFD02, 0xC10C79);
        inc_abs(0x99E2);
        return;
    }
    dc_cmp8((u8)g_cpu.A, 0x01);
    if (!g_cpu._flag_Z) return;
    dc_C10EE1_glue();
    dc_C10A88_glue();
    tdc(); tay(); tax();
    do {
        lda8(abs8(0x99E3 + g_cpu.X)); abs8_set(0xA6E1 + g_cpu.Y, (u8)g_cpu.A);
        inx(); inx(); iny(); iny();
        cpy(0x0180);
    } while (!g_cpu._flag_Z);
    tdc(); tax(); tay();
    stx_dp(0x80);
    do {
        dp8_set(0x81, 0);
        do {
            lda8(bus8(0xCCFA95 + g_cpu.X)); abs8_set(0xA70B + g_cpu.Y, (u8)g_cpu.A);
            inx(); iny(); iny();
            inc_dp(0x81);
            lda8(dp8(0x81)); dc_cmp8((u8)g_cpu.A, 0x09);
        } while (!g_cpu._flag_Z);
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        g_cpu.A = g_cpu.Y;
        g_cpu.A = dc_adc16(g_cpu.A, 0x002E);
        tay();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        inc_dp(0x80);
        lda8(dp8(0x80)); dc_cmp8((u8)g_cpu.A, 0x06);
    } while (!g_cpu._flag_Z);
    party_hp(0, 0x5E34, 0xA751);
    party_hp(1, 0x5EB4, 0xA7D1);
    party_hp(2, 0x5F34, 0xA851);
    lda8(abs8(0x95D5)); tax();
    lda8(bus8(0xCCF38C + g_cpu.X)); dp8_set(0xAF, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    lda8(dc_adc8(abs8(0x95EB + g_cpu.X), abs8(0x95DF + g_cpu.X)));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0xAF)));
    tax();
    lda8(abs8(0x9551 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) {
        static const u16 none[] = { 0xA7DB, 0xA7DD, 0x9EE3, 0x9EE4, 0x9EE5, 0x9EE6, 0x9EE7, 0x9EE8, 0x9EE9 };
        sta_all(none, 9);
    } else {
        dp8_set(0x80, (u8)g_cpu.A);
        lda8(abs8(0x95D5)); dc_set_a8(asl8((u8)g_cpu.A)); tax();
        lda8(bus8(0xCCF395 + g_cpu.X)); dp8_set(0xAF, (u8)g_cpu.A);
        lda8(bus8(0xCCF396 + g_cpu.X)); dp8_set(0xB0, (u8)g_cpu.A);
        lda8(dp8(0x80)); tax();
        lda8(bus8(0xCCF3A1 + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0xAF), dp8(0x80))); dp8_set(0xAF, (u8)g_cpu.A);
        lda8(dc_adc8(dp8(0xB0), 0x00)); dp8_set(0xB0, (u8)g_cpu.A);
        ldx(dp_x(0xAF));
        for (int k = 0; k < 7; k++) { lda8(abs8(0x1A80 + k + g_cpu.X)); abs8_set(0x9EE3 + k, (u8)g_cpu.A); }
        cost_digits(0x9EE6, 0xA75B);
        cost_digits(0x9EE8, 0xA7DB);
        cost_digits(0x9EE9, 0xA85B);
    }
    lda8(abs8(0x95D5)); tax();
    lda8(abs8(0x95DF + g_cpu.X)); dp8_set(0x80, (u8)g_cpu.A);
    dc_C10EBA_glue();
    lda8(abs8(0x9609));
    if (g_cpu._flag_Z) {
        lda8(dp8(0x80)); dc_set_a8(asl8((u8)g_cpu.A)); tax();
        dc_set_mx(0, g_cpu.x_flag);
        lda16(bus16(0xCCFAE3 + g_cpu.X)); tay(); tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(0x60); abs8_set(0xA6E1 + g_cpu.Y, 0x60);
        lda8(0x61); abs8_set(0xA6E3 + g_cpu.Y, 0x61);
        lda8(0x62); abs8_set(0xA721 + g_cpu.Y, 0x62);
        lda8(0x63); abs8_set(0xA723 + g_cpu.Y, 0x63);
    }
    lda8(abs8(0xA0DB));
    if (g_cpu._flag_N) { lda8(abs8(0x9EE3)); dc_jsl(0xCD0027, 0xC10E95); }
    dc_jsl(0xCFFD36, 0xC10E99);
    lda8(abs8(0xA86A));
    if (g_cpu._flag_Z) return;
    tdc(); tax();
    do { lda8(bus8(0xD15A50 + g_cpu.X)); abs8_set(0x0B40 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0180); } while (!g_cpu._flag_Z);
    inc_abs(0x99E2);
    abs8_set(0xA86A, 0);
}
