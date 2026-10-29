/* Bank CC: battle setup helpers. */
#include "ram.h"
#include "ops65.h"

/* X counts down from n while it stays non-negative (DEX; BPL), in the
 * current index width: with 8-bit indexes a start of 80 or more runs once. */
static void fill_down(u16 base, u16 n, u8 v)
{
    g_cpu.X = g_cpu.x_flag ? (n & 0xFF) : n;
    u16 sign = g_cpu.x_flag ? 0x80 : 0x8000;
    do {
        abs8_set(base + g_cpu.X, v);
        g_cpu.X = (u16)(g_cpu.X - 1);
        if (g_cpu.x_flag) g_cpu.X &= 0xFF;
    } while (!(g_cpu.X & sign));
}

/* CC:F06B  Clears the battle work areas (and sets a few to FF / F0). */
DC_ENTRY(CCF06B, RTL)
{
    fill_down(0x5D80, 0x8C, 0);
    fill_down(0xA240, 0x3F, 0);
    fill_down(0xA124, 0x83, 0);
    dc_set_a8(0xFF);
    g_cpu.X = g_cpu.x_flag ? 0x0A : 0x000A;
    u16 sign = g_cpu.x_flag ? 0x80 : 0x8000;
    do {
        abs8_set(0xA014 + g_cpu.X, 0xFF);
        abs8_set(0xA3D4 + g_cpu.X, 0);
        abs8_set(0xA4A4 + g_cpu.X, 0);
        abs8_set(0xA4AF + g_cpu.X, 0);
        abs8_set(0xA0DC + g_cpu.X, 0);
        g_cpu.X = (u16)(g_cpu.X - 1);
        if (g_cpu.x_flag) g_cpu.X &= 0xFF;
    } while (!(g_cpu.X & sign));
    fill_down(0xA1A8, 0x3F, 0);
    fill_down(0xA337, 0x71, 0);
    g_cpu.X = 3;
    do {
        abs8_set(0xA3B9 + g_cpu.X, 0);
        abs8_set(0xA3BD + g_cpu.X, 0);
        abs8_set(0xA4E8 + g_cpu.X, 0);
        g_cpu.X = (u16)(g_cpu.X - 1);
        if (g_cpu.x_flag) g_cpu.X &= 0xFF;
    } while (!(g_cpu.X & sign));
    abs8_set(0xA4EC, 0);
    abs8_set(0xA4ED, 0);
    fill_down(0xA3DF, 0x57, 0);
    fill_down(0xA437, 0x07, 0xFF);
    dc_set_a8(0xF0);
    fill_down(0xAB93, 0x57, 0xF0);
    static const u16 z[] = { 0xA3D1, 0x9931, 0x9876, 0xA21F, 0xA012, 0xAAFB, 0xAAFC, 0xAAFD, 0xAC5A, 0xA2B7 };
    for (unsigned k = 0; k < sizeof z / sizeof z[0]; k++) abs8_set(z[k], 0);
    dc_set_flags_nz16(g_cpu.X);
    if (g_cpu.x_flag) dc_set_flags_nz8((u8)g_cpu.X);
}

/* CC:F102  A44C.. (from X = D) up to A456 = 55. */
DC_ENTRY(CCF102, RTL)
{
    g_cpu.A = g_cpu.D;
    g_cpu.X = g_cpu.x_flag ? (g_cpu.D & 0xFF) : g_cpu.D;
    dc_set_a8(0x55);
    do {
        abs8_set(0xA44C + g_cpu.X, 0x55);
        g_cpu.X++;
        if (g_cpu.x_flag) g_cpu.X &= 0xFF;
        dc_cmp16(g_cpu.X, 0x000B);
    } while (g_cpu.X != 0x000B);
}

/* CC:F110  Shifts the 7E:7BFD map one row (256 bytes) down to 7CFD and
 * marks the cells of the present combatants (96F5+i set; cell from
 * 1D23+i high nibble, 1D0C+i / 16) with 40. Starts at 3 when 2989 bit 2. */
