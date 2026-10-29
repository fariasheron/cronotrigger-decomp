/* Bank D1: battle effect helpers called from the CD animation scripts (palette fades,
 * screen tables). Script pointer at dp 40 as in bank CD; 8-bit A, 16-bit indexes. */
#include "ops65.h"

/* D1:F692  Shake off: CCB0 = CCB8 = CCC0 = 0. */
DC_ENTRY(D1F692, RTS) { abs8_set(0xCCB0, 0); abs8_set(0xCCB8, 0); abs8_set(0xCCC0, 0); }
/* D1:FB68  CD39 += op 0. */
DC_ENTRY(D1FB68, RTL) { g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCD39), bus8(lp(0x40)))); abs8_set(0xCD39, (u8)g_cpu.A); }
/* D1:F474  A = C0:FE00[A + dp7C + carry] (random table). */
DC_ENTRY(D1F474, RTS) { lda8(dc_adc8((u8)g_cpu.A, dp8(0x7C))); tax(); lda8(bus8(0xC0FE00 + (u32)g_cpu.X)); }
/* D1:F5CD  Slot op 0's triangle CAEA..CAF4 = the three points at 5DA0..5DA5. */
DC_ENTRY(D1F5CD, RTL)
{
    static const u16 src[6] = { 0x5DA0, 0x5DA2, 0x5DA4, 0x5DA1, 0x5DA3, 0x5DA5 };
    static const u16 dst[6] = { 0xCAEA, 0xCAEE, 0xCAF2, 0xCAEC, 0xCAF0, 0xCAF4 };
    lda8(bus8(lp(0x40))); tax();
    for (int k = 0; k < 6; k++) { lda8(abs8(src[k])); abs8_set(dst[k] + g_cpu.X, (u8)g_cpu.A); }
}
/* D1:EBD0  A = dp45, or 0E - dp45 when CE10 is 0. */
DC_ENTRY(D1EBD0, RTS)
{
    lda8(abs8(0xCE10));
    if (!g_cpu._flag_Z) lda8(dp8(0x45));
    else { g_cpu._flag_C = 1; lda8(dc_sbc8(0x0E, dp8(0x45))); }
}
/* Colours a..a+n and the target copy a+200.. = white (7FFF), X from 0. */
static void white_fill(u16 a, u16 n)
{
    dc_set_mx(0, 0);
    lda16(0x7FFF);
    do {
        abs16_set(a + g_cpu.X, g_cpu.A); abs16_set(a + 0x200 + g_cpu.X, g_cpu.A);
        inx(); inx(); cpx(n);
    } while (!g_cpu._flag_Z);
}
/* D1:E8C1  Palette colours 0x60..0x8F (both copies) to white. */
DC_ENTRY(D1E8C1, RTL)
{
    tdc(); tax();
    white_fill(0x21A0, 0x0060);
    tdc(); dc_set_mx(1, 0);
}
/* D1:E899  Palette colours 0x20..0x7F and 0x90..0xBF (both copies) to white. */
DC_ENTRY(D1E899, RTL)
{
    tdc(); tax();
    white_fill(0x2040, 0x00C0);
    ldx(0x0000);
    white_fill(0x2120, 0x0060);
    tdc(); dc_set_mx(1, 0);
}
/* D1:F411  CD2F..CD36 = byte 0 of the eight 12-byte records at 0520. */
DC_ENTRY(D1F411, RTS)
{
    tdc(); tax(); tay();
    do {
        lda8(abs8(0x0520 + g_cpu.X)); abs8_set(0xCD2F + g_cpu.Y, (u8)g_cpu.A);
        lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x0C)); tax();
        iny(); cpy(0x0008);
    } while (!g_cpu._flag_Z);
}
/* D1:FE71  Start the hardware divide dp79..7A / dp7B (when 7E:A029 is set) or the
 * multiply dp77 * dp78. */
DC_ENTRY(D1FE71, RTL)
{
    lda8(bus8(0x7EA029));
    if (!g_cpu._flag_Z) {
        lda8(dp8(0x79)); bus8_set(0x004204, (u8)g_cpu.A);
        lda8(dp8(0x7A)); bus8_set(0x004205, (u8)g_cpu.A);
        lda8(dp8(0x7B)); bus8_set(0x004206, (u8)g_cpu.A);
    } else {
        lda8(dp8(0x77)); bus8_set(0x004202, (u8)g_cpu.A);
        lda8(dp8(0x78)); bus8_set(0x004203, (u8)g_cpu.A);
    }
}
/* D1:EBDF  Copy the 14 colours CDCC[A * 2..] to 20A2,Y and 22A2,Y (X kept). */
DC_ENTRY(D1EBDF, RTS)
{
    phx();
    dc_set_mx(0, 0);
    asl_a(1); tax();
    lda16(0x000E); dp16_set(0x45, 0x000E);
    do {
        lda16(abs16(0xCDCC + g_cpu.X));
        abs16_set(0x20A2 + g_cpu.Y, g_cpu.A); abs16_set(0x22A2 + g_cpu.Y, g_cpu.A);
        inx(); inx(); iny(); iny();
        u16 n = (u16)(dp16(0x45) - 1); dp16_set(0x45, n); dc_set_flags_nz16(n);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 0);
    plx();
}
/* D1:FE4C  When CA21 changed (CA22 = CA21) or CA24 is set: CE:EE6E with A = CA25 + CA22 + CCEC,
 * CA24 = 0. */
