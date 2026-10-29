/* Filled polygons as window tables. Bank C3.
 *
 * A window table has one word per line (left, right); 01/00 = empty.
 * dp (D = 0300) 50 = table address (bank 52), 53/55 = x0/y0,
 * 57/59 = x1/y1 of the edge being drawn, 60..67 = the four corners
 * of a quad (x, y pairs). */
#include "ram.h"

/* PHD PHB PHP ... TDC XBA PLP PLB PLD: everything comes back, A = 0003. */
static u8 poly_enter(void)
{
    dc_push16(g_cpu.D);
    dc_push8(g_cpu.DB);
    dc_sync_p();
    dc_push8(g_cpu.P);
    return g_cpu.P;
}
static void poly_leave(void)
{
    g_cpu.A = (u16)(g_cpu.D << 8 | g_cpu.D >> 8);
    g_cpu.P = dc_pull8();
    cpu_p_to_mirrors(&g_cpu);
    g_cpu.DB = dc_pull8();
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* dy = 0: the line gets (min, max) of the two x. */
static void edge_flat(void)
{
    dc_set_mx(0, 0);
    g_cpu.A = (u16)((dp16(0x55) & 0xFF) << 1);
    g_cpu.Y = g_cpu.A;
    dc_set_mx(1, 0);
    u32 t = ((u32)dp8(0x52) << 16 | dp16(0x50)) + g_cpu.Y;
    u8 x0 = dp8(0x53), x1 = dp8(0x57), first, second;
    dc_cmp8(x0, x1);
    if (g_cpu._flag_N) { first = x0; second = x1; }
    else { first = x1; second = x0; }
    bus8_set(t, first);
    g_cpu.Y += 2;
    bus8_set(t + 2, second);
    dc_set_a8(second);
}

/* One x step per line, error term in dp FC; the original's carries are
 * followed exactly (they shift the rounding by one now and then). */
static void edge_lines(int left)
{
    u16 e;
    if (!left) {
        g_cpu._flag_C = 1;
        e = dc_sbc16(dp16(0xF8), dp16(0xFA));
    } else {
        e = dc_sbc16(dp16(0xFA), dp16(0xF8));     /* carry is 0 here */
    }
    dp16_set(0xFC, e);
    u8 n;
    do {
        e = dp16(0xFC);
        if (!(e & 0x8000)) {
            do {
                g_cpu.X += left ? -1 : 1;
                e = dc_sbc16(e, dp16(0xFA));
            } while (!(e & 0x8000));
        }
        e = dc_adc16(e, dp16(0xF8));
        dp16_set(0xFC, e);
        g_cpu.A = g_cpu.X;
        abs8_set(g_cpu.Y, (u8)g_cpu.X);
        g_cpu.Y += 2;
        n = (u8)(dp8(0xF2) - 1);
        dp8_set(0xF2, n);
    } while (n);
}

/* Draws the edge (53,55)-(57,59) into the table: the x of each line from
 * the upper end down to the line above the lower end. */
static void edge(void)
{
    dc_set_mx(1, 0);
    g_cpu.D = 0x0300;
    g_cpu.DB = dp8(0x52);
    dp8_set(0x54, 0); dp8_set(0x58, 0); dp8_set(0x56, 0); dp8_set(0x5A, 0); dp8_set(0xFB, 0);
    g_cpu._flag_C = 1;
    u8 dy = dc_sbc8(dp8(0x59), dp8(0x55));
    dc_set_a8(dy);
    if (!dy) { edge_flat(); return; }
    if (!g_cpu._flag_C) {                       /* going up: swap the ends */
        g_cpu.Y = dp16(0x57); g_cpu.X = dp16(0x53);
        dp16_set(0x53, g_cpu.Y); dp16_set(0x57, g_cpu.X);
        g_cpu.Y = dp16(0x59); g_cpu.X = dp16(0x55);
        dp16_set(0x55, g_cpu.Y); dp16_set(0x59, g_cpu.X);
        g_cpu._flag_C = 1;
        dy = dc_sbc8((u8)g_cpu.X, dp8(0x55));
        dc_set_a8(dy);
    }
    dp8_set(0xFA, dy);
    dp8_set(0xF2, dy);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.Y = dc_adc16((u16)(dp16(0x55) << 1), dp16(0x50));
    g_cpu.X = dp16(0x53);
    g_cpu._flag_C = 1;
    u16 dx = dc_sbc16(dp16(0x57), dp16(0x53));
    if (!(dx & 0x8000)) {
        dp16_set(0xF8, dx);
        edge_lines(0);
    } else {
        dp16_set(0xF8, (u16)-dx);
        edge_lines(1);
    }
}

/* C3:1025 */
DC_ENTRY(C31025, RTL)
{
    poly_enter();
    edge();
    poly_leave();
}

/* C3:0011 (C3:0EFA)  Fills the table with the quad 60..67: the four edges
 * are drawn starting at the topmost corner; the first edge that goes up
 * moves the writes to the right byte of each line (dp 50 + 1). Lines above
 * the top and below the bottom get 01/00. */
static void quad(void)
{
    dc_set_mx(1, 0);
    g_cpu.D = 0x0300;
    dp8_set(0x69, 0);
    int start = 0;
    u8 top = dp8(0x61);
    for (int k = 1; k < 4; k++) {
        u8 y = dp8((u8)(0x61 + 2 * k));
        dc_cmp8(top, y);
        if (g_cpu._flag_C) { start = k; top = y; }
    }
    dc_set_a8(top);
    dp8_set(0x68, top);
    dc_set_mx(0, 0);
    g_cpu.Y = (u16)(0x0F35 + 0x13 * start);
    dp16_set(0xF6, 0x0F35);
    dp16_set(0xF4, 4);
    dp16_set(0xF0, g_cpu.Y);
    for (int k = start;; k = (k + 1) & 3) {
        int k2 = (k + 1) & 3;
        dp16_set(0x53, dp16((u8)(0x60 + 2 * k)));
        dp16_set(0x55, dp16((u8)(0x61 + 2 * k)));
        dp16_set(0x57, dp16((u8)(0x60 + 2 * k2)));
        g_cpu.A = dp16((u8)(0x61 + 2 * k2));
        dp16_set(0x59, g_cpu.A);
        /* C3:0F83 */
        dp16_set(0x55, dp16(0x55) & 0x00FF);
        dp16_set(0x59, dp16(0x59) & 0x00FF);
        dc_cmp16(dp16(0x59), dp16(0x55));
        if (g_cpu._flag_N && dp16(0xF6)) {
            dp16_set(0x50, (u16)(dp16(0x50) + 1));
            dp16_set(0x6A, dp16(0x55));
            dp16_set(0xF6, 0);
        }
        poly_enter();
        edge();
        poly_leave();
        u16 n = (u16)(dp16(0xF4) - 1);
        dp16_set(0xF4, n);
        if (!n) break;
    }
    /* C3:0FA5 */
    int hi = dp8(0x52) & 1;
    dp16_set(0x50, (u16)(dp16(0x50) - 1));
    u16 bottom = dp16(0x6A);
    dp16_set(0xF0, (u16)((((0x00FF - bottom) & 0xFFFF) << 1) - 1));
    g_cpu.Y = g_cpu.X = (u16)((bottom << 1) + dp16(0x50));
    g_cpu.A = 1;
    bus16_set((u32)dp8(0x52) << 16 | dp16(0x50), 1);
    u8 b = hi ? 0x7F : 0x7E;
    bus16_set((u32)b << 16 | g_cpu.X, 1);
    g_cpu.Y += 2;
    g_cpu.A = dp16(0xF0);
    if (!g_cpu.A) return;
    dc_mvn(b, b);
    g_cpu.X = dp16(0x50);
    g_cpu.Y = (u16)(g_cpu.X + 2);
    g_cpu.A = dp16(0x68);
    if (!g_cpu.A) return;
    g_cpu.A = (u16)((g_cpu.A << 1) - 1);
    dc_mvn(b, b);
}
DC_ENTRY(C30011, RTL)
{
    poly_enter();
    quad();
    poly_leave();
}
