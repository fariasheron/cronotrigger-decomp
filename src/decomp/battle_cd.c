/* Bank CD: battle animation scripts (techs, enemy attacks). The script
 * pointer is the long pointer at dp 40; the commands run with an 8-bit
 * accumulator and 16-bit indexes. */
#include "ops65.h"

static u8 arg8(void) { return bus8(lp(0x40)); }
static void store_arg(u16 a) { lda8(arg8()); abs8_set(a, (u8)g_cpu.A); }

/* ---- CD batch 1: one-byte script operands ---- */
DC_ENTRY(CD17DF, RTS) { store_arg(0xCE0E); }
DC_ENTRY(CD17FF, RTS) { store_arg(0xCD46); }
DC_ENTRY(CD18C2, RTS) { store_arg(0xCFFF); }
DC_ENTRY(CD1907, RTS) { store_arg(0xCD2E); }
DC_ENTRY(CD190D, RTS) { store_arg(0xCD2D); }
DC_ENTRY(CD1918, RTS) { store_arg(0xCD29); }
DC_ENTRY(CD1F4D, RTS) { store_arg(0xCA16); }
DC_ENTRY(CD20ED, RTS) { store_arg(0xBE99); }
DC_ENTRY(CD2FAB, RTS) { store_arg(0xC862); }
DC_ENTRY(CD2FB1, RTS) { store_arg(0xC863); }
DC_ENTRY(CD308C, RTS) { store_arg(0xBE91); }
DC_ENTRY(CD30B8, RTS) { store_arg(0xC865); }
DC_ENTRY(CD180A, RTS) { store_arg(0xCD45); inc_abs(0xCD44); }
DC_ENTRY(CD2687, RTS) { store_arg(0xCA25); inc_abs(0xCA24); }
/* (5D80 + X may cross into bank 7F, as abs,X does) */
DC_ENTRY(CD18C8, RTS)
{
    lda8(arg8()); tax();
    u32 a = 0x5D80u + g_cpu.X;
    u8 v = (u8)(abs8(a) + 1); abs8_set(a, v); dc_set_flags_nz8(v);
}
DC_ENTRY(CD182A, RTS) { lda8(arg8()); tax(); lda8(abs8(0x5D8F)); abs8_set(0xCAA4 + g_cpu.X, (u8)g_cpu.A); }
/* operand & CD46 (the channel mask) into a color/window register copy */
static void store_masked(u16 a) { lda8(arg8() & abs8(0xCD46)); abs8_set(a, (u8)g_cpu.A); }
DC_ENTRY(CD3017, RTS) { store_masked(0xBE8C); }
DC_ENTRY(CD3020, RTS) { store_masked(0xBE8B); }
DC_ENTRY(CD3029, RTS) { store_masked(0xBE8A); }
DC_ENTRY(CD3032, RTS) { store_masked(0xBE89); }
DC_ENTRY(CD3083, RTS) { store_masked(0xBE92); }
DC_ENTRY(CD1923, RTS)
{
    store_arg(0xCD24);
    ldx(0x0080); stx_abs(0xCD25);
    inc_abs(0xCD23);
}
DC_ENTRY(CD2DCB, RTS)
{
    ldx(dp16(0x43));
    lda8(arg8());
    abs8_set(0xCA38 + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0xCA39 + g_cpu.X, (u8)g_cpu.A);
}
DC_ENTRY(CD2AA1, RTS) { inc_abs(0xCE10); }
DC_ENTRY(CD2B5C, RTS) { lda8(abs8(0xCD00)); tax(); abs8_set(0x99B5 + g_cpu.X, 0); }
/* CD:1D4D  5D8F++ and the script pointer steps back one byte. */
DC_ENTRY(CD1D4D, RTS) { inc_abs(0x5D8F); ldy(dp16(0x40)); ldy((u16)(g_cpu.Y - 1)); dp16_set(0x40, g_cpu.Y); }
DC_ENTRY(CD2B05, RTS) { ldx(0x0060); stx_abs(0xCD3A); ldx(0x0040); stx_abs(0x007C); }
DC_ENTRY(CD2C7F, RTS) { tdc(); tax(); stx_abs(0xCAE4); stx_abs(0xCAE6); stx_abs(0xCAE8); }
/* CD:13E0 / 13E2  A = ((A >> 2) & 3) * 2 / (A & 3) * 2 */
DC_ENTRY(CD13E0, RTS) { lsr_a(2); lda8((u8)g_cpu.A & 0x03); asl_a(1); }
DC_ENTRY(CD13E2, RTS) { lda8((u8)g_cpu.A & 0x03); asl_a(1); }
/* CD:2969..298D  copies the word CA77+Y to CA7A / 7C / 7E / 80 + Y */
static void copy_ca77(u16 dst)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xCA77 + g_cpu.Y)); abs16_set(dst + g_cpu.Y, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD2969, RTS) { copy_ca77(0xCA7A); }
DC_ENTRY(CD2975, RTS) { copy_ca77(0xCA7C); }
DC_ENTRY(CD2981, RTS) { copy_ca77(0xCA7E); }
DC_ENTRY(CD298D, RTS) { copy_ca77(0xCA80); }

/* ---- CD batch 2 ---- */
DC_ENTRY(CD20D3, RTS) { dc_jsl(0xCEE000, 0xCD20D3); }
/* CD:0534  A = 1 when CA1F is set (else 0). */
DC_ENTRY(CD0534, RTL) { lda8(abs8(0xCA1F)); if (!g_cpu._flag_Z) lda8(0x01); }
/* CD:0B16  A = C0:FE00[A + $7C + C] (table lookup with the carry in). */
DC_ENTRY(CD0B16, RTS) { lda8(dc_adc8((u8)g_cpu.A, dp8(0x7C))); tax(); lda8(bus8(0xC0FE00 + g_cpu.X)); }
DC_ENTRY(CD2C8B, RTS) { inc_abs(0xCAEC); inc_abs(0xCAEC); dec_abs(0xCAF0); dec_abs(0xCAF0); }
static void copy_to_ca77(u16 src)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(src + g_cpu.Y)); abs16_set(0xCA77 + g_cpu.Y, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD29AA, RTS) { copy_to_ca77(0xCA7A); }
DC_ENTRY(CD29B6, RTS) { copy_to_ca77(0xCA7C); }
DC_ENTRY(CD29C2, RTS) { copy_to_ca77(0xCA7E); }
DC_ENTRY(CD29CE, RTS) { copy_to_ca77(0xCA80); }
/* CD:0ADE  Unless 0524 is set, counts CDC9 down, wrapping to 0D. */
DC_ENTRY(CD0ADE, RTS)
{
    lda8(abs8(0x0524));
    if (!g_cpu._flag_Z) return;
    dec_abs(0xCDC9);
    if (!g_cpu._flag_N) return;
    lda8(0x0D); abs8_set(0xCDC9, 0x0D);
}
DC_ENTRY(CD13DE, RTS) { lsr_a(4); lda8((u8)g_cpu.A & 0x03); asl_a(1); }
DC_ENTRY(CD13DC, RTS) { lsr_a(6); lda8((u8)g_cpu.A & 0x03); asl_a(1); }
DC_ENTRY(CD18CF, RTS)
{
    store_arg(0xCD24);
    lda8(abs8(0x5DA0)); tax(); stx_abs(0xCD25);
    inc_abs(0xCD23);
}
/* CD:1B6D  Script jump back: dp 40 = (operand word) - 1. */
DC_ENTRY(CD1B6D, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)(bus16(lp(0x40)) - 1)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD2A8B, RTS)
{
    tdc(); tax();
    do {
        u32 a = 0x9FFAu + g_cpu.X;
        u8 v = (u8)(abs8(a) + 1); abs8_set(a, v); dc_set_flags_nz8(v);
        inx(); cpx(0x0008);
    } while (!g_cpu._flag_Z);
}
/* CD:2F61  Script pointer = the saved one of the current thread (CA75+$43). */
DC_ENTRY(CD2F61, RTS)
{
    ldx(dp16(0x43));
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xCA75 + g_cpu.X)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:01A5  A = CD:019D[(2991 >> 3) & 7]. */
DC_ENTRY(CD01A5, RTS) { lda8(abs8(0x2991)); lsr_a(3); lda8((u8)g_cpu.A & 0x07); tax(); lda8(bus8(0xCD019D + g_cpu.X)); }
/* CD:1864  Backdrop color word (2000 and 2200) from the script. */
DC_ENTRY(CD1864, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(lp(0x40))); abs16_set(0x2000, g_cpu.A); abs16_set(0x2200, g_cpu.A);
    dp16_set(0x40, (u16)(dp16(0x40) + 1));
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
static void ca_add16(u16 a, int sub, int c_in)
{
    ldx(dp16(0x43));
    dc_set_mx(0, g_cpu.x_flag);
    if (c_in >= 0) g_cpu._flag_C = (u8)c_in;
    lda16(sub ? dc_sbc16(abs16(a + g_cpu.X), dp16(0x45)) : dc_adc16(abs16(a + g_cpu.X), dp16(0x45)));
    abs16_set(a + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD2E13, RTS) { ca_add16(0xCA60, 0, 0); }
DC_ENTRY(CD2E3C, RTS) { ca_add16(0xCA5E, 1, 1); }
DC_ENTRY(CD2E4D, RTS) { ca_add16(0xCA5E, 0, 0); }
DC_ENTRY(CD2E78, RTS) { ca_add16(0xCA60, 1, 1); }
DC_ENTRY(CD0E23, RTS)
{
    tdc(); tax();
    do {
        lda8(abs8(0xCCEE + g_cpu.X));
        abs8_set(0x2030 + g_cpu.X, (u8)g_cpu.A); abs8_set(0x2230 + g_cpu.X, (u8)g_cpu.A);
        inx(); cpx(0x0010);
    } while (!g_cpu._flag_Z);
}
DC_ENTRY(CD2D0C, RTS)
{
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCAEE), 0x02)); abs8_set(0xCAEE, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0xCAF2), 0x02)); abs8_set(0xCAF2, (u8)g_cpu.A);
}
/* CD:1834  Waits (script pointer stays) until CD3C wraps, then clears
 * 05A4/0598/058C/0580. */
DC_ENTRY(CD1834, RTS)
{
    inc_abs(0xCD3C);
    if (g_cpu._flag_Z) { abs8_set(0x05A4, 0); abs8_set(0x0598, 0); abs8_set(0x058C, 0); abs8_set(0x0580, 0); }
    ldy(dp16(0x40)); ldy((u16)(g_cpu.Y - 1)); dp16_set(0x40, g_cpu.Y);
}
DC_ENTRY(CD1E36, RTS)
{
    lda8(arg8() & 0x01); tax();
    lda8(arg8()); lsr_a(4);
    abs8_set(0xCAD1 + g_cpu.X, (u8)g_cpu.A);
}
/* CD:2EA2  Loop start: count (operand + 1) and the script position. */
DC_ENTRY(CD2EA2, RTS)
{
    ldx(dp16(0x43));
    lda8((u8)(arg8() + 1)); abs8_set(0xCA74 + g_cpu.X, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x40)); abs16_set(0xCA72 + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}

/* ---- CD batch 3 ---- */
DC_ENTRY(CD0003, RTL) { lda8(0x01); dp8_set(0x7E, 0x01); }
DC_ENTRY(CD0012, RTL) { dc_CD0534_glue(); }
/* CD:1B42 / 1B50  A clamped to 08..F8 / 08..D4 (screen limits). */
static void clamp(u8 lo, u8 hi)
{
    dc_cmp8((u8)g_cpu.A, lo);
    if (!g_cpu._flag_C) { lda8(lo); return; }
    dc_cmp8((u8)g_cpu.A, hi);
    if (g_cpu._flag_C) lda8(hi);
}
DC_ENTRY(CD1B42, RTS) { clamp(0x08, 0xF8); }
DC_ENTRY(CD1B50, RTS) { clamp(0x08, 0xD4); }
/* CD:1A8B  CAA4[n] -= 5D89, floored at 0. */
DC_ENTRY(CD1A8B, RTS)
{
    lda8(arg8()); tax();
    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0xCAA4 + g_cpu.X), abs8(0x5D89)));
    if (!g_cpu._flag_C) tdc();
    abs8_set(0xCAA4 + g_cpu.X, (u8)g_cpu.A);
}
/* CD:1A9C  CAA4[n] += 5D8C, capped at 5D8E. */
DC_ENTRY(CD1A9C, RTS)
{
    lda8(arg8()); tax();
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCAA4 + g_cpu.X), abs8(0x5D8C)));
    dc_cmp8((u8)g_cpu.A, abs8(0x5D8E));
    if (g_cpu._flag_C) lda8(abs8(0x5D8E));
    abs8_set(0xCAA4 + g_cpu.X, (u8)g_cpu.A);
}
DC_ENTRY(CD2CF5, RTS)
{
    lda8(0x80); abs8_set(0xCAEA, 0x80); abs8_set(0xCAEE, 0x80); abs8_set(0xCAF2, 0x80);
    abs8_set(0xCAEC, 0);
    lda8(0xD4); abs8_set(0xCAF0, 0xD4); abs8_set(0xCAF4, 0xD4);
}
DC_ENTRY(CD2C98, RTS)
{
    abs8_set(0xCAEA, 0); abs8_set(0xCAEE, 0);
    lda8(0x01); abs8_set(0xCAEC, 0x01);
    lda8(0x9F); abs8_set(0xCAF0, 0x9F);
    lda8(0x50); abs8_set(0xCAF4, 0x50);
    lda8(0xC0); abs8_set(0xCAF2, 0xC0);
}
/* CD:2B12  5DA1 clamped to 31..D4. */
DC_ENTRY(CD2B12, RTS)
{
    lda8(abs8(0x5DA1));
    dc_cmp8((u8)g_cpu.A, 0x31);
    if (!g_cpu._flag_C) lda8(0x31);
    else { dc_cmp8((u8)g_cpu.A, 0xD4); if (g_cpu._flag_C) lda8(0xD4); }
    abs8_set(0x5DA1, (u8)g_cpu.A);
}
static void step_back(void) { ldy(dp16(0x40)); ldy((u16)(g_cpu.Y - 1)); dp16_set(0x40, g_cpu.Y); }
/* CD:2EB5  Loop end: back to the saved position until the count runs out. */
DC_ENTRY(CD2EB5, RTS)
{
    ldx(dp16(0x43));
    dec_abs(0xCA74 + g_cpu.X);
    if (!g_cpu._flag_Z) { ldy(abs16(0xCA72 + g_cpu.X)); dp16_set(0x40, g_cpu.Y); return; }
    step_back();
}
/* CD:2EDE  Same, counting the passes in CA3A. */
DC_ENTRY(CD2EDE, RTS)
{
    ldx(dp16(0x43));
    dec_abs(0xCA74 + g_cpu.X);
    if (!g_cpu._flag_Z) {
        ldy(abs16(0xCA72 + g_cpu.X)); dp16_set(0x40, g_cpu.Y);
        inc_abs(0xCA3A + g_cpu.X);
        return;
    }
    abs8_set(0xCA3A + g_cpu.X, 0);
    step_back();
}
DC_ENTRY(CD2EC8, RTS)
{
    dc_CD2EA2_glue();
    abs8_set(0xCA3A + g_cpu.X, 0);
}
/* CD:1BAB  CAD6[n] -= 6F - 1D26. */
DC_ENTRY(CD1BAB, RTS)
{
    lda8(arg8()); tax();
    g_cpu._flag_C = 1; lda8(dc_sbc8(0x6F, abs8(0x1D26))); dp8_set(0x45, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0xCAD6 + g_cpu.X), dp8(0x45)));
    abs8_set(0xCAD6 + g_cpu.X, (u8)g_cpu.A);
}
/* CD:20D8  CAFC[n] += operand 2. */
DC_ENTRY(CD20D8, RTS)
{
    lda8(arg8()); tax();
    ldy(0x0001);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCAFC + g_cpu.X), bus8(lp(0x40) + 1)));
    abs8_set(0xCAFC + g_cpu.X, (u8)g_cpu.A);
    ldy(dp16(0x40)); iny(); dp16_set(0x40, g_cpu.Y);
}
/* CD:2F4E  Script call: return position saved in CA75 + $43, jump to the
 * operand word. */
DC_ENTRY(CD2F4E, RTS)
{
    ldx(dp16(0x43));
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)(dp16(0x40) + 1)); abs16_set(0xCA75 + g_cpu.X, g_cpu.A);
    lda16((u16)(bus16(lp(0x40)) - 1)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:17A9  Pokes the operand byte at the operand address (DB). */
DC_ENTRY(CD17A9, RTS)
{
    lda8(arg8());
    dc_push8((u8)g_cpu.A);
    ldy(0x0001);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(lp(0x40) + 1)); tax();
    dp16_set(0x40, (u16)(dp16(0x40) + 1));
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dc_pull8());
    abs8_set(g_cpu.X, (u8)g_cpu.A);
}
/* CD:2AAF  E850[4 * C0:FE00[CD3B++]] += 1 (16 bit). */
DC_ENTRY(CD2AAF, RTS)
{
    lda8(abs8(0xCD3B)); inc_abs(0xCD3B);
    tax();
    lda8(bus8(0xC0FE00 + g_cpu.X));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(2); tax();
    u32 a = 0xE850u + g_cpu.X;
    u16 v = (u16)(abs16(a) + 1); abs16_set(a, v); dc_set_flags_nz16(v);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD2B27, RTS)
{
    lda8(abs8(0x99D2) | 0x80); abs8_set(0x99D2, (u8)g_cpu.A);
    lda8(abs8(0x99D3)); abs8_set(0x05A5, (u8)g_cpu.A); abs8_set(0x0599, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(0x80, abs8(0x99D3)));
    abs8_set(0x05A6, (u8)g_cpu.A); abs8_set(0x059A, (u8)g_cpu.A);
}
/* CD:2DFA..2E89  Move the current thread's point (CA5E, CA60) by $45. */
static void ca_move(int add60, int add5e, int c60, int first5e)
{
    ldx(dp16(0x43));
    dc_set_mx(0, g_cpu.x_flag);
    for (int k = 0; k < 2; k++) {
        int is5e = first5e ? k == 0 : k == 1;
        u16 a = is5e ? 0xCA5E : 0xCA60;
        int add = is5e ? add5e : add60;
        if (k == 0) { if (c60 >= 0) g_cpu._flag_C = (u8)c60; }
        else g_cpu._flag_C = add ? 0 : 1;
        lda16(add ? dc_adc16(abs16(a + g_cpu.X), dp16(0x45)) : dc_sbc16(abs16(a + g_cpu.X), dp16(0x45)));
        abs16_set(a + g_cpu.X, g_cpu.A);
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD2DFA, RTS) { ca_move(1, 0, 0, 0); }
DC_ENTRY(CD2E23, RTS) { ca_move(1, 1, 0, 0); }
DC_ENTRY(CD2E89, RTS) { ca_move(0, 1, 0, 1); }
DC_ENTRY(CD2E5E, RTS) { ca_move(0, 0, 1, 0); }
/* CD:09CE  DMA channel 7: 0x200 bytes of 7E:2200 to CGRAM. */
DC_ENTRY(CD09CE, RTL)
{
    abs8_set(0x4377, 0); abs8_set(0x2121, 0);
    ldx(0x2202); stx_abs(0x4370);
    ldx(0x2200); stx_abs(0x4372);
    lda8(0x7E); abs8_set(0x4374, 0x7E);
    ldx(0x0200); stx_abs(0x4375);
    lda8(0x80); abs8_set(0x420B, 0x80);
}
/* CD:1813 / 17E5  Script jump back by the operand while CD44 is set / until
 * CE10 is set (CE10 cleared). */
static void jump_back(void)
{
    lda8(arg8()); tax(); stx_dp(0x45);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x40), dp16(0x45))); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD1813, RTS) { lda8(abs8(0xCD44)); if (!g_cpu._flag_Z) jump_back(); }
DC_ENTRY(CD17E5, RTS) { lda8(abs8(0xCE10)); if (g_cpu._flag_Z) jump_back(); abs8_set(0xCE10, 0); }
DC_ENTRY(CD2AEF, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    tdc(); tax();
    do {
        abs16_set(0xE500 + g_cpu.X, 0); abs16_set(0xE502 + g_cpu.X, 0);
        inx(); inx(); inx(); inx();
        cpx(0x06A0);
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD2B45, RTS)
{
    tdc(); tay();
    lda8(0xE0);
    do {
        abs8_set(0xBB07 + g_cpu.Y, 0xE0); abs8_set(0xBB87 + g_cpu.Y, 0xE0); abs8_set(0xBC07 + g_cpu.Y, 0xE0);
        iny(); iny(); iny(); iny();
        cpy(0x0080);
    } while (!g_cpu._flag_Z);
}
DC_ENTRY(CD2F8B, RTS)
{
    ldx((arg8() & 0x08) ? 0x00FF : 0xFF00);
    stx_abs(0xC155); stx_abs(0xC157);
    store_arg(0xC861);
}
/* CD:1ACB  Script branch on 5D80[n] & 0F: jumps to word (n & F) of the
 * table after the operand. */
/* CD:1AD4  Script jump through the word table after the operand, entry A/2. */
static void table_jump(void)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)(g_cpu.A + 1)); tay();
    lda16((u16)(bus16(lp(0x40) + g_cpu.Y) - 1)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD1ACB, RTS)
{
    lda8(arg8()); tax();
    lda8(abs8(0x5D80 + g_cpu.X) & 0x0F); asl_a(1);
    table_jump();
}
/* CD:1ABE  Same, by the high nibble. */
DC_ENTRY(CD1ABE, RTS)
{
    lda8(arg8()); tax();
    lda8(abs8(0x5D80 + g_cpu.X) & 0xF0); lsr_a(3);
    table_jump();
}
/* CD:1B22  CD08 = C:E7CF of the four bytes 5DA0..5DA3 + X (dp 23-26). */
DC_ENTRY(CD1B22, RTS)
{
    phx();
    for (int k = 0; k < 4; k++) { lda8(abs8(0x5DA0 + k + g_cpu.X)); dp8_set(0x23 + k, (u8)g_cpu.A); }
    dc_jsl(0xCEE7CF, 0xCD1B37);
    lda8(dp8(0x45)); abs8_set(0xCD08, (u8)g_cpu.A);
    plx();
}
DC_ENTRY(CD1B9F, RTS)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(0x6F, abs8(0x1D26))); tax(); stx_dp(0x45);
    dc_CD2E13_glue();
}
DC_ENTRY(CD2CDC, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    tdc(); tax();
    do {
        abs16_set(0xC15D + g_cpu.X, 0); abs16_set(0xC15F + g_cpu.X, 0);
        inx(); inx(); inx(); inx();
        cpx(0x0380);
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0xCC90, 0);
}
DC_ENTRY(CD2F6E, RTS)
{
    store_arg(0xBE8A);
    ldy(0x0001); lda8(bus8(lp(0x40) + 1)); abs8_set(0xBE91, (u8)g_cpu.A);
    iny(); lda8(bus8(lp(0x40) + 2) & abs8(0xCD46)); abs8_set(0xBE92, (u8)g_cpu.A);
    ldy(dp16(0x40)); iny(); iny(); dp16_set(0x40, g_cpu.Y);
}
/* CD:1061  Fills 0x800 bytes of D000+X and D800+X with the word 00EE. */
DC_ENTRY(CD1061, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.X, 0x0800)); dp16_set(0x53, g_cpu.A);
    lda16(0x00EE);
    do {
        abs16_set(0xD000 + g_cpu.X, 0x00EE); abs16_set(0xD800 + g_cpu.X, 0x00EE);
        inx(); inx();
        dc_cmp16(g_cpu.X, dp16(0x53));
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD18DF, RTS)
{
    static const u16 src[6] = { 0x5DA0, 0x5DA2, 0x5DA4, 0x5DA1, 0x5DA3, 0x5DA5 };
    static const u16 dst[6] = { 0xCAEA, 0xCAEE, 0xCAF2, 0xCAEC, 0xCAF0, 0xCAF4 };
    lda8(arg8()); tax();
    for (int k = 0; k < 6; k++) { lda8(abs8(src[k])); abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A); }
}
/* CD:1B5E  Script jump when 99D2 & 7F == 1, else skip the operand byte. */
DC_ENTRY(CD1B5E, RTS)
{
    lda8(abs8(0x99D2) & 0x7F);
    dc_cmp8((u8)g_cpu.A, 0x01);
    if (g_cpu._flag_Z) { dc_CD1B6D_glue(); return; }
    ldy(dp16(0x40)); iny(); dp16_set(0x40, g_cpu.Y);
}