DC_ENTRY(D1FE4C, RTS)
{
    lda8(abs8(0xCA21)); dc_cmp8((u8)g_cpu.A, abs8(0xCA22));
    if (!g_cpu._flag_Z) abs8_set(0xCA22, (u8)g_cpu.A);
    else { lda8(abs8(0xCA24)); if (g_cpu._flag_Z) return; }
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCA25), abs8(0xCA22)));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0xCCEC)));
    dc_jsl(0xCEEE6E, 0xD1FE69);
    abs8_set(0xCA24, 0);
}
/* D1:EA01  CDC9 = 1 + index of the first colour 20A2.. (14) equal to D0:FBE2, or 0. */
DC_ENTRY(D1EA01, RTS)
{
    tdc(); tax(); tay();
    dc_set_mx(0, 0);
    for (;;) {
        lda16(bus16(0xD0FBE2)); dc_cmp16(g_cpu.A, abs16(0x20A2 + g_cpu.X));
        if (g_cpu._flag_Z) break;
        iny(); inx(); inx(); cpx(0x001C);
        if (g_cpu._flag_Z) { tdc(); tay(); break; }
    }
    tdc();
    dc_set_mx(1, 0);
    lda8((u8)g_cpu.Y); abs8_set(0xCDC9, (u8)g_cpu.A);
}
/* D1:EEA5  Sprite rows C15F + 4n = 0, 1, 2.. counting down from n = C0 - CD3A; CD3A += 1. */
DC_ENTRY(D1EEA5, RTL)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(0xC0, abs8(0xCD3A)));
    dc_set_mx(0, 0);
    asl_a(2); tay();
    if (!g_cpu._flag_N) {
        tdc();
        do {
            abs16_set(0xC15F + g_cpu.Y, g_cpu.A);
            g_cpu.A++;
            ldy((u16)(g_cpu.Y - 4));
        } while (!g_cpu._flag_N);
    }
    { u16 v = (u16)(abs16(0xCD3A) + 1); abs16_set(0xCD3A, v); dc_set_flags_nz16(v); }
    tdc();
    dc_set_mx(1, 0);
}
/* D1:F0E9  C15F,Y = C163,Y = dp51 + A (sign-extended when N is set on entry). */
DC_ENTRY(D1F0E9, RTS)
{
    bool neg = g_cpu._flag_N;
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    if (neg) lda16(g_cpu.A | 0xFF00);
    lda16(dc_adc16(g_cpu.A, dp16(0x51)));
    abs16_set(0xC15F + g_cpu.Y, g_cpu.A); abs16_set(0xC163 + g_cpu.Y, g_cpu.A);
    dc_set_mx(1, 0);
}
/* D1:E8D9  DMA 16 rows of 32 bytes from A:X to VRAM dp45, dp45 + 20, .. (channel 7, DB = 00). */
DC_ENTRY(D1E8D9, RTL)
{
    dc_push8(g_cpu.DB);
    dc_push8((u8)g_cpu.A);
    lda8(0x00); dc_push8(0x00); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    lda8(dc_pull8());
    stx_abs(0x4372); abs8_set(0x4374, (u8)g_cpu.A);
    lda8(0x01); abs8_set(0x4370, 0x01);
    lda8(0x18); abs8_set(0x4371, 0x18);
    abs8_set(0x4376, 0);
    dc_set_mx(0, 1);
    ldy(0x20); ldx(0x80);
    lda16(0x0010); dp16_set(0x47, 0x0010);
    lda16(dp16(0x45));
    do {
        abs16_set(0x2116, g_cpu.A);
        sty_abs(0x4375);
        stx_abs(0x420B);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0020));
        u16 n = (u16)(dp16(0x47) - 1); dp16_set(0x47, n); dc_set_flags_nz16(n);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 0);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}
/* D1:F8EB  Sort the four points dp12..19 by y (4 bubble passes, 16-bit swaps). */
DC_ENTRY(D1F8EB, RTS)
{
    lda8(0x04);
    do {
        dc_push8((u8)g_cpu.A);
        for (u8 o = 0x12; o < 0x18; o += 2) {
            lda8(dp8((u8)(o + 1))); dc_cmp8((u8)g_cpu.A, dp8((u8)(o + 3)));
            if (g_cpu._flag_C) { ldx(dp16(o)); ldy(dp16((u8)(o + 2))); dp16_set((u8)(o + 2), g_cpu.X); dp16_set(o, g_cpu.Y); }
        }
        lda8(dc_pull8());
        lda8((u8)(g_cpu.A - 1));
    } while (!g_cpu._flag_Z);
}
/* D1:F972  Y = (dp0B - dp0C) * 256 / dp0D, signed (the low byte of the dividend is B). */
DC_ENTRY(D1F972, RTS)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(0x0B), dp8(0x0C)));
    bool neg = !g_cpu._flag_C;
    if (neg) lda8((u8)((u8)~g_cpu.A + 1));
    g_cpu.A = (u16)(g_cpu.A >> 8 | g_cpu.A << 8); dc_set_flags_nz8((u8)g_cpu.A);
    tay(); sty_abs(0x4204);
    lda8(dp8(0x0D)); abs8_set(0x4206, (u8)g_cpu.A);
    tdc();
    if (!neg) { ldy(abs16(0x4214)); return; }
    dc_set_mx(0, 0);
    lda16((u16)(~abs16(0x4214) + 1)); tay();
    tdc();
    dc_set_mx(1, 0);
}
/* D1:F47C  Camera jitter: dp45 = op 0 + (random & op 2), dp47 = op 1 + (random & op 2);
 * slot dp43's CA5A / CA5C = -dp45 / -dp47; skip 2. */
DC_ENTRY(D1F47C, RTL)
{
    ldy(0x0002);
    lda8(bus8(lp(0x40) + g_cpu.Y)); dp8_set(0x49, (u8)g_cpu.A);
    dc_D1F474_glue();
    lda8((u8)g_cpu.A & dp8(0x49));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, bus8(lp(0x40)))); dp8_set(0x45, (u8)g_cpu.A);
    ldy(0x0001);
    dc_D1F474_glue();
    lda8((u8)g_cpu.A & dp8(0x49));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, bus8(lp(0x40) + g_cpu.Y))); dp8_set(0x47, (u8)g_cpu.A);
    ldx(dp16(0x43));
    dc_set_mx(0, 0);
    lda16((u16)(~(dp16(0x45) & 0x00FF) + 1)); abs16_set(0xCA5A + g_cpu.X, g_cpu.A);
    lda16((u16)(~(dp16(0x47) & 0x00FF) + 1)); abs16_set(0xCA5C + g_cpu.X, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    ldy(dp16(0x40)); iny(); iny(); dp16_set(0x40, g_cpu.Y);
}
/* Add n to the byte at each of the three 12-byte party records' offset a. */
static void party_add3(u16 a, u8 n, bool sub)
{
    for (int k = 0; k < 3; k++) {
        u16 p = (u16)(a + 12 * k);
        g_cpu._flag_C = sub; lda8(sub ? dc_sbc8(abs8(p), n) : dc_adc8(abs8(p), n)); abs8_set(p, (u8)g_cpu.A);
    }
}
/* D1:E91A  Undo D1:E984: palette 2040.. (0x30 colours) moves to 20A0 (both copies) and is
 * cleared; the three party records 0544.. move back to 0520 with y + 30; CD2F..31 = 30,
 * CD32..34 = 0, CDC8 = 0, CE12 += 1. */
DC_ENTRY(D1E91A, RTL)
{
    dc_set_mx(0, 0);
    tdc(); tax();
    do {
        lda16(abs16(0x2040 + g_cpu.X));
        abs16_set(0x20A0 + g_cpu.X, g_cpu.A); abs16_set(0x22A0 + g_cpu.X, g_cpu.A);
        abs16_set(0x2040 + g_cpu.X, 0); abs16_set(0x2240 + g_cpu.X, 0);
        inx(); inx(); cpx(0x0060);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 0);
    tdc(); tax();
    do { lda8(abs8(0x0544 + g_cpu.X)); abs8_set(0x0520 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0024); } while (!g_cpu._flag_Z);
    party_add3(0x0521, 0x30, false);
    lda8(0x30);
    static const u16 d30[6] = { 0x0520, 0x052C, 0x0538, 0xCD2F, 0xCD30, 0xCD31 };
    sta_all(d30, 6);
    abs8_set(0xCD32, 0); abs8_set(0xCD33, 0); abs8_set(0xCD34, 0); abs8_set(0xCDC8, 0);
    inc_abs(0xCE12);
}
/* D1:E984  CDC8 += 1, CE0F += 1; palette 2040.. (both copies) = D0:FD00 (0x30 colours); the three
 * party records 0520.. copy to 0544 with y - 30 and word 8 = 0100; 0544/0550/055C and CD32..34 = 30;
 * CE12 += 1. */
