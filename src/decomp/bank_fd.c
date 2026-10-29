/* Bank FD: battle effect helpers called from C1, screen setup and HDMA list builders. */
#include "ram.h"
#include "ops65.h"

/* FD:CC58  dp EE (running line count of the HDMA lists below) = 0; Y = 0. */
DC_ENTRY(FDCC58, RTS)
{
    ldy(0);
    dp16_set(0xEE, 0);
}

/* FD:CCB2  dp EE += A (lines skipped). */
DC_ENTRY(FDCCB2, RTS)
{
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0xEE)));
    dp8_set(0xEE, (u8)g_cpu.A);
}

/* HDMA line list at table+Y: A lines in runs of at most 16 (count byte 80 | n, then the
 * address data + 4 * (EE & 0F) of the row data); dp EE counts the lines written. */
static void hdma_runs(u16 table, u16 data)
{
    for (;;) {
        dc_cmp8((u8)g_cpu.A, 0x10);
        bool rest = g_cpu._flag_C;
        u8 n = rest ? (u8)(g_cpu.A - 0x10) : (u8)g_cpu.A;
        dp8_set(0xD9, n);
        abs8_set(table + g_cpu.Y, rest ? 0x90 : (u8)(n | 0x80));
        g_cpu.A = (u16)(((dp8(0xEE) & 0x0F) << 2) + data);
        abs16_set(table + 1 + g_cpu.Y, g_cpu.A);
        g_cpu.Y += 3;
        g_cpu._flag_C = 0;
        lda8(dc_adc8(dp8(0xEE), rest ? 0x10 : n));
        dp8_set(0xEE, (u8)g_cpu.A);
        if (!rest) return;
        lda8(dp8(0xD9));
        if (g_cpu._flag_Z) return;
    }
}

/* FD:CC5E  A lines into the HDMA list at 1085 (rows from 1D27). */
DC_ENTRY(FDCC5E, RTS) { hdma_runs(0x1085, 0x1D27); }

/* FD:CCB8  A lines into the HDMA list at 133D (rows from 1DA7). */
DC_ENTRY(FDCCB8, RTS) { hdma_runs(0x133D, 0x1DA7); }

/* FD:EAC8  Screen off for loading (DB = 00): HDMA and DMA off, forced blank, NMI off, SEI. */
DC_ENTRY(FDEAC8, RTS)
{
    abs8_set(0x420B, 0);
    abs8_set(0x420C, 0);
    lda8(0x80); abs8_set(0x2100, 0x80);
    lda8(0x00); abs8_set(0x4200, 0x00);
    g_cpu._flag_I = 1;
}

/* FD:EAF4  NMI and IRQ hooks at 0500/0504: JML FD:DE15 and JML FD:DE87. */
DC_ENTRY(FDEAF4, RTS)
{
    lda8(0x5C);
    abs8_set(0x0500, 0x5C); abs8_set(0x0504, 0x5C);
    ldx(0xDE15); abs16_set(0x0501, 0xDE15);
    lda8(0xFD); abs8_set(0x0503, 0xFD);
    ldx(0xDE87); abs16_set(0x0505, 0xDE87);
    lda8(0xFD); abs8_set(0x0507, 0xFD);
}

/* FD:EAA6  Saves 7F:0000+X..01FF (X = dp 08) to 7F:74B3+X. */
DC_ENTRY(FDEAA6, RTS)
{
    ldx(dp16(0x08));
    do {
        lda8(bus8(0x7F0000 + g_cpu.X));
        bus8_set(0x7F74B3 + g_cpu.X, (u8)g_cpu.A);
        inx(); cpx(0x0200);
    } while (!g_cpu._flag_Z);
}

/* FD:EAB7  Restores 7F:0000+X..01FF from 7F:74B3+X (X = dp 08). */
DC_ENTRY(FDEAB7, RTS)
{
    ldx(dp16(0x08));
    do {
        lda8(bus8(0x7F74B3 + g_cpu.X));
        bus8_set(0x7F0000 + g_cpu.X, (u8)g_cpu.A);
        inx(); cpx(0x0200);
    } while (!g_cpu._flag_Z);
}

/* Stat raise: [0000 + dp00 + off] += dp0A, capped at cap. */
static void stat_add(u8 off, u8 cap)
{
    ldx(dp16(0x00));
    lda8(abs8(off + g_cpu.X));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, dp8(0x0A)));
    dc_cmp8((u8)g_cpu.A, cap);
    if (g_cpu._flag_C) lda8(cap);
    abs8_set(off + g_cpu.X, (u8)g_cpu.A);
}

/* Next flag bit of dp04 (ASL; N = the bit). */
static bool stat_next(void)
{
    u8 v = asl8(dp8(0x04)); dp8_set(0x04, v);
    lda8(v);
    return g_cpu._flag_N;
}

/* FD:B14D  Raises the stats of character dp00 by item A (CC:0000 + 2 * A + dp0E: flags,
 * amount): bit 7 0036 (max 99), 6 0038 (max 16), 5 0037, 4 003A, 3 003B (max FF),
 * 2 0039, 1 003C (max 99). */
DC_ENTRY(FDB14D, RTL)
{
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = (u16)(g_cpu.A << 1);
    lda16(dc_adc16(g_cpu.A, dp16(0x0E)));
    tax();
    tdc();
    dc_set_mx(1, 0);
    stx_dp(0x0C);
    lda8(bus8(0xCC0001 + g_cpu.X)); dp8_set(0x0A, (u8)g_cpu.A);
    lda8(bus8(0xCC0000 + g_cpu.X)); dp8_set(0x04, (u8)g_cpu.A);
    if (g_cpu._flag_N) stat_add(0x36, 0x63);
    if (stat_next()) stat_add(0x38, 0x10);
    if (stat_next()) stat_add(0x37, 0x63);
    if (stat_next()) stat_add(0x3A, 0x63);
    if (stat_next()) {
        ldx(dp16(0x00));
        lda8(abs8(0x3B + g_cpu.X));
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.A, dp8(0x0A)));
        if (g_cpu._flag_C) lda8(0xFF);
        abs8_set(0x3B + g_cpu.X, (u8)g_cpu.A);
    }
    if (stat_next()) stat_add(0x39, 0x63);
    if (stat_next()) stat_add(0x3C, 0x63);
}

/* FD:B141  Item stat raise (FD:B14D) when CC:0004 + dp08 is set. */
DC_ENTRY(FDB141, RTL)
{
    tdc();
    ldx(dp16(0x08));
    lda8(bus8(0xCC0004 + g_cpu.X));
    if (!g_cpu._flag_Z) dc_FDB14D_glue();
}

/* FD:F222  DMA 0700..091F to OAM (DB = 00). */
DC_ENTRY(FDF222, RTS)
{
    abs8_set(0x2102, 0); abs8_set(0x420B, 0);
    lda8(0x40); abs8_set(0x4300, 0x40);
    lda8(0x04); abs8_set(0x4301, 0x04);
    ldx(0x0700); abs16_set(0x4302, 0x0700);
    lda8(0x00); abs8_set(0x4304, 0); abs8_set(0x4307, 0);
    ldx(0x0220); abs16_set(0x4305, 0x0220);
    lda8(0x01); abs8_set(0x420B, 0x01);
}

/* FD:F266  DMA 7E:2000..21FF to CGRAM (DB = 00). */
DC_ENTRY(FDF266, RTS)
{
    abs8_set(0x2121, 0); abs8_set(0x420B, 0);
    lda8(0x42); abs8_set(0x4300, 0x42);
    lda8(0x22); abs8_set(0x4301, 0x22);
    ldx(0x2000); abs16_set(0x4302, 0x2000);
    lda8(0x7E); abs8_set(0x4304, 0x7E); abs8_set(0x4307, 0x7E);
    ldx(0x0200); abs16_set(0x4305, 0x0200);
    lda8(0x01); abs8_set(0x420B, 0x01);
}

/* FD:F1C1  DMA 7F:0000..3FFF to VRAM at dp 08 (DB = 00). */
DC_ENTRY(FDF1C1, RTS)
{
    abs8_set(0x2115, 0); abs8_set(0x420B, 0);
    lda8(0x40); abs8_set(0x4300, 0x40);
    lda8(0x18); abs8_set(0x4301, 0x18);
    lda8(0x7F); abs8_set(0x4304, 0x7F);
    ldx(0x0000); abs16_set(0x4302, 0x0000);
    ldx(dp16(0x08)); abs16_set(0x2116, g_cpu.X);
    ldx(0x4000); abs16_set(0x4305, 0x4000);
    lda8(0x01); abs8_set(0x420B, 0x01);
}

/* FD:A982  Clears AD89..B3F4 (battle work area). */
DC_ENTRY(FDA982, RTL)
{
    tdc();
    ldx(0xAD89);
    do { abs8_set(g_cpu.X, (u8)g_cpu.A); inx(); cpx(0xB3F5); } while (!g_cpu._flag_C);
}

/* FD:ECC0  Clears 7F:4000..5FFF through the WRAM port (DB = 00). */
DC_ENTRY(FDECC0, RTS)
{
    ldx(0x4000); abs16_set(0x2181, 0x4000);
    lda8(0x7F); abs8_set(0x2183, 0x7F);
    ldx(0x2000);
    do { abs8_set(0x2180, 0); dex(); } while (!g_cpu._flag_Z);
}

/* Fills n bytes at base with A (X = 0..n-1); leaves X = n, C set. */
static void fill_x(u16 base, u16 n)
{
    do { abs8_set(base + g_cpu.X, (u8)g_cpu.A); inx(); cpx(n); } while (!g_cpu._flag_C);
}

/* FD:ACFD  Clears B328..B3AB. */
DC_ENTRY(FDACFD, RTL) { tdc(); tax(); fill_x(0xB328, 0x0084); }

/* FD:ACEE  Clears AD9B..AE4B. */
DC_ENTRY(FDACEE, RTL) { tdc(); tax(); abs8_set(0xAD9B, (u8)g_cpu.A); fill_x(0xAD9C, 0x00B0); }

/* FD:AD09  Fills 1A80..1C23 with FF. */
DC_ENTRY(FDAD09, RTL) { tdc(); tax(); lda8(0xFF); fill_x(0x1A80, 0x01A4); }

/* FD:B223  AE6D + n = n for n = 0..10. */
DC_ENTRY(FDB223, RTL)
{
    tdc();
    do { tax(); abs8_set(0xAE6D + g_cpu.X, (u8)g_cpu.A); lda8((u8)(g_cpu.A + 1)); dc_cmp8((u8)g_cpu.A, 0x0B); } while (!g_cpu._flag_C);
}

/* FD:B067  dp AA |= dp 2C + 7E. */
DC_ENTRY(FDB067, RTL)
{
    lda8(dp8(0x2C));
    g_cpu._flag_C = 0;
    u8 v = dc_adc8((u8)g_cpu.A, 0x7E);
    dc_set_a8(v);
    u8 m = dp8(0xAA);
    g_cpu._flag_Z = (m & v) == 0;
    dp8_set(0xAA, m | v);
    g_cpu.A = g_cpu.D;
}

/* FD:B3EB  Enemy X: type 3D or 42 sets 5E72 + X to 32. */
DC_ENTRY(FDB3EB, RTL)
{
    lda8(abs8(0x5E56 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0x3D);
    if (!g_cpu._flag_Z) dc_cmp8((u8)g_cpu.A, 0x42);
    if (g_cpu._flag_Z) { lda8(0x32); abs8_set(0x5E72 + g_cpu.X, 0x32); }
}

/* FD:EC91  DMA 7F:4000..5FFF to VRAM 4000 (word writes, DB = 00). */
DC_ENTRY(FDEC91, RTS)
{
    lda8(0x80); abs8_set(0x2115, 0x80);
    abs8_set(0x420B, 0);
    lda8(0x41); abs8_set(0x4300, 0x41);
    lda8(0x18); abs8_set(0x4301, 0x18);
    lda8(0x7F); abs8_set(0x4304, 0x7F);
    ldx(0x4000); abs16_set(0x4302, 0x4000);
    ldx(0x2000); abs16_set(0x4305, 0x2000);
    ldx(0x4000); abs16_set(0x2116, 0x4000);
    lda8(0x01); abs8_set(0x420B, 0x01);
}

/* FD:EC80  Writes A (X-sized count) pairs FF 00 to the WRAM port; X kept. */
DC_ENTRY(FDEC80, RTS)
{
    phx();
    tax();
    if (!g_cpu._flag_Z) {
        lda8(0xFF);
        do { abs8_set(0x2180, 0xFF); abs8_set(0x2180, 0); dex(); } while (!g_cpu._flag_Z);
    }
    plx();
}

/* FD:B01C  dp04 += 5; dp00 and dp08 + 1 (low bytes). */
DC_ENTRY(FDB01C, RTL)
{
    g_cpu._flag_C = 0;
    dp16_set(0x04, dc_adc16(dp16(0x04), 0x0005));
    g_cpu.A = g_cpu.D;
    u8 v = (u8)(dp8(0x00) + 1); dp8_set(0x00, v);
    v = (u8)(dp8(0x08) + 1); dp8_set(0x08, v); dc_set_flags_nz8(v);
}

/* FD:B4D6  Clears 128 bytes at 5FAD + Y; X and Y kept. */
DC_ENTRY(FDB4D6, RTS)
{
    phx(); if (g_cpu.x_flag) dc_push8((u8)g_cpu.Y); else dc_push16(g_cpu.Y);
    tdc(); tax();
    do { abs8_set(0x5FAD + g_cpu.Y, (u8)g_cpu.A); inx(); iny(); cpx(0x0080); } while (!g_cpu._flag_C);
    ldy(g_cpu.x_flag ? dc_pull8() : dc_pull16()); plx();
}

/* FD:B201  With B3BD set: B2A5 += B28C, B28C = 0, B2AF = B2AF & 7F | 40. */
DC_ENTRY(FDB201, RTL)
{
    lda8(abs8(0xB3BD));
    if (g_cpu._flag_Z) return;
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(abs16(0xB28C), abs16(0xB2A5));
    abs16_set(0xB2A5, g_cpu.A);
    g_cpu.A = g_cpu.D;
    abs16_set(0xB28C, g_cpu.A);
    lda8((u8)((abs8(0xB2AF) & 0x7F) | 0x40));
    abs8_set(0xB2AF, (u8)g_cpu.A);
}

/* Entry B18C of a table of 11-byte records (index = B18C * 0B by C1:FDBF, kept in B2C3):
 * B18E = first byte, B18F = B18C, B190 = second byte, B191 = 0. */
static void record_pick(u32 table, u32 site)
{
    lda8(abs8(0xB18C)); tax(); stx_dp(0x28);
    lda8(0x0B); tax(); stx_dp(0x2A);
    dc_jsl(0xC1FDBF, site);
    ldx(dp16(0x2C)); abs16_set(0xB2C3, g_cpu.X);
    ldx(abs16(0xB2C3));
    lda8(bus8(table + g_cpu.X)); abs8_set(0xB18E, (u8)g_cpu.A);
    lda8(abs8(0xB18C)); abs8_set(0xB18F, (u8)g_cpu.A);
    lda8(bus8(table + 1 + g_cpu.X)); abs8_set(0xB190, (u8)g_cpu.A);
    abs8_set(0xB191, 0);
}

/* FD:AAD2  Record B18C of CC:6FCB. */
DC_ENTRY(FDAAD2, RTL) { record_pick(0xCC6FCB, 0xFDAADD); }

/* FD:AB01  Record B18C of CC:88CB. */
DC_ENTRY(FDAB01, RTL) { record_pick(0xCC88CB, 0xFDAB0C); }

/* FD:AAB0  7F:01EE = bit n set for each nonzero AE85 + n (n = 0..7). */
DC_ENTRY(FDAAB0, RTL)
{
    tdc();
    bus8_set(0x7F01EE, (u8)g_cpu.A);
    tax();
    lda8(0x01); dp8_set(0x00, 0x01);
    do {
        lda8(abs8(0xAE85 + g_cpu.X));
        if (!g_cpu._flag_Z) {
            lda8((u8)(bus8(0x7F01EE) | dp8(0x00)));
            bus8_set(0x7F01EE, (u8)g_cpu.A);
        }
        dp8_set(0x00, asl8(dp8(0x00)));
        inx(); cpx(0x0008);
    } while (!g_cpu._flag_C);
}

/* FD:F24C  OAM buffer: every sprite y = F0 (0701 + 4n), high table 0900..091F = 55. */
DC_ENTRY(FDF24C, RTS)
{
    ldx(0x0200); lda8(0xF0);
    do { abs8_set(0x06FD + g_cpu.X, 0xF0); ldx((u16)(g_cpu.X - 4)); } while (!g_cpu._flag_Z);
    ldx(0x0020); lda8(0x55);
    do { abs8_set(0x08FF + g_cpu.X, 0x55); dex(); } while (!g_cpu._flag_Z);
}

/* FD:EB13  PPU setup for a Mode 7 scene (DB = 00): DMA/HDMA off, OBJ base, mode 7,
 * BG1 map 4800, windows, main/sub screens, color math, joypad port, IRQ position 0. */
DC_ENTRY(FDEB13, RTS)
{
    static const struct { u16 r; u8 v; } regs[] = {
        { 0x420B, 0x00 }, { 0x420C, 0x00 }, { 0x2101, 0x82 }, { 0x2102, 0x00 }, { 0x2103, 0x00 },
        { 0x2105, 0x07 }, { 0x2106, 0x00 }, { 0x2107, 0x48 }, { 0x210B, 0x00 }, { 0x210C, 0x00 },
        { 0x2123, 0x03 }, { 0x2124, 0x00 }, { 0x2125, 0x00 }, { 0x2126, 0x00 }, { 0x2127, 0xFF },
        { 0x2128, 0x00 }, { 0x2129, 0xFF }, { 0x212A, 0x00 }, { 0x212B, 0x00 }, { 0x212C, 0x01 },
        { 0x212D, 0x10 }, { 0x212E, 0x01 }, { 0x212F, 0x10 }, { 0x2130, 0x22 }, { 0x2131, 0x41 },
        { 0x2132, 0xE0 }, { 0x2133, 0x00 }, { 0x4201, 0xFF }, { 0x4207, 0x00 }, { 0x4208, 0x00 },
        { 0x4209, 0x00 },
    };
    for (unsigned k = 0; k < sizeof regs / sizeof regs[0]; k++) abs8_set(regs[k].r, regs[k].v);
    lda8(0xFF);
}

/* FD:B121  A020 + n = 5E47 + 180 + 80n & 3 for n = 0..7 (enemy slot flags). */
DC_ENTRY(FDB121, RTL)
{
    ldx(0x0180); ldy(0x0000);
    do {
        lda8((u8)(abs8(0x5E47 + g_cpu.X) & 0x03));
        abs8_set(0xA020 + g_cpu.Y, (u8)g_cpu.A);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0080);
        tax();
        g_cpu.A = g_cpu.D;
        iny(); cpy(0x0008);
    } while (!g_cpu._flag_C);
}