/* ---- CD batch 4 ---- */
/* CD:20F3  Save the six 1D0C/1D23 bytes into the CAEA.. slot of the operand. */
DC_ENTRY(CD20F3, RTS)
{
    static const u16 src[6] = { 0x1D0C, 0x1D0D, 0x1D0E, 0x1D23, 0x1D24, 0x1D25 };
    static const u16 dst[6] = { 0xCAEA, 0xCAEE, 0xCAF2, 0xCAEC, 0xCAF0, 0xCAF4 };
    lda8(arg8()); tax();
    for (int k = 0; k < 6; k++) { lda8(abs8(src[k])); abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A); }
}
/* CD:2D1F  Rotate the 4-byte records BB07..BC86 down by one (first byte to the end). */
DC_ENTRY(CD2D1F, RTS)
{
    tdc(); tax();
    lda8(abs8(0xBB07 + g_cpu.X)); dc_push8((u8)g_cpu.A);
    do {
        lda8(abs8(0xBB0B + g_cpu.X)); abs8_set(0xBB07 + g_cpu.X, (u8)g_cpu.A);
        ldx((u16)(g_cpu.X + 4)); cpx(0x017C);
    } while (!g_cpu._flag_Z);
    lda8(dc_pull8()); abs8_set(0xBB07 + g_cpu.X, (u8)g_cpu.A);
}
/* CD:0239  Clear eight 0x80-byte rows from B400. */
DC_ENTRY(CD0239, RTS)
{
    static const u16 base[8] = { 0xB3FE, 0xB47E, 0xB4FE, 0xB57E, 0xB5FE, 0xB67E, 0xB6FE, 0xB77E };
    dc_set_mx(0, g_cpu.x_flag);
    tdc(); ldx(0x0080);
    do {
        for (int k = 0; k < 8; k++) abs16_set(base[k] + g_cpu.X, g_cpu.A);
        dex(); dex();
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:1B78  A101++, flip bit 3 of 993C..9946 and 9F46; script pointer back one. */
DC_ENTRY(CD1B78, RTS)
{
    inc_abs(0xA101);
    tdc(); tax();
    do {
        lda8(abs8(0x993C + g_cpu.X) ^ 0x08); abs8_set(0x993C + g_cpu.X, (u8)g_cpu.A);
        inx(); cpx(0x000B);
    } while (!g_cpu._flag_Z);
    lda8(abs8(0x9F46) ^ 0x08); abs8_set(0x9F46, (u8)g_cpu.A);
    u16 v = (u16)(dp16(0x40) - 1); dp16_set(0x40, v); dc_set_flags_nz16(v);
}
/* CD:1E07  While any of 0580/058C/0598/05A4 is set, jump back by the operand. */
DC_ENTRY(CD1E07, RTS)
{
    lda8(abs8(0x05A4) | abs8(0x0598) | abs8(0x058C) | abs8(0x0580));
    if (!g_cpu._flag_Z) jump_back();
}
/* CD:20B1  C15F,Y = C163,Y = dp49 + (A + 0x20) as a signed byte. */
DC_ENTRY(CD20B1, RTS)
{
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x20));
    bool neg = g_cpu._flag_N;
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(neg ? (u16)(g_cpu.A | 0xFF00) : g_cpu.A, dp16(0x49)));
    abs16_set(0xC15F + (u32)g_cpu.Y, g_cpu.A);
    abs16_set(0xC163 + (u32)g_cpu.Y, g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:3092  C155/C157 limits from operand bits 7 and 4-5; C864 = operand & 3F. */
DC_ENTRY(CD3092, RTS)
{
    u8 v = arg8(); lda8(v);
    if (v & 0x80) { ldx(0xFFFF); stx_abs(0xC157); ldx(0xFF00); }
    else if ((v & 0x30) == 0x30) ldx(0x00FF);
    else ldx(0xFF00);
    stx_abs(0xC155);
    lda8(arg8() & 0x3F); abs8_set(0xC864, (u8)g_cpu.A);
}
/* CD:0AEE  DMA channel 7 setup: B-bus 26 from 7E:C9A2 or 7E:C9CF (frame parity). */
DC_ENTRY(CD0AEE, RTS)
{
    lda8(0x44); abs8_set(0x4370, 0x44);
    lda8(0x26); abs8_set(0x4371, 0x26);
    lda8(0x7E); abs8_set(0x4377, 0x7E);
    abs8_set(0x4374, 0x7E);
    lda8(dp8(0x7C) & 0x01);
    ldx(g_cpu._flag_Z ? 0xC9A2 : 0xC9CF); stx_abs(0x4372);
}
/* CD:2CB3  Rotate the CAEA/CAEE/CAF2 and CAEC/CAF0/CAF4 triples by one. */
DC_ENTRY(CD2CB3, RTS)
{
    lda8(abs8(0xCAF2)); dc_push8((u8)g_cpu.A);
    lda8(abs8(0xCAF4)); dc_push8((u8)g_cpu.A);
    lda8(abs8(0xCAEE)); abs8_set(0xCAF2, (u8)g_cpu.A);
    lda8(abs8(0xCAF0)); abs8_set(0xCAF4, (u8)g_cpu.A);
    lda8(abs8(0xCAEA)); abs8_set(0xCAEE, (u8)g_cpu.A);
    lda8(abs8(0xCAEC)); abs8_set(0xCAF0, (u8)g_cpu.A);
    lda8(dc_pull8()); abs8_set(0xCAEC, (u8)g_cpu.A);
    lda8(dc_pull8()); abs8_set(0xCAEA, (u8)g_cpu.A);
}
/* CD:3BD4  dp 27..29 = dp23 * dp25..26 (8 x 16 bits, hardware multiplier). */
DC_ENTRY(CD3BD4, RTS)
{
    lda8(dp8(0x23)); bus8_set(0x004202, (u8)g_cpu.A);
    lda8(dp8(0x25)); bus8_set(0x004203, (u8)g_cpu.A);
    dp8_set(0x29, 0);
    dc_set_mx(0, g_cpu.x_flag); g_cpu._flag_C = 0;
    lda16(bus16(0x004216)); tax();
    lda16(dp16(0x26)); bus16_set(0x004203, g_cpu.A);
    stx_dp(0x27);
    lda16(dc_adc16(dp16(0x28), bus16(0x004216))); dp16_set(0x28, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:0CF6  Copy the 2C58/2C66.. color settings into BC87/BE89..; BE93 = C866 = E0, BB00 = 0F. */
DC_ENTRY(CD0CF6, RTS)
{
    static const u16 src[5] = { 0x2C66, 0x2C68, 0x2C6A, 0x2C6E, 0x2C6C };
    static const u16 dst[5] = { 0xBE89, 0xBE8B, 0xBE91, 0xBE99, 0xBE93 };
    lda8(abs8(0x2C58)); abs8_set(0xBC87, (u8)g_cpu.A);
    for (int k = 0; k < 5; k++) { ldx(abs16(src[k])); stx_abs(dst[k]); }
    lda8(0xE0); abs8_set(0xBE93, 0xE0); abs8_set(0xC866, 0xE0);
    lda8(0x0F); abs8_set(0xBB00, 0x0F);
}
/* CD:0E9D  Carry clear when the attack CCA2 is 7B-7F or 86. */
DC_ENTRY(CD0E9D, RTS)
{
    static const u8 ids[6] = { 0x86, 0x7B, 0x7C, 0x7D, 0x7E, 0x7F };
    lda8(abs8(0xCCA2));
    for (int k = 0; k < 6; k++) {
        dc_cmp8((u8)g_cpu.A, ids[k]);
        if (g_cpu._flag_Z) { tdc(); g_cpu._flag_C = 0; return; }
    }
    g_cpu._flag_C = 1;
}
/* CD:17BE  Palette color copy: entry (operand 1) to entry (operand 2), both 2000 and 2200. */
DC_ENTRY(CD17BE, RTS)
{
    lda8(arg8()); tax();
    ldy(1); lda8(bus8(lp(0x40) + g_cpu.Y));
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(1); tay();
    lda16(g_cpu.X); asl_a(1); tax();
    lda16(abs16(0x2000 + (u32)g_cpu.X)); abs16_set(0x2000 + (u32)g_cpu.Y, g_cpu.A);
    lda16(abs16(0x2200 + (u32)g_cpu.X)); abs16_set(0x2200 + (u32)g_cpu.Y, g_cpu.A);
    u16 v = (u16)(dp16(0x40) + 1); dp16_set(0x40, v); dc_set_flags_nz16(v);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:19C2  5D80[op1] += op2, or 5D80[op1 & 7F] = 0 when bit 7 is set; skips op2. */
DC_ENTRY(CD19C2, RTS)
{
    lda8(arg8());
    if (g_cpu._flag_N) {
        lda8((u8)g_cpu.A & 0x7F); tax(); abs8_set(0x5D80 + (u32)g_cpu.X, 0);
    } else {
        tax();
        ldy(1); lda8(bus8(lp(0x40) + g_cpu.Y)); dp8_set(0x45, (u8)g_cpu.A);
        g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x5D80 + (u32)g_cpu.X), dp8(0x45)));
        abs8_set(0x5D80 + (u32)g_cpu.X, (u8)g_cpu.A);
    }
    ldx(dp16(0x40)); inx(); stx_dp(0x40);
}
/* CD:1AEB  Start a slot's move: CA7C = FFFF / operand, CA7A = CA7E = FFFF, CA54 = CA60. */
DC_ENTRY(CD1AEB, RTS)
{
    lda8(0xFF); bus8_set(0x004204, 0xFF); bus8_set(0x004205, 0xFF);
    lda8(arg8()); bus8_set(0x004206, (u8)g_cpu.A);
    ldx(dp16(0x43));
    dc_set_mx(0, g_cpu.x_flag);
    abs16_set(0xCA52 + g_cpu.X, 0);
    lda16(0xFFFF); abs16_set(0xCA7A + g_cpu.X, 0xFFFF); abs16_set(0xCA7E + g_cpu.X, 0xFFFF);
    abs16_set(0xCA80 + g_cpu.X, 0);
    lda16(bus16(0x004214)); abs16_set(0xCA7C + g_cpu.X, g_cpu.A);
    lda16(abs16(0xCA60 + g_cpu.X)); abs16_set(0xCA54 + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}

/* ---- CD batch 5 ---- */
DC_ENTRY(CD0235, RTL) { dc_CD0239_glue(); }
/* CD:025E  Reset the B400 buffer state (0210 = 7E:B400), clear it, C2:0003. */
DC_ENTRY(CD025E, RTS)
{
    abs8_set(0x020C, (u8)g_cpu.A);
    abs8_set(0xCCED, 0); abs8_set(0x02A1, 0);
    ldx(0xB400); stx_abs(0x0210);
    lda8(0x7E); abs8_set(0x0212, 0x7E);
    abs8_set(0x0214, 0);
    dc_CD0239_glue();
    dc_jsl(0xC20003, 0xCD0278);
    lda8(0x40);
}
/* CD:3E8E  DMA channel 7 to VRAM: Y = VRAM address, A:X = source, dp 45 = size. */
DC_ENTRY(CD3E8E, RTS)
{
    dc_push8(g_cpu.DB); dc_push8((u8)g_cpu.A);
    lda8(0x00); dc_push8(0x00);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    lda8(dc_pull8());
    sty_abs(0x2116); stx_abs(0x4372); abs8_set(0x4374, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0x4370, 0x01);
    lda8(0x18); abs8_set(0x4371, 0x18);
    ldx(dp16(0x45)); stx_abs(0x4375);
    lda8(0x80); abs8_set(0x420B, 0x80);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}
/* Script pointer += 3 (two operand bytes after the command's). */
static void skip3(void) { ldy(dp16(0x40)); iny(); iny(); iny(); dp16_set(0x40, g_cpu.Y); }
static u8 argn(u16 n) { ldy(n); return bus8(lp(0x40) + g_cpu.Y); }
/* CD:2660  Slot n: CAF6/CAF8/CAFA = three operands, CAD8 = 0, CAFC = FF. */
DC_ENTRY(CD2660, RTS)
{
    lda8(arg8()); tax();
    lda8(argn(1)); abs8_set(0xCAF6 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(2)); abs8_set(0xCAF8 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(3)); abs8_set(0xCAFA + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0xCAD8 + g_cpu.X, 0);
    lda8(0xFF); abs8_set(0xCAFC + g_cpu.X, 0xFF);
    skip3();
}
/* CD:278E  Slot n: CAD4/CAD6 = operands, CADA/CADC/CADE = third, fixed C0/15/6A/40/40. */
DC_ENTRY(CD278E, RTS)
{
    lda8(arg8()); tax();
    lda8(argn(1)); abs8_set(0xCAD4 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(2)); abs8_set(0xCAD6 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(3));
    abs8_set(0xCADA + g_cpu.X, (u8)g_cpu.A); abs8_set(0xCADC + g_cpu.X, (u8)g_cpu.A); abs8_set(0xCADE + g_cpu.X, (u8)g_cpu.A);
    lda8(0xC0); abs8_set(0xCAE4 + g_cpu.X, 0xC0);
    lda8(0x15); abs8_set(0xCAE6 + g_cpu.X, 0x15);
    lda8(0x6A); abs8_set(0xCAE8 + g_cpu.X, 0x6A);
    lda8(0x40); abs8_set(0xCAE0 + g_cpu.X, 0x40); abs8_set(0xCAE2 + g_cpu.X, 0x40);
    skip3();
}
/* CD:2FED  CD1E36 (CAD1[op & 1] = op >> 4); CAA6/CAA8/CAA4 = the next three. */
DC_ENTRY(CD2FED, RTS)
{
    dc_CD1E36_glue();
    lda8(argn(1)); abs8_set(0xCAA6 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(2)); abs8_set(0xCAA8 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(3)); abs8_set(0xCAA4 + g_cpu.X, (u8)g_cpu.A);
    skip3();
}
/* CD:2BD2 / 2BA8  CA60[slot] +/-= signed step CE:F1D2[CA79[slot]++]. */
static void ca60_step(bool add)
{
    ldx(dp16(0x43));
    lda8(abs8(0xCA79 + g_cpu.X)); tax();
    lda8(bus8(0xCEF1D2 + (u32)g_cpu.X)); dp8_set(0x45, (u8)g_cpu.A);
    if (g_cpu._flag_N) { lda8(0xFF); dp8_set(0x46, 0xFF); } else dp8_set(0x46, 0);
    ldx(dp16(0x43));
    dc_set_mx(0, g_cpu.x_flag);
    u16 v = abs16(0xCA60 + g_cpu.X);
    if (add) { g_cpu._flag_C = 0; lda16(dc_adc16(v, dp16(0x45))); }
    else { g_cpu._flag_C = 1; lda16(dc_sbc16(v, dp16(0x45))); }
    abs16_set(0xCA60 + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    inc_abs(0xCA79 + g_cpu.X);
}
DC_ENTRY(CD2BD2, RTS) { ca60_step(true); }
DC_ENTRY(CD2BA8, RTS) { ca60_step(false); }
/* CD:2690 / 26C0  Save / restore the 2A-byte slot block CAD4.. in CB0E + CD:26B0[op]. */
static void slot_block(bool save)
{
    lda8(arg8()); asl_a(1); tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xCD26B0 + (u32)g_cpu.X)); tax();
    tdc(); tay();
    do {
        if (save) { lda16(abs16(0xCAD4 + (u32)g_cpu.Y)); abs16_set(0xCB0E + (u32)g_cpu.X, g_cpu.A); }
        else { lda16(abs16(0xCB0E + (u32)g_cpu.X)); abs16_set(0xCAD4 + (u32)g_cpu.Y, g_cpu.A); }
        inx(); inx(); iny(); iny();
        cpy(0x002A);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD2690, RTS) { slot_block(true); }
DC_ENTRY(CD26C0, RTS) { slot_block(false); }

/* ---- CD batch 6 ---- */
/* CD:3EB4  Run the VRAM DMA queued in CA26..CA2D, if any. */
DC_ENTRY(CD3EB4, RTS)
{
    lda8(abs8(0xCA26));
    if (g_cpu._flag_Z) return;
    ldx(abs16(0xCA2C)); stx_dp(0x45);
    ldx(abs16(0xCA27)); ldy(abs16(0xCA2A)); lda8(abs8(0xCA29));
    dc_CD3E8E_glue();
    abs8_set(0xCA26, 0);
}
/* CD:12B8  Copy the 16-byte tile (word [53] & 1FFF) from CF:0000 to 2D00,Y; Y += 16. */
DC_ENTRY(CD12B8, RTS)
{
    lda16(bus16(lp(0x53)) & 0x1FFF); asl_a(3); tax();
    for (int k = 0; k < 8; k += 2) {
        lda16(bus16(0xCF0000 + (u32)g_cpu.X + k)); abs16_set(0x2D00 + (u32)g_cpu.Y + k, g_cpu.A);
        lda16(bus16(0xCF0008 + (u32)g_cpu.X + k)); abs16_set(0x2D08 + (u32)g_cpu.Y + k, g_cpu.A);
    }
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.Y, 0x0010)); tay();
}
/* CD:13FE  Reverse the eight words of the tile at 2D00,Y (vertical flip); Y += 16. */
DC_ENTRY(CD13FE, RTS)
{
    for (int k = 0; k < 8; k += 2) {
        u32 a = 0x2D00 + (u32)g_cpu.Y + k, b = 0x2D0E + (u32)g_cpu.Y - k;
        lda16(abs16(a)); dp16_set(0x69, g_cpu.A);
        lda16(abs16(b)); abs16_set(a, g_cpu.A);
        lda16(dp16(0x69)); abs16_set(b, g_cpu.A);
    }
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.Y, 0x0010)); tay();
}
/* CD:38B6  WRAM port = 7E:(CD:38AA[frame parity, CC64 bit 0] + A * 4). */
DC_ENTRY(CD38B6, RTS)
{
    phx();
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(2); dp16_set(0x45, g_cpu.A);
    lda16(dp16(0x7C) & 0x0001); asl_a(1); dp16_set(0x47, g_cpu.A);
    lda16(bus16(0x7ECC64) & 0x0001);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x47)));
    asl_a(1); tax();
    lda16(bus16(0xCD38AA + (u32)g_cpu.X));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x45)));
    abs16_set(0x2181, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(0x7E); abs8_set(0x2183, 0x7E);
    plx();
}
/* CD:15D5  Start sub-script A (D1:646C table) in slot X: CA35 = its pointer + 1,
 * CA37 = bank D1, CA38 = first byte & 0F. */
DC_ENTRY(CD15D5, RTS)
{
    if (g_cpu.x_flag) dc_push8((u8)g_cpu.Y); else dc_push16(g_cpu.Y);
    phx(); phx();
    dc_set_mx(0, g_cpu.x_flag);
    asl_a(1); tax();
    lda16(bus16(0xD1646C + (u32)g_cpu.X)); dp16_set(0x53, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    tay();
    plx();
    lda8(0xD1); dp8_set(0x55, 0xD1); abs8_set(0xCA37 + g_cpu.X, 0xD1);
    lda8(bus8(lp(0x53) + g_cpu.Y) & 0x0F); abs8_set(0xCA38 + g_cpu.X, (u8)g_cpu.A);
    inc_abs(0xCA39 + g_cpu.X);
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)(dp16(0x53) + 1)); abs16_set(0xCA35 + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    inc_abs(0xCA32 + g_cpu.X);
    plx();
    ldy(g_cpu.x_flag ? dc_pull8() : dc_pull16());
}
/* CD:26E0  Slot n: CAE4/CAE6/CAE8 and CADA/CADC/CADE = six operands. */
DC_ENTRY(CD26E0, RTS)
{
    static const u16 dst[6] = { 0xCAE4, 0xCAE6, 0xCAE8, 0xCADA, 0xCADC, 0xCADE };
    lda8(arg8()); tax();
    for (int k = 0; k < 6; k++) { lda8(argn((u16)(k + 1))); abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A); }
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x40), 0x0006)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:2433 / 2436  Clamp the rectangle 5DA0..5DA3 + X (2436: X = 0) to the screen. */
static void clamp_rect(void)
{
    for (int k = 0; k < 4; k++) {
        lda8(abs8(0x5DA0 + k + (u32)g_cpu.X));
        if (k & 1) dc_CD1B50_glue(); else dc_CD1B42_glue();
        abs8_set(0x5DA0 + k + (u32)g_cpu.X, (u8)g_cpu.A);
    }
    plx();
}
DC_ENTRY(CD2433, RTS) { phx(); clamp_rect(); }
DC_ENTRY(CD2436, RTS) { phx(); ldx(0x0000); clamp_rect(); }

