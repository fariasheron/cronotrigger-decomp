/* Field objects: position tests. Bank C0. */
#include "ram.h"

/* C0:5B63  Is the object below the bottom bound? Compares the object's tile Y
 * with bound_bottom + 1 (C set when tile_y >= bottom + 1). The sum is kept
 * in scratch $01D9. */
DC_ENTRY(C05B63, RTS)
{
    u8 limit = (u8)(ram8(FIELD_BOUND_BOTTOM) + 1);
    ram8_set(SCRATCH_01D9, limit);
    u8 ty = ram8(OBJ_TILE_Y(g_cpu.X));
    dc_set_a8(ty);
    dc_cmp8(ty, limit);
}

/* C0:5B71  Compare top bound + 1 with the object's tile Y
 * (C set when top + 1 >= tile_y). */
DC_ENTRY(C05B71, RTS)
{
    u8 limit = (u8)(ram8(FIELD_BOUND_TOP) + 1);
    dc_set_a8(limit);
    dc_cmp8(limit, ram8(OBJ_TILE_Y(g_cpu.X)));
}

/* C0:5B79  Compare the object's tile X with the right bound
 * (C set when tile_x >= right). */
DC_ENTRY(C05B79, RTS)
{
    u8 limit = ram8(FIELD_BOUND_RIGHT);
    ram8_set(SCRATCH_01D9, limit);
    u8 tx = ram8(OBJ_TILE_X(g_cpu.X));
    dc_set_a8(tx);
    dc_cmp8(tx, limit);
}

/* C0:5B86  Compare the left bound with the object's tile X
 * (C set when left >= tile_x). */
DC_ENTRY(C05B86, RTS)
{
    u8 limit = ram8(FIELD_BOUND_LEFT);
    dc_set_a8(limit);
    dc_cmp8(limit, ram8(OBJ_TILE_X(g_cpu.X)));
}

/* C0:3543  X = Y, clear carry. */
DC_ENTRY(C03543, RTS)
{
    g_cpu.X = g_cpu.Y;
    if (g_cpu.x_flag) dc_set_flags_nz8((u8)g_cpu.X); else dc_set_flags_nz16(g_cpu.X);
    g_cpu._flag_C = 0;
}

/* C0:354F  Run C0:5410 for object Y, keeping Y in $01E3 and returning it in X. */
DC_ENTRY(C0354F, RTS)
{
    DC_ASSUME_D(C0354F, 0x0100);
    ram16_set(0x01E3, g_cpu.Y);
    dc_jsr(0xC05410, 0xC03551);
    g_cpu.X = ram16(0x01E3);
    dc_set_flags_nz16(g_cpu.X);
}

/* C0:A9CD  Link the current object into the sprite list of its screen row
 * (0E00 + (row & FE) heads, 0E80/0E81 links, 0E01 = 0 once linked);
 * nothing to do when 0E01 bit 7 is clear. 8-bit registers on return. */
DC_ENTRY(C0A9CD, RTS)
{
    dc_set_mx(1, 1);
    u8 x = dp8(0x6D);
    g_cpu.X = x;
    u8 f = abs8(0x0E01 + x);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (!(f & 0x80)) return;
    u8 row = abs8(0x0A80 + x);
    g_cpu._flag_C = row & 1;
    u8 y = (u8)((row >> 1) & 0xFE);
    g_cpu.Y = y;
    u8 head = abs8(0x0E00 + y);
    if (head & 0x80) {
        abs8_set(0x0E00 + y, x);
        abs8_set(0x0E01 + x, 0);
        abs8_set(0x0E80 + x, 0x80);
        abs8_set(0x0E81 + x, 0x80);
        dc_set_a8(0x80);
        dc_set_flags_nz8(0x80);
        return;
    }
    dp8_set(0xC5, y);
    abs8_set(0x0E80 + head, x);
    abs8_set(0x0E81 + x, head);
    abs8_set(0x0E00 + y, x);
    abs8_set(0x0E01 + x, 0);
    dc_set_a8(x);
    dc_set_flags_nz8(x);
}
/* C0:5CC7  Sanity checks: a map whose script starts 0D 0A, or more than
 * 0x1700 bytes in 0306, stops on the error screen. */
DC_ENTRY(C05CC7, RTS)
{
    u8 a = ram8(0x12000);
    dc_set_a8(a);
    if (a == 0x0D) {
        u8 b = ram8(0x12001);
        dc_set_a8(b);
        if (b == 0x0A) { g_cpu.X = 0x01F0; dc_jsr(0xC02E1E, 0xC05CDA); return; }
    }
    g_cpu.X = 0x1700;
    dc_cmp16(0x1700, abs16(0x0306));
    if (g_cpu._flag_N) { g_cpu.X = 0x000F; dc_jsr(0xC02E1E, 0xC05CE8); }
}

/* C0:A98A  Unlink the current object from its screen row's sprite list
 * (undoes C0:A9CD); 0E01 = 80 marks it unlinked. */
DC_ENTRY(C0A98A, RTS)
{
    dc_set_mx(1, 1);
    u8 x = dp8(0x6D);
    g_cpu.X = x;
    u8 f = abs8(0x0E01 + x);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (f & 0x80) return;
    u8 prev = abs8(0x0E80 + x);
    if (!(prev & 0x80)) {
        u8 next = abs8(0x0E81 + x);
        abs8_set(0x0E81 + prev, next);
        g_cpu.Y = next;
        abs8_set(0x0E80 + next, prev);
        g_cpu.A = (u16)(next << 8 | 0x80);
        dc_set_flags_nz8(0x80);
        abs8_set(0x0E80 + x, 0x80);
        abs8_set(0x0E81 + x, 0x80);
        abs8_set(0x0E01 + x, 0x80);
        return;
    }
    u8 row = abs8(0x0A80 + x);
    g_cpu._flag_C = row & 1;
    u8 y = (u8)((row >> 1) & 0xFE);
    g_cpu.Y = y;
    u8 next = abs8(0x0E81 + x);
    abs8_set(0x0E00 + y, next);
    u8 a = next;
    if (!(next & 0x80)) { g_cpu.Y = next; abs8_set(0x0E80 + next, 0x80); a = 0x80; }
    dc_set_a8(a);
    dc_set_flags_nz8(a);
    abs8_set(0x0E81 + x, a);
    abs8_set(0x0E01 + x, a);
}
/* C0:7056  Free the current object's palette slot pair in 0B88 (index =
 * 1700 >> 5): C set when it owned the first entry (and also the second). */
DC_ENTRY(C07056, RTS)
{
    u8 me = dp8(0x6D);
    g_cpu.X = me;
    u16 w = (u16)(abs16(0x1700 + me) >> 5);
    g_cpu._flag_C = (abs16(0x1700 + me) >> 4) & 1;
    u8 x = (u8)w;
    g_cpu.X = x;
    g_cpu.A = (u16)((w & 0xFF00) | me);
    u8 v = abs8(0x0B88 + x);
    dc_cmp8(me, v);
    if (me != v) { g_cpu._flag_C = 0; return; }
    u8 v2 = abs8(0x0B89 + x);
    dc_cmp8(me, v2);
    if (me == v2) abs8_set(0x0B89 + x, 0x80);
    abs8_set(0x0B88 + x, 0x80);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
    g_cpu._flag_C = 1;
}
/* C0:5AC5  Make object dp EB run its function 1 now (priority 1) when it is
 * free (1C01 = 0), present, enabled and busy with a priority >= 2: save the
 * current position to the priority slot, jump to the function's start
 * (7F:2001 + obj*16 + 2) and clear the movement state. */
DC_ENTRY(C05AC5, RTS)
{
    u16 o = dp8(0xEB);
    g_cpu.X = o;
    u8 v = abs8(OBJ_CALLS_BLOCKED(o));
    dc_set_a8(v); dc_set_flags_nz8(v);
    if (v) return;
    v = abs8(OBJ_REMOVED(o)); dc_set_a8(v); dc_set_flags_nz8(v);
    if (v & 0x80) return;
    v = abs8(OBJ_EV_DISABLED(o)); dc_set_a8(v); dc_set_flags_nz8(v);
    if (v & 0x80) return;
    u8 prio = abs8(OBJ_PRIORITY(o));
    dc_set_a8(prio);
    dc_cmp8(prio, 2);
    if (prio < 2) return;
    abs8_set(0x4202, prio);
    abs8_set(0x4203, 0x80);
    dc_set_mx(0, 0);
    u16 prod = abs16(0x4216), eb = dp16(0xEB);
    u16 slot = (u16)(prod + eb);
    dp16_set(0xD9, slot);
    ram16_set(0x10580 + slot, abs16(OBJ_SCRIPT_POS(o)));
    u16 sh = (u16)(eb << 4), idx = (u16)(sh + 2);
    g_cpu._flag_C = (u32)sh + 2 > 0xFFFF;
    g_cpu._flag_V = ((~(sh ^ 2) & (sh ^ idx)) >> 15) & 1;
    u16 start = ram16(0x12001 + idx);
    abs16_set(OBJ_SCRIPT_POS(eb), start);
    dc_set_mx(1, 1);
    u16 x = eb & 0xFF;
    g_cpu.X = x;
    g_cpu.A = (u16)((start & 0xFF00) | 1);
    dc_set_flags_nz8(1);
    abs8_set(OBJ_PRIORITY(x), 1);
    abs8_set(OBJ_1A80(x), 0);
    abs8_set(OBJ_1A01(x), 0);
    abs8_set(0x1001 + x, 0);
}
/* C0:A947  Is the current object near the camera? Box = tile X in
 * [1D0A-4, 1D0C+4), tile Y in [1D0E, 1D10+5) (compared as signed bytes after
 * doubling the object's tile, kept in dp C3..C6). 0F00 = 80 and C set when
 * inside, 0F00 = 0 and C clear when outside. */
DC_ENTRY(C0A947, RTS)
{
    g_cpu._flag_C = 1;
    u8 left = dc_sbc8(abs8(0x1D0A), 4);
    dp8_set(0xC3, left);
    g_cpu._flag_C = 0;
    u8 right = dc_adc8(abs8(0x1D0C), 4);
    dp8_set(0xC4, right);
    u8 top = abs8(0x1D0E);
    dp8_set(0xC5, top);
    g_cpu._flag_C = 0;
    u8 bottom = dc_adc8(abs8(0x1D10), 5);
    dp8_set(0xC6, bottom);
    u8 x = dp8(0x6D);
    g_cpu.X = x;
    bool in = false;
    u8 tx = abs8(0x1801 + x);
    g_cpu._flag_C = tx >> 7;
    tx = (u8)(tx << 1);
    dc_set_a8(tx);
    dc_cmp8(tx, left);
    if (!g_cpu._flag_N) { dc_cmp8(tx, right);
    if (g_cpu._flag_N) {
        u8 ty = abs8(0x1881 + x);
        g_cpu._flag_C = ty >> 7;
        ty = (u8)(ty << 1);
        dc_set_a8(ty);
        dc_cmp8(ty, top);
        if (!g_cpu._flag_N) { dc_cmp8(ty, bottom); in = g_cpu._flag_N; }
    } }
    if (in) { dc_set_a8(0x80); dc_set_flags_nz8(0x80); abs8_set(0x0F00 + x, 0x80); g_cpu._flag_C = 1; }
    else { abs8_set(0x0F00 + x, 0); g_cpu._flag_C = 0; }
}

