/* Scaled bitmap renderer. Bank C3. */
#include "ram.h"

/* 8 source pixels (dp 3B..42, one byte each) to 4 bitplane bytes at
 * DB:Y..Y+3: plane p of every pixel, first pixel in bit 7. The shifts
 * go through A and the carry exactly as the original (LSR dp / ROL A). */
static void planes8(void)
{
    u8 a = (u8)g_cpu.A;
    for (int p = 0; p < 4; p++) {
        for (int k = 0; k < 8; k++) {
            u8 v = dp8((u8)(0x3B + k));
            u8 c = v & 1;
            dp8_set((u8)(0x3B + k), v >> 1);
            u8 out = a >> 7;
            a = (u8)(a << 1 | c);
            g_cpu._flag_C = out;
        }
        abs8_set(g_cpu.Y + p, a);
    }
    dc_set_flags_nz8(a);
    dc_set_a8(a);
}

/* Samples 8 pixels of source row dp 31 into dp 3B..42: x (in X, 8.8
 * fixed point) steps by dp 29 with the carry of each step feeding the
 * next one, as in the original. The 16-bit loads leave the byte after
 * the last pixel in dp 43. */
static void sample8(void)
{
    for (int k = 0; k < 8; k++) {
        u16 x = dc_adc16(g_cpu.X, dp16(0x29));
        g_cpu.X = x;
        g_cpu.Y = x >> 8;
        u16 px = bus16(dba((u32)dp16(0x31) + g_cpu.Y));
        g_cpu.A = px;
        dp16_set((u8)(0x3B + k), px);
    }
    dc_set_flags_nz16(g_cpu.A);
}

/* C3:0307  Draws a scaled copy of a bitmap as 4-plane rows of 32 bytes.
 * dp 25 = output rows + 1, 27 = vertical step (source rows advance by
 * (33 + 27) >> 1, masked to a 64-byte row), 2B = source end, 23 = source
 * base, 29 = horizontal step, 1F = 16-pixel groups per row, 2D = output
 * row (bank DB). An output row whose source row equals the previous one
 * is copied. At the end the bytes from 2D up to the previous end (37)
 * are cleared and 37 = 2D. */
DC_ENTRY(C30307, RTL)
{
    dp16_set(0x33, 0);
    for (;;) {
        u16 n = (u16)(dp16(0x25) - 1);
        dp16_set(0x25, n);
        if (n) {
            u16 a = dc_adc16(dp16(0x33), dp16(0x27));
            dp16_set(0x33, a);
            a = (u16)((a >> 1) & 0xFFC0);
            dc_cmp16(a, dp16(0x2B));
            if ((u16)(a - dp16(0x2B)) & 0x8000) {
                g_cpu._flag_C = 0;
                a = dc_adc16(a, dp16(0x23));
                dc_cmp16(a, dp16(0x31));
                if (a == dp16(0x31)) {
                    g_cpu.Y = dp16(0x2D);
                    g_cpu._flag_C = 1;
                    g_cpu.X = dc_sbc16(dp16(0x2D), 0x0020);
                    g_cpu.A = 0x001F;
                    dc_mvn(0x7F, 0x7F);
                } else {
                    dp16_set(0x31, a);
                    g_cpu.X = 0;
                    dp16_set(0x35, dp16(0x1F));
                    do {
                        for (int half = 0; half < 2; half++) {
                            sample8();
                            g_cpu.Y = dp16(0x2F);
                            dc_set_mx(1, 0);
                            planes8();
                            dc_set_mx(0, 0);
                            g_cpu._flag_C = 0;
                            g_cpu.A = dc_adc16(g_cpu.Y, 4);
                            dp16_set(0x2F, g_cpu.A);
                            g_cpu.A = g_cpu.X;
                        }
                        u16 c = (u16)(dp16(0x35) - 1);
                        dp16_set(0x35, c);
                        dc_set_flags_nz16(c);
                    } while (dp16(0x35));
                }
                g_cpu._flag_C = 0;
                u16 r = dc_adc16(dp16(0x2D), 0x0020);
                dp16_set(0x2D, r);
                dp16_set(0x2F, r);
                g_cpu.A = r;
                continue;
            }
        }
        break;
    }
    u16 left = dc_sbc16(dp16(0x37), dp16(0x2D));
    g_cpu.A = left;
    if (!(left & 0x8000) && left) {
        g_cpu.X = dp16(0x2D);
        abs16_set(g_cpu.X, 0);
        g_cpu.Y = (u16)(g_cpu.X + 1);
        dc_mvn(0x7F, 0x7F);
    }
    g_cpu.A = dp16(0x2D);
    dc_set_flags_nz16(g_cpu.A);
    dp16_set(0x37, g_cpu.A);
}