/* ---- CD batch 7 ---- */
DC_ENTRY(CD38B2, RTL) { dc_CD38B6_glue(); }
/* CD:1B1F  Clamp the rectangle, then CD:1B22. */
DC_ENTRY(CD1B1F, RTS) { dc_CD2436_glue(); dc_CD1B22_glue(); }
/* CD:1C34  CA5A/CA5C of the current slot = minus rectangle corner A (after the clamp). */
DC_ENTRY(CD1C34, RTS)
{
    asl_a(1); tay();
    dc_CD2436_glue();
    ldx(dp16(0x43));
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)(((abs16(0x5DA0 + (u32)g_cpu.Y) & 0x00FF) ^ 0xFFFF) + 1)); abs16_set(0xCA5A + g_cpu.X, g_cpu.A);
    lda16((u16)(((abs16(0x5DA1 + (u32)g_cpu.Y) & 0x00FF) ^ 0xFFFF) + 1)); abs16_set(0xCA5C + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD1C32, RTS) { lda8(arg8()); dc_CD1C34_glue(); }
/* CD:24F3  CAA6/CAA8 (or CAA7/CAA9 when the operand's bit 7 is set) = rectangle corner. */
DC_ENTRY(CD24F3, RTS)
{
    dc_CD2436_glue();
    lda8(arg8()); asl_a(1); tay();
    lda8(arg8());
    u16 o = g_cpu._flag_N ? 1 : 0;
    lda8(abs8(0x5DA0 + (u32)g_cpu.Y)); abs8_set(0xCAA6 + o, (u8)g_cpu.A);
    lda8(abs8(0x5DA1 + (u32)g_cpu.Y)); abs8_set(0xCAA8 + o, (u8)g_cpu.A);
}
/* CD:11B8  C9FC..CA01 = the 6-byte record (X & 7FFF) of D1:A233; CA33 = its last word. */
DC_ENTRY(CD11B8, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(g_cpu.X & 0x7FFF); asl_a(1); abs16_set(0xC9FC, g_cpu.A);
    asl_a(1); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xC9FC))); tax();
    tdc(); tay();
    do {
        lda16(bus16(0xD1A233 + (u32)g_cpu.X)); abs16_set(0xC9FC + (u32)g_cpu.Y, g_cpu.A);
        inx(); inx(); iny(); iny();
        cpy(0x0006);
    } while (!g_cpu._flag_Z);
    lda16(abs16(0xCA00)); abs16_set(0xCA33, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:1229  CD:12B8 then CD:13FE on the same tile (copy flipped). */
DC_ENTRY(CD1229, RTS)
{
    if (g_cpu.x_flag) dc_push8((u8)g_cpu.Y); else dc_push16(g_cpu.Y);
    dc_CD12B8_glue();
    ldy(g_cpu.x_flag ? dc_pull8() : dc_pull16());
    dc_CD13FE_glue();
}
/* CD:1052  Fill D000.. and D800.. with 00EE in three 2 KB blocks. */
DC_ENTRY(CD1052, RTS)
{
    ldx(0x0000); dc_CD1061_glue();
    ldx(0x1000); dc_CD1061_glue();
    ldx(0x2000); dc_CD1061_glue();
}
/* CD:3BAB  dp27 = dp23 / dp25, dp29 = remainder (16-bit shift-subtract). */
static void rol_dp16(u8 o)
{
    u16 v = dp16(o); u8 c = (u8)(v >> 15);
    v = (u16)(v << 1 | g_cpu._flag_C); g_cpu._flag_C = c;
    dp16_set(o, v); dc_set_flags_nz16(v);
}
DC_ENTRY(CD3BAB, RTS)
{
    dp16_set(0x27, 0); dp16_set(0x29, 0);
    lda16(dp16(0x23)); if (g_cpu._flag_Z) return;
    lda16(dp16(0x25)); if (g_cpu._flag_Z) return;
    ldx(0x0010);
    do {
        rol_dp16(0x23); rol_dp16(0x29);
        g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x29), dp16(0x25))); dp16_set(0x29, g_cpu.A);
        if (!g_cpu._flag_C) {
            lda16(dc_adc16(dp16(0x29), dp16(0x25))); dp16_set(0x29, g_cpu.A);
            g_cpu._flag_C = 0;
        }
        rol_dp16(0x27);
        dex();
    } while (!g_cpu._flag_Z);
}
/* CD:2B69  0A00..0AFF = 0x0C * CE:F48E[4n] >> 8, negated for the second half. */
DC_ENTRY(CD2B69, RTS)
{
    dc_set_mx(1, 1);
    ldx(0x00);
    lda8(0x0C); bus8_set(0x004202, 0x0C);
    tdc(); tay();
    do {
        lda8((u8)g_cpu.X);
        bool neg = g_cpu._flag_N;
        lda8(bus8(0xCEF48E + g_cpu.X)); bus8_set(0x004203, (u8)g_cpu.A);
        lda8(bus8(0x004217));
        if (neg) lda8((u8)((g_cpu.A ^ 0xFF) + 1));
        abs8_set(0x0A00 + g_cpu.Y, (u8)g_cpu.A);
        inx(); inx(); inx(); inx();
        iny();
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, 0);
}
/* CD:2FBC  Slot n: CAA6 += op1, CAA8 += op2 (capped at D3), CAA4 += op3. */
DC_ENTRY(CD2FBC, RTS)
{
    lda8(arg8()); tax();
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCAA6 + g_cpu.X), argn(1))); abs8_set(0xCAA6 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCAA8 + g_cpu.X), argn(2)));
    dc_cmp8((u8)g_cpu.A, 0xD3);
    if (g_cpu._flag_C) lda8(0xD3);
    abs8_set(0xCAA8 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCAA4 + g_cpu.X), argn(3))); abs8_set(0xCAA4 + g_cpu.X, (u8)g_cpu.A);
    skip3();
}
/* CD:1A51  Script jump back by the second operand. */
static void jump_back2(void)
{
    lda8(argn(1)); tax(); stx_dp(0x45);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x40), dp16(0x45))); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
static void skip1(void) { ldy(dp16(0x40)); iny(); dp16_set(0x40, g_cpu.Y); }
/* CD:1A7A / 1A66 / 2518  Loop back while CAA4[n] != 0 / CAA4[n] != 5D8E / 5D80[n] == 0. */
DC_ENTRY(CD1A7A, RTS)
{
    lda8(arg8()); tax();
    lda8(abs8(0xCAA4 + g_cpu.X));
    if (!g_cpu._flag_Z) jump_back2(); else skip1();
}
DC_ENTRY(CD1A66, RTS)
{
    lda8(arg8()); tax();
    lda8(abs8(0xCAA4 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, abs8(0x5D8E));
    if (!g_cpu._flag_Z) jump_back2(); else skip1();
}
DC_ENTRY(CD2518, RTS)
{
    lda8(arg8()); tax();
    lda8(abs8(0x5D80 + (u32)g_cpu.X));
    if (g_cpu._flag_Z) jump_back2(); else skip1();
}
/* CD:0340  Pad edge / auto-repeat: CC97 = newly pressed buttons of F8 (repeat after
 * 0F frames, then every frame, 18 when bit 7 is pressed), or'ed into EE. */
DC_ENTRY(CD0340, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0xCC99)); abs16_set(0xCC97, g_cpu.A);
    lda16(dp16(0xF8)); abs16_set(0xCC99, g_cpu.A);
    lda16(g_cpu.A | abs16(0xCC97));
    if (g_cpu._flag_Z) {
        abs16_set(0xCC97, g_cpu.A);
        lda16(0x000F); abs16_set(0xCC9B, 0x000F);
    } else {
        lda16(abs16(0xCC97) & abs16(0xCC99));
        bool repeat = false;
        if (!g_cpu._flag_Z) {
            u16 n = (u16)(abs16(0xCC9B) - 1); abs16_set(0xCC9B, n); dc_set_flags_nz16(n);
            repeat = g_cpu._flag_Z;
        }
        if (repeat) {
            lda16(0x0001); abs16_set(0xCC9B, 0x0001);
            lda16(abs16(0xCC99)); abs16_set(0xCC97, g_cpu.A);
        } else {
            lda16((abs16(0xCC97) ^ 0xFFFF) & abs16(0xCC99)); abs16_set(0xCC97, g_cpu.A);
        }
    }
    lda16(abs16(0xCC97) | dp16(0xEE)); dp16_set(0xEE, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0xCC97));
    if (g_cpu._flag_N) { lda8(0x18); abs8_set(0xCC9B, 0x18); }
}

/* ---- CD batch 8 ---- */
/* CD:1488  Copy the 16-byte tile D0:X to 2D00,Y and its mask ((lo | hi) of each
 * row pair, and dp 5F) to 2D10,Y. */
DC_ENTRY(CD1488, RTS)
{
    for (int k = 0; k < 16; k += 2) {
        u32 d = 0x2D00 + (u32)g_cpu.Y + k;
        lda16(bus16(0xD00000 + (u32)g_cpu.X + k)); abs16_set(d, g_cpu.A);
        xba();
        lda16(g_cpu.A | abs16(d));
        lda16(g_cpu.A & dp16(0x5F)); abs16_set(d + 0x10, g_cpu.A);
    }
}
/* CD:0E35  dp 53..55 = CE:EFFA[n] (n = 2C6B bit 6), X = n * 16 (+ 20 when 2A21 bit 0), Y = 0. */
DC_ENTRY(CD0E35, RTS)
{
    dc_CD0E23_glue();
    dp8_set(0x55, 0);
    lda8(abs8(0x2C6B) & 0x40);
    if (!g_cpu._flag_Z) { lda8(0x01); dp8_set(0x55, 0x01); }
    tax();
    lda8(bus8(0xCEEFFA + (u32)g_cpu.X)); dp8_set(0x53, (u8)g_cpu.A);
    lda8((u8)g_cpu.X); asl_a(4); tax();
    lda8(abs8(0x2A21) & 0x01);
    if (!g_cpu._flag_Z) { lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x20)); tax(); }
    tdc(); tay();
}
/* CD:1E45  Slot n: CAF6/CAF8/CAFA/CAD8 += four operands. */
DC_ENTRY(CD1E45, RTS)
{
    static const u16 dst[4] = { 0xCAF6, 0xCAF8, 0xCAFA, 0xCAD8 };
    lda8(arg8()); tax();
    for (int k = 0; k < 4; k++) {
        g_cpu._flag_C = 0; lda8(dc_adc8(abs8(dst[k] + g_cpu.X), argn((u16)(k + 1))));
        abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A);
    }
    ldy(dp16(0x40)); iny(); iny(); iny(); iny(); dp16_set(0x40, g_cpu.Y);
}
/* CD:1275  CD:12B8, then mirrored (CD:127A). */
static void mirror_tile(void);
DC_ENTRY(CD1275, RTS)
{
    if (g_cpu.x_flag) dc_push8((u8)g_cpu.Y); else dc_push16(g_cpu.Y);
    dc_CD12B8_glue();
    ldy(g_cpu.x_flag ? dc_pull8() : dc_pull16());
    mirror_tile();
}
/* CD:127A  Mirror every byte of the tile at 2D00,Y through C0:FD00 (horizontal flip). */
static void mirror_tile(void)
{
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(0x04); dp8_set(0x61, 0x04);
    do {
        for (int j = 0; j < 4; j++) {
            lda8(abs8(0x2D00 + (u32)g_cpu.Y + j)); tax();
            lda8(bus8(0xC0FD00 + (u32)g_cpu.X)); abs8_set(0x2D00 + (u32)g_cpu.Y + j, (u8)g_cpu.A);
        }
        iny(); iny(); iny(); iny();
        u8 n = (u8)(dp8(0x61) - 1); dp8_set(0x61, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
    dc_set_mx(0, g_cpu.x_flag);
}
/* CD:0577 / 053C  Shake step over the nine 2D-byte records at C86A: +1/-2/+1
 * (or the reverse), and C8F5/C94F += 4 (or -= 4). */
static void shake(bool fwd)
{
    tdc(); tax();
    do {
        if (fwd) inc_abs(0xC86A + g_cpu.X); else dec_abs(0xC86A + g_cpu.X);
        if (fwd) { g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0xC86D + g_cpu.X), 0x02)); }
        else { g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xC86D + g_cpu.X), 0x02)); }
        abs8_set(0xC86D + g_cpu.X, (u8)g_cpu.A);
        if (fwd) inc_abs(0xC870 + g_cpu.X); else dec_abs(0xC870 + g_cpu.X);
        dc_set_mx(0, g_cpu.x_flag);
        lda16(g_cpu.X); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x002D)); tax();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        cpx(0x0195);
    } while (!g_cpu._flag_Z);
    dc_set_mx(0, g_cpu.x_flag);
    if (fwd) {
        g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xC8F5), 0x0004)); abs16_set(0xC8F5, g_cpu.A);
        g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xC94F), 0x0004)); abs16_set(0xC94F, g_cpu.A);
    } else {
        g_cpu._flag_C = 1; lda16(dc_sbc16(abs16(0xC94F), 0x0004)); abs16_set(0xC94F, g_cpu.A);
        g_cpu._flag_C = 1; lda16(dc_sbc16(abs16(0xC8F5), 0x0004)); abs16_set(0xC8F5, g_cpu.A);
    }
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
DC_ENTRY(CD0577, RTS) { shake(true); }
DC_ENTRY(CD053C, RTS) { shake(false); }
/* CD:187E  CD37 += operand (operand 80-FE resets it first, FF adds 0); fill the
 * 0A00/0A80 tables with +CD37 / -CD37. */
DC_ENTRY(CD187E, RTS)
{
    lda8(0x03); abs8_set(0xCA92, 0x03);
    lda8(arg8()); abs8_set(0xCC95, (u8)g_cpu.A);
    if (g_cpu._flag_N) {
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) abs8_set(0xCD37, 0);
        tdc();
    }
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0xCD37))); abs8_set(0xCD37, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    dp16_set(0x45, g_cpu.A);
    lda16((u16)((g_cpu.A ^ 0xFFFF) + 1)); dp16_set(0x47, g_cpu.A);
    tdc(); tax();
    do {
        abs16_set(0x0A00 + g_cpu.X, dp16(0x45)); abs16_set(0x0A02 + g_cpu.X, 0);
        abs16_set(0x0A80 + g_cpu.X, dp16(0x47)); abs16_set(0x0A82 + g_cpu.X, 0);
        inx(); inx(); inx(); inx();
        cpx(0x0080);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}

/* ---- CD batch 9 ---- */
/* CD:1221  CD:1229 (copy flipped vertically), then mirrored: rotated 180 degrees. */
DC_ENTRY(CD1221, RTS)
{
    if (g_cpu.x_flag) dc_push8((u8)g_cpu.Y); else dc_push16(g_cpu.Y);
    dc_CD1229_glue();
    ldy(g_cpu.x_flag ? dc_pull8() : dc_pull16());
    mirror_tile();
}
/* CD:0B1E  When 99D2 is 01-7F, cap the four 0580 records: 0582 = 99D3 - 0581
 * where 0581 + 0582 would pass 99D3. */
DC_ENTRY(CD0B1E, RTS)
{
    lda8(abs8(0x99D2));
    if (g_cpu._flag_N || g_cpu._flag_Z) return;
    tdc(); tax();
    do {
        lda8(abs8(0x0580 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0x0581 + g_cpu.X));
            if (!g_cpu._flag_N) {
                g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0x0582 + g_cpu.X)));
                dc_cmp8((u8)g_cpu.A, abs8(0x99D3));
                if (!g_cpu._flag_Z && g_cpu._flag_C) {
                    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0x99D3), abs8(0x0581 + g_cpu.X)));
                    abs8_set(0x0582 + g_cpu.X, (u8)g_cpu.A);
                }
            }
        }
        lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x0C)); tax();
        cpx(0x0030);
    } while (!g_cpu._flag_Z);
}
/* CD:0D33  Clear the slot tables CA32/CA52/CA72, then start the two sub-scripts in CA93/CA94. */
DC_ENTRY(CD0D33, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    tdc(); tax();
    do {
        abs16_set(0xCA32 + g_cpu.X, 0); abs16_set(0xCA52 + g_cpu.X, 0); abs16_set(0xCA72 + g_cpu.X, 0);
        inx(); inx(); cpx(0x0020);
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, g_cpu.x_flag);
    tdc(); tax(); tay();
    do {
        lda8(abs8(0xCA93 + g_cpu.Y)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) dc_CD15D5_glue();
        iny();
        lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x10)); tax();
        cpx(0x0020);
    } while (!g_cpu._flag_Z);
}
/* CD:38E5  Y = (dp0B - dp0C) * 256 / dp0D, signed (A = 1 when negative). */
DC_ENTRY(CD38E5, RTS)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0x0B), dp8(0x0C)));
    if (g_cpu._flag_C) {
        xba(); tay(); sty_abs(0x4204);
        lda8(dp8(0x0D)); abs8_set(0x4206, (u8)g_cpu.A);
        tdc();
        ldy(abs16(0x4214));
        return;
    }
    lda8((u8)((g_cpu.A ^ 0xFF) + 1));
    xba(); tay(); sty_abs(0x4204);
    lda8(dp8(0x0D)); abs8_set(0x4206, (u8)g_cpu.A);
    tdc();
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)((abs16(0x4214) ^ 0xFFFF) + 1)); tay();
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(0x01);
}
/* CD:2AC6  Three CD:2AAF steps, then every active E850 entry moves 16 up (E500 -= 10). */
DC_ENTRY(CD2AC6, RTS)
{
    dc_CD2AAF_glue(); dc_CD2AAF_glue(); dc_CD2AAF_glue();
    dc_set_mx(0, g_cpu.x_flag);
    tdc(); tax();
    do {
        lda16(abs16(0xE850 + g_cpu.X));
        if (!g_cpu._flag_Z) { g_cpu._flag_C = 1; lda16(dc_sbc16(abs16(0xE500 + g_cpu.X), 0x0010)); abs16_set(0xE500 + g_cpu.X, g_cpu.A); }
        inx(); inx(); inx(); inx();
        cpx(0x0350);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}

/* ---- CD batch 10 ---- */
/* CD:0E79 / 0E61  After CD:0E35, when dp 55 is clear: CCDA..CCE1 = 8 bytes of
 * CE:EFFE / CE:F006 + X, CCD9 = dp 53, CCE5 bit 7 set. */
static void load_ccda(u32 table)
{
    dc_CD0E35_glue();
    lda8(dp8(0x55));
    if (!g_cpu._flag_Z) return;
    do {
        lda8(bus8(table + (u32)g_cpu.X)); abs8_set(0xCCDA + g_cpu.Y, (u8)g_cpu.A);
        inx(); iny(); cpy(0x0008);
    } while (!g_cpu._flag_Z);
    lda8(dp8(0x53)); abs8_set(0xCCD9, (u8)g_cpu.A);
    lda8(abs8(0xCCE5) | 0x80); abs8_set(0xCCE5, (u8)g_cpu.A);
}
DC_ENTRY(CD0E79, RTS) { load_ccda(0xCEEFFE); }
DC_ENTRY(CD0E61, RTS) { load_ccda(0xCEF006); }
/* CD:3B10  Point the WRAM port like CD:38B6 (no offset), then write the word pair
 * (X, Y high bytes) A times, skipping two bytes after each pair. */
DC_ENTRY(CD3B10, RTS)
{
    phx();
    dp8_set(0x45, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(dp16(0x7C) & 0x0001); asl_a(1); dp16_set(0x47, g_cpu.A);
    lda16(bus16(0x7ECC64) & 0x0001);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x47)));
    asl_a(1); tax();
    lda16(bus16(0xCD38AA + (u32)g_cpu.X)); abs16_set(0x2181, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(0x7E); abs8_set(0x2183, 0x7E);
    plx();
    lda8(dp8(0x45));
    if (g_cpu._flag_Z) return;
    dp16_set(0x47, g_cpu.X); dp16_set(0x49, g_cpu.Y);
    dc_push16(g_cpu.X); dc_push16(g_cpu.Y);
    dc_set_mx(1, 1);
    ldy(dp8(0x4A)); ldx(dp8(0x48));
    do {
        abs8_set(0x2180, (u8)g_cpu.X); abs8_set(0x2180, (u8)g_cpu.Y);
        lda8(abs8(0x2180)); lda8(abs8(0x2180));
        u8 n = (u8)(dp8(0x45) - 1); dp8_set(0x45, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, 0);
    ldy(dc_pull16()); ldx(dc_pull16());
    tdc();
}
/* CD:1D98  Load (or with bit 7, clear) the two CCB0/CCB8 wave records; skips 7 bytes. */
DC_ENTRY(CD1D98, RTS)
{
    static const u16 dst[7] = { 0xCCB6, 0xCCB7, 0xCCB5, 0xCCBE, 0xCCBF, 0xCCBD, 0xCCC0 };
    lda8(arg8());
    if (!g_cpu._flag_N) {
        abs8_set(0xCCB2, 0); abs8_set(0xCCB4, 0); abs8_set(0xCCBA, 0); abs8_set(0xCCBC, 0);
        lda8(0x01); abs8_set(0xCCB3, 0x01); abs8_set(0xCCBB, 0x01);
        for (int k = 0; k < 7; k++) { lda8(argn((u16)(k + 1))); abs8_set(dst[k], (u8)g_cpu.A); }
        lda8(abs8(0xCCB6) | abs8(0xCCB7)); abs8_set(0xCCB0, (u8)g_cpu.A);
        lda8(abs8(0xCCBE) | abs8(0xCCBF)); abs8_set(0xCCB8, (u8)g_cpu.A);
    } else {
        abs8_set(0xCCB0, 0); abs8_set(0xCCB8, 0); abs8_set(0xCCC0, 0);
    }
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x40), 0x0007)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:0CA1  Copy the 2C56.. color/window settings into BC85.. (BC86 bit 0 cleared
 * when 2A21 bit 0 is set) and the 2C70.. words into 32 rows of BC89/BD09/BD89, then CD:0CF6. */