DC_ENTRY(CCF110, RTL)
{
    dc_set_mx(0, 0);
    g_cpu.X = 0x7BFD;
    g_cpu.Y = 0x7CFD;
    g_cpu.A = 0x00FF;
    dc_mvn(0x7E, 0x7E);
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    dp8_set(0x80, 0);
    if (abs8(0x2989) & 0x04) dp8_set(0x80, 0x03);
    dp8_set(0x82, 0x0B);
    dc_set_a8(dp8(0x80));
    g_cpu.Y = g_cpu.A;
    do {
        u8 v = abs8(0x96F5 + g_cpu.Y);
        dc_set_a8(v);
        if (v) {
            u8 hi = abs8(0x1D23 + g_cpu.Y) & 0xF0;
            dp8_set(0x84, hi);
            u8 c = abs8(0x1D0C + g_cpu.Y);
            g_cpu._flag_C = (c >> 3) & 1;
            v = (u8)(c >> 4 | hi);
            dc_set_a8(v);
            g_cpu.X = g_cpu.A;
            dc_set_a8(0x40);
            abs8_set(0x7CFD + g_cpu.X, 0x40);
        }
        g_cpu.Y++;
        u8 n = (u8)(dp8(0x82) - 1);
        dp8_set(0x82, n);
        dc_set_flags_nz8(n);
    } while (dp8(0x82));
}

/* CC:F365  dp AF = AD * AE (hardware multiplier); 77/78 keep the factors. */
DC_ENTRY(CCF365, RTS)
{
    u8 a = dp8(0xAD), b = dp8(0xAE);
    dp8_set(0x77, a);
    bus8_set(0x004202, a);
    dp8_set(0x78, b);
    bus8_set(0x004203, b);
    dc_set_mx(0, g_cpu.x_flag);
    dp16_set(0xAF, bus16(0x004216));
    g_cpu.A = g_cpu.D;
    dc_set_flags_nz16(g_cpu.A);
    dc_set_mx(1, g_cpu.x_flag);
}

/* CC:E22E  Two 64-byte DMAs to VRAM: FF:9CF0 to word address (D low,
 * D low) and FF:9D30 to (D low, D low + 1); with D = 0, 0000 and 0100. */
DC_ENTRY(CCE22E, RTS)
{
    bus8_set(0x004370, 0x01);
    bus8_set(0x004371, 0x18);
    bus8_set(0x004372, 0xF0);
    bus8_set(0x004373, 0x9C);
    bus8_set(0x004374, 0xFF);
    bus8_set(0x004375, 0x40);
    bus8_set(0x004376, 0x00);
    g_cpu.A = g_cpu.D;
    bus8_set(0x002116, (u8)g_cpu.A);
    bus8_set(0x002117, (u8)g_cpu.A);
    bus8_set(0x00420B, 0x80);
    bus8_set(0x004372, 0x30);
    bus8_set(0x004373, 0x9D);
    bus8_set(0x004375, 0x40);
    bus8_set(0x004376, 0x00);
    g_cpu.A = g_cpu.D;
    bus8_set(0x002116, (u8)g_cpu.A);
    dc_set_a8((u8)(g_cpu.A + 1));
    bus8_set(0x002117, (u8)g_cpu.A);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
    bus8_set(0x00420B, 0x80);
}

/* CC:F156  Battle formation setup from the encounter at AE91..AE99:
 * A014 = formation kind, A015/A016 = two parameters (negative ones are
 * replaced from CC:FBC8 for kinds 0-2); A017 = FF00 | index of the first
 * set bit of AE95 (from the top); A018.. / AC5D.. = FF00 | index of each
 * set bit of AE99 (top first, 11 bits); then the positions (1D0C / 1D23)
 * of the up to 3 party members listed at A014.. go to 99AF / 99B2. */