DC_ENTRY(D1E984, RTL)
{
    inc_abs(0xCDC8); inc_abs(0xCE0F);
    dc_set_mx(0, 0);
    tdc(); tax();
    do {
        lda16(bus16(0xD0FD00 + (u32)g_cpu.X));
        abs16_set(0x2040 + g_cpu.X, g_cpu.A); abs16_set(0x2240 + g_cpu.X, g_cpu.A);
        inx(); inx(); cpx(0x0060);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 0);
    tdc(); tax();
    do { lda8(abs8(0x0520 + g_cpu.X)); abs8_set(0x0544 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0024); } while (!g_cpu._flag_Z);
    party_add3(0x0545, 0x30, true);
    ldx(0x0100); stx_abs(0x0548); stx_abs(0x0554); stx_abs(0x0560);
    lda8(0x30);
    static const u16 d30[6] = { 0x0544, 0x0550, 0x055C, 0xCD32, 0xCD33, 0xCD34 };
    sta_all(d30, 6);
    inc_abs(0xCE12);
}
/* D1:E70A  A101 = 0 (2A21 bit 1 also flips 0575 bit 6); swap palette rows 2320/2340/2360 with
 * 23A0/23C0/23E0 and 2120/2140/2160 with 21A0/21C0/21E0 (16 colours each); dp40 -= 1. */
DC_ENTRY(D1E70A, RTL)
{
    abs8_set(0xA101, 0);
    lda8(abs8(0x2A21) & 0x02);
    if (!g_cpu._flag_Z) { lda8(abs8(0x0575) ^ 0x40); abs8_set(0x0575, (u8)g_cpu.A); }
    dc_set_mx(0, 0);
    static const u16 rows[6] = { 0x2320, 0x2340, 0x2360, 0x2120, 0x2140, 0x2160 };
    ldx(0x0000);
    do {
        for (int k = 0; k < 6; k++) {
            ldy(abs16(rows[k] + g_cpu.X));
            lda16(abs16(rows[k] + 0x80 + g_cpu.X)); abs16_set(rows[k] + g_cpu.X, g_cpu.A);
            lda16(g_cpu.Y); abs16_set(rows[k] + 0x80 + g_cpu.X, g_cpu.A);
        }
        inx(); inx(); cpx(0x0020);
    } while (!g_cpu._flag_Z);
    { u16 v = (u16)(dp16(0x40) - 1); dp16_set(0x40, v); dc_set_flags_nz16(v); }
    tdc();
    dc_set_mx(1, 0);
}
/* Fill the window line table of this frame's buffer (dp7C bit 0: odd at `odd`, else `even`)
 * with the word val: n blocks of `stride` bytes, X from `stride` down to xstep in xstep steps. */
static void win_fill(u16 even, u16 odd, u16 val, u16 stride, int n, int xstep)
{
    lda8(dp8(0x7C) & 0x01);
    u16 base = g_cpu._flag_Z ? even : odd;
    dc_set_mx(0, 1);
    ldx(stride); lda16(val);
    do {
        for (int k = 0; k < n; k++) abs16_set((u16)(base + stride * k) + g_cpu.X, g_cpu.A);
        for (int k = 0; k < xstep; k++) dex();
    } while (!g_cpu._flag_Z);
    dc_set_mx(0, 0);
    dc_set_mx(1, 0);
}
/* D1:FCF4  Both window edges of all 0xE0 lines to (FF, 00): window off. */
DC_ENTRY(D1FCF4, RTS) { win_fill(0xC15F, 0xC4DF, 0x00FF, 0x0038, 16, 2); }
/* D1:FBDA / FC38 / FC96  One window pair of every line to (00, FF) / (FF, 00) / second pair (FF, 00). */
DC_ENTRY(D1FBDA, RTS) { win_fill(0xC15D, 0xC4DD, 0xFF00, 0x0070, 8, 4); }
DC_ENTRY(D1FC38, RTS) { win_fill(0xC15D, 0xC4DD, 0x00FF, 0x0070, 8, 4); }
DC_ENTRY(D1FC96, RTS) { win_fill(0xC15F, 0xC4DF, 0x00FF, 0x0070, 8, 4); }
/* D1:F5F5  Copy the second window pair of all lines from the other frame's buffer into this
 * one (offset by one pair). */
DC_ENTRY(D1F5F5, RTL)
{
    tdc(); tax();
    dc_set_mx(0, 1);
    lda16(dp16(0x7C) & 0x0001);
    u16 src = g_cpu._flag_Z ? 0xC4E1 : 0xC161, dst = g_cpu._flag_Z ? 0xC163 : 0xC4E3;
    do {
        for (int k = 0; k < 8; k++) { lda16(abs16(src + 0x70 * k + g_cpu.X)); abs16_set(dst + 0x70 * k + g_cpu.X, g_cpu.A); }
        inx(); inx(); inx(); inx(); cpx(0x70);
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 1);
    dc_set_mx(1, 0);
}
/* dp0B = dp[x1], dp0C = dp[x0], dp0D = dp[y1] - dp[y0]; D1:F972 slope into dp[out]. */
static void quad_slope(u8 y1, u8 y0, u8 x1, u8 x0, u8 out)
{
    g_cpu._flag_C = 1; lda8(dc_sbc8(dp8(y1), dp8(y0))); dp8_set(0x0D, (u8)g_cpu.A);
    lda8(dp8(x1)); dp8_set(0x0B, (u8)g_cpu.A);
    lda8(dp8(x0)); dp8_set(0x0C, (u8)g_cpu.A);
    dc_D1F972_glue(); dp16_set(out, g_cpu.Y);
}
/* D1:F91D  Edge slopes of the sorted quad dp12..19: 0->2 (dp03), 2->3 (dp05), 0->1 (dp07),
 * 1->3 (dp09); dp0F = top line. */
DC_ENTRY(D1F91D, RTS)
{
    lda8(dp8(0x13)); dp8_set(0x0F, (u8)g_cpu.A);
    quad_slope(0x17, 0x13, 0x16, 0x12, 0x03);
    quad_slope(0x19, 0x17, 0x18, 0x16, 0x05);
    quad_slope(0x15, 0x13, 0x14, 0x12, 0x07);
    quad_slope(0x19, 0x15, 0x18, 0x14, 0x09);
}
/* D1:FB72  Horizontal band: CD38 += op 0; lines 6A - CD38 .. 6A + CD38 of this frame's
 * window table get the pair (CD39, ~CD39). */
DC_ENTRY(D1FB72, RTL)
{
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCD38), bus8(lp(0x40)))); abs8_set(0xCD38, (u8)g_cpu.A);
    lda8(abs8(0xCD39)); dp8_set(0x49, (u8)g_cpu.A);
    lda8((u8)~g_cpu.A); dp8_set(0x4A, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCD38), 0x6A)); dp8_set(0x45, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(0x6A, abs8(0xCD38)));
    dc_set_mx(0, 0);
    asl_a(2); tax();
    lda16(dp16(0x45) & 0x00FF); asl_a(2); dp16_set(0x45, g_cpu.A);
    cpx(dp16(0x45));
    if (!g_cpu._flag_Z) {
        lda16(dp16(0x7C) & 0x0001);
        u16 t = g_cpu._flag_Z ? 0xC161 : 0xC4E1;
        lda16(dp16(0x49));
        do { abs16_set(t + g_cpu.X, g_cpu.A); inx(); inx(); inx(); inx(); cpx(dp16(0x45)); } while (!g_cpu._flag_Z);
    }
    tdc();
    dc_set_mx(1, 0);
}
/* D1:F426  When CFFF is set (once, CE12 = 1): save the eight record bytes 0520 (D1:F411) and
 * clear those that are negative or 1x. When CFFF is clear (once, CE12 = 0): restore them. */