/* FD:AE99  B3BA..BC = the three party members' 5E57/5ED7/5F57; B3BD counts those equal to A9. */
DC_ENTRY(FDAE99, RTL)
{
    abs8_set(0xB3BD, 0);
    static const u16 src[3] = { 0x5E57, 0x5ED7, 0x5F57 };
    for (int k = 0; k < 3; k++) {
        lda8(abs8(src[k]));
        abs8_set(0xB3BA + k, (u8)g_cpu.A);
        dc_cmp8((u8)g_cpu.A, 0xA9);
        if (g_cpu._flag_Z) inc_abs(0xB3BD);
    }
}

/* FD:B33F  New game party data: AF1D = 0, AEC5 = 3, 2600..282F = CC:0000.., 2400 + n = n. */
DC_ENTRY(FDB33F, RTS)
{
    abs8_set(0xAF1D, 0);
    lda8(0x03); abs8_set(0xAEC5, 0x03);
    tdc(); tax();
    do { lda8(bus8(0xCC0000 + g_cpu.X)); abs8_set(0x2600 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0230); } while (!g_cpu._flag_C);
    tdc(); tax();
    do { lda8((u8)g_cpu.X); abs8_set(0x2400 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0200); } while (!g_cpu._flag_Z);
}

/* FD:A8A5  B3EA = 1 when battle slot A (high byte, 80 per slot) is out of action:
 * 5E4A bit 7, 5E4E | 5E53 bit 7, or 5E4B & 86. */
DC_ENTRY(FDA8A5, RTL)
{
    abs8_set(0xB3EA, 0);
    xba();
    g_cpu._flag_C = g_cpu.A & 1;
    g_cpu.A >>= 1;
    dc_set_flags_nz16(g_cpu.A);
    tay();
    g_cpu.A = g_cpu.D;
    lda8(abs8(0x5E4A + g_cpu.Y));
    bool out = (g_cpu.A & 0x80) != 0;
    if (!out) { lda8((u8)(abs8(0x5E4E + g_cpu.Y) | abs8(0x5E53 + g_cpu.Y))); out = (g_cpu.A & 0x80) != 0; }
    if (!out) { lda8(abs8(0x5E4B + g_cpu.Y)); out = (g_cpu.A & 0x86) != 0; }
    g_cpu._flag_Z = !out;
    if (out) { lda8(0x01); abs8_set(0xB3EA, 0x01); }
}

/* Palette fades (DB = 00): 0522 + Y colors from 7E:2000 + X to 7E:2200 + X. The fade level is
 * the multiplier already in 4202; a channel times it, high byte of the product (4217). */
static u8 mul_hi(u8 v)
{
    abs8_set(0x4203, v);
    lda8(abs8(0x4217));
    return (u8)g_cpu.A;
}

/* Count from 0522 + Y into Y (B is the count's high byte). */
static void fade_count(void) { lda8(abs8(0x0522 + g_cpu.Y)); tay(); }
static bool fade_next(void) { inx(); inx(); ldy((u16)(g_cpu.Y - 1)); return !g_cpu._flag_Z; }

/* FD:E6A1  Palette fade step (DB = 00): for 0522 + Y colors from 7E:2000 + X, blue scaled by
 * the multiplier set in 4202 (4217 = blue * 4202 >> 8), red/green kept; result to 7E:2200 + X. */
DC_ENTRY(FDE6A1, RTS)
{
    lda8(abs8(0x0522 + g_cpu.Y));
    tay();
    do {
        mul_hi((u8)(bus8(0x7E2001 + g_cpu.X) & 0x7C));
        xba();
        dc_set_mx(0, 0);
        lda16((u16)(g_cpu.A & 0x7C00));
        dp16_set(0x0C, g_cpu.A);
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03FF) | dp16(0x0C)));
        bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
        dc_set_mx(1, 0);
        inx(); inx();
        ldy((u16)(g_cpu.Y - 1));
    } while (!g_cpu._flag_Z);
}

/* FD:AEC4  Party members whose 5E57 slot is AB get bit 2 of their 5E50 status. */
DC_ENTRY(FDAEC4, RTL)
{
    for (int k = 0; k < 3; k++) {
        u16 o = (u16)(k * 0x80);
        lda8(abs8(0x5E57 + o));
        dc_cmp8((u8)g_cpu.A, 0xAB);
        if (g_cpu._flag_Z) { lda8((u8)(abs8(0x5E50 + o) | 0x04)); abs8_set(0x5E50 + o, (u8)g_cpu.A); }
    }
}

/* FD:B6C4  A0BA + dp04 = A, A0BB + dp04 = 1, B3BF |= 2, AF23 = 0 (the C1:FDC3 call after
 * it is skipped: AF23 was just cleared). */
DC_ENTRY(FDB6C4, RTL)
{
    ldx(dp16(0x04));
    abs8_set(0xA0BA + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01); ldx(dp16(0x04)); abs8_set(0xA0BB + g_cpu.X, 0x01);
    lda8((u8)(abs8(0xB3BF) | 0x02)); abs8_set(0xB3BF, (u8)g_cpu.A);
    abs8_set(0xAF23, 0);
    lda8(abs8(0xAF23));
}

/* FD:E708  Red scaled. */
DC_ENTRY(FDE708, RTS)
{
    fade_count();
    do {
        lda8((u8)(bus8(0x7E2000 + g_cpu.X) & 0x1F));
        mul_hi((u8)g_cpu.A);
        dc_set_mx(0, 0);
        lda16((u16)(g_cpu.A & 0x001F)); dp16_set(0x0C, g_cpu.A);
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x7FE0) | dp16(0x0C)));
        bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
        dc_set_mx(1, 0);
    } while (fade_next());
    lda8(0x00); xba();
}

/* Toward white: c + (max - c) * level, per channel byte (mask of the channel in that byte). */
static void whiten_byte(u8 off, u8 mask)
{
    lda8((u8)(bus8(0x7E2000 + off + g_cpu.X) & mask)); dp8_set(0x0C, (u8)g_cpu.A);
    lda8((u8)(g_cpu.A ^ mask));
    abs8_set(0x4203, (u8)g_cpu.A);
    lda8(dp8(0x0C));
    g_cpu._flag_C = 0;
    lda8((u8)(dc_adc8((u8)g_cpu.A, abs8(0x4217)) & mask)); dp8_set(0x0C, (u8)g_cpu.A);
    lda8((u8)((bus8(0x7E2000 + off + g_cpu.X) & (off ? 0x03 : 0xE0)) | dp8(0x0C)));
    bus8_set(0x7E2200 + off + g_cpu.X, (u8)g_cpu.A);
}

/* FD:E935  Blue toward white. */
DC_ENTRY(FDE935, RTS) { fade_count(); do whiten_byte(1, 0x7C); while (fade_next()); }

/* FD:E962  Red toward white. */
DC_ENTRY(FDE962, RTS) { fade_count(); do whiten_byte(0, 0x1F); while (fade_next()); }

/* FD:E98F  Green toward white. */
DC_ENTRY(FDE98F, RTS)
{
    fade_count();
    do {
        dc_set_mx(0, 0);
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03E0) >> 2));
        dc_set_mx(1, 0);
        dp8_set(0x0C, (u8)g_cpu.A);
        lda8((u8)(g_cpu.A ^ 0xF8));
        abs8_set(0x4203, (u8)g_cpu.A);
        lda8(dp8(0x0C));
        g_cpu._flag_C = 0;
        lda8((u8)(dc_adc8((u8)g_cpu.A, abs8(0x4217)) & 0xF8));
        dc_set_mx(0, 0);
        lda16((u16)(g_cpu.A << 2)); dp16_set(0x0C, g_cpu.A);
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x7C1F) | dp16(0x0C)));
        bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
        dc_set_mx(1, 0);
    } while (fade_next());
}

/* FD:A8CE  Resets battle slot dp14: record 7 * slot (offset from C1:FDCF) 93EE/93EF = 0,
 * 93F3 = FF; AFAB/99DD/9F22 + slot = B158 + slot; B188/B03A + slot = 0. */
DC_ENTRY(FDA8CE, RTS)
{
    tdc();
    lda8(dp8(0x14));
    asl_a(3);
    g_cpu._flag_C = 1;
    lda8(dc_sbc8((u8)g_cpu.A, dp8(0x14)));
    tax();
    dc_jsl(0xC1FDCF, 0xFDA8D8);
    abs8_set(0x93EE + g_cpu.X, 0); abs8_set(0x93EF + g_cpu.X, 0);
    lda8(0xFF); abs8_set(0x93F3 + g_cpu.X, 0xFF);
    tdc();
    lda8(dp8(0x14)); tax();
    lda8(abs8(0xB158 + g_cpu.X));
    abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x99DD + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x9F22 + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0xB188 + g_cpu.X, 0); abs8_set(0xB03A + g_cpu.X, 0);
}

/* Two 3-byte HDMA entries (E4 lines, F0 lines) at base for a window pair, then the end 00. */
static void window_hdma(u16 base, bool low)
{
    lda8(0xE4); abs8_set(base, 0xE4);
    ldx(low ? 0x18A8 : 0x17C8); abs16_set(base + 1, g_cpu.X);
    lda8(0xF0); abs8_set(base + 3, 0xF0);
    ldx(low ? 0x190C : 0x182C); abs16_set(base + 4, g_cpu.X);
    lda8(0x00); abs8_set(base + 6, 0x00);
}

/* FD:C439  HDMA list at 11E1, buffer by bit 0 of 0351 (clear: 18A8/190C). */
DC_ENTRY(FDC439, RTS)
{
    lda8(abs8(0x0351)); g_cpu._flag_C = g_cpu.A & 1; lda8((u8)((u8)g_cpu.A >> 1));
    window_hdma(0x11E1, !g_cpu._flag_C);
}

/* FD:C477  HDMA list at 1499, the other buffer (bit 0 set: 18A8/190C). */
DC_ENTRY(FDC477, RTS)
{
    lda8(abs8(0x0351)); g_cpu._flag_C = g_cpu.A & 1; lda8((u8)((u8)g_cpu.A >> 1));
    window_hdma(0x1499, g_cpu._flag_C);
}

/* FD:FFEE (JMP E275)  Clears the first byte of the twelve 12-byte records at 0520 (DB = 00). */
DC_ENTRY(FDFFEE, RTL)
{
    php();
    dc_push8(g_cpu.DB);
    lda8(0x00); g_cpu.DB = 0x00;
    dc_set_mx(1, 1);
    ldy(0x00);
    do {
        lda8(0x00);
        abs8_set(0x0520 + g_cpu.Y, 0x00);
        g_cpu._flag_C = 0;
        lda8(dc_adc8((u8)g_cpu.Y, 0x0C));
        tay();
        dc_cmp8((u8)g_cpu.A, 0x90);
    } while (!g_cpu._flag_Z);
    g_cpu.DB = dc_pull8();
    plp();
}

/* FD:C0D7  PPU registers through D = 2100: OBJ/BG modes, tile bases, window regions,
 * mosaic and scroll registers 0D-12 cleared (each written twice), color math. */
DC_ENTRY(FDC0D7, RTL)
{
    dc_push16(g_cpu.D);
    g_cpu.D = 0x2100;
    static const u8 set[][2] = { { 0x05, 0x09 }, { 0x0B, 0x11 }, { 0x0C, 0x05 }, { 0x07, 0x69 }, { 0x08, 0x71 }, { 0x09, 0x79 } };
    for (unsigned k = 0; k < 6; k++) dp8_set(set[k][0], set[k][1]);
    dp8_set(0x01, 0); dp8_set(0x33, 0);
    for (u8 r = 0x0D; r <= 0x12; r++) { dp8_set(r, 0); dp8_set(r, 0); }
    dp8_set(0x2C, 0x13); dp8_set(0x30, 0x02);
    lda8(0xE0); dp8_set(0x32, 0xE0);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* FD:B4E7  Battle timers: eleven entries of thirteen tables set to their start values
 * (B0D4/AF27 3C, B158 69, B0DF/AF32 96, ...). */
DC_ENTRY(FDB4E7, RTL)
{
    static const struct { u8 v; u16 a, b; } t[] = {
        { 0x3C, 0xB0D4, 0xAF27 }, { 0x69, 0xB158, 0 }, { 0x96, 0xB0DF, 0xAF32 }, { 0x5A, 0xB0EA, 0xAF3D },
        { 0x3C, 0xB116, 0xAF69 }, { 0x3C, 0xB121, 0xAF74 }, { 0x1E, 0xB12C, 0xAF7F }, { 0x5A, 0xB100, 0xAF53 },
        { 0x19, 0xB137, 0xAF8A }, { 0x6E, 0xB0F5, 0xAF48 }, { 0x3C, 0xB10B, 0xAF5E }, { 0x3C, 0xB142, 0xAF95 },
        { 0x78, 0xB14D, 0xAFA0 },
    };
    tdc(); tax();
    do {
        for (unsigned k = 0; k < sizeof t / sizeof t[0]; k++) {
            lda8(t[k].v);
            abs8_set(t[k].a + g_cpu.X, t[k].v);
            if (t[k].b) abs8_set(t[k].b + g_cpu.X, t[k].v);
        }
        inx(); cpx(0x000B);
    } while (!g_cpu._flag_C);
}

/* FD:A93C  Resets the battle slots 0-2 whose B3E7 + slot flag is set (FD:A8CE). */
DC_ENTRY(FDA93C, RTL)
{
    for (u8 s = 0; s < 3; s++) {
        lda8(abs8(0xB3E7 + s));
        if (g_cpu._flag_Z) continue;
        lda8(s); dp8_set(0x14, s);
        dc_FDA8CE_glue();
    }
}

/* FD:F204  Busy loop over the bytes at F800 + 4n (n = 0..63): dp80 ends as the last
 * (F800[x] + 80 >> 1) + 7F & 7F of the counted run; X and Y end 0. */
DC_ENTRY(FDF204, RTS)
{
    dc_set_mx(1, 1);
    ldx(0x00);
    do {
        lda8(abs8(0xF800 + g_cpu.X));
        g_cpu._flag_C = 0;
        u8 a = dc_adc8((u8)g_cpu.A, 0x80);
        g_cpu._flag_C = a & 1;
        a >>= 1;
        for (int y = 0x80; y; y--) { a &= 0x7F; dp8_set(0x80, a); a++; }
        dc_set_a8(a);
        ldy(0); ldx((u8)(g_cpu.X + 4));
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, 0);
}

/* HDMA list at base: with 00:1DFD clear two entries (64 and 70 lines of p2); else thirteen
 * 10-line entries of p13 and the end 00. */
/* Two-entry HDMA list: h1 lines of p1, h2 lines of p2, end. */
static void hdma_pair(u16 base, u8 h1, u16 p1, u8 h2, u16 p2)
{
    lda8(h1); abs8_set(base, h1);
    ldx(p1); abs16_set(base + 1, p1);
    lda8(h2); abs8_set(base + 3, h2);
    ldx(p2); abs16_set(base + 4, p2);
    lda8(0x00); abs8_set(base + 6, 0x00);
}

static void hdma_list13_if(u32 flag, u16 base, u16 p2, u16 p13)
{
    lda8(bus8(flag));
    if (g_cpu._flag_Z) { hdma_pair(base, 0x64, p2, 0x70, p2); return; }
    lda8(0x90);
    for (int k = 0; k < 13; k++) abs8_set(base + 3 * k, 0x90);
    abs8_set(base + 39, 0);
    ldx(p13);
    for (int k = 0; k < 13; k++) abs16_set(base + 1 + 3 * k, p13);
}

static void hdma_list13(u16 base, u16 p2, u16 p13) { hdma_list13_if(0x001DFD, base, p2, p13); }

/* FD:C52E  HDMA list at 12E6 (1514 / 1DA7). */
DC_ENTRY(FDC52E, RTS) { hdma_list13(0x12E6, 0x1514, 0x1DA7); }

/* FD:C4B5  HDMA list at 102E (1510 / 1D27). */
DC_ENTRY(FDC4B5, RTS) { hdma_list13(0x102E, 0x1510, 0x1D27); }

/* FD:EF23  (A * the 16-bit quotient in 4214) >> 6 as a word to the WRAM port (DB = 00);
 * uses the hardware multiplier twice, scratch dp0C..11. */
DC_ENTRY(FDEF23, RTS)
{
    abs8_set(0x4202, (u8)g_cpu.A);
    ldy(abs16(0x4214)); dp16_set(0x10, g_cpu.Y);
    lda8(dp8(0x10)); abs8_set(0x4203, (u8)g_cpu.A);
    dp8_set(0x0E, 0); dp8_set(0x0F, 0);
    ldy(abs16(0x4216)); dp16_set(0x0C, g_cpu.Y);
    lda8(dp8(0x11)); abs8_set(0x4203, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0x0D), abs16(0x4216)));
    dp16_set(0x0D, g_cpu.A);
    for (int k = 0; k < 6; k++) {
        u16 hi = dp16(0x0E), lo = dp16(0x0C);
        g_cpu._flag_C = hi & 1; hi >>= 1; dp16_set(0x0E, hi);
        u8 c = g_cpu._flag_C; g_cpu._flag_C = lo & 1; lo = (u16)(lo >> 1 | c << 15); dp16_set(0x0C, lo);
        dc_set_flags_nz16(lo);
    }
    lda16(dp16(0x0C));
    dc_set_mx(1, 0);
    abs8_set(0x2180, (u8)g_cpu.A);
    xba();
    abs8_set(0x2180, (u8)g_cpu.A);
    lda8(0x00);
}

/* Scaled fade with any of the channels: blue and red through the multiplier into the word
 * at dp0C (with the 7C1F mask), then green scaled (bits 5-9, as 8-bit c << 3) or kept. */
static void fade_mix(bool blue, bool red, bool green)
{
    if (blue) { mul_hi((u8)(bus8(0x7E2001 + g_cpu.X) & 0x7C)); xba(); }
    else if (red) { lda8(bus8(0x7E2001 + g_cpu.X)); xba(); }
    if (red) mul_hi((u8)(bus8(0x7E2000 + g_cpu.X) & 0x1F));
    else lda8(bus8(0x7E2000 + g_cpu.X));
    dc_set_mx(0, 0);
    if (blue || red) lda16((u16)(g_cpu.A & 0x7C1F));
    else lda16((u16)(bus16(0x7E2000 + g_cpu.X) & 0x7C1F));
    dp16_set(0x0C, g_cpu.A);
    if (green) {
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03E0) >> 2));
        dc_set_mx(1, 0);
        mul_hi((u8)g_cpu.A);
        lda8((u8)(g_cpu.A & 0xF8));
        dc_set_mx(0, 0);
        lda16((u16)((g_cpu.A << 2) | dp16(0x0C)));
    } else {
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03E0) | dp16(0x0C)));
    }
    bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
    dc_set_mx(1, 0);
}