DC_ENTRY(CCF156, RTL)
{
    abs8_set(0xA015, abs8(0xAE97));
    abs8_set(0xA016, abs8(0xAE98));
    u8 kind = abs8(0xAE91);
    dc_set_a8(kind);
    abs8_set(0xA014, kind);
    dc_cmp8(kind, 0x03);
    if (!g_cpu._flag_C) {
        g_cpu._flag_C = kind >> 7;
        dc_set_a8((u8)(kind << 1));
        g_cpu.X = g_cpu.A;
        if (abs8(0xA015) & 0x80) abs8_set(0xA015, bus8(0xCCFBC8 + g_cpu.X));
        if (abs8(0xA016) & 0x80) abs8_set(0xA016, bus8(0xCCFBC9 + g_cpu.X));
    }
    g_cpu.A = g_cpu.X = g_cpu.D;
    dc_set_mx(0, 0);
    dp16_set(0x80, abs16(0xAE95));
    if (dp16(0x80)) {
        for (;;) {
            u16 v = dp16(0x80);
            g_cpu._flag_C = v >> 15;
            dp16_set(0x80, (u16)(v << 1));
            if (g_cpu._flag_C) break;
            g_cpu.X++;
        }
    }
    abs16_set(0xA017, g_cpu.X);
    abs16_set(0xAC5C, g_cpu.X);
    g_cpu.A = (u16)(abs16(0xA017) | 0xFF00);
    abs16_set(0xA017, g_cpu.A);
    abs16_set(0xAC5C, g_cpu.A);
    g_cpu.A = g_cpu.X = g_cpu.Y = g_cpu.D;
    dp16_set(0x80, abs16(0xAE99));
    if (dp16(0x80)) {
        do {
            u16 v = dp16(0x80);
            g_cpu._flag_C = v >> 15;
            dp16_set(0x80, (u16)(v << 1));
            if (g_cpu._flag_C) {
                g_cpu.A = (u16)(g_cpu.X | 0xFF00);
                abs16_set(0xA018 + g_cpu.Y, g_cpu.A);
                abs16_set(0xAC5D + g_cpu.Y, g_cpu.A);
                g_cpu.Y++;
            }
            g_cpu.X++;
            dc_cmp16(g_cpu.X, 0x000B);
        } while (g_cpu.X != 0x000B);
    }
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    g_cpu.X = g_cpu.D;
    for (;;) {
        u8 m = abs8(0xA014 + g_cpu.X);
        dc_set_a8(m);
        if (m & 0x80) break;
        g_cpu.Y = g_cpu.A;
        abs8_set(0x99AF + g_cpu.X, abs8(0x1D0C + g_cpu.Y));
        abs8_set(0x99B2 + g_cpu.X, abs8(0x1D23 + g_cpu.Y));
        g_cpu.X++;
        dc_cmp16(g_cpu.X, 0x0003);
        if (g_cpu.X == 0x0003) break;
    }
}

/* CC:F1E7  Battle palette animations (D = 0): for each of the 4 slots active in A3B9, the
 * timer A3C1 counts down from A3C5; then frame A3BD advances (back to 0 past A3CD - A3C9)
 * and colour set A3C9 + frame of the list at CD:0000 + [9882] (a byte: colour index, flags:
 * bit 7 = 7 colours from D1:4400 else 3 from D1:4100, bits 0-1 = palette row) is copied to
 * the battle palette at 2302 + 8 * row. */
static void palette_copy(u32 src, u8 n)
{
    lda8(n); dp8_set(0xAD, n);
    lda8(dp8(0x82)); dp8_set(0xAE, (u8)g_cpu.A);
    dc_CCF365_glue();
    ldx(dp16(0xAF));
    do {
        lda8(bus8(src + g_cpu.X)); abs8_set(0x2302 + g_cpu.Y, (u8)g_cpu.A);
        inx(); iny();
        u8 c = (u8)(dp8(0xAD) - 1); dp8_set(0xAD, c); dc_set_flags_nz8(c);
    } while (!g_cpu._flag_Z);
}

DC_ENTRY(CCF1E7, RTL)
{
    tdc(); tax(); dp16_set(0x80, g_cpu.X);
    do {
        ldx(dp16(0x80));
        lda8(abs8(0xA3B9 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            dec_abs(0xA3C1 + g_cpu.X);
            if (g_cpu._flag_Z) {
                lda8(abs8(0xA3C5 + g_cpu.X)); abs8_set(0xA3C1 + g_cpu.X, (u8)g_cpu.A);
                inc_abs(0xA3BD + g_cpu.X);
                g_cpu._flag_C = 0;
                lda8(dc_adc8(abs8(0xA3C9 + g_cpu.X), abs8(0xA3BD + g_cpu.X)));
                dc_cmp8((u8)g_cpu.A, abs8(0xA3CD + g_cpu.X));
                if (g_cpu._flag_C && !g_cpu._flag_Z) {
                    abs8_set(0xA3BD + g_cpu.X, 0);
                    lda8(abs8(0xA3C9 + g_cpu.X));
                }
                lda8(asl8((u8)g_cpu.A));
                dc_set_mx(0, 0);
                g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0x9882)));
                tax(); tdc(); dp16_set(0x82, g_cpu.A);
                dc_set_mx(1, 0);
                lda8(bus8(0xCD0000 + g_cpu.X)); dp8_set(0x82, (u8)g_cpu.A);
                lda8(bus8(0xCD0001 + g_cpu.X)); dp8_set(0x84, (u8)g_cpu.A);
                lda8((u8)(g_cpu.A & 0x03));
                lda8(asl8((u8)g_cpu.A)); lda8(asl8((u8)g_cpu.A)); lda8(asl8((u8)g_cpu.A));
                tay();
                lda8(dp8(0x84));
                if (g_cpu._flag_N) palette_copy(0xD14400, 0x0E);
                else palette_copy(0xD14100, 0x06);
            }
        }
        u8 i = (u8)(dp8(0x80) + 1); dp8_set(0x80, i);
        lda8(i); dc_cmp8(i, 0x04);
    } while (!g_cpu._flag_Z);
}

