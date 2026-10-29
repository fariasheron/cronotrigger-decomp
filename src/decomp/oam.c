/* Sprite list writers. Bank C3. */
#include "ram.h"

/* C3:0D5E  Copies a sprite group into the OAM buffer at 0700 + dp 11.
 * The group is read through the WRAM port from 7E:A (A = address):
 * a count byte n, then n - 1 entries of 4 bytes (tile, attributes, x, y).
 * x is offset by dp 15 (16-bit); anything at 0100 or beyond is parked at
 * x = 01, y = 98. y is offset by dp 17. Each sprite also shifts its x bit 8
 * and a set size bit into the high table byte at dp 13 (4 sprites per byte).
 * dp 11 / 13 are left pointing past the last sprite. */
DC_ENTRY(C30D5E, RTL)
{
    abs16_set(0x2181, g_cpu.A);
    dc_set_mx(1, 0);
    g_cpu.Y = dp16(0x11);
    g_cpu.X = dp16(0x13);
    abs8_set(0x2183, 0);
    u8 n = abs8(0x2180);
    dc_set_a8(n);
    dp8_set(0x19, n);
    for (;;) {
        n = (u8)(dp8(0x19) - 1);
        dp8_set(0x19, n);
        dc_set_flags_nz8(n);
        if (!n) break;
        abs8_set(0x0702 + g_cpu.Y, abs8(0x2180));
        abs8_set(0x0703 + g_cpu.Y, abs8(0x2180));
        g_cpu._flag_C = 0;
        u16 x = dc_adc16(abs8(0x2180), dp16(0x15));
        dc_cmp16(x, 0x0100);
        if (!((u16)(x - 0x0100) & 0x8000)) x = 0x9801;
        abs16_set(0x0700 + g_cpu.Y, x);
        g_cpu._flag_C = x >> 15;                    /* ASL: x bit 8 is the sign */
        g_cpu.A = (u16)(x << 1);
        u8 hi = abs8(g_cpu.X);
        hi = (u8)(g_cpu._flag_C << 7 | hi >> 1);
        hi = (u8)(0x80 | hi >> 1);                  /* size bit */
        g_cpu._flag_C = 0;
        abs8_set(g_cpu.X, hi);
        dc_set_a8(dc_adc8(abs8(0x2180), dp8(0x17)));
        abs8_set(0x0701 + g_cpu.Y, (u8)g_cpu.A);
        g_cpu.Y += 4;
        dc_set_a8((u8)g_cpu.Y & 0x0F);
        if (!((u8)g_cpu.Y & 0x0F)) g_cpu.X++;
    }
    dc_set_mx(0, 0);
    dp16_set(0x13, g_cpu.X);
    dp16_set(0x11, g_cpu.Y);
}