DC_ENTRY(D1F426, RTL)
{
    lda8(abs8(0xCFFF));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xCE12));
        if (!g_cpu._flag_Z) return;
        inc_abs(0xCE12);
        dc_D1F411_glue();
        tdc(); tax();
        do {
            lda8(abs8(0x0520 + g_cpu.X));
            bool clear = g_cpu._flag_N;
            if (!clear) { lda8((u8)g_cpu.A & 0xF0); dc_cmp8((u8)g_cpu.A, 0x10); clear = g_cpu._flag_Z; }
            if (clear) abs8_set(0x0520 + g_cpu.X, 0);
            lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x0C)); tax();
            cpx(0x0060);
        } while (!g_cpu._flag_Z);
    } else {
        lda8(abs8(0xCE12));
        if (g_cpu._flag_Z) return;
        abs8_set(0xCE12, 0);
        tdc(); tax(); tay();
        do {
            lda8(abs8(0xCD2F + g_cpu.Y)); abs8_set(0x0520 + g_cpu.X, (u8)g_cpu.A);
            lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x0C)); tax();
            iny(); cpy(0x0008);
        } while (!g_cpu._flag_Z);
    }
}
/* One shake axis from dp[d], dp[d + 1]: base+1 = D1:F69C[bits 2-4], base+3 = D1:F6A4[bits 5-7]
 * (low bytes 0), base+5 / base+4 = low / high nibble of dp[d + 1]. */
static void shake_axis(u8 d, u16 base)
{
    lda8(dp8(d) & 0x1C); lsr_a(2); tax();
    lda8(bus8(0xD1F69C + (u32)g_cpu.X)); abs8_set(base + 1, (u8)g_cpu.A); abs8_set(base, 0);
    lda8(dp8(d) & 0xE0); lsr_a(5); tax();
    lda8(bus8(0xD1F6A4 + (u32)g_cpu.X)); abs8_set(base + 3, (u8)g_cpu.A); abs8_set(base + 2, 0);
    lda8(dp8(d + 1) & 0x0F); abs8_set(base + 5, (u8)g_cpu.A);
    lda8(dp8(d + 1) & 0xF0); lsr_a(4); abs8_set(base + 4, (u8)g_cpu.A);
}
/* D1:F6B4  Shake setup from dp54..57: axes CCB2.. and CCBA.., modes CCB0 / CCB8 = low 2 bits
 * of dp54 / dp56, CCC0 = D1:F6AC[dp54 & 3] | D1:F6B0[dp56 & 3] (also in dp5B). */
DC_ENTRY(D1F6B4, RTS)
{
    dc_D1F692_glue();
    shake_axis(0x54, 0xCCB2);
    shake_axis(0x56, 0xCCBA);
    lda8(dp8(0x54) & 0x03); tax(); lda8(bus8(0xD1F6AC + (u32)g_cpu.X)); dp8_set(0x5B, (u8)g_cpu.A);
    lda8(dp8(0x56) & 0x03); tax(); lda8(bus8(0xD1F6B0 + (u32)g_cpu.X) | dp8(0x5B)); abs8_set(0xCCC0, (u8)g_cpu.A);
    lda8(dp8(0x54) & 0x03); abs8_set(0xCCB0, (u8)g_cpu.A);
    lda8(dp8(0x56) & 0x03); abs8_set(0xCCB8, (u8)g_cpu.A);
}
/* Window row Y: C15D,Y = C161,Y = A + (E500,Y or dp4F), A sign-extended when N is set on
 * entry; start the multiply CE:F48E[dp45] * dp4D; carry = dp45 negative. */
static void row_add_mul(bool from_e500)
{
    bool neg = g_cpu._flag_N;
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    if (neg) lda16(g_cpu.A | 0xFF00);
    lda16(dc_adc16(g_cpu.A, from_e500 ? abs16(0xE500 + g_cpu.Y) : dp16(0x4F)));
    abs16_set(0xC15D + g_cpu.Y, g_cpu.A); abs16_set(0xC161 + g_cpu.Y, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    lda8(dp8(0x45));
    bool n = g_cpu._flag_N;
    tax();
    lda8(bus8(0xCEF48E + (u32)g_cpu.X)); bus8_set(0x004202, (u8)g_cpu.A);
    lda8(dp8(0x4D)); bus8_set(0x004203, (u8)g_cpu.A);
    g_cpu._flag_C = n;
}
/* D1:EF63 / F07E  Row add from E500,Y / from dp4F. */
DC_ENTRY(D1EF63, RTS) { row_add_mul(true); }
DC_ENTRY(D1F07E, RTS) { row_add_mul(false); }
/* D1:F559  Accelerated move of slot dp43: speed CA7A grows by CA7C (until it overflows, then
 * CA52 = 0) or shrinks (until it underflows, then CA52 += 1), 32-bit position CA7E..81
 * follows it, CA60 = CA54 -/+ position (minus for slot 10). */
DC_ENTRY(D1F559, RTL)
{
    ldx(dp16(0x43));
    u16 x = g_cpu.X;
    lda8(abs8(0xCA52 + x));
    bool up = !g_cpu._flag_Z;
    dc_set_mx(0, 0);
    if (up) {
        lda16(abs16(0xCA7A + x)); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCA7C + x)));
        if (g_cpu._flag_C) abs16_set(0xCA52 + x, 0); else abs16_set(0xCA7A + x, g_cpu.A);
        lda16(abs16(0xCA7E + x)); g_cpu._flag_C = 1; lda16(dc_sbc16(g_cpu.A, abs16(0xCA7A + x))); abs16_set(0xCA7E + x, g_cpu.A);
        lda16(dc_sbc16(abs16(0xCA80 + x), 0x0000)); abs16_set(0xCA80 + x, g_cpu.A);
    } else {
        lda16(abs16(0xCA7A + x)); g_cpu._flag_C = 1; lda16(dc_sbc16(g_cpu.A, abs16(0xCA7C + x)));
        if (!g_cpu._flag_C) { u16 v = (u16)(abs16(0xCA52 + x) + 1); abs16_set(0xCA52 + x, v); dc_set_flags_nz16(v); }
        else abs16_set(0xCA7A + x, g_cpu.A);
        lda16(abs16(0xCA7E + x)); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCA7A + x))); abs16_set(0xCA7E + x, g_cpu.A);
        lda16(dc_adc16(abs16(0xCA80 + x), 0x0000)); abs16_set(0xCA80 + x, g_cpu.A);
    }
    cpx(0x0010);
    lda16(abs16(0xCA54 + x));
    if (g_cpu.X == 0x0010) { g_cpu._flag_C = 1; lda16(dc_sbc16(g_cpu.A, abs16(0xCA80 + x))); }
    else { g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCA80 + x))); }
    abs16_set(0xCA60 + x, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
}
/* n rows of HDMA table pairs at a / b (step 4): a = (01, dp54), b = (81, dp56); the words
 * dp53 -= step, dp55 += step after each row. */
