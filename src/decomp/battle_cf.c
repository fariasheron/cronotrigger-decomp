/* Bank CF: battle code at CF:E300-FE84 (the rest of the bank is data, see
 * tools/decomp/not_code_cf.txt). It runs with DB = 7E, D = 0000 and an
 * 8-bit accumulator; the per-frame battle update (CF:E58A, run by the
 * interpreter) calls most of it: sprite and effect movers, OAM builders,
 * HDMA channel presets. */
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
static void stx_dp(u8 o) { if (g_cpu.x_flag) dp8_set(o, (u8)g_cpu.X); else dp16_set(o, g_cpu.X); }
static void tdc(void) { g_cpu.A = g_cpu.D; dc_set_flags_nz16(g_cpu.D); }
/* TAX/TAY take B too when the index is 16-bit. */
static void tax(void) { ldx(g_cpu.A); }
static void tay(void) { ldy(g_cpu.A); }
static void php(void) { dc_sync_p(); dc_push8(g_cpu.P); }
static void plp(void)
{
    g_cpu.P = dc_pull8();
    cpu_p_to_mirrors(&g_cpu);
    if (g_cpu.x_flag) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
}
/* Loads, stores and adds of A in the current accumulator width. */
static u16 ldm(u32 a)
{
    if (g_cpu.m_flag) { lda8(abs8(a)); return (u8)g_cpu.A; }
    lda16(abs16(a)); return g_cpu.A;
}
static void stm(u32 a) { if (g_cpu.m_flag) abs8_set(a, (u8)g_cpu.A); else abs16_set(a, g_cpu.A); }
static void adcm(u32 a)
{
    if (g_cpu.m_flag) dc_set_a8(dc_adc8((u8)g_cpu.A, abs8(a)));
    else g_cpu.A = dc_adc16(g_cpu.A, abs16(a));
}
static void lsr_a(int n)
{
    for (int k = 0; k < n; k++) {
        if (g_cpu.m_flag) { u8 v = (u8)g_cpu.A; g_cpu._flag_C = v & 1; dc_set_a8(v >> 1); dc_set_flags_nz8(v >> 1); }
        else { g_cpu._flag_C = g_cpu.A & 1; g_cpu.A >>= 1; dc_set_flags_nz16(g_cpu.A); }
    }
}

/* ---- small helpers ---------------------------------------------------------- */
DC_ENTRY(CFF61E, RTS) {}
/* CF:FADD / CF:FCF6  A >> 4. */
DC_ENTRY(CFFADD, RTS) { lsr_a(4); }
DC_ENTRY(CFFCF6, RTS) { lsr_a(4); }

/* CF:F957  dp D0 = 1 when the points (CC, CD) and (CE, CF) are less than D1
 * apart on both axes, else 0. */
static bool near_axis(u8 a, u8 b)
{
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(a, b));
    if (g_cpu._flag_N) lda8((u8)(-(u8)g_cpu.A));
    dc_cmp8((u8)g_cpu.A, dp8(0xD1));
    return !g_cpu._flag_C;
}
DC_ENTRY(CFF957, RTS)
{
    DC_ASSUME_M(CFF957, 1);
    dp8_set(0xD0, 0);
    if (near_axis(dp8(0xCC), dp8(0xCE)) && near_axis(dp8(0xCD), dp8(0xCF))) {
        dp8_set(0xD0, 1); dc_set_flags_nz8(1);
    }
}

/* CF:FA17  dp 04..06 = dp00 * the 16-bit dp02 (hardware multiplier, DB = the low
 * byte of D for the duration); returns A = dp05, the product >> 8. */
static void mul_8x16(void)
{
    dc_push8(g_cpu.DB);
    tdc();
    g_cpu.DB = (u8)g_cpu.A;
    lda8(dp8(0x00)); abs8_set(0x4202, (u8)g_cpu.A);
    lda8(dp8(0x02)); abs8_set(0x4203, (u8)g_cpu.A);
    php();
    lda8(dp8(0x03));
    dp8_set(0x06, 0);
    ldx(abs_x(0x4216));
    abs8_set(0x4203, (u8)g_cpu.A);
    stx_dp(0x04);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0x05), abs16(0x4216)));
    dp16_set(0x05, g_cpu.A);
    tdc();
    plp();
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.DB = dc_pull8();
    lda8(dp8(0x05));
}
DC_ENTRY(CFFA17, RTS) { mul_8x16(); }

/* CF:F9FB  A = dp00 * sin(A) >> 8: the angle A (256 steps) indexes the quarter-wave
 * byte table at C0:F900 (steps of 4 bytes); the second half of the circle is
 * negative. Leaves the signed factor in dp02. */
DC_ENTRY(CFF9FB, RTS)
{
    DC_ASSUME_X(CFF9FB, 0);
    dc_set_mx(0, 0);
    g_cpu._flag_C = (g_cpu.A >> 14) & 1;
    u16 i = (u16)(g_cpu.A << 2) & 0x03FF;
    ldx(i);
    lda16(bus16(0xC0F900 + i) & 0x00FF);
    dc_cmp16(i, 0x0200);
    if (g_cpu._flag_C) lda16((u16)-g_cpu.A);
    dp16_set(0x02, g_cpu.A);
    dc_set_mx(1, 0);
    mul_8x16();
}

/* CF:F9D3  Effect sprites 0-7 that are on (96F8) follow the sprite A002: their
 * positions 1D0F / 1D26 = that sprite's plus the offsets AA64 / AA6C. */
DC_ENTRY(CFF9D3, RTS)
{
    ldm(0xA002);
    tay();
    ldx(0x0007);
    do {
        if (ldm(0x96F8 + g_cpu.X)) {
            g_cpu._flag_C = 0; ldm(0x1D0F + g_cpu.Y); adcm(0xAA64 + g_cpu.X); stm(0x1D0F + g_cpu.X);
            g_cpu._flag_C = 0; ldm(0x1D26 + g_cpu.Y); adcm(0xAA6C + g_cpu.X); stm(0x1D26 + g_cpu.X);
        }
        ldx((u16)(g_cpu.X - 1));
    } while (!g_cpu._flag_N);
}

/* ---- HDMA channel presets ---------------------------------------------------- */
/* CF:FD02 / FD36 / FD6A  Fill entry X = D (0) of the HDMA channel table (stride 6
 * from 9EEA: on, mode, table address and bank, ...), then dp A0 = 80. */
static void hdma_preset(const u8 v[9])
{
    tdc(); tax();
    for (int k = 0; k < 9; k++) { lda8(v[k]); abs8_set(0x9EEA + 6 * k + g_cpu.X, v[k]); }
    lda8(0x80); dp8_set(0xA0, 0x80);
}
DC_ENTRY(CFFD02, RTL) { static const u8 v[9] = { 0x01, 0x18, 0xC0, 0x0C, 0x00, 0x80, 0x01, 0x20, 0x78 }; hdma_preset(v); }
DC_ENTRY(CFFD36, RTL) { static const u8 v[9] = { 0x01, 0x18, 0xE1, 0xA6, 0x7E, 0x80, 0x01, 0x20, 0x78 }; hdma_preset(v); }
DC_ENTRY(CFFD6A, RTL) { static const u8 v[9] = { 0x01, 0x18, 0x80, 0x0E, 0x00, 0x80, 0x01, 0x20, 0x78 }; hdma_preset(v); }

/* CF:FD9E  dp A2 = the word CC:F8ED[A]. */
DC_ENTRY(CFFD9E, RTL)
{
    DC_ASSUME_X(CFFD9E, 0);
    dc_set_mx(0, 0);
    g_cpu._flag_C = g_cpu.A >> 15;
    ldx((u16)(g_cpu.A << 1));
    ldx(bus16(0xCCF8ED + g_cpu.X));
    tdc();
    dc_set_mx(1, 0);
    dp16_set(0xA2, g_cpu.X);
}

/* ---- sprite positions and OAM ----------------------------------------------- */
static u16 dp_x(u8 o) { return g_cpu.x_flag ? dp8(o) : dp16(o); }
static void sty_dp(u8 o) { if (g_cpu.x_flag) dp8_set(o, (u8)g_cpu.Y); else dp16_set(o, g_cpu.Y); }
static u8 asl8(u8 v) { g_cpu._flag_C = v >> 7; v = (u8)(v << 1); dc_set_flags_nz8(v); return v; }
/* TXA/TYA in m1 (B kept). */
static void txa8(void) { lda8((u8)g_cpu.X); }

/* CF:EA8A  One OAM entry (at 0700 + dp C9, 16 bytes per step) for the target
 * cursor of slot X when A5CC is set and 9FEB clear: 8 left / 4 up of the slot's
 * position 1D00 / 1D17, tile 02, attributes A5E3 | 9F46. */
DC_ENTRY(CFEA8A, RTS)
{
    DC_ASSUME_M(CFEA8A, 1);
    u16 x = g_cpu.X;
    if (!ldm(0xA5CC + x) || ldm(0x9FEB + x)) return;
    ldy(dp_x(0xC9));
    u16 y = g_cpu.Y;
    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0x1D00 + x), 0x08)); abs8_set(0x0700 + y, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0x1D17 + x), 0x04)); abs8_set(0x0701 + y, (u8)g_cpu.A);
    lda8(0x02); abs8_set(0x0702 + y, 0x02);
    lda8(abs8(0xA5E3) | abs8(0x9F46)); abs8_set(0x0703 + y, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(y, 0x0010);
    tay();
    sty_dp(0xC9);
    lda16(0x0000);
    dc_set_mx(1, g_cpu.x_flag);
}

/* CF:EC78  Slots 0-10 that are on (A15B) with a step count A500 >= 0 move by one
 * step of their path: (A517 * 8 + 7 - A500) indexes the X / Y offset tables at
 * CC:F494 / CC:F594, added to 1D0C / 1D23; A500 counts down. */
DC_ENTRY(CFEC78, RTS)
{
    DC_ASSUME_M(CFEC78, 1);
    DC_ASSUME_X(CFEC78, 0);
    tdc(); tay();
    do {
        u16 y = g_cpu.Y;
        if (ldm(0xA15B + y) && (ldm(0xA500 + y), !g_cpu._flag_N)) {
            g_cpu._flag_C = 1; lda8(dc_sbc8(0x07, abs8(0xA500 + y)));
            tax(); stx_dp(0xC6);
            lda8(abs8(0xA517 + y));
            g_cpu._flag_C = (g_cpu.A >> 13) & 1;
            g_cpu.A = (u16)(g_cpu.A << 3);
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(g_cpu.A, dp16(0xC6));
            ldx(g_cpu.A);
            tdc();
            u16 x = g_cpu.X;
            g_cpu._flag_C = 0; lda8(dc_adc8(bus8(0xCCF494 + x), abs8(0x1D0C + y))); abs8_set(0x1D0C + y, (u8)g_cpu.A);
            g_cpu._flag_C = 0; lda8(dc_adc8(bus8(0xCCF594 + x), abs8(0x1D23 + y))); abs8_set(0x1D23 + y, (u8)g_cpu.A);
            g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0xA500 + y), 0x01)); abs8_set(0xA500 + y, (u8)g_cpu.A);
        }
        ldy((u16)(g_cpu.Y + 1));
        dc_cmp16(g_cpu.Y, 0x000B);
    } while (!g_cpu._flag_Z);
}