/* FD:E73A  Red and blue scaled. */
DC_ENTRY(FDE73A, RTS) { fade_count(); do fade_mix(true, true, false); while (fade_next()); }

/* FD:E6D1  Green scaled. */
DC_ENTRY(FDE6D1, RTS) { fade_count(); do fade_mix(false, false, true); while (fade_next()); }

/* FD:E779  Red and green scaled. */
DC_ENTRY(FDE779, RTS) { fade_count(); do fade_mix(false, true, true); while (fade_next()); }

/* FD:E7C0  Blue and green scaled. */
DC_ENTRY(FDE7C0, RTS) { fade_count(); do fade_mix(true, false, true); while (fade_next()); }

/* FD:E64F  All three channels scaled. */
DC_ENTRY(FDE64F, RTS) { fade_count(); do fade_mix(true, true, true); while (fade_next()); }

/* FD:E9CA  Red and blue toward white. */
DC_ENTRY(FDE9CA, RTS)
{
    fade_count();
    do {
        lda8((u8)(bus8(0x7E2001 + g_cpu.X) & 0x7C)); dp8_set(0x0C, (u8)g_cpu.A);
        abs8_set(0x4203, (u8)(g_cpu.A ^ 0x7C));
        lda8(dp8(0x0C)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0x4217)));
        xba();
        lda8((u8)(bus8(0x7E2000 + g_cpu.X) & 0x1F)); dp8_set(0x0C, (u8)g_cpu.A);
        abs8_set(0x4203, (u8)(g_cpu.A ^ 0x1F));
        lda8(dp8(0x0C)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(0x4217)));
        dc_set_mx(0, 0);
        lda16((u16)(g_cpu.A & 0x7C1F)); dp16_set(0x0C, g_cpu.A);
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03E0) | dp16(0x0C)));
        bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
        dc_set_mx(1, 0);
    } while (fade_next());
}

/* One channel toward white: c + (max - c) * level (multiplier 4202) as 8-bit; scratch at dp. */
static void whiten8(u8 c, u8 max, u8 dp)
{
    dp8_set(dp, c);
    abs8_set(0x4203, (u8)(c ^ max));
    lda8(dp8(dp));
    g_cpu._flag_C = 0;
    lda8(dc_adc8((u8)g_cpu.A, abs8(0x4217)));
}

/* Toward white with any of the channels; channels not listed are dropped, except green,
 * which is kept when not whitened (as the original does). */
static void whiten_mix(bool blue, bool red, bool green)
{
    if (blue) { whiten8((u8)(bus8(0x7E2001 + g_cpu.X) & 0x7C), 0x7C, 0x0C); xba(); }
    if (red) whiten8((u8)(bus8(0x7E2000 + g_cpu.X) & 0x1F), 0x1F, 0x0C);
    dc_set_mx(0, 0);
    lda16((u16)(g_cpu.A & (blue && red ? 0x7C1F : blue ? 0x7C00 : 0x001F)));
    dp16_set(0x0C, g_cpu.A);
    if (green) {
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03E0) >> 2));
        dc_set_mx(1, 0);
        whiten8((u8)g_cpu.A, 0xF8, 0x0E);
        lda8((u8)(g_cpu.A & 0xF8));
        dc_set_mx(0, 0);
        lda16((u16)((g_cpu.A << 2) | dp16(0x0C)));
    } else {
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03E0) | dp16(0x0C)));
    }
    bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
    dc_set_mx(1, 0);
}

/* FD:EA11  Blue and green toward white (red dropped). */
DC_ENTRY(FDEA11, RTS) { fade_count(); do whiten_mix(true, false, true); while (fade_next()); }

/* FD:EA5C  Red and green toward white (blue dropped). */
DC_ENTRY(FDEA5C, RTS) { fade_count(); do whiten_mix(false, true, true); while (fade_next()); }

/* Palette rotation by one color over 0522 + Y colors from 0521 + Y, in both 7E:2000 and
 * 7E:2200 (DB = 7E during the copy, 00 after). */
static void palette_rotate(bool up)
{
    lda8(abs8(0x0522 + g_cpu.Y));
    lda8(asl8((u8)g_cpu.A)); dp8_set(0x12, (u8)g_cpu.A); dp8_set(0x13, 0);
    lda8(abs8(0x0521 + g_cpu.Y));
    dc_set_mx(0, 0);
    g_cpu._flag_C = g_cpu.A >> 15; g_cpu.A = (u16)(g_cpu.A << 1); dc_set_flags_nz16(g_cpu.A);
    if (up) { dp16_set(0x10, g_cpu.A); lda16(dc_adc16(g_cpu.A, dp16(0x12))); tax(); }
    else { tax(); lda16(dc_adc16(g_cpu.A, dp16(0x12))); dp16_set(0x10, g_cpu.A); }
    dc_set_mx(1, 0);
    lda8(0x7E); g_cpu.DB = 0x7E;
    dc_set_mx(0, 0);
    lda16(abs16(0x2000 + g_cpu.X)); dp16_set(0x0C, g_cpu.A);
    lda16(abs16(0x2200 + g_cpu.X)); dp16_set(0x0E, g_cpu.A);
    do {
        int d = up ? -2 : 2;
        lda16(abs16(0x2000 + d + g_cpu.X)); abs16_set(0x2000 + g_cpu.X, g_cpu.A);
        lda16(abs16(0x2200 + d + g_cpu.X)); abs16_set(0x2200 + g_cpu.X, g_cpu.A);
        ldx((u16)(g_cpu.X + d));
        dc_cmp16(g_cpu.X, dp16(0x10));
    } while (!g_cpu._flag_Z);
    lda16(dp16(0x0C)); abs16_set(0x2000 + g_cpu.X, g_cpu.A);
    lda16(dp16(0x0E)); abs16_set(0x2200 + g_cpu.X, g_cpu.A);
    lda16(0x0000);
    dc_set_mx(1, 0);
    g_cpu.A = g_cpu.D;
    g_cpu.DB = (u8)g_cpu.A;
    dc_set_flags_nz8(g_cpu.DB);
}

/* FD:E4E8  Rotate the palette range up (last color to the first). */
DC_ENTRY(FDE4E8, RTS) { palette_rotate(true); }

/* FD:E532  Rotate the palette range down (first color to the last). */
DC_ENTRY(FDE532, RTS) { palette_rotate(false); }

/* FD:B3FE  Enemy X with AEFF + X set: 5E6C..6F + Y = 4, then 5 at the slot picked by the
 * first of bits 7/6/5/4 of 5E2E + Y (offset 0/1/2/3); none: stays 4. */
DC_ENTRY(FDB3FE, RTL)
{
    lda8(abs8(0xAEFF + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    lda8(0x04);
    for (int k = 0; k < 4; k++) abs8_set(0x5E6C + k + g_cpu.Y, 0x04);
    lda8(abs8(0x5E2E + g_cpu.Y));
    u8 v = (u8)g_cpu.A;
    int n = (v & 0x80) ? 0 : (v & 0x40) ? 1 : (v & 0x20) ? 2 : (v & 0x10) ? 3 : -1;
    g_cpu._flag_Z = n < 0;
    if (n < 0) return;
    for (int k = 0; k < n; k++) iny();
    lda8(0x05); abs8_set(0x5E6C + g_cpu.Y, 0x05);
}

/* FD:F123  Wave on 7E:2020..205F (colors from dp08): red and green parts from the sine
 * table F800 at dp1B and its mirror (FF - dp1B), scaled by FD:F181 + X; blue kept. */
DC_ENTRY(FDF123, RTS)
{
    ldx(dp16(0x08));
    do {
        lda8(dp8(0x1B)); tay();
        lda8(abs8(0xF800 + g_cpu.Y)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x80));
        abs8_set(0x4202, (u8)g_cpu.A);
        lda8(bus8(0xFDF181 + g_cpu.X)); abs8_set(0x4203, (u8)g_cpu.A);
        lda8(abs8(0x4217)); dp8_set(0x0E, (u8)g_cpu.A); dp8_set(0x0F, 0);
        lda8((u8)(dp8(0x1B) ^ 0xFF)); tay();
        lda8(abs8(0xF800 + g_cpu.Y)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x80));
        abs8_set(0x4202, (u8)g_cpu.A);
        lda8(bus8(0xFDF181 + g_cpu.X)); abs8_set(0x4203, (u8)g_cpu.A);
        lda8(abs8(0x4217));
        xba();
        dc_set_mx(0, 0);
        lda16((u16)((g_cpu.A >> 3) | dp16(0x0E))); dp16_set(0x0E, g_cpu.A);
        lda16((u16)((bus16(0x7E2020 + g_cpu.X) & 0x7C00) | dp16(0x0E)));
        bus16_set(0x7E2020 + g_cpu.X, g_cpu.A);
        lda16(0x0000);
        dc_set_mx(1, 0);
        inx(); inx(); cpx(0x0040);
    } while (!g_cpu._flag_Z);
}

/* FD:E437  Loads 0522 + Y colors of palette set 0526 + Y + 2 * (0523 + Y) * (0522 + Y) from
 * F6:0000 into 7E:2200 + 2 * (0521 + Y). */
DC_ENTRY(FDE437, RTS)
{
    lda8(abs8(0x0521 + g_cpu.Y));
    dc_set_mx(0, 0);
    g_cpu.A = (u16)(g_cpu.A << 1);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(g_cpu.A, 0x2200)); dp16_set(0x18, g_cpu.A);
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8(0x7E); dp8_set(0x1A, 0x7E);
    lda8(abs8(0x0523 + g_cpu.Y)); abs8_set(0x4202, (u8)g_cpu.A);
    lda8(abs8(0x0522 + g_cpu.Y)); lda8(asl8((u8)g_cpu.A)); abs8_set(0x4203, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0x0526 + g_cpu.Y), abs16(0x4216)));
    tax();
    lda16(0x0000);
    dc_set_mx(1, 0);
    dc_push16(g_cpu.Y);
    lda8(abs8(0x0522 + g_cpu.Y)); tay();
    dc_set_mx(0, 0);
    do {
        lda16(bus16(0xF60000 + g_cpu.X));
        bus16_set(lp(0x18), g_cpu.A);
        dp16_set(0x18, (u16)(dp16(0x18) + 2));
        inx(); inx();
        ldy((u16)(g_cpu.Y - 1));
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, 0);
    lda8(0x00); xba();
    ldy(dc_pull16());
}

/* FD:AE52  Copies party member AEFF + Y's 50-byte record (2600 + 50 * id) to the address at
 * FD:A80B + 2Y and clears it up to 80 bytes; negative id: nothing. */
DC_ENTRY(FDAE52, RTL)
{
    tdc();
    lda8(abs8(0xAEFF + g_cpu.Y));
    if (g_cpu._flag_N) return;
    tax(); stx_dp(0x02);
    lda8((u8)g_cpu.Y);
    lda8(asl8((u8)g_cpu.A));
    tax(); stx_dp(0x00);
    dc_set_mx(0, 0);
    ldx(dp16(0x00));
    lda16(bus16(0xFDA80B + g_cpu.X));
    tay();
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    dp8_set(0x00, (u8)g_cpu.A);
    ldx(dp16(0x02)); stx_dp(0x2A);
    ldx(0x0050); stx_dp(0x28);
    dc_jsl(0xC1FDBF, 0xFDAE77);
    ldx(dp16(0x2C));
    do {
        lda8(abs8(0x2600 + g_cpu.X)); abs8_set(g_cpu.Y, (u8)g_cpu.A);
        iny(); inx();
        dp8_set(0x00, (u8)(dp8(0x00) + 1));
        lda8(dp8(0x00)); dc_cmp8((u8)g_cpu.A, 0x50);
    } while (!g_cpu._flag_C);
    do {
        tdc();
        abs8_set(g_cpu.Y, (u8)g_cpu.A);
        iny();
        dp8_set(0x00, (u8)(dp8(0x00) + 1));
        lda8(dp8(0x00));
    } while (!g_cpu._flag_N);
}

/* FD:B480  Rest of an enemy record from CC:4700 + X + 1 into the slot at Y: 5FCA..CC = 0,
 * 5 bytes to 5FCF (5FCD/CE = the 4th/5th), 12 bytes to 5FE4, 3 bytes to 5FC6. */
static void enemy_record_tail(void)
{
    dc_push16(g_cpu.Y);
    tdc(); dp8_set(0x00, (u8)g_cpu.A);
    for (int k = 0; k < 3; k++) abs8_set(0x5FCA + k + g_cpu.Y, (u8)g_cpu.A);
    do {
        inx(); lda8(bus8(0xCC4700 + g_cpu.X)); abs8_set(0x5FCF + g_cpu.Y, (u8)g_cpu.A); iny();
        dp8_set(0x00, (u8)(dp8(0x00) + 1)); lda8(dp8(0x00)); dc_cmp8((u8)g_cpu.A, 0x05);
    } while (!g_cpu._flag_C);
    ldy(dc_pull16());
    lda8(abs8(0x5FD2 + g_cpu.Y)); abs8_set(0x5FCD + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x5FD3 + g_cpu.Y)); abs8_set(0x5FCE + g_cpu.Y, (u8)g_cpu.A);
    dc_push16(g_cpu.Y);
    tdc(); dp8_set(0x00, (u8)g_cpu.A);
    do {
        inx(); lda8(bus8(0xCC4700 + g_cpu.X)); abs8_set(0x5FE4 + g_cpu.Y, (u8)g_cpu.A); iny();
        dp8_set(0x00, (u8)(dp8(0x00) + 1)); lda8(dp8(0x00)); dc_cmp8((u8)g_cpu.A, 0x0C);
    } while (!g_cpu._flag_C);
    ldy(dc_pull16());
    tdc(); dp8_set(0x00, (u8)g_cpu.A);
    do {
        inx(); lda8(bus8(0xCC4700 + g_cpu.X)); abs8_set(0x5FC6 + g_cpu.Y, (u8)g_cpu.A); iny();
        dp8_set(0x00, (u8)(dp8(0x00) + 1)); lda8(dp8(0x00)); dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_C);
}

/* FD:B479  Entered 7 bytes into FD:B438: a CPY and an EOR whose results are overwritten,
 * then the record tail. */
DC_ENTRY(FDB479, RTL)
{
    dc_cmp16(g_cpu.Y, abs16(0xBF99));
    lda8((u8)(g_cpu.A ^ bus8(0x5FBF99 + g_cpu.X)));
    enemy_record_tail();
}

/* FD:B438  Enemy slot Y (dp02 high byte / 2) from enemy id AF0D+X: record at CC:4700 +
 * 23 * id; FD:B4D6 clears the slot, then id to 5FAD, the first two bytes twice (5FB0/B2,
 * 5FB1/B3), the third to 5FBF, and the record tail. */
DC_ENTRY(FDB438, RTL)
{
    tdc(); tay(); dp16_set(0x04, g_cpu.Y);
    lda8(abs8(0xAF0D + g_cpu.X)); dp8_set(0x04, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    asl_a(3); dp16_set(0x00, g_cpu.A);
    asl_a(1);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x00)));
    g_cpu._flag_C = 1; lda16(dc_sbc16(g_cpu.A, dp16(0x04)));
    tax();
    lda16(dp16(0x02)); xba(); lsr_a(1); tay();
    tdc();
    dc_set_mx(1, 0);
    dc_FDB4D6_glue();
    lda8(dp8(0x04)); abs8_set(0x5FAD + g_cpu.Y, (u8)g_cpu.A);
    lda8(bus8(0xCC4700 + g_cpu.X)); abs8_set(0x5FB0 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x5FB2 + g_cpu.Y, (u8)g_cpu.A);
    inx();
    lda8(bus8(0xCC4700 + g_cpu.X)); abs8_set(0x5FB1 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0x5FB3 + g_cpu.Y, (u8)g_cpu.A);
    inx();
    lda8(bus8(0xCC4700 + g_cpu.X)); abs8_set(0x5FBF + g_cpu.Y, (u8)g_cpu.A);
    enemy_record_tail();
}

/* FD:E8D7  All three channels toward white. */
DC_ENTRY(FDE8D7, RTS)
{
    fade_count();
    do {
        whiten8((u8)(bus8(0x7E2001 + g_cpu.X) & 0x7C), 0x7C, 0x0D);
        xba();
        whiten8((u8)(bus8(0x7E2000 + g_cpu.X) & 0x1F), 0x1F, 0x0C);
        dc_set_mx(0, 0);
        lda16((u16)(g_cpu.A & 0x7C1F)); dp16_set(0x0C, g_cpu.A);
        lda16((u16)((bus16(0x7E2000 + g_cpu.X) & 0x03E0) >> 2));
        dc_set_mx(1, 0);
        whiten8((u8)g_cpu.A, 0xF8, 0x0E);
        lda8((u8)(g_cpu.A & 0xF8));
        dc_set_mx(0, 0);
        lda16((u16)((g_cpu.A << 2) | dp16(0x0C)));
        bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
        dc_set_mx(1, 0);
    } while (fade_next());
}

/* FD:F1ED  Through D = 2100: WRAM port at 7F:0000, then the F204 loop twice (its dp80
 * stores feed the port). */