static void hdma_ramp(u16 a, u16 b, u16 step, u16 end)
{
    tdc(); tax();
    do {
        lda8(dp8(0x54)); abs8_set(a + 1 + g_cpu.X, (u8)g_cpu.A);
        lda8(dp8(0x56)); abs8_set(b + 1 + g_cpu.X, (u8)g_cpu.A);
        lda8(0x81); abs8_set(b + g_cpu.X, 0x81);
        lda8(0x01); abs8_set(a + g_cpu.X, 0x01);
        dc_set_mx(0, 0);
        g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x53), step)); dp16_set(0x53, g_cpu.A);
        g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x55), step)); dp16_set(0x55, g_cpu.A);
        tdc();
        dc_set_mx(1, 0);
        inx(); inx(); inx(); inx(); cpx(end);
    } while (!g_cpu._flag_Z);
}
/* D1:F83D  Build the two HDMA tables C02F.. (0x120 bytes cleared): 24 rows from E8AB / E055
 * by 55, then 12 rows from E856 / E0AA by AA; ends E0 at C0ED / C14D. */
DC_ENTRY(D1F83D, RTL)
{
    lda8(0xE8); dp8_set(0x54, 0xE8);
    lda8(0xE0); dp8_set(0x56, 0xE0);
    lda8(0xAB); dp8_set(0x53, 0xAB);
    lda8(0x55); dp8_set(0x55, 0x55);
    tdc(); tax();
    do { abs8_set(0xC02F + g_cpu.X, 0); inx(); cpx(0x0120); } while (!g_cpu._flag_Z);
    hdma_ramp(0xC030, 0xC090, 0x0055, 0x0060);
    lda8(0xE8); dp8_set(0x54, 0xE8);
    lda8(0xE0); dp8_set(0x56, 0xE0);
    lda8(0x56); dp8_set(0x53, 0x56);
    lda8(0xAA); dp8_set(0x55, 0xAA);
    hdma_ramp(0xC0F0, 0xC120, 0x00AA, 0x0030);
    lda8(0xE0); abs8_set(0xC0ED, 0xE0); abs8_set(0xC14D, 0xE0);
    abs8_set(0xC0EB, 0); abs8_set(0xC0EC, 0); abs8_set(0xC0EE, 0);
    abs8_set(0xC14B, 0); abs8_set(0xC14C, 0); abs8_set(0xC14E, 0);
}
/* D1:FBCE  Window fill picked by X: FCF4, FC38, FC96, FBDA. */
DC_ENTRY(D1FBCE, RTL)
{
    static const glue_fn t[4] = { dc_D1FCF4_glue, dc_D1FC38_glue, dc_D1FC96_glue, dc_D1FBDA_glue };
    jump_table(t, 4, 0xFBD2, 0xD1FBCE);
}
/* D1:FD7E  Mirror the first window pair of lines (op 0 * 2) / 4 .. 0x1C into the second pair
 * of the other frame's buffer, inverted and byte-swapped (8-bit X). */
DC_ENTRY(D1FD7E, RTL)
{
    lda8(bus8(lp(0x40))); asl_a(1); tax();
    dc_set_mx(0, 1);
    lda16(g_cpu.X); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0070)); dp16_set(0x45, g_cpu.A);
    lda16(dp16(0x7C) & 0x0001);
    u16 src = g_cpu._flag_Z ? 0xC4E1 : 0xC161, dst = g_cpu._flag_Z ? 0xC163 : 0xC4E3;
    do {
        for (int k = 0; k < 8; k++) {
            u16 v = (u16)~abs16(src + 0x70 * k + g_cpu.X);
            lda16((u16)(v << 8 | v >> 8)); dc_set_flags_nz8((u8)g_cpu.A);
            abs16_set(dst + 0x70 * k + g_cpu.X, g_cpu.A);
        }
        inx(); inx(); inx(); inx(); cpx(dp8(0x45));
    } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 1);
    dc_set_mx(1, 0);
}
/* CD3A += op 0; dp4B = 60 - CD3A, dp4D = dp4B (halved when half). */
static void ring_size(bool half)
{
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xCD3A), bus8(lp(0x40)))); abs8_set(0xCD3A, (u8)g_cpu.A);
    g_cpu._flag_C = 1; lda8(dc_sbc8(0x60, abs8(0xCD3A))); dp8_set(0x4B, (u8)g_cpu.A);
    if (half) lsr_a(1);
    dp8_set(0x4D, (u8)g_cpu.A);
}
/* 106 window rows, Y step 8: centre dp4F = CA5A + CA5E, dp51 = CA5C + CA60; each row gets
 * x = sin(dp47) * dp4B / 256 (row_add: D1:EF63 or F07E, which also starts the next product)
 * and D1:F0E9 with the other product, angles dp45 / dp47 stepping by 1. */
static void ring_rows(void (*row_add)(void))
{
    dc_set_mx(0, 0);
    lda16(abs16(0xCA5A)); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCA5E))); dp16_set(0x4F, g_cpu.A);
    lda16(abs16(0xCA5C)); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xCA60))); dp16_set(0x51, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    tdc(); tay();
    do {
        lda8(dp8(0x47));
        bool neg = g_cpu._flag_N;
        tax();
        lda8(bus8(0xCEF48E + (u32)g_cpu.X)); bus8_set(0x004202, (u8)g_cpu.A);
        lda8(dp8(0x4B)); bus8_set(0x004203, (u8)g_cpu.A);
        lda8(bus8(0x004217));
        if (neg) lda8((u8)-(u8)g_cpu.A);
        row_add();
        bool neg2 = g_cpu._flag_C;
        lda8(bus8(0x004217));
        if (neg2) lda8((u8)-(u8)g_cpu.A);
        dc_D1F0E9_glue();
        { u8 v = (u8)(dp8(0x45) + 1); dp8_set(0x45, v); }
        { u8 v = (u8)(dp8(0x47) + 1); dp8_set(0x47, v); dc_set_flags_nz8(v); }
        dc_set_mx(0, 0);
        lda16(g_cpu.Y); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0008)); tay();
        tdc();
        dc_set_mx(1, 0);
        cpy(0x0350);
    } while (!g_cpu._flag_Z);
}
/* D1:EEC4  Ring (radius grows by op 0) from angles dp7C * 4 / dp7C * 2, rows from E500. */
DC_ENTRY(D1EEC4, RTL)
{
    ring_size(true);
    lda8(dp8(0x7C)); asl_a(2); tax(); stx_dp(0x45);
    lda8(dp8(0x7C)); asl_a(1); tax(); stx_dp(0x47);
    ring_rows(dc_D1EF63_glue);
}
/* D1:EFE3  Ring from angles dp7C * 2 / dp7C * 4, rows from dp4F. */
DC_ENTRY(D1EFE3, RTL)
{
    lda8(dp8(0x7C)); asl_a(1); tax(); stx_dp(0x47);
    asl_a(1); tax(); stx_dp(0x45);
    ring_size(false);
    ring_rows(dc_D1F07E_glue);
}
/* D1:EFD0  As EFE3 from angle (CD3B + A8) * 2; CD3B += 1. */
DC_ENTRY(D1EFD0, RTL)
{
    lda8(abs8(0xCD3B)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0xA8));
    asl_a(1); tax(); stx_dp(0x47);
    asl_a(1); tax(); stx_dp(0x45);
    inc_abs(0xCD3B);
    ring_size(false);
    ring_rows(dc_D1F07E_glue);
}
/* D1:F74D  Mode 7 style tables from dp53 / dp54: CCC2.. / CCCE.. = 8 bytes of CE:F0E6 / F0EE
 * row dp53 >> 4, flags CCC1 / CCCD, counts and steps from CE:F126 / F146, signs CCC8/9 and
 * CCD4/5 forced (bit 7 of dp54) and swapped (bit 6); CCE5 += 1. */