/* C0:0B86  Field state reset on map entry: copy the destination (dp 00..04)
 * to 0A..0E, clear the scroll, event and effect state, set default VRAM
 * pointers (7B/7D/7F), the HDMA tables' first entries from E4:FFE0, no
 * party leader or followers (80), the layer scroll defaults from 7E:2980..
 * and dp F8 = 0400. */
DC_ENTRY(C00B86, RTS)
{
    u16 v = dp16(0x00); g_cpu.X = v; dp16_set(0x0A, v);
    v = dp16(0x02); g_cpu.X = v; dp16_set(0x0C, v);
    dp8_set(0x0E, dp8(0x04));
    static const u8 zero1[] = { 0x10, 0x11, 0x17, 0x18, 0x38, 0x0F };
    for (unsigned i = 0; i < sizeof zero1; i++) dp8_set(zero1[i], 0);
    dp8_set(0x21, 0xE0);
    dp8_set(0x19, 0);
    dp8_set(0xBC, 0);
    abs8_set(0x0BDE, 0x02);
    dp16_set(0x7D, 0x0900);
    dp16_set(0x7F, 0x0770);
    g_cpu.X = 0x08A0;
    dp16_set(0x7B, 0x08A0);
    dp8_set(0xB1, 0xE4); dp8_set(0xB4, 0xE4); dp8_set(0xB7, 0xE4); dp8_set(0xBA, 0xE4);
    dp8_set(0x1F, 1); dp8_set(0x20, 1);
    dp8_set(0x68, 5);
    dp8_set(0x28, 0x5F);
    static const u8 zero2[] = { 0x53, 0x26, 0x29, 0x2F, 0x2D, 0x30, 0x44, 0x45, 0x46,
                                0x5F, 0x78, 0xBB, 0x62 };
    for (unsigned i = 0; i < sizeof zero2; i++) dp8_set(zero2[i], 0);
    dp8_set(0x63, 0x80);
    dp8_set(0x39, 0);
    dp8_set(0x54, 0);
    dp16_set(0x2B, 0);
    dp16_set(0xAF, bus16(0xE4FFE0));
    dp16_set(0xB2, bus16(0xE4FFE2));
    dp16_set(0xB5, bus16(0xE4FFE4));
    dp16_set(0xB8, bus16(0xE4FFE6));
    abs16_set(0x0150, 0);
    ram16_set(0x2000, 0);
    dp16_set(0x58, 0);
    static const u8 none[] = { 0x97, 0x99, 0x9B, 0x8D, 0x8E, 0x8F, 0x91, 0x90, 0x92, 0x93 };
    for (unsigned i = 0; i < sizeof none; i++) dp8_set(none[i], 0x80);
    dp8_set(0x94, ram8(0x2980));
    dp8_set(0x95, ram8(0x2981));
    dp8_set(0x96, ram8(0x2982));
    dp8_set(0xEB, 0x80); dp8_set(0xAE, 0x80);
    dp8_set(0x55, 1);
    dp8_set(0xFC, 0x53); dp8_set(0xFB, 0x65); dp8_set(0xFA, 0x45);
    ram8_set(0x2989, 0);
    abs8_set(0x0BD9, 0); abs8_set(0x0BDA, 0); abs8_set(0x0BDB, 0); abs8_set(0x0BE9, 0);
    u8 f8 = abs8(0x0400);
    g_cpu.A = f8;
    dp8_set(0xF8, f8);
    dc_set_flags_nz8(f8);
}

/* C0:011B  Snapshot for the save menu (7F:1D09..1D26): the three party
 * slots' pixel X/Y and 0C00 per member (80 = empty, skipped), the members
 * dp AB..AD, 7F:3728/3748/3768/3781 and 1DF9. Leaves the menu flag dp 29
 * as 1 (with dp 26/27 = 0) when set, and runs C0:0918 first; the leader's
 * tile and facing go to dp 02..04. */
DC_ENTRY(C0011B, RTS)
{
    u8 f = dp8(0x29);
    dc_set_a8(f);
    if (f) { dp8_set(0x29, 1); dp8_set(0x26, 0); dp8_set(0x27, 0); dc_set_a8(0); }
    dc_jsr(0xC00918, 0xC00129);
    u16 y = g_cpu.x_flag ? dp8(0x97) : dp16(0x97);
    g_cpu.Y = y;
    dp8_set(0x02, abs8(0x1801 + y));
    dp8_set(0x03, abs8(0x1881 + y));
    dp8_set(0x04, abs8(0x1600 + y));
    for (u16 x = 0; x != 6; x += 2) {
        g_cpu.X = x;
        u8 m = b0_8((u16)(g_cpu.D + 0x97 + x));
        g_cpu.A = (u16)((g_cpu.D << 8) | m);
        dc_set_flags_nz8(m);
        if (m & 0x80) continue;
        y = g_cpu.x_flag ? m : g_cpu.A;
        g_cpu.Y = y;
        ram16_set(0x11D09 + x, abs16(0x1800 + y));
        ram16_set(0x11D0F + x, abs16(0x1880 + y));
        ram16_set(0x11D15 + x, abs16(0x0C00 + y));
        g_cpu.A = abs16(0x0C00 + y);
    }
    g_cpu.X = 6;
    dc_cmp16(6, 6);
    ram8_set(0x11D1B, dp8(0xAB));
    ram8_set(0x11D1C, dp8(0xAC));
    ram8_set(0x11D1D, dp8(0xAD));
    ram16_set(0x11D1E, ram16(0x13728));
    ram16_set(0x11D20, ram16(0x13748));
    ram16_set(0x11D22, ram16(0x13768));
    u16 w = ram16(0x13781);
    ram16_set(0x11D24, w);
    g_cpu.A = w;
    u8 b = abs8(0x1DF9);
    dc_set_a8(b);
    dc_set_flags_nz8(b);
    ram8_set(0x11D26, b);
}

/* C0:59D9  Tick the objects' script timers (1001, reloaded from 1000; 1000
 * bit 7 = timer off) and queue each object whose timer ran out: kind
 * (1100) 0 objects go on the list headed by 0174, other active kinds on the
 * list whose tail is 0175, linked through 1080 (bit 7 = not queued). An
 * empty list (head/tail bit 7 set) starts with this object as both 0174
 * and 0175. */