DC_ENTRY(FDF1ED, RTS)
{
    dc_push16(g_cpu.D);
    dc_push16(0x2100); g_cpu.D = dc_pull16();
    ldx(0x0000); dp16_set(0x81, 0x0000);
    lda8(0x7F); dp8_set(0x83, 0x7F);
    dc_FDF204_glue();
    dc_FDF204_glue();
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* FD:B0E2  B045+X.. up to 8E set to 0A; then 9F2C..9F34 cleared and, for each of the 8
 * slots whose AF0D entry is not FF, 9F2C+n = 2 unless bit 1 of 5FC7 + n*80 is set. */
static void battle_slots_reset(void)
{
    lda8(0x0A);
    do { abs8_set(0xB045 + g_cpu.X, 0x0A); inx(); cpx(0x008F); } while (!g_cpu._flag_C);
    tdc(); tax();
    do { abs8_set(0x9F2C + g_cpu.X, 0); inx(); cpx(0x0008); } while (!g_cpu._flag_C);
    abs8_set(0x9F34, 0);
    tdc(); tax(); tay();
    do {
        lda8(abs8(0xAF0D + g_cpu.Y)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            lda8((u8)((abs8(0x5FC7 + g_cpu.X) & 0x02) ^ 0x02));
            abs8_set(0x9F2C + g_cpu.Y, (u8)g_cpu.A);
        }
        iny();
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; ldx(dc_adc16(g_cpu.X, 0x0080));
        tdc();
        dc_set_mx(1, 0);
        cpy(0x0008);
    } while (!g_cpu._flag_C);
}

/* FD:B0DF  Entered one byte early: an SBC [7B],Y whose result becomes X, then B0E2. */
DC_ENTRY(FDB0DF, RTL)
{
    lda8(dc_sbc8((u8)g_cpu.A, bus8((lp(0x7B) + g_cpu.Y) & 0xFFFFFF)));
    tax();
    battle_slots_reset();
}

/* FD:B0D5  AFB6..B044 = low byte of D (0), then B0E2 from X = 0. */
DC_ENTRY(FDB0D5, RTL)
{
    tdc(); tax();
    do { abs8_set(0xAFB6 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x008F); } while (!g_cpu._flag_Z);
    tdc(); tax();
    battle_slots_reset();
}

/* FD:DCBD  Mode 7 style HDMA parameters at 7F:701C.. from the angle in dp1B, then the
 * 7E:2102 list shifted down two words from dp08 on, its first two words moved to the end. */
DC_ENTRY(FDDCBD, RTS)
{
    lda8(0x80); bus8_set(0x7F701C, 0x80); bus8_set(0x7F7020, 0x80);
    lda8(0x00);
    static const u16 zero[] = { 0x701D, 0x7021, 0x7024, 0x7025, 0x7028, 0x7029 };
    for (unsigned k = 0; k < 6; k++) bus8_set(0x7F0000 | zero[k], 0);
    lda8(dp8(0x1B));
    dc_set_mx(0, 0);
    asl_a(2); lda16((u16)(g_cpu.A & 0x01FF)); bus16_set(0x7F7026, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0070)); bus16_set(0x7F701E, g_cpu.A);
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8((u8)(dp8(0x1B) ^ 0xFF));
    dc_set_mx(0, 0);
    asl_a(2);
    g_cpu._flag_C = 1; lda16(dc_sbc16(g_cpu.A, 0x0070));
    lda16((u16)(g_cpu.A & 0x01FF)); bus16_set(0x7F702A, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x0070)); bus16_set(0x7F7022, g_cpu.A);
    lda16(0x0000);
    dc_set_mx(1, 0);
    ldx(dp16(0x08));
    dc_set_mx(0, 0);
    lda16(bus16(0x7E2102)); dp16_set(0x0C, g_cpu.A);
    lda16(bus16(0x7E2104)); dp16_set(0x0E, g_cpu.A);
    do {
        lda16(bus16(0x7E2106 + g_cpu.X)); bus16_set(0x7E2102 + g_cpu.X, g_cpu.A);
        inx(); inx(); cpx(0x001A);
    } while (!g_cpu._flag_Z);
    bus16_set(0x7E211C, dp16(0x0C));
    bus16_set(0x7E211E, dp16(0x0E));
    lda16(0x0000);
    dc_set_mx(1, 0);
}

/* FD:B77D  Battle menu state reset: B263+X.. cleared and B26B+X.. set to FF (up to 8),
 * AECC/AED8 x11 = FF, B2B6/AEB3/AE85/B320/AE7D x8 and AEB2 = low byte of D, B202 = 0,
 * B3CE x24 = FF, B30F/B310 = 0, B3BE = 3, B263/B26B x8 = FF, 93F3/93FA/9401 = FF. */
DC_ENTRY(FDB77D, RTL)
{
    tax();
    lda8(0xFF);
    do { abs8_set(0xB263 + g_cpu.X, 0); abs8_set(0xB26B + g_cpu.X, 0xFF); inx(); cpx(0x0008); } while (!g_cpu._flag_C);
    tdc(); tax(); lda8(0xFF);
    do { abs8_set(0xAECC + g_cpu.X, 0xFF); abs8_set(0xAED8 + g_cpu.X, 0xFF); inx(); cpx(0x000B); } while (!g_cpu._flag_C);
    tdc(); abs8_set(0xAEB2, (u8)g_cpu.A); tax();
    static const u16 t[] = { 0xB2B6, 0xAEB3, 0xAE85, 0xB320, 0xAE7D };
    do {
        for (unsigned k = 0; k < 5; k++) abs8_set(t[k] + g_cpu.X, (u8)g_cpu.A);
        inx(); cpx(0x0008);
    } while (!g_cpu._flag_C);
    abs8_set(0xB202, 0);
    tdc(); tax(); lda8(0xFF);
    do { abs8_set(0xB3CE + g_cpu.X, 0xFF); inx(); cpx(0x0018); } while (!g_cpu._flag_C);
    abs8_set(0xB30F, 0); abs8_set(0xB310, 0);
    lda8(0x03); abs8_set(0xB3BE, 0x03);
    tdc(); tax(); lda8(0xFF);
    do { abs8_set(0xB263 + g_cpu.X, 0xFF); abs8_set(0xB26B + g_cpu.X, 0xFF); inx(); cpx(0x0008); } while (!g_cpu._flag_C);
    abs8_set(0x93F3, 0xFF); abs8_set(0x93FA, 0xFF); abs8_set(0x9401, 0xFF);
}

/* FD:AF80  Enemy list entry for id A (dp06): ids from BC with bit 7 of their CC:0ABC
 * record get a 5-byte entry at 1580 + dp04 (id, CC:1BB5 byte, record & 7F, dp0E) and
 * dp04 += 5; others count in AF23 (cleared first). dp00 and dp08 always advance. */
DC_ENTRY(FDAF80, RTL)
{
    abs8_set(0xAF23, 0);
    dp8_set(0x06, (u8)g_cpu.A);
    dc_cmp8((u8)g_cpu.A, 0x5A);
    lda8(dp8(0x06)); dc_cmp8((u8)g_cpu.A, 0xBC);
    int found = 0;
    if (g_cpu._flag_C) {
        lda8(dp8(0x06)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, 0xBC));
        tax(); dp16_set(0x0A, g_cpu.X);
        lda8(asl8((u8)g_cpu.A)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x0A)));
        tax(); dp16_set(0x02, g_cpu.X);
        ldx(dp16(0x02));
        lda8(bus8(0xCC0ABC + g_cpu.X)); g_cpu._flag_Z = !(g_cpu.A & 0x80);
        found = !g_cpu._flag_Z;
    }
    if (found) {
        u16 e = dp16(0x04);
        lda8(dp8(0x06)); ldx(e); abs8_set(0x1580 + e, (u8)g_cpu.A);
        ldx(dp16(0x0A)); lda8(bus8(0xCC1BB5 + g_cpu.X)); ldx(e); abs8_set(0x1581 + e, (u8)g_cpu.A);
        ldx(dp16(0x02)); lda8((u8)(bus8(0xCC0ABC + g_cpu.X) & 0x7F)); ldx(e); abs8_set(0x1582 + e, (u8)g_cpu.A);
        lda8(dp8(0x0E)); ldx(e); abs8_set(0x1583 + e, (u8)g_cpu.A);
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; lda16(dc_adc16(e, 0x0005)); dp16_set(0x04, g_cpu.A);
        tdc();
        dc_set_mx(1, 0);
    } else {
        inc_abs(0xAF23);
    }
    u8 v = (u8)(dp8(0x00) + 1); dp8_set(0x00, v);
    v = (u8)(dp8(0x08) + 1); dp8_set(0x08, v); dc_set_flags_nz8(v);
}

/* FD:E485  Palette record at 0521+Y: count (0522) colours from F6:0000 + 0526 + 2 * count
 * * the byte at the 0529 pointer (+0523, bank 052B) into 7E:2200 + 2 * first (0521). */
DC_ENTRY(FDE485, RTS)
{
    lda8(abs8(0x0523 + g_cpu.Y));
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0x0529 + g_cpu.Y))); dp16_set(0x18, g_cpu.A);
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8(abs8(0x052B + g_cpu.Y)); dp8_set(0x1A, (u8)g_cpu.A);
    lda8(bus8(lp(0x18)));
    dc_push8((u8)g_cpu.A);
    lda8(abs8(0x0521 + g_cpu.Y));
    dc_set_mx(0, 0);
    asl_a(1); g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, 0x2200)); dp16_set(0x18, g_cpu.A);
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8(0x7E); dp8_set(0x1A, 0x7E);
    lda8(dc_pull8()); abs8_set(0x4202, (u8)g_cpu.A);
    lda8(asl8(abs8(0x0522 + g_cpu.Y))); abs8_set(0x4203, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    lda16(abs16(0x0526 + g_cpu.Y));
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0x4216)));
    tax();
    lda16(0x0000);
    dc_set_mx(1, 0);
    dc_push16(g_cpu.Y);
    lda8(abs8(0x0522 + g_cpu.Y)); tay();
    dc_set_mx(0, 0);
    do {
        lda16(bus16(0xF60000 + g_cpu.X)); bus16_set(lp(0x18), g_cpu.A);
        dp16_set(0x18, (u16)(dp16(0x18) + 2));
        inx(); inx();
        ldy((u16)(g_cpu.Y - 1));
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, 0);
    lda8(0x00); xba();
    ldy(dc_pull16());
}

/* FD:B2DE  When 2989 bit 5 is set: B33F, then eight 50-byte rows at 2603: four words from
 * the FD table picked by 29AC (FD:B99C), and 23 / 80 / 23 at +8 / +9 / +B. */
DC_ENTRY(FDB2DE, RTL)
{
    lda8(abs8(0x2989));
    g_cpu._flag_Z = !(g_cpu.A & 0x20);
    if (g_cpu._flag_Z) return;
    dc_FDB33F_glue();
    tdc();
    lda8(asl8(abs8(0x29AC))); tax();
    dc_set_mx(0, 0);
    lda16(bus16(0xFDB99C + g_cpu.X)); tax();
    tdc();
    dc_set_mx(1, 0);
    tdc(); tay();
    do {
        lda8(0x23); abs8_set(0x260B + g_cpu.Y, 0x23); abs8_set(0x260E + g_cpu.Y, 0x23);
        lda8(0x80); abs8_set(0x260C + g_cpu.Y, 0x80);
        dc_set_mx(0, 0);
        for (int k = 0; k < 4; k++) {
            lda16(bus16(0xFD0000 + g_cpu.X)); abs16_set(0x2603 + 2 * k + g_cpu.Y, g_cpu.A);
            if (k < 3) { inx(); inx(); }
        }
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.Y, 0x0050)); tay();
        tdc();
        inx(); inx();
        dc_set_mx(1, 0);
        cpy(0x0280);
    } while (!g_cpu._flag_C);
}

/* FD:EF6F  Through the WRAM port: 7E:2000..21FF cleared, then 64 words at 7E:2000 from the
 * FD:F001 / F081 tables (low | high << 10, from dp08 on) and 128 words at 7E:2100 from
 * FD:F101 (<< 10, 16 entries). */
DC_ENTRY(FDEF6F, RTS)
{
    ldx(0x2000); abs16_set(0x2181, 0x2000);
    lda8(0x7E); abs8_set(0x2183, 0x7E);
    ldy(0x0200);
    do { abs8_set(0x2180, 0); ldy((u16)(g_cpu.Y - 1)); } while (!g_cpu._flag_Z);
    ldx(0x2000); abs16_set(0x2181, 0x2000);
    lda8(0x7E); abs8_set(0x2183, 0x7E);
    ldy(0x0040);
    dc_set_mx(0, 0);
    lda16(dp16(0x08)); dp16_set(0x0C, g_cpu.A);
    do {
        ldx((u16)((dp16(0x0C) & 0x3F) << 1));
        lda16(bus16(0xFDF001 + g_cpu.X)); dp16_set(0x0E, g_cpu.A);
        lda16(bus16(0xFDF081 + g_cpu.X)); xba(); asl_a(2);
        lda16((u16)(g_cpu.A | dp16(0x0E)));
        abs8_set(0x2180, (u8)g_cpu.A); abs8_set(0x2180, (u8)(g_cpu.A >> 8));
        dp16_set(0x0C, (u16)(dp16(0x0C) + 1));
        ldy((u16)(g_cpu.Y - 1));
    } while (!g_cpu._flag_Z);
    lda16(0x0000);
    dc_set_mx(1, 0);
    ldx(0x2100); abs16_set(0x2181, 0x2100);
    lda8(0x7E); abs8_set(0x2183, 0x7E);
    ldy(0x0080);
    dc_set_mx(0, 0);
    lda16(dp16(0x08)); dp16_set(0x0C, g_cpu.A);
    do {
        ldx((u16)((dp16(0x0C) & 0x0F) << 1));
        lda16(bus16(0xFDF101 + g_cpu.X)); xba(); asl_a(2);
        abs8_set(0x2180, (u8)g_cpu.A); abs8_set(0x2180, (u8)(g_cpu.A >> 8));
        dp16_set(0x0C, (u16)(dp16(0x0C) + 1));
        ldy((u16)(g_cpu.Y - 1));
    } while (!g_cpu._flag_Z);
    lda16(0x0000);
    dc_set_mx(1, 0);
}

/* FD:ECD5  Plot colour A at pixel (dp15, dp17) of the 4-plane bitmap at 7F:4000: each
 * plane bit sets (mask 0525+x&7) or clears (mask 052D+x&7) the pixel. */
DC_ENTRY(FDECD5, RTS)
{
    dp8_set(0x0A, (u8)g_cpu.A);
    lda8((u8)((dp8(0x17) & 0x07) << 1)); dp8_set(0x18, (u8)g_cpu.A);
    dp8_set(0x19, 0);
    lda8((u8)((dp8(0x17) & 0xF8) >> 2)); xba();
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x18))); dp16_set(0x18, g_cpu.A);
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8((u8)(dp8(0x15) & 0xF8));
    dc_set_mx(0, 0);
    asl_a(2);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0x18))); tax();
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8((u8)(dp8(0x15) & 0x07)); tay();
    static const u8 plane[] = { 0x00, 0x01, 0x10, 0x11 };
    for (unsigned k = 0; k < 4; k++) {
        u8 v = dp8(0x0A); g_cpu._flag_C = v & 1; dp8_set(0x0A, v >> 1);
        u32 a = 0x7F4000 + plane[k] + g_cpu.X;
        if (g_cpu._flag_C) lda8((u8)(bus8(a) | abs8(0x0525 + g_cpu.Y)));
        else lda8((u8)(bus8(a) & abs8(0x052D + g_cpu.Y)));
        bus8_set(a, (u8)g_cpu.A);
    }
}

/* FD:B732  Battle state reset: dp16..21, AF25 and a list of B1/B2/B3/AE flags = 0 (low
 * byte of D), B18E..B1BD and B2A7..B2AC = 0, then FD:B77D. */
DC_ENTRY(FDB732, RTL)
{
    tdc(); tax();
    for (u8 o = 0x16; o <= 0x20; o += 2) dp16_set(o, g_cpu.X);
    abs16_set(0xAF25, g_cpu.X);
    static const u16 b[] = { 0xB1FC, 0xB253, 0xAECB, 0xB2C0, 0xB3B9 };
    for (unsigned k = 0; k < 5; k++) abs8_set(b[k], (u8)g_cpu.A);
    abs16_set(0xB28C, g_cpu.X); abs16_set(0xB2A5, g_cpu.X); abs16_set(0xB2DB, g_cpu.X);
    abs8_set(0xB2DD, (u8)g_cpu.A); abs8_set(0xB2AF, (u8)g_cpu.A);
    abs16_set(0xB2B0, g_cpu.X); abs16_set(0xB2B2, g_cpu.X); abs16_set(0xB2B4, g_cpu.X);
    do { abs8_set(0xB18E + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0030); } while (!g_cpu._flag_C);
    tax();
    do { abs8_set(0xB2A7 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0006); } while (!g_cpu._flag_C);
    dc_FDB77D_glue();
}

/* FD:B363  Enemy slot Y (AEFF+Y not FF): when 5E79 bit 7 is set, OR 5E7B into the byte
 * 5E4A + 5E7A; enemies of type A0 / A1 (5E57) get 5E32 raised by 1/4 / 1/2 (max 999). */
DC_ENTRY(FDB363, RTL)
{
    tdc(); tax();
    dp16_set(0x00, g_cpu.X); dp16_set(0x02, g_cpu.X); dp16_set(0x04, g_cpu.X);
    lda8(abs8(0xAEFF + g_cpu.Y)); dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    dc_set_mx(0, 0);
    g_cpu.A = g_cpu.Y; xba(); lsr_a(1); tax(); dp16_set(0x02, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    lda8(abs8(0x5E79 + g_cpu.X));
    if (g_cpu.A & 0x80) {
        tdc();
        lda8(abs8(0x5E7B + g_cpu.X)); dp8_set(0x00, (u8)g_cpu.A);
        lda8(abs8(0x5E7A + g_cpu.X)); dp8_set(0x04, (u8)g_cpu.A);
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.X, dp16(0x04))); tax();
        tdc();
        dc_set_mx(1, 0);
        lda8((u8)(abs8(0x5E4A + g_cpu.X) | dp8(0x00))); abs8_set(0x5E4A + g_cpu.X, (u8)g_cpu.A);
    }
    ldx(dp16(0x02));
    lda8(abs8(0x5E57 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xA0);
    int shift = 0;
    if (g_cpu._flag_Z) shift = 2;
    else { lda8(abs8(0x5E57 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xA1); if (g_cpu._flag_Z) shift = 1; }
    if (shift) {
        dc_set_mx(0, 0);
        lda16(abs16(0x5E32 + g_cpu.X)); lsr_a(shift);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0x5E32 + g_cpu.X)));
        dc_cmp16(g_cpu.A, 0x03E7);
        if (g_cpu._flag_C) lda16(0x03E7);
        abs16_set(0x5E32 + g_cpu.X, g_cpu.A);
    }
    tdc();
    dc_set_mx(1, 0);
}

/* FD:C1EE  HDMA channels 0-7 through D = 4300: modes / B-bus targets (channel 7 to 2128 or
 * 2129 by 013C bit 0), tables in bank 7F, one of two sets of 57-byte tables (0F80.. or
 * 1238..) chosen by 0153 & 0F. */