/* CF:ECC2  Anchor points A240 / A250 (words) of the 8 effect sprites from A437:
 * bit 7 = leave alone, bit 6 = copy the anchor of sprite (A437 & 7), else the
 * position 1D0C / 1D23 of the battle object A437. */
DC_ENTRY(CFECC2, RTS)
{
    DC_ASSUME_X(CFECC2, 0);
    ldx(0x0007); stx_dp(0xC6);
    dc_set_mx(1, 1);
    do {
        ldx(dp8(0xC6));
        u16 x = g_cpu.X;
        lda8(abs8(0xA437 + x));
        if (!g_cpu._flag_N) {
            lda8((u8)g_cpu.A & 0x40);
            if (!g_cpu._flag_Z) {
                lda8(abs8(0xA437 + x) & 0x07);
                lda8(asl8((u8)g_cpu.A)); tay();
                txa8(); lda8(asl8((u8)g_cpu.A)); tax();
                for (int k = 0; k < 0x20; k += 0x10)
                    for (int j = 0; j < 2; j++) { lda8(abs8(0xA240 + k + j + g_cpu.Y)); abs8_set(0xA240 + k + j + g_cpu.X, (u8)g_cpu.A); }
            } else {
                lda8(abs8(0xA437 + x)); tay();
                txa8(); lda8(asl8((u8)g_cpu.A)); tax();
                lda8(abs8(0x1D0C + g_cpu.Y)); abs8_set(0xA240 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xA241 + g_cpu.X, 0);
                lda8(abs8(0x1D23 + g_cpu.Y)); abs8_set(0xA250 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xA251 + g_cpu.X, 0);
            }
        }
        u8 c = (u8)(dp8(0xC6) - 1); dp8_set(0xC6, c); dc_set_flags_nz8(c);
    } while (!g_cpu._flag_N);
    dc_set_mx(1, 0);
}

/* CF:ED18 (one axis)  Push the slot's new position into its 4-deep trail: the
 * low bytes t and high bytes h (4 entries, stride 16 hex) move up one step and
 * the newest is the base position + the signed offset off. */
static void trail_axis(u16 x, u16 t, u16 h, u16 off, u16 base)
{
    for (int k = 2; k >= 0; k--) { lda8(abs8(t + 0x16 * k + x)); abs8_set(t + 0x16 * (k + 1) + x, (u8)g_cpu.A); }
    for (int k = 2; k >= 0; k--) { lda8(abs8(h + 0x16 * k + x)); abs8_set(h + 0x16 * (k + 1) + x, (u8)g_cpu.A); }
    abs8_set(h + x, 0);
    lda8(abs8(off + x));
    if (g_cpu._flag_N) { u8 v = (u8)(abs8(h + x) - 1); abs8_set(h + x, v); dc_set_flags_nz8(v); }
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(base + x))); abs8_set(t + x, (u8)g_cpu.A);
    lda8(dc_adc8(abs8(h + x), 0x00)); abs8_set(h + x, (u8)g_cpu.A);
}
/* CF:ED18  Slots 0-10 with a trail (A192): every AB67 frames (countdown AB72)
 * record the position 1D0C / 1D23 plus the offset A4A4 / A4AF as a 16-bit
 * point in the trail history (afterimages). */
DC_ENTRY(CFED18, RTS)
{
    DC_ASSUME_X(CFED18, 0);
    ldx(0x000A);
    dc_set_mx(1, 1);
    do {
        u16 x = g_cpu.X;
        if (ldm(0xA192 + x)) {
            u8 t = (u8)(abs8(0xAB72 + x) - 1); abs8_set(0xAB72 + x, t); dc_set_flags_nz8(t);
            if (!t) {
                lda8(abs8(0xAB67 + x)); abs8_set(0xAB72 + x, (u8)g_cpu.A);
                trail_axis(x, 0xAB93, 0xAC64, 0xA4A4, 0x1D0C);
                trail_axis(x, 0xAB9E, 0xAC6F, 0xA4AF, 0x1D23);
            }
        }
        ldx((u16)(g_cpu.X - 1));
    } while (!g_cpu._flag_N);
    dc_set_mx(1, 0);
}

/* CF:E781  While AC5A is on: step the palette fade script D1:5E10 (entry A3D2;
 * 80 = end, which clears AC5A): each of the 11 slots takes the step D1:5E10 or,
 * when A0DC is set, D1:5E49 into A0E7, and adds it to its 4 colour channels
 * 922E + 4 * n (44 bytes from 922E). */
DC_ENTRY(CFE781, RTS)
{
    DC_ASSUME_X(CFE781, 0);
    if (!ldm(0xAC5A)) return;
    lda8(abs8(0xA3D2)); tax();
    u32 x = g_cpu.X;
    lda8(bus8(0xD15E10 + x));
    dc_cmp8((u8)g_cpu.A, 0x80);
    if (g_cpu._flag_Z) { abs8_set(0xAC5A, 0); return; }
    tdc(); tay();
    do {
        u16 y = g_cpu.Y;
        lda8(abs8(0xA0DC + y));
        lda8(bus8((g_cpu._flag_Z ? 0xD15E10 : 0xD15E49) + x));
        abs8_set(0xA0E7 + y, (u8)g_cpu.A);
        ldy((u16)(y + 1)); dc_cmp16(g_cpu.Y, 0x000B);
    } while (!g_cpu._flag_Z);
    { u8 v = (u8)(abs8(0xA3D2) + 1); abs8_set(0xA3D2, v); dc_set_flags_nz8(v); }
    tdc(); tax(); tay();
    do {
        dp8_set(0xC6, 0x04); lda8(0x04);
        do {
            g_cpu._flag_C = 0;
            lda8(dc_adc8(abs8(0x922E + g_cpu.X), abs8(0xA0E7 + g_cpu.Y)));
            abs8_set(0x922E + g_cpu.X, (u8)g_cpu.A);
            ldx((u16)(g_cpu.X + 4));
            u8 c = (u8)(dp8(0xC6) - 1); dp8_set(0xC6, c); dc_set_flags_nz8(c);
        } while (!g_cpu._flag_Z);
        ldy((u16)(g_cpu.Y + 1)); dc_cmp16(g_cpu.Y, 0x000B);
    } while (!g_cpu._flag_Z);
}

/* ---- math ---------------------------------------------------------------------- */
/* CF:FA43  Direction from point AC4C/AC4D to AC4A/AC4B as an angle (AC52 and A):
 * signed deltas AC4E / AC50 (16-bit), their sizes AC55 / AC57, the octant angle
 * from the 32 x 32 table C0:F300 indexed by (|dy| / 8) * 32 + |dx| / 8 (AC59),
 * then mirrored into the quadrant of the deltas. */
static u8 abs8_of(u16 lo, u16 hi)
{
    lda8(abs8(lo) ^ abs8(hi));
    g_cpu._flag_C = 1;
    lda8(dc_sbc8((u8)g_cpu.A, abs8(hi)));
    return (u8)g_cpu.A;
}
static void delta16(u16 a, u16 b, u16 out)
{
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(abs8(a), abs8(b))); abs8_set(out, (u8)g_cpu.A);
    lda8(dc_sbc8(0x00, 0x00)); abs8_set(out + 1, (u8)g_cpu.A);
}
DC_ENTRY(CFFA43, RTS)
{
    DC_ASSUME_M(CFFA43, 1);
    delta16(0xAC4A, 0xAC4C, 0xAC4E);
    delta16(0xAC4B, 0xAC4D, 0xAC50);
    abs8_set(0xAC55, abs8_of(0xAC4E, 0xAC4F)); abs8_set(0xAC56, 0);
    abs8_set(0xAC57, abs8_of(0xAC50, 0xAC51)); abs8_set(0xAC58, 0);
    dc_set_mx(0, g_cpu.x_flag);
    u16 v = abs16(0xAC55);
    g_cpu._flag_C = (v >> 2) & 1;
    abs16_set(0xAC53, v >> 3);
    v = (u16)((abs16(0xAC57) & 0xFFF8) << 2);
    g_cpu._flag_C = 0;
    v = dc_adc16(v, abs16(0xAC53));
    abs16_set(0xAC59, v);
    g_cpu._flag_C = v >> 15;
    ldx((u16)(v << 1));
    tdc();
    dc_set_mx(1, g_cpu.x_flag);
    ldx(abs_x(0xAC59));
    lda8(bus8(0xC0F300 + g_cpu.X)); abs8_set(0xAC52, (u8)g_cpu.A);
    if (!(abs8(0xAC4F) & 0x80)) {
        lda8(abs8(0xAC51));
        if (!g_cpu._flag_N) { g_cpu._flag_C = 0; lda8(dc_adc8(0x80, abs8(0xAC52))); }
        else { g_cpu._flag_C = 1; lda8(dc_sbc8(0x80, abs8(0xAC52))); }
        abs8_set(0xAC52, (u8)g_cpu.A);
        return;
    }
    lda8(abs8(0xAC51));
    if (!g_cpu._flag_N) {
        tdc(); g_cpu._flag_C = 1;
        lda8(dc_sbc8((u8)g_cpu.A, abs8(0xAC52))); abs8_set(0xAC52, (u8)g_cpu.A);
    }
    lda8(abs8(0xAC52));
}

/* CF:FDAD  The number 9499 (16-bit, < 10000) as four digit tiles (table CC:F90D)
 * at 949C..949F, thousands first; 9499 is left at (ones - 10): the last digit
 * is not written back. */
static void count_digit(u16 unit, bool keep)
{
    tdc(); tax();
    do {
        g_cpu._flag_C = 1;
        lda16(dc_sbc16(abs16(0x9499), unit)); abs16_set(0x9499, g_cpu.A);
        ldx((u16)(g_cpu.X + 1));
    } while (g_cpu._flag_C);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0x9499), unit));
    if (keep) abs16_set(0x9499, g_cpu.A);
    ldx((u16)(g_cpu.X - 1));
    dc_push16(g_cpu.X);
}
DC_ENTRY(CFFDAD, RTL)
{
    DC_ASSUME_M(CFFDAD, 0);
    DC_ASSUME_X(CFFDAD, 0);
    dc_push16(g_cpu.X);
    count_digit(1000, true);
    count_digit(100, true);
    count_digit(10, false);
    dc_set_mx(1, 0);
    tax();
    for (int k = 3; k >= 0; k--) {
        lda8(bus8(0xCCF90D + g_cpu.X)); abs8_set(0x949C + k, (u8)g_cpu.A);
        ldx(dc_pull16());
    }
}

/* CF:FAE2  Bank the highest pending item record (A0B5[n], 5 bytes at A0B8 + 8n)
 * into the list at 1580 (5 bytes per entry, up to 256): an entry with the same
 * id gets its count (+3) raised, at most 99; else the record goes to the first
 * free entry (id 0; the original runs past the list when it is full). Then the
 * record is cleared, and A0D0 counts it when 95D5 >= 0 and 95DB == 2. */
