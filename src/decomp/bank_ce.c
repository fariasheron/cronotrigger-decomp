/* Bank CE: menu screen helpers (OAM clear, palette and tile row copies). */
#include "ram.h"

/* CE:F066  Hides all 128 sprites (y = F0 in the OAM buffer at 0700) and
 * sets the high table (0900-0920) to AAAA (large, x bit 8 clear). */
DC_ENTRY(CEF066, RTL)
{
    dc_set_mx(1, 1);
    dc_set_a8(0xF0);
    for (g_cpu.X = 0x10; g_cpu.X; g_cpu.X -= 4)
        for (int k = 0; k < 31; k++) abs8_set(0x070D + 0x10 * k + g_cpu.X, 0xF0);
    dc_set_mx(0, 1);
    g_cpu.A = 0xAAAA;
    for (g_cpu.X = 0x1E; g_cpu.X; g_cpu.X -= 2) abs16_set(0x0900 + g_cpu.X, 0xAAAA);
    abs16_set(0x0901, 0xAAAA);
    g_cpu.A = g_cpu.D;
    dc_set_flags_nz16(g_cpu.A);
    dc_set_mx(1, 0);
}

/* CE:E18E  Animated palette row: with CDC8 set, clears CD47..CDC6; else
 * copies 128 bytes of CE:E08E + 4 * ((CDC7 / 4) & 1F) there and counts
 * CDC7 up. */
DC_ENTRY(CEE18E, RTL)
{
    if (abs8(0xCDC8)) {
        g_cpu.A = g_cpu.X = g_cpu.D;
        dc_set_mx(0, 0);
        do {
            abs16_set(0xCD47 + g_cpu.X, 0);
            g_cpu.X += 2;
            dc_cmp16(g_cpu.X, 0x0080);
        } while (g_cpu.X != 0x0080);
        g_cpu.A = g_cpu.D;
        dc_set_flags_nz16(g_cpu.A);
        dc_set_mx(1, 0);
        return;
    }
    u8 db = g_cpu.DB;
    u8 t = abs8(0xCDC7);
    abs8_set(0xCDC7, (u8)(t + 1));
    dc_set_a8((t >> 2) & 0x1F);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16((u16)(g_cpu.A << 2), 0xE08E);
    g_cpu.Y = 0xCD47;
    g_cpu.A = 0x007F;
    dc_mvn(0x7E, 0xCE);
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    g_cpu.DB = db;
    dc_set_flags_nz8(db);
}

/* CE:E000  Swaps in or out (dp 7C bit 0) a 27-word slice of 8 tables:
 * C161.. <-> C4E1.. (each 6C bytes apart), starting at 2 * [dp 40]. */
DC_ENTRY(CEE000, RTL)
{
    u8 i = bus8((u32)dp8(0x42) << 16 | dp16(0x40));
    g_cpu._flag_C = i >> 7;
    dc_set_a8((u8)(i << 1));
    g_cpu.X = g_cpu.A;
    dc_set_mx(0, 1);
    g_cpu.A = g_cpu.X;
    g_cpu._flag_C = 0;
    dp16_set(0x45, dc_adc16(g_cpu.A, 0x006C));
    int out = dp16(0x7C) & 1;
    do {
        for (int k = 0; k < 8; k++) {
            u16 a = (u16)(0xC161 + 0x6C * k), b = (u16)(0xC4E1 + 0x6C * k);
            g_cpu.A = abs16((out ? a : b) + g_cpu.X);
            abs16_set((out ? b : a) + g_cpu.X, g_cpu.A);
        }
        g_cpu.X = (u8)(g_cpu.X + 4);
        dc_cmp8((u8)g_cpu.X, dp8(0x45));
    } while ((u8)g_cpu.X != dp8(0x45));
    g_cpu.A = g_cpu.D;
    dc_set_flags_nz16(g_cpu.A);
    dc_set_mx(1, 0);
}

/* CE:EE6E  Copies 9 rows of 30 bytes (sources listed at CE:F58E +
 * CE:F24E[A]; the third depends on 2A21 bit 0) to 7E:C867, 2D apart. */
DC_ENTRY(CEEE6E, RTL)
{
    u8 db = g_cpu.DB;
    g_cpu._flag_C = (g_cpu.A >> 7) & 1;
    dc_set_a8((u8)(g_cpu.A << 1));
    g_cpu.X = g_cpu.A;
    dc_set_mx(0, 0);
    g_cpu.X = bus16(0xCEF24E + g_cpu.X);
    u16 x = g_cpu.X;
    u16 src[9];
    src[0] = bus16(0xCEF58E + x);
    src[1] = bus16(0xCEF590 + x);
    src[2] = (abs16(0x2A21) & 1) ? bus16(0xCEF5A0 + x) : bus16(0xCEF592 + x);
    for (int k = 3; k < 9; k++) src[k] = bus16(0xCEF594 + x + 2 * (k - 3));
    for (int k = 0; k < 9; k++) {
        g_cpu.X = src[k];
        g_cpu.Y = (u16)(0xC867 + 0x2D * k);
        g_cpu.A = 0x001D;
        dc_mvn(0x7E, 0xCE);
    }
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    g_cpu.DB = db;
    dc_set_flags_nz8(db);
}