DC_ENTRY(C059D9, RTS)
{
    u16 d0 = g_cpu.D;
    dc_push16(d0);
    g_cpu.D = 0x1000;
    u8 end = (u8)(ram8(0x12000) << 1);
    abs8_set(0x01BD, end);
    u8 x = 0, a = 0;
    do {
        g_cpu.X = x;
        a = b0_8(0x1000 + x);
        bool ready = false;
        if (!(a & 0x80)) {
            a = b0_8(0x1001 + x);
            if (!a) ready = true;
            else { u8 t = (u8)(a - 1); b0_8_set(0x1001 + x, t); ready = !t; }
        }
        if (ready) {
            a = abs8(0x1100 + x);
            if (!a) {
                if (x != abs8(0x0174)) {
                    a = abs8(0x0174);
                    if (a & 0x80) goto both;
                    b0_8_set(0x1080 + x, a);
                    abs8_set(0x0174, x);
                    a = b0_8(0x1000 + x);
                    b0_8_set(0x1001 + x, a);
                }
            } else if (!(a & 0x80) && x != abs8(0x0175)) {
                a = b0_8(0x1080 + x);
                if (a & 0x80) {
                    a = abs8(0x0175);
                    if (a & 0x80) goto both;
                    g_cpu.Y = x;
                    b0_8_set(0x1080 + a, x);
                    abs8_set(0x0175, x);
                    a = x;
                    a = b0_8(0x1000 + x);
                    b0_8_set(0x1001 + x, a);
                }
            }
            goto next;
both:
            abs8_set(0x0174, x);
            abs8_set(0x0175, x);
            a = b0_8(0x1000 + x);
            b0_8_set(0x1001 + x, a);
        }
next:
        x = (u8)(x + 2);
        g_cpu.X = x;
    } while (x != abs8(0x01BD));
    dc_set_a8(a);
    dc_cmp8(x, x);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* C0:734C  Take the current object out of the palette-slot chain 0B00/0B01
 * (prev/next, 80 = none), updating the slot owner 0B80 + (0F81 >> 1) when
 * it was the first; nothing when dp 54 bit 7 is set. C set. */
DC_ENTRY(C0734C, RTS)
{
    u8 f = dp8(0x54);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (f & 0x80) { g_cpu._flag_C = 1; return; }
    u8 me = dp8(0x6D);
    g_cpu.X = me;
    u8 next = abs8(0x0B01 + me);
    u8 prev = abs8(0x0B00 + me);
    if (next & 0x80) {
        u8 s = (u8)(abs8(0x0F81 + me) >> 1);
        if (prev & 0x80) {
            g_cpu.X = s;
            dc_set_a8(0x80);
            abs8_set(0x0B80 + s, 0x80);
        } else {
            dp8_set(0xD9, prev);
            g_cpu.X = s;
            abs8_set(0x0B80 + s, prev);
            g_cpu.X = prev;
            dc_set_a8(0x80);
            abs8_set(0x0B01 + prev, 0x80);
        }
        dc_set_flags_nz8(0x80);
        g_cpu._flag_C = 1;
        return;
    }
    dp8_set(0xD9, next);
    g_cpu.X = next;
    if (prev & 0x80) {
        dc_set_a8(0x80);
        dc_set_flags_nz8(0x80);
        abs8_set(0x0B00 + next, 0x80);
    } else {
        abs8_set(0x0B00 + next, prev);
        g_cpu.X = prev;
        dc_set_a8(next);
        dc_set_flags_nz8(next);
        abs8_set(0x0B01 + prev, next);
    }
    g_cpu._flag_C = 1;
}

/* C0:1ADF  Menu cursor input for a list of kind A: kind 1 (or a list whose
 * cursor dp 63 is unset or 4) only checks button F6 bit 7 (clears dp 34);
 * kind 2 moves the cursor dp 63 down / up (F7 bits 2 / 3) inside dp 64..65,
 * wrapping, or picks 3 into dp 62 on F8 bit 7. Other kinds save the cursor
 * in dp 66 and park it on 4. */
DC_ENTRY(C01ADF, RTS)
{
    u8 kind = (u8)g_cpu.A;
    dc_cmp8(kind, 1);
    if (kind != 1) {
        dc_cmp8(kind, 2);
        if (kind == 2) {
            u8 b = abs8(0x00F7);
            dc_set_a8(b);
            if (b & 4) {
                u8 c = (u8)(dp8(0x63) + 1);
                dc_cmp8(c, dp8(0x65));
                if (c > dp8(0x65)) { c = dp8(0x64); dc_set_flags_nz8(c); }
                dc_set_a8(c);
                dp8_set(0x63, c);
                return;
            }
            if (b & 8) {
                u8 c = dp8(0x63);
                dc_set_a8(c); dc_set_flags_nz8(c);
                if (c) {
                    c--;
                    dc_cmp8(c, dp8(0x64));
                    if (g_cpu._flag_C) { dc_set_a8(c); dp8_set(0x63, c); return; }
                }
                c = dp8(0x65);
                dc_set_a8(c); dc_set_flags_nz8(c);
                dp8_set(0x63, c);
                return;
            }
            u8 f = abs8(0x00F8);
            dc_set_a8(f);
            g_cpu._flag_Z = !(f & 0x80);
            if (f & 0x80) { dc_set_a8(3); dc_set_flags_nz8(3); dp8_set(0x62, 3); }
            return;
        }
        u8 c = dp8(0x63);
        dc_set_a8(c); dc_set_flags_nz8(c);
        if (!(c & 0x80)) {
            dc_cmp8(c, 4);
            if (c != 4) { dp8_set(0x66, c); dc_set_a8(4); dc_set_flags_nz8(4); dp8_set(0x63, 4); }
        }
    }
    u8 b = abs8(0x00F6);
    dc_set_a8(b);
    g_cpu._flag_Z = !(b & 0x80);
    if (b & 0x80) {
        g_cpu.X = 0;
        dc_set_flags_nz16(0);
        if (g_cpu.x_flag) dp8_set(0x34, 0); else dp16_set(0x34, 0);
    }
}

/* C0:7199 / 7212 / 7217  Give the current object sprite palette slot 5 /
 * 6 / 7: when the slot is free (0B80 bit 7) take it; when its owner uses
 * the same palette (1400 word) join the owner's chain (0B00/0B01) instead;
 * otherwise C clear (returning with 16-bit A, as the original does). On
 * success copy the palette (24 bytes from E4:[1400]) into both palette
 * buffers 7E:2100 + slot*32 + 2 and 7E:2300 + ..., 0F81 = slot*2, C set.
 * (Joining a chain stores into the owner's 0B80 entry and uses the owner's
 * number as the slot, as the original does.) */
/* Join owner's chain when it uses the same palette (1400 word); false (with
 * 16-bit A, as the original leaves it) when it does not. */
static bool pal_join(u8 owner, u8 me)
{
    dp8_set(0xDB, owner);
    dc_set_mx(0, 1);
    u16 pal = abs16(0x1400 + owner);
    g_cpu.X = me;
    dc_cmp16(pal, abs16(0x1400 + me));
    g_cpu.A = pal;
    if (pal != abs16(0x1400 + me)) return false;
    dc_set_mx(1, 1);
    abs8_set(0x0B00 + me, owner);
    abs8_set(0x0B01 + me, 0x80);
    abs8_set(0x0B01 + owner, me);
    return true;
}
static void pal_free_links(u8 me)
{
    abs8_set(0x0B00 + me, 0x80);
    abs8_set(0x0B01 + me, 0x80);
}
/* Own slot s (0B80, 0F81 = s*2) and load the palette into both buffers. */
static void pal_finish(u8 s, u8 me)
{
    abs8_set(0x0B80 + s, me);
    u8 a = (u8)((s << 1) & 0x0E);
    abs8_set(0x0F81 + me, a);
    dc_set_mx(0, 0);
    u16 dst = (u16)(0x2100 + a * 16 + 2);
    g_cpu.Y = dst;
    dc_push8(g_cpu.DB);
    dc_push16(dst);
    u16 src = abs16(0x1400 + me);
    g_cpu.X = src;
    dc_push16(src);
    g_cpu.A = 0x17;
    dc_mvn(0x7E, 0xE4);
    g_cpu.X = dc_pull16();
    u16 d2 = dc_pull16();
    g_cpu._flag_C = 0;
    g_cpu.Y = dc_adc16(d2, 0x0200);
    g_cpu.A = 0x17;
    dc_mvn(0x7E, 0xE4);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
    dc_set_mx(1, 1);
    g_cpu._flag_C = 1;
}
static void take_pal_slot(u8 slot)
{
    u8 me = dp8(0x6D);
    g_cpu.X = slot;
    u8 owner = abs8(0x0B80 + slot);
    dp8_set(0xC3, slot);
    if (owner & 0x80) { pal_free_links(me); pal_finish(slot, me); return; }
    if (!pal_join(owner, me)) { g_cpu._flag_C = 0; return; }
    pal_finish(owner, me);
}
DC_ENTRY(C07199, RTS) { take_pal_slot(5); }
DC_ENTRY(C07212, RTS) { take_pal_slot(6); }
DC_ENTRY(C07217, RTS) { take_pal_slot(7); }

/* C0:041A  Save the three party members' places for the menu: tile
 * position relative to the camera (1D0A/2, 1D0E/2), sub-tile 0A00/0A80 & F
 * and 0F01 into 7E:29B0 + 5*slot, and set their 0C00/0C01 to 20 | (2989 &
 * 10). Empty slots (dp 97/99/9B bit 7) are skipped. */
DC_ENTRY(C0041A, RTS)
{
    u8 cx = abs8(0x1D0A) >> 1, cy = abs8(0x1D0E) >> 1;
    dp8_set(0xDB, cx);
    dp8_set(0xDD, cy);
    for (int k = 0; k < 3; k++) {
        u8 m = dp8(0x97 + 2 * k);
        g_cpu.A = (u16)((g_cpu.D << 8) | m);
        dc_set_flags_nz8(m);
        if (m & 0x80) continue;
        u16 x = g_cpu.x_flag ? m : g_cpu.A;
        g_cpu.X = x;
        u32 out = 0x29B0 + 5 * k;
        g_cpu._flag_C = 1;
        ram8_set(out + 0, dc_sbc8(abs8(0x1801 + x), cx));
        ram8_set(out + 2, abs8(0x0A00 + x) & 0x0F);
        g_cpu._flag_C = 1;
        ram8_set(out + 1, dc_sbc8(abs8(0x1881 + x), cy));
        ram8_set(out + 3, abs8(0x0A80 + x) & 0x0F);
        ram8_set(out + 4, abs8(0x0F01 + x));
        u8 v = (u8)((ram8(0x2989) & 0x10) | 0x20);
        dc_set_a8(v);
        dc_set_flags_nz8(v);
        abs8_set(0x0C00 + x, v);
        abs8_set(0x0C01 + x, v);
    }
}

/* C0:721C / 72B4  Give the current object a sprite palette slot among 0..4
 * / 1..3: join a slot whose owner has the same palette, else take the first
 * free one (as C0:7199, but a joined slot keeps its number); C clear when
 * all are taken by other palettes. */
static void pal_search(u8 lo, u8 hi)
{
    u8 me = dp8(0x6D);
    for (u8 s = lo; s < hi; s++) {
        g_cpu.X = s;
        u8 owner = abs8(0x0B80 + s);
        dc_set_a8(owner);
        if (owner & 0x80) continue;
        dp8_set(0xC3, s);
        if (pal_join(owner, me)) { pal_finish(s, me); return; }
        dc_set_mx(1, 1);
    }
    for (u8 s = lo; s < hi; s++) {
        g_cpu.X = s;
        u8 owner = abs8(0x0B80 + s);
        dc_set_a8(owner);
        if (owner & 0x80) {
            dp8_set(0xC3, s);
            pal_free_links(me);
            pal_finish(s, me);
            return;
        }
    }
    g_cpu.X = hi;
    dc_cmp8(hi, hi);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C0721C, RTS) { pal_search(0, 5); }
DC_ENTRY(C072B4, RTS) { pal_search(1, 4); }

/* C0:31B1  Push object dp EB one tile away from the leader (in the
 * leader's facing direction) when it is pushable (1B01 bit 2), has a call
 * priority (1C00, saved through the multiplier) and the target tile is
 * free (7E:7040 bit 7 clear): 7F:3781 = target tile, the object's script
 * position goes to the priority slot and it jumps to 177F; dp 54 bit 1
 * marks the push. Index registers come back 8-bit. */
DC_ENTRY(C031B1, RTS)
{
    u8 f = dp8(0x54);
    dc_set_a8(f);
    if (f & 2) return;
    u8 prio = abs8(0x1C00 + g_cpu.X);
    dc_set_a8(prio);
    dc_set_flags_nz8(prio);
    if (!prio) return;
    abs8_set(0x4202, prio);
    u8 o = dp8(0xEB);
    g_cpu.Y = o;
    u8 fl = abs8(0x1B01 + o);
    dc_set_a8(fl);
    g_cpu._flag_Z = !(fl & 2);
    if (!(fl & 2)) return;
    u8 l = dp8(0x97);
    g_cpu.X = l;
    u8 dir = abs8(0x1600 + l);
    u8 tx = abs8(0x1801 + o), ty = abs8(0x1881 + o);
    if (dir == 0) { if (!ty) { dc_set_a8(0); dc_set_flags_nz8(0); return; } ty--; }
    else if (dir == 1) ty++;
    else if (dir == 2) { if (!tx) { g_cpu.A = (u16)(ty << 8); dc_set_flags_nz8(0); return; } tx--; }
    else tx++;
    u16 t = (u16)(ty << 8 | tx);
    g_cpu.A = t;
    dc_set_mx(1, 0);
    g_cpu.X = t;
    u8 c = ram8(0x7040 + t);
    dc_set_a8(c);
    dc_set_flags_nz8(c);
    if (c & 0x80) { dc_set_mx(1, 1); return; }
    dc_set_mx(0, 0);
    g_cpu.A = t;
    g_cpu.X = 0x1780;
    ram16_set(0x13781, t);
    abs16_set(0x4203, 0x0080);
    g_cpu._flag_C = 0;
    u16 eb = dp16(0xEB);
    u16 slot = dc_adc16(abs16(0x4216), eb);
    dp16_set(0xD9, slot);
    ram16_set(0x10580 + slot, abs16(0x1180 + eb));
    g_cpu.A = 0x177F;
    abs16_set(0x1180 + eb, 0x177F);
    dc_set_mx(1, 1);
    u8 x = (u8)eb;
    g_cpu.X = x;
    dc_set_a8(0);
    abs8_set(0x1C00 + x, 0);
    abs8_set(0x1A80 + x, 0);
    abs8_set(0x1A01 + x, 0);
    abs8_set(0x1001 + x, 0);
    dc_set_a8(2);
    g_cpu._flag_Z = !(dp8(0x54) & 2);
    dp8_set(0x54, dp8(0x54) | 2);
}

/* C0:7170  Give the current object a sprite palette by its kind (1100):
 * slot 5 / 6 / 7 for kinds 0 / 1 / 2 (party members), a slot in 1..3 for
 * kinds 5 and 6, in 0..4 otherwise (table C0:7181). Nothing while dp 54 bit
 * 7 is set or for removed objects. */
DC_ENTRY(C07170, RTS)
{
    u8 f = dp8(0x54);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (f & 0x80) return;
    u8 o = dp8(0x6D);
    g_cpu.X = o;
    u8 k = abs8(0x1100 + o);
    dc_set_a8(k);
    dc_set_flags_nz8(k);
    if (k & 0x80) return;
    g_cpu._flag_C = k >> 7;
    k = (u8)(k << 1);
    dc_set_a8(k);
    g_cpu.X = k;
    dc_jsr(0xC00000 | bus16(0xC07181 + k), 0xC0717D);
}

/* C0:3154  Make object dp EB (free, present, enabled, priority >= 3) run
 * its function 2 at priority 2 (saving its position to the priority slot,
 * as C0:5AC5 does for function 1), then try to push it (C0:31B1). */
DC_ENTRY(C03154, RTS)
{
    u8 o = dp8(0xEB);
    g_cpu.X = o;
    u8 v = abs8(OBJ_CALLS_BLOCKED(o)); dc_set_a8(v); dc_set_flags_nz8(v);
    if (v) return;
    v = abs8(OBJ_REMOVED(o)); dc_set_a8(v); dc_set_flags_nz8(v);
    if (v & 0x80) return;
    v = abs8(OBJ_EV_DISABLED(o)); dc_set_a8(v); dc_set_flags_nz8(v);
    if (v & 0x80) return;
    u8 prio = abs8(OBJ_PRIORITY(o));
    dc_set_a8(prio);
    dc_cmp8(prio, 3);
    if (prio >= 3) {
        abs8_set(0x4202, prio);
        abs8_set(0x4203, 0x80);
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        u16 eb = dp16(0xEB);
        u16 slot = dc_adc16(abs16(0x4216), eb);
        dp16_set(0xD9, slot);
        g_cpu.X = slot;
        ram16_set(0x10580 + slot, abs16(OBJ_SCRIPT_POS(o)));
        g_cpu._flag_C = 0;
        u16 idx = dc_adc16((u16)(dp16(0xEB) << 4), 4);
        u16 start = ram16(0x12001 + idx);
        g_cpu.A = start;
        eb = dp16(0xEB);
        abs16_set(OBJ_SCRIPT_POS(eb), start);
        dc_set_mx(1, 1);
        u8 x = (u8)eb;
        g_cpu.X = x;
        dc_set_a8(2);
        dc_set_flags_nz8(2);
        abs8_set(OBJ_PRIORITY(x), 2);
        abs8_set(OBJ_1A80(x), 0);
        abs8_set(OBJ_1A01(x), 0);
        abs8_set(0x1001 + x, 0);
    }
    dc_jsr(0xC031B1, 0xC031AD);
}

/* C0:0557  Save NPC Y (0D00 = its menu slot code < C0) for the menu: slot
 * s = code/16 - 4 + (code/8 & 1), dp A3+s = Y, and 12 bytes at 7E:29BF +
 * s*12: tile position relative to dp F0/F1, sub-tile, 0F01 (0 for kinds
 * >= 3 of 1201), 1101, 0, flags (1201 & 3, | 40 for kind 6, | 1A81 bit 7),
 * palette slot (giving it one first when 1A81 bit 7), 1200, 1280, 1281. */
DC_ENTRY(C00557, RTS)
{
    u8 y = (u8)g_cpu.A;
    g_cpu.Y = y;
    u8 code = abs8(0x0D00 + y);
    dc_set_a8(code);
    dc_cmp8(code, 0xC0);
    if (code >= 0xC0) return;
    u8 k = code >> 3;
    dc_push8(k);
    g_cpu._flag_C = 1;
    u8 d9 = dc_sbc8(k >> 1, 4);
    dp8_set(0xD9, d9);
    k = dc_pull8();
    g_cpu._flag_C = 0;
    u8 s = dc_adc8(k & 1, d9);
    abs8_set(0x4202, s);
    g_cpu.X = s;
    b0_8_set((u16)(g_cpu.D + 0xA3 + s), y);
    abs8_set(0x4203, 0x0C);
    g_cpu._flag_C = 1;
    u8 rx = dc_sbc8(abs8(0x1801 + y), dp8(0xF0));
    u8 x = abs8(0x4216);
    g_cpu.X = x;
    ram8_set(0x29BF + x, rx);
    ram8_set(0x29C1 + x, abs8(0x0A00 + y) & 0x0F);
    g_cpu._flag_C = 1;
    ram8_set(0x29C0 + x, dc_sbc8(abs8(0x1881 + y), dp8(0xF1)));
    ram8_set(0x29C2 + x, abs8(0x0A80 + y) & 0x0F);
    ram8_set(0x29C4 + x, abs8(0x1101 + y));
    ram8_set(0x29C5 + x, 0);
    u8 kind = abs8(0x1100 + y);
    dc_cmp8(kind, 6);
    u8 m = abs8(0x1201 + y) & 3;
    dp8_set(0xD9, m);
    u8 e = kind == 6 ? (u8)(m | 0x40) : m;
    dp8_set(0xEE, e);
    ram8_set(0x29C6 + x, (u8)((abs8(0x1A81 + y) & 0x80) | e));
    u8 mm = dp8(0xD9) & 7;
    dc_cmp8(mm, 3);
    ram8_set(0x29C3 + x, mm >= 3 ? 0 : abs8(0x0F01 + y));
    if (abs8(0x1A81 + y) & 0x80) {
        dp8_set(0x6D, y);
        dc_push8(x);
        dc_push8(y);
        dc_jsr(0xC072B4, 0xC005F4);
        g_cpu.Y = y = dc_pull8();
        g_cpu.X = x = dc_pull8();
    }
    u8 p = abs8(0x0F81 + y);
    g_cpu._flag_C = p & 1;
    ram8_set(0x29C7 + x, p >> 1);
    ram8_set(0x29CA + x, abs8(0x1200 + y));
    ram8_set(0x29C9 + x, abs8(0x1281 + y));
    u8 last = abs8(0x1280 + y);
    dc_set_a8(last);
    dc_set_flags_nz8(last);
    ram8_set(0x29C8 + x, last);
}

/* C0:6FBA  Give the current object sprite VRAM: one 16x16 slot of 0B88
 * (searched from 19 down to 1) when 1201 & 3 is 0, else a free pair of
 * slots (0C, 0E ... 16) for a larger sprite. 0D00 = the slot's tile number
 * ((slot & FE) * 16 + (slot & 1) * 8), 1700 = slot * 32. C clear when none
 * is free. */
static void claim_vram(u8 s, u8 me)
{
    dc_set_mx(0, 1);
    u16 hi = (u16)((s & 0xFE) << 4);
    dp16_set(0xD9, hi);
    g_cpu._flag_C = 0;
    u16 tile = dc_adc16((u16)((s & 1) << 3), hi);
    dp8_set(0xD9, s);
    g_cpu.X = me;
    abs16_set(0x0D00 + me, tile);
    dc_set_mx(1, 1);
    u16 v = (u16)(dp8(0xD9) << 5);
    g_cpu._flag_C = (dp8(0xD9) >> 3) & 1;
    g_cpu.A = v;
    dc_set_flags_nz16(v);
    abs16_set(0x1700 + me, v);
    g_cpu._flag_C = 1;
}
DC_ENTRY(C06FBA, RTS)
{
    DC_ASSUME_M(C06FBA, 1);
    u8 me = dp8(0x6D);
    g_cpu.X = me;
    u8 k = abs8(0x1201 + me) & 3;
    dc_set_a8(k);
    if (!k) {
        for (u8 s = 0x19; s; s--) {
            g_cpu.X = s;
            u8 v = abs8(0x0B88 + s);
            dc_set_a8(v);
            if (v & 0x80) { abs8_set(0x0B88 + s, me); dc_set_a8(s); claim_vram(s, me); return; }
        }
        g_cpu.X = 0;
        dc_set_flags_nz8(0);
        g_cpu._flag_C = 0;
        return;
    }
    for (u8 s = 0x0C; s < 0x18; s += 2) {
        g_cpu.X = s;
        u8 v = abs8(0x0B88 + s);
        dc_set_a8(v);
        if (!(v & 0x80)) continue;
        v = abs8(0x0B89 + s);
        dc_set_a8(v);
        if (!(v & 0x80)) continue;
        abs8_set(0x0B88 + s, me);
        abs8_set(0x0B89 + s, me);
        dc_set_a8(s);
        claim_vram(s, me);
        return;
    }
    g_cpu.X = 0x18;
    dc_cmp8(0x18, 0x18);
    g_cpu._flag_C = 0;
}

/* C0:6F9A  Give the current object sprite VRAM by its kind (1100, table
 * C0:6FA7); nothing for removed objects. Works with either accumulator
 * width, as the original does (bit 15 / bit 7 test, 8-bit index). */
DC_ENTRY(C06F9A, RTS)
{
    u8 o = dp8(0x6D);
    g_cpu.X = o;
    u16 k;
    if (g_cpu.m_flag) { u8 v = abs8(0x1100 + o); dc_set_a8(v); dc_set_flags_nz8(v); k = v; if (v & 0x80) return; g_cpu._flag_C = v >> 7; k = (u8)(v << 1); dc_set_a8((u8)k); }
    else { u16 v = abs16(0x1100 + o); g_cpu.A = v; dc_set_flags_nz16(v); if (v & 0x8000) return; g_cpu._flag_C = v >> 15; k = (u16)(v << 1); g_cpu.A = k; }
    u8 x = (u8)k;
    g_cpu.X = x;
    dc_jsr(0xC00000 | bus16(0xC06FA7 + x), 0xC06FA3);
}

/* C0:04ED  Save the NPCs listed in 019D.. (up to 6, ending at 80) for the
 * menu with C0:0557: camera tile dp F0/F1, the eight 12-byte records'
 * words at 7E:29C4 + 12k and dp A3..AA reset to FFFF first. */
DC_ENTRY(C004ED, RTS)
{
    dc_set_mx(1, 1);
    dp8_set(0xF0, abs8(0x1D0A) >> 1);
    dp8_set(0xF1, abs8(0x1D0E) >> 1);
    dc_set_mx(0, 1);
    g_cpu.A = 0xFFFF;
    for (int k = 0; k < 8; k++) ram16_set(0x29C4 + 12 * k, 0xFFFF);
    for (int k = 0; k < 4; k++) dp16_set(0xA3 + 2 * k, 0xFFFF);
    dc_set_mx(1, 1);
    for (int k = 0; k < 6; k++) {
        u8 v = dp8(0x9D + k);
        dc_set_a8(v);
        dc_set_flags_nz8(v);
        if (v & 0x80) break;
        dc_jsr(0xC00557, 0xC0052E + 7 * k);
    }
    dc_set_mx(1, 0);
}

/* C0:038F  Snapshot for the menu: map and palette (C0:039B), party (041A),
 * NPCs (04ED), then the screen settings into 7E:2C56..2C7B: colour math
 * pairs (1D7D|1D84 ...), 0BD7/0BD8, 0BDF, brightness dp 21 and the six
 * scroll words 1D87..1D91 (twice). */
DC_ENTRY(C0038F, RTS)
{
    dc_jsr(0xC0039B, 0xC0038F);
    dc_jsr(0xC0041A, 0xC00392);
    dc_jsr(0xC004ED, 0xC00395);
    ram8_set(0x2C56, abs8(0x1D7D) | abs8(0x1D84));
    ram8_set(0x2C57, abs8(0x1D7F) | abs8(0x1D85));
    ram8_set(0x2C58, abs8(0x1D81) | abs8(0x1D86));
    ram8_set(0x2C59, 0);
    ram8_set(0x2C66, abs8(0x0BD7));
    ram8_set(0x2C67, abs8(0x0BD8));
    ram8_set(0x2C68, 0);
    ram8_set(0x2C69, 0);
    ram8_set(0x2C6A, 2);
    ram8_set(0x2C6B, abs8(0x0BDF));
    ram8_set(0x2C6C, dp8(0x21));
    ram8_set(0x2C6D, 0);
    ram8_set(0x2C6E, 9);
    ram8_set(0x2C6F, 0);
    dc_set_mx(0, 0);
    u16 w = 0;
    for (int k = 0; k < 6; k++) {
        w = abs16(0x1D87 + 2 * k);
        ram16_set(0x2C5A + 2 * k, w);
        ram16_set(0x2C70 + 2 * k, w);
    }
    g_cpu.A = w;
    dc_set_flags_nz16(w);
    dc_set_mx(1, 0);
}

/* C0:9923  Find an object near the point (dp 66, dp 68): scanning from
 * 7F:2000 down to 1, the first on-screen (0F00), present, non-party object
 * less than 0xE0 pixels away on both axes goes in 01EB; C = its 1B01 bit 0.
 * None: 01EB = 80, C clear. The distance subtractions run without SEC, as
 * in the original: the first uses the carry of the last party compare,
 * the second the carry of the first range check (always clear there). */
DC_ENTRY(C09923, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    abs16_set(0x01EB, 0x0080);
    dc_set_mx(1, 1);
    u8 n = ram8(0x12000);
    g_cpu._flag_C = n >> 7;
    u8 x = (u8)(n << 1);
    for (;;) {
        g_cpu.X = x;
        u8 v = abs8(0x0F00 + x);
        dc_set_a8(v);
        if (v) {
            u8 k = abs8(0x1100 + x);
            dc_set_a8(k);
            dc_set_flags_nz8(k);
            bool skip = k & 0x80;
            for (int p = 0; p < 3 && !skip; p++) { u8 m = abs8(0x0197 + 2 * p); dc_cmp8(x, m); skip = x == m; }
            if (!skip) {
                bool sec = abs8(0x1B01 + x) & 1;
                abs8_set(0x01EB, x);
                dc_set_mx(0, 1);
                bool near = true;
                u16 off[2] = { 0x1880, 0x1800 };
                u8 pt[2] = { 0x68, 0x66 };
                for (int a = 0; a < 2 && near; a++) {
                    u16 d = dc_sbc16(abs16(off[a] + x), dp16(pt[a]));
                    if (d & 0x8000) { d = (u16)(~d + 1); g_cpu.A = d; }
                    g_cpu.A = d;
                    dc_cmp16(d, 0xE0);
                    near = d < 0xE0;
                }
                dc_set_mx(1, 1);
                if (near) { dc_set_mx(1, 0); g_cpu._flag_C = sec; return; }
            }
        }
        x = (u8)(x - 2);
        g_cpu.X = x;
        if (!x) break;
    }
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
    abs8_set(0x01EB, 0x80);
    dc_set_mx(1, 0);
    g_cpu._flag_C = 0;
}

/* C0:B788  Place object X's sprite (16-bit X, DB swapped to 7F): the four
 * OAM X bytes at 7F:4BC0 + 1700[X] + 0/8/10/18 = the frame's offsets
 * (4BC2..) + screen X 0A00, their bit 8 packed with the size bits AA into
 * 7F:4F00 + dp 6D; the Y bytes 4BC1.. = offsets (4BC4..) + screen Y 0A80
 * (dp C5, bit 8 in C6), clipped to E0 (off screen) on overflow: five rows
 * when Y >= 100, four otherwise. C0:B8CA first copies the frame's four
 * sprites from 7F:4800 + 1700[X] (attribute words too); C0:BCDC does the
 * same for an 8-sprite frame of object dp 6D. */
static u8 sprite_y(u16 x, u16 k, int how, bool copy)
{
    if (copy) abs8_set(0x4BC4 + x + 8 * k, abs8(0x4804 + x + 8 * k));
    u8 c5 = dp8(0xC5);
    g_cpu._flag_C = 0;
    u8 v = dc_adc8(abs8(0x4BC4 + x + 8 * k), c5);
    bool clip;
    if (how == 0) {          /* Y >= 100: only a wrapped sum below E0 stays */
        clip = !g_cpu._flag_C;
        if (!clip) { dc_cmp8(v, 0xE0); clip = !g_cpu._flag_C; }
    } else if (how == 1) {   /* negative Y: a wrapped sum >= E0 is hidden */
        clip = false;
        if (g_cpu._flag_C) { dc_cmp8(v, 0xE0); clip = g_cpu._flag_C; }
    } else if (how == 2) {   /* positive Y: a sum 80..DF is hidden */
        clip = false;
        if (v & 0x80) { dc_cmp8(v, 0xE0); clip = !g_cpu._flag_C; }
    } else if (how == 3) {   /* negative Y (8-sprite version): any sum >= E0 */
        dc_cmp8(v, 0xE0);
        clip = g_cpu._flag_C;
    } else if (how == 5) {   /* negative Y (12-sprite version): no clipping */
        clip = false;
    } else if (how == 6) {   /* Y >= 100 (12-sprite version): carry or 80..FF */
        clip = g_cpu._flag_C || (v & 0x80);
    } else {                 /* positive Y, signed offsets: 80..DF hidden unless
                              * a positive offset did not carry */
        u8 off = abs8(0x4BC4 + x + 8 * k);
        clip = false;
        if (!(!(off & 0x80) && !g_cpu._flag_C) && (v & 0x80)) { dc_cmp8(v, 0xE0); clip = !g_cpu._flag_C; }
    }
    if (clip) { v = 0xE0; dc_set_flags_nz8(v); }
    dc_set_a8(v);
    abs8_set(0x4BC1 + x + 8 * k, v);
    return v;
}
/* n = 4 sprites (C0:B788 / B8CA, object X), 8 (C0:BCDC / BA65) or 12
 * (C0:BFF2, C2BF with copy) for object dp 6D, groups of four with their size bits in
 * 4F00 / 4F01 / 4B40 (the third one as the original writes it). The Y
 * clipping rules differ per routine (c6_how / neg_how / pos_how). */
static void sprite_place(bool copy, int n, int c6_how, int neg_how, int pos_how)
{
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7F);
    dc_push8(0x7F);
    g_cpu.DB = dc_pull8();
    dc_set_mx(0, 0);
    if (n > 4) g_cpu.X = dp16(0x6D);
    u16 o = g_cpu.X;
    dp16_set(0xC5, ram16(0x0A80 + o) & 0x01FF);
    u16 sx = ram16(0x0A00 + o);
    dp16_set(0xC3, sx);
    dp16_set(0xE5, 0);
    u16 base = ram16(0x1700 + o);
    dp16_set(0xD9, base);
    u16 x;
    u8 bits;
    u16 s;
    for (int g = 0; g < n / 4; g++) {
        u16 end = base;
        if (g) {
            dp8_set(0xE5, 0);
            dc_set_mx(0, 0);
            g_cpu._flag_C = 0;
            end = dc_adc16(dp16(0xD9), (u16)(0x20 * g));
            dp16_set(0xE7, end);
        }
        x = (u16)(end + 0x18);
        for (;;) {
            g_cpu.X = x;
            g_cpu._flag_C = 0;
            u16 w = abs16((copy ? 0x4802 : 0x4BC2) + x);
            if (copy) abs16_set(0x4BC2 + x, w);
            s = dc_adc16(w, dp16(0xC3));
            abs8_set(0x4BC0 + x, (u8)s);
            dp8_set(0xE6, (s >> 8) & 1);
            bits = (u8)(dp8(0xE5) << 2 | dp8(0xE6));
            if (x == end) break;
            dp8_set(0xE5, bits);
            x -= 8;
        }
        dc_set_mx(1, 0);
        bits |= 0xAA;
        g_cpu.A = (u16)((s & 0xFF) << 8 | bits);
        g_cpu.X = dp16(0x6D);
        abs8_set((g < 2 ? 0x4F00 + g : 0x4B40) + g_cpu.X, bits);   /* sic: 4B40 for the third group */
    }
    x = dp16(0xD9);
    g_cpu.X = x;
    if (dp8(0xC6)) {
        int rows = n == 4 ? 5 : n;
        for (int k = 0; k < rows; k++) sprite_y(x, k, c6_how, copy && k < n);
    } else {
        int how = dp8(0xC5) & 0x80 ? neg_how : pos_how;
        for (int k = 0; k < n; k++) sprite_y(x, k, how, copy);
    }
    if (copy) {
        dc_set_mx(0, 0);
        for (int k = 0; k < n; k++) {
            g_cpu.A = abs16(0x4806 + x + 8 * k);
            abs16_set(0x4BC6 + x + 8 * k, g_cpu.A);
        }
        dc_set_mx(1, 0);
    }
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}
DC_ENTRY(C0B788, RTS) { sprite_place(false, 4, 0, 1, 2); }
DC_ENTRY(C0B8CA, RTS) { sprite_place(true, 4, 0, 1, 2); }
DC_ENTRY(C0BCDC, RTS) { sprite_place(true, 8, 0, 3, 2); }
DC_ENTRY(C0BA65, RTS) { sprite_place(false, 8, 0, 3, 4); }
DC_ENTRY(C0BFF2, RTS) { sprite_place(false, 12, 6, 5, 2); }
DC_ENTRY(C0C2BF, RTS) { sprite_place(true, 12, 6, 5, 2); }