DC_ENTRY(CFFAE2, RTL)
{
    DC_ASSUME_X(CFFAE2, 0);
    ldy(0x0002);
    for (;;) {
        lda8(abs8(0xA0B5 + g_cpu.Y));
        if (!g_cpu._flag_Z) break;
        ldy((u16)(g_cpu.Y - 1));
        if (g_cpu._flag_N) return;
    }
    dp16_set(0x80, g_cpu.Y);
    lda8((u8)g_cpu.Y);
    for (int k = 0; k < 3; k++) lda8(asl8((u8)g_cpu.A));
    tax(); stx_dp(0x86);
    lda8(abs8(0xA0B8 + g_cpu.X)); dp8_set(0x84, (u8)g_cpu.A);
    tdc(); tay();
    bool found = false;
    do {
        lda8(abs8(0x1580 + g_cpu.Y));
        dc_cmp8((u8)g_cpu.A, dp8(0x84));
        if (g_cpu._flag_Z) { found = true; break; }
        ldy((u16)(g_cpu.Y + 5));
        dc_cmp16(g_cpu.Y, 0x0500);
    } while (!g_cpu._flag_Z);
    if (found) {
        g_cpu._flag_C = 0;
        lda8(dc_adc8(abs8(0x1583 + g_cpu.Y), 0x01));
        dc_cmp8((u8)g_cpu.A, 0x64);
        if (!g_cpu._flag_C) abs8_set(0x1583 + g_cpu.Y, (u8)g_cpu.A);
    } else {
        tdc(); tay();
        do {
            lda8(abs8(0x1580 + g_cpu.Y));
            if (g_cpu._flag_Z) break;
            ldy((u16)(g_cpu.Y + 5));
            dc_cmp16(g_cpu.Y, 0x0500);
        } while (!g_cpu._flag_Z);
        lda8(0x05); dp8_set(0x82, 0x05);
        do {
            lda8(abs8(0xA0B8 + g_cpu.X)); abs8_set(0x1580 + g_cpu.Y, (u8)g_cpu.A);
            ldx((u16)(g_cpu.X + 1)); ldy((u16)(g_cpu.Y + 1));
            u8 c = (u8)(dp8(0x82) - 1); dp8_set(0x82, c); dc_set_flags_nz8(c);
        } while (!g_cpu._flag_Z);
    }
    ldx(dp16(0x80)); abs8_set(0xA0B5 + g_cpu.X, 0);
    ldx(dp16(0x86));
    for (int k = 0; k < 5; k++) abs8_set(0xA0B8 + k + g_cpu.X, 0);
    lda8(abs8(0x95D5));
    if (!g_cpu._flag_N) {
        lda8(abs8(0x95DB)); dc_cmp8((u8)g_cpu.A, 0x02);
        if (g_cpu._flag_Z) { u8 v = (u8)(abs8(0xA0D0) + 1); abs8_set(0xA0D0, v); dc_set_flags_nz8(v); }
    }
}

/* ---- palettes ------------------------------------------------------------------ */
/* CF:E7D7  Colour cycling of the 3 party slots (A46D on, A101 = 0 none paused):
 * colour (A441 table CC:F754 + phase A499) from CE:0100 into palette row
 * 993C (not 8) at 2302/2318 + 16 * row; the phase steps by 2 and wraps at 7C. */
DC_ENTRY(CFE7D7, RTS)
{
    DC_ASSUME_M(CFE7D7, 1);
    DC_ASSUME_X(CFE7D7, 0);
    ldy(0x0002); sty_dp(0xC6);
    dc_set_mx(1, 1);
    do {
        ldy(dp_x(0xC6));
        u16 y = g_cpu.Y;
        if (ldm(0xA46D + y) && !ldm(0xA101)) {
            dc_set_mx(1, 0);
            lda8(asl8(abs8(0xA441 + y))); tax();
            g_cpu._flag_C = 0;
            lda8(abs8(0xA499 + y));
            dc_set_mx(0, 0);
            g_cpu.A = dc_adc16(g_cpu.A, bus16(0xCCF754 + g_cpu.X));
            tax();
            tdc();
            dc_set_mx(1, 0);
            lda8(abs8(0x993C + y));
            dc_cmp8((u8)g_cpu.A, 0x08);
            if (!g_cpu._flag_Z) {
                for (int k = 0; k < 4; k++) lda8(asl8((u8)g_cpu.A));
                tay();
                u32 x = g_cpu.X;
                lda8(bus8(0xCE0100 + x)); abs8_set(0x2302 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x2318 + g_cpu.Y, (u8)g_cpu.A);
                lda8(bus8(0xCE0101 + x)); abs8_set(0x2303 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x2319 + g_cpu.Y, (u8)g_cpu.A);
                ldx(dp16(0xC6));
                u8 v = (u8)(abs8(0xA499 + g_cpu.X) + 2); abs8_set(0xA499 + g_cpu.X, v);
                lda8(v); dc_cmp8(v, 0x7C);
                if (g_cpu._flag_Z) abs8_set(0xA499 + g_cpu.X, 0);
                dc_set_mx(1, 1);
            }
        }
        u8 c = (u8)(dp8(0xC6) - 1); dp8_set(0xC6, c); dc_set_flags_nz8(c);
    } while (!g_cpu._flag_N);
    dc_set_mx(1, 0);
}

/* ---- battle object sprites (OAM) ------------------------------------------------ */
/* Direct page indexed (bank 0, wraps at 64K). */
static u8 dpx8(u8 o, u16 x) { return b0_8((u16)(g_cpu.D + o + x)); }
static u16 sign8(u8 v) { return (v & 0x80) ? 0xFF : 0x00; }

/* CF:EB94  Copy the attribute words (byte 2 of each 4-byte OAM entry) of up to
 * 32 entries from X to Y, 4 entries per step, dp CD steps (stops at 0; a word
 * count in 16-bit mode, as CF:E83A calls it). */
DC_ENTRY(CFEB94, RTS)
{
    for (int g = 0; g < 8; g++) {
        for (int k = 0; k < 4; k++) {
            u16 o = (u16)(0x02 + 0x10 * g + 4 * k);
            ldm(o + g_cpu.X); stm(o + g_cpu.Y);
        }
        if (g == 7) break;
        if (g_cpu.m_flag) { u8 c = (u8)(dp8(0xCD) - 1); dp8_set(0xCD, c); dc_set_flags_nz8(c); }
        else { u16 c = (u16)(dp16(0xCD) - 1); dp16_set(0xCD, c); dc_set_flags_nz16(c); }
        if (g_cpu._flag_Z) return;
    }
}

/* PHY/PHX; PLD: D = v (through the stack, like the original). */
static void set_d(u16 v) { dc_push16(v); g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D); }
static void phx16(void) { dc_push16(g_cpu.X); }
static void plx16(void) { ldx(dc_pull16()); }
/* Mark the OAM entry at Y as large/off-screen in the high table when its 16-bit
 * position is off screen: y high byte set with y low < F0, or x high byte set. */
static void oam_high_bit(u8 xhi, u8 ylo, u8 yhi)
{
    bool high;
    lda8(dp8(yhi));
    if (!g_cpu._flag_Z) { lda8(dp8(ylo)); dc_cmp8((u8)g_cpu.A, 0xF0); high = !g_cpu._flag_C; }
    else { lda8(dp8(xhi)); high = !g_cpu._flag_Z; }
    if (high) {
        lda8(abs8(0x902D + g_cpu.Y)); tax();
        lda8(abs8(0x0900 + g_cpu.X) | abs8(0x8E2D + g_cpu.Y)); abs8_set(0x0900 + g_cpu.X, (u8)g_cpu.A);
    }
}

/* CF:EEEB  OAM entries of the effect sprite A3DF[X] & 7 (D = 0 inside, 1D00 on
 * return): origin = anchor A240 / A250 + offset A260 / A270 (dp CC / CE), palette
 * bits AB5F (dp A1); when A1B0 is on, the frame A05B[A2C0 & 3F] has its pieces
 * (4 bytes each, list CC:F7C0[n] into 4500) written from OAM offset 1DC9 on,
 * which then advances. */
DC_ENTRY(CFEEEB, RTS)
{
    DC_ASSUME_M(CFEEEB, 1);
    DC_ASSUME_X(CFEEEB, 0);
    ldy(0x0000); set_d(g_cpu.Y);
    lda8(abs8(0xA3DF + g_cpu.X) & 0x07); tax();
    lda8(asl8((u8)g_cpu.A)); tay();
    u16 x = g_cpu.X, y = g_cpu.Y;
    lda8(abs8(0xAB5F + x)); dp8_set(0xA1, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xA240 + y), abs16(0xA260 + y))); dp16_set(0xCC, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xA250 + y), abs16(0xA270 + y))); dp16_set(0xCE, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    if (ldm(0xA1B0 + x)) {
        lda8(abs8(0xA2C0 + x) & 0x3F); tay();
        lda8(abs8(0xA05B + g_cpu.Y));
        lda8(asl8((u8)g_cpu.A)); lda8(asl8((u8)g_cpu.A));
        tax(); dp16_set(0xC8, g_cpu.X);
        dc_set_mx(0, 0);
        g_cpu._flag_C = g_cpu.Y >> 15;
        ldx((u16)(g_cpu.Y << 1));
        ldx(bus16(0xCCF7C0 + g_cpu.X)); dp16_set(0xC6, g_cpu.X);
        ldy(abs16(0x1DC9));
        lda16(dp16(0xC8));
        dc_push16(g_cpu.A);
        g_cpu._flag_C = (g_cpu.A >> 1) & 1;
        lda16(g_cpu.A >> 2); dp16_set(0xC8, g_cpu.A);
        tdc();
        dc_set_mx(1, 0);
        do {
            static const u8 pos[2][2] = { { 0xCC, 0xCA }, { 0xCE, 0xD0 } };
            for (int k = 0; k < 2; k++) {
                lda8(abs8(0x4500 + k + g_cpu.X));
                dc_set_mx(0, 0);
                g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(pos[k][0]))); dp16_set(pos[k][1], g_cpu.A);
                dc_set_mx(1, 0);
                abs8_set(0x0700 + k + g_cpu.Y, (u8)g_cpu.A);
                tdc();
            }
            lda8(abs8(0x4502 + g_cpu.X)); abs8_set(0x0702 + g_cpu.Y, (u8)g_cpu.A);
            lda8((abs8(0x4503 + g_cpu.X) & 0xCF) | dp8(0xA1)); abs8_set(0x0703 + g_cpu.Y, (u8)g_cpu.A);
            oam_high_bit(0xCB, 0xD0, 0xD1);
            ldx((u16)(dp16(0xC6) + 4)); dp16_set(0xC6, g_cpu.X);
            ldy((u16)(g_cpu.Y + 4));
            u8 c = (u8)(dp8(0xC8) - 1); dp8_set(0xC8, c); dc_set_flags_nz8(c);
        } while (!g_cpu._flag_Z);
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        lda16(dc_adc16(dc_pull16(), abs16(0x1DC9))); abs16_set(0x1DC9, g_cpu.A);
        tdc();
        dc_set_mx(1, 0);
    }
    ldx(0x1D00); set_d(g_cpu.X);
}