DC_ENTRY(D1F74D, RTS)
{
    lda8(dp8(0x53) & 0xF0); tax();
    tdc(); tay();
    do {
        lda8(bus8(0xCEF0E6 + (u32)g_cpu.X)); abs8_set(0xCCC2 + g_cpu.Y, (u8)g_cpu.A);
        lda8(bus8(0xCEF0EE + (u32)g_cpu.X)); abs8_set(0xCCCE + g_cpu.Y, (u8)g_cpu.A);
        inx(); iny(); cpy(0x0008);
    } while (!g_cpu._flag_Z);
    lda8(dp8(0x54) & 0x40);
    if (!g_cpu._flag_Z) lda8(0x10);
    lda8((u8)g_cpu.A | 0x40); abs8_set(0xCCC1, (u8)g_cpu.A); abs8_set(0xCCCD, (u8)g_cpu.A);
    lda8(dp8(0x54) & 0x07); abs8_set(0xCCC7, (u8)g_cpu.A); abs8_set(0xCCD3, (u8)g_cpu.A);
    lda8(dp8(0x54) & 0x38); lsr_a(1); tax();
    lda8(bus8(0xCEF126 + (u32)g_cpu.X)); abs8_set(0xCCC5, (u8)g_cpu.A);
    lda8(bus8(0xCEF127 + (u32)g_cpu.X)); abs8_set(0xCCC6, (u8)g_cpu.A);
    lda8(bus8(0xCEF128 + (u32)g_cpu.X)); abs8_set(0xCCD1, (u8)g_cpu.A);
    lda8(bus8(0xCEF129 + (u32)g_cpu.X)); abs8_set(0xCCD2, (u8)g_cpu.A);
    lda8(dp8(0x54) & 0x38); lsr_a(2); tax();
    lda8(bus8(0xCEF146 + (u32)g_cpu.X)); abs8_set(0xCCC4, (u8)g_cpu.A);
    lda8(bus8(0xCEF147 + (u32)g_cpu.X)); abs8_set(0xCCD0, (u8)g_cpu.A);
    static const u16 sign[4] = { 0xCCC8, 0xCCC9, 0xCCD4, 0xCCD5 };
    lda8(dp8(0x54));
    bool neg = g_cpu._flag_N, swap = (dp8(0x54) & 0x40) != 0;
    if (neg) {
        for (int k = 0; k < 4; k++) {
            lda8(abs8(sign[k]));
            if (!swap && !g_cpu._flag_Z) lda8(0x80);
            if (swap && g_cpu._flag_Z) lda8((u8)g_cpu.A | 0x80);
            abs8_set(sign[k], (u8)g_cpu.A);
        }
    }
    if (!neg || swap) lda8(dp8(0x54) & 0x40);
    if (swap) {
        lda8(abs8(0xCCC9)); u8 t = abs8(0xCCC8); abs8_set(0xCCC8, (u8)g_cpu.A); lda8(t); abs8_set(0xCCC9, t);
        lda8(abs8(0xCCD5)); t = abs8(0xCCD4); abs8_set(0xCCD4, (u8)g_cpu.A); lda8(t); abs8_set(0xCCD5, t);
    }
    inc_abs(0xCCE5);
}
/* Add the 64 bytes dp00.. (D = 0A00) to bytes 0-1 and dp[off].. to bytes 2-3 of the 4-byte
 * rows at a and b, carry running through the whole loop (no CLC). */
static void wobble_rows(u16 a, u16 b, u8 off)
{
    do {
        for (int k = 0; k < 8; k++) {
            u16 t = (u16)((k < 4 ? a : b) + (k & 3) + g_cpu.Y);
            u8 o = (k & 2) ? off : 0;
            lda8(dc_adc8(abs8(t), b0_8((u16)(g_cpu.D + o + g_cpu.X)))); abs8_set(t, (u8)g_cpu.A);
        }
        inx(); iny(); iny(); iny(); iny();
    } while (!g_cpu._flag_Z);
}
/* D1:F108  Add the wobble offsets at 0A00.. to the window rows of this frame's buffer:
 * C5A1 / C6A1 (offsets +40) on odd frames, C221 / C321 (+20) on even ones. */
DC_ENTRY(D1F108, RTL)
{
    u16 d = g_cpu.D;
    g_cpu.D = 0x0A00; dc_set_flags_nz16(g_cpu.D);
    dc_set_mx(1, 1);
    ldy(0x00); ldx(0x00);
    lda8(abs8(0x007C) & 0x01);
    if (!g_cpu._flag_Z) wobble_rows(0xC5A1, 0xC6A1, 0x40);
    else wobble_rows(0xC221, 0xC321, 0x20);
    g_cpu.D = d; dc_set_flags_nz16(d);
    dc_set_mx(1, 0);
}
/* D1:EDCD  Window opening around point 5DA0/5DA1 growing by one per call. Op 0 = 0: the
 * columns 5DA0 -/+ CD3A (clamped 00..FF, CD3A up to 8) on all 0x35 lines of four tables;
 * else the lines 5DA1 -/+ CD3B (clamped 0..D4, CD3B up to 9) get the second pair (00, FF). */