/* CE:E7CF  Angle (00-FF, 40 = a quarter turn) from point (dp 25, 26) to
 * (dp 23, 24), both rounded down to multiples of 8, through the
 * arctangent table C0:F300 (32x32 in steps of 8); dp 45 = angle. The
 * rounding of the deltas (+8 when bit 2 is set) is the original's. */
DC_ENTRY(CEE7CF, RTL)
{
    for (int k = 0; k < 2; k++) {
        u8 r = dp8((u8)(0x23 + k)) & 7;
        dp8_set(0x45, r);
        g_cpu._flag_C = 1;
        dp8_set((u8)(0x23 + k), dc_sbc8(dp8((u8)(0x23 + k)), r));
        g_cpu._flag_C = 1;
        dp8_set((u8)(0x25 + k), dc_sbc8(dp8((u8)(0x25 + k)), r));
    }
    for (int k = 0; k < 2; k++) {
        g_cpu._flag_C = 1;
        dp8_set((u8)(0x27 + 2 * k), dc_sbc8(dp8((u8)(0x23 + k)), dp8((u8)(0x25 + k))));
        dp8_set((u8)(0x28 + 2 * k), dc_sbc8(0x00, 0x00));        /* sign */
    }
    for (int k = 0; k < 2; k++) {
        u8 d = dp8((u8)(0x27 + 2 * k)), s = dp8((u8)(0x28 + 2 * k));
        g_cpu._flag_C = 1;
        dp8_set((u8)(0x48 + 2 * k), dc_sbc8(d ^ s, s));          /* |delta| */
        dp8_set((u8)(0x49 + 2 * k), 0);
    }
    dc_set_mx(0, 0);
    for (int k = 0; k < 2; k++) {
        u16 v = dp16((u8)(0x48 + 2 * k));
        if (v & 4) { g_cpu._flag_C = 0; dp16_set((u8)(0x48 + 2 * k), dc_adc16(v, 8)); }
    }
    u16 col = dp16(0x48) >> 3;
    g_cpu._flag_C = dp16(0x48) >> 2 & 1;
    dp16_set(0x46, col);
    g_cpu._flag_C = 0;
    dp16_set(0x4C, dc_adc16((u16)((dp16(0x4A) & 0xFFF8) << 2), col));
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    g_cpu.X = dp16(0x4C);
    u8 a = bus8(0xC0F300 + g_cpu.X);
    dp8_set(0x45, a);
    if (dp8(0x28) & 0x80) {
        dc_set_a8(dp8(0x2A));
        if (dp8(0x2A) & 0x80) return;
        g_cpu._flag_C = 1;
        a = dc_sbc8(0x00, a);
    } else if (dp8(0x2A) & 0x80) {
        g_cpu._flag_C = 1;
        a = dc_sbc8(0x80, a);
    } else {
        g_cpu._flag_C = 0;
        a = dc_adc8(0x80, a);
    }
    dc_set_a8(a);
    dp8_set(0x45, a);
}

/* CE:E885  Fill one byte of every 4-byte slot in 16 tables (0x38 apart).
 * dp7C bit 0 picks the table group (C4E0 or C160); CAA3 bit 7 picks the
 * value (clear: FF, set: low byte of D and INC $45); dp45 bit 1 picks the
 * byte within the slot. */
DC_ENTRY(CEE885, RTS)
{
    u16 base = (dp8(0x7C) & 1) ? 0xC4E0 : 0xC160;
    u8 v;
    int zero = (abs8(0xCAA3) & 0x80) != 0;
    if (!zero) { base -= (dp8(0x45) & 2) ? 0 : 2; v = 0xFF; dc_set_a8(0xFF); }
    else       { base -= (dp8(0x45) & 2) ? 1 : 3; g_cpu.A = g_cpu.D; v = (u8)g_cpu.D; }
    for (u8 y = 0x38; y; y -= 4)
        for (int t = 0; t < 16; t++) abs8_set((u16)(base + 0x38 * t + y), v);
    g_cpu.Y = 0;
    if (zero) {
        u8 r = (u8)(dp8(0x45) + 1);
        dp8_set(0x45, r);
        dc_set_flags_nz8(r);
    } else dc_set_flags_nz8(0);
}

/* CE:EA82  Build the 0x380-byte window table pair (left/right edge per line,
 * 4 bytes a line) at C160 (dp7C bit 0 clear) or C4E0 (set).
 * A = first line; dp51 = lines before the shape; CAA1/CAA2/CA9F/4F give a
 * Bresenham-style edge (start 4F, CAA2 lines, step CAA1/CAA2, CA9F bit 7 =
 * leftwards); CAA3 picks how the rest of the table is completed (00: blank,
 * 01..7F: extend and mirror bottom to top, 80..FF: mirror top to bottom).
 * Entry bit 1 selects the second window's bytes. */