/* CF:EDB8 / CF:EDF7 (loop)  For the effect sprites attached to object dp C5
 * (A3DF, one per 11 slots from C5 - 0C): those whose sprite A1D8 is on are
 * drawn with CF:EEEB. EDB8 draws the ones with bit 7 set (in front), EDF7 the
 * others (bit 6 ignored). */
static void draw_attached(bool front)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0xC5), 0x0C)); tax();
    do {
        phx16();
        lda8(abs8(0xA3DF + g_cpu.X));
        if (!g_cpu._flag_Z && g_cpu._flag_N == front) {
            phx16();
            lda8((u8)g_cpu.A & (front ? 0x7F : 0xBF)); tax();
            bool on = abs8(0xA1D8 + g_cpu.X) != 0;
            plx16();
            if (on) dc_CFEEEB_glue();
        }
        plx16();
        lda8((u8)g_cpu.X);
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x0B)); tax();
        dc_cmp16(g_cpu.X, 0x0058);
    } while (!g_cpu._flag_C);
}
DC_ENTRY(CFEDF7, RTS)
{
    DC_ASSUME_M(CFEDF7, 1);
    DC_ASSUME_X(CFEDF7, 0);
    if (!ldm(0xA013) || ldm(0xA3D1)) return;
    draw_attached(false);
}

/* CF:EDB8  Before the body of object dp C5 (D = 1D00):
 *  - A3D1 set: its 4-piece shadow/afterimage sprite (A3D4 on) from 922D + 16 * n,
 *    high bits 92DD, at OAM offset dp C9 (advanced by 16);
 *  - else in a normal battle frame (A013 or A0FE set): the front effect sprites;
 *  - else (D = 0 inside) the status icon of the object 1DC5 (A48E on): frame
 *    A4DB (4 frames, every A4C5 ticks, countdown A4BA) of icon set A4D0, 4 bytes
 *    from CC:F6D4 at its position 1D0C / 1D23, OAM offset 1DC9 advanced by 4. */
DC_ENTRY(CFEDB8, RTS)
{
    DC_ASSUME_M(CFEDB8, 1);
    DC_ASSUME_X(CFEDB8, 0);
    if (ldm(0xA3D1)) {
        g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0xC5), 0x0C)); tax();
        if (!ldm(0xA3D4 + g_cpu.X)) return;
        ldy(dp16(0xC9));
        lda8(abs8(0x902D + g_cpu.Y)); tay();
        lda8(abs8(0x92DD + g_cpu.X)); abs8_set(0x0900 + g_cpu.Y, (u8)g_cpu.A);
        lda8((u8)g_cpu.X);
        for (int k = 0; k < 4; k++) lda8(asl8((u8)g_cpu.A));
        tax();
        lda8(0x04); abs8_set(0x00C8, 0x04);
        ldy(dp16(0xC9));
        do {
            for (int k = 0; k < 4; k++) { lda8(abs8(0x922D + k + g_cpu.X)); abs8_set(0x0700 + k + g_cpu.Y, (u8)g_cpu.A); }
            ldy((u16)(g_cpu.Y + 4)); ldx((u16)(g_cpu.X + 4));
            u8 c = (u8)(abs8(0x00C8) - 1); abs8_set(0x00C8, c); dc_set_flags_nz8(c);
        } while (!g_cpu._flag_Z);
        dp16_set(0xC9, g_cpu.Y);
        return;
    }
    if (ldm(0xA013) || ldm(0xA0FE)) { draw_attached(true); return; }
    ldy(0x0000); set_d(g_cpu.Y);
    g_cpu._flag_C = 1; lda8(dc_sbc8(abs8(0x1DC5), 0x0C)); tax();
    u16 x = g_cpu.X;
    if (ldm(0xA48E + x)) {
        lda8(abs8(0x1D0C + x)); dp8_set(0xCC, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + x)); dp8_set(0xCD, (u8)g_cpu.A);
        u8 t = (u8)(abs8(0xA4BA + x) - 1); abs8_set(0xA4BA + x, t); dc_set_flags_nz8(t);
        if (!t) {
            lda8(abs8(0xA4C5 + x)); abs8_set(0xA4BA + x, (u8)g_cpu.A);
            u8 f = (u8)(abs8(0xA4DB + x) + 1); abs8_set(0xA4DB + x, f);
            lda8(f); dc_cmp8(f, 0x04);
            if (g_cpu._flag_Z) { tdc(); abs8_set(0xA4DB + x, (u8)g_cpu.A); }
        }
        lda8(abs8(0xA4DB + x));
        lda8(asl8((u8)g_cpu.A)); lda8(asl8((u8)g_cpu.A));
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0xA4D0 + x))); tax();
        u32 f = g_cpu.X;
        ldy(abs16(0x1DC9));
        g_cpu._flag_C = 0; lda8(dc_adc8(bus8(0xCCF6D4 + f), dp8(0xCC))); abs8_set(0x0700 + g_cpu.Y, (u8)g_cpu.A);
        g_cpu._flag_C = 0; lda8(dc_adc8(bus8(0xCCF6D5 + f), dp8(0xCD))); abs8_set(0x0701 + g_cpu.Y, (u8)g_cpu.A);
        lda8(bus8(0xCCF6D6 + f)); abs8_set(0x0702 + g_cpu.Y, (u8)g_cpu.A);
        lda8(bus8(0xCCF6D7 + f) | abs8(0xA5E3)); abs8_set(0x0703 + g_cpu.Y, (u8)g_cpu.A);
        ldy((u16)(g_cpu.Y + 4)); abs16_set(0x1DC9, g_cpu.Y);
    }
    ldy(0x1D00); set_d(g_cpu.Y);
}

/* CF:E99C head (also inline in CF:E83A)  Copy the attribute words of object dp C5's
 * sprite pieces from its frame buffer (CC:F84D[C5]) to its OAM entries at
 * 0700 + dp C9, (dp C7 + 1) / 16 groups of 4 (16-bit mode, as CF:EB94 runs). */
static void copy_piece_attrs(void)
{
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(dp16(0xC9), 0x0700); tay();
    lda16(dp16(0xC5));
    g_cpu._flag_C = g_cpu.A >> 15;
    ldx((u16)(g_cpu.A << 1));
    ldx(bus16(0xCCF84D + g_cpu.X));
    lda16((u16)(dp16(0xC7) + 1));
    lsr_a(4);
    dp16_set(0xCD, g_cpu.A);
    dc_CFEB94_glue();
}
/* Shared by CF:EAC5 / CF:E99C: X = object dp C5, Y = dp 42 = its piece offsets
 * (16 bytes per object from 9F47); true when the object is hidden (9FEB). */
static bool obj_head(void)
{
    lda8(dp8(0xC5)); tax();
    g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, 0x0C));
    for (int k = 0; k < 4; k++) lda8(asl8((u8)g_cpu.A));
    tay(); dp16_set(0x42, g_cpu.Y);
    return ldm(0x9FEB + g_cpu.X) != 0;
}
/* Shared by CF:EAC5 / CF:E99C: origin (dp 2E / 30) += signed offset (dp 3A / 3C),
 * priority bits 9930 | 99A9 (dp 37); for each of the CC:F50A[9670] pieces
 * (dp 34), its offset 9F47 / 9F49 (16-bit, from Y = dp 42) gives the screen
 * position; X or Y off screen sets the size/high bit in the high table. Steps
 * dp C9 / 42 by 4. */
static void obj_pieces(u16 x)
{
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; dp16_set(0x2E, dc_adc16(dp16(0x2E), dp16(0x3A)));
    g_cpu._flag_C = 0; dp16_set(0x30, dc_adc16(dp16(0x30), dp16(0x3C)));
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8(abs8(0x9930 + x) | abs8(0x99A9 + x)); dp8_set(0x37, (u8)g_cpu.A); dp8_set(0x36, 0);
    lda8(asl8(abs8(0x9670 + x))); tax();
    lda8(bus8(0xCCF50A + g_cpu.X)); tax(); dp16_set(0x34, g_cpu.X);
    do {
        ldy(dp16(0x42)); ldx(dp16(0xC9));
        u16 y = g_cpu.Y, o = g_cpu.X;
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x9F47 + y), dp16(0x2E))); dp16_set(0x32, g_cpu.A);
        g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0x9F49 + y), dp16(0x30))); dp16_set(0x38, g_cpu.A);
        lda16((u16)((abs16(0x0702 + o) & 0xC1FF) | dp16(0x36))); abs16_set(0x0702 + o, g_cpu.A);
        lda16(0x0000);
        dc_set_mx(1, 0);
        lda8(dp8(0x32)); abs8_set(0x0700 + o, (u8)g_cpu.A);
        lda8(dp8(0x38)); abs8_set(0x0701 + o, (u8)g_cpu.A);
        ldy(dp16(0xC9));
        oam_high_bit(0x33, 0x38, 0x39);
        ldx((u16)(dp16(0xC9) + 4)); dp16_set(0xC9, g_cpu.X);
        ldy((u16)(dp16(0x42) + 4)); dp16_set(0x42, g_cpu.Y);
        u8 c = (u8)(dp8(0x34) - 1); dp8_set(0x34, c); dc_set_flags_nz8(c);
    } while (!g_cpu._flag_Z);
}
/* CF:EAC5  Place the OAM pieces of battle object dp C5 (D = 1D00, so the object
 * table 1D00.. is the direct page) at its position (dp 00 / 17 + X) plus the
 * signed offsets A498 / A4A3. */
DC_ENTRY(CFEAC5, RTS)
{
    DC_ASSUME_M(CFEAC5, 0);
    DC_ASSUME_X(CFEAC5, 0);
    lda16(0x0000);
    dc_set_mx(1, 0);
    if (obj_head()) return;
    u16 x = g_cpu.X;
    lda8(dpx8(0x00, x)); dp8_set(0x2E, (u8)g_cpu.A); dp8_set(0x2F, 0);
    lda8(abs8(0xA498 + x)); dp8_set(0x3A, (u8)g_cpu.A);
    lda8((u8)sign8((u8)g_cpu.A)); dp8_set(0x3B, (u8)g_cpu.A);
    lda8(dpx8(0x17, x)); dp8_set(0x30, (u8)g_cpu.A); dp8_set(0x31, 0);
    lda8(abs8(0xA4A3 + x)); dp8_set(0x3C, (u8)g_cpu.A);
    lda8((u8)sign8((u8)g_cpu.A)); dp8_set(0x3D, (u8)g_cpu.A);
    obj_pieces(x);
}
/* CF:E99C  One afterimage of object dp C5 at the trail point dp 32 / 33 (low
 * bytes; the high bytes dp 34 / 35 only give the sign): attributes, then its
 * pieces like CF:EAC5. */