DC_ENTRY(CD0CA1, RTS)
{
    static const u16 src[6] = { 0x2C70, 0x2C72, 0x2C74, 0x2C76, 0x2C78, 0x2C7A };
    static const u16 dst[6] = { 0xBC89, 0xBC8B, 0xBD09, 0xBD0B, 0xBD89, 0xBD8B };
    lda8(abs8(0x2C56)); abs8_set(0xBC85, (u8)g_cpu.A);
    lda8(abs8(0x2C57)); abs8_set(0xBC86, (u8)g_cpu.A);
    lda8(abs8(0x2C59)); abs8_set(0xBC88, (u8)g_cpu.A);
    lda8(abs8(0x2A21) & 0x01);
    if (!g_cpu._flag_Z) { lda8(abs8(0xBC86) & 0xFE); abs8_set(0xBC86, (u8)g_cpu.A); }
    tdc(); tax();
    dc_set_mx(0, g_cpu.x_flag);
    do {
        for (int k = 0; k < 6; k++) { lda16(abs16(src[k])); abs16_set(dst[k] + g_cpu.X, g_cpu.A); }
        inx(); inx(); inx(); inx();
        cpx(0x0080);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    dc_CD0CF6_glue();
}

/* ---- CD batch 11 ---- */
DC_ENTRY(CD0C9D, RTL) { dc_CD0CA1_glue(); }
/* CD:303B  C866 low 5 bits += signed op1 (clamped 0..1F); op1 = 0 sets C866 = op0. */
DC_ENTRY(CD303B, RTS)
{
    lda8(argn(1));
    if (g_cpu._flag_N || !g_cpu._flag_Z) {
        bool down = g_cpu._flag_N;
        g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xC866) & 0x1F, argn(1))); dp8_set(0x45, (u8)g_cpu.A);
        if (g_cpu.A & 0xE0) dp8_set(0x45, down ? 0x00 : 0x1F);
        lda8((abs8(0xC866) & 0xE0) | dp8(0x45));
    } else {
        lda8(arg8());
    }
    abs8_set(0xC866, (u8)g_cpu.A);
    skip1();
}
/* CD:29E3  Fill the 0550 record (CD:2A42[op5 & 7]) from the six packed operands. */
DC_ENTRY(CD29E3, RTS)
{
    lda8(argn(5)); dp8_set(0x47, (u8)g_cpu.A);
    lda8((u8)g_cpu.A & 0x07); tax();
    lda8(bus8(0xCD2A42 + (u32)g_cpu.X)); tax();
    lda8(arg8()); dp8_set(0x45, (u8)g_cpu.A);
    lda8((u8)g_cpu.A & 0xF0); abs8_set(0x0550 + g_cpu.X, (u8)g_cpu.A);
    lda8(dp8(0x45) & 0x0F); abs8_set(0x0556 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(1)); abs8_set(0x0551 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(2)); abs8_set(0x0552 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(3)); abs8_set(0x0557 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(4)); abs8_set(0x0558 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(5) & 0xF8); abs8_set(0x0553 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(6) & 0x0F); abs8_set(0x0555 + g_cpu.X, (u8)g_cpu.A);
    lda8(argn(6)); lsr_a(4); abs8_set(0x0554 + g_cpu.X, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x40), 0x0006)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:3134  Direction (+1/-1 in CA9F/CAA0) and distance (CAA1/CAA2) from CA9B/CA9C to CA9D/CA9E. */
static void axis(u16 from, u16 to, u16 dir, u16 dist)
{
    lda8(abs8(from)); dc_cmp8((u8)g_cpu.A, abs8(to));
    if (g_cpu._flag_Z) return;
    if (g_cpu._flag_C) { dec_abs(dir); g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(from), abs8(to))); }
    else { inc_abs(dir); g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(to), abs8(from))); }
    abs8_set(dist, (u8)g_cpu.A);
}
DC_ENTRY(CD3134, RTS)
{
    tdc(); tax(); stx_abs(0xCA9F); stx_abs(0xCAA1);
    axis(0xCA9B, 0xCA9D, 0xCA9F, 0xCAA1);
    axis(0xCA9C, 0xCA9E, 0xCAA0, 0xCAA2);
}
/* CD:1D56  Slot n: CADA/CADC/CADE += op2..op4 unless the sum reaches op1. */
DC_ENTRY(CD1D56, RTS)
{
    static const u16 dst[3] = { 0xCADA, 0xCADC, 0xCADE };
    lda8(arg8()); tax();
    lda8(argn(1)); dp8_set(0x45, (u8)g_cpu.A);
    for (int k = 0; k < 3; k++) {
        g_cpu._flag_C = 0; lda8(dc_adc8(abs8(dst[k] + g_cpu.X), argn((u16)(k + 2))));
        dc_cmp8((u8)g_cpu.A, dp8(0x45));
        if (!g_cpu._flag_C) abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A);
    }
    ldy(dp16(0x40)); iny(); iny(); iny(); iny(); dp16_set(0x40, g_cpu.Y);
}
/* CD:0154  AND dp 73 into a column of 2-byte rows (stride 20, plus the +10 plane):
 * shape CE:F192[dp53 * 8 + X], start CE:F166[dp59] + dp75. */
DC_ENTRY(CD0154, RTS)
{
    stx_dp(0x55);
    lda8(dp8(0x53)); asl_a(3);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(g_cpu.A & 0x00FF);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x55))); tax();
    lda16(bus16(0xCEF192 + (u32)g_cpu.X)); dp16_set(0x55, g_cpu.A);
    lda16(bus16(0xCEF194 + (u32)g_cpu.X)); dp16_set(0x57, g_cpu.A);
    lda16(dp16(0x59)); asl_a(1); tax();
    lda16(bus16(0xCEF166 + (u32)g_cpu.X));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x55)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x75))); tax();
    do {
        lda16(abs16((u32)g_cpu.X) & dp16(0x73)); abs16_set((u32)g_cpu.X, g_cpu.A);
        lda16(abs16(0x0010 + (u32)g_cpu.X) & dp16(0x73)); abs16_set(0x0010 + (u32)g_cpu.X, g_cpu.A);
        lda16(g_cpu.X); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0020)); tax();
        u16 n = (u16)(dp16(0x57) - 1); dp16_set(0x57, n); dc_set_flags_nz16(n);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}

/* ---- CD batch 12 ---- */
/* CD:05B2  Screen shake driver: CA1E > 0 shakes back (CD:053C) CA1F times, < 0
 * shakes forward (CD:0577); FF starts it (CE:EE6E with CA22). */
DC_ENTRY(CD05B2, RTS)
{
    lda8(abs8(0xCA1E));
    if (g_cpu._flag_Z) return;
    if (!g_cpu._flag_N) {
        lda8(abs8(0xCA1F));
        if (!g_cpu._flag_Z) {
            dc_CD053C_glue();
            dec_abs(0xCA1F);
            if (g_cpu._flag_Z) { lda8(0x02); abs8_set(0xCA25, 0x02); inc_abs(0xCA24); }
            return;
        }
    } else {
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) {
            abs8_set(0xCA25, 0);
            lda8(abs8(0xCA22));
            dc_jsl(0xCEEE6E, 0xCD05DA);
            lda8(0x01); abs8_set(0xCA1E, 0x01);
            return;
        }
        dc_CD0577_glue();
        dec_abs(0xCA1F);
        if (!g_cpu._flag_Z) return;
        abs8_set(0xCA25, 0); inc_abs(0xCA24);
    }
    lda8(abs8(0xCA1E));
    if (!g_cpu._flag_N) abs8_set(0xCE16, 0);
    abs8_set(0xCA1E, 0);
}
/* CD:211B  dp4B = dp47 + (A - dp47) * dp21 / 256 (interpolate; A = result).
 * The positive path reads $4217 as a word, so the joypad byte $4218 lands in the high byte. */
DC_ENTRY(CD211B, RTS)
{
    tay(); dp16_set(0x45, g_cpu.Y);
    dp8_set(0x48, 0);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x45), dp16(0x47))); dp16_set(0x49, g_cpu.A);
    lda16(dp16(0x49));
    if (g_cpu._flag_N) {
        lda16((u16)((g_cpu.A ^ 0xFFFF) + 1));
        dc_set_mx(1, g_cpu.x_flag);
        bus8_set(0x004202, (u8)g_cpu.A);
        lda8(dp8(0x21)); bus8_set(0x004203, (u8)g_cpu.A);
        lda8(bus8(0x004217));
        dc_set_mx(0, g_cpu.x_flag);
        lda16((u16)((g_cpu.A ^ 0xFFFF) + 1));
    } else {
        bus16_set(0x004202, g_cpu.A);
        lda16(dp16(0x21)); bus16_set(0x004203, g_cpu.A);
        lda16(bus16(0x004217));
        dc_set_mx(0, g_cpu.x_flag);
    }
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x47))); dp16_set(0x4B, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x4B));
}
/* CD:11E4  Build the 64 tiles of graphics set X (CF:8000 + X * 40): each map word's
 * bits 14-13 pick plain / mirrored / flipped / rotated (CD:1219 table). */
DC_ENTRY(CD11E4, RTS)
{
    static void (*const op[4])(void) = { dc_CD12B8_glue, dc_CD1275_glue, dc_CD1229_glue, dc_CD1221_glue };
    lda8(0xCF); dp8_set(0x55, 0xCF);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(g_cpu.X); asl_a(6);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x8000)); dp16_set(0x53, g_cpu.A);
    tdc(); tay();
    lda16(0x0040); dp16_set(0x67, 0x0040);
    do {
        lda16(bus16(lp(0x53))); xba(); lsr_a(5);
        lda16(g_cpu.A & 0x0006); tax();
        op[g_cpu.X >> 1]();
        for (int k = 0; k < 2; k++) { u16 v = (u16)(dp16(0x53) + 1); dp16_set(0x53, v); dc_set_flags_nz16(v); }
        u16 n = (u16)(dp16(0x67) - 1); dp16_set(0x67, n); dc_set_flags_nz16(n);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:1004  Palette rows 2032/2232 and 203A/223A from D1:6002 entries CA95 / CA96 (FF = none). */
static void pal_from_ca95(void)
{
    tdc(); tay();
    lda8(abs8(0xCA95)); dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) {
        dc_set_mx(0, g_cpu.x_flag);
        asl_a(3); tax();
        do {
            lda16(bus16(0xD16002 + (u32)g_cpu.X)); abs16_set(0x2032 + g_cpu.Y, g_cpu.A); abs16_set(0x2232 + g_cpu.Y, g_cpu.A);
            inx(); inx(); iny(); iny(); cpy(0x0006);
        } while (!g_cpu._flag_Z);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
    tdc(); tay();
    lda8(abs8(0xCA96)); dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_Z) {
        dc_set_mx(0, g_cpu.x_flag);
        lda16(g_cpu.A & 0x00FF); asl_a(3); tax();
        do {
            lda16(bus16(0xD16002 + (u32)g_cpu.X)); abs16_set(0x203A + g_cpu.Y, g_cpu.A); abs16_set(0x223A + g_cpu.Y, g_cpu.A);
            inx(); inx(); iny(); iny(); cpy(0x0006);
        } while (!g_cpu._flag_Z);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
    }
}
DC_ENTRY(CD1AB2, RTS) { lda8(abs8(0x5D8A)); abs8_set(0xCA95, (u8)g_cpu.A); abs8_set(0xCA96, (u8)g_cpu.A); pal_from_ca95(); }
DC_ENTRY(CD1F3C, RTS) { lda8(arg8()); abs8_set(0xCA95, (u8)g_cpu.A); abs8_set(0xCA96, (u8)g_cpu.A); pal_from_ca95(); }
/* CD:31AA  Circle table: B800.. = half-widths of the circle with radius CAA4[CAAA] & 7F
 * (midpoint algorithm); dp47 = rows; WRAM port = 7E:BA00. Carry clear when the radius is 0. */
DC_ENTRY(CD31AA, RTS)
{
    lda8(abs8(0xCAAA)); tax();
    lda8(abs8(0xCAA4 + g_cpu.X) & 0x7F);
    if (g_cpu._flag_Z) { g_cpu._flag_C = 0; return; }
    dc_set_mx(1, 1);
    dp8_set(0x49, (u8)g_cpu.A); tax();
    dp8_set(0x51, 0);
    tdc(); tay();
    lda8((u8)(g_cpu.A - 1));
    for (;;) {
        dp8_set(0x47, (u8)g_cpu.A);
        lda8((u8)g_cpu.X); dc_cmp8((u8)g_cpu.A, dp8(0x51));
        if (!g_cpu._flag_C) break;
        abs8_set(0xB800 + g_cpu.Y, (u8)g_cpu.A); iny();
        lda8(dp8(0x51)); asl_a(1); lda8((u8)g_cpu.A ^ 0xFF);
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x49))); dp8_set(0x49, (u8)g_cpu.A);
        { u8 n = (u8)(dp8(0x51) + 1); dp8_set(0x51, n); dc_set_flags_nz8(n); }
        dc_cmp8((u8)g_cpu.A, dp8(0x47));
        if (!g_cpu._flag_C) continue;
        dex();
        lda8((u8)g_cpu.X); asl_a(1);
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x49))); dp8_set(0x49, (u8)g_cpu.A);
    }
    dp8_set(0x47, (u8)g_cpu.Y);
    lda8(0x00); bus8_set(0x002181, 0x00);
    lda8(0xBA); bus8_set(0x002182, 0xBA);
    lda8(0x7E); bus8_set(0x002183, 0x7E);
    dc_set_mx(1, 0);
    g_cpu._flag_C = 1;
}

/* ---- CD batch 13: sine products (CE:F48E holds |sin| for angles 00-FF, the
 * sign comes from the angle's bit 7; 8-bit indexes) ---- */
static u8 sin_mag(void) { return bus8(0xCEF48E + (u32)g_cpu.X); }
static void neg_a(void) { lda8((u8)((g_cpu.A ^ 0xFF) + 1)); }
/* Start |sin(A)| * |Y| on the multiplier; returns whether the product is negative. */
static bool sin_mul_start(void)
{
    tax(); bool xneg = g_cpu._flag_N;
    lda8(sin_mag()); bus8_set(0x004202, (u8)g_cpu.A);
    lda8((u8)g_cpu.Y); bool yneg = g_cpu._flag_N;
    if (yneg) neg_a();
    bus8_set(0x004203, (u8)g_cpu.A);
    return xneg != yneg;
}
/* CD:3C00 / 3BFD  A = sin(A) * Y / 256 (3BFD: cos). */
DC_ENTRY(CD3C00, RTS) { bool neg = sin_mul_start(); lda8(bus8(0x004217)); if (neg) neg_a(); }
DC_ENTRY(CD3BFD, RTS) { g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x40)); dc_CD3C00_glue(); }
/* CD:3C58 / 3C55  Start the same product (3C55: cos); carry = its sign. */
DC_ENTRY(CD3C58, RTS) { g_cpu._flag_C = sin_mul_start(); }
DC_ENTRY(CD3C55, RTS) { g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x40)); dc_CD3C58_glue(); }
/* CD:3D90  A = sin(A) / 2 * dp47 / 256. */
DC_ENTRY(CD3D90, RTS)
{
    phx();
    tax(); bool neg = g_cpu._flag_N;
    lda8(sin_mag()); lsr_a(1); bus8_set(0x004202, (u8)g_cpu.A);
    lda8(dp8(0x47)); bus8_set(0x004203, (u8)g_cpu.A);
    plx();
    lda8(bus8(0x004217));
    if (neg) neg_a();
}
/* CD:3DC3  A = (sin(A) / 2 * dp47 / 256) * |sin(dp45)| / 256 (dp49 = |sin(dp45)|, Y = A). */
DC_ENTRY(CD3DC3, RTS)
{
    phx();
    tay();
    lda8(dp8(0x45)); tax();
    lda8(sin_mag()); dp8_set(0x49, (u8)g_cpu.A);
    ldx(g_cpu.Y); bool neg = g_cpu._flag_N;
    lda8(sin_mag()); lsr_a(1); bus8_set(0x004202, (u8)g_cpu.A);
    lda8(dp8(0x47)); bus8_set(0x004203, (u8)g_cpu.A);
    plx();
    lda8(bus8(0x004217)); bus8_set(0x004202, (u8)g_cpu.A);
    lda8(dp8(0x49)); bus8_set(0x004203, (u8)g_cpu.A);
    lda8(bus8(0x004217));
    if (neg) neg_a();
}

/* ---- CD batch 14: jump tables (JMP (abs,X); the targets return to our caller) ---- */
/* CD:13CA  A = dp5D bits picked by dp5B & 6 (CD:13E2/13E0/13DE/13DC). */
DC_ENTRY(CD13CA, RTS)
{
    static const glue_fn t[4] = { dc_CD13E2_glue, dc_CD13E0_glue, dc_CD13DE_glue, dc_CD13DC_glue };
    lda8(dp8(0x5B) & 0x06); tax();
    lda8(dp8(0x5D));
    jump_table(t, 4, 0x13D4, 0xCD13D1);
}
/* CD:2958 / 2999  Copy slot Y's CA77 block to / from one of four places (operand). */
DC_ENTRY(CD2958, RTS)
{
    static const glue_fn t[4] = { dc_CD2969_glue, dc_CD2975_glue, dc_CD2981_glue, dc_CD298D_glue };
    ldy(dp16(0x43));
    lda8(arg8()); asl_a(1); tax();
    jump_table(t, 4, 0x2961, 0xCD295E);
}
DC_ENTRY(CD2999, RTS)
{
    static const glue_fn t[4] = { dc_CD29AA_glue, dc_CD29B6_glue, dc_CD29C2_glue, dc_CD29CE_glue };
    ldy(dp16(0x43));
    lda8(arg8()); asl_a(1); tax();
    jump_table(t, 4, 0x29A2, 0xCD299F);
}
/* CD:2DD6  CA move by kind (operand bits 7-5) for (bits 4-0) + 1 frames (dp 45). */
DC_ENTRY(CD2DD6, RTS)
{
    static const glue_fn t[8] = { dc_CD2E89_glue, dc_CD2E78_glue, dc_CD2E5E_glue, dc_CD2E4D_glue,
                                  dc_CD2E3C_glue, dc_CD2E23_glue, dc_CD2E13_glue, dc_CD2DFA_glue };
    lda8((u8)((arg8() & 0x1F) + 1)); tax(); stx_dp(0x45);
    lda8(arg8() & 0xE0); lsr_a(4); tax();
    jump_table(t, 8, 0x2DEA, 0xCD2DE7);
}

/* ---- CD batch 15: sound/sequence loaders (long entries; written for 16-bit
 * indexes: with x = 1 their LDX #imm decodes as garbage) ---- */
/* Request A (unless it is the one playing, CD06, or FF): source pointer
 * bank:ptr in 020D-020F, then CD:025E; CCEC = 10, CA24++, CD05 = 1. */
static void seq_request(u16 ptr, u8 bank, bool from53)
{
    dc_cmp8((u8)g_cpu.A, abs8(0xCD06));
    if (g_cpu._flag_Z) return;
    abs8_set(0xCD06, (u8)g_cpu.A);
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    ldx(ptr); stx_abs(0x020D);
    if (from53) lda8(dp8(0x53));
    dc_push8((u8)g_cpu.A);
    lda8(bank); abs8_set(0x020F, bank);
    lda8(dc_pull8());
    dc_CD025E_glue();
    lda8(0x10); abs8_set(0xCCEC, 0x10);
    inc_abs(0xCA24);
    lda8(0x01); abs8_set(0xCD05, 0x01);
}
DC_ENTRY(CD0027, RTL)
{
    DC_ASSUME_X(CD0027, 0);
    ldx(abs16(0xCD1D));
    if (!g_cpu._flag_Z) return;
    seq_request(0x3A09, 0xCC, false);
}
/* CD:002D / 0030  Same with the CE:EF11 table, dp53 = 0B / 0C. */
static void seq_request53(u8 v)
{
    ldx(abs16(0xCD1D));
    if (!g_cpu._flag_Z) return;
    dc_push8((u8)g_cpu.A);
    lda8(v); dp8_set(0x53, v);
    lda8(dc_pull8());
    seq_request(0xEF11, 0xCE, true);
}
DC_ENTRY(CD002D, RTL) { DC_ASSUME_X(CD002D, 0); seq_request53(0x0B); }
DC_ENTRY(CD0030, RTL) { DC_ASSUME_X(CD0030, 0); seq_request53(0x0C); }