/* C0:C73A (reached from C0:B701 only)  Place a 24-sprite frame of object
 * dp 6D: six groups of four X bytes moved by C0:C6E7, their size bytes to
 * 4F00 / 4F01 / 4B40 / 4B41 / 4F80 / 4F81 + object (as the original lays
 * them out), then the 24 Y bytes (clipped as C0:BFF2 does). */
static void sprite_place24(void)
{
    static const u16 dst[6] = { 0x4F00, 0x4F01, 0x4B40, 0x4B41, 0x4F80, 0x4F81 };
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7F);
    dc_push8(0x7F);
    g_cpu.DB = dc_pull8();
    dc_set_mx(0, 0);
    u16 o = dp16(0x6D);
    g_cpu.X = o;
    dp16_set(0xC5, ram16(0x0A80 + o) & 0x01FF);
    dp16_set(0xC3, ram16(0x0A00 + o));
    g_cpu.A = ram16(0x1700 + o);
    g_cpu.X = g_cpu.A;
    for (int g = 0; g < 6; g++) {
        if (g) {
            dc_set_mx(0, 0);
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(g_cpu.X, 0x0020);
            g_cpu.X = g_cpu.A;
        }
        dc_jsr(0xC0C6E7, (u32)(0xC0C757 + 0x14 * g));
        dp16_set(0xD9, g_cpu.X);
        g_cpu.X = dp16(0x6D);
        if (g_cpu.m_flag) abs8_set(dst[g] + g_cpu.X, (u8)g_cpu.A);
        else abs16_set(dst[g] + g_cpu.X, g_cpu.A);
        if (g < 5) g_cpu.X = dp16(0xD9);
    }
    g_cpu.X = dp16(0x6D);
    dc_set_mx(0, 0);
    u16 x = ram16(0x1700 + g_cpu.X);
    g_cpu.A = x;
    g_cpu.X = x;
    dc_set_mx(1, 0);
    if (dp8(0xC6)) {
        for (int k = 0; k < 24; k++) sprite_y(x, k, 6, false);
    } else {
        int how = dp8(0xC5) & 0x80 ? 5 : 2;
        dp8_set(0xC9, 0x18);
        u8 n;
        do {
            sprite_y(x, 0, how, false);
            dc_set_mx(0, 0);
            g_cpu._flag_C = 0;
            x = dc_adc16(x, 0x0008);
            g_cpu.A = x;
            g_cpu.X = x;
            dc_set_mx(1, 0);
            n = (u8)(dp8(0xC9) - 1);
            dp8_set(0xC9, n);
        } while (n);
    }
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}
/* C0:B701  Place object dp 6D's sprites by its size (1201 bits 0..1: 4 /
 * 8 / 12 / 24 sprites) when its frame changed (1B00 != 0): a new frame
 * (1B00 = 1..7F, or with bit 7 a frame number >= 1 / 2 / 3) is copied from
 * the frame buffer (C0:B8CA / BCDC / C2BF), its VRAM slot is released
 * (C0:E9E2 / E9FF / EA1F) and 1B00 = 80; otherwise only the position is
 * refreshed (C0:B788 / BA65 / BFF2). C set when nothing was done. */