DC_ENTRY(CFE99C, RTS)
{
    DC_ASSUME_X(CFE99C, 0);
    copy_piece_attrs();
    lda16(0x0000);
    dc_set_mx(1, 0);
    if (obj_head()) return;
    u16 x = g_cpu.X;
    lda8(dp8(0x32)); dp8_set(0x2E, (u8)g_cpu.A); dp8_set(0x2F, 0);
    lda8(0x00); dp8_set(0x3A, 0);
    lda8(dp8(0x34)); lda8((u8)sign8((u8)g_cpu.A)); dp8_set(0x3B, (u8)g_cpu.A);
    lda8(dp8(0x33)); dp8_set(0x30, (u8)g_cpu.A); dp8_set(0x31, 0);
    lda8(0x00); dp8_set(0x3C, 0);
    lda8(dp8(0x35)); lda8((u8)sign8((u8)g_cpu.A)); dp8_set(0x3D, (u8)g_cpu.A);
    obj_pieces(x);
}
/* CF:E92C  Afterimages of object X: every AB71 frames (countdown AB7C) draw its
 * 4 trail points (CF:ED18's history) with CF:E99C. */
DC_ENTRY(CFE92C, RTS)
{
    DC_ASSUME_M(CFE92C, 1);
    DC_ASSUME_X(CFE92C, 0);
    stx_dp(0xCB);
    u8 t = (u8)(abs8(0xAB7C + g_cpu.X) - 1); abs8_set(0xAB7C + g_cpu.X, t); dc_set_flags_nz8(t);
    if (t) return;
    lda8(abs8(0xAB71 + g_cpu.X)); abs8_set(0xAB7C + g_cpu.X, (u8)g_cpu.A);
    for (int k = 0; k < 4; k++) {
        if (k) ldx(dp16(0xCB));
        static const u16 src[4] = { 0xAB87, 0xAB92, 0xAC58, 0xAC63 };
        for (int j = 0; j < 4; j++) { lda8(abs8(src[j] + 0x16 * k + g_cpu.X)); dp8_set((u8)(0x32 + j), (u8)g_cpu.A); }
        dc_CFE99C_glue();
    }
}

/* CF:E83A  Draw the battle objects 0C-16 (D = 1D00 inside, 0 on return): list
 * the visible ones (96E9) by their y (1D17) at dp 8A / A1 (id | 40; a second
 * entry id at y FE for objects with a shadow/aura A5C1, which also sets 1D39),
 * sort the list by y, largest first, end it with FF; then for each entry from
 * OAM offset dp C9 = 10: front effect sprites (CF:EDB8), the pieces with their
 * attributes (CF:EB94, CF:EAC5) unless the object is hidden (dp 39 + id) or
 * has no frame (dp 73 + id, kept in dp C7), the effect sprites behind
 * (CF:EDF7), the target cursor (CF:EA8A; a hidden object instead gets dp 39 =
 * 80 and no cursor), and its afterimages (CF:E92C, A186) in battle frames. */
DC_ENTRY(CFE83A, RTS)
{
    DC_ASSUME_M(CFE83A, 1);
    DC_ASSUME_X(CFE83A, 0);
    ldx(0x1D00); set_d(g_cpu.X);
    ldy(0x000C); ldx(0x0000);
    do {
        u16 y = g_cpu.Y;
        if (ldm(0x96E9 + y)) {
            lda8(abs8(0x1D17 + y)); b0_8_set((u16)(g_cpu.D + 0x8A + g_cpu.X), (u8)g_cpu.A);
            lda8((u8)y | 0x40); b0_8_set((u16)(g_cpu.D + 0xA1 + g_cpu.X), (u8)g_cpu.A);
            ldx((u16)(g_cpu.X + 1));
            lda8(abs8(0xA5C1 + y)); abs8_set(0x1D39 + y, (u8)g_cpu.A);
            if (!g_cpu._flag_Z) {
                abs8_set(0x1D39 + y, 0x01);
                lda8(0xFE); b0_8_set((u16)(g_cpu.D + 0x8A + g_cpu.X), 0xFE);
                lda8((u8)y); b0_8_set((u16)(g_cpu.D + 0xA1 + g_cpu.X), (u8)g_cpu.A);
                ldx((u16)(g_cpu.X + 1));
            }
        }
        ldy((u16)(y + 1));
        dc_cmp16(g_cpu.Y, 0x0017);
    } while (!g_cpu._flag_Z);
    ldx((u16)(g_cpu.X - 1)); dp16_set(0x30, g_cpu.X);
    ldy(0x0000);
    lda8(dp8(0x30));
    if (!g_cpu._flag_Z) {
        /* exchange sort on the y bytes (dp 8A..) with their ids (dp A1..) */
        do {
            ldx(g_cpu.Y);
            do {
                u16 a = (u16)(g_cpu.D + 0x8B + g_cpu.X), b = (u16)(g_cpu.D + 0xA2 + g_cpu.X);
                u8 v = abs8(0x1D8A + g_cpu.Y);
                dc_cmp8(v, b0_8(a));
                if (!g_cpu._flag_C) {
                    abs8_set(0x1D8A + g_cpu.Y, b0_8(a)); b0_8_set(a, v);
                    u8 w = abs8(0x1DA1 + g_cpu.Y);
                    abs8_set(0x1DA1 + g_cpu.Y, b0_8(b)); b0_8_set(b, w);
                    lda8(w);
                } else lda8(v);
                ldx((u16)(g_cpu.X + 1));
                dc_cmp16(g_cpu.X, dp16(0x30));
            } while (!g_cpu._flag_Z);
            ldy((u16)(g_cpu.Y + 1));
            dc_cmp16(g_cpu.Y, dp16(0x30));
        } while (!g_cpu._flag_Z);
    }
    lda8(0xFF); abs8_set(0x1DA2 + g_cpu.Y, 0xFF);
    ldy(0x0000); dp16_set(0xC5, 0); dp16_set(0xC7, 0); dp16_set(0xC3, 0);
    ldy(0x0010); dp16_set(0xC9, 0x0010);
    for (;;) {
        ldx(dp16(0xC3));
        lda8(dpx8(0xA1, g_cpu.X));
        if (g_cpu._flag_N) break;
        lda8((u8)g_cpu.A & 0x1F); dp8_set(0xC5, (u8)g_cpu.A); tax();
        bool cursor = true;
        if (!(lda8(dpx8(0x39, g_cpu.X)), g_cpu._flag_N)) {
            ldx(dp16(0xC5));
            lda8(dpx8(0x73, g_cpu.X));
            if (!g_cpu._flag_Z) {
                dp8_set(0xC7, (u8)g_cpu.A);
                dc_CFEDB8_glue();
                copy_piece_attrs();
                dc_CFEAC5_glue();
                dc_CFEDF7_glue();
                ldx(dp16(0xC5));
                lda8(dpx8(0x39, g_cpu.X));
                if (!g_cpu._flag_Z) {
                    lda8(0x80); b0_8_set((u16)(g_cpu.D + 0x39 + g_cpu.X), 0x80);
                    cursor = false;
                }
            }
        }
        if (cursor) dc_CFEA8A_glue();
        if (ldm(0xA013)) {
            ldx(dp16(0xC3));
            lda8(dpx8(0xA1, g_cpu.X));
            if (g_cpu.A & 0x40) {
                lda8((u8)g_cpu.A & 0x1F); tax();
                if (ldm(0xA186 + g_cpu.X)) dc_CFE92C_glue();
            }
        }
        u8 c = (u8)(dp8(0xC3) + 1); dp8_set(0xC3, c); dc_set_flags_nz8(c);
    }
    ldx(0x0000); set_d(g_cpu.X);
}

/* ---- effect movers (CF:EFAF and its two dispatch tables) -------------------------
 * Group 1: battle objects 0-10 (A337 on, handler A86D, period A2EB/A2FE), they
 * move 1D0C / 1D23. Group 2: effect sprites 0-7 (A342, A878, A2F6/A309), they
 * move the anchors A240 / A250. Per slot dp C6 = n, C8 = 2n, CA = 4n; byte arrays
 * of group 2 sit 0B after group 1's, word arrays 16 after, dword arrays 2C after;
 * the done flag is A19D / A1E0. */
static u16 slot(void) { ldx(dp16(0xC6)); return g_cpu.X; }
static void inc8_abs(u32 a) { u8 v = (u8)(abs8(a) + 1); abs8_set(a, v); dc_set_flags_nz8(v); }
static u8 dec8_abs(u32 a) { u8 v = (u8)(abs8(a) - 1); abs8_set(a, v); dc_set_flags_nz8(v); return v; }
static void sta8(u32 a) { abs8_set(a, (u8)g_cpu.A); }
static void sin_of(u8 angle) { lda8(angle); dc_CFF9FB_glue(); }

/* Velocity: position words A8D4 / A8FA += A888 / A8AE (index dp C8). */
static void vel_step(u16 o)
{
    ldx(dp16(0xC8));
    u16 x = g_cpu.X;
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xA888 + o + x), abs16(0xA8D4 + o + x))); abs16_set(0xA8D4 + o + x, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xA8AE + o + x), abs16(0xA8FA + o + x))); abs16_set(0xA8FA + o + x, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
}
/* Bouncing height, dp CC steps (16-bit count): rising (A9DE = 0) the speed A920
 * (dword at dp CA) loses A9B8 each step until it would go negative, then it
 * falls (A9DE = 1; bump_done also counts the bounce in the done flag, a word
 * increment); falling it gains A9B8 until it overflows, then restarts at AA3D.
 * The height A96C (dword) follows the speed. dp 04 = the height's high word. */