/* ---- CD batch 16: orbit motion ---- */
/* CD:1BC0  A = sin(A) * dp45 / 256, registers kept. */
DC_ENTRY(CD1BC0, RTS)
{
    phx();
    if (g_cpu.x_flag) dc_push8((u8)g_cpu.Y); else dc_push16(g_cpu.Y);
    dc_set_mx(g_cpu.m_flag, 1);
    ldy(dp8(0x45));
    dc_CD3C00_glue();
    dc_set_mx(g_cpu.m_flag, 0);
    ldy(dc_pull16()); ldx(dc_pull16());
}
/* One axis: A (8-bit) + dp(c) + dp(t), negated as a word into dst + slot. */
static void orbit_store(u8 c, u8 t, u16 dst)
{
    ldx(dp8(0x43));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(c)));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(t)));
    dc_set_mx(0, g_cpu.x_flag);
    lda16((u16)((g_cpu.A ^ 0xFFFF) + 1)); abs16_set(dst + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:1CD5  Advance the slot's three orbit angles (CA7F += 2, CA80 += 5, CA81 += 6)
 * and set its offset CA5A/CA5C from the radii in dp 49-4C and the centre dp 4D/4E. */
DC_ENTRY(CD1CD5, RTS)
{
    static const struct { u16 a; u8 k; } step[3] = { { 0xCA81, 6 }, { 0xCA80, 5 }, { 0xCA7F, 2 } };
    ldx(dp16(0x43));
    for (int i = 0; i < 3; i++) {
        g_cpu._flag_C = 0; lda8(dc_adc8(abs8(step[i].a + g_cpu.X), step[i].k)); abs8_set(step[i].a + g_cpu.X, (u8)g_cpu.A);
    }
    tdc(); tay();
    dc_set_mx(1, 1);
    ldx(dp8(0x43)); ldy(dp8(0x49)); lda8(abs8(0xCA7F + g_cpu.X)); dc_CD3BFD_glue(); dp8_set(0x45, (u8)g_cpu.A);
    ldx(dp8(0x43)); ldy(dp8(0x4A)); lda8(abs8(0xCA7F + g_cpu.X)); dc_CD3C00_glue(); dp8_set(0x47, (u8)g_cpu.A);
    ldx(dp8(0x43)); ldy(dp8(0x4B)); lda8(abs8(0xCA81 + g_cpu.X)); dc_CD3BFD_glue();
    orbit_store(0x4D, 0x45, 0xCA5A);
    ldx(dp8(0x43)); ldy(dp8(0x4C)); lda8(abs8(0xCA80 + g_cpu.X)); dc_CD3C00_glue();
    orbit_store(0x4E, 0x47, 0xCA5C);
    dc_set_mx(1, 0);
}
/* CD:1CBF  Radii from CD:1C5B[operand], then CD:1CD5. */
DC_ENTRY(CD1CBF, RTS)
{
    lda8(arg8()); asl_a(2); tax();
    dc_set_mx(0, g_cpu.x_flag);
    lda16(bus16(0xCD1C5B + (u32)g_cpu.X)); dp16_set(0x49, g_cpu.A);
    lda16(bus16(0xCD1C5D + (u32)g_cpu.X)); dp16_set(0x4B, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    dc_CD1CD5_glue();
}
/* CD:1C63  Centre = the current offset (CD:1C34 corner 0, negated), then CD:1CBF. */
DC_ENTRY(CD1C63, RTS)
{
    tdc(); dc_CD1C34_glue();
    ldx(dp16(0x43));
    lda8(abs8(0xCA5A + g_cpu.X)); neg_a(); dp8_set(0x4D, (u8)g_cpu.A);
    lda8(abs8(0xCA5C + g_cpu.X)); neg_a(); dp8_set(0x4E, (u8)g_cpu.A);
    dc_CD1CBF_glue();
}

/* ---- CD batch 17 ---- */
/* CD:2EF7  BE9A: low nibble = operand bits 7-4, high nibble += / -= (bit 7) the
 * operand's low nibble, clamped to 0..F. */
DC_ENTRY(CD2EF7, RTS)
{
    lda8(arg8()); lsr_a(4); lda8((u8)g_cpu.A & 0x0F); dp8_set(0x45, (u8)g_cpu.A);
    lda8(arg8());
    bool down = g_cpu._flag_N;
    lda8((u8)g_cpu.A & 0x0F); dp8_set(0x47, (u8)g_cpu.A);
    lda8(abs8(0xBE9A)); lsr_a(4); lda8((u8)g_cpu.A & 0x0F);
    if (down) { g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x47))); }
    else { g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x47))); }
    dp8_set(0x49, (u8)g_cpu.A);
    lda8((u8)g_cpu.A & 0xF0);
    if (g_cpu._flag_Z) lda8(dp8(0x49));
    else if (down) tdc();
    else lda8(0x0F);
    asl_a(4); lda8((u8)g_cpu.A | dp8(0x45));
    abs8_set(0xBE9A, (u8)g_cpu.A);
}
/* CD:204B  C15D,Y = C161,Y = A as a signed byte (by the caller's N flag), then
 * start |sin(dp45)| * 0F on the multiplier; carry = sign of dp45. */
DC_ENTRY(CD204B, RTS)
{
    bool neg = g_cpu._flag_N;
    dc_set_mx(0, g_cpu.x_flag);
    if (neg) lda16(g_cpu.A | 0xFF00);
    abs16_set(0xC15D + (u32)g_cpu.Y, g_cpu.A); abs16_set(0xC161 + (u32)g_cpu.Y, g_cpu.A);
    if (neg) tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x45));
    bool sneg = g_cpu._flag_N;
    tax();
    lda8(sin_mag()); bus8_set(0x004202, (u8)g_cpu.A);
    lda8(0x0F); bus8_set(0x004203, 0x0F);
    g_cpu._flag_C = sneg;
}

/* ---- CD batch 18 ---- */
/* CD:0706  Fill the 32 rows from the six words in dp 45..4F (x = 1, m = 0). */
static void row_fill(void)
{
    static const u16 dst[6] = { 0xBC89, 0xBC8B, 0xBD09, 0xBD0B, 0xBD89, 0xBD8B };
    ldx(0x00);
    do {
        for (int k = 0; k < 6; k++) {
            lda16(dp16((u8)(0x45 + 2 * k)));
            for (int j = 0; j < 16; j += 4) abs16_set(dst[k] + j + g_cpu.X, g_cpu.A);
        }
        lda16(g_cpu.X); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0010)); tax();
        cpx(0x80);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 1);
    dc_set_mx(1, 0);
}
/* dp 45.. = base + offset (+ BB01/BB03 shake on the first two). */
static void scroll_sum(int k, u16 base, u16 off)
{
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(base), abs16(off)));
    if (k < 2) { g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xBB01 + 2 * k))); }
    dp16_set((u8)(0x45 + 2 * k), g_cpu.A);
}
/* CD:06C4  Scroll registers for the 32 rows: the 2C70.. base values plus the
 * CCA4.. offsets (and the BB01/BB03 shake on BG1) into BC89/BD09/BD89. */
DC_ENTRY(CD06C4, RTS)
{
    dc_set_mx(1, 1);
    dc_set_mx(0, 1);
    for (int k = 0; k < 6; k++) scroll_sum(k, (u16)(0x2C70 + 2 * k), (u16)(0xCCA4 + 2 * k));
    row_fill();
}
/* CD:076C  Same, with BG3 following the current slot (CA5A+CA5E, CA5C+CA60). */
DC_ENTRY(CD076C, RTS)
{
    dc_set_mx(1, 1);
    dc_set_mx(0, 1);
    for (int k = 0; k < 4; k++) scroll_sum(k, (u16)(0x2C70 + 2 * k), (u16)(0xCCA4 + 2 * k));
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xCA5A), abs16(0xCA5E)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCCAC))); dp16_set(0x4D, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xCA5C), abs16(0xCA60)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCCAE))); dp16_set(0x4F, g_cpu.A);
    row_fill();
}
/* CD:2BFC  Shift the 0710 sprite list and 0901 table up by 4 entries, then rebuild
 * 16 sprites from the 4500 template at the slot's position (CA5A+CA5E, CA5C+CA60),
 * pushing those off screen (Y = F0) to the hidden row. */
DC_ENTRY(CD2BFC, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    ldx(0x08BF); ldy(0x08FF); lda16(0x01AF); dc_mvp(0x7E, 0x7E);
    ldx(0x091B); ldy(0x091F); lda16(0x001A); dc_mvp(0x7E, 0x7E);
    ldx(dp16(0x43));
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xCA5A + g_cpu.X), abs16(0xCA5E + g_cpu.X))); dp16_set(0x45, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xCA5C + g_cpu.X), abs16(0xCA60 + g_cpu.X))); dp16_set(0x47, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    tdc(); tax();
    do {
        g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0x45), abs8(0x4500 + g_cpu.X))); abs8_set(0x0710 + g_cpu.X, (u8)g_cpu.A);
        g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0x47), abs8(0x4501 + g_cpu.X))); abs8_set(0x0711 + g_cpu.X, (u8)g_cpu.A);
        lda8(dc_adc8(dp8(0x48), 0x00) & 0x01);           /* Y high byte (+ carry of the low add) */
        bool high = !g_cpu._flag_Z;
        lda8(abs8(0x0711 + g_cpu.X));
        dc_cmp8((u8)g_cpu.A, high ? 0xF0 : 0xD5);
        if (high ? !g_cpu._flag_C : g_cpu._flag_C) { lda8(0xF0); abs8_set(0x0711 + g_cpu.X, 0xF0); }
        lda8(abs8(0x4502 + g_cpu.X)); abs8_set(0x0712 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0x4503 + g_cpu.X) | 0x30); abs8_set(0x0713 + g_cpu.X, (u8)g_cpu.A);
        inx(); inx(); inx(); inx();
        cpx(0x0040);
    } while (!g_cpu._flag_Z);
    ldx(0xAAAA); stx_abs(0x0901); stx_abs(0x0903);
}
/* CD:19E5  Operand bit 7: CA7A..CA7F of the slot = six CD:0B16 values & 1F.
 * Otherwise move by CA7A[op & 0B]: CD:2E13 + 2E4D (bit 6) or CD:2E78 + 2E3C, with
 * dp45 = CA7D of that entry for the second step. */
DC_ENTRY(CD19E5, RTS)
{
    lda8(arg8());
    if (g_cpu._flag_N) {
        for (int k = 0; k < 6; k++) {
            dc_CD0B16_glue();
            ldx(dp16(0x43));
            lda8((u8)g_cpu.A & 0x1F); abs8_set(0xCA7A + k + g_cpu.X, (u8)g_cpu.A);
        }
        return;
    }
    lda8((u8)g_cpu.A & 0x0B);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x43))); tax();
    lda8(abs8(0xCA7A + g_cpu.X)); dp8_set(0x45, (u8)g_cpu.A);
    lda8(arg8() & 0x40);
    bool up = !g_cpu._flag_Z;
    phx();
    if (up) dc_CD2E13_glue(); else dc_CD2E78_glue();
    plx();
    lda8(abs8(0xCA7D + g_cpu.X)); dp8_set(0x45, (u8)g_cpu.A);
    if (up) dc_CD2E4D_glue(); else dc_CD2E3C_glue();
}

/* ---- CD batch 19 ---- */
/* CD:1C7B  Spinning orbit: centre 80,87; radii = CD28 (stepped every 4th frame,
 * dp4B doubled); operand 0 reloads the radii table (CD:1CBF), else CD:1CD5; on odd
 * frames the offset is mirrored. */
DC_ENTRY(CD1C7B, RTS)
{
    lda8(0x80); dp8_set(0x4D, 0x80);
    lda8(0x87); dp8_set(0x4E, 0x87);
    lda8(dp8(0x7C) & 0x03);
    if (g_cpu._flag_Z) inc_abs(0xCD28);
    lda8(abs8(0xCD28)); dp8_set(0x49, (u8)g_cpu.A); dp8_set(0x4A, (u8)g_cpu.A); dp8_set(0x4C, (u8)g_cpu.A);
    asl_a(1); dp8_set(0x4B, (u8)g_cpu.A);
    lda8(arg8());
    if (g_cpu._flag_Z) dc_CD1CBF_glue(); else dc_CD1CD5_glue();
    lda8(dp8(0x7C) & 0x01);
    if (g_cpu._flag_Z) return;
    ldx(dp16(0x43));
    lda8(abs8(0xCA5C + g_cpu.X)); neg_a(); abs8_set(0xCA5C + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0xCA5A + g_cpu.X)); neg_a(); abs8_set(0xCA5A + g_cpu.X, (u8)g_cpu.A);
}
/* CD:1BCE  Slot n: angles CAE4/CAE6/CAE8 += op1..3; CAF6/CAF8/CAFA = sin(angle) *
 * 20/20/24 / 256 + op4..6. */
DC_ENTRY(CD1BCE, RTS)
{
    static const struct { u16 ang, dst; u8 r; } ax[3] = { { 0xCAE4, 0xCAF6, 0x20 }, { 0xCAE6, 0xCAF8, 0x20 }, { 0xCAE8, 0xCAFA, 0x24 } };
    lda8(arg8()); tax();
    for (int k = 0; k < 3; k++) {
        g_cpu._flag_C = 0; lda8(dc_adc8(abs8(ax[k].ang + g_cpu.X), argn((u16)(k + 1)))); abs8_set(ax[k].ang + g_cpu.X, (u8)g_cpu.A);
    }
    iny();
    for (int k = 0; k < 3; k++) {
        if (k) iny();
        lda8(ax[k].r); dp8_set(0x45, ax[k].r);
        lda8(abs8(ax[k].ang + g_cpu.X)); dc_CD1BC0_glue();
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, bus8(lp(0x40) + g_cpu.Y)));
        abs8_set(ax[k].dst + g_cpu.X, (u8)g_cpu.A);
    }
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x40), 0x0006)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
}
/* CD:2D51  Window wave table: 32 rows of BB06/BB86/BC06 (width dp45, HDMA line
 * counts) and BB07/BB87/BC07 (offset |X - CD04| floored at 0, rising then falling);
 * CD04 counts down to 0. */
static void window_wave(void)
{
    lda8(abs8(0xCD04));
    if (!g_cpu._flag_Z) dec_abs(0xCD04);
    lda8(abs8(0xCD04)); dp8_set(0x47, (u8)g_cpu.A);
    dc_set_mx(1, 1);
    tdc(); tax(); tay();
    for (int half = 0; half < 2; half++) {
        do {
            lda8(dp8(0x45));
            abs8_set(0xBB06 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0xBB86 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0xBC06 + g_cpu.Y, (u8)g_cpu.A);
            lda8((u8)g_cpu.X); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x47)));
            if (!g_cpu._flag_C) tdc();
            dp8_set(0x49, (u8)g_cpu.A);
            lda8((u8)g_cpu.A | 0x80); abs8_set(0xBB07 + g_cpu.Y, (u8)g_cpu.A);
            lda8(dp8(0x49) | 0x40); abs8_set(0xBB87 + g_cpu.Y, (u8)g_cpu.A);
            lda8(dp8(0x49) | 0x20); abs8_set(0xBC07 + g_cpu.Y, (u8)g_cpu.A);
            if (half) { dex(); dex(); } else { inx(); inx(); }
            iny(); iny(); iny(); iny();
            cpy(half ? 0x80 : 0x40);
        } while (!g_cpu._flag_Z);
        if (!half) { dex(); dex(); }
    }
    dc_set_mx(1, 0);
}
DC_ENTRY(CD2D39, RTS) { lda8(0x01); abs8_set(0x5D9A, 0x01); lda8(0x14); dp8_set(0x45, 0x14); window_wave(); }
DC_ENTRY(CD2D44, RTS) { lda8(0x04); dp8_set(0x45, 0x04); window_wave(); }
DC_ENTRY(CD2D4A, RTS) { abs8_set(0xCD04, 0); lda8(0x04); dp8_set(0x45, 0x04); window_wave(); }

/* ---- CD batch 20 ---- */
/* One row pair of the span table: B900,X = max(dp4B - v, 0), B901,X = min(v + dp4D, FF), both
 * also sent through the WRAM port (`alt`: only on even counts of dp4F). X -= 2. */
static void span_row(u8 v, bool alt)
{
    bool port = true;
    if (alt) { port = !((u8)(dp8(0x4F) + 1) & 1); dp8_set(0x4F, (u8)(dp8(0x4F) + 1)); }
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0x4B), v));
    if (!g_cpu._flag_C) tdc();
    abs8_set(0xB900 + g_cpu.X, (u8)g_cpu.A);
    if (port) bus8_set(0x002180, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(v, dp8(0x4D)));
    if (g_cpu._flag_C) lda8(0xFF);
    abs8_set(0xB901 + g_cpu.X, (u8)g_cpu.A);
    if (port) bus8_set(0x002180, (u8)g_cpu.A);
    dex(); dex();
}
/* Span table from the half-widths B800[0..dp47) around the centre CAA6[CAAA], written downward
 * from B9FE: first each row in order, then back up through the distinct widths (mirrored half). */
static void span_table(bool alt)
{
    dc_set_mx(1, 1);
    tdc(); tay();
    lda8(abs8(0xCAAA)); tax();
    lda8(abs8(0xCAA6 + g_cpu.X)); dp8_set(0x4B, (u8)g_cpu.A); dp8_set(0x4D, (u8)g_cpu.A);
    ldx(0xFE);
    if (alt) { lda8(0xFF); dp8_set(0x4F, 0xFF); }
    do {
        span_row(abs8(0xB800 + g_cpu.Y), alt);
        iny(); cpy(dp8(0x47));
    } while (!g_cpu._flag_Z);
    tdc(); abs8_set(0xB800 + g_cpu.Y, (u8)g_cpu.A);
    ldy((u16)(g_cpu.Y - 1));
    lda8((u8)(abs8(0xB800 + g_cpu.Y) - 1)); dp8_set(0x47, (u8)g_cpu.A);
    do {
        lda8(abs8(0xB800 + g_cpu.Y)); dc_cmp8((u8)g_cpu.A, abs8(0xB7FF + g_cpu.Y));
        if (!g_cpu._flag_Z) span_row(dp8(0x47), alt);
        dp8_set(0x47, (u8)(dp8(0x47) - 1));
        ldy((u16)(g_cpu.Y - 1));
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, 0);
}
/* CD:31FE  Window table for the circle: from the half-widths at B800 (CD:31AA) and
 * the centre CAA6[CAAA], left/right edges (clamped 0..FF) go to B900.. backwards
 * from FE and to the WRAM port, top half then mirrored bottom half. */
DC_ENTRY(CD31FE, RTS) { span_table(false); }
/* CD:28D2  Slot X (8-bit index): CAEC/CAF0/CAF4 = CAD6 + cos terms (radius CAE2),
 * CAEA/CAEE/CAF2 = CAD4 + sin terms (radius CAE0), angles CAE4/CAE6/CAE8 and
 * scales CADA/CADC/CADE (CD:3DC3). */
DC_ENTRY(CD28D2, RTS)
{
    static const u16 ang[3] = { 0xCAE4, 0xCAE6, 0xCAE8 }, rad[3] = { 0xCADA, 0xCADC, 0xCADE };
    static const u16 cdst[3] = { 0xCAEC, 0xCAF0, 0xCAF4 }, sdst[3] = { 0xCAEA, 0xCAEE, 0xCAF2 };
    for (int pass = 0; pass < 2; pass++) {
        lda8(abs8((pass ? 0xCAE0 : 0xCAE2) + g_cpu.X)); dp8_set(0x45, (u8)g_cpu.A);
        for (int k = 0; k < 3; k++) {
            lda8(abs8(rad[k] + g_cpu.X)); dp8_set(0x47, (u8)g_cpu.A);
            lda8(abs8(ang[k] + g_cpu.X));
            if (!pass) { g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x40)); }
            dc_CD3DC3_glue();
            g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8((pass ? 0xCAD4 : 0xCAD6) + g_cpu.X)));
            abs8_set((pass ? sdst[k] : cdst[k]) + g_cpu.X, (u8)g_cpu.A);
        }
    }
}
/* CD:1FC8  Wave: for the 0x6A sprites from Y = 0, C15D/C161 = sin(frame*4 + n) *
 * 0x32 / 256 and C15F/C163 = dp49 + sin(frame + n) * 0F (CD:204B, CD:20B1). */
DC_ENTRY(CD1FC8, RTS)
{
    lda8(arg8()); dp8_set(0x45, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xCA5C), abs16(0xCA60))); dp16_set(0x49, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    lda8(dp8(0x7C)); tax(); stx_dp(0x45);
    asl_a(2); tax(); stx_dp(0x47);
    tdc(); tay();
    do {
        lda8(dp8(0x47));
        bool neg = g_cpu._flag_N;
        tax();
        lda8(sin_mag()); bus8_set(0x004202, (u8)g_cpu.A);
        lda8(0x32); bus8_set(0x004203, 0x32);
        lda8(bus8(0x004217));
        if (neg) neg_a();
        dc_CD204B_glue();
        bool flip = g_cpu._flag_C;
        lda8(bus8(0x004217));
        if (flip) neg_a();
        dc_CD20B1_glue();
        dp8_set(0x45, (u8)(dp8(0x45) + 1));
        { u8 v = (u8)(dp8(0x47) + 1); dp8_set(0x47, v); dc_set_flags_nz8(v); }
        dc_set_mx(0, g_cpu.x_flag);
        lda16(g_cpu.Y); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0008)); tay();
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        cpy(0x0350);
    } while (!g_cpu._flag_Z);
}
/* Slot op 0 (8-bit index): add operand op[k] to field dst[k], call `then`, skip `len` bytes. */
static void add_ops(const u16 *dst, const u8 *op, int n, glue_fn then, u16 len)
{
    dc_set_mx(1, 1);
    lda8(arg8()); tax();
    for (int k = 0; k < n; k++) {
        lda8(abs8(dst[k] + g_cpu.X));
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, argn(op[k])));
        abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A);
    }
    then();
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x40), len)); dp16_set(0x40, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
}
/* CD:2717  Slot n (op 0): add op 1 to CAD4, op 2 to CAD6, op 3 to the scales CADA/CADC/CADE,
 * op 4 / op 5 to the radii CAE0 / CAE2, op 6 to the angles CAE4/CAE6/CAE8; then CD:28D2. */
DC_ENTRY(CD2717, RTS)
{
    static const u16 dst[10] = { 0xCAD4, 0xCAD6, 0xCADA, 0xCADC, 0xCADE, 0xCAE0, 0xCAE2, 0xCAE4, 0xCAE6, 0xCAE8 };
    static const u8 op[10] = { 1, 2, 3, 3, 3, 4, 5, 6, 6, 6 };
    add_ops(dst, op, 10, dc_CD28D2_glue, 0x0006);
}
/* CD:010A  dp59 = CD00, dp53 = 967C[CD00] (2 at index 6 becomes 5); CCFF += CE:EFEA[CD01 & 7];
 * dp73 = mask CE:F1C2[CCFF & 7], dp75 = (CCFF & 38) / 4; CD:0154 for X = 0 and X = 4. */
DC_ENTRY(CD010A, RTS)
{
    lda8(abs8(0xCD00)); tax(); stx_dp(0x59);
    lda8(abs8(0x967C + g_cpu.X));
    if ((u8)g_cpu.A == 0x02 && g_cpu.X == 0x0006) lda8(0x05);
    tax(); stx_dp(0x53);
    lda8(abs8(0xCD01) & 0x07); tax();
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCCFF), bus8(0xCEEFEA + (u32)g_cpu.X))); abs8_set(0xCCFF, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    u16 v = g_cpu.A;
    lda16(v & 0x0007); asl_a(1); tax();
    lda16(bus16(0xCEF1C2 + (u32)g_cpu.X)); dp16_set(0x73, g_cpu.A);
    lda16(v & 0x0038); lsr_a(2); dp16_set(0x75, g_cpu.A);
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(0x0000); dc_CD0154_glue();
    ldx(0x0004); dc_CD0154_glue();
}
/* CD:1509  Tile entry [dp53] (bits 0-12 tile, bit 13 two-plane): CD:1488 copies D0:tile*8;
 * a one-plane tile then takes its 8 bytes after the copy as the low byte of each row's
 * mask word at 2D10,Y, with the old mask | row, AND dp5F, in the high byte. Y += 20. */