static void wt_fill16(u16 at, u16 y, u16 end, int below)  /* 00FF pairs every 4 bytes */
{
    do { abs16_set((u16)(at + y), 0x00FF); y += 4; } while (below ? y < end : y != end);
    g_cpu.Y = y;
}

static void wt_mirror(u16 at, u16 x, u16 y, int dx, int dy, u16 stop, int stop_on_x)
{
    u8 a;
    do {
        a = abs8((u16)(at + x)) ^ 0xFF;
        abs8_set((u16)(at + y), a);
        x += dx; y += dy;
    } while ((stop_on_x ? x : y) != stop);
    g_cpu.X = x; g_cpu.Y = y;
    dc_set_a8(a);
    dc_set_flags_nz8(0);          /* CPX/CPY equal: Z=1, N=0, C=1 */
    g_cpu._flag_C = 1;
}

DC_ENTRY(CEEA82, RTS)
{
    u8 entry = (u8)g_cpu.A;
    dc_push8(entry);
    dp16_set(0x45, g_cpu.A);
    dc_set_mx(1, 1);
    dc_jsr(0xCEE885, 0xCEEA88);
    dc_set_mx(1, 0);
    u16 a16 = (u16)((g_cpu.A & 0xFF00) | dp8(0x51));
    g_cpu._flag_C = 0;
    dp16_set(0x47, dc_adc16((u16)(a16 << 2), dp16(0x45)));
    g_cpu.A = g_cpu.D;
    u16 hi = g_cpu.D & 0xFF00;
    u16 G = (dp8(0x7C) & 1) ? 0xC4E0 : 0xC160;
    u16 y, x = g_cpu.X;

    /* the lines above the shape */
    if (abs8(0xCAA3) & 0x80) {
        y = dp16(0x45);
        if (y != dp16(0x47)) {
            u8 v = dp8(0x4F);
            if (v == 0) {
                wt_fill16(G, y, dp16(0x47), 0);
                y = g_cpu.Y;
            } else if (v == 0xFF) {
                x = (u16)(0x037C + dp16(0x45));
                do { abs8_set((u16)(G + 1 + y), 0xFF); abs8_set((u16)(G + x), 0xFF); x -= 4; y += 4; }
                while (y != dp16(0x47));
            } else {
                do { abs8_set((u16)(G + 1 + y), v); y += 4; } while (y != dp16(0x47));
            }
        }
    }

    /* the edge */
    y = dp16(0x47);
    x = hi | abs8(0xCAA1);
    dp16_set(0x45, x); dp16_set(0x47, x);
    x = hi | abs8(0xCAA2);
    dp16_set(0x49, x); dp16_set(0x4B, x);
    g_cpu.A = hi | abs8(0xCAA2);
    if (x) {
        x = hi | dp8(0x4F);
        int dir = (abs8(0xCA9F) & 0x80) ? -1 : 1;
        do {
            u8 a = dp8(0x45);
            while (a >= dp8(0x49)) { g_cpu._flag_C = 1; a = dc_sbc8(a, dp8(0x49)); x += dir; }
            g_cpu._flag_C = 0;
            dp8_set(0x45, dc_adc8(a, dp8(0x47)));
            abs8_set((u16)(G + 1 + y), (u8)x);
            y += 4;
            dp8_set(0x4B, (u8)(dp8(0x4B) - 1));
        } while (dp8(0x4B));
        g_cpu.A = hi | (u8)x;
    }

    /* the rest */
    int k = (dc_pull8() & 2) ? 3 : 1;
    u16 end = k == 1 ? 0x0380 : 0x0382;
    u8 c = abs8(0xCAA3);
    g_cpu.A = hi | c;
    if (c & 0x80) {
        wt_mirror((u16)(G + k + 1), g_cpu.D, 0x037C, 4, -4, 0x01C0, 1);
        return;
    }
    if (c == 0) {
        wt_fill16((u16)(G + 0x1C0 + k), g_cpu.D, 0x01C0, 0);
        g_cpu.X = x;
        g_cpu.A = g_cpu.D;
        g_cpu._flag_N = g_cpu.D >> 15;
        g_cpu._flag_Z = g_cpu.D == 0;
        g_cpu._flag_C = 1;
        return;
    }
    if (y != end) {
        u8 a = (u8)x;
        if (a == 0xFF) wt_fill16((u16)(G + 1), y, end, 1);
        else if (a == 0) {
            do { abs8_set((u16)(G + 1 + y), 0); abs8_set((u16)(G + k + 1 + x), 0); x += 4; y += 4; }
            while (y < end);
        } else {
            do { abs8_set((u16)(G + 1 + y), a); y += 4; } while (y < end);
        }
    }
    wt_mirror((u16)(G + k), 0x037C, g_cpu.D, -4, 4, 0x01C0, 0);
}

/* CE:EA7E  Long entry of CE:EA82. */
DC_ENTRY(CEEA7E, RTL) { dc_CEEA82_glue(); }