DC_ENTRY(C0B701, RTS)
{
    static const u32 place[3] = { 0xC0B788, 0xC0BA65, 0xC0BFF2 };
    static const u32 load[3] = { 0xC0B8CA, 0xC0BCDC, 0xC0C2BF };
    static const u32 freev[3] = { 0xC0E9E2, 0xC0E9FF, 0xC0EA1F };
    static const u16 s_place[3] = { 0xB732, 0xB759, 0xB780 };
    static const u16 s_load[3] = { 0xB71F, 0xB744, 0xB76B };
    u16 o = dp16(0x6D);
    g_cpu.X = o;
    u8 t = abs8(0x1201 + o) & 3;
    dc_set_a8(t);
    dc_set_flags_nz8(t);
    if (t) { dc_cmp8(t, 1); if (t != 1) dc_cmp8(t, 2); }
    if (t == 3) { sprite_place24(); return; }
    u8 f = abs8(0x1B00 + o);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (!f) { g_cpu._flag_C = 1; return; }
    bool fresh;
    if (!(f & 0x80)) {
        if (t) {
            dc_cmp8(f, (u8)(t + 1));
            if (f < t + 1) { g_cpu._flag_C = 1; return; }
        }
        fresh = true;
    } else {
        u8 m = f & 0x7F;
        dc_set_a8(m);
        dc_set_flags_nz8(m);
        if (t) dc_cmp8(m, (u8)(t + 1));
        fresh = t ? m >= t + 1 : m != 0;
    }
    if (fresh) {
        dc_jsr(load[t], 0xC00000 | s_load[t]);
        dc_jsr(freev[t], 0xC00000 | (u16)(s_load[t] + 3));
        u16 x = g_cpu.x_flag ? dp8(0x6D) : dp16(0x6D);
        g_cpu.X = x;
        dc_set_a8(0x80);
        dc_set_flags_nz8(0x80);
        abs8_set(0x1B00 + x, 0x80);
    } else {
        dc_jsr(place[t], 0xC00000 | s_place[t]);
    }
    g_cpu._flag_C = 0;
}