DC_ENTRY(CD1509, RTS)
{
    lda16(bus16(lp(0x53))); dp16_set(0x65, g_cpu.A);
    lda16(g_cpu.A & 0x1FFF); asl_a(3); tax();
    lda16(bus16(lp(0x53)) & 0x2000);
    bool two = !g_cpu._flag_Z;
    dc_CD1488_glue();
    if (!two) {
        lda16(g_cpu.X); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0010)); tax();
        for (int k = 0; k < 8; k++) {
            u32 d = 0x2D10 + (u32)g_cpu.Y + 2 * k;
            lda16(bus8(0xD00000 + (u32)g_cpu.X + k)); dp16_set(0x57, g_cpu.A);
            lda16((g_cpu.A | abs16(d)) & dp16(0x5F));
            xba();
            lda16(g_cpu.A | dp16(0x57)); abs16_set(d, g_cpu.A);
        }
    }
    lda16(g_cpu.Y); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0020)); tay();
}
/* CD:07E2  Per-line scroll tables for three layers (32 lines, 4 bytes each): BC89/BD09/BD89
 * = H scroll (+ the wave 0A00,X when CC91/CC93/CC95 is set), BC8B/BD0B/BD8B = V scroll. */
DC_ENTRY(CD07E2, RTS)
{
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x2C70), abs16(0xCCA4)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xBB01))); dp16_set(0x45, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x2C72), abs16(0xCCA6)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xBB03))); dp16_set(0x47, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x2C74), abs16(0xCCA8))); dp16_set(0x49, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x2C76), abs16(0xCCAA))); dp16_set(0x4B, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xCA5A), abs16(0xCA5E)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCCAC))); dp16_set(0x4D, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xCA5C), abs16(0xCA60)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCCAE))); dp16_set(0x4F, g_cpu.A);
    lda16(abs16(0xCC91)); dp16_set(0x1A, g_cpu.A);
    lda16(abs16(0xCC93)); dp16_set(0x1C, g_cpu.A);
    lda16(abs16(0xCC95)); dp16_set(0x1E, g_cpu.A);
    static const u8 wave[3] = { 0x1A, 0x1C, 0x1E }, hs[3] = { 0x45, 0x49, 0x4D }, vs[3] = { 0x47, 0x4B, 0x4F };
    static const u16 hd[3] = { 0xBC89, 0xBD09, 0xBD89 };
    ldx(0x00);
    do {
        for (int k = 0; k < 3; k++) {
            if (dp16(wave[k]) == 0) lda16(dp16(hs[k]));
            else { g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x0A00 + g_cpu.X), dp16(hs[k]))); }
            abs16_set(hd[k] + g_cpu.X, g_cpu.A);
        }
        for (int k = 0; k < 3; k++) { lda16(dp16(vs[k])); abs16_set(hd[k] + 2 + g_cpu.X, g_cpu.A); }
        ldx(g_cpu.X + 4);
    } while (g_cpu.X != 0x80);
    g_cpu._flag_C = 1;
    tdc();
    dc_set_mx(1, 0);
}
/* Step byte `cur` one toward `target`; reaching 0 from above clears `flag`. Leaves the
 * compare's carry. */
static void ramp(u16 cur, u16 target, u16 flag)
{
    lda8(abs8(cur)); dc_cmp8((u8)g_cpu.A, abs8(target));
    if (g_cpu._flag_Z) return;
    if (g_cpu._flag_C) { dec_abs(cur); if (g_cpu._flag_Z) abs8_set(flag, 0); }
    else inc_abs(cur);
}
/* CD:0B8B  Layer shake: amplitude CCB6 ramps to CCB7 (0 clears CCB0), dp45 = 4202 * CCB6 / 256.
 * When CCB8 is on, every CCBB frames CCB9 = sin-ish C0:FE00[dp7C + 41], and every CCBD frames
 * amplitude CCBE ramps to CCBF (0 clears CCB8), dp47 = CCB9 * CCBE / 256. CCC0 bits 7..2 pick
 * which of CCA4..CCAE take dp45 / dp47. CCE5 bit 7 / on copies CCD9 (12) / CCC1 (24) to 0598 / 0580. */
static void shake_b4(void);
static void shake_a4(void)
{
    lda8(abs8(0xCCB6)); bus8_set(0x004203, (u8)g_cpu.A);
    lda8(bus8(0x004217)); dp8_set(0x45, (u8)g_cpu.A);
    shake_b4();
}
DC_ENTRY(CD0B8B, RTS)
{
    /* LDY $CC,X (the last two bytes of 0B8A STZ $CCB4): dead load, Y is reloaded below. */
    ramp(0xCCB6, 0xCCB7, 0xCCB0);
    shake_a4();
}
static void shake_b4(void)
{
    lda8(abs8(0xCCB8));
    if (g_cpu._flag_Z) {
        tax(); stx_abs(0xCCA6); stx_abs(0xCCAA); stx_abs(0xCCAE);
        dp8_set(0x47, (u8)g_cpu.A);
    } else {
        inc_abs(0xCCBA);
        lda8(abs8(0xCCBA)); dc_cmp8((u8)g_cpu.A, abs8(0xCCBB));
        if (g_cpu._flag_C) {
            abs8_set(0xCCBA, 0);
            lda8(0x40); dc_CD0B16_glue(); abs8_set(0xCCB9, (u8)g_cpu.A);
        }
        lda8(abs8(0xCCB9)); bus8_set(0x004202, (u8)g_cpu.A);
        inc_abs(0xCCBC);
        lda8(abs8(0xCCBC)); dc_cmp8((u8)g_cpu.A, abs8(0xCCBD));
        if (g_cpu._flag_C) { abs8_set(0xCCBC, 0); ramp(0xCCBE, 0xCCBF, 0xCCB8); }
        lda8(abs8(0xCCBE)); bus8_set(0x004203, (u8)g_cpu.A);
        lda8(bus8(0x004217)); dp8_set(0x47, (u8)g_cpu.A);
    }
    lda8(dp8(0x45)); tay();
    lda8(dp8(0x47)); tax();
    lda8(abs8(0xCCC0));
    if (!g_cpu._flag_Z) {
        static const u16 dst[6] = { 0xCCA4, 0xCCA6, 0xCCA8, 0xCCAA, 0xCCAC, 0xCCAE };
        for (int k = 0; k < 6; k++) {
            lda8(asl8((u8)g_cpu.A));
            if (g_cpu._flag_C) { if (k & 1) stx_abs(dst[k]); else sty_abs(dst[k]); }
        }
    }
    lda8(abs8(0xCCE5));
    if (g_cpu._flag_Z) return;
    bool all = g_cpu._flag_N;
    dc_set_mx(0, 0);
    if (all) { ldx(0xCCD9); ldy(0x0598); lda16(0x000B); }
    else { ldx(0xCCC1); ldy(0x0580); lda16(0x0017); }
    dc_mvn(0x7E, 0x7E);
    tdc();
    dc_set_mx(1, 0);
    lda8(abs8(0xCCE5) & (all ? 0x7F : 0xFE)); abs8_set(0xCCE5, (u8)g_cpu.A);
}
/* Signed sine CE:F48E[X] (sign = X bit 7) times dp45 / 256, through 4202/4203 (`early` steps
 * Y by 4 while the multiply runs). */
static void wave_term(bool early)
{
    bool neg = g_cpu.X & 0x80;
    lda8(bus8(0xCEF48E + (u32)g_cpu.X)); bus8_set(0x004202, (u8)g_cpu.A);
    lda8(dp8(0x45)); bus8_set(0x004203, (u8)g_cpu.A);
    if (early) { iny(); iny(); iny(); iny(); }
    lda8(bus8(0x004217));
    if (neg) lda8((u8)((u8)~g_cpu.A + 1));
}
/* CD:253B  Line wave table at 0A00 (words, sign-extended), amplitude op 1, phase CC90 advanced
 * on frames where dp7C & (op 0 & 1F) == 0. Op 2 bit 7: 64 lines, phase step op 2 & 7F, angle +4
 * per line (CA92 = 3); else 32 lines, phase step op 2, angle +8 (CA92 = 2). Op 0 bits 7/6/5 turn
 * the wave on for layers CC95/CC93/CC91 (CC91 stays off when 99D2 & 7F == 1). */
DC_ENTRY(CD253B, RTS)
{
    ldy(0x0001);
    lda8(bus8(lp(0x40) + g_cpu.Y)); dp8_set(0x45, (u8)g_cpu.A);
    iny();
    lda8(bus8(lp(0x40)) & 0x1F); dp8_set(0x47, (u8)g_cpu.A);
    lda8(bus8(lp(0x40) + g_cpu.Y));
    bool fine = g_cpu._flag_N;
    u8 step = fine ? (u8)(g_cpu.A & 0x7F) : (u8)g_cpu.A;
    if (fine) dp8_set(0x49, step);
    dc_set_mx(1, 1);
    lda8(dp8(0x7C) & dp8(0x47));
    if (g_cpu._flag_Z) { g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCC90), step)); abs8_set(0xCC90, (u8)g_cpu.A); }
    lda8(abs8(0xCC90)); tax();
    tdc(); tay();
    if (fine) {
        do {
            wave_term(false);
            abs8_set(0x0A00 + g_cpu.Y, (u8)g_cpu.A);
            if (g_cpu._flag_N) lda8(0xFF); else tdc();
            abs8_set(0x0A01 + g_cpu.Y, (u8)g_cpu.A);
            lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x04)); tax();
            iny(); iny(); iny(); iny();
        } while (!g_cpu._flag_Z);
        lda8(0x03); abs8_set(0xCA92, 0x03);
    } else {
        do {
            wave_term(true);
            abs8_set(0x09FC + g_cpu.Y, (u8)g_cpu.A);
            if (g_cpu._flag_N) lda8(0xFF); else tdc();
            abs8_set(0x09FD + g_cpu.Y, (u8)g_cpu.A);
            lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x08)); tax();
            cpy(0x80);
        } while (!g_cpu._flag_Z);
        lda8(0x02); abs8_set(0xCA92, 0x02);
    }
    dc_set_mx(1, 0);
    tdc(); tax();
    stx_abs(0xCC91); stx_abs(0xCC93); stx_abs(0xCC95);
    lda8(bus8(lp(0x40)));
    static const u16 on[3] = { 0xCC95, 0xCC93, 0xCC91 };
    for (int k = 0; k < 3; k++) {
        lda8(asl8((u8)g_cpu.A));
        u8 m = abs8(on[k]), c = m >> 7;
        m = (u8)(m << 1 | g_cpu._flag_C); abs8_set(on[k], m);
        g_cpu._flag_C = c; dc_set_flags_nz8(m);
    }
    ldy(dp16(0x40)); iny(); iny(); dp16_set(0x40, g_cpu.Y);
    lda8(abs8(0x99D2) & 0x7F); dc_cmp8((u8)g_cpu.A, 0x01);
    if (g_cpu._flag_Z) { tdc(); tax(); stx_abs(0xCC91); }
}
/* CD:0B51  Full layer shake (CD:0B8B is its amplitude step): when CCB0 is on, every CCB3 frames
 * CCB1 = C0:FE00[dp7C + CCB2 + 1] and every CCB5 frames CCB6 ramps; else CCA4/CCA8/CCAC = 0. */
DC_ENTRY(CD0B51, RTS)
{
    lda8(abs8(0xCCB0));
    if (g_cpu._flag_Z) {
        tax(); stx_abs(0xCCA4); stx_abs(0xCCA8); stx_abs(0xCCAC);
        dp8_set(0x45, (u8)g_cpu.A);
        shake_b4();
        return;
    }
    inc_abs(0xCCB2);
    lda8(abs8(0xCCB2)); dc_cmp8((u8)g_cpu.A, abs8(0xCCB3));
    if (g_cpu._flag_C) { abs8_set(0xCCB2, 0); dc_CD0B16_glue(); abs8_set(0xCCB1, (u8)g_cpu.A); }
    lda8(abs8(0xCCB1)); bus8_set(0x004202, (u8)g_cpu.A);
    inc_abs(0xCCB4);
    lda8(abs8(0xCCB4)); dc_cmp8((u8)g_cpu.A, abs8(0xCCB5));
    if (g_cpu._flag_C) { abs8_set(0xCCB4, 0); ramp(0xCCB6, 0xCCB7, 0xCCB0); }
    shake_a4();
}
/* CD:3273  CD:31FE with every other row pair left out of the WRAM port stream. */
DC_ENTRY(CD3273, RTS) { span_table(true); }
/* CD:07B9  CD:07E2, then layer 3 (BD89/BD8B) gets per-line rows for the second half too
 * (X = 80..FC). */
DC_ENTRY(CD07B9, RTS)
{
    dc_CD07E2_glue();
    dc_set_mx(0, 1);
    do {
        if (dp16(0x1E) == 0) lda16(dp16(0x4D));
        else { g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x0A00 + g_cpu.X), dp16(0x4D))); }
        abs16_set(0xBD89 + g_cpu.X, g_cpu.A);
        lda16(dp16(0x4F)); abs16_set(0xBD8B + g_cpu.X, g_cpu.A);
        inx(); inx(); inx(); inx();
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 0);
}
/* CD:13F6  CD:1509 (Y kept), then CD:13FE twice. */
DC_ENTRY(CD13F6, RTS)
{
    dc_push16(g_cpu.Y);
    dc_CD1509_glue();
    ldy(dc_pull16());
    dc_CD13FE_glue();
    dc_CD13FE_glue();
}
/* CD:1445  CD:1509 (Y kept), then the 32 bytes at 2D00,Y through the C0:FD00 table. */
static void bitrev_tile(void);
DC_ENTRY(CD1445, RTS)
{
    dc_push16(g_cpu.Y);
    dc_CD1509_glue();
    ldy(dc_pull16());
    bitrev_tile();
}
/* CD:144A  The 32 bytes at 2D00,Y through the C0:FD00 table (horizontal flip). */
static void bitrev_tile(void)
{
    tdc();
    dc_set_mx(1, 0);
    lda8(0x08); dp8_set(0x61, 0x08);
    do {
        for (int k = 0; k < 4; k++) {
            lda8(abs8(0x2D00 + g_cpu.Y + k)); tax();
            lda8(bus8(0xC0FD00 + (u32)g_cpu.X)); abs8_set(0x2D00 + g_cpu.Y + k, (u8)g_cpu.A);
        }
        iny(); iny(); iny(); iny();
        u8 n = (u8)(dp8(0x61) - 1); dp8_set(0x61, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
    dc_set_mx(0, 0);
}
/* CD:367C  CD:3685 with DB = 00. */
DC_ENTRY(CD367C, RTS)
{
    dc_push8(g_cpu.DB);
    tdc(); dc_push8((u8)g_cpu.A);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    dc_jsr(0xCD3685, 0xCD3680);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}
/* CD:2173  Slot op & 1 (CC64): blend factor dp21 = CAFC; dp12/14/16 = the sin terms CAEA/CAEE/CAF2
 * pulled toward CAD4, dp13/15/17 = the cos terms CAEC/CAF0/CAF4 toward CAD6 (CD:211B);
 * dp20 = 0, then CD:367C. */
DC_ENTRY(CD2173, RTS)
{
    static const u16 src[6] = { 0xCAEA, 0xCAEE, 0xCAF2, 0xCAEC, 0xCAF0, 0xCAF4 };
    static const u8 dst[6] = { 0x12, 0x14, 0x16, 0x13, 0x15, 0x17 };
    lda8(bus8(lp(0x40)) & 0x01); abs8_set(0xCC64, (u8)g_cpu.A);
    tax();
    lda8(abs8(0xCAFC + g_cpu.X)); dp8_set(0x21, (u8)g_cpu.A);
    for (int k = 0; k < 6; k++) {
        if (k % 3 == 0) { lda8(abs8((k ? 0xCAD6 : 0xCAD4) + g_cpu.X)); dp8_set(0x47, (u8)g_cpu.A); }
        lda8(abs8(src[k] + g_cpu.X));
        dc_CD211B_glue();
        dp8_set(dst[k], (u8)g_cpu.A);
    }
    abs8_set(0x0020, 0);
    dc_CD367C_glue();
}
/* CD:238D  Normalize the 16.16 pair dp25:dp23 down to 8 bits of fraction: with n leading zero
 * bits in dp23 (n < 8), dp23 <<= n and dp25 >>= 8 - n; with n >= 8, dp23 = low byte << 8. */
DC_ENTRY(CD238D, RTS)
{
    int n = 0;
    while (n < 8 && !(dp16(0x23) & (0x8000 >> n))) n++;
    if (n == 0) {
        g_cpu._flag_C = 1;
        lda16(dp16(0x25)); xba(); lda16(g_cpu.A & 0x00FF); dp16_set(0x25, g_cpu.A);
    } else if (n == 8) {
        g_cpu._flag_C = 0;
        lda16(dp16(0x23)); xba(); lda16(g_cpu.A & 0xFF00); dp16_set(0x23, g_cpu.A);
    } else {
        lda16(dp16(0x23)); asl_a(n); dp16_set(0x23, g_cpu.A);
        lda16(dp16(0x25)); lsr_a(8 - n); dp16_set(0x25, g_cpu.A);
    }
}
/* CD:107D  Build tilemap rows at 7E:D000 (0x200 per row block) from the layout records at
 * D1:A575[C9FE] for C9FC & 3F blocks: CAAB = record step (bit 7: walk the 32-byte ring at CAAD),
 * CAAE.. = 16 bytes of presence bits; each present cell (column dp65, row dp66 of CA01 rows)
 * gets tile byte [dp53] packed as ((t & 38) * 4 + (t & 7) * 2) and an attribute byte
 * (t & C0) ^ dp70 | 38 | priority 04 when CF:E200[dp73 + tile / 16] has bit CE:F03E[(tile / 2) & 7]. */
DC_ENTRY(CD107D, RTS)
{
    stx_dp(0x73);
    abs8_set(0xCAAD, 0);
    lda8(0x38); dp8_set(0x71, 0x38);
    dp8_set(0x6F, 0); dp8_set(0x70, 0);
    lda8(0xD1); dp8_set(0x55, 0xD1);
    ldx(0xD000); stx_dp(0x57);
    lda8(0x7E); dp8_set(0x59, 0x7E);
    lda8(abs8(0xC9FC) & 0x3F); abs8_set(0xC9FC, (u8)g_cpu.A);
    do {
        lda8(abs8(0xCA01)); dp8_set(0x6D, (u8)g_cpu.A);
        dc_set_mx(0, 0);
        lda16(abs16(0xC9FE)); asl_a(1); tax();
        lda16(bus16(0xD1A575 + (u32)g_cpu.X)); dp16_set(0x53, g_cpu.A);
        lda16(bus16(lp(0x53)) & 0x00FF); abs16_set(0xCAAB, g_cpu.A);
        ldy(0x0001);
        do {
            lda16(bus16(lp(0x53) + g_cpu.Y)); abs16_set(0xCAAD + g_cpu.Y, g_cpu.A);
            iny(); iny(); cpy(0x0021);
        } while (!g_cpu._flag_Z);
        lda16(abs16(0xCA01) & 0x00FF); asl_a(1); lda16((u16)(g_cpu.A + 1));
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x53))); dp16_set(0x53, g_cpu.A);
        dp16_set(0x65, 0); dp16_set(0x61, 0);
        tdc();
        dc_set_mx(1, 0);
        tay();
        lda8(0x01); dp8_set(0x5F, 0x01);
        for (;;) {
            u8 n = (u8)(dp8(0x5F) - 1); dp8_set(0x5F, n);
            if (n == 0) {
                lda8(0x08); dp8_set(0x5F, 0x08);
                ldx(dp16(0x61)); dp8_set(0x61, (u8)(dp8(0x61) + 1));
                lda8(abs8(0xCAAE + g_cpu.X)); dp8_set(0x63, (u8)g_cpu.A);
            }
            u8 bits = dp8(0x63); g_cpu._flag_C = bits >> 7; bits = (u8)(bits << 1);
            dp8_set(0x63, bits); dc_set_flags_nz8(bits);
            if (g_cpu._flag_C) {
                g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0x65), dp8(0x6F))); tax();
                lda8(bus8(0xCEF046 + (u32)g_cpu.X)); tax(); stx_dp(0x69);
                lda8(dp8(0x66));
                dc_set_mx(0, 0);
                asl_a(5); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x69))); tay();
                tdc();
                dc_set_mx(1, 0);
                lda8(bus8(lp(0x53))); dp8_set(0x67, (u8)g_cpu.A);
                lda8(abs8(0xCAAB));
                dc_set_mx(0, 0);
                if (g_cpu._flag_N) {
                    inc_abs(0xCAAD);
                    dp16_set(0x53, (u16)(dp16(0x53) + 1));
                    lda16(abs16(0xCAAD) & 0x001F);
                    if (g_cpu._flag_Z) { g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x53), 0x0020)); dp16_set(0x53, g_cpu.A); }
                } else {
                    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x53), abs16(0xCAAB))); dp16_set(0x53, g_cpu.A);
                }
                tdc();
                dc_set_mx(1, 0);
                u8 t = dp8(0x67);
                dp8_set(0x6B, (u8)((t & 0x07) << 1));
                g_cpu._flag_C = 0; lda8(dc_adc8((u8)((t & 0x38) << 2), dp8(0x6B)));
                bus8_set(lp(0x57) + g_cpu.Y, (u8)g_cpu.A);
                lsr_a(1);
                u8 half = (u8)g_cpu.A;
                lda8(half & 0x07); tax();
                lda8(bus8(0xCEF03E + (u32)g_cpu.X)); dp8_set(0x75, (u8)g_cpu.A);
                lda8(half); lsr_a(3);
                g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x73))); tax();
                lda8(bus8(0xCFE200 + (u32)g_cpu.X) & dp8(0x75));
                dp8_set(0x75, g_cpu._flag_Z ? 0x00 : 0x04);
                if (!g_cpu._flag_Z) lda8(0x04);
                iny();
                lda8((u8)((t & 0xC0) ^ dp8(0x70) | dp8(0x71) | dp8(0x75)));
                bus8_set(lp(0x57) + g_cpu.Y, (u8)g_cpu.A);
            }
            dp8_set(0x65, (u8)(dp8(0x65) + 1));
            lda8(dp8(0x65)); dc_cmp8((u8)g_cpu.A, 0x10);
            if (!g_cpu._flag_Z) continue;
            dp8_set(0x65, 0);
            dp8_set(0x66, (u8)(dp8(0x66) + 1));
            n = (u8)(dp8(0x6D) - 1); dp8_set(0x6D, n);
            if (n == 0) break;
        }
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x57), 0x0200)); dp16_set(0x57, g_cpu.A);
        abs16_set(0xC9FE, (u16)(abs16(0xC9FE) + 1));
        tdc();
        dc_set_mx(1, 0);
        dec_abs(0xC9FC);
    } while (!g_cpu._flag_Z);
}
/* CD:13EE  CD:13F6 (Y kept), then the tile mirrored through C0:FD00 (CD:144A). */
DC_ENTRY(CD13EE, RTS)
{
    dc_push16(g_cpu.Y);
    dc_CD13F6_glue();
    ldy(dc_pull16());
    bitrev_tile();
}
/* CD:06B4  Scroll table builder by mode CA92: CD:06C4 / 076C / 07E2 / 07B9. */
DC_ENTRY(CD06B4, RTS)
{
    static const glue_fn t[4] = { dc_CD06C4_glue, dc_CD076C_glue, dc_CD07E2_glue, dc_CD07B9_glue };
    lda8(abs8(0xCA92)); asl_a(1); tax();
    jump_table(t, 4, 0x06BC, 0xCD06B9);
}
/* Offsets of the corners at angles CD08 - 40 / CD08 + 40: dp49 / dp4B (CD:3BFD / 3C00 of
 * the first), dp45 / dp47 (of the second); 8-bit X / Y. */