/* CC:F2A9  Battle sprite slot dp 80: move A230 to the target A2B8 (one step: copied when
 * different), then load its frame from the CE:0000 records at [AD * AE = A230 * 4 * A238]
 * + 4 * A1B8 + A220 (A1B8 + 1 wraps to 0 and clears A1A8 when A1C8): the delay A1B0
 * (bit 7 sets flag 20 of A2C0), the signed x / y offsets A260 / A270 and the flags A2C0
 * (top bits restart A1A8 with 80, or set A1B8 = FF when A1A8 = 2). */
static void offset_byte(u32 src, u16 at)
{
    lda8(bus8(src + g_cpu.X)); abs8_set(at + g_cpu.Y, (u8)g_cpu.A);
    if (g_cpu._flag_N) lda8(0xFF); else tdc();
    abs8_set(at + 1 + g_cpu.Y, (u8)g_cpu.A);
}

DC_ENTRY(CCF2A9, RTS)
{
    ldx(dp16(0x80));
    lda8(abs8(0xA230 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, abs8(0xA2B8 + g_cpu.X));
    if (!g_cpu._flag_Z) abs8_set(0xA230 + g_cpu.X, (u8)g_cpu.A);
    else lda8(abs8(0xA230 + g_cpu.X));
    dp8_set(0xAD, (u8)g_cpu.A);
    lda8(abs8(0xA238 + g_cpu.X)); lda8(asl8((u8)g_cpu.A)); lda8(asl8((u8)g_cpu.A));
    dp8_set(0xAE, (u8)g_cpu.A);
    dc_CCF365_glue();
    ldx(dp16(0xAF)); dp16_set(0x82, g_cpu.X);
    lda8(dp8(0x80)); lda8(asl8((u8)g_cpu.A)); tay(); dp16_set(0x84, g_cpu.Y);
    ldx(dp16(0x80));
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0xA1B8 + g_cpu.X), 0x01));
    if (g_cpu._flag_Z) {
        lda8(abs8(0xA1C8 + g_cpu.X));
        if (!g_cpu._flag_Z) abs8_set(0xA1A8 + g_cpu.X, 0);
        tdc();
    }
    abs8_set(0xA1B8 + g_cpu.X, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    asl_a(2);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x82)));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xA220 + g_cpu.Y)));
    tax(); tdc();
    dc_set_mx(1, 0);
    dp8_set(0x8E, 0); dp8_set(0x8F, 0);
    ldy(dp16(0x80));
    lda8(bus8(0xCE0003 + g_cpu.X));
    if (g_cpu._flag_N) {
        u8 v = (u8)(dp8(0x8F) + 1); dp8_set(0x8F, v); dc_set_flags_nz8(v);
        lda8((u8)(g_cpu.A & 0x7F));
    }
    abs8_set(0xA1B0 + g_cpu.Y, (u8)g_cpu.A);
    ldy(dp16(0x84));
    offset_byte(0xCE0001, 0xA260);
    offset_byte(0xCE0002, 0xA270);
    ldy(dp16(0x80));
    lda8((u8)(bus8(0xCE0000 + g_cpu.X) & 0xE0));
    if (!g_cpu._flag_Z) {
        lda8(abs8(0xA1A8 + g_cpu.Y)); dc_cmp8((u8)g_cpu.A, 0x02);
        if (g_cpu._flag_Z) { lda8(0xFF); abs8_set(0xA1B8 + g_cpu.Y, 0xFF); }
        else { tdc(); abs8_set(0xA1A8 + g_cpu.Y, (u8)g_cpu.A); lda8(0x80); dp8_set(0x8E, 0x80); }
    }
    lda8((u8)((bus8(0xCE0000 + g_cpu.X) & 0x1F) | dp8(0x8E)));
    abs8_set(0xA2C0 + g_cpu.Y, (u8)g_cpu.A);
    lda8(dp8(0x8F));
    if (!g_cpu._flag_Z) { lda8((u8)(abs8(0xA2C0 + g_cpu.Y) | 0x20)); abs8_set(0xA2C0 + g_cpu.Y, (u8)g_cpu.A); }
}