DC_ENTRY(FDC1EE, RTL)
{
    dc_push16(g_cpu.D);
    dc_set_mx(0, 0);
    lda16(0x4300); g_cpu.D = 0x4300;
    dc_set_mx(1, 0);
    static const u8 mode[8] = { 0x44, 0x43, 0x43, 0x43, 0x44, 0x41, 0x41, 0x40 };
    static const u8 dest[7] = { 0x07, 0x0D, 0x0F, 0x11, 0x2C, 0x26, 0x31 };
    for (unsigned c = 0; c < 8; c++) dp8_set((u8)(c << 4), mode[c]);
    for (unsigned c = 0; c < 7; c++) dp8_set((u8)(c << 4 | 1), dest[c]);
    lda8(abs8(0x013C));
    lda8(g_cpu.A & 0x01 ? 0x29 : 0x28); dp8_set(0x71, (u8)g_cpu.A);
    lda8(0x7F);
    for (unsigned c = 0; c < 8; c++) dp8_set((u8)(c << 4 | 4), 0x7F);
    for (unsigned c = 0; c < 8; c++) dp8_set((u8)(c << 4 | 7), 0x7F);
    lda8((u8)(abs8(0x0153) & 0x0F));
    u16 t = g_cpu._flag_Z ? 0x0F80 : 0x1238;
    for (unsigned c = 0; c < 8; c++) { ldx((u16)(t + 0x57 * c)); dp16_set((u8)(c << 4 | 2), g_cpu.X); }
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* FD:D52D  HDMA stripe table at 1538 (2-byte entries, 8-bit X): band height 2 * (28 / 8)
 * from the divider, values E8 down to E1 with line count 1, a gap of E0 / 81, then E0 up
 * to E9 with 81; copied to 7F:1600 (0x100 bytes). */
static void stripes(u8 first, u8 cnt)
{
    while (dc_cmp8((u8)g_cpu.X, dp8(0xDB)), !g_cpu._flag_Z) {
        lda8(first ? first : dp8(0xE1)); abs8_set(0x1539 + g_cpu.X, (u8)g_cpu.A);
        lda8(cnt); abs8_set(0x1538 + g_cpu.X, cnt);
        inx(); inx();
    }
}

static void stripe_step(u8 o)
{
    lda8(dp8(0xDB)); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(o))); dp8_set(0xDB, (u8)g_cpu.A);
}

DC_ENTRY(FDD52D, RTS)
{
    lda8(0x28); bus8_set(0x004204, 0x28);
    lda8(0x00); bus8_set(0x004205, 0x00);
    lda8(0x08); bus8_set(0x004206, 0x08);
    dc_set_mx(1, 1);
    ldx(0x00);
    lda8(asl8(bus8(0x004214))); dp8_set(0xDB, (u8)g_cpu.A); dp8_set(0xDD, (u8)g_cpu.A);
    lda8(asl8(bus8(0x004216))); dp8_set(0xDF, (u8)g_cpu.A);
    lda8(0xE8);
    for (;;) {
        dp8_set(0xE1, (u8)g_cpu.A);
        stripes(0, 0x01);
        stripe_step(0xDD);
        lda8((u8)(dp8(0xE1) - 1)); dc_cmp8((u8)g_cpu.A, 0xE0);
        if (g_cpu._flag_Z) break;
    }
    stripe_step(0xDF);
    stripes(0xE0, 0x81);
    lda8(0xE0); dp8_set(0xE1, 0xE0);
    for (;;) {
        stripe_step(0xDD);
        lda8(dp8(0xE1)); dc_cmp8((u8)g_cpu.A, 0xE9);
        if (g_cpu._flag_Z) break;
        lda8((u8)(g_cpu.A + 1)); dp8_set(0xE1, (u8)g_cpu.A);
        stripes(0, 0x81);
    }
    dc_set_mx(0, 0);
    ldx(0x1538); ldy(0x1600); lda16(0x00FF);
    dc_mvn(0x7F, 0x7F);
    dc_set_mx(1, 0);
}

/* FD:AEF2  Three party members (FD:A80B list): 1C48..1C56 cleared, then for each member
 * with a character id (0029 + slot) a 5-byte entry at 1C48: id, 80 / 01 at +2 / +3, and at
 * +4 the bit (80 >> n) of the B1BE slot of the first set bit in its CC:06A7 byte (via
 * C1:FDBF on id * 6). */
DC_ENTRY(FDAEF2, RTL)
{
    tdc(); tax();
    do { bus8_set(0x001C48 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x000F); } while (!g_cpu._flag_Z);
    tdc(); tax();
    dp16_set(0x02, g_cpu.X); dp16_set(0x0A, g_cpu.X);
    do {
        ldx(dp16(0x02));
        dc_set_mx(0, 0);
        g_cpu.A = g_cpu.X; asl_a(1); tax();
        ldx(bus16(0xFDA80B + g_cpu.X));
        tdc();
        dc_set_mx(1, 0);
        lda8(abs8(0x0029 + g_cpu.X)); dp8_set(0x06, (u8)g_cpu.A);
        if (!g_cpu._flag_Z) {
            tax(); dp16_set(0x28, g_cpu.X);
            lda8(0x06); tax(); dp16_set(0x2A, g_cpu.X);
            dc_jsl(0xC1FDBF, 0xFDAF22);
            ldx(dp16(0x2C)); dp16_set(0x04, g_cpu.X);
            lda8(dp8(0x06)); ldx(dp16(0x0A)); bus8_set(0x001C48 + g_cpu.X, (u8)g_cpu.A);
            tdc(); tay();
            ldx(dp16(0x04));
            lda8(bus8(0xCC06A7 + g_cpu.X));
            while (!(g_cpu.A & 0x80)) {
                lda8(asl8((u8)g_cpu.A)); iny(); cpy(0x0008);
                if (g_cpu._flag_C) break;
            }
            lda8(abs8(0xB1BE + g_cpu.Y));
            if (!g_cpu._flag_N) {
                tax();
                lda8(0x80);
                for (;;) { ldx((u16)(g_cpu.X - 1)); if (g_cpu._flag_N) break; lsr_a(1); }
                ldx(dp16(0x0A)); bus8_set(0x001C4C + g_cpu.X, (u8)g_cpu.A);
            }
            ldx(dp16(0x02));
            lda8(0x80); ldx(dp16(0x0A)); abs8_set(0x1C4A + g_cpu.X, 0x80);
            lda8(0x01); abs8_set(0x1C4B + g_cpu.X, 0x01);
        }
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x0A), 0x0005)); dp16_set(0x0A, g_cpu.A);
        tdc();
        dc_set_mx(1, 0);
        dp8_set(0x02, (u8)(dp8(0x02) + 1));
        lda8(dp8(0x02)); dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_C);
}

/* FD:B22E  Battle tables reset: 5E2D..63AC = 0, the target lists AEFF/AF0A = FF then the
 * three party ids from 2980 (count in AEC5), B1BE = slot of each party id, and the enemy
 * ids from the 29C4 list (12-byte entries, 8 slots; bit 7 of +2 hides it: AF02 = FF and
 * AF15 bit 7) counted in AEC6. */
DC_ENTRY(FDB22E, RTL)
{
    tdc(); tax();
    do { abs8_set(0x5E2D + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0580); } while (!g_cpu._flag_C);
    lda8(0xFF);
    static const u16 ff[] = { 0x99DD, 0x99DE, 0x99DF, 0x9F22, 0x9F23, 0x9F24 };
    for (unsigned k = 0; k < 6; k++) abs8_set(ff[k], 0xFF);
    tdc(); tax(); lda8(0xFF);
    do { abs8_set(0xAEFF + g_cpu.X, 0xFF); abs8_set(0xAF0A + g_cpu.X, 0xFF); inx(); cpx(0x000B); } while (!g_cpu._flag_C);
    tdc(); tax();
    do { abs8_set(0xAF15 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0008); } while (!g_cpu._flag_C);
    abs8_set(0xAEC5, (u8)g_cpu.A);
    tax();
    do {
        lda8(abs8(0x2980 + g_cpu.X));
        if (!g_cpu._flag_N) {
            abs8_set(0xAEFF + g_cpu.X, (u8)g_cpu.A); abs8_set(0xAF0A + g_cpu.X, (u8)g_cpu.A);
            inc_abs(0xAEC5);
        }
        inx(); cpx(0x0003);
    } while (!g_cpu._flag_C);
    dp8_set(0x24, 0);
    tdc(); tax(); lda8(0xFF);
    do { abs8_set(0xB1BE + g_cpu.X, 0xFF); inx(); cpx(0x0007); } while (!g_cpu._flag_C);
    tdc(); tax();
    do {
        lda8(abs8(0xAEFF + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            tay();
            lda8((u8)g_cpu.X); abs8_set(0xB1BE + g_cpu.Y, (u8)g_cpu.A);
        }
        inx(); cpx(0x0003);
    } while (!g_cpu._flag_C);
    tdc(); abs8_set(0xAEC6, (u8)g_cpu.A); tax(); tay();
    do {
        lda8(abs8(0x29C5 + g_cpu.X));
        if (!g_cpu._flag_N) {
            lda8(abs8(0x29C4 + g_cpu.X));
            abs8_set(0xAF02 + g_cpu.Y, (u8)g_cpu.A); abs8_set(0xAF0D + g_cpu.Y, (u8)g_cpu.A);
            lda8(abs8(0x29C6 + g_cpu.X));
            if (g_cpu._flag_N) {
                lda8(0xFF); abs8_set(0xAF02 + g_cpu.Y, 0xFF);
                lda8((u8)(abs8(0xAF15 + g_cpu.Y) | 0x80)); abs8_set(0xAF15 + g_cpu.Y, (u8)g_cpu.A);
            }
        }
        inc_abs(0xAEC6);
        iny();
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; ldx(dc_adc16(g_cpu.X, 0x000C));
        tdc();
        dc_set_mx(1, 0);
        cpx(0x0060);
    } while (!g_cpu._flag_C);
}

/* FD:C124  Battle window HDMA tables in bank 7F (DB = 7F): window regions from 1D7D..86,
 * scroll / colour pairs, 1520.. from 0BD7/0BD8, then the stripe table (FD:D52D). */
DC_ENTRY(FDC124, RTL)
{
    dc_push8(g_cpu.DB);
    lda8(0x7F); dc_push8(0x7F); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    static const u16 or_src[3][2] = { { 0x1D7D, 0x1D84 }, { 0x1D7F, 0x1D85 }, { 0x1D81, 0x1D86 } };
    for (unsigned k = 0; k < 3; k++) {
        lda8((u8)(bus8(or_src[k][0]) | bus8(or_src[k][1]))); abs8_set(0x14F0 + k, (u8)g_cpu.A);
    }
    lda8(0x00); abs8_set(0x14F3, 0x00);
    lda8(0x1C); abs8_set(0x14F4, 0x1C);
    lda8((u8)(bus8(0x001D7F) | 0x01)); abs8_set(0x14F5, (u8)g_cpu.A);
    lda8(0x1C); abs8_set(0x14F6, 0x1C);
    lda8(0x00); bus8_set(0x7F14F7, 0x00);
    dc_set_mx(0, 0);
    static const u16 w[][2] = { { 0x14F8, 0x0000 }, { 0x14FA, 0x0037 }, { 0x14FC, 0x0000 }, { 0x14FE, 0xFFF1 },
                                { 0x1500, 0x0000 }, { 0x1502, 0xFFBC }, { 0x1504, 0x0000 }, { 0x1506, 0xFF76 } };
    for (unsigned k = 0; k < 8; k++) { lda16(w[k][1]); abs16_set(w[k][0], g_cpu.A); }
    lda16(0xFF00); abs16_set(0x1534, 0xFF00); abs16_set(0x1536, 0xFF00);
    dc_set_mx(1, 0);
    lda8(0xE0); abs8_set(0x1531, 0xE0); abs8_set(0x1533, 0xE0);
    lda8(bus8(0x000BD7)); abs8_set(0x1520, (u8)g_cpu.A);
    lda8(bus8(0x000BD8)); abs8_set(0x1521, (u8)g_cpu.A);
    static const u8 b[10] = { 0x00, 0x00, 0x05, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00 };
    for (unsigned k = 0; k < 10; k++) { lda8(b[k]); abs8_set(0x1522 + k, b[k]); }
    dp8_set(0x27, 0);
    dc_FDD52D_glue();
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}

/* FD:ED89  HDMA channels 1-5 on bank 7F tables (6000.., 74AC..), their headers, and two
 * 256-entry curves through the WRAM port at 7F:601C / 7F:681C: for x = dp08..FF, d = 2|x|
 * (x signed) and FFFF / (F0 - d * E0 >> 8) scaled by FD:EF23. */
DC_ENTRY(FDED89, RTS)
{
    static const struct { u8 mode, dest; u16 table; } ch[5] = {
        { 0x43, 0x1B, 0x6000 }, { 0x43, 0x1D, 0x6007 }, { 0x43, 0x1F, 0x600E }, { 0x43, 0x0D, 0x6015 }, { 0x41, 0x26, 0x74AC } };
    for (unsigned c = 0; c < 5; c++) {
        u16 r = (u16)(0x4310 + 0x10 * c);
        lda8(ch[c].mode); abs8_set(r, ch[c].mode);
        lda8(ch[c].dest); abs8_set(r + 1, ch[c].dest);
        ldx(ch[c].table); abs16_set(r + 2, ch[c].table);
        lda8(0x7F); abs8_set(r + 4, 0x7F); abs8_set(r + 7, 0x7F);
    }
    lda8(0xF0);
    static const u16 f0[] = { 0x6000, 0x6003, 0x6007, 0x600A, 0x74AC, 0x74AF };
    for (unsigned k = 0; k < 6; k++) bus8_set(0x7F0000 | f0[k], 0xF0);
    lda8(0x70);
    static const u16 s70[] = { 0x600E, 0x6011, 0x6015, 0x6018 };
    for (unsigned k = 0; k < 4; k++) bus8_set(0x7F0000 | s70[k], 0x70);
    lda8(0x00);
    static const u16 z[] = { 0x6006, 0x600D, 0x6014, 0x601B, 0x74B2 };
    for (unsigned k = 0; k < 5; k++) bus8_set(0x7F0000 | z[k], 0x00);
    dc_set_mx(0, 0);
    static const u16 p[][2] = { { 0x6001, 0x605C }, { 0x6004, 0x621C }, { 0x6008, 0x685C }, { 0x600B, 0x6A1C },
                                { 0x600F, 0x701C }, { 0x6012, 0x7020 }, { 0x6016, 0x7024 }, { 0x6019, 0x7028 },
                                { 0x74AD, 0x70CC }, { 0x74B0, 0x71AC } };
    for (unsigned k = 0; k < 10; k++) { lda16(p[k][1]); bus16_set(0x7F0000 | p[k][0], p[k][1]); }
    lda16(0x0000);
    dc_set_mx(1, 0);
    for (int pass = 0; pass < 2; pass++) {
        u16 dst = pass ? 0x681C : 0x601C;
        ldx(dst); abs16_set(0x2181, dst);
        lda8(0x7F); abs8_set(0x2183, 0x7F);
        if (!pass) { ldx(0xFFFF); abs16_set(0x4204, 0xFFFF); }
        ldx(dp16(0x08));
        do {
            lda8((u8)g_cpu.X);
            if (g_cpu._flag_N) lda8((u8)(g_cpu.A ^ 0xFF));
            lda8(asl8((u8)g_cpu.A)); abs8_set(0x4202, (u8)g_cpu.A);
            lda8(0xE0); abs8_set(0x4203, 0xE0);
            lda8(abs8(0x4217)); dp8_set(0x0A, (u8)g_cpu.A);
            lda8(0xF0); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x0A))); abs8_set(0x4206, (u8)g_cpu.A);
            if (!pass) {
                lda8(abs8(0x4214)); abs8_set(0x4202, (u8)g_cpu.A);
                lda8(0xFF);
                dc_FDEF23_glue();
                abs8_set(0x2180, 0); abs8_set(0x2180, 0);
            } else {
                abs8_set(0x2180, 0); abs8_set(0x2180, 0);
                lda8(0xFF);
                dc_FDEF23_glue();
            }
            inx(); cpx(0x0100);
        } while (!g_cpu._flag_Z);
    }
}

/* FD:A990  Enemies joining mid-battle: four (address, value) pokes from the CC:0003 record
 * at B1D2 into the member block of B18B (FD:A80B); then each enemy slot with an id (AF0D),
 * no current enemy (AF02 = FF) and not hidden (AF15 bit 7) is listed in AECC (+3), set up
 * by FD:B438, and its HP (5FB0) set to 5FB2 / 2, / 4 or 1 by the record's last pointer
 * byte (1 / 2 / 3). AECB = count, AE91..94 = B18B, 2, dp12, 0. */
DC_ENTRY(FDA990, RTL)
{
    tdc();
    lda8(asl8(abs8(0xB18B))); tax();
    dc_set_mx(0, 0);
    lda16(bus16(0xFDA80B + g_cpu.X)); tay();
    tdc();
    dc_set_mx(1, 0);
    for (int k = 0; k < 4; k++) {
        ldx(abs16(0xB1D2));
        lda8(bus8(0xCC0003 + 2 * k + g_cpu.X)); tax(); dp16_set(0x0E, g_cpu.X);
        ldx(abs16(0xB1D2));
        lda8(bus8(0xCC0004 + 2 * k + g_cpu.X));
        bus8_set(dba((u32)dp16(0x0E) + g_cpu.Y), (u8)g_cpu.A);
    }
    tdc(); tax();
    dp16_set(0x10, g_cpu.X); dp16_set(0x0C, g_cpu.X);
    do {
        ldx(dp16(0x10));
        lda8(abs8(0xAF0D + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_Z) goto next;
        lda8(abs8(0xAF02 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) goto next;
        lda8(abs8(0xAF15 + g_cpu.X));
        if (g_cpu.A & 0x80) { g_cpu._flag_Z = 0; goto next; }
        ldy(dp16(0x0C));
        lda8((u8)g_cpu.X); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x03)); abs8_set(0xAECC + g_cpu.Y, (u8)g_cpu.A);
        dp8_set(0x0C, (u8)(dp8(0x0C) + 1));
        lda8(abs8(0xAF0D + g_cpu.X)); abs8_set(0xAF02 + g_cpu.X, (u8)g_cpu.A);
        lda8(abs8(0xB15B + g_cpu.X)); abs8_set(0xAFAE + g_cpu.X, (u8)g_cpu.A);
        lda8(0xFF); abs8_set(0xB26B + g_cpu.X, 0xFF);
        lda8((u8)g_cpu.X); abs8_set(0xAE6D + g_cpu.X, (u8)g_cpu.A);
        abs8_set(0xAE85 + g_cpu.X, 0);
        dp16_set(0x02, g_cpu.X);
        dc_FDB438_glue();
        dc_set_mx(0, 0);
        lda16(dp16(0x10)); xba(); lsr_a(1); tax();
        tdc();
        dc_set_mx(1, 0);
        lda8(dp8(0x0E));
        dc_cmp8((u8)g_cpu.A, 0x01);
        int sh = -1;
        if (g_cpu._flag_Z) sh = 1;
        else { dc_cmp8((u8)g_cpu.A, 0x02); if (g_cpu._flag_Z) sh = 2; else { dc_cmp8((u8)g_cpu.A, 0x03); if (g_cpu._flag_Z) sh = 0; } }
        if (sh >= 0) {
            dc_set_mx(0, 0);
            if (sh) { lda16(abs16(0x5FB2 + g_cpu.X)); lsr_a(sh); }
            else lda16(0x0001);
            abs16_set(0x5FB0 + g_cpu.X, g_cpu.A);
            tdc();
            dc_set_mx(1, 0);
        }
    next:
        dp8_set(0x10, (u8)(dp8(0x10) + 1));
        lda8(dp8(0x10)); dc_cmp8((u8)g_cpu.A, 0x08);
    } while (!g_cpu._flag_C);
    lda8(dp8(0x0C)); abs8_set(0xAECB, (u8)g_cpu.A);
    lda8(abs8(0xB18B)); abs8_set(0xAE91, (u8)g_cpu.A);
    lda8(0x02); abs8_set(0xAE92, 0x02);
    lda8(dp8(0x12)); abs8_set(0xAE93, (u8)g_cpu.A);
    abs8_set(0xAE94, 0);
}