static void bounce(u16 o, u16 bump_done)
{
    dc_set_mx(0, 0);
    ldx(dp16(0xC8)); ldy(dp16(0xCA));
    u16 x = g_cpu.X + o, y = g_cpu.Y + 2 * o;
    do {
        lda16(abs16(0xA9DE + x));
        if (g_cpu._flag_Z) {
            g_cpu._flag_C = 1; lda16(dc_sbc16(abs16(0xA920 + y), abs16(0xA9B8 + x))); abs16_set(0xA920 + y, g_cpu.A);
            if (!g_cpu._flag_C) {
                tdc(); abs16_set(0xA920 + y, g_cpu.A); abs16_set(0xA922 + y, g_cpu.A);
                lda16((u16)(g_cpu.A + 1)); abs16_set(0xA9DE + x, g_cpu.A);
                if (bump_done) {
                    u16 s = g_cpu.X;
                    ldx(dp16(0xC6));
                    u16 v = (u16)(abs16(bump_done + g_cpu.X) + 1); abs16_set(bump_done + g_cpu.X, v);
                    ldx(s);
                }
            }
        } else {
            g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xA920 + y), abs16(0xA9B8 + x))); abs16_set(0xA920 + y, g_cpu.A);
            if (g_cpu._flag_C) {
                lda16(abs16(0xAA3D + x)); abs16_set(0xA920 + y, g_cpu.A);
                tdc(); abs16_set(0xA922 + y, g_cpu.A); abs16_set(0xA9DE + x, g_cpu.A);
            }
        }
        lda16(abs16(0xA9DE + x));
        if (g_cpu._flag_Z) {
            g_cpu._flag_C = 0;
            lda16(dc_adc16(abs16(0xA920 + y), abs16(0xA96C + y))); abs16_set(0xA96C + y, g_cpu.A);
            lda16(dc_adc16(abs16(0xA922 + y), abs16(0xA96E + y))); abs16_set(0xA96E + y, g_cpu.A);
        } else {
            g_cpu._flag_C = 1;
            lda16(dc_sbc16(abs16(0xA96C + y), abs16(0xA920 + y))); abs16_set(0xA96C + y, g_cpu.A);
            lda16(dc_sbc16(abs16(0xA96E + y), abs16(0xA922 + y))); abs16_set(0xA96E + y, g_cpu.A);
        }
        u16 c = (u16)(dp16(0xCC) - 1); dp16_set(0xCC, c); dc_set_flags_nz16(c);
    } while (!g_cpu._flag_Z);
    lda16(abs16(0xA96E + y)); dp16_set(0x04, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
}
/* A -/+ the height dp 04 (twice unless `single`). */
static void apply_hop(bool down, bool single)
{
    for (int k = single ? 1 : 2; k--; ) {
        g_cpu._flag_C = down;
        lda8(down ? dc_sbc8((u8)g_cpu.A, dp8(0x04)) : dc_adc8((u8)g_cpu.A, dp8(0x04)));
    }
}
/* Group 1 position from the velocity: 1D0C / 1D23 = A8D5 / A8FB (high bytes) + A35D / A370. */
static void obj_from_vel(void)
{
    u16 x = slot(); ldy(dp16(0xC8)); u16 y = g_cpu.Y;
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA8D5 + y), abs8(0xA35D + x))); sta8(0x1D0C + x);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA8FB + y), abs8(0xA370 + x))); sta8(0x1D23 + x);
}
/* Group 2 x anchor from the velocity: A240 = A8EB + A368 (word, high 0). Leaves A
 * at the y base A911 + A37B. */
static void fx_from_vel(void)
{
    u16 x = slot(); ldy(dp16(0xC8)); u16 y = g_cpu.Y;
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA8EB + y), abs8(0xA368 + x))); sta8(0xA240 + y);
    tdc(); sta8(0xA241 + y);
}
static void fx_y_base(void)
{
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA911 + g_cpu.Y), abs8(0xA37B + g_cpu.X)));
}
static void fx_y_store(void) { sta8(0xA250 + g_cpu.Y); tdc(); sta8(0xA251 + g_cpu.Y); }

/* CF:F03E  Group 1, straight line: move by the velocity for ABEB frames. */
DC_ENTRY(CFF03E, RTS)
{
    u16 x = slot();
    if (ldm(0xA19D + x)) return;
    if (ldm(0xABEB + x)) {
        vel_step(0);
        obj_from_vel();
        if (dec8_abs(0xABEB + g_cpu.X)) return;
    }
    lda8(0x01); sta8(0xA19D + g_cpu.X);
}
/* CF:F087  Group 1, line with bounces (A324 steps a frame) for ABFE frames; the
 * height goes to the y offset A4AF (up or down by AA2A, once or twice by AA04). */
DC_ENTRY(CFF087, RTS)
{
    u16 x = slot();
    if (ldm(0xA19D + x)) return;
    if (ldm(0xABFE + x)) {
        vel_step(0);
        x = slot();
        lda8(abs8(0xA324 + x)); tax(); stx_dp(0xCC);
        bounce(0, 0);
        obj_from_vel();
        bool down = !ldm(0xAA2A + g_cpu.X);
        bool single = ldm(0xAA04 + g_cpu.Y) != 0;
        lda8(0x00); apply_hop(down, single);
        sta8(0xA4AF + g_cpu.X);
        x = slot();
        if (dec8_abs(0xABFE + x)) return;
    }
    abs8_set(0xA4AF + g_cpu.X, 0);
    lda8(0x01); sta8(0xA19D + g_cpu.X);
}
/* CF:F194  Group 1, spiral: angle A34A (kept in A5AA), radius A311 += A324, around
 * A35D / A370; ends off screen (x or y tile 0 or edge) or after A639 frames. */
DC_ENTRY(CFF194, RTS)
{
    u16 x = slot();
    if (ldm(0xA19D + x)) return;
    bool done = true;
    lda8(abs8(0x1D23 + x)); dc_CFFADD_glue();
    if (!g_cpu._flag_Z && (dc_cmp8((u8)g_cpu.A, 0x0E), !g_cpu._flag_Z)) {
        lda8(abs8(0x1D0C + x)); dc_CFFADD_glue();
        if (!g_cpu._flag_Z && (dc_cmp8((u8)g_cpu.A, 0x0F), !g_cpu._flag_Z) && dec8_abs(0xA639 + x)) done = false;
    }
    if (done) { lda8(0x01); sta8(0xA19D + x); return; }
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA311 + x), abs8(0xA324 + x))); sta8(0xA311 + x); dp8_set(0x00, (u8)g_cpu.A);
    lda8(abs8(0xA34A + x)); sta8(0xA5AA + x);
    dc_CFF9FB_glue(); dp8_set(0xCC, (u8)g_cpu.A);
    x = slot();
    g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0xA34A + x), 0x40)); dp8_set(0xCD, (u8)g_cpu.A);
    x = slot();
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA370 + x), dp8(0xCC))); sta8(0x1D23 + x);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA35D + x), dp8(0xCD))); sta8(0x1D0C + x);
}
/* CF:F1F8 / CF:F354  Group 1, circling offset: angle AA9A += AAD3, radius AAE6
 * (halved for the y offset in F354) into the offsets A4AF / A4A4, for the
 * 16-bit count AAAD frames. */
static void orbit_offset(bool half_y)
{
    ldy(dp16(0xC6));
    u16 y = g_cpu.Y;
    if (ldm(0xA19D + y)) return;
    lda8(abs8(0xAAE6 + y));
    if (half_y) { g_cpu._flag_C = g_cpu.A & 1; lda8((u8)g_cpu.A >> 1); }
    dp8_set(0x00, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xAA9A + y), abs8(0xAAD3 + y))); sta8(0xAA9A + y);
    dc_CFF9FB_glue(); sta8(0xA4AF + y);
    if (half_y) { lda8(abs8(0xAAE6 + y)); dp8_set(0x00, (u8)g_cpu.A); }
    g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0xAA9A + y), 0x40)); sta8(0xA4A4 + y);
    ldx(dp16(0xC8));
    u16 x = g_cpu.X;
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(abs8(0xAAAD + x), 0x01)); sta8(0xAAAD + x);
    lda8(dc_sbc8(abs8(0xAAAE + x), 0x00)); sta8(0xAAAE + x);
    lda8((u8)g_cpu.A | abs8(0xAAAD + x));
    if (g_cpu._flag_Z) { ldx(g_cpu.Y); inc8_abs(0xA19D + g_cpu.X); }
}
DC_ENTRY(CFF1F8, RTS) { orbit_offset(false); }
DC_ENTRY(CFF354, RTS) { orbit_offset(true); }

/* CF:F23D  Group 1, jump to the target A383 / A396 with 4 bounce steps a frame:
 * done on landing (bounce counted in A19D), or when near the target (within 10
 * hex and A4AF >= E8 the first time, AB00 = 1; within 1 later). The y offset
 * is always -2 * height (the single-height store is overwritten). */
DC_ENTRY(CFF23D, RTS)
{
    ldx(dp16(0xC8));
    if (ldm(0xA9DE + g_cpu.X)) {
        u16 x = slot();
        lda8(abs8(0x1D0C + x)); dp8_set(0xCC, (u8)g_cpu.A);
        lda8(abs8(0x1D23 + x)); dp8_set(0xCD, (u8)g_cpu.A);
        lda8(abs8(0xA383 + x)); dp8_set(0xCE, (u8)g_cpu.A);
        lda8(abs8(0xA396 + x)); dp8_set(0xCF, (u8)g_cpu.A);
        bool first = !ldm(0xAB00 + x);
        lda8(first ? 0x10 : 0x01); dp8_set(0xD1, (u8)g_cpu.A);
        dc_CFF957_glue();
        lda8(dp8(0xD0));
        if (!g_cpu._flag_Z) {
            if (!first) { inc8_abs(0xA19D + x); return; }
            lda8(abs8(0xA4AF + x)); dc_cmp8((u8)g_cpu.A, 0xE8);
            if (g_cpu._flag_C) { inc8_abs(0xA19D + x); inc8_abs(0xAB00 + x); return; }
        }
    }
    vel_step(0);
    ldx(0x0004); stx_dp(0xCC);
    bounce(0, 0xA19D);
    obj_from_vel();
    if (ldm(0xAA04 + g_cpu.Y)) { lda8(0x00); apply_hop(true, true); sta8(0xA4AF + g_cpu.X); }
    lda8(0x00); apply_hop(true, false); sta8(0xA4AF + g_cpu.X);
}

/* CF:F3BA  Group 1 shake: for AB13 frames (then the offsets and A337 are cleared),
 * every AB34 frames (countdown AB29) toggle AB3F between rest (offsets 0) and the
 * offset of radius AB1E at angle A5AA. */
DC_ENTRY(CFF3BA, RTS)
{
    u16 x = slot();
    if (!dec8_abs(0xAB13 + x)) {
        abs8_set(0xA4A4 + x, 0); abs8_set(0xA4AF + x, 0); abs8_set(0xA337 + x, 0);
        return;
    }
    if (dec8_abs(0xAB29 + x)) return;
    lda8(abs8(0xAB34 + x)); sta8(0xAB29 + x);
    lda8(abs8(0xAB1E + x)); dp8_set(0x00, (u8)g_cpu.A);
    lda8(abs8(0xAB3F + x) ^ 0x01); sta8(0xAB3F + x);
    if (!g_cpu._flag_Z) { abs8_set(0xA4A4 + x, 0); abs8_set(0xA4AF + x, 0); return; }
    ldy(dp16(0xC6));
    u16 y = g_cpu.Y;
    sin_of(abs8(0xA5AA + y)); sta8(0xA4AF + y);
    g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0xA5AA + y), 0x40)); sta8(0xA4A4 + y);
}
/* CF:F39F  Shake (CF:F3BA); slot 3 with A113 set also copies its offsets to slot 9. */
DC_ENTRY(CFF39F, RTS)
{
    dc_CFF3BA_glue();
    lda8(dp8(0xC6)); dc_cmp8((u8)g_cpu.A, 0x03);
    if (!g_cpu._flag_Z || !ldm(0xA113)) return;
    lda8(abs8(0xA4A7)); sta8(0xA4AD);
    lda8(abs8(0xA4B2)); sta8(0xA4B8);
}

/* CF:F405 / CF:F884 (middle)  Homing: turn the heading AC11 (o = 0B for group 2)
 * toward the angle AC52 of the target by up to A324 steps of 1 (AC37 = turn
 * the other way); once it has met it (AC24 set) it follows exactly. Returns
 * with A = the direction's sprite frame C0:F700[heading], X = slot. */
