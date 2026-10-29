/* Bank FF helpers: a ROM checksum, the HDMA table for a screen effect and
 * which combination techs the party can use. */
#include "ram.h"
#include "ops65.h"

/* FF:FBD3  dp 7B = FF:FBE1[dp 79 & 3]: start of one of four ROM blocks. */
DC_ENTRY(FFFBD3, RTS)
{
    g_cpu.X = (u16)((dp16(0x79) & 3) << 1);
    g_cpu._flag_C = 0;
    g_cpu.A = bus16(0xFFFBE1 + g_cpu.X);
    dc_set_flags_nz16(g_cpu.A);
    dp16_set(0x7B, g_cpu.A);
}

/* FF:FBE7  A = sum of the 0x500 words at 30:(dp 7B), the carry of each
 * addition going into the next one. */
DC_ENTRY(FFFBE7, RTS)
{
    dc_sync_p();
    u8 p = g_cpu.P, db = g_cpu.DB;
    dc_set_mx(0, 0);
    g_cpu.DB = 0x30;
    dp16_set(0x00, dp16(0x7B));
    g_cpu.A = 0;
    g_cpu._flag_C = 0;
    g_cpu.Y = 0x09FE;
    do {
        g_cpu.A = dc_adc16(g_cpu.A, bus16(dba((u32)dp16(0x00) + g_cpu.Y)));
        g_cpu.Y -= 2;
    } while (!(g_cpu.Y & 0x8000));
    g_cpu.DB = db;
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}

/* FF:FC04  HDMA table at DB:6E00 (DMA channel 1 source): 60 lines, E0
 * lines, then 16 groups of 3 one-line entries (value 02 for the first 8
 * groups, 82 after) with bytes from FF:FC57. */
DC_ENTRY(FFFC04, RTL)
{
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_set_mx(0, 0);
    bus16_set(0x004312, 0x6E00);
    g_cpu.X = 0x0060;
    abs16_set(0x6E00, 0x0060);
    abs16_set(0x6E02, 0x00E0);
    abs16_set(0x6E93, 0);
    dc_set_mx(1, 1);
    dp8_set(0x02, 0);
    dp8_set(0x00, 0x02);
    g_cpu.Y = 3;
    u8 k;
    do {
        dp8_set(0x01, 3);
        do {
            abs8_set(0x6E00 + g_cpu.Y++, 0x01);
            abs8_set(0x6E00 + g_cpu.Y++, dp8(0x00));
            g_cpu.X = dp8(0x02);
            abs8_set(0x6E00 + g_cpu.Y++, bus8(0xFFFC57 + g_cpu.X));
            dp8_set(0x01, (u8)(dp8(0x01) - 1));
        } while (dp8(0x01));
        k = (u8)(dp8(0x02) + 1);
        dp8_set(0x02, k);
        if (k >= 8) { g_cpu.X = 0x82; dp8_set(0x00, 0x82); }
    } while (k < 0x10);
    g_cpu.A = k;
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}

/* FF:F958  Which of the 5 combination groups at CC:2953 / CC:2983 the
 * party can use: dp (D = 0400) A3 = mask of the members present (2980..
 * 2988, bit from FF:F9BB, bit 0 always clear), A4+i = 1 when group i needs
 * nobody missing and all three of its techs are learned (bits of 2837). */
DC_ENTRY(FFF958, RTL)
{
    u16 d0 = g_cpu.D;
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_set_mx(1, 1);
    g_cpu.D = 0x0400;
    dp8_set(0xA3, 0xFE);
    for (int y = 8; y >= 0; y--) {
        g_cpu.Y = (u16)y;
        g_cpu.X = abs8(0x2980 + g_cpu.Y);
        if (!(g_cpu.X & 0x80)) dp8_set(0xA3, dp8(0xA3) & ~bus8(0xFFF9BB + g_cpu.X));
    }
    g_cpu.Y = 0xFF;
    for (u8 i = 0; i < 5; i++) {
        dp8_set(0xA0, i);
        g_cpu.X = i;
        dp8_set((u8)(0xA4 + i), 0);
        if (bus8(0xCC2983 + i) & dp8(0xA3)) continue;
        g_cpu.X = (u16)(3 * i);
        int ok = 1;
        for (int t = 0; t < 3; t++, g_cpu.X++) {
            u8 n = (u8)(bus8(0xCC2953 + g_cpu.X) - 1);
            dp8_set(0xA1, (u8)(3 - t));
            dp8_set(0xA2, n);
            g_cpu.Y = n >> 3;
            if (!(bus8(0xFFF9BB + (n & 7)) & abs8(0x2837 + g_cpu.Y))) { ok = 0; break; }
        }
        if (ok) {
            dp8_set(0xA1, 0);
            g_cpu.X = i;
            dp8_set((u8)(0xA4 + i), (u8)(dp8((u8)(0xA4 + i)) + 1));
        }
    }
    dp8_set(0xA0, 5);
    dc_set_a8(5);
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
    g_cpu.D = d0;
    dc_set_flags_nz16(d0);
}