DC_ENTRY(D1EDCD, RTL)
{
    lda8(bus8(lp(0x40)));
    if (g_cpu._flag_Z) {
        lda8(abs8(0x5DA0)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, abs8(0xCD3A)));
        if (!g_cpu._flag_C) tdc();
        dp8_set(0x45, (u8)g_cpu.A);
        lda8(abs8(0x5DA0)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0xCD3A)));
        if (g_cpu._flag_C) lda8(0xFF);
        dp8_set(0x46, (u8)g_cpu.A);
        lda8(abs8(0xCD3A)); dc_cmp8((u8)g_cpu.A, 0x08);
        if (!g_cpu._flag_C) inc_abs(0xCD3A);
        tdc(); tax();
        dc_set_mx(0, 0);
        lda16(dp16(0x7C) & 0x0001);
        u16 t = g_cpu._flag_Z ? 0xC161 : 0xC4E1;
        lda16(dp16(0x45));
        do {
            for (int k = 0; k < 4; k++) abs16_set(t + 0xD4 * k + g_cpu.X, g_cpu.A);
            inx(); inx(); inx(); inx(); cpx(0x00D4);
        } while (!g_cpu._flag_Z);
    } else {
        lda8(abs8(0x5DA1)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, abs8(0xCD3B)));
        if (!g_cpu._flag_C) tdc();
        dp8_set(0x45, (u8)g_cpu.A);
        lda8(abs8(0x5DA1)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0xCD3B)));
        if (g_cpu._flag_C) lda8(0xD4);
        else { dc_cmp8((u8)g_cpu.A, 0xD4); if (g_cpu._flag_C) lda8(0xD4); }
        dp8_set(0x46, (u8)g_cpu.A);
        lda8(abs8(0xCD3B)); dc_cmp8((u8)g_cpu.A, 0x09);
        if (!g_cpu._flag_C) inc_abs(0xCD3B);
        tdc(); tax();
        lda8(dp8(0x45));
        dc_set_mx(0, 0);
        asl_a(2); tax();
        lda16(dp16(0x46) & 0x00FF); asl_a(2); dp16_set(0x45, g_cpu.A);
        cpx(dp16(0x45));
        if (!g_cpu._flag_Z) {
            lda16(dp16(0x7C) & 0x0001);
            u16 t = g_cpu._flag_Z ? 0xC163 : 0xC4E3;
            lda16(0xFF00);
            do { abs16_set(t + g_cpu.X, g_cpu.A); inx(); inx(); inx(); inx(); cpx(dp16(0x45)); } while (!g_cpu._flag_Z);
        }
    }
    tdc();
    dc_set_mx(1, 0);
}
/* Swap the words dp[a] and dp[b] (through X / Y) when dp[a] >= dp[b] (low bytes). */
static void sort_x(u8 a, u8 b)
{
    lda8(dp8(a)); dc_cmp8((u8)g_cpu.A, dp8(b));
    if (g_cpu._flag_C) { ldx(dp16(a)); ldy(dp16(b)); dp16_set(b, g_cpu.X); dp16_set(a, g_cpu.Y); }
}
/* dp[cnt] = dp[hi] - dp[lo] (with B from the index, as TAX / STX do). */
static void span_count(u8 cnt, u8 hi, u8 lo)
{
    lda8(dp8(hi)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(lo))); tax(); stx_dp(cnt);
}
/* dp[cnt] lines of the window port: X += dp[ax], Y += dp[ay] (carry runs on), both written to
 * the port, then two port reads. */
static void quad_span(u8 cnt, u8 ax, u8 ay)
{
    lda16(dp16(cnt));
    if (g_cpu._flag_Z) return;
    g_cpu._flag_C = 0;
    do {
        lda16(dc_adc16(g_cpu.X, dp16(ax))); tax(); abs16_set(0x217F, g_cpu.A);
        lda16(dc_adc16(g_cpu.Y, dp16(ay))); tay(); abs16_set(0x217F, g_cpu.A);
        lda16(abs16(0x217F)); lda16(abs16(0x217F));
        u16 n = (u16)(dp16(cnt) - 1); dp16_set(cnt, n); dc_set_flags_nz16(n);
    } while (!g_cpu._flag_Z);
}
/* D1:F9E7  Fill the window port with the quad dp12..19 (points sorted by y, D1:F8EB): flat
 * top (the two top points sorted by x, then a trapezoid), or the 0-1-2 / 0-2-1 split by
 * which side the second point lies on. Edge slopes from D1:F91D, port start CD:38B2. */
DC_ENTRY(D1F9E7, RTS)
{
    lda8(dp8(0x13)); dc_cmp8((u8)g_cpu.A, dp8(0x15));
    if (g_cpu._flag_Z) {
        sort_x(0x12, 0x14);
        sort_x(0x16, 0x18);
        dc_D1F91D_glue();
        lda8(dp8(0x0F)); dc_jsl(0xCD38B2, 0xD1FA2E);
        span_count(0x45, 0x17, 0x15);
        lda8(dp8(0x12));
        dc_set_mx(0, 0); g_cpu._flag_C = 0;
        xba(); tax();
        lda16(dp16(0x14) & 0x00FF); xba(); tay();
        quad_span(0x45, 0x03, 0x09);
        tdc();
        dc_set_mx(1, 0);
        return;
    }
    lda8(dp8(0x16)); dc_cmp8((u8)g_cpu.A, dp8(0x14));
    bool right = g_cpu._flag_C;
    if (right) { ldx(dp16(0x14)); ldy(dp16(0x16)); dp16_set(0x16, g_cpu.X); dp16_set(0x14, g_cpu.Y); }
    dc_D1F91D_glue();
    lda8(dp8(0x0F)); dc_jsl(0xCD38B2, right ? 0xD1FAEA : 0xD1FA6A);
    if (!right) {
        span_count(0x45, 0x15, 0x13); span_count(0x47, 0x17, 0x15); span_count(0x49, 0x19, 0x17);
    } else {
        span_count(0x45, 0x17, 0x13); span_count(0x47, 0x15, 0x17); span_count(0x49, 0x19, 0x15);
    }
    lda8(dp8(0x12));
    dc_set_mx(0, 0);
    xba(); tax(); tay();
    quad_span(0x45, 0x03, 0x07);
    if (!right) quad_span(0x47, 0x03, 0x09); else quad_span(0x47, 0x05, 0x07);
    quad_span(0x49, 0x05, 0x09);
    tdc();
    dc_set_mx(1, 0);
}
/* One colour channel of CDCC,Y (mask m, step s, 8- or 16-bit) one step toward D0:FBE2,X. */
static void fade_channel(u16 off, u16 m, u16 s, bool wide)
{
    if (wide) {
        lda16(bus16(0xD0FBE2 + off + (u32)g_cpu.X) & m); dp16_set(0x45, g_cpu.A);
        lda16(abs16(0xCDCC + off + g_cpu.Y) & m); dc_cmp16(g_cpu.A, dp16(0x45));
        if (g_cpu._flag_Z) return;
        u16 v = abs16(0xCDCC + off + g_cpu.Y);
        if (g_cpu._flag_C) { g_cpu._flag_C = 1; lda16(dc_sbc16(v, s)); }
        else { g_cpu._flag_C = 0; lda16(dc_adc16(v, s)); }
        abs16_set(0xCDCC + off + g_cpu.Y, g_cpu.A);
    } else {
        lda8(bus8(0xD0FBE2 + off + (u32)g_cpu.X) & m); dp8_set(0x45, (u8)g_cpu.A);
        lda8(abs8(0xCDCC + off + g_cpu.Y) & m); dc_cmp8((u8)g_cpu.A, dp8(0x45));
        if (g_cpu._flag_Z) return;
        u8 v = abs8(0xCDCC + off + g_cpu.Y);
        if (s == 1) lda8((u8)(g_cpu._flag_C ? v - 1 : v + 1));
        else if (g_cpu._flag_C) { g_cpu._flag_C = 1; lda8(dc_sbc8(v, (u8)s)); }
        else { g_cpu._flag_C = 0; lda8(dc_adc8(v, (u8)s)); }
        abs8_set(0xCDCC + off + g_cpu.Y, (u8)g_cpu.A);
    }
}
/* D1:EA4B  Party-HP palette tint (nothing while 5D9B or any of 0580/058C/0598/05A4 is set).
 * Idle (CE0A = 0): pick stage 1..8 from 5FB0 against steps of 5FB2 / 8 (or CE0E & 7F when
 * forced), and when it differs from CE0C start a 32-frame fade toward the next stage's
 * 14 colours (D1:E9EF -> D0:FBE2). Fading: move each colour CDCC one step per channel toward
 * stage CE0B, copy to CDE8 and to the three palette rows picked by CDC9 (D1:EA21/2F/3D). */