static void steer(u16 o)
{
    u16 x = slot();
    lda8(abs8(0xA324 + o + x)); dp8_set(0x00, (u8)g_cpu.A); tay();
    if (ldm(0xAC24 + o + x)) lda8(abs8(0xAC52));
    else {
        lda8(abs8(0xAC11 + o + x)); dc_cmp8((u8)g_cpu.A, abs8(0xAC52));
        if (!g_cpu._flag_Z) {
            bool back = ldm(0xAC37 + o + x) != 0;
            lda8(abs8(0xAC11 + o + x));
            for (;;) {
                g_cpu._flag_C = back;
                lda8(back ? dc_sbc8((u8)g_cpu.A, 0x01) : dc_adc8((u8)g_cpu.A, 0x01));
                dc_cmp8((u8)g_cpu.A, abs8(0xAC52));
                if (g_cpu._flag_Z) { inc8_abs(0xAC24 + o + x); break; }
                ldy((u16)(g_cpu.Y - 1));
                if (g_cpu._flag_Z) break;
            }
        }
    }
    sta8(0xAC11 + o + x);
    tax();
    lda8(bus8(0xC0F700 + g_cpu.X));
    slot();
}
/* Load the mover's point (dp CC/CD and AC4A/AC4B) and target (CE/CF, AC4C/AC4D)
 * and test them for nearness (CF:F957, within 10 hex): true = arrived. */
static bool arrived(u8 px, u8 py, u8 tx, u8 ty)
{
    const u8 v[4] = { px, py, tx, ty };
    for (int k = 0; k < 4; k++) { lda8(v[k]); dp8_set((u8)(0xCC + k), v[k]); abs8_set(0xAC4A + k, v[k]); }
    lda8(0x10); dp8_set(0xD1, 0x10);
    dc_CFF957_glue();
    lda8(dp8(0xD0));
    return !g_cpu._flag_Z;
}
/* CF:F405  Group 1 homing toward A383 / A396: done when near; else steer
 * (frame to 96DB) and step by the unit vector of the heading. */
DC_ENTRY(CFF405, RTS)
{
    u16 x = slot();
    if (arrived(abs8(0x1D0C + x), abs8(0x1D23 + x), abs8(0xA383 + x), abs8(0xA396 + x))) { inc8_abs(0xA19D + x); return; }
    dc_CFFA43_glue();
    steer(0);
    sta8(0x96DB + g_cpu.X);
    sin_of(abs8(0xAC11 + g_cpu.X)); dp8_set(0xCC, (u8)g_cpu.A);
    x = slot();
    g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0xAC11 + x), 0x40)); dp8_set(0xCD, (u8)g_cpu.A);
    x = slot();
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x1D23 + x), dp8(0xCC))); sta8(0x1D23 + x);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x1D0C + x), dp8(0xCD))); sta8(0x1D0C + x);
}
/* CF:F884  Group 2 homing of the anchor (negative coordinates count as 0) toward
 * A38E / A3A1; frame to A230; 16-bit step with sign-extended unit vector. */
DC_ENTRY(CFF884, RTS)
{
    u16 x = slot(); ldy(dp16(0xC8)); u16 y = g_cpu.Y;
    lda8(abs8(0xA241 + y)); if (g_cpu._flag_N) tdc(); else lda8(abs8(0xA240 + y));
    u8 px = (u8)g_cpu.A;
    lda8(abs8(0xA251 + y)); if (g_cpu._flag_N) tdc(); else lda8(abs8(0xA250 + y));
    if (arrived(px, (u8)g_cpu.A, abs8(0xA38E + x), abs8(0xA3A1 + x))) { inc8_abs(0xA1E0 + x); return; }
    dc_CFFA43_glue();
    steer(0x0B);
    sta8(0xA230 + g_cpu.X);
    dp8_set(0xCB, 0); dp8_set(0xCD, 0);
    sin_of(abs8(0xAC1C + g_cpu.X)); dp8_set(0xCA, (u8)g_cpu.A);
    if (g_cpu._flag_N) { lda8(0xFF); dp8_set(0xCB, 0xFF); }
    x = slot();
    g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0xAC1C + x), 0x40)); dp8_set(0xCC, (u8)g_cpu.A);
    if (g_cpu._flag_N) { lda8(0xFF); dp8_set(0xCD, 0xFF); }
    ldx(dp16(0xC8));
    x = g_cpu.X;
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xA250 + x), dp16(0xCA))); abs16_set(0xA250 + x, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(abs16(0xA240 + x), dp16(0xCC))); abs16_set(0xA240 + x, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
}

/* CF:F4B2  Group 2 straight line for ABF6 frames. */
DC_ENTRY(CFF4B2, RTS)
{
    u16 x = slot();
    if (ldm(0xA1E0 + x)) return;
    if (ldm(0xABF6 + x)) {
        vel_step(0x16);
        fx_from_vel(); fx_y_base(); fx_y_store();
        if (dec8_abs(0xABF6 + g_cpu.X)) return;
    }
    lda8(0x01); sta8(0xA1E0 + g_cpu.X);
}
/* CF:F503  Group 2 line with bounces for AC09 frames (height into the y anchor). */
DC_ENTRY(CFF503, RTS)
{
    u16 x = slot();
    if (ldm(0xA1E0 + x)) return;
    if (ldm(0xAC09 + x)) {
        vel_step(0x16);
        x = slot();
        lda8(abs8(0xA32F + x)); tax(); stx_dp(0xCC);
        bounce(0x16, 0);
        fx_from_vel();
        bool down = !ldm(0xAA35 + g_cpu.X);
        bool single = ldm(0xAA1A + g_cpu.Y) != 0;
        fx_y_base(); apply_hop(down, single);
        fx_y_store();
        x = slot();
        if (dec8_abs(0xAC09 + x)) return;
    }
    lda8(0x01); sta8(0xA1E0 + g_cpu.X);
}
/* CF:F619  (tail of CF:F503) group 2 slot X done. */
DC_ENTRY(CFF619, RTS) { lda8(0x01); sta8(0xA1E0 + g_cpu.X); }
/* CF:F61F  Group 2 spiral around A368 / A37B (angle A355 kept in A5B5, radius
 * A31C += A32F); ends off screen or after A644 frames. */
DC_ENTRY(CFF61F, RTS)
{
    u16 x = slot(); ldy(dp16(0xC8)); u16 y = g_cpu.Y;
    if (ldm(0xA1E0 + x)) return;
    bool done = true;
    lda8(abs8(0xA250 + y)); dc_CFFADD_glue();
    if (!g_cpu._flag_Z && (dc_cmp8((u8)g_cpu.A, 0x0E), !g_cpu._flag_Z)) {
        lda8(abs8(0xA240 + y)); dc_CFFADD_glue();
        if (!g_cpu._flag_Z && (dc_cmp8((u8)g_cpu.A, 0x0F), !g_cpu._flag_Z) && dec8_abs(0xA644 + x)) done = false;
    }
    if (done) { lda8(0x01); sta8(0xA1E0 + x); return; }
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA31C + x), abs8(0xA32F + x))); sta8(0xA31C + x); dp8_set(0x00, (u8)g_cpu.A);
    lda8(abs8(0xA355 + x)); sta8(0xA5B5 + x);
    dc_CFF9FB_glue(); dp8_set(0xCC, (u8)g_cpu.A);
    x = slot();
    g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0xA355 + x), 0x40)); dp8_set(0xCD, (u8)g_cpu.A);
    ldy(dp16(0xC6)); ldx(dp16(0xC8));
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA37B + g_cpu.Y), dp8(0xCC))); sta8(0xA250 + g_cpu.X); abs8_set(0xA251 + g_cpu.X, 0);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA368 + g_cpu.Y), dp8(0xCD))); sta8(0xA240 + g_cpu.X); abs8_set(0xA241 + g_cpu.X, 0);
}
/* CF:F68D / CF:F7FC  Group 2 circling: angle AAA5 += AADE, radius AAF1 (halved
 * for y in F7FC) around AA7F / AA92, for the 16-bit count AAC3 frames. */
static void orbit_anchor(bool half_y)
{
    u16 y = g_cpu.Y;
    lda8(abs8(0xAAF1 + y));
    if (half_y) { g_cpu._flag_C = g_cpu.A & 1; lda8((u8)g_cpu.A >> 1); }
    dp8_set(0x00, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xAAA5 + y), abs8(0xAADE + y))); sta8(0xAAA5 + y);
    dc_CFF9FB_glue(); dp8_set(0xCC, (u8)g_cpu.A);
    if (half_y) { lda8(abs8(0xAAF1 + y)); dp8_set(0x00, (u8)g_cpu.A); }
    g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0xAAA5 + y), 0x40)); dp8_set(0xCD, (u8)g_cpu.A);
    ldx(dp16(0xC8));
    u16 x = g_cpu.X;
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xAA7F + y), dp8(0xCD))); sta8(0xA240 + x);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xAA92 + y), dp8(0xCC))); sta8(0xA250 + x);
    g_cpu._flag_C = 1;
    lda8(dc_sbc8(abs8(0xAAC3 + x), 0x01)); sta8(0xAAC3 + x);
    lda8(dc_sbc8(abs8(0xAAC4 + x), 0x00)); sta8(0xAAC4 + x);
    lda8((u8)g_cpu.A | abs8(0xAAC3 + x));
    if (g_cpu._flag_Z) { ldx(y); inc8_abs(0xA1E0 + g_cpu.X); }
}
DC_ENTRY(CFF68D, RTS)
{
    ldy(dp16(0xC6));
    if (ldm(0xA1E0 + g_cpu.Y)) return;
    orbit_anchor(false);
}
/* CF:F7FC  ... and the sprite 0B * n + AB4F of the effect list A3DF goes in front
 * (bit 7) on the near half of the circle (angle < 80), else behind (bit 6). */
DC_ENTRY(CFF7FC, RTS)
{
    lda8(dp8(0xC6)); tay(); dp8_set(0x00, (u8)g_cpu.A);
    if (ldm(0xA1E0 + g_cpu.Y)) return;
    lda8(0x0B); dp8_set(0x02, 0x0B); dp8_set(0x03, 0);
    dc_CFFA17_glue();
    g_cpu._flag_C = 0; lda8(dc_adc8(dp8(0x04), abs8(0xAB4F + g_cpu.Y))); tax();
    lda8(abs8(0xAAA5 + g_cpu.Y)); dc_cmp8((u8)g_cpu.A, 0x80);
    u8 side = g_cpu._flag_C ? 0x40 : 0x80;
    lda8((u8)((abs8(0xA3DF + g_cpu.X) & 0x3F) | side)); sta8(0xA3DF + g_cpu.X);
    orbit_anchor(true);
}
/* CF:F6E2  Group 2 jump toward A38E / A3A1, 4 bounce steps a frame: done on
 * landing or when near (within 18 hex the first time, AB0B = 1; within 1 later). */