/* FF:F9C4  Check of the SRAM save block dp 79 & 3 (DB = 30): dp 00 = 8080 when its
 * signature 30:7FF8 + 2n is not E41B, 4040 when the sum of the block (FF:FBE7) differs from
 * 30:7FF0 + 2n, else 0000. */
DC_ENTRY(FFF9C4, RTL)
{
    phx();
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(0, 0);
    g_cpu.DB = 0x30; dc_set_flags_nz8(0x30);
    dc_FFFBD3_glue();
    lda16((u16)(dp16(0x79) & 0x0003)); asl_a(1); tax();
    lda16(abs16(0x7FF8 + g_cpu.X));
    dc_cmp16(g_cpu.A, 0xE41B);
    if (!g_cpu._flag_Z) lda16(0x8080);
    else {
        dc_FFFBE7_glue();
        dc_cmp16(g_cpu.A, abs16(0x7FF0 + g_cpu.X));
        lda16(g_cpu._flag_Z ? 0x0000 : 0x4040);
    }
    dp16_set(0x00, g_cpu.A);
    plp();
    g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB);
    plx();
}

/* STA [dp 16],Y */
static void sta_ind16_y(void)
{
    bus8_set((((u32)dp8(0x18) << 16) | dp16(0x16)) + g_cpu.Y, (u8)g_cpu.A);
}
static void inc_dp8(u8 o) { u8 v = (u8)(dp8(o) + 1); dp8_set(o, v); dc_set_flags_nz8(v); }
static void dec_dp8(u8 o) { u8 v = (u8)(dp8(o) - 1); dp8_set(o, v); dc_set_flags_nz8(v); }

/* FF:F813  Tech menu setup: dp 85 = the tech character (dp 73, or dp 75 when 7F:0000 >= 49
 * and 7F:00BA bit 0 is clear), the 9 menu rows 7709.. marked (bit 7), the combo groups
 * (FF:F958); the techs learned (2837..) of the groups whose members are all present copied
 * to 7F00.., then for each of the 7 single-tech characters the next tech is granted (bit
 * from FF:F9BB, entry 7701 + 8 * c + n flagged 40) unless 7F:01E0 bit c is clear and the
 * level FF:F951 + c is not reached. Finally the tech name tiles of CC:28DB (15 rows of 3
 * pairs at CC:24B3) and the combo marks of CC:2935 (bits of CC:2979) go to 7E:1640.. */