/* FD:C5A7 / FD:C6F7  Battle window set from 1D87..91: four words to its 1508-style slots;
 * unless 1DF9 or 1DFD is set, the 1D8F/1D91 pair goes to its slots too; otherwise a wavy
 * 16-line list at `list` (C0:FF30 offsets picked by ++1DFB, + 1D8F, ADC carry chained from
 * entry; every second word 1D91) duplicated 0x40 bytes on. Then dp21 and 0BE0 (8-bit X). */
struct win_set { u16 a, b, c, list, d, e, f; };
static void battle_window_set(const struct win_set *w)
{
    dc_set_mx(0, 0);
    lda16(bus16(0x001D87)); abs16_set(w->a, g_cpu.A);
    lda16(bus16(0x001D89)); abs16_set(w->a + 2, g_cpu.A);
    lda16(bus16(0x001D8B)); abs16_set(w->b, g_cpu.A);
    lda16(bus16(0x001D8D)); abs16_set(w->b + 2, g_cpu.A);
    lda16(bus16(0x001DF9));
    if (g_cpu._flag_Z) lda16(bus16(0x001DFD));
    if (g_cpu._flag_Z) {
        lda16(bus16(0x001D8F)); abs16_set(w->c, g_cpu.A);
        lda16(bus16(0x001D91)); abs16_set(w->c + 2, g_cpu.A);
    } else {
        dc_set_mx(1, 0);
        lda8((u8)(bus8(0x001DFB) + 1)); bus8_set(0x001DFB, (u8)g_cpu.A);
        dc_set_mx(0, 0);
        lda16((u16)(g_cpu.A & 0x003E)); tax();
        for (int k = 0; k < 16; k++) {
            lda16(dc_adc16(bus16(0xC0FF30 + 4 * k + g_cpu.X), bus16(0x001D8F)));
            abs16_set(w->list + 4 * k, g_cpu.A);
        }
        lda16(bus16(0x001D91));
        for (int k = 0; k < 16; k++) abs16_set(w->list + 2 + 4 * k, g_cpu.A);
        dc_push8(g_cpu.DB);
        ldx(w->list); ldy((u16)(w->list + 0x40)); lda16(0x003F);
        dc_mvn(0x7F, 0x7F);
        g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    }
    dc_set_mx(1, 1);
    lda8(dp8(0x21)); abs8_set(w->d, (u8)g_cpu.A);
    lda8(bus8(0x000BE0)); abs8_set(w->e, (u8)g_cpu.A); abs8_set(w->f, (u8)g_cpu.A);
}

DC_ENTRY(FDC5A7, RTS)
{
    static const struct win_set w = { 0x1508, 0x1510, 0x1518, 0x1D27, 0x152D, 0x152C, 0x1530 };
    battle_window_set(&w);
}

DC_ENTRY(FDC6F7, RTS)
{
    static const struct win_set w = { 0x150C, 0x1514, 0x151C, 0x1DA7, 0x152F, 0x152E, 0x1532 };
    battle_window_set(&w);
}

/* FD:FFF4 (JMP FD:E292)  Palette animation set for the location's palette id (F6:0003 +
 * 01FE): six 12-byte records at 0520 (D = 0500) from the FD:FA77 script picked by
 * FD:F9C7. Type 0 = empty; 10/80 = three bytes, two zeros and four bytes (80 also sets a
 * long pointer 0529 = FD:FA77 + X to its following data and skips 1 + low nibble bytes);
 * others three bytes, two zeros and one byte. Record 6 onward cleared. */
DC_ENTRY(FDFFF4, RTL)
{
    php();
    dc_push16(g_cpu.D);
    dc_push8(g_cpu.DB);
    lda8(0x00); dc_push8(0x00); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(0);
    dc_set_mx(1, 0);
    ldx(0x0500); dc_push16(0x0500); g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    lda8(0x00); xba();
    ldx(abs16(0x01FE));
    lda8(asl8(bus8(0xF60003 + g_cpu.X))); tax();
    dc_set_mx(0, 0);
    ldx(bus16(0xFDF9C7 + g_cpu.X));
    lda16(0x0520); abs16_set(0x2181, 0x0520);
    lda16(0x000C); dp16_set(0x18, 0x000C);
    dc_set_mx(1, 0);
    lda8(0x00); abs8_set(0x2183, 0x00);
    ldy(0x0000);
    do {
        lda8((u8)(bus8(0xFDFA77 + g_cpu.X) & 0xF0));
        if (g_cpu._flag_Z) {
            inx(); abs8_set(0x0520 + g_cpu.Y, (u8)g_cpu.A);
        } else {
            u8 type = (u8)g_cpu.A;
            dc_cmp8(type, 0x10);
            if (!g_cpu._flag_Z) dc_cmp8(type, 0x80);
            int big = g_cpu._flag_Z;
            if (big) dc_push8(type);
            for (int k = 0; k < 3; k++) { lda8(bus8(0xFDFA77 + g_cpu.X)); abs8_set(0x0520 + k + g_cpu.Y, (u8)g_cpu.A); inx(); }
            lda8(0x00); abs8_set(0x0523 + g_cpu.Y, 0); abs8_set(0x0524 + g_cpu.Y, 0);
            for (int k = 0; k < (big ? 4 : 1); k++) { lda8(bus8(0xFDFA77 + g_cpu.X)); abs8_set(0x0525 + k + g_cpu.Y, (u8)g_cpu.A); inx(); }
            if (big) {
                lda8(dc_pull8()); dc_cmp8((u8)g_cpu.A, 0x80);
                if (g_cpu._flag_Z) {
                    dc_set_mx(0, 0);
                    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.X, 0xFA77)); abs16_set(0x0529 + g_cpu.Y, g_cpu.A);
                    lda16(0x0000);
                    dc_set_mx(1, 0);
                    lda8(0xFD); abs8_set(0x052B + g_cpu.Y, 0xFD);
                    lda8((u8)(abs8(0x0520 + g_cpu.Y) & 0x0F)); dp8_set(0x0E, (u8)g_cpu.A);
                    dp8_set(0x0F, 0);
                    dc_set_mx(0, 0);
                    g_cpu._flag_C = 1; lda16(dc_adc16(g_cpu.X, dp16(0x0E))); tax();
                    lda16(0x0000);
                    dc_set_mx(1, 0);
                }
            }
        }
        lda8((u8)g_cpu.Y); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x0C)); tay();
        dc_cmp8((u8)g_cpu.A, 0x48);
    } while (!g_cpu._flag_Z);
    lda8(0x00);
    for (int k = 0; k < 6; k++) abs8_set(0x0520 + 12 * k + g_cpu.Y, 0);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    plp();
}

/* FD:E5E1 / FD:E869  Palette effect of record Y on colours from 0521 (X = 2 * first), by
 * type 0526 (A counts down to it): toward black E64F E7C0 E73A E6A1 E779 E6D1, else E708;
 * toward white E8D7 E962 E98F EA5C E935 E9CA, else EA11. */
static void fade_dispatch(bool white)
{
    lda8(abs8(0x0521 + g_cpu.Y));
    dc_set_mx(0, 0);
    asl_a(1); tax();
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8(abs8(0x0526 + g_cpu.Y));
    static const glue_fn f[2][7] = {
        { dc_FDE64F_glue, dc_FDE7C0_glue, dc_FDE73A_glue, dc_FDE6A1_glue, dc_FDE779_glue, dc_FDE6D1_glue, dc_FDE708_glue },
        { dc_FDE8D7_glue, dc_FDE962_glue, dc_FDE98F_glue, dc_FDEA5C_glue, dc_FDE935_glue, dc_FDE9CA_glue, dc_FDEA11_glue } };
    for (unsigned k = 0; k < 6; k++) {
        if (g_cpu._flag_Z) { f[white][k](); return; }
        lda8((u8)(g_cpu.A - 1));
    }
    f[white][6]();
}

/* FD:E57C / FD:E807  Oscillating palette effect: level 0527 += 0523, strength (2|level| *
 * 0528) >> 8 (negated toward black) into the multiplier, then the effect. */
static void fade_oscillate(bool white)
{
    dc_push16(g_cpu.Y);
    g_cpu._flag_C = 0; lda8(dc_adc8(abs8(0x0527 + g_cpu.Y), abs8(0x0523 + g_cpu.Y)));
    abs8_set(0x0527 + g_cpu.Y, (u8)g_cpu.A);
    if (g_cpu._flag_N) lda8((u8)((g_cpu.A ^ 0xFF) + 1));
    lda8(asl8((u8)g_cpu.A)); abs8_set(0x4202, (u8)g_cpu.A);
    lda8(abs8(0x0528 + g_cpu.Y)); abs8_set(0x4203, (u8)g_cpu.A);
    lda8(abs8(0x4217));
    if (!white) lda8((u8)((g_cpu.A ^ 0xFF) + 1));
    abs8_set(0x4202, (u8)g_cpu.A);
    fade_dispatch(white);
    ldy(dc_pull16());
}

DC_ENTRY(FDE57C, RTS) { fade_oscillate(false); }
DC_ENTRY(FDE807, RTS) { fade_oscillate(true); }

/* Copy the record's colours back unchanged (7E:2000 -> 7E:2200) and end it (0520 = 0). */
static void fade_restore(void)
{
    lda8(0x00); abs8_set(0x0520 + g_cpu.Y, 0x00);
    lda8(abs8(0x0521 + g_cpu.Y));
    dc_set_mx(0, 0);
    asl_a(1); tax();
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8(abs8(0x0522 + g_cpu.Y)); tay();
    dc_set_mx(0, 0);
    do {
        lda16(bus16(0x7E2000 + g_cpu.X)); bus16_set(0x7E2200 + g_cpu.X, g_cpu.A);
        inx(); inx();
        ldy((u16)(g_cpu.Y - 1));
    } while (!g_cpu._flag_Z);
    lda16(0x0000);
    dc_set_mx(1, 0);
}

/* FD:E5A4 / FD:E82C  Palette effect stepping level 0527 by 0523 toward the target 0528
 * (0525 bit 7: level = 0525 at once). Reaching the target clears 0520. Toward white, going
 * below 0 restores the colours and ends the record; toward black, passing FF does. */
static void fade_step(bool white)
{
    dc_push16(g_cpu.Y);
    lda8(abs8(0x0525 + g_cpu.Y));
    if (!g_cpu._flag_N) {
        lda8(abs8(0x0527 + g_cpu.Y)); dc_cmp8((u8)g_cpu.A, abs8(0x0528 + g_cpu.Y));
        int reached;
        if (!g_cpu._flag_C) {
            lda8(dc_adc8((u8)g_cpu.A, abs8(0x0523 + g_cpu.Y)));
            if (g_cpu._flag_C && !white) { fade_restore(); ldy(dc_pull16()); return; }
            reached = g_cpu._flag_C;
            if (!reached) { dc_cmp8((u8)g_cpu.A, abs8(0x0528 + g_cpu.Y)); reached = g_cpu._flag_C; }
        } else {
            lda8(dc_sbc8((u8)g_cpu.A, abs8(0x0523 + g_cpu.Y)));
            if (!g_cpu._flag_C && white) { fade_restore(); ldy(dc_pull16()); return; }
            reached = !g_cpu._flag_C;
            if (!reached) { dc_cmp8((u8)g_cpu.A, abs8(0x0528 + g_cpu.Y)); reached = !g_cpu._flag_C; }
        }
        if (reached) {
            lda8(0x00); abs8_set(0x0520 + g_cpu.Y, 0x00);
            lda8(abs8(0x0528 + g_cpu.Y));
        }
    }
    abs8_set(0x0527 + g_cpu.Y, (u8)g_cpu.A);
    lda8(abs8(0x0527 + g_cpu.Y)); abs8_set(0x4202, (u8)g_cpu.A);
    fade_dispatch(white);
    ldy(dc_pull16());
}

DC_ENTRY(FDE5A4, RTS) { fade_step(false); }
DC_ENTRY(FDE82C, RTS) { fade_step(true); }

/* FD:ABA2  Rewards of the defeated enemy slot dp0E (not flagged AF12 bit 6, id AF0A): the
 * CC:5E00 record (7 bytes per id) adds exp and gold (B28C / B2A5 words), tech points (B2DB,
 * carry into B2DD) and lists its item (+4) in the drops B2A7.. or, when +5 matches it, the
 * charms B2AA.. (3 each, new item sets B2AF bit 5); a normal drop needs AE5D bit 6 of slot
 * dp0E - 3 and a random 0..99 below 90. */
static void reward_list(u16 list)
{
    tdc(); tay();
    do {
        lda8(abs8(list + g_cpu.Y));
        if (g_cpu._flag_Z) {
            lda8(dp8(0x10)); abs8_set(list + g_cpu.Y, (u8)g_cpu.A);
            lda8((u8)(abs8(0xB2AF) | 0x20)); abs8_set(0xB2AF, (u8)g_cpu.A);
            return;
        }
        dc_cmp8((u8)g_cpu.A, dp8(0x10));
        if (g_cpu._flag_Z) return;
        iny(); cpy(0x0003);
    } while (!g_cpu._flag_C);
}

DC_ENTRY(FDABA2, RTL)
{
    tdc();
    ldx(dp16(0x0E));
    lda8(abs8(0xAF12 + g_cpu.X));
    if (g_cpu.A & 0x40) { g_cpu._flag_Z = 0; return; }
    lda8(abs8(0xAF0A + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) return;
    tax(); dp16_set(0x28, g_cpu.X);
    ldx(0x0007); dp16_set(0x2A, 0x0007);
    dc_jsl(0xC1FDBF, 0xFDABC1);
    ldx(dp16(0x2C)); dp16_set(0x06, g_cpu.X);
    tdc();
    lda8(dp8(0x0E)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, 0x03));
    tax(); dp16_set(0x04, g_cpu.X);
    dc_set_mx(0, 0);
    ldx(dp16(0x06));
    g_cpu._flag_C = 0; lda16(dc_adc16(bus16(0xCC5E00 + g_cpu.X), abs16(0xB28C))); abs16_set(0xB28C, g_cpu.A);
    g_cpu._flag_C = 0; lda16(dc_adc16(bus16(0xCC5E02 + g_cpu.X), abs16(0xB2A5))); abs16_set(0xB2A5, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    lda8(bus8(0xCC5E06 + g_cpu.X));
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, abs16(0xB2DB)));
    if (g_cpu._flag_C) { dc_set_mx(1, 0); inc_abs(0xB2DD); dc_set_mx(0, 0); }
    abs16_set(0xB2DB, g_cpu.A);
    tdc();
    dc_set_mx(1, 0);
    lda8(bus8(0xCC5E04 + g_cpu.X));
    if (g_cpu._flag_Z) return;
    dp8_set(0x10, (u8)g_cpu.A);
    dc_cmp8((u8)g_cpu.A, bus8(0xCC5E05 + g_cpu.X));
    if (g_cpu._flag_Z) { reward_list(0xB2AA); return; }
    ldx(dp16(0x04));
    lda8(abs8(0xAE5D + g_cpu.X));
    if (!(g_cpu.A & 0x40)) { g_cpu._flag_Z = 1; return; }
    tdc(); tax();
    lda8(0x64);
    dc_jsl(0xC1FDCB, 0xFDAC23);
    dc_cmp8((u8)g_cpu.A, 0x5A);
    if (g_cpu._flag_C) return;
    reward_list(0xB2A7);
}

/* FD:B033  X = offset of the record of enemy / item id A in its table: ids below 5A in
 * 5-byte records at 0262, 5A..7A 3-byte at 047E, 7B..93 3-byte at 04E1, 94..BB 4-byte at
 * 052C, BC..F1 4-byte at 05CC (X unchanged for F2 and up). */
DC_ENTRY(FDB033, RTL)
{
    static const struct { u8 end, size; u16 base; } g[5] = {
        { 0x5A, 5, 0x0262 }, { 0x7B, 3, 0x047E }, { 0x94, 3, 0x04E1 }, { 0xBC, 4, 0x052C }, { 0xF2, 4, 0x05CC } };
    u8 first = 0;
    for (unsigned k = 0; k < 5; k++) {
        dc_cmp8((u8)g_cpu.A, g[k].end);
        if (!g_cpu._flag_C) {
            if (k) { g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, first)); }
            tax(); dp16_set(0x28, g_cpu.X);
            lda8(g[k].size); tax(); dp16_set(0x2A, g_cpu.X);
            static const u32 site[5] = { 0xFDB03F, 0xFDB061, 0xFDB082, 0xFDB0A3, 0xFDB0C4 };
            dc_jsl(0xC1FDBF, site[k]);
            dc_set_mx(0, 0);
            g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x2C), g[k].base)); tax();
            tdc();
            dc_set_mx(1, 0);
            return;
        }
        first = g[k].end;
    }
}

/* FD:B555  Party battle state: AEE6..AEF6 and B19E.. / 5E0D.. (0x20) cleared, FD:B3EB for
 * each member whose B3BA.. entry is B3, then for the two equipment slots of each member
 * (flag 01 at 5E77 / 5E9F + n*80) the CC:2A05 record (3 bytes per id) ORs its bits into
 * the member's status byte. */
DC_ENTRY(FDB555, RTL)
{
    tdc(); tax();
    do { abs8_set(0xAEE6 + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0011); } while (!g_cpu._flag_C);
    tdc(); tax();
    do { abs8_set(0xB19E + g_cpu.X, (u8)g_cpu.A); abs8_set(0x5E0D + g_cpu.X, (u8)g_cpu.A); inx(); cpx(0x0020); } while (!g_cpu._flag_C);
    for (unsigned k = 0; k < 3; k++) {
        lda8(abs8(0xB3BA + k)); dc_cmp8((u8)g_cpu.A, 0xB3);
        if (g_cpu._flag_Z) { ldx((u16)(0x80 * k)); dc_FDB3EB_glue(); }
    }
    for (unsigned s = 0; s < 2; s++) {
        tdc();
        for (unsigned k = 0; k < 3; k++) {
            u16 m = (u16)(0x80 * k), flag = (u16)((s ? 0x5E9F : 0x5E77) + m);
            lda8(abs8(flag)); dc_cmp8((u8)g_cpu.A, 0x01);
            if (!g_cpu._flag_Z) continue;
            lda8(asl8(abs8(flag - 1))); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, abs8(flag - 1)));
            tax();
            lda8(bus8(0xCC2A05 + g_cpu.X)); tay();
            lda8((u8)(abs8(0x5E2D + m + g_cpu.Y) | bus8(0xCC2A06 + g_cpu.X)));
            abs8_set(0x5E2D + m + g_cpu.Y, (u8)g_cpu.A);
        }
    }
}

