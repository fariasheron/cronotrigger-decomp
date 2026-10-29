/* Field screen effects: circular window wipes (C3:0008) and the
 * brightness ripple (C3:000E). Bank C0.
 *
 * 7F:1522/1523 and 0BD9..0BDE are the window/color-math settings the
 * NMI copies to the PPU; dp 28 bit 20 enables the window table, bit 80
 * the ripple. dp 3A/3B = circle center, 3C = radius (3D = target),
 * 39 = effect state, 53 selects the table buffer. */
#include "ram.h"

static void tsb8(u8 o, u8 v) { u8 m = dp8(o); g_cpu._flag_Z = (m & v) == 0; dp8_set(o, m | v); }
static void trb8(u8 o, u8 v) { u8 m = dp8(o); g_cpu._flag_Z = (m & v) == 0; dp8_set(o, m & ~v); }

static void window_regs(u8 w, u8 d9, u8 da, u8 db)
{
    bus8_set(0x7F1522, w);
    bus8_set(0x7F1523, w);
    abs8_set(0x0BD9, d9);
    abs8_set(0x0BDA, da);
    abs8_set(0x0BDB, db);
}

/* Draws the circle (center 0350/0352, radius 0354) into the window table
 * of the current buffer; inside = 0 (window outside the circle) or 1. */
static void circle(u8 cx, u8 cy, u8 r, u8 inside, u32 site)
{
    abs8_set(0x0350, cx);
    abs8_set(0x0352, cy);
    abs8_set(0x0354, r);
    tsb8(0x28, 0x20);
    g_cpu.X = dp8(0x53) ? 0x17C8 : 0x1AC8;
    abs16_set(0x0356, g_cpu.X);
    abs8_set(0x0358, 0x7F);
    dc_set_a8(inside);
    dc_jsl(0xC30008, site);
}

/* Window off: settings cleared, table disabled. */
static void circle_off(void)
{
    dp8_set(0x39, 0);
    window_regs(0x00, 0x00, 0x00, 0x00);
}

/* C0:220E  Circle opening (radius grows to dp 3D), outside masked. */
DC_ENTRY(C0220E, RTS)
{
    dc_cmp8(dp8(0x3C), dp8(0x3D));
    if (g_cpu._flag_C) { dp8_set(0x39, 0); dp8_set(0x3C, (u8)(dp8(0x3C) - 1)); }
    window_regs(0x17, 0x22, 0x00, 0x00);
    circle(dp8(0x3A), dp8(0x3B), dp8(0x3C), 0, 0xC0225C);
    dp8_set(0x3C, (u8)(dp8(0x3C) + 1));
}

/* C0:2269  Circle closing to nothing, outside masked; at radius 0 the
 * window turns off. */
DC_ENTRY(C02269, RTS)
{
    if (!dp8(0x3C)) {
        circle_off();
        dc_set_a8(0x20);
        trb8(0x28, 0x20);
        return;
    }
    dp8_set(0x3C, (u8)(dp8(0x3C) - 1));
    window_regs(0x17, 0x22, 0x00, 0x00);
    circle(dp8(0x3A), dp8(0x3B), dp8(0x3C), 0, 0xC022B7);
}

/* C0:232A  Circle opening with color math on the inside (0BDB = 20,
 * 0BDE = 12). */
DC_ENTRY(C0232A, RTS)
{
    dc_cmp8(dp8(0x3C), dp8(0x3D));
    if (g_cpu._flag_C) { dp8_set(0x39, 0); dp8_set(0x3C, (u8)(dp8(0x3C) - 1)); }
    window_regs(0x17, 0x00, 0x00, 0x20);
    abs8_set(0x0BDE, 0x12);
    circle(dp8(0x3A), dp8(0x3B), dp8(0x3C), 1, 0xC0237D);
    dp8_set(0x3C, (u8)(dp8(0x3C) + 1));
}

/* C0:238A  Same, closing; at radius 0 everything returns to normal and
 * the screen scroll (dp 21/22) goes back to E0. */
DC_ENTRY(C0238A, RTS)
{
    if (!dp8(0x3C)) {
        circle_off();
        abs8_set(0x0BDE, 0x02);
        dp8_set(0x21, 0xE0);
        dp8_set(0x22, 0xE0);
        dc_set_a8(0x20);
        trb8(0x28, 0x20);
        return;
    }
    dp8_set(0x3C, (u8)(dp8(0x3C) - 1));
    window_regs(0x17, 0x00, 0x00, 0x20);
    abs8_set(0x0BDE, 0x12);
    circle(dp8(0x3A), dp8(0x3B), dp8(0x3C), 1, 0xC023DD);
}

/* C0:22D8  Brightness ripple: one step while 0350 (the ripple mode) is
 * on; once it ends and the radius is under 2, the effect is cleared. */
DC_ENTRY(C022D8, RTS)
{
    if (abs8(0x0350)) {
        window_regs(0x17, 0x88, 0x88, 0x88);
        abs8_set(0x0BDC, 0x00);
        abs8_set(0x0BDD, 0xFF);
        dc_jsl(0xC3000E, 0xC022FA);
        dc_set_a8(0x80);
        tsb8(0x28, 0x80);
        dc_set_a8(0x0F);
        dp8_set(0x19, 0x0F);
        return;
    }
    dc_set_a8(dp8(0x3C));
    dc_cmp8(dp8(0x3C), 0x02);
    if (dp8(0x3C) >= 2) return;
    window_regs(0x00, 0x00, 0x00, 0x00);
    abs8_set(0x0BDD, 0x00);
    dc_set_a8(0x80);
    trb8(0x28, 0x80);
}

/* Fixed circle (center 7C,06) for two scenes; dp 39 counts 7 -> 8
 * (2514) or 9 -> 10 (2589), otherwise it is reset. */
static void fixed_circle(u8 r, u8 last, u32 site)
{
    circle(0x7C, 0x06, r, 0, site);
    u8 s = dp8(0x39);
    dc_set_a8(s);
    dc_cmp8(s, last);
    dp8_set(0x39, s == last ? (u8)(s + 1) : 0);
}

/* C0:2514 */
DC_ENTRY(C02514, RTS)
{
    bus8_set(0x7F1522, 0x17);
    bus8_set(0x7F1523, 0x00);
    abs8_set(0x0BD9, 0x00);
    abs8_set(0x0BDB, 0x00);
    abs8_set(0x0BDA, 0x03);
    abs8_set(0x0BDE, 0x22);
    abs16_set(0x1D8D, 0x0100);
    abs16_set(0x1D8F, 0xF01D);
    abs16_set(0x1D91, 0x01C0);
    fixed_circle(0x29, 0x07, 0xC02579);
}

/* C0:2589 */
DC_ENTRY(C02589, RTS)
{
    bus8_set(0x7F1522, 0x17);
    bus8_set(0x7F1523, 0x00);
    abs8_set(0x0BD9, 0x20);
    abs8_set(0x0BDB, 0x00);
    abs8_set(0x0BDA, 0x02);
    abs8_set(0x0BDE, 0x22);
    abs8_set(0x0BDF, 0x04);
    abs8_set(0x0BE0, 0x04);
    abs8_set(0x0BD7, 0x04);
    bus8_set(0x7F1520, 0x04);
    abs8_set(0x0BD8, 0x13);
    bus8_set(0x7F1521, 0x13);
    abs16_set(0x1D8D, 0x0100);
    fixed_circle(0x27, 0x09, 0xC025FE);
}