DC_ENTRY(FFF813, RTL)
{
    php();
    dc_set_mx(1, 1);
    ldx(dp8(0x73));
    lda8(bus8(0x7F0000)); dc_cmp8((u8)g_cpu.A, 0x49);
    if (g_cpu._flag_C) {
        lda8(bus8(0x7F00BA)); g_cpu._flag_Z = !(g_cpu.A & 0x01);
        if (g_cpu._flag_Z) ldx(dp8(0x75));
    }
    dp8_set(0x85, (u8)g_cpu.X); dp8_set(0x86, 0);
    lda8(0x80);
    static const u16 rows[9] = { 0x7709, 0x770C, 0x770F, 0x771A, 0x771D, 0x7721, 0x7724, 0x7727, 0x7729 };
    for (int k = 0; k < 9; k++) {
        u8 m = abs8(rows[k]); g_cpu._flag_Z = !(m & 0x80); abs8_set(rows[k], (u8)(m | 0x80));
    }
    dc_FFF958_glue();
    lda8(0xFE); dp8_set(0x00, 0xFE);
    /* the members present clear their bits in dp 00 */
    for (ldy(0x08);; ) {
        ldx(abs8(0x2980 + g_cpu.Y));
        if (!g_cpu._flag_N) {
            lda8(bus8(0xFFF9BB + g_cpu.X));
            g_cpu._flag_Z = !(g_cpu.A & dp8(0x00)); dp8_set(0x00, (u8)(dp8(0x00) & ~g_cpu.A));
        }
        ldy((u8)(g_cpu.Y - 1));
        if (g_cpu._flag_N) break;
    }
    for (ldx(0x24);; ) {
        lda8((u8)(bus8(0xCC2963 + g_cpu.X) & dp8(0x00)));
        if (g_cpu._flag_Z) { lda8(abs8(0x2837 + g_cpu.X)); abs8_set(0x7F00 + g_cpu.X, (u8)g_cpu.A); }
        ldx((u8)(g_cpu.X - 1));
        if (g_cpu._flag_N) break;
    }
    lda8(bus8(0x7F01E0)); dp8_set(0x02, (u8)g_cpu.A);
    ldy(0x00);
    do {
        lda8(0x08); dp8_set(0x00, 0x08);
        lda8(abs8(0x7F00 + g_cpu.Y));
        for (;;) {                                  /* dp 00 = 8 - trailing zero bits */
            g_cpu._flag_C = g_cpu.A & 1; lda8((u8)(g_cpu.A >> 1));
            if (g_cpu._flag_C) break;
            dec_dp8(0x00);
            if (g_cpu._flag_Z) break;
        }
        u8 f = dp8(0x02); g_cpu._flag_C = f & 1; f >>= 1; dp8_set(0x02, f); dc_set_flags_nz8(f);
        bool grant = true;
        if (!g_cpu._flag_C) {
            ldx(g_cpu.Y);
            lda8(dp8(0x00)); dc_cmp8((u8)g_cpu.A, bus8(0xFFF951 + g_cpu.X));
            grant = !g_cpu._flag_C;
        }
        if (grant) {
            ldx(dp8(0x00));
            lda8(bus8(0xFFF9BB + g_cpu.X));
            if (!g_cpu._flag_Z) {
                lda8((u8)(g_cpu.A | abs8(0x7F00 + g_cpu.Y))); abs8_set(0x7F00 + g_cpu.Y, (u8)g_cpu.A);
                lda8((u8)g_cpu.Y);
                lda8(asl8((u8)g_cpu.A)); lda8(asl8((u8)g_cpu.A)); lda8(asl8((u8)g_cpu.A));
                lda8(dc_adc8((u8)g_cpu.A, dp8(0x00)));
                tax();
                lda8((u8)((abs8(0x7701 + g_cpu.X) & 0x1E) | 0x40)); abs8_set(0x7701 + g_cpu.X, (u8)g_cpu.A);
            }
        }
        iny(); cpy(0x07);
    } while (!g_cpu._flag_C);
    /* tech name tiles: 15 rows, each 3 pairs to [16] + offsets CC:24B4 / CC:24B5 << 6 */
    dc_set_mx(0, 0);
    dp16_set(0x06, 0);
    lda16(0x1640); dp16_set(0x16, 0x1640);
    lda16(0x007E); dp16_set(0x18, 0x007E);
    do {
        dc_set_mx(0, 0);
        lda16(dp16(0x06)); asl_a(1); lda16(dc_adc16(g_cpu.A, dp16(0x06))); tax();
        asl_a(1); dp16_set(0x08, g_cpu.A);
        lda16((u16)(bus16(0xCC24B3 + g_cpu.X) & 0xFF00)); lsr_a(2); dp16_set(0x00, g_cpu.A);
        lda16((u16)(bus16(0xCC24B4 + g_cpu.X) & 0xFF00)); lsr_a(2); dp16_set(0x02, g_cpu.A);
        dc_set_mx(1, 0);
        ldx(dp16(0x08));
        lda8(0x03); dp8_set(0x04, 0x03);
        do {
            ldy(dp16(0x00)); lda8(bus8(0xCC28DB + g_cpu.X)); sta_ind16_y(); inx();
            ldy(dp16(0x02)); lda8(bus8(0xCC28DB + g_cpu.X)); sta_ind16_y();
            inc_dp8(0x16); inx();
            dec_dp8(0x04);
        } while (!g_cpu._flag_Z);
        inc_dp8(0x06);
        lda8(dp8(0x06)); dc_cmp8((u8)g_cpu.A, 0x0F);
    } while (!g_cpu._flag_C);
    /* combo marks: for each set bit c of CC:2979 + row, the next CC:2935 tile at row + 64 c */
    dp8_set(0x04, 0); dp8_set(0x06, 0);
    do {
        ldx(dp16(0x06));
        lda8(bus8(0xCC2979 + g_cpu.X)); dp8_set(0x00, (u8)g_cpu.A);
        dp8_set(0x02, 0); dp8_set(0x03, 0);
        dec_dp8(0x03);
        for (;;) {
            inc_dp8(0x03);
            u8 m = dp8(0x00); g_cpu._flag_C = m >> 7; m = (u8)(m << 1); dp8_set(0x00, m); dc_set_flags_nz8(m);
            if (g_cpu._flag_C) {
                dc_set_mx(0, 0);
                lda16(dp16(0x02)); lsr_a(2); tay();
                ldx(dp16(0x04));
                dc_set_mx(1, 0);
                lda8(bus8(0xCC2935 + g_cpu.X)); sta_ind16_y();
                inc_dp8(0x04);
                continue;
            }
            lda8(dp8(0x00));
            if (g_cpu._flag_Z) break;
        }
        inc_dp8(0x06); inc_dp8(0x16);
        lda8(dp8(0x06)); dc_cmp8((u8)g_cpu.A, 0x0F);
    } while (!g_cpu._flag_C);
    plp();
}