static void corner_angles(void)
{
    lda8(abs8(0x5D8E)); tay();
    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0xCD08), 0x40)); dp8_set(0x45, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x80)); dp8_set(0x47, (u8)g_cpu.A);
    dc_set_mx(1, 1);
    lda8(dp8(0x45)); dc_CD3BFD_glue(); dp8_set(0x49, (u8)g_cpu.A);
    lda8(dp8(0x45)); dc_CD3C00_glue(); dp8_set(0x4B, (u8)g_cpu.A);
    lda8(dp8(0x47)); dc_CD3BFD_glue(); dp8_set(0x45, (u8)g_cpu.A);
    lda8(dp8(0x47)); dc_CD3C00_glue(); dp8_set(0x47, (u8)g_cpu.A);
}
/* CD:1B1F slot 4, then the corner offsets. */
static void corner_offsets(void) { ldx(0x0004); dc_CD1B1F_glue(); corner_angles(); }
/* CD:1937  Triangle from point op 1 (5DA0 table, 4 bytes each: apex x/y, base x/y): CD:1B1F
 * slot 4; the base corners sit at angles CD08 - 40 and CD08 + 40 (CD:3BFD / 3C00 offsets);
 * blend off (CC64 = dp20 = 0), drawn by CD:367C. */
DC_ENTRY(CD1937, RTS)
{
    corner_offsets();
    ldy(0x01);
    lda8(bus8(lp(0x40) + g_cpu.Y)); asl_a(2); tay();
    lda8(bus8(lp(0x40))); tax();
    static const u8 off[4] = { 0x49, 0x4B, 0x45, 0x47 };
    for (int k = 0; k < 4; k++) {
        g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x5DA2 + (k & 1) + g_cpu.Y), dp8(off[k])));
        dp8_set((u8)(0x12 + k), (u8)g_cpu.A);
    }
    lda8(abs8(0x5DA0 + g_cpu.Y)); dp8_set(0x16, (u8)g_cpu.A);
    lda8(abs8(0x5DA1 + g_cpu.Y)); dp8_set(0x17, (u8)g_cpu.A);
    dc_set_mx(1, 0);
    abs8_set(0xCC64, 0); abs8_set(0x0020, 0);
    dc_CD367C_glue();
    skip1();
}
/* Signed product of y and sin (cos: cos) of ang, through CD:3C58 / 3C55; carry stays set when
 * the result was negated. */
static void rot_term(u8 y, u8 ang, bool cos)
{
    ldy(y); lda8(ang);
    if (cos) dc_CD3C55_glue(); else dc_CD3C58_glue();
    bool neg = g_cpu._flag_C;
    lda8(bus8(0x004217));
    if (neg) lda8((u8)((u8)~g_cpu.A + 1));
}
static u8 rot_sum(u8 y, u8 ang, bool cos)
{
    rot_term(y, ang, cos);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x47)));
    return (u8)g_cpu.A;
}
static u8 rot_diff(u8 y, u8 ang, bool cos)
{
    rot_term(y, ang, cos);
    g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x47)));
    return (u8)g_cpu.A;
}
/* CD:3C8F  Rotate the point (CB06, CB08, CB0A) into (CAFE, CB00, CB02) by the angles dp4B
 * (about the middle axis), dp49 (about the first) and dp4D (about the last). */
DC_ENTRY(CD3C8F, RTS)
{
    rot_term(abs8(0xCB06), dp8(0x4B), true); dp8_set(0x47, (u8)g_cpu.A);
    abs8_set(0xCAFE, rot_sum(abs8(0xCB0A), dp8(0x4B), false));
    lda8(abs8(0xCB08)); abs8_set(0xCB00, (u8)g_cpu.A);
    rot_term((u8)(~abs8(0xCB06) + 1), dp8(0x4B), false); dp8_set(0x47, (u8)g_cpu.A);
    abs8_set(0xCB02, rot_sum(abs8(0xCB0A), dp8(0x4B), true));
    rot_term(abs8(0xCB02), dp8(0x49), false); dp8_set(0x47, (u8)g_cpu.A);
    u8 y = abs8(0xCB00);
    abs8_set(0xCB00, rot_diff(y, dp8(0x49), true));
    rot_term(y, dp8(0x49), false); dp8_set(0x47, (u8)g_cpu.A);
    abs8_set(0xCB02, rot_sum(abs8(0xCB02), dp8(0x49), true));
    rot_term(abs8(0xCAFE), dp8(0x4D), false); dp8_set(0x47, (u8)g_cpu.A);
    y = abs8(0xCB00);
    abs8_set(0xCB00, rot_sum(y, dp8(0x4D), true));
    rot_term(y, dp8(0x4D), false); dp8_set(0x47, (u8)g_cpu.A);
    abs8_set(0xCAFE, rot_diff(abs8(0xCAFE), dp8(0x4D), true));
}
/* After A = a - b (carry = no borrow): A = b + (a - b) / 2, halving toward zero. */
static void mid_after_sbc(u16 b)
{
    if (!g_cpu._flag_C) { lda8((u8)((u8)~g_cpu.A + 1)); lsr_a(1); lda8((u8)((u8)~g_cpu.A + 1)); }
    else lsr_a(1);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(b + g_cpu.X)));
}
/* dp[o] += 4 when dp[o] - dp[o + 4] is within -4..3. */
static void nudge_apart(u8 o)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(o), dp8((u8)(o + 4))));
    u8 d = (u8)g_cpu.A;
    dc_cmp8(d, 0xFC);
    if (!g_cpu._flag_C) dc_cmp8(d, 0x04);
    if (d >= 0xFC || d < 0x04) { g_cpu._flag_C = 0; lda8(dc_adc8(dp8(o), 0x04)); dp8_set(o, (u8)g_cpu.A); }
}
/* Order the point pairs at dp o and dp o + 2 by their first byte (16-bit swap). */
static void sort_pair(u8 o)
{
    lda8(dp8(o)); dc_cmp8((u8)g_cpu.A, dp8((u8)(o + 2)));
    if (g_cpu._flag_C) { ldx(dp16(o)); ldy(dp16((u8)(o + 2))); dp16_set(o, g_cpu.Y); dp16_set((u8)(o + 2), g_cpu.X); }
}
/* dp25 = |slope|; dp23 = x: dp27..29 = x * slope (CD:3BD4), negated for a negative slope. */
static void slope_mul(u16 slope)
{
    lda16(slope);
    bool neg = g_cpu._flag_N;
    if (neg) lda16((u16)(~g_cpu.A + 1));
    dp16_set(0x25, g_cpu.A);
    dc_set_mx(1, 0); tdc(); dc_CD3BD4_glue(); dc_set_mx(0, 0);
    if (neg) { lda16((u16)(~dp16(0x28) + 1)); dp16_set(0x28, g_cpu.A); }
}
/* CD:2208  (Entered with A = CAEA,X and the carry for the SBC.) Two edges of the slot X
 * quad: P = (dp12, dp13) .. (dp14, dp15) and Q = (dp16, dp17) .. (dp18, dp19), the Q start
 * being the midpoint of CAEA/CAF2 and CAEC/CAF4. Their slopes (CD:38E5) give dp45 / dp47 and
 * intercepts dp49 / dp4B; the crossing point goes to CAD4/CAD6 of slot op 0. */
static void edge_cross(void)
{
    lda8(dc_sbc8((u8)g_cpu.A, abs8(0xCAF2 + g_cpu.X))); mid_after_sbc(0xCAF2); dp8_set(0x16, (u8)g_cpu.A);
    lda8(abs8(0xCAEC + g_cpu.X));
    g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, abs8(0xCAF4 + g_cpu.X))); mid_after_sbc(0xCAF4); dp8_set(0x17, (u8)g_cpu.A);
    lda8(abs8(0xCAEE + g_cpu.X)); dp8_set(0x18, (u8)g_cpu.A);
    lda8(abs8(0xCAF0 + g_cpu.X)); dp8_set(0x19, (u8)g_cpu.A);
    dc_push8(g_cpu.DB);
    tdc(); dc_push8((u8)g_cpu.A); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    nudge_apart(0x12); nudge_apart(0x13);
    sort_pair(0x12); sort_pair(0x16);
    lda8(dp8(0x15)); dp8_set(0x0B, (u8)g_cpu.A);
    lda8(dp8(0x13)); dp8_set(0x0C, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0x14), dp8(0x12))); dp8_set(0x0D, (u8)g_cpu.A);
    dc_CD38E5_glue(); dp16_set(0x45, g_cpu.Y);
    lda8(dp8(0x19)); dp8_set(0x0B, (u8)g_cpu.A);
    lda8(dp8(0x17)); dp8_set(0x0C, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0x18), dp8(0x16))); dp8_set(0x0D, (u8)g_cpu.A);
    dc_CD38E5_glue(); dp16_set(0x47, g_cpu.Y);
    lda8(dp8(0x12)); dp8_set(0x23, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    slope_mul(dp16(0x45));
    g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x13) & 0x00FF, dp16(0x28))); dp16_set(0x49, g_cpu.A);
    tdc(); dc_set_mx(1, 0);
    lda8(dp8(0x16)); dp8_set(0x23, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    slope_mul(dp16(0x47));
    g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x17) & 0x00FF, dp16(0x28))); dp16_set(0x4B, g_cpu.A);
    /* x = (b_Q - b_P) / (s_P - s_Q) via CD:238D / 3BAB, sign by hand. */
    g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x4B), dp16(0x49)));
    bool dneg = g_cpu._flag_N;
    if (dneg) lda16((u16)(~g_cpu.A + 1));
    dp16_set(0x23, g_cpu.A);
    g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x45), dp16(0x47)));
    bool sneg = g_cpu._flag_N;
    if (sneg) lda16((u16)(~g_cpu.A + 1));
    dp16_set(0x25, g_cpu.A);
    dc_CD238D_glue(); dc_CD3BAB_glue();
    if (dneg != sneg) { lda16((u16)(~dp16(0x27) + 1)); dp16_set(0x27, g_cpu.A); }
    tdc(); dc_set_mx(1, 0);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    lda8(bus8(lp(0x40))); tax();
    lda8(dp8(0x27)); abs8_set(0xCAD4 + g_cpu.X, (u8)g_cpu.A);
    phx();
    dp8_set(0x23, (u8)g_cpu.A);
    ldx(dp16(0x45)); stx_dp(0x25);
    dc_CD3BD4_glue();
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x28), dp16(0x49))); dp16_set(0x28, g_cpu.A);
    tdc(); dc_set_mx(1, 0);
    plx();
    lda8(dp8(0x28)); abs8_set(0xCAD6 + g_cpu.X, (u8)g_cpu.A);
}
DC_ENTRY(CD2208, RTS) { edge_cross(); }
/* CD:21C0  Slot op 0: CAFC = FF; P starts at the midpoint of CAEA/CAEE, CAEC/CAF0 and ends at
 * CAF2/CAF4; then CD:2208. */
DC_ENTRY(CD21C0, RTS)
{
    lda8(bus8(lp(0x40))); tax();
    lda8(0xFF); abs8_set(0xCAFC + g_cpu.X, 0xFF);
    lda8(abs8(0xCAEA + g_cpu.X));
    g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, abs8(0xCAEE + g_cpu.X))); mid_after_sbc(0xCAEE); dp8_set(0x12, (u8)g_cpu.A);
    lda8(abs8(0xCAEC + g_cpu.X));
    g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, abs8(0xCAF0 + g_cpu.X))); mid_after_sbc(0xCAF0); dp8_set(0x13, (u8)g_cpu.A);
    lda8(abs8(0xCAF2 + g_cpu.X)); dp8_set(0x14, (u8)g_cpu.A);
    lda8(abs8(0xCAF4 + g_cpu.X)); dp8_set(0x15, (u8)g_cpu.A);
    lda8(abs8(0xCAEA + g_cpu.X));
    g_cpu._flag_C = 1;
    edge_cross();
}
/* CD:1FA0  CD:21C0, then the slot dp43 camera CA5A/CA5C = -(crossing point CAD4/CAD6 of op 0). */
DC_ENTRY(CD1FA0, RTS)
{
    dc_CD21C0_glue();
    lda8(bus8(lp(0x40))); tay();
    ldx(dp16(0x43));
    dc_set_mx(0, 0);
    lda16((u16)(~(abs16(0xCAD4 + g_cpu.Y) & 0x00FF) + 1)); abs16_set(0xCA5A + g_cpu.X, g_cpu.A);
    lda16((u16)(~(abs16(0xCAD6 + g_cpu.Y) & 0x00FF) + 1)); abs16_set(0xCA5C + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
}
static void sty_dp(u8 o) { if (g_cpu.x_flag) dp8_set(o, (u8)g_cpu.Y); else dp16_set(o, g_cpu.Y); }
/* Rotation angles dp49/4B/4D = CAF6/CAF8/CAFA and blend dp21 = CAFC of slot X (`via_y`: stored
 * through Y, so a 16-bit Y also writes B into the byte after). */
static void slot_angles(bool via_y)
{
    static const u16 src[4] = { 0xCAF6, 0xCAF8, 0xCAFA, 0xCAFC };
    static const u8 dst[4] = { 0x49, 0x4B, 0x4D, 0x21 };
    for (int k = 0; k < 4; k++) {
        lda8(abs8(src[k] + g_cpu.X));
        if (via_y) { tay(); sty_dp(dst[k]); } else dp8_set(dst[k], (u8)g_cpu.A);
    }
}
/* CD:28A1  Point (A = angle, slot X): CB06/CB08 = CD:3D90 sin/cos(A), CB0A = CAD8; rotated by
 * the slot's angles (CD:3C8F). */
DC_ENTRY(CD28A1, RTS)
{
    phx();
    dc_push8((u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x40));
    dc_CD3D90_glue(); abs8_set(0xCB08, (u8)g_cpu.A);
    lda8(dc_pull8());
    dc_CD3D90_glue(); abs8_set(0xCB06, (u8)g_cpu.A);
    lda8(abs8(0xCAD8 + g_cpu.X)); abs8_set(0xCB0A, (u8)g_cpu.A);
    slot_angles(false);
    dc_CD3C8F_glue();
    plx();
}
/* CD:1EF7  Point A of model dp4F (D1:EC5F, 4 bytes each): CB06/CB08 = x/y, CB0A = z + dp51;
 * rotated by slot X's angles (CD:3C8F). */
DC_ENTRY(CD1EF7, RTS)
{
    phx();
    asl_a(2); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x4F))); tax();
    lda8(bus8(0xD1EC5F + (u32)g_cpu.X)); tay(); sty_abs(0xCB06);
    lda8(bus8(0xD1EC60 + (u32)g_cpu.X)); tay(); sty_abs(0xCB08);
    g_cpu._flag_C = 0; lda8(dc_adc8(bus8(0xD1EC61 + (u32)g_cpu.X), dp8(0x51))); tay(); sty_abs(0xCB0A);
    plx(); phx();
    slot_angles(true);
    dc_set_mx(1, 1);
    dc_CD3C8F_glue();
    dc_set_mx(1, 0);
    plx();
}
/* CD:1368  Draw dp67 tiles of tile row X (entries at CF:A000 + X * 64, bank CF graphics at
 * dp57..59) into 2D00,Y: palette mask dp5F = CD:1300[CD:13CA of CC9D/CC9E (from CF:E000[X])
 * per group of 8], then by the entry's flip bits 14-13 CD:1509 / 1445 / 13F6 / 13EE. */
DC_ENTRY(CD1368, RTS)
{
    static const glue_fn t[4] = { dc_CD1509_glue, dc_CD1445_glue, dc_CD13F6_glue, dc_CD13EE_glue };
    lda8(0xCF); dp8_set(0x55, 0xCF); dp8_set(0x59, 0xCF);
    dc_set_mx(0, 0);
    lda16(g_cpu.X); asl_a(1); dp16_set(0x57, g_cpu.A);
    asl_a(5); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0xA000)); dp16_set(0x53, g_cpu.A);
    ldx(dp16(0x57));
    lda16(bus16(0xCFE000 + (u32)g_cpu.X)); abs16_set(0xCC9D, g_cpu.A); abs16_set(0xCC9F, g_cpu.A);
    tdc(); tay();
    dp16_set(0x5B, 0);
    do {
        tdc();
        dc_set_mx(1, 0);
        lda8(dp8(0x5B) & 0x18); lsr_a(3); tax();
        lda8(abs8(0xCC9D + g_cpu.X)); dp8_set(0x5D, (u8)g_cpu.A);
        dc_CD13CA_glue();
        tax();
        dc_set_mx(0, 0);
        lda16(bus16(0xCD1300 + (u32)g_cpu.X)); dp16_set(0x5F, g_cpu.A);
        lda16(bus16(lp(0x53))); xba(); lsr_a(5); lda16(g_cpu.A & 0x0006); tax();
        jump_table(t, 4, 0x13E6, 0xCD13B9);
        dp16_set(0x5B, (u16)(dp16(0x5B) + 1));
        dp16_set(0x53, (u16)(dp16(0x53) + 2));
        u16 n = (u16)(dp16(0x67) - 1); dp16_set(0x67, n); dc_set_flags_nz16(n);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 0);
}
/* ---- Triangle rasterizer (CD:3685 / 3952): vertices (x, y) = dp12/13, dp14/15, dp16/17;
 * left/right edge x in 8.8 go out through the WRAM port (STA $217F writes the high byte to
 * 2180), four bytes per line, the last two skipped by reading 2180. ---- */