DC_ENTRY(CFF6E2, RTS)
{
    ldy(dp16(0xC8));
    if (ldm(0xA9F4 + g_cpu.Y)) {
        u16 x = slot(), y = g_cpu.Y;
        lda8(abs8(0xA240 + y)); dp8_set(0xCC, (u8)g_cpu.A);
        lda8(abs8(0xA250 + y)); dp8_set(0xCD, (u8)g_cpu.A);
        lda8(abs8(0xA38E + x)); dp8_set(0xCE, (u8)g_cpu.A);
        lda8(abs8(0xA3A1 + x)); dp8_set(0xCF, (u8)g_cpu.A);
        bool first = !ldm(0xAB0B + x);
        lda8(first ? 0x18 : 0x01); dp8_set(0xD1, (u8)g_cpu.A);
        dc_CFF957_glue();
        lda8(dp8(0xD0));
        if (!g_cpu._flag_Z) {
            inc8_abs(0xA1E0 + x);
            if (first) inc8_abs(0xAB0B + x);
            return;
        }
    }
    vel_step(0x16);
    ldx(0x0004); stx_dp(0xCC);
    bounce(0x16, 0xA1E0);
    fx_from_vel();
    bool single = ldm(0xAA1A + g_cpu.Y) != 0;
    fx_y_base(); apply_hop(true, single);
    fx_y_store();
}

/* CF:EFAF  Per frame: in a battle frame (A013) run the movers of the 11 objects
 * and 8 effect sprites that are on, each every A2FE / A309 frames, by their
 * handler A86D / A878; otherwise unless A0FD the 8 orbiting sparkles:
 * radius 9737 += 98C2 every 9742 frames (off at 8), angle 9786, around
 * 98B7 / 98AF into 1D26 / 1D0F. */
static void (*const mover1[8])(void) = {
    dc_CFF03E_glue, dc_CFF087_glue, dc_CFF194_glue, dc_CFF1F8_glue,
    dc_CFF23D_glue, dc_CFF354_glue, dc_CFF39F_glue, dc_CFF405_glue,
};
static void (*const mover2[8])(void) = {
    dc_CFF4B2_glue, dc_CFF503_glue, dc_CFF61F_glue, dc_CFF68D_glue,
    dc_CFF6E2_glue, dc_CFF7FC_glue, dc_CFF39F_glue, dc_CFF884_glue,
};
static void run_movers(u16 n, u16 on, u16 timer, u16 period, u16 handler, u16 table, u32 site, void (*const fn[8])(void))
{
    ldx(n); stx_dp(0xC6);
    do {
        u16 x = slot();
        if (ldm(on + x) && !dec8_abs(timer + x)) {
            lda8(abs8(period + x)); sta8(timer + x);
            dc_set_mx(0, 0);
            lda16(dp16(0xC6));
            lda16((u16)(g_cpu.A << 1)); dp16_set(0xC8, g_cpu.A);
            g_cpu._flag_C = g_cpu.A >> 15;
            lda16((u16)(g_cpu.A << 1)); dp16_set(0xCA, g_cpu.A);
            tdc();
            dc_set_mx(1, 0);
            lda8(asl8(abs8(handler + x))); tax();
            if (g_cpu.X < 16) fn[g_cpu.X >> 1]();
            else dc_jsr(0xCF0000 | bus16(0xCF0000 | (u16)(table + g_cpu.X)), site);
        }
        u8 c = (u8)(dp8(0xC6) - 1); dp8_set(0xC6, c); dc_set_flags_nz8(c);
    } while (!g_cpu._flag_N);
}
static void sparkles(void)
{
    ldx(0x0007); stx_dp(0xC6);
    do {
        u16 x = slot();
        if (ldm(0x98A7 + x) && !dec8_abs(0x9819 + x)) {
            lda8(abs8(0x9742 + x)); sta8(0x9819 + x);
            g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x9737 + x), abs8(0x98C2 + x))); sta8(0x9737 + x); dp8_set(0x00, (u8)g_cpu.A);
            dc_cmp8((u8)g_cpu.A, 0x08);
            if (g_cpu._flag_Z) { abs8_set(0x98A7 + x, 0); abs8_set(0x9737 + x, 0); }
            sin_of(abs8(0x9786 + x)); dp8_set(0xC8, (u8)g_cpu.A);
            x = slot();
            g_cpu._flag_C = 0; sin_of(dc_adc8(abs8(0x9786 + x), 0x40)); dp8_set(0xC9, (u8)g_cpu.A);
            x = slot();
            g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x98B7 + x), dp8(0xC8))); sta8(0x1D26 + x);
            g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x98AF + x), dp8(0xC9))); sta8(0x1D0F + x);
        }
        u8 c = (u8)(dp8(0xC6) - 1); dp8_set(0xC6, c); dc_set_flags_nz8(c);
    } while (!g_cpu._flag_N);
}
DC_ENTRY(CFEFAF, RTS)
{
    DC_ASSUME_M(CFEFAF, 1);
    DC_ASSUME_X(CFEFAF, 0);
    if (!ldm(0xA013)) {
        if (!ldm(0xA0FD)) sparkles();
        return;
    }
    run_movers(0x000A, 0xA337, 0xA2EB, 0xA2FE, 0xA86D, 0xF01E, 0xCFEFE6, mover1);
    run_movers(0x0007, 0xA342, 0xA2F6, 0xA309, 0xA878, 0xF02E, 0xCFF016, mover2);
}

/* ---- vblank DMA (CF:E300) ------------------------------------------------------- */
/* Long stores to the DMA channel 7 registers / PPU ports. */
static void io_sta(u16 r) { bus8_set(r, (u8)g_cpu.A); }
static void io_sta16(u16 r) { bus16_set(r, g_cpu.A); }
static void dma7_go(void) { lda8(0x80); io_sta(0x420B); }
/* Channel 7 mode / B-bus port, then the source address and bank. */
static void dma7_set(u8 mode, u8 port, u8 lo, u8 hi, u8 bank)
{
    const u8 v[5] = { mode, port, lo, hi, bank };
    for (int k = 0; k < 5; k++) { lda8(v[k]); io_sta((u16)(0x4370 + k)); }
}
/* One battle tile upload pair from the table CC:F957 (index x): source words
 * F957 / F95F (bank 7E), sizes F977 / F97F, VRAM addresses F967 / F96F; the second
 * transfer only when its size is not 0. */
static void tile_upload(u16 x)
{
    dma7_set(0x01, 0x18, bus8(0xCCF957 + x), bus8(0xCCF958 + x), 0x7E);
    dc_set_mx(0, 0);
    lda16(bus16(0xCCF977 + x)); io_sta16(0x4375);
    lda16(bus16(0xCCF967 + x)); io_sta16(0x2116);
    tdc();
    dc_set_mx(1, 0);
    dma7_go();
    lda8(bus8(0xCCF95F + x)); io_sta(0x4372);
    lda8(bus8(0xCCF960 + x)); io_sta(0x4373);
    dc_set_mx(0, 0);
    lda16(bus16(0xCCF97F + x));
    if (!g_cpu._flag_Z) {
        io_sta16(0x4375);
        lda16(bus16(0xCCF96F + x)); io_sta16(0x2116);
        tdc();
        dc_set_mx(1, 0);
        dma7_go();
        return;
    }
    tdc();
    dc_set_mx(1, 0);
}
/* CF:E300  Battle vblank transfers on DMA channel 7: the OAM buffer 0700 (220
 * bytes); the queued VRAM upload of the HDMA preset table 9EEA (dp A0 set);
 * the 7E:4000 block to VRAM 7A00 (99E2 set); the palette 2200 (200 bytes);
 * outside battle frames with dp E5 set one of 4 rotating tile uploads (dp A4);
 * on A012 = 1 the two portrait uploads of the slot 9679 (if 967C < 3); and when
 * the countdown A118 runs out the first tile upload again. */
DC_ENTRY(CFE300, RTL)
{
    DC_ASSUME_M(CFE300, 1);
    DC_ASSUME_X(CFE300, 0);
    tdc(); u8 d = (u8)g_cpu.A;
    dma7_set(d, 0x04, 0x00, 0x07, 0x00);
    lda8(0x20); io_sta(0x4375); lda8(0x02); io_sta(0x4376);
    tdc(); io_sta(0x2102);
    dma7_go();
    lda8(dp8(0xA0));
    if (!g_cpu._flag_Z) {
        for (int k = 0; k < 7; k++) { lda8(abs8(0x9EEA + 6 * k)); io_sta((u16)(0x4370 + k)); }
        lda8(abs8(0x9F14)); io_sta(0x2116);
        lda8(abs8(0x9F1A)); io_sta(0x2117);
        dma7_go();
        dp8_set(0xA0, 0);
    }
    if (ldm(0x99E2)) {
        dma7_set(0x01, 0x18, 0x40, 0x0B, 0x00);
        lda8(0x80); io_sta(0x4375); lda8(0x01); io_sta(0x4376);
        lda8(0x00); io_sta(0x2116); lda8(0x7A); io_sta(0x2117);
        dma7_go();
        abs8_set(0x99E2, 0);
    }
    dma7_set(0x00, 0x22, 0x00, 0x22, 0x7E);
    lda8(0x00); io_sta(0x4375); lda8(0x02); io_sta(0x4376);
    tdc(); io_sta(0x2121);
    dma7_go();
    if (!ldm(0xA013) && (lda8(dp8(0xE5)), !g_cpu._flag_Z)) {
        lda8(asl8(dp8(0xA4))); tax();
        tile_upload(g_cpu.X);
        u8 n = (u8)(dp8(0xA4) + 1); dp8_set(0xA4, n);
        lda8(n); dc_cmp8(n, 0x04);
        if (g_cpu._flag_Z) dp8_set(0xA4, 0);
    }
    lda8(abs8(0xA012));
    if (!g_cpu._flag_Z && (dc_cmp8((u8)g_cpu.A, 0x01), g_cpu._flag_Z)) {
        abs8_set(0xA012, 0);
        lda8(abs8(0x9679)); tax();
        lda8(abs8(0x967C + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0x03);
        if (!g_cpu._flag_C) {
            lda8(asl8((u8)g_cpu.X)); tax();
            u16 x = g_cpu.X;
            for (int k = 0; k < 2; k++) {
                u16 o = (u16)(x + 0x16 * k);
                if (k == 0) dma7_set(0x01, 0x18, abs8(0xA654 + o), abs8(0xA655 + o), 0x7E);
                else { lda8(abs8(0xA654 + o)); io_sta(0x4372); lda8(abs8(0xA655 + o)); io_sta(0x4373); }
                lda8(abs8(0xA6AC + o)); io_sta(0x4375); lda8(abs8(0xA6AD + o)); io_sta(0x4376);
                lda8(abs8(0xA680 + o)); io_sta(0x2116); lda8(abs8(0xA681 + o)); io_sta(0x2117);
                dma7_go();
            }
        }
    }
    if (ldm(0xA118) && !dec8_abs(0xA118)) tile_upload(0);
}