/* CC:E000  Battle graphics upload in steps (98CA = 1, 2, 3, then done; 0 = nothing): DMA
 * channel 7 blocks to VRAM. A VRAM byte of -1 is D's low byte (TDC; 0 in battle). */
struct cc_dma { u8 mode; u8 src[3]; u8 size[2]; s16 vram[2]; };
static void cc_dma_run(const struct cc_dma *d, unsigned n)
{
    for (unsigned i = 0; i < n; i++, d++) {
        if (d->mode) {
            lda8(0x01); bus8_set(0x004370, 0x01);
            lda8(0x18); bus8_set(0x004371, 0x18);
        }
        for (int k = 0; k < 3; k++) { lda8(d->src[k]); bus8_set(0x004372 + k, d->src[k]); }
        for (int k = 0; k < 2; k++) { lda8(d->size[k]); bus8_set(0x004375 + k, d->size[k]); }
        for (int k = 0; k < 2; k++) {
            if (d->vram[k] < 0) tdc(); else lda8((u8)d->vram[k]);
            bus8_set(0x002116 + k, (u8)g_cpu.A);
        }
        lda8(0x80); bus8_set(0x00420B, 0x80);
    }
}

DC_ENTRY(CCE000, RTL)
{
    static const struct cc_dma s1[] = {
        { 1, { 0x60, 0x8C, 0xFF }, { 0x00, 0x10 }, { -1, 0x58 } },
        { 0, { 0xC2, 0x5E, 0xD1 }, { 0x40, 0x00 }, { 0x60, -1 } },
        { 0, { 0x82, 0x5E, 0xD1 }, { 0x40, 0x00 }, { 0x20, -1 } },
    }, s2[] = {
        { 1, { 0xC0, 0x0C, 0x00 }, { 0x80, 0x07 }, { 0x20, 0x78 } },
        { 0, { 0x02, 0x5F, 0xD1 }, { 0x40, 0x00 }, { 0x60, 0x01 } },
        { 0, { 0x02, 0x5F, 0xD1 }, { 0x40, 0x00 }, { 0x20, 0x01 } },
        { 0, { 0xFD, 0x7F, 0x7E }, { 0xC0, 0x01 }, { -1, 0x7B } },
        { 0, { 0xBD, 0x81, 0x7E }, { 0x40, 0x00 }, { -1, 0x78 } },
    }, s3[] = {
        { 1, { 0x40, 0x0B, 0x00 }, { 0x80, 0x01 }, { -1, 0x7A } },
        { 1, { 0x00, 0x2D, 0x7E }, { 0x00, 0x0C }, { 0x00, 0x18 } },
    };
    lda8(abs8(0x98CA));
    if (g_cpu._flag_Z) return;
    lda8((u8)(g_cpu.A - 1));
    if (g_cpu._flag_Z) cc_dma_run(s1, 3);
    else {
        lda8((u8)(g_cpu.A - 1));
        if (g_cpu._flag_Z) { cc_dma_run(s2, 5); dc_CCE22E_glue(); }
        else {
            lda8((u8)(g_cpu.A - 1));
            if (!g_cpu._flag_Z) return;
            cc_dma_run(s3, 2);
        }
    }
    inc_abs(0x98CA);
}

/* CC:F278  Battle sprite frames (D = 0): the 8 slots active in A1A8 count their delay
 * A1B0 down, and those at 0 load their next frame (CC:F2A9). */
DC_ENTRY(CCF278, RTL)
{
    tdc(); tax();
    do {
        lda8(abs8(0xA1A8 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xA1B0 + g_cpu.X));
            if (!g_cpu._flag_Z) dec_abs(0xA1B0 + g_cpu.X);
        }
        inx(); cpx(0x0008);
    } while (!g_cpu._flag_Z);
    tdc(); tax(); dp16_set(0x80, g_cpu.X);
    do {
        ldx(dp16(0x80));
        lda8(abs8(0xA1A8 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xA1B0 + g_cpu.X));
            if (g_cpu._flag_Z) dc_CCF2A9_glue();
        }
        u8 i = (u8)(dp8(0x80) + 1); dp8_set(0x80, i);
        lda8(i); dc_cmp8(i, 0x08);
    } while (!g_cpu._flag_Z);
}