static void swap16_dp(u8 a, u8 b) { ldx(dp16(a)); ldy(dp16(b)); dp16_set(b, g_cpu.X); dp16_set(a, g_cpu.Y); }
/* Swap vertex a and b when y(a) >= y(b). */
static void sort_y(u8 a, u8 b)
{
    lda8(dp8((u8)(a + 1))); dc_cmp8((u8)g_cpu.A, dp8((u8)(b + 1)));
    if (g_cpu._flag_C) swap16_dp(a, b);
}
/* dp0D = dp[hi] - dp[lo] (rows), dp0B / dp0C = end / start x: CD:38E5 slope in Y (A = sign). */
static void tri_slope(u8 hi, u8 lo, u8 x1, u8 x0)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(hi), dp8(lo))); dp8_set(0x0D, (u8)g_cpu.A);
    lda8(dp8(x1)); dp8_set(0x0B, (u8)g_cpu.A);
    lda8(dp8(x0)); dp8_set(0x0C, (u8)g_cpu.A);
    dc_CD38E5_glue();
}
/* X = Y = dp12 << 8 (A's high byte as the fraction). */
static void tri_start_apex(void)
{
    lda8(dp8(0x12));
    dc_set_mx(0, 0); xba(); tax(); tay(); tdc(); dc_set_mx(1, 0);
}
/* dp[hi] - dp[lo] lines stepping X by dp[ax] and Y by dp[ay] (carry runs on between adds). */
static void tri_span(u8 hi, u8 lo, u8 ax, u8 ay)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(hi), dp8(lo)));
    if (g_cpu._flag_Z) return;
    dp8_set(0x01, (u8)g_cpu.A); dp8_set(0x02, 0);
    dc_set_mx(0, 0); g_cpu._flag_C = 0;
    do {
        lda16(dc_adc16(g_cpu.X, dp16(ax))); tax(); abs16_set(0x217F, g_cpu.A);
        lda16(dc_adc16(g_cpu.Y, dp16(ay))); tay(); abs16_set(0x217F, g_cpu.A);
        lda16(abs16(0x217F)); lda16(abs16(0x217F));
        u16 n = (u16)(dp16(0x01) - 1); dp16_set(0x01, n); dc_set_flags_nz16(n);
    } while (!g_cpu._flag_Z);
    tdc(); dc_set_mx(1, 0);
}
/* Port setup for line dp0F, then the upper part (to dp11) and the lower part (to dp10). */
static void tri_raster(glue_fn setup, u8 ax2, u8 ay2)
{
    lda8(dp8(0x0F)); setup();
    tri_span(0x11, 0x0F, 0x03, 0x05);
    tri_span(0x10, 0x11, ax2, ay2);
}
/* `full`: CD:3685 (second edge may run upward; port via CD:38B6); else CD:3952's tail (CD:3B10). */
static void triangle(bool full)
{
    glue_fn setup = full ? dc_CD38B6_glue : dc_CD3B10_glue;
    sort_y(0x12, 0x14); sort_y(0x14, 0x16); sort_y(0x12, 0x14);
    lda8(dp8(0x13)); dc_cmp8((u8)g_cpu.A, dp8(0x15));
    if (g_cpu._flag_Z) {
        /* Flat top: 12 and 14 share the top line, both edges run to 16. */
        lda8(dp8(0x12)); dc_cmp8((u8)g_cpu.A, dp8(0x14));
        if (g_cpu._flag_C) swap16_dp(0x12, 0x14);
        tri_slope(0x17, 0x13, 0x16, 0x12); dp16_set(0x03, g_cpu.Y);
        tri_slope(0x17, 0x15, 0x16, 0x14); dp16_set(0x05, g_cpu.Y);
        lda8(dp8(0x13)); dp8_set(0x0F, (u8)g_cpu.A);
        lda8(dp8(0x17)); dp8_set(0x11, (u8)g_cpu.A); dp8_set(0x10, (u8)g_cpu.A);
        dc_set_mx(0, 0);
        lda16(dp16(0x12) & 0x00FF); xba(); tax();
        lda16(dp16(0x14) & 0x00FF); xba(); tay();
        tdc(); dc_set_mx(1, 0);
        tri_raster(setup, 0x07, 0x05);
        return;
    }
    lda8(dp8(0x14)); dc_cmp8((u8)g_cpu.A, dp8(0x16));
    if (g_cpu._flag_C) swap16_dp(0x14, 0x16);
    lda8(dp8(0x15)); dc_cmp8((u8)g_cpu.A, dp8(0x17));
    if (g_cpu._flag_Z) {
        /* Flat bottom: both edges from 12, ending at 14 / 16 on the same line. */
        tri_slope(0x15, 0x13, 0x14, 0x12); dp16_set(0x03, g_cpu.Y);
        tri_slope(0x17, 0x13, 0x16, 0x12); dp16_set(0x05, g_cpu.Y);
        lda8(dp8(0x13)); dp8_set(0x0F, (u8)g_cpu.A);
        lda8(dp8(0x17)); dp8_set(0x10, (u8)g_cpu.A); dp8_set(0x11, (u8)g_cpu.A);
        tri_start_apex();
        tri_raster(setup, 0x07, 0x05);
        return;
    }
    lda8(dp8(0x13)); dp8_set(0x0F, (u8)g_cpu.A);
    tri_slope(0x15, 0x13, 0x14, 0x12); dp8_set(0x00, (u8)g_cpu.A); dp16_set(0x03, g_cpu.Y);
    tri_slope(0x17, 0x13, 0x16, 0x12); dp16_set(0x05, g_cpu.Y);
    dc_cmp8((u8)g_cpu.A, dp8(0x00));
    if (g_cpu._flag_Z) {
        ldy(dp16(0x03)); cpy(dp16(0x05));
        if (g_cpu._flag_C) { swap16_dp(0x14, 0x16); swap16_dp(0x03, 0x05); }
    }
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0x17), dp8(0x15)));
    if (!full || g_cpu._flag_C) {
        tri_slope(0x17, 0x15, 0x16, 0x14); dp16_set(0x07, g_cpu.Y);
        lda8(dp8(0x15)); dp8_set(0x11, (u8)g_cpu.A);
        lda8(dp8(0x17)); dp8_set(0x10, (u8)g_cpu.A);
        tri_start_apex();
        tri_raster(setup, 0x07, 0x05);
    } else {
        lda8((u8)((u8)~g_cpu.A + 1)); dp8_set(0x0D, (u8)g_cpu.A);
        lda8(dp8(0x14)); dp8_set(0x0B, (u8)g_cpu.A);
        lda8(dp8(0x16)); dp8_set(0x0C, (u8)g_cpu.A);
        dc_CD38E5_glue(); dp16_set(0x07, g_cpu.Y);
        lda8(dp8(0x17)); dp8_set(0x11, (u8)g_cpu.A);
        lda8(dp8(0x15)); dp8_set(0x10, (u8)g_cpu.A);
        tri_start_apex();
        tri_raster(setup, 0x03, 0x07);
    }
}
/* CD:3685  Filled triangle into the window line table. */
DC_ENTRY(CD3685, RTS) { triangle(true); }
/* CD:3952  Right triangle from two points (the corner takes the lower point's y and the other
 * point's x), rasterized like CD:3685 but through CD:3B10. */
DC_ENTRY(CD3952, RTS)
{
    lda8(dp8(0x13)); dc_cmp8((u8)g_cpu.A, dp8(0x15));
    if (g_cpu._flag_C) { lda8(dp8(0x15)); dp8_set(0x17, (u8)g_cpu.A); lda8(dp8(0x12)); dp8_set(0x16, (u8)g_cpu.A); }
    else { lda8(dp8(0x13)); dp8_set(0x17, (u8)g_cpu.A); lda8(dp8(0x14)); dp8_set(0x16, (u8)g_cpu.A); }
    triangle(false);
}
/* Slot X (A) as the triangle: CC64 = A, dp20 = 0, dp12..17 = CAEA..CAF4. */
static void load_tri(void)
{
    tax(); abs8_set(0xCC64, (u8)g_cpu.A); abs8_set(0x0020, 0);
    for (int k = 0; k < 6; k++) { lda8(abs8(0xCAEA + 2 * k + g_cpu.X)); dp8_set((u8)(0x12 + k), (u8)g_cpu.A); }
}
/* CD:3657  Draw slot A's triangle (CD:367C). */
DC_ENTRY(CD3657, RTS) { load_tri(); dc_CD367C_glue(); }
/* CD:3924  Draw slot A's right triangle with DB = 00 (CD:3952). */
DC_ENTRY(CD3924, RTS)
{
    load_tri();
    dc_push8(g_cpu.DB);
    tdc(); dc_push8((u8)g_cpu.A); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    dc_CD3952_glue();
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}
/* Screen point = rotated point CAFE/CB00 + slot centre CAD4/CAD6 into CAEA + o / CAEC + o. */
static void place_point(u16 o)
{
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCAFE), abs8(0xCAD4 + g_cpu.X))); abs8_set(0xCAEA + o + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCB00), abs8(0xCAD6 + g_cpu.X))); abs8_set(0xCAEC + o + g_cpu.X, (u8)g_cpu.A);
}
/* CD:1EAA  Model dp4F's three points (CD:1EF7) placed for slot X, then CD:2173. */
static void model_tri(void)
{
    for (int k = 0; k < 3; k++) {
        if (k == 0) tdc(); else lda8((u8)k);
        dc_CD1EF7_glue();
        place_point((u16)(4 * k));
    }
    dc_CD2173_glue();
}
/* CD:1E27  Slot op 0: model 2 (dp4F = 20) at depth CAD8. */
DC_ENTRY(CD1E27, RTS)
{
    lda8(bus8(lp(0x40))); tax();
    lda8(0x20); dp8_set(0x4F, 0x20);
    lda8(abs8(0xCAD8 + g_cpu.X)); dp8_set(0x51, (u8)g_cpu.A);
    model_tri();
}
/* CD:1E7C  Slot op 0 & 1: model op 0 (dp4F = op * 16) at depth CAD8. */
DC_ENTRY(CD1E7C, RTS)
{
    lda8(bus8(lp(0x40)) & 0x01); tax();
    lda8(bus8(lp(0x40))); asl_a(4); dp8_set(0x4F, (u8)g_cpu.A);
    lda8(abs8(0xCAD8 + g_cpu.X)); dp8_set(0x51, (u8)g_cpu.A);
    model_tri();
}
/* CD:1E91  Slot op 0: model op 0 at depth 0, spinning (CAF8 += 1, CAF6 += 2, CAFA += 1). */
DC_ENTRY(CD1E91, RTS)
{
    lda8(bus8(lp(0x40))); tax();
    lda8(bus8(lp(0x40))); asl_a(4); dp8_set(0x4F, (u8)g_cpu.A);
    dp8_set(0x51, 0);
    inc_abs(0xCAF8 + g_cpu.X); inc_abs(0xCAF6 + g_cpu.X); inc_abs(0xCAF6 + g_cpu.X); inc_abs(0xCAFA + g_cpu.X);
    model_tri();
}
/* CD:2843  Slot X (8-bit index): the three orbit points (angle CAE4/E6/E8, radius
 * CADA/DC/DE; CD:28A1) placed into CAEA..CAF4. */
DC_ENTRY(CD2843, RTS)
{
    for (int k = 0; k < 3; k++) {
        lda8(abs8(0xCADA + 2 * k + g_cpu.X)); dp8_set(0x47, (u8)g_cpu.A);
        lda8(abs8(0xCAE4 + 2 * k + g_cpu.X));
        dc_CD28A1_glue();
        place_point((u16)(4 * k));
    }
}
/* CD:2952 / 1F47  Draw slot op 0's triangle (CD:3657) / right triangle (CD:3924). */
DC_ENTRY(CD2952, RTS) { lda8(bus8(lp(0x40))); dc_CD3657_glue(); }
DC_ENTRY(CD1F47, RTS) { lda8(bus8(lp(0x40))); dc_CD3924_glue(); }
/* CD:27CA  Slot op 0: add op 1..3 to the centre CAD4/CAD6/CAD8, op 4 to the radii
 * CADA/CADC/CADE, op 5..8 to the angles CAF6/CAF8/CAFA and blend CAFC; then CD:2843. */
DC_ENTRY(CD27CA, RTS)
{
    static const u16 dst[10] = { 0xCAD4, 0xCAD6, 0xCAD8, 0xCADA, 0xCADC, 0xCADE, 0xCAF6, 0xCAF8, 0xCAFA, 0xCAFC };
    static const u8 op[10] = { 1, 2, 3, 4, 4, 4, 5, 6, 7, 8 };
    add_ops(dst, op, 10, dc_CD2843_glue, 0x0008);
}

/* Script ops that call one bank D1 effect routine. */
DC_ENTRY(CD184B, RTS) { dc_jsl(0xD1EFD0, 0xCD184B); }
DC_ENTRY(CD1850, RTS) { dc_jsl(0xD1EDCD, 0xCD1850); }
DC_ENTRY(CD1855, RTS) { dc_jsl(0xD1EEC4, 0xCD1855); }
DC_ENTRY(CD185A, RTS) { dc_jsl(0xD1EEA5, 0xCD185A); }
DC_ENTRY(CD185F, RTS) { dc_jsl(0xD1EFE3, 0xCD185F); }
DC_ENTRY(CD1874, RTS) { dc_jsl(0xD1FB72, 0xCD1874); }
DC_ENTRY(CD1879, RTS) { dc_jsl(0xD1FB68, 0xCD1879); }
DC_ENTRY(CD1913, RTS) { dc_jsl(0xD1F47C, 0xCD1913); }
DC_ENTRY(CD191E, RTS) { dc_jsl(0xD1F5CD, 0xCD191E); }
DC_ENTRY(CD1932, RTS) { dc_jsl(0xD1F5F5, 0xCD1932); }
DC_ENTRY(CD1AE1, RTS) { dc_jsl(0xD1FD7E, 0xCD1AE1); }
DC_ENTRY(CD1AE6, RTS) { dc_jsl(0xD1F559, 0xCD1AE6); }
DC_ENTRY(CD1B9A, RTS) { dc_jsl(0xD1E70A, 0xCD1B9A); }
DC_ENTRY(CD2A97, RTS) { dc_jsl(0xD1E8C1, 0xCD2A97); }
DC_ENTRY(CD2A9C, RTS) { dc_jsl(0xD1E899, 0xCD2A9C); }
DC_ENTRY(CD2AA5, RTS) { dc_jsl(0xD1E91A, 0xCD2AA5); }
DC_ENTRY(CD2AAA, RTS) { dc_jsl(0xD1E984, 0xCD2AAA); }
DC_ENTRY(CD2B64, RTS) { dc_jsl(0xD1F108, 0xCD2B64); }
/* CD:29DA  Window fill op 0 (D1:FBCE). */
DC_ENTRY(CD29DA, RTS) { lda8(bus8(lp(0x40))); asl_a(1); tax(); dc_jsl(0xD1FBCE, 0xCD29DE); }
/* CD:2A4A  Window / screen effect op picked by op 0 (29 entries). */
DC_ENTRY(CD2A4A, RTS)
{
    static const glue_fn t[29] = {
        dc_CD2D4A_glue, dc_CD2D1F_glue, dc_CD2CF5_glue, dc_CD2D0C_glue,
        dc_CD2CDC_glue, dc_CD2CB3_glue, dc_CD2C98_glue, dc_CD2C8B_glue,
        dc_CD2C7F_glue, dc_CD2BFC_glue, dc_CD2BD2_glue, dc_CD2BA8_glue,
        dc_CD2B69_glue, dc_CD2B64_glue, dc_CD2D44_glue, dc_CD2B5C_glue,
        dc_CD2B45_glue, dc_CD2B27_glue, dc_CD2B12_glue, dc_CD2D39_glue,
        dc_CD2B05_glue, dc_CD2AEF_glue, dc_CD2AC6_glue, dc_CD2AAA_glue,
        dc_CD2AA5_glue, dc_CD2AA1_glue, dc_CD2A9C_glue, dc_CD2A8B_glue,
        dc_CD2A97_glue
    };
    lda8(bus8(lp(0x40))); asl_a(1); tax();
    jump_table(t, 29, 0x2A51, 0xCD2A4E);
}
/* CD:001B  Long entry: effect setup D1:F67A. */
DC_ENTRY(CD001B, RTL) { dc_jsl(0xD1F67A, 0xCD003E); }
/* CD:0607  Per-frame VRAM upload. When CA17 is set: graphics set CA16 (size CD:05FF[CA16]) from
 * 7E:CA18 by D1:E8D9 unless it is the one already loaded (CD1F / CD20); A01F += 1. Then
 * CE14 += 1 and DMA 0x200 bytes to VRAM 5800 / 5980 (by CE14 parity): from 7E:B400 / B600
 * when CCEC = 10 (CD:3E8E), else the fixed CD:06B2 block with a fixed source (DMA mode 09). */
DC_ENTRY(CD0607, RTS)
{
    lda8(abs8(0xCA17));
    if (!g_cpu._flag_Z) {
        abs8_set(0xCA17, 0);
        lda8(abs8(0xCA16)); asl_a(1);
        u8 set = (u8)g_cpu.A;
        abs8_set(0xCA16, 0);
        tax();
        dc_set_mx(0, 0);
        lda16(bus16(0xCD05FF + (u32)g_cpu.X)); dp16_set(0x45, g_cpu.A);
        tdc();
        dc_set_mx(1, 0);
        lda8(set);
        dc_cmp8(set, abs8(0xCD1F));
        bool load = true;
        if (!g_cpu._flag_Z) {
            abs8_set(0xCD1F, set);
            ldx(abs16(0xCA18)); abs16_set(0xCD20, g_cpu.X);
        } else {
            ldx(abs16(0xCA18)); cpx(abs16(0xCD20));
            if (g_cpu._flag_Z) load = false; else abs16_set(0xCD20, g_cpu.X);
        }
        if (load) {
            ldx(abs16(0xCA18));
            lda8(0x7E);
            dc_jsl(0xD1E8D9, 0xCD0644);
            inc_abs(0xA01F);
        }
    }
    ldx(0x0200); stx_dp(0x45);
    inc_abs(0xCE14);
    lda8(abs8(0xCCEC)); dc_cmp8((u8)g_cpu.A, 0x10);
    bool odd;
    if (g_cpu._flag_Z) {
        lda8(bus8(0x7ECE14) & 0x01); odd = !g_cpu._flag_Z;
        ldx(odd ? 0xB400 : 0xB600); ldy(odd ? 0x5800 : 0x5980);
        lda8(0x7E);
        dc_CD3E8E_glue();
        return;
    }
    dc_push8(g_cpu.DB);
    lda8(0x00); dc_push8(0x00); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    lda8(bus8(0x7ECE14) & 0x01); odd = !g_cpu._flag_Z;
    ldx(odd ? 0x5800 : 0x5980); stx_abs(0x2116);
    lda8(0x09); abs8_set(0x4370, 0x09);
    lda8(0x18); abs8_set(0x4371, 0x18);
    ldx(0x06B2); stx_abs(0x4372);
    lda8(0xCD); abs8_set(0x4374, 0xCD);
    ldx(0x0200); stx_abs(0x4375);
    lda8(0x80); abs8_set(0x420B, 0x80);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}
/* Quad op 0 from point pair op 1 (5DA0 + 4n) and the corner offsets (as CD:1937), drawn by
 * D1:F9AF; skips the second operand (CD:2466). */
static void rotated_quad(void)
{
    corner_angles();
    ldy(0x01); lda8(bus8(lp(0x40) + 1)); asl_a(2); tay();
    lda8(bus8(lp(0x40))); tax();
    static const u8 src[8] = { 0, 1, 0, 1, 2, 3, 2, 3 }, add[8] = { 0x49, 0x4B, 0x45, 0x47, 0x49, 0x4B, 0x45, 0x47 };
    for (int k = 0; k < 8; k++) {
        lda8(abs8(0x5DA0 + src[k] + g_cpu.Y)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(add[k])));
        abs8_set(0xCD0D + 2 * k + g_cpu.X, (u8)g_cpu.A);
    }
    dc_set_mx(1, 0);
    dc_jsl(0xD1F9AF, 0xCD24E9);
    skip1();
}
/* CD:2460  Rotated quad with CD:1B1F slot 4. */
DC_ENTRY(CD2460, RTS) { ldx(0x0004); dc_CD1B1F_glue(); rotated_quad(); }
/* CD:19B1  Rotated quad with the clamp rectangle of slot op 1 (CD:2433, 1B22). */
DC_ENTRY(CD19B1, RTS)
{
    ldy(0x0001); lda8(bus8(lp(0x40) + 1)); asl_a(2); tax();
    dc_CD2433_glue(); dc_CD1B22_glue();
    rotated_quad();
}
/* CD:1805  Sine quad op (D1:EBFF). */
DC_ENTRY(CD1805, RTS) { dc_jsl(0xD1EBFF, 0xCD1805); }

/* ---- odd entries left by the inventory ---- */
static void plb_pull(void) { g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB); }
/* CD:0C89  Calls C0:0002 (the C0 jump table entry to C0:2C41) with DB, D and P saved. */
DC_ENTRY(CD0C89, RTL)
{
    dc_push8(g_cpu.DB); dc_push16(g_cpu.D); php();
    dc_jsl(0xC00002, 0xCD0C8C);
    plp();
    g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    plb_pull();
    tdc();
}
/* CD:09F1  Battle frame update (entry 0 of the CD:0C83 table, run from D1 through
 * CD:0C95): CD:09CE, HDMA channels 0-6 from the 7E:C867.. lists, C1:000F when CAD3
 * is set, channel 7 (CD:0AEE), HDMA on (420C = FF), 2100 from 7E:BB00, then
 * CD:06C4 / 0B1E, D1:F426, FD:FFF7 and CD:0ADE. */
DC_ENTRY(CD09F1, RTS)
{
    static const struct { u8 mode, reg; u16 src; } ch[7] = {
        { 0x44, 0x07, 0xC867 }, { 0x43, 0x0D, 0xC894 }, { 0x43, 0x0F, 0xC8C1 }, { 0x43, 0x11, 0xC8EE },
        { 0x44, 0x2C, 0xC91B }, { 0x44, 0x30, 0xC948 }, { 0x41, 0x05, 0xC975 },
    };
    dc_CD09CE_glue();
    for (int k = 0; k < 7; k++) {
        u16 r = (u16)(0x4300 + 0x10 * k);
        lda8(ch[k].mode); abs8_set(r, ch[k].mode);
        lda8(ch[k].reg); abs8_set(r + 1, ch[k].reg);
        ldx(ch[k].src); stx_abs(r + 2);
        lda8(0x7E); abs8_set(r + 4, 0x7E);
        abs8_set(r + 7, 0x7E);
    }
    lda8(0x7E); dc_push8(0x7E); plb_pull();
    lda8(abs8(0xCAD3));
    if (!g_cpu._flag_Z) dc_jsl(0xC1000F, 0xCD0AB4);
    dc_push8(g_cpu.DB);
    tdc(); dc_push8((u8)g_cpu.A); plb_pull();
    dc_CD0AEE_glue();
    lda8(0xFF); abs8_set(0x420C, 0xFF);
    lda8(bus8(0x7EBB00)); abs8_set(0x2100, (u8)g_cpu.A);
    plb_pull();
    dc_CD06C4_glue();
    dc_CD0B1E_glue();
    dc_jsl(0xD1F426, 0xCD0AD2);
    dc_jsl(0xFDFFF7, 0xCD0AD6);
    dc_CD0ADE_glue();
}
/* CD:30BE  Command 97 (entry 23 of the CD:16B5 table): op2 bit 7 sets CA77 + slot (dp43, +1
 * when op2 bit 1) to op0; otherwise it adds op1, looks up the CE:F28E word for it
 * (CA9B, with 7080 in CA9D, the pair ordered by their high bytes), steps the axes
 * (CD:3134) into dp4F / dp51 and calls CE:EA7E with op2 & 7F. Skips 2 bytes. */
DC_ENTRY(CD30BE, RTS)
{
    tdc(); tax();
    ldx(dp_x(0x43));
    lda8(argn(2) & 0x02);
    if (!g_cpu._flag_Z) inx();
    lda8(argn(2));
    if (g_cpu._flag_N) {
        lda8(arg8()); abs8_set(0xCA77 + g_cpu.X, (u8)g_cpu.A);
    } else {
        lda8(abs8(0xCA77 + g_cpu.X));
        u8 op1 = argn(1);
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, op1));
        abs8_set(0xCA77 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xCAA3, (u8)g_cpu.A);
        dc_set_mx(0, g_cpu.x_flag);
        asl_a(1); tax();
        lda16(bus16(0xCEF28E + (u32)g_cpu.X)); abs16_set(0xCA9B, g_cpu.A);
        lda16(0x7080); abs16_set(0xCA9D, 0x7080);
        tdc();
        dc_set_mx(1, g_cpu.x_flag);
        lda8(abs8(0xCA9E));
        dc_cmp8((u8)g_cpu.A, abs8(0xCA9C));
        if (!g_cpu._flag_C) {
            ldx(abs_x(0xCA9B)); phx();
            ldx(abs_x(0xCA9D)); stx_abs(0xCA9B);
            plx(); stx_abs(0xCA9D);
        }
        dc_CD3134_glue();
        lda8(abs8(0xCA9B)); dp8_set(0x4F, (u8)g_cpu.A);
        lda8(abs8(0xCA9C)); dp8_set(0x51, (u8)g_cpu.A);
        lda8(argn(2) & 0x7F);
        dc_jsl(0xCEEA7E, 0xCD3129);
    }
    ldy(dp16(0x40)); iny(); iny(); dp16_set(0x40, g_cpu.Y);
}