DC_ENTRY(D1EA4B, RTL)
{
    lda8(abs8(0x5D9B) | abs8(0x05A4) | abs8(0x0598) | abs8(0x058C) | abs8(0x0580));
    if (!g_cpu._flag_Z) return;
    lda8(abs8(0xCE0A));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xCE0B)); asl_a(1); tax();
        dc_set_mx(0, 0);
        lda16(bus16(0xD1E9EF + (u32)g_cpu.X)); tax();
        tdc(); tay();
        dc_set_mx(1, 0);
        do {
            fade_channel(0, 0x1F, 1, false);
            dc_set_mx(0, 0);
            fade_channel(0, 0x03E0, 0x0020, true);
            tdc();
            dc_set_mx(1, 0);
            fade_channel(1, 0x7C, 4, false);
            iny(); iny(); inx(); inx(); cpy(0x001C);
        } while (!g_cpu._flag_Z);
        tdc();
        dc_set_mx(1, 0);
        dec_abs(0xCE0D);
        if (g_cpu._flag_Z) abs8_set(0xCE0A, 0);
        tdc(); tax();
        dc_set_mx(0, 0);
        do { lda16(abs16(0xCDCC + g_cpu.X)); abs16_set(0xCDE8 + g_cpu.X, g_cpu.A); inx(); inx(); cpx(0x001C); } while (!g_cpu._flag_Z);
        tdc();
        dc_set_mx(1, 0);
        lda8(abs8(0xCDC9)); tax();
        static const u16 row[3] = { 0xEA21, 0xEA2F, 0xEA3D };
        for (int k = 0; k < 3; k++) {
            lda8(bus8(0xD10000 + row[k] + (u32)g_cpu.X)); dp8_set(0x45, (u8)g_cpu.A);
            ldy((u16)(0x20 * k));
            dc_D1EBD0_glue(); dc_D1EBDF_glue();
        }
        return;
    }
    ldx(0x0001);
    dc_set_mx(0, 0);
    lda16(abs16(0x5FB2)); lsr_a(3); dp16_set(0x45, g_cpu.A); dp16_set(0x47, g_cpu.A);
    for (;;) {
        lda16(abs16(0x5FB0)); dc_cmp16(g_cpu.A, dp16(0x45));
        if (!g_cpu._flag_C) break;
        lda16(dp16(0x45)); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x47))); dp16_set(0x45, g_cpu.A);
        inx(); cpx(0x0008);
        if (g_cpu._flag_Z) break;
    }
    tdc();
    dc_set_mx(1, 0);
    lda8(abs8(0xCE0E));
    if (g_cpu._flag_N) { lda8((u8)g_cpu.A & 0x7F); tax(); }
    lda8((u8)g_cpu.X); abs8_set(0xCE0B, (u8)g_cpu.A);
    dc_cmp8((u8)g_cpu.A, abs8(0xCE0C));
    if (g_cpu._flag_Z) return;
    inc_abs(0xCE0A);
    lda8(0x20); abs8_set(0xCE0D, 0x20);
    lda8(abs8(0xCE0C)); asl_a(1); tax();
    lda8(abs8(0xCE0E));
    if (g_cpu._flag_N) { lda8(abs8(0xCE0B)); abs8_set(0xCE0C, (u8)g_cpu.A); }
    else {
        lda8(abs8(0xCE0B)); dc_cmp8((u8)g_cpu.A, abs8(0xCE0C));
        if (g_cpu._flag_C) inc_abs(0xCE0C); else dec_abs(0xCE0C);
        lda8(abs8(0xCE0C)); abs8_set(0xCE0B, (u8)g_cpu.A);
    }
    tdc(); tay();
    dc_set_mx(0, 0);
    lda16(bus16(0xD1E9EF + (u32)g_cpu.X)); tax();
    do { lda16(bus16(0xD0FBE2 + (u32)g_cpu.X)); abs16_set(0xCDCC + g_cpu.Y, g_cpu.A); inx(); inx(); iny(); iny(); cpy(0x001C); } while (!g_cpu._flag_Z);
    tdc();
    dc_set_mx(1, 0);
}
/* D1:F67A  Effect setup picked by dp53 >> 4: 0-3 Mode 7 style tables (F74D), 4 shake (F6B4),
 * 5 shake off (F692). */
DC_ENTRY(D1F67A, RTL)
{
    static const glue_fn t[6] = { dc_D1F74D_glue, dc_D1F74D_glue, dc_D1F74D_glue, dc_D1F74D_glue,
                                  dc_D1F6B4_glue, dc_D1F692_glue };
    lda8(dp8(0x53) & 0xF0); lsr_a(3); tax();
    jump_table(t, 6, 0xF686, 0xD1F682);
}
/* D1:F9AF  Draw quad X (CD0D.., every other byte) into the window port with DB = 00: sort
 * the points (F8EB) and rasterize (F9E7); CC64 = X. */
DC_ENTRY(D1F9AF, RTL)
{
    lda8((u8)g_cpu.X); abs8_set(0xCC64, (u8)g_cpu.A);
    for (int k = 0; k < 8; k++) { lda8(abs8(0xCD0D + 2 * k + g_cpu.X)); dp8_set(0x12 + k, (u8)g_cpu.A); }
    dc_D1F8EB_glue();
    dc_push8(g_cpu.DB);
    tdc(); dc_push8((u8)g_cpu.A); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    dc_D1F9E7_glue();
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}
/* D1:EBFF  Quad op 0 around 5DA0/5DA1: x = B0 +/- 28 * sin(dp7C) / 256 (negated when dp7C is
 * negative) .. +20 at y 0, x 5DA0 - 10 .. + 10 at y 5DA1; drawn with D1:F9AF. */
DC_ENTRY(D1EBFF, RTL)
{
    lda8(0x28); bus8_set(0x004202, 0x28);
    lda8(dp8(0x7C));
    bool neg = g_cpu._flag_N;
    tax();
    lda8(bus8(0xCEF48E + (u32)g_cpu.X)); bus8_set(0x004203, (u8)g_cpu.A);
    lda8(bus8(lp(0x40))); tax();
    lda8(bus8(0x004217));
    if (neg) lda8((u8)-(u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0xB0)); abs8_set(0xCD0D + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0xCD0F + g_cpu.X, 0);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x20)); abs8_set(0xCD11 + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0xCD13 + g_cpu.X, 0);
    lda8(abs8(0x5DA0)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, 0x10)); abs8_set(0xCD15 + g_cpu.X, (u8)g_cpu.A);
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x20)); abs8_set(0xCD19 + g_cpu.X, (u8)g_cpu.A);
    lda8(abs8(0x5DA1)); abs8_set(0xCD17 + g_cpu.X, (u8)g_cpu.A); abs8_set(0xCD1B + g_cpu.X, (u8)g_cpu.A);
    dc_D1F9AF_glue();
}