/* FD:EB8B  Circular window of radius dp23 centred at (dp1F, dp21) = (80, 80) into one of the
 * two 7F:70AC / 72AC HDMA buffers (alternating with dp24) through the WRAM port: the
 * half-widths per line in 7F:702C+ come from a hardware-math circle walk, then each line
 * gets left / right (clamped to 0 / FF), padded with FF 00 lines (FD:EC80). */
static void circle_line(u8 i, bool tdc_floor)
{
    lda8((u8)g_cpu.Y); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, bus8(0x7F702C + i)));
    if (!g_cpu._flag_C) { if (tdc_floor) tdc(); else lda8(0x00); }
    abs8_set(0x2180, (u8)g_cpu.A);
    lda8((u8)g_cpu.Y); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, bus8(0x7F702C + i)));
    if (g_cpu._flag_C) lda8(0xFF);
    abs8_set(0x2180, (u8)g_cpu.A);
}

DC_ENTRY(FDEB8B, RTS)
{
    lda8(0x80); dp8_set(0x1F, 0x80); dp8_set(0x21, 0x80);
    dp8_set(0x24, (u8)(dp8(0x24) + 1));
    lda8(dp8(0x24)); lsr_a(1);
    u16 buf = g_cpu._flag_C ? 0x72AC : 0x70AC;
    ldx(buf); abs16_set(0x2181, buf);
    lda8(0x7F); abs8_set(0x2183, 0x7F);
    lda8(dp8(0x23)); dp8_set(0x0A, (u8)g_cpu.A);
    if (g_cpu._flag_Z) { lda8(0xFF); dc_FDEC80_glue(); return; }
    ldx(0x0100); abs16_set(0x4204, 0x0100);
    lda8(dp8(0x0A)); abs8_set(0x4206, (u8)g_cpu.A);
    lda8(abs8(0x4214)); abs8_set(0x4202, (u8)g_cpu.A);
    lda8(dp8(0x0A)); dp8_set(0x15, (u8)g_cpu.A);
    lda8(0x80); dp8_set(0x14, 0x80);
    dp8_set(0x16, 0); dp8_set(0x17, 0);
    dc_set_mx(0, 1);
    for (;;) {
        ldx(dp8(0x17)); abs8_set(0x4203, (u8)g_cpu.X);
        g_cpu._flag_C = 1; lda16(dc_sbc16(dp16(0x14), abs16(0x4216))); dp16_set(0x14, g_cpu.A);
        if (g_cpu._flag_N) break;
        xba(); bus16_set(0x7F702C + g_cpu.X, g_cpu.A);
        ldy(dp8(0x15)); abs8_set(0x4203, (u8)g_cpu.Y);
        g_cpu._flag_C = 0; lda16(dc_adc16(dp16(0x16), abs16(0x4216))); dp16_set(0x16, g_cpu.A);
    }
    lda16(0x0000);
    dc_set_mx(1, 1);
    lda8(dp8(0x21)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x0A)));
    if (g_cpu._flag_C) dc_FDEC80_glue();
    ldy(dp8(0x1F));
    ldx(dp8(0x0A)); dc_cmp8((u8)g_cpu.X, dp8(0x21));
    if (g_cpu._flag_C) ldx(dp8(0x21));
    dex();
    do { circle_line((u8)g_cpu.X, true); dex(); } while (!g_cpu._flag_Z);
    ldy(dp8(0x1F));
    ldx(dp8(0x0A));
    lda8(dp8(0x21)); dc_cmp8((u8)g_cpu.A, 0xFF);
    if (!g_cpu._flag_C) {
        g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x0A)));
        dc_cmp8((u8)g_cpu.A, 0xFF);
        if (g_cpu._flag_C) { lda8(0xFF); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x21))); tax(); }
        dp8_set(0x18, (u8)g_cpu.X);
        ldx(dp8(0x08));
        do { circle_line((u8)g_cpu.X, false); inx(); dc_cmp8((u8)g_cpu.X, dp8(0x18)); } while (!g_cpu._flag_Z);
        lda8(0x00); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x0A)));
        g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x21)));
        dc_FDEC80_glue();
    }
    dc_set_mx(1, 0);
}

/* FD:B7EB  Battle start ATB: for each party member (FD:A80B) and enemy (FD:A811, AEC6 of
 * them) speed 0038 is capped at 16 and the start gauge is 69 - 6 * speed + the CC:2E31
 * entry for (speed - 1 + 16 * (2990 & 7)) into B158/AFAB (B15B/AFAE for enemies, B03A /
 * B03D = 1; enemy flag 000A bit 0 sets AF15 bit 6). Then every gauge is lowered so the
 * smallest valid one becomes 1, and the party's go to 99DD.. / 9F22... */
static void atb_start(u16 slot_tab, u16 out1, u16 out2)
{
    ldx(dp16(0x02));
    dc_set_mx(0, 0);
    g_cpu.A = g_cpu.X; asl_a(1); tax();
    lda16(bus16(0xFD0000 | (u16)(slot_tab + g_cpu.X))); tax(); dp16_set(0x04, g_cpu.X);
    tdc();
    dc_set_mx(1, 0);
    lda8(abs8(0x0038 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0x10);
    if (g_cpu._flag_C) { lda8(0x10); abs8_set(0x0038 + g_cpu.X, 0x10); }
    tdc();
    lda8((u8)(abs8(0x2990) & 0x07)); tax(); dp16_set(0x28, g_cpu.X);
    ldx(0x0010); dp16_set(0x2A, 0x0010);
    dc_jsl(0xC1FDBF, slot_tab == 0xA80B ? 0xFDB821 : 0xFDB899);
    ldx(dp16(0x04));
    lda8((u8)(abs8(0x0038 + g_cpu.X) - 1));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x2C))); tax();
    lda8(bus8(0xCC2E31 + g_cpu.X)); dp8_set(0x00, (u8)g_cpu.A);
    ldx(dp16(0x04));
    if (slot_tab == 0xA80B) {
        lda8(abs8(0x0038 + g_cpu.X)); tax(); dp8_set(0x28, (u8)g_cpu.A);
    } else {
        lda8(abs8(0x000A + g_cpu.X));
        if (g_cpu.A & 0x01) {
            ldy(dp16(0x02));
            lda8((u8)(abs8(0xAF15 + g_cpu.Y) | 0x40)); abs8_set(0xAF15 + g_cpu.Y, (u8)g_cpu.A);
        }
        tdc();
        lda8(abs8(0x0038 + g_cpu.X)); tax(); dp16_set(0x28, g_cpu.X);
    }
    lda8(0x06); tax(); dp16_set(0x2A, g_cpu.X);
    dc_jsl(0xC1FDBF, slot_tab == 0xA80B ? 0xFDB842 : 0xFDB8CC);
    lda8(0x69); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x2C)));
    g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, dp8(0x00)));
    ldx(dp16(0x02));
    abs8_set(out1 + g_cpu.X, (u8)g_cpu.A); abs8_set(out2 + g_cpu.X, (u8)g_cpu.A);
}

DC_ENTRY(FDB7EB, RTL)
{
    tdc(); tax(); dp16_set(0x02, g_cpu.X);
    do {
        ldx(dp16(0x02));
        lda8(abs8(0xAEFF + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            atb_start(0xA80B, 0xB158, 0xAFAB);
            lda8(0x01); abs8_set(0xB03A + g_cpu.X, 0x01);
        }
        dp8_set(0x02, (u8)(dp8(0x02) + 1));
        lda8(dp8(0x02)); dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_C);
    tdc(); tax(); dp16_set(0x02, g_cpu.X);
    do {
        ldx(dp16(0x02));
        lda8(abs8(0xAF0D + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            atb_start(0xA811, 0xB15B, 0xAFAE);
            lda8(abs8(0xAF02 + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
            if (!g_cpu._flag_Z) { lda8(0x01); abs8_set(0xB03D + g_cpu.X, 0x01); }
        }
        dp8_set(0x02, (u8)(dp8(0x02) + 1));
        lda8(dp8(0x02)); dc_cmp8((u8)g_cpu.A, abs8(0xAEC6));
    } while (!g_cpu._flag_C);
    tdc(); tax(); tay(); dp16_set(0x00, g_cpu.X);
    lda8(abs8(0xAFAB));
    do {
        inx();
        dc_cmp8((u8)g_cpu.A, abs8(0xAFAB + g_cpu.X));
        if (g_cpu._flag_C) {
            lda8(abs8(0xAFAB + g_cpu.X));
            if (!g_cpu._flag_Z) {
                lda8(abs8(0xAEFF + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
                if (!g_cpu._flag_Z) ldy(g_cpu.X);
            }
            lda8(abs8(0xAFAB + g_cpu.Y));
        }
        cpx(0x000A);
    } while (!g_cpu._flag_C);
    lda8((u8)(abs8(0xAFAB + g_cpu.Y) - 1)); dp8_set(0x00, (u8)g_cpu.A);
    tdc(); tax();
    do {
        lda8(abs8(0xAEFF + g_cpu.X)); dc_cmp8((u8)g_cpu.A, 0xFF);
        if (!g_cpu._flag_Z) {
            lda8(abs8(0xAFAB + g_cpu.X));
            if (!g_cpu._flag_Z) {
                g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, dp8(0x00))); abs8_set(0xAFAB + g_cpu.X, (u8)g_cpu.A);
            }
        }
        inx(); cpx(0x000B);
    } while (!g_cpu._flag_C);
    for (unsigned k = 0; k < 3; k++) {
        lda8(abs8(0xAFAB + k)); abs8_set(0x99DD + k, (u8)g_cpu.A); abs8_set(0x9F22 + k, (u8)g_cpu.A);
    }
}

/* FD:C2EB / FD:C847  Battle window HDMA lists (DB = 7F) for one of the two buffers: the
 * 64 / 70 line pairs of each channel, the wavy list when 1DF9 is set, the E4 / F0 window
 * table, then FD:C439 / C477, and the window registers from 1D87.. (FD:C5A7 / C6F7) or, with
 * dp53 bit 7, from 3467.. directly. */
struct win_lists {
    u16 e1, e2; glue_fn list13; u16 l, lp2, lp13, e3, e4, e4a, e4b, e5; u16 p1, p2, p3, p5;
    glue_fn win, set; u16 copy, d, e, f;
};
static void battle_window_lists(const struct win_lists *w)
{
    dc_push8(g_cpu.DB);
    lda8(0x7F); dc_push8(0x7F); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    dc_set_mx(1, 0);
    hdma_pair(w->e1, 0x64, w->p1, 0x70, w->p1);
    hdma_pair(w->e2, 0x64, w->p2, 0x70, w->p2);
    w->list13();
    hdma_list13_if(0x001DF9, w->l, w->lp2, w->lp13);
    hdma_pair(w->e3, 0x64, w->p3, 0x70, w->p3);
    hdma_pair(w->e4, 0xE4, w->e4a, 0xF0, w->e4b);
    hdma_pair(w->e5, 0x64, w->p5, 0x70, w->p5);
    w->win();
    lda8(dp8(0x53));
    if (!g_cpu._flag_N) { w->set(); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB); return; }
    dc_set_mx(0, 0);
    static const u16 off[6] = { 0, 2, 8, 0x0A, 0x10, 0x12 };
    for (unsigned k = 0; k < 6; k++) { lda16(abs16(0x3467 + 2 * k)); abs16_set(w->copy + off[k], g_cpu.A); }
    dc_set_mx(1, 1);
    lda8(dp8(0x21)); abs8_set(w->d, (u8)g_cpu.A);
    lda8(abs8(0x22C0)); abs8_set(w->e, (u8)g_cpu.A); abs8_set(w->f, (u8)g_cpu.A);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}

DC_ENTRY(FDC2EB, RTS)
{
    static const struct win_lists w = { 0x0F80, 0x0FD7, dc_FDC4B5_glue, 0x1085, 0x1518, 0x1D27, 0x10DC, 0x1133, 0x17C8, 0x1890,
        0x118A, 0x14F0, 0x1508, 0x1520, 0x152C, dc_FDC439_glue, dc_FDC5A7_glue, 0x1508, 0x152D, 0x152C, 0x1530 };
    battle_window_lists(&w);
}

DC_ENTRY(FDC847, RTS)
{
    static const struct win_lists w = { 0x1238, 0x128F, dc_FDC52E_glue, 0x133D, 0x151C, 0x1DA7, 0x1394, 0x13EB, 0x1AC8, 0x1B90,
        0x1442, 0x14F0, 0x150C, 0x1520, 0x152E, dc_FDC477_glue, dc_FDC6F7_glue, 0x150C, 0x152F, 0x152E, 0x1532 };
    battle_window_lists(&w);
}

/* FD:C995 / CD0C / CFCF / D27E  The battle window HDMA lists of FD:C2EB / C847 squeezed by
 * the letterbox height h = 00:0127 (h = 0 runs the plain builder). Layout A (C995, CD0C)
 * and layout B (CFCF, D27E) split the lines differently; each runs on one of the two list
 * buffers. A list is a run of (count, pointer) entries ending in 00, the counts computed
 * from h by the ops below. */
enum { LB_K, LB_SUB, LB_SUBI, LB_SUBD, LB_DBL, LB_CLD, LB_CL2, LB_SCROLL };
enum { LB_MAIN = 0, LB_ALT = 1, LB_MAIN4 = 2 };
struct lb_ent { u8 op, k; u16 p; };

static u8 lb_h(void) { return bus8(0x000127); }

/* A = the entry's line count: k, k - h (+1 / -1), 2h, min(h - 1, 0A), 2h - 14 or 2. */
static void lb_count(const struct lb_ent *e)
{
    switch (e->op) {
    case LB_K: lda8(e->k); return;
    case LB_DBL: lda8(lb_h()); lda8(asl8((u8)g_cpu.A)); return;
    case LB_CLD:
    case LB_CL2:
        lda8(lb_h()); dc_cmp8((u8)g_cpu.A, 0x0A);
        bool big = g_cpu._flag_C && !g_cpu._flag_Z;
        if (e->op == LB_CLD) {
            if (big) lda8(0x0A); else { lda8(lb_h()); lda8((u8)(g_cpu.A - 1)); }
        } else {
            if (!big) lda8(0x02);
            else { lda8(asl8((u8)g_cpu.A)); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, 0x14)); }
        }
        return;
    }
    lda8(e->k); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, lb_h()));
    if (e->op == LB_SUBI) lda8((u8)(g_cpu.A + 1));
    if (e->op == LB_SUBD) lda8((u8)(g_cpu.A - 1));
}

static u16 lb_ptr(const struct lb_ent *e, u16 main, u16 alt)
{
    return e->p == LB_MAIN ? main : e->p == LB_ALT ? alt : e->p == LB_MAIN4 ? (u16)(main + 4) : e->p;
}

/* Fixed list at base. The scroll entry is (2h - 1) | 80 lines of a direct table at
 * alt + 2 * ((28 - h) & 3F). */
static void lb_list(u16 base, const struct lb_ent *e, unsigned n, u16 main, u16 alt)
{
    for (unsigned i = 0; i < n; i++, base += 3) {
        if (e[i].op == LB_SCROLL) {
            lda8(lb_h()); lda8(asl8((u8)g_cpu.A)); lda8((u8)(g_cpu.A - 1)); lda8((u8)(g_cpu.A | 0x80));
            abs8_set(base, (u8)g_cpu.A);
            lda8(0x28); g_cpu._flag_C = 1; lda8(dc_sbc8((u8)g_cpu.A, lb_h()));
            dc_set_mx(0, 0);
            lda16((u16)(g_cpu.A & 0x3F)); asl_a(1);
            g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, alt));
            abs16_set(base + 1, g_cpu.A);
            dc_set_mx(1, 0);
            continue;
        }
        lb_count(&e[i]); abs8_set(base, (u8)g_cpu.A);
        ldx(lb_ptr(&e[i], main, alt)); abs16_set(base + 1, g_cpu.X);
    }
    lda8(0x00); abs8_set(base, 0x00);
}

/* Wavy list (1DF9 set): the counts go through FD:CC5E / CCB8 in runs of 16 rows, except the
 * alt entry, written as is (its lines still advance dp EE through FD:CCB2). */
static void lb_wavy(u16 base, const struct lb_ent *e, unsigned n, u16 alt, glue_fn runs)
{
    dc_FDCC58_glue();
    for (unsigned i = 0; i < n; i++) {
        lb_count(&e[i]);
        if (e[i].p != LB_ALT) { runs(); continue; }
        abs8_set(base + g_cpu.Y, (u8)g_cpu.A);
        dc_set_mx(0, 0);
        lda16(alt); abs16_set(base + 1 + g_cpu.Y, alt);
        dc_set_mx(1, 0);
        g_cpu.Y += 3; dc_set_flags_nz16(g_cpu.Y);
        lda8(lb_h()); lda8(asl8((u8)g_cpu.A));
        dc_FDCCB2_glue();
    }
    lda8(0x00); abs8_set(base + g_cpu.Y, 0x00);
}

struct lb_buf {
    glue_fn plain; u16 e1, e2, p2; glue_fn list13; u16 l, lp; glue_fn runs;
    u16 e4, e4a, e4b, e4c, e5, p5, p5b; glue_fn win, set;
};
struct lb_layout {
    const struct lb_ent *side, *wavy, *e3, *e5; u8 nside, nwavy, ne3, ne5;
    u16 alt1, alt2, alt3; void (*e4)(const struct lb_buf *);
};

/* dp D9 += 2 * (A & FF): the running pointer into the indirect table of the 1133 list. */
static void lb_d9_adv(void)
{
    dc_set_mx(0, 0);
    lda16((u16)(g_cpu.A & 0x00FF)); asl_a(1);
    g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.A, dp16(0xD9))); dp16_set(0xD9, g_cpu.A);
    dc_set_mx(1, 0);
}

/* Indirect HDMA run: count (k - h op) | 80 at at, pointer dp D9; dp DB = the count when
 * the pointer advances past it. */
static void lb_run(u16 at, u8 op, u8 k, bool keep)
{
    struct lb_ent e = { op, k, 0 };
    lb_count(&e);
    if (keep) dp8_set(0xDB, (u8)g_cpu.A);
    lda8((u8)(g_cpu.A | 0x80)); abs8_set(at, (u8)g_cpu.A);
}

/* 2h lines of the fixed row e4c at at (D9 still skips them). */
static void lb_gap(u16 at, const struct lb_buf *b)
{
    lda8(lb_h()); lda8(asl8((u8)g_cpu.A)); abs8_set(at, (u8)g_cpu.A);
    lb_d9_adv();
    ldx(b->e4c); abs16_set(at + 1, b->e4c);
}