/* C0:B309  Place object dp 6D's sprites (C0:B701) and, when that did
 * something, queue them for OAM by priority (0F80 bits 2..3: 0 / 4 / 8 ->
 * the lists at 0181 / 0185 / 0189 with their OAM-buffer addresses 01DB /
 * 01DD / 01DF): the group size bytes (4F00.. as C0:B701 lays them out) are
 * appended to the list, then 4 bytes per sprite (X, Y, tile, attributes)
 * go through the WRAM port at the list's buffer address, which advances by
 * 10 per group of four sprites. */
DC_ENTRY(C0B309, RTS)
{
    static const u16 size_at[6] = { 0x4F00, 0x4F01, 0x4B40, 0x4B41, 0x4F80, 0x4F81 };
    static const u8 groups[4] = { 1, 2, 3, 6 };
    dc_jsr(0xC0B701, 0xC0B309);
    if (g_cpu._flag_C) return;
    u16 o = dp16(0x6D);
    g_cpu.X = o;
    u8 t = abs8(0x1201 + o) & 3;
    dc_set_a8(t);
    int g = groups[t];
    dc_push16(g_cpu.D);
    dc_set_mx(0, 0);
    g_cpu.A = 0x2100;
    g_cpu.D = 0x2100;
    dc_set_mx(1, 0);
    u8 pr = abs8(0x0F80 + o) & 0x0C;
    int l = !pr ? 0 : (pr & 4) ? 1 : 2;
    u16 list = (u16)(0x0181 + 4 * l), addr = (u16)(0x01DB + 2 * l);
    for (int k = g - 1; k >= 1; k--) dc_push8(ram8(0x10000 + size_at[k] + o));
    u8 v = ram8(0x10000 + size_at[0] + o);
    u16 x = abs16(list);
    for (int k = 0; k < g; k++) {
        if (k) v = dc_pull8();
        dc_set_a8(v);
        abs8_set(x, v);
        x++;
    }
    abs16_set(list, x);
    g_cpu.X = abs16(addr);
    dp16_set(0x81, g_cpu.X);
    g_cpu.X = abs16(0x016D);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    abs16_set(addr, dc_adc16(abs16(addr), (u16)(0x10 * g)));
    x = abs16(0x1700 + g_cpu.X);
    g_cpu.A = x;
    dc_set_mx(1, 0);
    abs8_set(0x01C9, (u8)(4 * g));
    u8 n;
    do {
        dp8_set(0x80, bus8(0x7F4BC0 + x));
        dp8_set(0x80, bus8(0x7F4BC1 + x));
        dp8_set(0x80, bus8(0x7F4BC6 + x));
        dp8_set(0x80, bus8(0x7F4BC7 + x));
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        x = dc_adc16(x, 8);
        g_cpu.A = x;
        dc_set_mx(1, 0);
        n = (u8)(abs8(0x01C9) - 1);
        abs8_set(0x01C9, n);
    } while (n);
    g_cpu.X = x;
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* ---- C0:CAD9  Load the current object's animation frame into its sprite
 * slot. Objects come in three sizes (1201 & 3): small ones fit one frame
 * per call; medium and large ones spread the tile upload over two or three
 * frames (1B00 & 7F counts the steps). A frame is a list of 2-byte tile
 * words at [1300:1380 + 0F01 * K] followed by 2-byte sprite pieces (x, y);
 * tiles go to WRAM 1:D0 (one 32-byte tile each, C0:E534 translated, C0:E687
 * copied), the VRAM transfer is queued at 0940..09A0 and the pieces become
 * the object's sprite list at 7F:4802 + 1700. */

/* CC04: a new frame A (unless it is the one already loaded, 0F01) claims a
 * sprite slot with the allocator; false when there is nothing to do. */
static bool frame_claim(u8 a, u32 alloc, u32 site)
{
    dc_cmp8(a, abs8(0x0F01 + g_cpu.X));
    if (g_cpu._flag_Z) { g_cpu._flag_C = 0; return false; }
    dp8_set(0xEE, a);
    dc_jsr(alloc, site);
    if (!g_cpu._flag_C) { g_cpu._flag_C = 0; return false; }
    a = dp8(0xEE);
    dc_set_a8(a);
    abs8_set(0x0F01 + g_cpu.X, a);
    dc_set_mx(0, 1);
    u16 x = dp8(0x6D);
    g_cpu.X = x;
    u16 d0 = abs16(0x0D80 + x);
    g_cpu.A = d0;
    dp16_set(0xD0, d0);
    dc_set_mx(1, 1);
    return true;
}

/* A later step: D0 = the slot's address + off. */
static void frame_resume(u16 off)
{
    u16 x = dp8(0x6D);
    g_cpu.X = x;
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    u16 d0 = dc_adc16(abs16(0x0D80 + x), off);
    g_cpu.A = d0;
    dp16_set(0xD0, d0);
    dc_set_mx(1, 1);
}

/* Point [D3] at frame 0F01 (k bytes each); ends with 16-bit A. */
static void frame_locate(u8 k)
{
    u16 x = g_cpu.X;
    u8 f = abs8(0x0F01 + x);
    dc_set_a8(f);
    abs8_set(0x4202, f);
    abs8_set(0x4203, k);
    dp8_set(0xD5, abs8(0x1300 + x));
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    u16 p = dc_adc16(abs16(0x4216), abs16(0x1380 + x));
    dp16_set(0xD3, p);
    g_cpu.A = p;
}

/* ... and WRAM at 1:D0 for its tiles; ends 16-bit. */
static void frame_src(u8 k)
{
    frame_locate(k);
    dc_set_mx(1, 1);
    dc_set_a8(1);
    abs8_set(0x2183, 1);
    dc_set_mx(0, 0);
    g_cpu.A = dp16(0xD0);
    abs16_set(0x2181, g_cpu.A);
}

static u32 frame_ptr(void) { return (u32)dp8(0xD5) << 16 | dp16(0xD3); }

/* Upload n tiles from frame word y on (sites: the E687 call; E534's is +B). */
static void frame_tiles(u16 n, u16 y, u32 site)
{
    dp16_set(0xC9, n);
    g_cpu.Y = y;
    for (bool first = true;; first = false) {
        if (!first) {
            g_cpu._flag_C = 0;
            u16 d0 = dc_adc16(dp16(0xD0), 0x20);
            dp16_set(0xD0, d0);
        }
        u32 p = (frame_ptr() + g_cpu.Y) & 0xFFFFFF;
        u16 w = (u16)(bus8(p) | bus8((p + 1) & 0xFFFFFF) << 8);
        g_cpu.A = w;
        if (w & 0x4000) dc_jsr(0xC0E534, site + 0xB);
        else dc_jsr(0xC0E687, site);
        g_cpu.Y = (u16)(g_cpu.Y + 2);
        u16 c = (u16)(dp16(0xC9) - 1);
        dp16_set(0xC9, c);
        dc_set_flags_nz16(c);
        if (!c) break;
    }
}

/* A second run of tiles in the same step, to the slot's address + off. */
static void frame_tiles_at(u16 off, u16 n, u16 y, u32 site)
{
    u16 x = dp16(0x6D);
    g_cpu.X = x;
    g_cpu._flag_C = 0;
    u16 d0 = dc_adc16(abs16(0x0D80 + x), off);
    g_cpu.A = d0;
    dp16_set(0xD0, d0);
    abs16_set(0x2181, d0);
    frame_tiles(n, y, site);
}

/* More steps follow: count this one, C set. */
static void frame_step(void)
{
    dc_set_mx(1, 1);
    u16 x = dp8(0x6D);
    g_cpu.X = x;
    u8 s = (u8)(abs8(0x1B00 + x) + 1);
    abs8_set(0x1B00 + x, s);
    dc_set_flags_nz8(s);
    g_cpu._flag_C = 1;
}

/* Queue the VRAM transfer: two rows (0D00 & 1FF) * 16 and +qa from WRAM 0D80
 * and +qb, lengths qc and qd. */
static void frame_queue(u16 qa, u16 qb, u16 qc, u16 qd)
{
    dc_set_mx(0, 1);
    u16 me = dp8(0x6D);
    u16 t = (u16)((abs16(0x0D00 + me) & 0x1FF) << 4);
    u16 q = dp8(0x79);
    abs16_set(0x0950 + q, t);
    g_cpu._flag_C = 0;
    abs16_set(0x0970 + q, dc_adc16(t, qa));
    u16 v = abs16(0x0D80 + me);
    abs16_set(0x0940 + q, v);
    g_cpu._flag_C = 0;
    abs16_set(0x0960 + q, dc_adc16(v, qb));
    abs16_set(0x0980 + q, qc);
    abs16_set(0x0990 + q, qd);
    abs16_set(0x09A0 + q, (u16)(abs16(0x09A0 + q) + 1));
    q = (u8)(q + 2);
    abs16_set(0x09A0 + q, 0);
    dp8_set(0x79, (u8)q);
    g_cpu.X = q;
}

/* Write n sprite pieces (from frame byte y0) to the object's sprite list
 * (list + 1700). Tiles count up by 2 from 0D00 (one jump of 10 before piece
 * jump_at); attributes are 0F81 | 0D01 | 0C00 for the first four pieces
 * and 0C01 after, or (by_y) 0C01 for pieces whose y is E8 or more. The step
 * counter advances incs times (incs < 0: set to 80, frame complete); C
 * clear. */
static void frame_sprites(u8 n, u16 y0, u8 jump_at, bool by_y, int incs, bool bank0, u32 list)
{
    u16 me = dp8(0x6D);
    u16 base = bank0 ? bus16(0x001700 + me) : abs16(0x1700 + me);
    g_cpu.A = base;
    dc_set_mx(1, 0);
    u16 x = base;
    g_cpu.X = x;
    u32 src = frame_ptr();
    for (u8 k = 0; k < n; k++) {
        u32 o = list + x + 8u * k;
        u8 b = bus8((src + y0 + 2u * k) & 0xFFFFFF);
        bus8_set(o + 2, b);
        bus8_set(o + 3, (b & 0x80) ? 0xFF : 0x00);
        bus8_set(o + 4, bus8((src + y0 + 2u * k + 1) & 0xFFFFFF));
    }
    u16 y = dp16(0x6D);
    g_cpu.Y = y;
    u8 a = abs8(0x0D00 + y);
    for (u8 k = 0; k < n; k++) {
        if (k) a = (u8)(a + 2);
        if (k == jump_at) { g_cpu._flag_C = 0; a = dc_adc8(a, 0x10); }
        bus8_set(list + 6 + x + 8u * k, a);
    }
    u8 d9 = (u8)(abs8(0x0F81 + y) | abs8(0x0D01 + y));
    dp8_set(0xD9, d9);
    for (u8 k = 0; k < n; k++) {
        bool hi = by_y ? bus8(list + 4 + x + 8u * k) >= 0xE8 : k >= 4;
        a = (u8)(d9 | abs8((hi ? 0x0C01 : 0x0C00) + y));
        bus8_set(list + 7 + x + 8u * k, a);
    }
    dc_set_a8(a);
    x = dp16(0x6D);
    g_cpu.X = x;
    if (incs < 0) {
        dc_set_a8(0x80);
        dc_set_flags_nz8(0x80);
        abs8_set(0x1B00 + x, 0x80);
    }
    for (int i = 0; i < incs; i++) {
        u8 s = (u8)(abs8(0x1B00 + x) + 1);
        abs8_set(0x1B00 + x, s);
        dc_set_flags_nz8(s);
    }
    dc_set_mx(1, 1);
    g_cpu._flag_C = 0;
}

DC_ENTRY(C0CAD9, RTS)
{
    u16 x = dp8(0x6D);
    g_cpu.X = x;
    u8 v = abs8(0x1100 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    if (v & 0x80) { g_cpu._flag_C = 0; return; }
    v = abs8(0x1A81 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    if (v == 0 || (v & 0x80)) { g_cpu._flag_C = 0; return; }
    v = abs8(0x0F00 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    if (!v) { g_cpu._flag_C = 0; return; }
    u8 size = abs8(0x1201 + x) & 3;
    dc_set_a8(size);
    if (size == 3) { dc_cmp8(3, 2); g_cpu._flag_C = 0; return; }

    /* common prologue: tile source bank CF / CD, WRAM bank D2, frames in E4 */
    dp8_set(0xCF, abs8(0x1200 + x));
    dp8_set(0xD2, 0x7F);
    g_cpu.A = abs16(0x1280 + x);
    dp16_set(0xCD, g_cpu.A);
    dp8_set(0xD8, 0xE4);
    u8 lock = abs8(0x1780 + x);
    dc_set_a8(lock);
    dc_cmp8(lock, 3);

    if (size == 0) {                              /* small: 16 tiles, 4 pieces */
        u8 a;
        if (lock == 3) { a = abs8(0x1301 + x); dc_set_a8(a); }
        else {
            dc_jsr(0xC0CB3A, 0xC0CBFE);
            if (!g_cpu._flag_C) return;
            a = (u8)g_cpu.A;
        }
        if (!frame_claim(a, 0xC0E952, 0xC0CC0D)) return;
        frame_src(0x28);
        frame_tiles(0x10, 0, 0xC0CC64);
        frame_queue(0x100, 0x100, 0x100, 0x100);
        frame_sprites(4, 0x20, 0xFF, true, 1, false, 0x7F4800);
        return;
    }
    if (size == 1) {                              /* medium: 32 tiles, 8 pieces */
        if (lock == 3) {
            u8 a = abs8(0x1301 + x);
            dc_set_a8(a);
            if (!frame_claim(a, 0xC0E97A, 0xC0CF2D)) return;
            frame_src(0x50);
            frame_tiles(0x20, 0, 0xC0CF84);
            frame_queue(0x100, 0x200, 0x200, 0x200);
            frame_sprites(8, 0x40, 0xFF, false, 2, true, 0x7F4800);
            return;
        }
        u8 f = abs8(0x1B00 + x) & 0x7F;
        dc_set_a8(f);
        if (f == 0) {
            dc_jsr(0xC0CB3A, 0xC0D28A);
            if (!g_cpu._flag_C) return;
            if (!frame_claim((u8)g_cpu.A, 0xC0E97A, 0xC0D299)) return;
            frame_src(0x50);
            frame_tiles(0x10, 0, 0xC0D2F0);
            frame_step();
            return;
        }
        frame_resume(0x200);
        frame_src(0x50);
        frame_tiles(0x10, 0x20, 0xC0D35C);
        frame_queue(0x100, 0x200, 0x200, 0x200);
        frame_sprites(8, 0x40, 0xFF, false, 1, true, 0x7F4800);
        return;
    }
    /* large: 48 tiles over three steps, 12 pieces (shape 68 lays them out
     * differently) */
    if (lock == 3) return;
    u8 f = abs8(0x1B00 + x) & 0x7F;
    dc_set_a8(f);
    u8 shape = abs8(0x0D00 + x);
    dc_set_a8(shape);
    dc_cmp8(shape, 0x68);
    bool z = shape == 0x68;
    if (f == 0) {
        dc_jsr(0xC0CB3A, z ? 0xC0D608 : 0xC0D546);
        if (!g_cpu._flag_C) return;
        if (!frame_claim((u8)g_cpu.A, 0xC0E9AA, z ? 0xC0D617 : 0xC0D555)) return;
        frame_src(0x78);
        if (z) frame_tiles(0x10, 0, 0xC0D66E);
        else {
            frame_tiles(8, 0, 0xC0D5AC);
            frame_tiles_at(0x200, 8, 0x10, 0xC0D5EB);
        }
        frame_step();
    } else if (f == 1) {
        frame_resume(z ? 0x200 : 0x100);
        frame_src(0x78);
        frame_tiles(8, 0x20, z ? 0xC0D789 : 0xC0D6DC);
        frame_tiles_at(z ? 0x400 : 0x300, 8, 0x30, z ? 0xC0D7C8 : 0xC0D71B);
        frame_step();
    } else if (!z) {
        frame_resume(0x400);
        frame_src(0x78);
        frame_tiles(0x10, 0x40, 0xC0D834);
        frame_queue(0x300, 0x500, 0x500, 0x100);
        frame_sprites(12, 0x60, 8, false, 1, false, 0x7F4800);
    } else {
        frame_resume(0x300);
        frame_src(0x78);
        frame_tiles(8, 0x40, 0xC0DAB8);
        frame_tiles_at(0x500, 8, 0x50, 0xC0DAF3);
        frame_queue(0x100, 0x100, 0x100, 0x500);
        frame_sprites(12, 0x60, 4, false, 1, false, 0x7F4800);
    }
}

/* C0:CA76  Work through the frame-load queue (dp 76, linked through 1081,
 * last entry dp 77) with C0:CAD9 while there is time left: stop once the
 * scanline (latched V counter) is past dp 6B but not yet in vblank (F0+).
 * Skipped while a VRAM transfer is already queued (09A0). An object whose
 * frame needs more steps stays at the head of the queue (a locked large
 * object never finishes, so the loop then runs out the clock). */
DC_ENTRY(C0CA76, RTS)
{
    u8 q = abs8(0x09A0);
    dc_set_a8(q);
    dc_set_flags_nz8(q);
    if (q) return;
    dc_set_mx(1, 0);
    dc_set_mx(1, 1);
    dp8_set(0x79, 0);
    dc_set_a8(abs8(0x213F));
    for (;;) {
        dc_spend(600);                      /* about one pass of the original loop */
        abs8(0x2137);
        u8 lo = dc_vcount_read();
        u8 hi = dc_vcount_read() & 1;
        u16 v = (u16)(hi << 8 | lo);
        g_cpu.A = v;
        dc_set_mx(0, 1);
        dc_cmp16(v, 0xF0);
        if (g_cpu._flag_N) {
            dc_cmp16(v, dp16(0x6B));
            if (g_cpu._flag_C) { dc_set_mx(1, 1); return; }
        }
        g_cpu.A = 0;
        dc_set_mx(1, 1);
        u8 head = dp8(0x76);
        dc_set_a8(head);
        dc_set_flags_nz8(head);
        if (head & 0x80) return;
        dp8_set(0x6D, head);
        dc_cmp8(head, dp8(0x77));
        bool last = head == dp8(0x77);
        dc_jsr(0xC0CAD9, last ? 0xC0CAC2 : 0xC0CAAE);
        if (g_cpu._flag_C) continue;
        head = dp8(0x76);
        g_cpu.X = head;
        if (last) {
            dc_set_a8(0x80);
            abs8_set(0x1081 + head, 0x80);
            dp8_set(0x76, 0x80);
            dp8_set(0x77, 0x80);
        } else {
            u8 next = abs8(0x1081 + head);
            dp8_set(0x76, next);
            dc_set_a8(0x80);
            abs8_set(0x1081 + head, 0x80);
        }
        dc_set_flags_nz8(0x80);
    }
}

/* C0:B271  Build the frame's sprite lists: reset the three OAM lists
 * (0770.., 08A0.., 0710..) and their queue descriptors, then queue every
 * object of each of the 64 draw slots (0E00 + 2n, chained through 0E81)
 * with C0:B309; whatever is left of the previous frame's lists is hidden
 * (y = E0). */
static void hide_rest(u8 start_dp, u8 end_dp, u16 limit)
{
    u16 x = dp16(start_dp);
    for (;;) {
        dc_cmp16(x, dp16(end_dp));
        if (g_cpu._flag_C) break;
        abs8_set(0x0001 + x, 0xE0);
        x = (u16)(x + 4);
        dc_cmp16(x, limit);
        if (g_cpu._flag_C) break;
    }
    x = dp16(start_dp);
    dp16_set(end_dp, x);
    g_cpu.X = x;
    dc_set_flags_nz16(x);
}

DC_ENTRY(C0B271, RTS)
{
    dp16_set(0x89, 0x0901);
    dp16_set(0x8B, 0);
    dp16_set(0x81, 0x0907);
    dp16_set(0x83, 0);
    dp16_set(0x85, 0x091A);
    dp16_set(0x87, 0);
    dp16_set(0xDF, 0x0710);
    dp16_set(0xDB, 0x0770);
    dc_set_a8(0);
    abs8_set(0x2183, 0);
    dp8_set(0x6E, 0);
    dp16_set(0xDD, 0x08A0);
    g_cpu.Y = 0x7E;
    do {
        u8 o = abs8(0x0E00 + g_cpu.Y);
        dc_set_a8(o);
        if (!(o & 0x80)) {
            dp8_set(0x6D, o);
            dc_jsr(0xC0B309, 0xC0B2AF);
            for (;;) {
                g_cpu.X = dp16(0x6D);
                o = abs8(0x0E81 + g_cpu.X);
                dc_set_a8(o);
                if (o & 0x80) break;
                dp8_set(0x6D, o);
                dc_jsr(0xC0B309, 0xC0B2BB);
            }
        }
        g_cpu.Y = (u16)(g_cpu.Y - 2);
    } while (!(g_cpu.Y & 0x8000));
    dc_set_a8(0xE0);
    hide_rest(0xDB, 0x7B, 0x08A0);
    hide_rest(0xDD, 0x7D, 0x0900);
    hide_rest(0xDF, 0x7F, 0x0770);
    g_cpu._flag_C = 1;
}

/* C0:CB0A  Redraw the object's current frame into its second sprite list
 * (7F:4BC0 + 1700) without uploading tiles, and mark it complete (1B00 =
 * 80). Same checks as C0:CAD9, but those exits leave C alone. */
static void frame_redraw(void)
{
    u16 x = dp8(0x6D);
    g_cpu.X = x;
    u8 v = abs8(0x1100 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    if (v & 0x80) return;
    v = abs8(0x1A81 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    if (v == 0 || (v & 0x80)) return;
    v = abs8(0x0F00 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    if (!v) return;
    u8 size = abs8(0x1201 + x) & 3;
    dc_set_a8(size);
    if (size == 3) { dc_cmp8(3, 2); g_cpu._flag_C = 0; return; }
    if (size == 0) {
        frame_locate(0x28);
        frame_sprites(4, 0x20, 0xFF, true, -1, false, 0x7F4BC0);
    } else if (size == 1) {
        frame_locate(0x50);
        frame_sprites(8, 0x40, 0xFF, false, -1, true, 0x7F4BC0);
    } else {
        u8 shape = abs8(0x0D00 + x);
        dc_set_a8(shape);
        dc_cmp8(shape, 0x68);
        frame_locate(0x78);
        frame_sprites(12, 0x60, shape == 0x68 ? 4 : 8, false, -1, false, 0x7F4BC0);
    }
}

/* C0:08A0 / C0:07EC  Place object A from row X of the scene's object table
 * at 7E:29B0 (5 bytes: tile x, tile y, sub x, sub y, frame; 07EC's rows are
 * at 29BF and a zero at 29C6 hides the object instead): position 1800/1880
 * with the scroll origin dp F0/F1 added, map position 0A00/0A80, then
 * C0:A9CD and a redraw. */
static void obj_place(u32 row, u32 site)
{
    u16 x = g_cpu.X, y = g_cpu.Y;
    u8 tx = bus8(row + x), ty = bus8(row + 1 + x), sx = bus8(row + 2 + x), sy = bus8(row + 3 + x);
    g_cpu._flag_C = 0;
    abs8_set(0x1801 + y, dc_adc8(tx, dp8(0xF0)));
    g_cpu._flag_C = 0;
    abs8_set(0x1881 + y, dc_adc8(ty, dp8(0xF1)));
    abs8_set(0x1800 + y, (u8)(sx << 4));
    abs8_set(0x1880 + y, (u8)(sy << 4));
    abs8_set(0x0F01 + y, bus8(row + 4 + x));
    dp8_set(0xD9, (u8)(tx << 4));
    abs8_set(0x0A00 + y, (u8)(sx | dp8(0xD9)));
    dp8_set(0xD9, (u8)(ty << 4));
    abs8_set(0x0A80 + y, (u8)(sy | dp8(0xD9)));
    g_cpu._flag_C = (ty >> 4) & 1;                  /* last ASL */
    dc_set_a8(0);
    dc_set_flags_nz8(0);
    abs8_set(0x1601 + y, 0);
    dp8_set(0x6D, (u8)y);
    dc_jsr(0xC0A9CD, site);
    frame_redraw();
}

DC_ENTRY(C008A0, RTS)
{
    g_cpu.Y = g_cpu.A & 0xFF;
    obj_place(0x7E29B0, 0xC008FF);
}

DC_ENTRY(C007EC, RTS)
{
    u16 x = g_cpu.X, y = g_cpu.A & 0xFF;
    g_cpu.Y = y;
    if (bus8(0x7E29C6 + x)) { obj_place(0x7E29BF, 0xC0086B); return; }
    abs8_set(0x1000 + y, abs8(0x1000 + y) | 0x80);
    u8 a = abs8(0x1A81 + y);
    dc_set_a8(a);
    if (a & 0x80) {
        dc_push8((u8)x);
        dc_push8((u8)y);
        dc_jsr(0xC0734C, 0xC00802);
        g_cpu.Y = dc_pull8();
        g_cpu.X = dc_pull8();
    }
    dc_set_a8(0);
    dc_set_flags_nz8(0);
    abs8_set(0x1A81 + g_cpu.Y, 0);
}

/* C0:0871  Place the party (dp 97 / 99 / 9B, a negative slot is empty)
 * from rows 0, 5 and 10 of the scene's object table, each after
 * C0:A98A. */
DC_ENTRY(C00871, RTS)
{
    static const u16 sites[3][2] = { { 0x0875, 0x087C }, { 0x0885, 0x088C }, { 0x0895, 0x089C } };
    for (int k = 0; k < 3; k++) {
        u8 o = dp8((u8)(0x97 + 2 * k));
        dc_set_a8(o);
        dc_set_flags_nz8(o);
        if (k && (o & 0x80)) continue;
        dp8_set(0x6D, o);
        dc_jsr(0xC0A98A, 0xC00000 | sites[k][0]);
        dc_set_a8(dp8((u8)(0x97 + 2 * k)));
        g_cpu.X = (u16)(5 * k);
        dc_jsr(0xC008A0, 0xC00000 | sites[k][1]);
    }
}

/* C0:076B  Place the eight extra scene objects (dp A3..AA, negative =
 * none) from rows 0..7 (12 bytes each) of the table at 7E:29BF, each after
 * C0:A98A. */
DC_ENTRY(C0076B, RTS)
{
    for (int k = 0; k < 8; k++) {
        u16 at = (u16)(0x076B + 0x10 * k);
        u8 o = dp8((u8)(0xA3 + k));
        dc_set_a8(o);
        dc_set_flags_nz8(o);
        if (o & 0x80) continue;
        dp8_set(0x6D, o);
        dc_jsr(0xC0A98A, 0xC00000 | (u16)(at + 6));
        dc_set_a8(dp8((u8)(0xA3 + k)));
        g_cpu.X = (u16)(12 * k);
        dc_jsr(0xC007EC, 0xC00000 | (u16)(at + 0xD));
    }
}

/* C0:0715  Set up the field's objects after a room load: scroll origin
 * (dp F0/F1 = 1D0A/1D0E halved), the scene's objects (C0:076B) and the
 * party (C0:0871), the sprite lists (C0:B271). With 7F:01EC = 1 the party
 * keeps its current frame (1780 = 3: locked to 1301 = 0F01). */
DC_ENTRY(C00715, RTS)
{
    dc_set_mx(1, 1);
    u8 v = abs8(0x1D0A);
    g_cpu._flag_C = v & 1;
    dc_set_a8((u8)(v >> 1));
    dp8_set(0xF0, (u8)(v >> 1));
    v = abs8(0x1D0E);
    g_cpu._flag_C = v & 1;
    dc_set_a8((u8)(v >> 1));
    dp8_set(0xF1, (u8)(v >> 1));
    dc_jsr(0xC0076B, 0xC00723);
    dc_jsr(0xC00871, 0xC00726);
    dc_set_mx(1, 0);
    dc_jsr(0xC0B271, 0xC0072B);
    u8 f = ram8(0x101EC);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (f != 1) {
        if (f) dc_set_flags_nz8((u8)(f - 1));
        return;
    }
    g_cpu.A = (u16)(g_cpu.D << 8 | g_cpu.D >> 8);
    for (int k = 0; k < 3; k++) {
        u8 o = dp8((u8)(0x97 + 2 * k));
        dc_set_a8(o);
        dc_set_flags_nz8(o);
        if (o & 0x80) continue;
        u16 y = g_cpu.A;
        g_cpu.Y = y;
        dc_set_a8(3);
        abs8_set(0x1780 + y, 3);
        u8 fr = abs8(0x0F01 + y);
        dc_set_a8(fr);
        dc_set_flags_nz8(fr);
        abs8_set(0x1301 + y, fr);
    }
}
