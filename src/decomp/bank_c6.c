/* Bank C6 helpers. */
#include "ram.h"

/* C6:EDE1  Four divisions on the hardware divider, pipelined:
 * dp 00 = (08) / (0D), 02 = (0A) / (0D), 04 = -((0A) / (0F)),
 * 06 = (08) / (0F) (16-bit dividends, 8-bit divisors). */
DC_ENTRY(C6EDE1, RTS)
{
    u16 a = dp16(0x08), b = dp16(0x0A);
    u8 d0 = dp8(0x0D), d1 = dp8(0x0F);
    abs16_set(0x4204, a);
    abs8_set(0x4206, d0);
    abs16_set(0x4204, b);
    u16 q = abs16(0x4214);
    abs8_set(0x4206, d0);
    dp16_set(0x00, q);
    abs16_set(0x4204, b);
    q = abs16(0x4214);
    abs8_set(0x4206, d1);
    dp16_set(0x02, q);
    abs16_set(0x4204, a);
    q = abs16(0x4214);
    abs8_set(0x4206, d1);
    dc_set_mx(0, 0);
    dp16_set(0x04, (u16)-q);
    g_cpu.X = a;
    g_cpu.Y = abs16(0x4214);
    g_cpu.A = (u16)-q;
    dc_set_flags_nz16(g_cpu.Y);
    dp16_set(0x06, g_cpu.Y);
}

/* C6:E8C7  Two tiles of an entry at (dp 0E): for each, the byte at
 * 7E:AB25 + (word & 3FF), if not 0, gets bits 2-4 of the entry's
 * attribute byte shifted up 2 (palette) and goes to [05] / [05] + 8. */
DC_ENTRY(C6E8C7, RTS)
{
    u16 src = dp16(0x0E);
    u32 dst = (u32)dp8(0x07) << 16 | dp16(0x05);
    dp8_set(0x00, (u8)((bus8(dba((u32)src + 3)) & 0x1C) << 2));
    dp8_set(0x01, (u8)((bus8(dba((u32)src + 7)) & 0x1C) << 2));
    g_cpu._flag_C = 0;
    for (int k = 0; k < 2; k++) {
        dc_set_mx(0, 0);
        g_cpu.X = bus16(dba((u32)src + 2 + 4 * k)) & 0x03FF;
        g_cpu.A = g_cpu.X;
        dc_set_mx(1, 0);
        g_cpu.Y = (u16)(k ? 8 : 2);
        u8 t = bus8(0x7EAB25 + g_cpu.X);
        dc_set_a8(t);
        dc_set_flags_nz8(t);
        if (t) {
            t |= dp8((u8)k);
            dc_set_a8(t);
            dc_set_flags_nz8(t);
            bus8_set(dst + (k ? 8 : 0), t);
        }
    }
}