static void lb_e4_a(const struct lb_buf *b)
{
    u16 t = b->e4;
    lda8(0x87); abs8_set(t, 0x87); ldx(b->e4a); abs16_set(t + 1, b->e4a);
    lb_run(t + 3, LB_SUB, 0x29, true);
    ldx((u16)(b->e4a + 0x0E)); dp16_set(0xD9, g_cpu.X); abs16_set(t + 4, g_cpu.X);
    lda8(dp8(0xDB)); lb_d9_adv();
    lb_gap(t + 6, b);
    lb_run(t + 9, LB_SUB, 0x64, true);
    ldx(dp16(0xD9)); abs16_set(t + 10, g_cpu.X);
    lda8(dp8(0xDB)); lb_d9_adv();
    lda8(0x47); lda8(0xC7); abs8_set(t + 12, 0xC7);
    ldx(dp16(0xD9)); abs16_set(t + 13, g_cpu.X);
    lda8(0x00); abs8_set(t + 15, 0x00);
}

static void lb_e4_b(const struct lb_buf *b)
{
    u16 t = b->e4;
    lda8(0xE4); abs8_set(t, 0xE4); ldx(b->e4a); abs16_set(t + 1, b->e4a);
    lda8(0x9D); abs8_set(t + 3, 0x9D); ldx(b->e4b); abs16_set(t + 4, b->e4b);
    lb_run(t + 6, LB_SUBI, 0x29, true);
    ldx((u16)(b->e4b + 0x3A)); abs16_set(t + 7, g_cpu.X); dp16_set(0xD9, g_cpu.X);
    lda8(dp8(0xDB)); lb_d9_adv();
    lb_gap(t + 9, b);
    lb_run(t + 12, LB_SUBD, 0x29, false);
    ldx(dp16(0xD9)); abs16_set(t + 13, g_cpu.X);
    lda8(0x00); abs8_set(t + 15, 0x00);
}

static void letterbox(const struct lb_layout *y, const struct lb_buf *b)
{
    lda8(abs8(0x0127));
    if (g_cpu._flag_Z) { b->plain(); return; }
    dc_push8(g_cpu.DB);
    lda8(0x7F); dc_push8(0x7F); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    dc_set_mx(1, 0);
    lb_list(b->e1, y->side, y->nside, 0x14F0, y->alt1);
    lb_list(b->e2, y->side, y->nside, b->p2, y->alt2);
    b->list13();
    lda8(bus8(0x001DF9));
    if (g_cpu._flag_Z) lb_list(b->l, y->side, y->nside, b->lp, y->alt3);
    else lb_wavy(b->l, y->wavy, y->nwavy, y->alt3, b->runs);
    lb_list((u16)(b->e1 + 0x015C), y->e3, y->ne3, 0x1520, 0);
    y->e4(b);
    lb_list(b->e5, y->e5, y->ne5, b->p5, b->p5b);
    b->win();
    b->set();
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}

#define LB_N(t) (u8)(sizeof(t) / sizeof(t[0]))
static const struct lb_ent lba_side[] = { { LB_K, 0x07, LB_MAIN }, { LB_SUB, 0x29, LB_MAIN }, { LB_DBL, 0, LB_ALT },
    { LB_SUB, 0x64, LB_MAIN }, { LB_K, 0x1E, LB_MAIN } };
static const struct lb_ent lba_wavy[] = { { LB_K, 0x07, 0 }, { LB_SUB, 0x29, 0 }, { LB_DBL, 0, LB_ALT },
    { LB_SUB, 0x82, 0 }, { LB_K, 0x1E, 0 } };
static const struct lb_ent lba_e3[] = { { LB_K, 0x07, LB_MAIN }, { LB_SUB, 0x29, LB_MAIN }, { LB_CLD, 0, 0x1528 },
    { LB_CL2, 0, 0x1524 }, { LB_CLD, 0, 0x1528 }, { LB_SUB, 0x29, LB_MAIN }, { LB_SUB, 0x64, LB_MAIN }, { LB_K, 0x1E, LB_MAIN } };
static const struct lb_ent lba_e5[] = { { LB_K, 0x07, LB_MAIN }, { LB_SUB, 0x29, LB_MAIN }, { LB_SCROLL, 0, 0 },
    { LB_K, 0x01, LB_MAIN4 }, { LB_SUB, 0x64, LB_MAIN }, { LB_K, 0x1E, LB_MAIN } };
static const struct lb_ent lbb_side[] = { { LB_K, 0x64, LB_MAIN }, { LB_K, 0x1D, LB_MAIN }, { LB_SUBI, 0x29, LB_MAIN },
    { LB_DBL, 0, LB_ALT }, { LB_SUBD, 0x29, LB_MAIN } };
static const struct lb_ent lbb_wavy[] = { { LB_K, 0x58, 0 }, { LB_K, 0x29, 0 }, { LB_SUBI, 0x29, 0 },
    { LB_DBL, 0, LB_ALT }, { LB_SUBD, 0x29, 0 } };
static const struct lb_ent lbb_e3[] = { { LB_K, 0x64, LB_MAIN }, { LB_K, 0x1D, LB_MAIN }, { LB_SUBI, 0x29, LB_MAIN },
    { LB_CLD, 0, 0x1528 }, { LB_CL2, 0, 0x1524 }, { LB_CLD, 0, 0x1528 }, { LB_SUBD, 0x29, LB_MAIN } };
static const struct lb_ent lbb_e5[] = { { LB_K, 0x64, LB_MAIN }, { LB_K, 0x1D, LB_MAIN }, { LB_SUBI, 0x29, LB_MAIN },
    { LB_SCROLL, 0, 0 }, { LB_K, 0x01, LB_MAIN4 }, { LB_SUBD, 0x29, LB_MAIN } };
static const struct lb_layout lb_a = { lba_side, lba_wavy, lba_e3, lba_e5, LB_N(lba_side), LB_N(lba_wavy), LB_N(lba_e3),
    LB_N(lba_e5), 0x14F4, 0x14F8, 0x14FC, lb_e4_a };
static const struct lb_layout lb_b = { lbb_side, lbb_wavy, lbb_e3, lbb_e5, LB_N(lbb_side), LB_N(lbb_wavy), LB_N(lbb_e3),
    LB_N(lbb_e5), 0x14F4, 0x1500, 0x1504, lb_e4_b };
static const struct lb_buf lb_buf0 = { dc_FDC2EB_glue, 0x0F80, 0x0FD7, 0x1508, dc_FDC4B5_glue, 0x1085, 0x1518, dc_FDCC5E_glue,
    0x1133, 0x17C8, 0x1890, 0x1534, 0x118A, 0x152C, 0x1538, dc_FDC439_glue, dc_FDC5A7_glue };
static const struct lb_buf lb_buf1 = { dc_FDC847_glue, 0x1238, 0x128F, 0x150C, dc_FDC52E_glue, 0x133D, 0x151C, dc_FDCCB8_glue,
    0x13EB, 0x1AC8, 0x1B90, 0x1536, 0x1442, 0x152E, 0x1600, dc_FDC477_glue, dc_FDC6F7_glue };

DC_ENTRY(FDC995, RTS) { letterbox(&lb_a, &lb_buf0); }
DC_ENTRY(FDCD0C, RTS) { letterbox(&lb_a, &lb_buf1); }
DC_ENTRY(FDCFCF, RTS) { letterbox(&lb_b, &lb_buf0); }
DC_ENTRY(FDD27E, RTS) { letterbox(&lb_b, &lb_buf1); }

/* FD:FFF7 (JMP FD:E39C)  Palette animation tick (D = 0500, DB = low byte of D): for each of
 * the 12 records at 0520, when its timer 0524 reaches the period 0525 it restarts, types
 * 1x / 8x advance their frame 0523 (wrapping at low nibble + 1), and the record runs by
 * type (high nibble): 1 FD:E437, 2 E532, 3 E4E8, 4 E82C, 5 E5A4, 6 E807, 7 E57C, else E485. */
DC_ENTRY(FDFFF7, RTL)
{
    dc_push8(g_cpu.DB);
    dc_push16(g_cpu.D);
    tdc(); dc_push8((u8)g_cpu.A); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    ldx(0x0500); dc_push16(0x0500); g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    lda8(0x00); xba();
    ldy(0x0000);
    static const glue_fn f[8] = { dc_FDE437_glue, dc_FDE532_glue, dc_FDE4E8_glue, dc_FDE82C_glue,
                                  dc_FDE5A4_glue, dc_FDE807_glue, dc_FDE57C_glue, dc_FDE485_glue };
    do {
        lda8((u8)(abs8(0x0524 + g_cpu.Y) + 1)); abs8_set(0x0524 + g_cpu.Y, (u8)g_cpu.A);
        dc_cmp8((u8)g_cpu.A, abs8(0x0525 + g_cpu.Y));
        if (g_cpu._flag_Z) {
            lda8(0x00); abs8_set(0x0524 + g_cpu.Y, 0x00);
            lda8((u8)(abs8(0x0520 + g_cpu.Y) & 0xF0));
            dc_cmp8((u8)g_cpu.A, 0x10);
            if (!g_cpu._flag_Z) dc_cmp8((u8)g_cpu.A, 0x80);
            if (g_cpu._flag_Z) {
                lda8((u8)(abs8(0x0523 + g_cpu.Y) + 1)); abs8_set(0x0523 + g_cpu.Y, (u8)g_cpu.A);
                lda8((u8)((abs8(0x0520 + g_cpu.Y) & 0x0F) + 1));
                dc_cmp8((u8)g_cpu.A, abs8(0x0523 + g_cpu.Y));
                if (g_cpu._flag_Z) { lda8(0x00); abs8_set(0x0523 + g_cpu.Y, 0x00); }
            }
            lda8(abs8(0x0520 + g_cpu.Y)); lsr_a(4);
            if (!g_cpu._flag_Z) {
                unsigned k = 0;
                while (k < 7) {
                    lda8((u8)(g_cpu.A - 1));
                    if (g_cpu._flag_Z) break;
                    k++;
                }
                f[k]();
            }
        }
        lda8(0x00); xba();
        lda8((u8)g_cpu.Y); g_cpu._flag_C = 0; lda8(dc_adc8((u8)g_cpu.A, 0x0C)); tay();
        dc_cmp8((u8)g_cpu.A, 0x90);
    } while (!g_cpu._flag_Z);
    g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
}

/* FD:FFFA (JMP FD:DE98)  Palette cycle set of the location (F6:0001 + 01FE, & 3F) from the
 * FD:F310 script (FD:F290): twelve 5-byte records at 05B0 (D = 0500), their colour data
 * streamed to 7F:0400.. through the WRAM port. A record is 80 (none) or type, -, two bytes
 * and bank 7F; type 2 sends 4 x 2 colours (bytes << 2, as words through 217F) and 4 x 4
 * bytes, type 4 sends 2 x 4 colours and 2 x 4 byte pairs, other types 8 colours and 8 pairs. */
static void colour_word_217f(u32 a)
{
    lda16((u16)((bus8(a) & 0xFF) << 2));
    xba(); abs16_set(0x217F, g_cpu.A);
    xba(); abs16_set(0x217F, g_cpu.A);
}

DC_ENTRY(FDFFFA, RTL)
{
    php();
    dc_push16(g_cpu.D);
    dc_push8(g_cpu.DB);
    lda8(0x00); dc_push8(0x00); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(0);
    dc_set_mx(1, 0);
    ldx(0x0500); dc_push16(0x0500); g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    lda8(0x00); xba();
    ldx(abs16(0x01FE));
    lda8((u8)(bus8(0xF60001 + g_cpu.X) & 0x3F)); lda8(asl8((u8)g_cpu.A)); tax();
    dc_set_mx(0, 0);
    ldx(bus16(0xFDF290 + g_cpu.X));
    lda16(0x0400); abs16_set(0x2181, 0x0400);
    lda16(0x000C); dp16_set(0x18, 0x000C);
    dc_set_mx(1, 0);
    lda8(0x7F); abs8_set(0x2183, 0x7F);
    ldy(0x0000);
    do {
        lda8(bus8(0xFDF310 + g_cpu.X)); inx();
        dc_cmp8((u8)g_cpu.A, 0x80);
        if (g_cpu._flag_Z) {
            lda8(0x80); abs8_set(0x05B3 + g_cpu.Y, 0x80);
        } else {
            abs8_set(0x05B0 + g_cpu.Y, (u8)g_cpu.A);
            lda8(bus8(0xFDF310 + g_cpu.X)); abs8_set(0x05B2 + g_cpu.Y, (u8)g_cpu.A); inx();
            lda8(bus8(0xFDF310 + g_cpu.X)); abs8_set(0x05B3 + g_cpu.Y, (u8)g_cpu.A); inx();
            lda8(0x7F); abs8_set(0x05B4 + g_cpu.Y, 0x7F);
            lda8(abs8(0x05B0 + g_cpu.Y));
            u8 type = (u8)g_cpu.A;
            lda8(0x00); abs8_set(0x05B0 + g_cpu.Y, 0); abs8_set(0x05B1 + g_cpu.Y, 0);
            if (type == 0x02) {
                lda8(0x04); dp8_set(0x10, 0x04);
                do {
                    dc_set_mx(0, 0);
                    colour_word_217f(0xFDF310 + g_cpu.X);
                    colour_word_217f(0xFDF311 + g_cpu.X);
                    dc_set_mx(1, 0);
                    dp8_set(0x10, (u8)(dp8(0x10) - 1));
                } while (dp8(0x10));
                inx(); inx();
                lda8(0x04); dp8_set(0x10, 0x04);
                do {
                    for (int k = 0; k < 4; k++) { lda8(bus8(0xFDF310 + k + g_cpu.X)); abs8_set(0x2180, (u8)g_cpu.A); }
                    dp8_set(0x10, (u8)(dp8(0x10) - 1));
                } while (dp8(0x10));
                inx(); inx(); inx(); inx();
            } else if (type == 0x04) {
                lda8(0x08); dp8_set(0x10, 0x08);
                do {
                    phx();
                    do {
                        dc_set_mx(0, 0);
                        u32 a = 0xFDF310 + g_cpu.X; inx(); colour_word_217f(a);
                        dc_set_mx(1, 0);
                        dp8_set(0x10, (u8)(dp8(0x10) - 1));
                        lda8((u8)(dp8(0x10) & 0x03));
                    } while (!g_cpu._flag_Z);
                    plx();
                    lda8(dp8(0x10));
                } while (!g_cpu._flag_Z);
                inx(); inx(); inx(); inx();
                lda8(0x08); dp8_set(0x10, 0x08);
                do {
                    phx();
                    do {
                        lda8(bus8(0xFDF310 + g_cpu.X)); abs8_set(0x2180, (u8)g_cpu.A); inx();
                        lda8(bus8(0xFDF310 + g_cpu.X)); abs8_set(0x2180, (u8)g_cpu.A); inx();
                        dp8_set(0x10, (u8)(dp8(0x10) - 1));
                        lda8((u8)(dp8(0x10) & 0x03));
                    } while (!g_cpu._flag_Z);
                    plx();
                    lda8(dp8(0x10));
                } while (!g_cpu._flag_Z);
                dc_set_mx(0, 0);
                g_cpu._flag_C = 0; ldx(dc_adc16(g_cpu.X, 0x0008));
                lda16(0x0000);
                dc_set_mx(1, 0);
            } else {
                lda8(0x08); dp8_set(0x10, 0x08);
                do {
                    dc_set_mx(0, 0);
                    lda16((u16)((bus8(0xFDF310 + g_cpu.X) & 0xFF) << 2));
                    dc_set_mx(1, 0);
                    abs8_set(0x2180, (u8)g_cpu.A); xba(); abs8_set(0x2180, (u8)g_cpu.A);
                    inx();
                    dp8_set(0x10, (u8)(dp8(0x10) - 1));
                } while (dp8(0x10));
                lda8(0x08); dp8_set(0x10, 0x08);
                do {
                    lda8(bus8(0xFDF310 + g_cpu.X)); abs8_set(0x2180, (u8)g_cpu.A);
                    lda8(bus8(0xFDF311 + g_cpu.X)); abs8_set(0x2180, (u8)g_cpu.A);
                    inx(); inx();
                    dp8_set(0x10, (u8)(dp8(0x10) - 1));
                } while (dp8(0x10));
            }
        }
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0; lda16(dc_adc16(g_cpu.Y, 0x0005)); tay();
        lda16(0x0000);
        dc_set_mx(1, 0);
        u8 n = (u8)(dp8(0x18) - 1); dp8_set(0x18, n); dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    plp();
}

/* FD:FFFD (JMP FD:E022)  Animated tile upload: on alternate frames (dp 051D counter) one of
 * two groups of six 5-byte slots (D = 0500; first group at B0, second at CE) is sent to VRAM
 * by DMA channel 7. A slot is a frame pointer P, the VRAM word address V (negative = off)
 * and the byte count B; slot n advances P by the step 7F:(0400 + 20 * n + (P >> 8 & 0E))
 * and copies from the source of 7F:(0410 + 20 * n + same index). */
static void tile_slot(u8 dp, u16 table)
{
    lda16(dp16(dp + 2));
    if (g_cpu._flag_N) return;
    abs16_set(0x2116, g_cpu.A);
    ldx((u16)((dp16(dp) & 0x0E00) >> 8));
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(dp), bus16(0x7F0000 + table + g_cpu.X)));
    dp16_set(dp, g_cpu.A);
    lda16(bus16(0x7F0010 + table + g_cpu.X));
    abs16_set(0x4372, g_cpu.A);
    ldx(dp8(dp + 4));
    abs8_set(0x4374, (u8)g_cpu.X);
    abs16_set(0x4375, 0);
    abs8_set(0x4375, (u8)g_cpu.Y);
    abs8_set(0x420B, (u8)g_cpu.Y);
}

DC_ENTRY(FDFFFD, RTL)
{
    php();
    dc_push16(g_cpu.D);
    dc_push8(g_cpu.DB);
    lda8(0x00); dc_push8(0x00); g_cpu.DB = dc_pull8(); dc_set_flags_nz8(0);
    lda8(0x80); abs8_set(0x2115, 0x80);
    lda8(0x41); abs8_set(0x4370, 0x41);
    lda8(0x18); abs8_set(0x4371, 0x18);
    ldx(0x0500); dc_push16(0x0500); g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D);
    dp8_set(0x1D, (u8)(dp8(0x1D) + 1));
    dc_set_mx(1, 1);
    ldy(0x80);
    bool odd = dp8(0x1D) & 1;
    dc_set_mx(0, 1);
    u8 base = odd ? 0xCE : 0xB0;
    u16 table = odd ? 0x04C0 : 0x0400;
    for (int n = 0; n < 6; n++) tile_slot((u8)(base + 5 * n), (u16)(table + 0x20 * n));
    lda16(0x0000);
    dc_set_mx(1, 0);
    g_cpu.DB = dc_pull8();
    g_cpu.D = dc_pull16();
    plp();
}
