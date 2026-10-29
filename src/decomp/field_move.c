/* Field movement helpers. Bank C0. */
#include "ram.h"

#define dps8 dp8_set
#define dps16 dp16_set
/* LDA abs,X (16-bit) in the data bank, crossing into the next bank as the
 * CPU does; for indexes that are not known to be small. */
static inline u16 absx16(u16 base, u16 x) { return bus16(((u32)g_cpu.DB << 16) + base + x); }

/* C0:ABA2  Direction from (dp F0, F1) to (dp F2, F3) as an angle 00-FF in
 * dp EE (EF = 0). The quadrant gives the base (00 down-right, 40 down-left,
 * 80 up-left, C0 up-right, angles running clockwise from straight down); the
 * two distances (along and across the quadrant, in F6/F7) are scaled to 5
 * bits each and index the 32x32 arctangent table DB:F300. */
DC_ENTRY(C0ABA2, RTS)
{
    dps8(0xEF, 0);
    u8 x0 = dp8(0xF0), y0 = dp8(0xF1), x1 = dp8(0xF2), y1 = dp8(0xF3);
    u8 a, b, base;                            /* a -> F6, b -> F7 */
    if (y1 >= y0) {
        u8 dy = (u8)(y1 - y0);
        if (x1 >= x0) { a = (u8)(x1 - x0); b = dy; base = 0x00; }
        else          { a = dy; b = (u8)(x0 - x1); base = 0x40; }
    } else {
        u8 dy = (u8)(y0 - y1);
        if (x1 < x0)  { a = (u8)(x0 - x1); b = dy; base = 0x80; }
        else          { a = dy; b = (u8)(x1 - x0); base = 0xC0; }
    }
    dps8(0xF6, a);
    dps8(0xF7, b);
    u16 lo, idx;
    if ((a | b) & 0x40)      { lo = (u16)((a & 0x7F) >> 2); idx = (u16)(((b & 0x7C) << 3) | lo); }
    else if ((a | b) & 0x20) { lo = (u16)((a & 0x3F) >> 1); idx = (u16)(((b & 0x3E) << 4) | lo); }
    else                     { lo = (u16)(a & 0x1F);        idx = (u16)(((b & 0x1F) << 5) | lo); }
    dps16(0xF4, lo);
    g_cpu.X = idx;
    u8 t = bus8((u32)g_cpu.DB << 16 | (u16)(0xF300 + idx));
    u16 r = (u16)(t + base);
    u8 v = (u8)r;
    g_cpu._flag_C = r >> 8;
    g_cpu._flag_V = ((~(t ^ base) & (t ^ v)) >> 7) & 1;
    dc_set_flags_nz8(v);
    dps8(0xEE, v);
    g_cpu.A = (u16)((idx & 0xFF00) | v);
}

/* Velocity component: speed * |sincos[angle]| / 256 with the table's sign
 * (DB:F800 holds sine, angles >= 80 negative), via the hardware multiplier. */
static u8 vel_component(u8 angle)
{
    u8 t = bus8((u32)g_cpu.DB << 16 | (u16)(0xF800 + angle));
    if (angle < 0x80) { io8_set(0x4203, t); return io8(0x4217); }
    io8_set(0x4203, (u8)-t);
    return (u8)-io8(0x4217);
}
/* One try: velocity for angle dp EE with the speed of object `spd_obj`
 * (1A00), into dp DB/DC (y) and DD/DE (x). Returns false when both are 0.
 * Leaves C/V as the original's last ADC/CPX. */
static bool velocity(u8 spd_obj, bool retry)
{
    io8_set(0x4202, ram8(0x1A00 + spd_obj));
    u8 ee = dp8(0xEE);
    dps8(0xDB, vel_component(ee));
    u8 x = (u8)(ee + 0x40);
    g_cpu._flag_V = ((~(ee ^ 0x40) & (ee ^ x)) >> 7) & 1;
    g_cpu._flag_C = x >= 0x80;               /* CPX #$80 */
    u8 dd = vel_component(x);
    dps8(0xDD, dd);
    if (dd || !retry) dps8(0xDE, (dd & 0x80) ? 0xFF : 0);   /* the retry leaves DE alone when DD = 0 */
    u8 db = dp8(0xDB);
    if (!dd && !db) return false;
    dps8(0xDC, (db & 0x80) ? 0xFF : 0);
    return true;
}
/* Map tile index of the object's position plus the velocity; stores the
 * new x at dp DF and the tile's two map bytes (7E:7000/7E:7040) at E1/E5.
 * A's high byte is left as the original leaves it. */
static u16 dest_tile(u16 o)
{
    u16 nx = (u16)(ram16(OBJ_POS_X(o)) + dp16(0xDD));
    dps16(0xDF, nx);
    u16 py = ram16(OBJ_POS_Y(o)), dy = dp16(0xDB), ny = (u16)(py + dy);
    g_cpu._flag_C = (u32)py + dy > 0xFFFF;
    g_cpu._flag_V = ((~(py ^ dy) & (py ^ ny)) >> 15) & 1;
    u16 idx = (u16)(((ny & 0xFF00) | (nx >> 8)) & ram16(0x1D1E));
    g_cpu.A = idx;
    dps8(0xE1, ram8(0x7000 + idx));
    dps8(0xE5, ram8(0x7040 + idx));
    return idx;
}
/* Sprite layer bits for the destination tile: from the tile's flags when the
 * object follows the map (0F80 bit 7), otherwise from 0F80 itself. */
static void tile_layers(u16 y)
{
    u8 p = ram8(0x0F80 + y);
    if (p & 0x80) {
        dc_set_mx(1, 1);
        u16 o = dp8(0x6D);
        g_cpu.X = o;
        ram8_set(0x0C01 + o, (u8)(((dp8(0xE5) & 0x40) >> 2) | 0x20));
        ram8_set(0x0C00 + o, (u8)(((dp8(0xE1) & 0x40) >> 2) | 0x20));
    } else {
        dps8(0xD9, p);
        u16 o = dp16(0x6D);
        ram8_set(0x0C01 + o, p & 0x30);
        ram8_set(0x0C00 + o, (u8)((p << 4) & 0x30));
    }
}
/* C0:ACFD  Set the current object's velocity (1900 x, 1980 y; 8-bit) for
 * moving in direction dp EE at its speed (1A00). Objects that collide with
 * the map (1C80 bit 0) heading into a blocked tile (7E:7040 bit 7) try once
 * more in a direction from the table DB:FE00 (dp F8 steps through it) and
 * stand still if that is blocked too. X in = object (low byte). */
DC_ENTRY(C0ACFD, RTS)
{
    dc_set_mx(1, 1);
    u8 sx = (u8)g_cpu.X;
    ram8_set(0x1900 + sx, 0);
    ram8_set(0x1980 + sx, 0);
    bool moving = velocity(sx, false);
    if (moving) {
        u16 o = dp8(0x6D);
        dest_tile(o);
        dc_set_mx(1, 0);
        u16 y = dp16(0x6D);
        g_cpu.Y = y;
        g_cpu.X = g_cpu.A;
        g_cpu.A = (u16)((g_cpu.A & 0xFF00) | ram8(0x7040 + g_cpu.X));
        if ((ram8(OBJ_MOVE_PROPS(y)) & 1) && (dp8(0xE5) & 0x80)) {
            dc_set_mx(1, 1);
            dps8(0xF8, (u8)(dp8(0xF8) + 1));
            dps8(0xEE, bus8((u32)g_cpu.DB << 16 | (u16)(0xFE00 + dp8(0xF8))));
            u8 o8 = dp8(0x6D);
            if (velocity(o8, true)) {
                dest_tile(o8);
                dc_set_mx(1, 0);
                g_cpu.X = g_cpu.A;
                if (dp8(0xE5) & 0x80) {
                    dc_set_mx(1, 1);
                    dps8(0xDB, 0);
                    dps8(0xDD, 0);
                } else {
                    tile_layers(g_cpu.Y);
                }
            }
        } else {
            tile_layers(y);
        }
    }
    u16 o = g_cpu.x_flag ? dp8(0x6D) : dp16(0x6D);
    g_cpu.X = o;
    ram8_set(0x1900 + o, dp8(0xDD));
    u8 db = dp8(0xDB);
    ram8_set(0x1980 + o, db);
    dc_set_a8(db);
    dc_set_flags_nz8(db);
    dc_set_mx(1, 0);
}

/* ---- animation by movement -------------------------------------------- */
/* Set object o's movement animation (1680) to a, restarting it (1681, 1601)
 * when it changes; flags/A as the original's CMP and LDA #a leave them. */
static void set_walk_anim(u16 o, u8 a)
{
    u8 v = ram8(0x1680 + o);
    dc_cmp8(v, a);
    dc_set_a8(v);
    if (v == a) return;
    dc_set_a8(a);
    dc_set_flags_nz8(a);
    ram8_set(0x1680 + o, a);
    ram8_set(0x1681 + o, 0);
    ram8_set(OBJ_1601(o), 0);
}
/* C0:5614  Pick the current object's animation from its velocity: 0 when
 * still, 1 walking, 6 running (speed sum >= 20 for objects of kind < 4).
 * 1780 is the animation lock: 0 free, 3 = play 1680 until it ends (FF) and
 * then free, anything else leaves the animation alone. */
DC_ENTRY(C05614, RTS)
{
    u16 o = dp16(0x6D);
    g_cpu.X = o;
    u8 lock = ram8(0x1780 + o);
    dc_set_a8(lock);
    if (lock) {
        dc_cmp8(lock, 3);
        if (lock != 3) return;
        u8 an = ram8(0x1680 + o);
        dc_cmp8(an, 0xFF);
        if (an != 0xFF) { dc_set_a8(1); dc_set_flags_nz8(1); ram8_set(0x1780 + o, 1); return; }
        ram8_set(0x1780 + o, 0);
    }
    u8 vx = ram8(0x1900 + o), vy = ram8(0x1980 + o);
    if (vx & 0x80) vx = (u8)-vx;
    if (vy & 0x80) vy = (u8)-vy;
    dps8(0xD9, vx);
    u8 s = (u8)(vx + vy);
    g_cpu._flag_C = (vx + vy) > 0xFF;
    g_cpu._flag_V = ((~(vx ^ vy) & (vy ^ s)) >> 7) & 1;
    if (!s) { set_walk_anim(o, 0); return; }
    if (s >= 0x20 && ram8(OBJ_REMOVED(o)) < 4) { set_walk_anim(o, 6); return; }
    set_walk_anim(o, 1);
}
/* C0:568B  Unless the animation is locked (1780), set the current object's
 * animation to 0 (standing). */
DC_ENTRY(C0568B, RTS)
{
    u16 o = dp16(0x6D);
    g_cpu.X = o;
    u8 lock = ram8(0x1780 + o);
    dc_set_a8(lock);
    dc_set_flags_nz8(lock);
    if (lock) return;
    set_walk_anim(o, 0);
}

/* ---- touching other objects ------------------------------------------- */
static u16 sbc16(u16 a, u16 b)
{
    u16 r = (u16)(a - b - (g_cpu._flag_C ? 0 : 1));
    g_cpu._flag_C = (u32)a >= (u32)b + (g_cpu._flag_C ? 0 : 1);
    g_cpu._flag_V = (((a ^ b) & (a ^ r)) >> 15) & 1;
    dc_set_flags_nz16(r);
    return r;
}
/* Is object dp E9 less than a tile away from (dp DB, dp DD) along the
 * facing axis, on the facing side, and less than a tile away across it?
 * The first subtraction uses the caller's carry, the second a clear one,
 * exactly as the original SBCs without SEC/CLC. */
static void touch_test(bool y_axis, bool behind)
{
    dc_set_mx(0, 1);
    u16 o = dp8(0xE9);
    g_cpu.X = o;
    u16 pa = y_axis ? OBJ_POS_Y(o) : OBJ_POS_X(o), pb = y_axis ? OBJ_POS_X(o) : OBJ_POS_Y(o);
    u8 da = y_axis ? 0xDD : 0xDB, db = y_axis ? 0xDB : 0xDD;
    bool ok = false;
    u16 a = sbc16(ram16(pa), dp16(da));
    g_cpu.A = a;
    if (behind ? (a & 0x8000) : !(a & 0x8000)) {
        if (behind) a ^= 0xFFFF;
        g_cpu.A = a;
        dc_cmp16(a, 0x100);
        if (a < 0x100) {
            a = sbc16(ram16(pb), dp16(db));
            if (a & 0x8000) a ^= 0xFFFF;
            g_cpu.A = a;
            dc_cmp16(a, 0x100);
            ok = a < 0x100;
        }
    }
    dc_set_mx(1, 1);
    g_cpu._flag_C = ok;
}
DC_ENTRY(C05BFA, RTS) { touch_test(true, true); }    /* facing up */
DC_ENTRY(C05C20, RTS) { touch_test(true, false); }   /* facing down */
DC_ENTRY(C05C47, RTS) { touch_test(false, true); }   /* facing left */
DC_ENTRY(C05C6D, RTS) { touch_test(false, false); }  /* facing right */

/* C0:5B95  Does the current object (if it is shown and solid, 1C80 bit 1)
 * touch another shown, present, solid (1B01 bit 0) object on the side it
 * faces? Checks objects n..1 (n = 7F:2000); C set on the first hit, which
 * is left in X. dp EA = 0, D9 = facing*2, DB/DD = the object's position. */
DC_ENTRY(C05B95, RTS)
{
    dps8(0xEA, 0);
    u16 me = dp16(0x6D);
    g_cpu.X = me;
    u8 shown = ram8(OBJ_0F00(me));
    dc_set_a8(shown);
    dc_set_flags_nz8(shown);
    g_cpu._flag_C = 0;
    if (!shown) return;
    u8 mp = ram8(OBJ_MOVE_PROPS(me));
    dc_set_a8(mp);
    dc_set_flags_nz8(mp);
    if (!(mp & 2)) { g_cpu._flag_Z = 1; return; }
    dps16(0xC7, g_cpu.Y);
    u8 f2 = (u8)(ram8(OBJ_FACING(me)) << 1);
    dps8(0xD9, f2);
    dps16(0xDB, ram16(OBJ_POS_X(me)));
    u16 py = ram16(OBJ_POS_Y(me));
    dps16(0xDD, py);
    g_cpu.A = py;
    dc_set_mx(1, 1);
    u8 n2 = (u8)(ram8(0x12000) << 1);
    g_cpu._flag_C = ram8(0x12000) >> 7;      /* ASL */
    u8 x = n2;
    bool hit = false;
    while (x) {
        u8 v = ram8(OBJ_0F00(x));
        dc_set_a8(v);
        if (v) {
            v = ram8(OBJ_REMOVED(x));
            dc_set_a8(v);
            if (!(v & 0x80)) {
                g_cpu._flag_C = x >= dp8(0x6D);   /* CPX $6D */
                if (x != dp8(0x6D)) {
                    v = ram8(0x1B01 + x);
                    dc_set_a8(v);
                    if (v & 1) {
                        dps8(0xE9, x);
                        g_cpu.X = dp8(0xD9);
                        dc_jsr(0xC00000 | bus16(0xC05BF2 + g_cpu.X), 0xC05BE6);
                        x = dp8(0xE9);
                        if (g_cpu._flag_C) { hit = true; break; }
                    }
                }
            }
        }
        x = (u8)(x - 2);
    }
    dc_set_mx(1, 0);
    g_cpu.X = x;
    u16 y = dp16(0xC7);
    g_cpu.Y = y;
    dc_set_flags_nz16(y);
    g_cpu._flag_C = hit;
}

/* C0:AC69  Set the current object's velocity for direction dp EE at the
 * speed of object X (1A00), without map collision; 1900/1980 as signed
 * 16-bit words (y also in dp DB). */
DC_ENTRY(C0AC69, RTS)
{
    dc_set_mx(1, 1);
    u8 sx = (u8)g_cpu.X;
    ram8_set(0x1900 + sx, 0);
    ram8_set(0x1980 + sx, 0);
    io8_set(0x4202, ram8(0x1A00 + sx));
    u8 ee = dp8(0xEE);
    u16 o = dp8(0x6D);
    g_cpu.X = o;
    u8 vy = vel_component(ee);
    dps8(0xDB, vy);
    ram8_set(0x1980 + o, vy);
    ram8_set(0x1981 + o, (vy & 0x80) ? 0xFF : 0);
    u8 x = (u8)(ee + 0x40);
    g_cpu._flag_V = ((~(ee ^ 0x40) & (ee ^ x)) >> 7) & 1;
    g_cpu._flag_C = x >= 0x80;
    u8 vx = vel_component(x);
    ram8_set(0x1900 + o, vx);
    dc_set_mx(1, 0);
    u8 hi = (vx & 0x80) ? 0xFF : 0;
    ram8_set(0x1901 + o, hi);
    u8 a = hi ? 0xFF : vx;
    dc_set_a8(a);
    dc_set_flags_nz8(a);
}

/* C0:305D  One step toward the middle-bottom of the current tile (x fraction
 * 80, y fraction F0), 16 pixels a frame for one frame (1A01 = 1).
 * C clear when already there. */
DC_ENTRY(C0305D, RTS)
{
    u16 o = dp16(0x6D);
    g_cpu.X = o;
    ram8_set(0x1900 + o, 0);
    ram8_set(0x1980 + o, 0);
    u8 fx = ram8(OBJ_POS_X(o)) & 0xF0;
    dc_cmp8(fx, 0x80);
    dc_set_a8(fx);
    if (fx != 0x80) {
        u8 v = fx >= 0x80 ? 0xF0 : 0x10;
        ram8_set(0x1900 + o, v);
        dc_set_a8(v);
    }
    u8 fy = ram8(OBJ_POS_Y(o)) & 0xF0;
    dc_cmp8(fy, 0xF0);
    dc_set_a8(fy);
    if (fy == 0xF0) {
        if (fx == 0x80) { g_cpu._flag_C = 0; return; }
    } else {
        ram8_set(0x1980 + o, fy >= 0xF0 ? 0xF0 : 0x10);
    }
    ram8_set(OBJ_1A01(o), 1);
    dc_set_a8(1);
    dc_set_flags_nz8(1);
    g_cpu._flag_C = 1;
}

/* C0:30B3  One 16-pixel step toward object dp E9 (positions compared in
 * 16-pixel units), for one frame (1A01 = 7F:0B00 = 1). C clear when both
 * are in the same unit. */
DC_ENTRY(C030B3, RTS)
{
    u16 o = dp16(0x6D), t = dp16(0xE9);
    ram8_set(0x1900 + o, 0);
    ram8_set(0x1980 + o, 0);
    u16 mx = ram16(OBJ_POS_X(o)) & 0xFFF0, tx = absx16(0x1800, t) & 0xFFF0;
    dps16(0xD9, mx);
    g_cpu.A = tx;
    if (tx != mx) {
        dc_set_a8(tx >= mx ? 0x10 : 0xF0);
        ram8_set(0x1900 + o, (u8)g_cpu.A);
    }
    u16 my = ram16(OBJ_POS_Y(o)) & 0xFFF0, ty = absx16(0x1880, t) & 0xFFF0;
    dps16(0xD9, my);
    dc_cmp16(ty, my);
    g_cpu.A = ty;
    if (ty == my) {
        if (tx == mx) { g_cpu.X = t; g_cpu._flag_C = 0; return; }
    } else {
        ram8_set(0x1980 + o, ty >= my ? 0x10 : 0xF0);
    }
    g_cpu.X = o;
    dc_set_a8(1);
    dc_set_flags_nz8(1);
    ram8_set(OBJ_1A01(o), 1);
    ram8_set(0x10B00 + o, 1);
    g_cpu._flag_C = 1;
}

/* C0:9AC8  Map tile at index A & dp 1E: dp 58 = 7E:30C0[i], 5A = 7E:7000[i]
 * (layer flags), 5C = 7E:7040[i] (collision); X ends 8-bit. */
static void tile_info(u16 i)
{
    dp8_set(0x58, ram8(0x30C0 + i));
    dp8_set(0x5A, ram8(0x7000 + i));
    u8 c = ram8(0x7040 + i);
    dp8_set(0x5C, c);
    dc_set_a8(c);
    dc_set_flags_nz8(c);
    dc_set_mx(1, 1);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C09AC8, RTS)
{
    dc_set_mx(1, 0);
    u16 i = (u16)(g_cpu.A & dp16(0x1E));
    g_cpu.A = i;
    g_cpu.X = i;
    tile_info(i);
}
/* C0:9AA1  Same, but only for a blocked tile (7E:70C0 bit 7); a free one
 * returns C set with 16-bit X. */
DC_ENTRY(C09AA1, RTS)
{
    dc_set_mx(1, 0);
    u16 i = (u16)(g_cpu.A & dp16(0x1E));
    g_cpu.A = i;
    g_cpu.X = i;
    u8 v = ram8(0x70C0 + i);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    if (!(v & 0x80)) { g_cpu._flag_C = 1; return; }
    tile_info(i);
}
/* C0:A079 / A083 / A08D / A097  Face the current object left / right (02 03)
 * or up / down (00 01) and give object Y the walking animation; dp 58/59 = 0. */
static void face_and_walk(u8 dir)
{
    u16 o = g_cpu.x_flag ? dp8(0x6D) : dp16(0x6D);
    g_cpu.X = o;
    abs8_set(OBJ_FACING(o), dir);
    dp8_set(0x58, 0);
    dp8_set(0x59, 0);
    set_walk_anim(g_cpu.Y, 1);
}
DC_ENTRY(C0A079, RTS) { face_and_walk(2); }
DC_ENTRY(C0A083, RTS) { face_and_walk(3); }
DC_ENTRY(C0A08D, RTS) { face_and_walk(0); }
DC_ENTRY(C0A097, RTS) { face_and_walk(1); }
/* C0:A0A1 / A0AB / A0B5 / A0BF  Face the current object left / right / up /
 * down and give object Y the running animation (6), or animation 17 when Y
 * is the party leader on foot with no event control (1D32, 1D33, dp E5 all
 * 0); dp 58/59 = 0. */
static void run_anim(void)
{
    dp8_set(0x58, 0);
    dp8_set(0x59, 0);
    bool leader = !abs8(0x1D32) && !abs8(0x1D33) && (u8)g_cpu.Y == dp8(0x97) && !dp8(0xE5);
    set_walk_anim(g_cpu.Y, leader ? 0x17 : 0x06);
}
static void face_and_run(u8 dir)
{
    u16 o = dp8(0x6D);
    g_cpu.X = o;
    dc_set_a8(dir);
    abs8_set(OBJ_FACING(o), dir);
    run_anim();
}
DC_ENTRY(C0A0A1, RTS) { face_and_run(2); }
DC_ENTRY(C0A0AB, RTS) { face_and_run(3); }
DC_ENTRY(C0A0B5, RTS) { face_and_run(0); }
DC_ENTRY(C0A0BF, RTS) { face_and_run(1); }
/* C0:A11D / A132 / A147 / A15C  Same, turning to 2 / 3 / 2 / 3 unless the
 * object already faces that way or 1 / 1 / 0 / 0. */
static void face_unless_run(u8 dir, u8 other)
{
    u16 o = dp8(0x6D);
    g_cpu.X = o;
    u8 f = abs8(OBJ_FACING(o));
    if (f != dir && f != other) abs8_set(OBJ_FACING(o), dir);
    run_anim();
}
DC_ENTRY(C0A11D, RTS) { face_unless_run(2, 1); }
DC_ENTRY(C0A132, RTS) { face_unless_run(3, 1); }
DC_ENTRY(C0A147, RTS) { face_unless_run(2, 0); }
DC_ENTRY(C0A15C, RTS) { face_unless_run(3, 0); }
/* C0:5B8D  C = dp EB bit 7 clear. */
DC_ENTRY(C05B8D, RTS)
{
    u8 v = dp8(0xEB);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    g_cpu._flag_C = !(v & 0x80);
}

/* C0:1ED0  Is the tile under object X's x tile (7E:70C0 + tile) free?
 * C set when bit 7 is clear. dp DB = the tile x. */
static void tile_free(void)
{
    u8 tx = abs8(OBJ_TILE_X(g_cpu.X));
    dc_set_a8(tx);
    u16 x = g_cpu.x_flag ? tx : g_cpu.A;
    g_cpu.X = x;
    if (g_cpu.x_flag) dp8_set(0xDB, (u8)x); else dp16_set(0xDB, x);
    u8 v = ram8(0x70C0 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    g_cpu._flag_C = !(v & 0x80);
}
DC_ENTRY(C01ED0, RTS) { tile_free(); }
/* C0:1EC7  Same, for the tile below party leader dp 97 (index (y+1, x)
 * in 16-bit index mode). */
static void leader_tile_free(s8 dx, u8 dy)
{
    dc_set_mx(1, g_cpu.x_flag);
    u16 o = g_cpu.x_flag ? dp8(0x97) : dp16(0x97);
    g_cpu.X = o;
    u8 ty = (u8)(abs8(OBJ_TILE_Y(o)) + dy), tx = (u8)(abs8(OBJ_TILE_X(o)) + dx);
    g_cpu.A = (u16)(ty << 8 | tx);
    u16 x = g_cpu.x_flag ? tx : g_cpu.A;
    g_cpu.X = x;
    if (g_cpu.x_flag) dp8_set(0xDB, (u8)x); else dp16_set(0xDB, x);
    u8 v = ram8(0x70C0 + x);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    g_cpu._flag_C = !(v & 0x80);
}
DC_ENTRY(C01EC7, RTS) { leader_tile_free(0, 1); }
/* C0:1EE0 / 1EF7  Same for the tiles left / right of the leader. */
DC_ENTRY(C01EE0, RTS) { leader_tile_free(-1, 0); }
DC_ENTRY(C01EF7, RTS) { leader_tile_free(1, 0); }
/* C0:1E9A  Is the tile above the leader free, or else the one two above?
 * C set when one of them is free. */
DC_ENTRY(C01E9A, RTS)
{
    leader_tile_free(0, 0xFF);
    if (g_cpu._flag_C) return;
    leader_tile_free(0, 0xFE);
}
/* C0:A0C9 / A0DE / A0F3 / A108  Turn the current object to face 2 / 3 / 2 / 3
 * unless it already faces that way or 1 / 1 / 0 / 0, then give object Y the
 * walking animation; dp 58/59 = 0. */
static void face_unless(u8 dir, u8 other)
{
    u16 o = g_cpu.x_flag ? dp8(0x6D) : dp16(0x6D);
    g_cpu.X = o;
    u8 f = abs8(OBJ_FACING(o));
    if (f != dir && f != other) abs8_set(OBJ_FACING(o), dir);
    dp8_set(0x58, 0);
    dp8_set(0x59, 0);
    set_walk_anim(g_cpu.Y, 1);
}
DC_ENTRY(C0A0C9, RTS) { face_unless(2, 1); }
DC_ENTRY(C0A0DE, RTS) { face_unless(3, 1); }
DC_ENTRY(C0A0F3, RTS) { face_unless(2, 0); }
DC_ENTRY(C0A108, RTS) { face_unless(3, 0); }

/* C0:5B1F  Cancel what object dp EB is running at a priority >= 1: save its
 * script position to the priority slot 7F:0580 + prio*0x80 + obj, park it
 * at 16FF and clear priority, timed move, move frames and 1001. */
DC_ENTRY(C05B1F, RTS)
{
    u16 o = dp8(0xEB);
    g_cpu.X = o;
    u8 v = abs8(OBJ_REMOVED(o));
    dc_set_a8(v);
    if (v & 0x80) { dc_set_flags_nz8(v); return; }
    u8 prio = abs8(OBJ_PRIORITY(o));
    dc_set_a8(prio);
    dc_cmp8(prio, 1);
    if (prio < 1) return;
    abs8_set(0x4202, prio);
    abs8_set(0x4203, 0x80);
    dc_set_mx(0, 0);
    u16 prod = abs16(0x4216), eb = dp16(0xEB);
    u32 sum = (u32)prod + eb;
    u16 slot = (u16)sum;
    g_cpu._flag_C = sum > 0xFFFF;
    g_cpu._flag_V = ((~(prod ^ eb) & (prod ^ slot)) >> 15) & 1;
    dp16_set(0xD9, slot);
    ram16_set(0x10580 + slot, abs16(OBJ_SCRIPT_POS(o)));
    abs16_set(OBJ_SCRIPT_POS(eb), 0x16FF);
    dc_set_mx(1, 1);
    u16 x = eb & 0xFF;
    g_cpu.X = x;
    g_cpu.A = 0x1600;
    dc_set_flags_nz8(0);
    abs8_set(OBJ_PRIORITY(x), 0);
    abs8_set(OBJ_1A80(x), 0);
    abs8_set(OBJ_1A01(x), 0);
    abs8_set(0x1001 + x, 0);
}

/* C0:9A1F / 9A60  Edge test for scrolling (y / x): dp 52 = A; compares half
 * of dp 10 / 0C with dp 1D / 1B: below -> C from 88 / 80 >= the leader's
 * 0A80 / 0A00; above -> C set; equal -> C from dp 96 / 93 + A >= 10. */
static void edge_test(u8 half_dp, u8 lim_dp, u8 add_dp, u8 k, u16 table)
{
    u8 a = (u8)g_cpu.A;
    dp8_set(0x52, a);
    u8 h = dp8(half_dp) >> 1, lim = dp8(lim_dp);
    dc_cmp8(h, lim);
    dc_set_a8(h);
    if (h < lim) {
        dc_set_a8(k);
        u16 x = g_cpu.x_flag ? abs8(0x0197) : abs16(0x0197);
        g_cpu.X = x;
        dc_cmp8(k, abs8(table + x));
        return;
    }
    if (h != lim) { g_cpu._flag_C = 1; return; }
    u8 b = dp8(add_dp), r = (u8)(b + a);
    g_cpu._flag_V = ((~(b ^ a) & (b ^ r)) >> 7) & 1;
    dc_set_a8(r);
    dc_cmp8(r, 0x10);
}
DC_ENTRY(C09A1F, RTS) { edge_test(0x10, 0x1D, 0x96, 0x88, 0x0A80); }
DC_ENTRY(C09A60, RTS) { edge_test(0x0C, 0x1B, 0x93, 0x80, 0x0A00); }

/* C0:1D84 / 1DD1  Is object dp EB in front of the camera point (dp C3, C5)
 * by 0..1BF pixels along y / x and within BF pixels across? The first SBC
 * uses the caller's carry, the second the clear carry its CMP left. */
static void view_test2(bool y_axis, bool behind);
static void view_test(bool y_axis) { view_test2(y_axis, false); }
static void view_test2(bool y_axis, bool behind)
{
    dc_set_mx(0, 1);
    u16 o = dp8(0xEB);
    g_cpu.X = o;
    u16 pa = y_axis ? OBJ_POS_Y(o) : OBJ_POS_X(o), pb = y_axis ? OBJ_POS_X(o) : OBJ_POS_Y(o);
    u8 da = y_axis ? 0xC5 : 0xC3, db = y_axis ? 0xC3 : 0xC5;
    bool ok = false;
    u16 a = sbc16(abs16(pa), dp16(da));
    g_cpu.A = a;
    if (behind ? (a & 0x8000) : !(a & 0x8000)) {
        if (behind) a ^= 0xFFFF;
        g_cpu.A = a;
        dc_cmp16(a, 0x01C0);
        if (a < 0x01C0) {
            a = sbc16(abs16(pb), dp16(db));
            if (a & 0x8000) { a ^= 0xFFFF; dc_set_flags_nz16(a); }
            g_cpu.A = a;
            dc_cmp16(a, 0x00C0);
            ok = a < 0x00C0;
        }
    }
    dc_set_mx(1, 1);
    g_cpu._flag_C = ok;
}
DC_ENTRY(C01D84, RTS) { view_test(true); }
DC_ENTRY(C01DD1, RTS) { view_test(false); }
DC_ENTRY(C01D5E, RTS) { view_test2(true, true); }
DC_ENTRY(C01DAB, RTS) { view_test2(false, true); }

/* C0:9A3D / 9A7E  Edge tests the other way (y / x): dp 54 = A; compares
 * dp 1C / 1A with half of dp 0E / 0A: below -> C from the leader's 0A80 /
 * 0A00 >= 88 / 80; above -> C set; equal -> C set when dp 96 / 93 - A < 0. */
static void edge_test2(u8 half_dp, u8 lim_dp, u8 sub_dp, u8 k, u16 table)
{
    u8 a = (u8)g_cpu.A;
    dp8_set(0x54, a);
    u8 h = dp8(half_dp) >> 1;
    dp8_set(0x52, h);
    u8 lim = dp8(lim_dp);
    dc_set_a8(lim);
    dc_cmp8(lim, h);
    if (lim < h) {
        u16 x = g_cpu.x_flag ? abs8(0x0197) : abs16(0x0197);
        g_cpu.X = x;
        u8 v = abs8(table + x);
        dc_set_a8(v);
        dc_cmp8(v, k);
        return;
    }
    if (lim != h) { g_cpu._flag_C = 1; return; }
    u8 b = dp8(sub_dp), r = (u8)(b - a);
    g_cpu._flag_V = (((b ^ a) & (b ^ r)) >> 7) & 1;
    dc_set_a8(r);
    dc_set_flags_nz8(r);
    g_cpu._flag_C = (r & 0x80) != 0;
}
DC_ENTRY(C09A3D, RTS) { edge_test2(0x0E, 0x1C, 0x96, 0x88, 0x0A80); }
DC_ENTRY(C09A7E, RTS) { edge_test2(0x0A, 0x1A, 0x93, 0x80, 0x0A00); }

/* C0:9C37  May the party move on? (dp 6C, dp 6A and 0155 == 3 cases.) */
DC_ENTRY(C09C37, RTS)
{
    u8 a = dp8(0x6C);
    dc_set_a8(a);
    dc_set_flags_nz8(a);
    if (a) {
        u8 v = abs8(0x0155);
        dc_set_a8(v);
        dc_cmp8(v, 3);
        g_cpu._flag_C = v == 3;
        return;
    }
    a = dp8(0x6A);
    dc_set_a8(a);
    dc_set_flags_nz8(a);
    if (!a) { g_cpu._flag_C = 1; return; }
    dc_cmp8(a, 3);
    if (a == 3) { g_cpu._flag_C = 0; return; }
    u8 w = abs8(0x0155);
    dc_cmp8(a, w);
    if (a == w) { g_cpu._flag_C = 0; return; }
    dc_set_a8(w);
    dc_cmp8(w, 3);
    g_cpu._flag_C = w != 3;
}

/* C0:A7BC  Test bit ((dp 52 - dp 06) >> 2) of the bitmap at 7F:0001;
 * C = the bit. dp 50 = the bit mask (its high byte keeps the index). */
DC_ENTRY(C0A7BC, RTS)
{
    u16 x0 = g_cpu.X;
    u16 i = (u16)((u16)(dp16(0x52) - dp16(0x06)) >> 2);
    dp16_set(0x50, i);
    u16 x = (u16)((i >> 3) & 0x3F);
    u8 b = (u8)(i & 7);
    g_cpu.Y = b;
    u8 mask = abs8(0xFF20 + b);
    dp8_set(0x50, mask);
    u8 v = ram8(0x10001 + x);
    g_cpu.A = v;
    g_cpu._flag_V = (mask >> 6) & 1;
    g_cpu.X = x0;
    dc_set_flags_nz16(x0);
    g_cpu._flag_C = (v & mask) != 0;
}

/* C0:AB45  Current object's screen position in tiles, 9-bit wrapped:
 * 0A00 = pixel X/16 - (camera tile X*8 + 1D87 & 7), 0A80 the same for Y
 * (1D0E, 1D89). dp D9/DB = the sub-tile offsets, DD/DF = the camera
 * origin. */
DC_ENTRY(C0AB45, RTS)
{
    dc_set_mx(0, 1);
    u8 o = dp8(0x6D);
    g_cpu.X = o;
    u16 sx = abs16(0x1D87) & 7, sy = abs16(0x1D89) & 7;
    dp16_set(0xD9, sx);
    dp16_set(0xDB, sy);
    u16 ox = (u16)(((abs16(0x1D0A) & 0xFF) << 3) + sx);
    u16 oy = (u16)(((abs16(0x1D0E) & 0xFF) << 3) + sy);
    dp16_set(0xDD, ox);
    dp16_set(0xDF, oy);
    u16 r = 0;
    for (int k = 0; k < 2; k++) {
        g_cpu._flag_C = 1;
        r = sbc16((u16)(abs16((k ? 0x1880 : 0x1800) + o) >> 4), k ? oy : ox);
        if (!g_cpu._flag_C) r |= 0x100;
        r &= 0x1FF;
        abs16_set((k ? 0x0A80 : 0x0A00) + o, r);
    }
    g_cpu.A = r;
    dc_set_flags_nz16(r);
    dc_set_mx(1, 1);
}

/* C0:0617  List up to 12 objects of kind 5 or 6 (1100) that are on screen
 * or flagged (1A81 bit 7 or 0F00) and strictly inside the 15x14 tiles
 * around the camera (1D0A/2, 1D0E/2), scanning from object 7F:2000 down,
 * into 019D.., ending with 80. */
DC_ENTRY(C00617, RTS)
{
    u8 cx = abs8(0x1D0A) >> 1, cy = abs8(0x1D0E) >> 1;
    dp8_set(0xDB, cx);
    dp8_set(0xDD, cy);
    u8 y = 0;
    g_cpu.Y = 0;
    u8 x = (u8)(ram8(0x12000) << 1);
    g_cpu._flag_C = ram8(0x12000) >> 7;
    do {
        g_cpu.X = x;
        if ((abs8(0x1A81 + x) & 0x80) || abs8(0x0F00 + x)) {
            u8 kind = abs8(0x1100 + x);
            dc_cmp8(kind, 5);
            if (kind != 5) dc_cmp8(kind, 6);
            if (kind == 5 || kind == 6) {
                g_cpu._flag_C = 1;
                u8 dx = dc_sbc8(abs8(0x1801 + x), cx);
                if (dx) { dc_cmp8(dx, 0x0F);
                if (dx < 0x0F) {
                    g_cpu._flag_C = 1;
                    u8 dy = dc_sbc8(abs8(0x1881 + x), cy);
                    if (dy) { dc_cmp8(dy, 0x0E);
                    if (dy < 0x0E) {
                        abs8_set(0x019D + y, x);
                        y++;
                        g_cpu.Y = y;
                        dc_cmp8(y, 0x0C);
                        if (y == 0x0C) break;
                    } }
                } }
            }
        }
        x = (u8)(x - 2);
        g_cpu.X = x;
    } while (x);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
    abs8_set(0x019D + y, 0x80);
}

/* C0:99DE  Cancel the camera step dp 2E / 30 when the map edge is reached in
 * that direction (C0:9A60 / 9A7E / 9A1F / 9A3D, given the step in tiles).
 * A blocked downward step also runs the upward test, as the original falls
 * through. */
static void edge_cancel(u8 step_dp, u32 pos_fn, u32 neg_fn, u16 pos_site, u16 neg_site, bool fall)
{
    u8 s = dp8(step_dp);
    dc_set_a8(s);
    dc_set_flags_nz8(s);
    if (!s) return;
    if (!(s & 0x80)) {
        dc_set_a8(s >> 4);
        g_cpu._flag_C = (s >> 3) & 1;
        dc_jsr(pos_fn, 0xC00000 | pos_site);
        if (!g_cpu._flag_C) return;
        dp8_set(step_dp, 0);
        if (!fall) return;
    }
    u8 a = (u8)(-(u8)g_cpu.A);
    g_cpu._flag_C = (a >> 3) & 1;
    dc_set_a8(a >> 4);
    dc_jsr(neg_fn, 0xC00000 | neg_site);
    if (g_cpu._flag_C) dp8_set(step_dp, 0);
}
DC_ENTRY(C099DE, RTS)
{
    edge_cancel(0x2E, 0xC09A60, 0xC09A7E, 0x99E8, 0x99F8, false);
    edge_cancel(0x30, 0xC09A1F, 0xC09A3D, 0x9A09, 0x9A17, true);
}

/* C0:9FF2  Pick the current object Y's standing animation: 19 for the
 * leader while dp 62 = 2; 0 normally (or with no idle timer, dp 1F = 0);
 * when the idle counter dp 58 reaches 2FF (and every 0x80 frames after),
 * mark it (bit 15), advance the random index dp F8 and play the idle
 * animation 21 when DB:FE00[F8] < 8. */
static void idle_anim(void)
{
    g_cpu.X = dp8(0x6D);
    u8 st = dp8(0x62);
    dc_set_a8(st);
    if (st) {
        dc_cmp8(st, 2);
        if (st == 2) {
            dc_cmp8((u8)g_cpu.Y, dp8(0x97));
            if ((u8)g_cpu.Y == dp8(0x97)) { set_walk_anim(g_cpu.Y, 0x19); return; }
        }
    }
    u8 idle = dp8(0x1F);
    dc_set_a8(idle);
    bool special = false;
    if (idle) {
        dc_set_mx(0, 1);
        u16 w = dp16(0x58);
        g_cpu.A = w;
        dc_cmp16(w, 0x2FF);
        if (w == 0x2FF) special = true;
        else if (w > 0x2FF) {
            u16 l = w & 0x7F;
            g_cpu.A = l;
            dc_cmp16(l, 0x7F);
            if (l == 0x7F) special = true;
            else { dc_set_mx(1, 1); return; }
        }
        if (special) {
            g_cpu.A = 0x8000;
            g_cpu._flag_Z = !(w & 0x8000);
            dp16_set(0x58, w | 0x8000);
            dc_set_mx(1, 1);
            u8 r = (u8)(dp8(0xF8) + 1);
            dp8_set(0xF8, r);
            g_cpu.X = r;
            u8 v = abs8(0xFE00 + r);
            dc_cmp8(v, 8);
            if (v < 8) { set_walk_anim(g_cpu.Y, 0x21); return; }
        }
        dc_set_mx(1, 1);
    }
    set_walk_anim(g_cpu.Y, 0);
}
DC_ENTRY(C09FF2, RTS) { idle_anim(); }

/* C0:A712  Build the map's room table: clear 7E:70C0..70FF to 80 and copy
 * it over the rows 71C0..AFC0 (one per 0x100), then for room list
 * F5:F000[0100 & 1FF] (following redirects: an entry whose first word is 0
 * names another list) number each rectangle's tile (F5:0000 entry & dp 1E)
 * in 7E:70C0 with its index; tiles in the bitmap (C0:A7BC) with 7E:30C0
 * bit 0 turn their 7E:3000 code FE into FF and E0 into E1. */
DC_ENTRY(C0A712, RTS)
{
    for (u16 x = 0; x < 0x40; x++) ram8_set(0x70C0 + x, 0x80);
    g_cpu.X = 0x40;
    dc_cmp16(0x40, 0x40);
    dc_set_a8(0x80);
    dc_set_mx(0, 0);
    g_cpu.Y = 0xAFC0;
    do {
        g_cpu.X = 0x70C0;
        g_cpu.A = 0x3F;
        dc_push8(g_cpu.DB);
        dc_mvn(0x7E, 0x7E);
        g_cpu.DB = dc_pull8();
        g_cpu._flag_C = 1;
        g_cpu.A = g_cpu.Y = dc_sbc16(g_cpu.Y, 0x0140);
        dc_cmp16(g_cpu.Y, 0x70C0);
    } while (g_cpu.Y != 0x70C0);
    dp16_set(0x06, bus16(0xF5F000));
    u16 a = abs16(0x0100) & 0x01FF;
    u16 x;
    for (;;) {
        g_cpu._flag_C = a >> 15;
        x = (u16)(a << 1);
        g_cpu.X = x;
        u16 end = bus16(0xF5F002 + x);
        dp16_set(0x56, end);
        a = bus16(0xF5F000 + x);
        g_cpu.A = a;
        dc_cmp16(a, end);
        if (a == end) { dc_set_mx(1, 0); return; }
        dp16_set(0x08, a);
        dp16_set(0x50, x);
        g_cpu.X = a;
        u16 h = bus16(0xF50000 + a);
        g_cpu.A = h;
        if (h) break;
        a = bus16(0xF50002 + a);
    }
    x = dp16(0x08);
    g_cpu.X = x;
    dp16_set(0x54, 0);
    for (;;) {
        dc_cmp16(x, dp16(0x56));
        if (x >= dp16(0x56)) break;
        dp16_set(0x52, x);
        x = bus16(0xF50000 + x) & dp16(0x1E);
        g_cpu.X = x;
        dc_set_mx(1, 0);
        u8 n = dp8(0x54);
        dc_set_a8(n);
        ram8_set(0x70C0 + x, n);
        dc_jsr(0xC0A7BC, 0xC0A782);
        if (g_cpu._flag_C) {
            x = g_cpu.X;
            u8 f = ram8(0x30C0 + x);
            dc_set_a8(f);
            if (f & 1) {
                u8 c = ram8(0x3000 + x);
                dc_set_a8(c);
                dc_cmp8(c, 0xFE);
                if (c == 0xFE) { dc_set_a8(0xFF); ram8_set(0x3000 + x, 0xFF); }
                else { dc_cmp8(c, 0xE0); if (c == 0xE0) { dc_set_a8(0xE1); ram8_set(0x3000 + x, 0xE1); } }
            }
        }
        n = (u8)(dp8(0x54) + 1);
        dc_set_a8(n);
        dp8_set(0x54, n);
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        x = dc_adc16(dp16(0x52), 4);
        g_cpu.X = x;
    }
    dc_set_mx(1, 0);
}

/* LAAC7: fractional-speed step shared by C0:AA07 and C0:AAFD (8-bit A/X
 * on entry): while the step counter 1A01 runs, 1800 += 16-bit 1900 and the
 * 1980 fraction accumulates 1B81 (carry into 1981) before 1880 += 1980. */
static void step_frac(u8 o)
{
    u8 n = abs8(0x1A01 + o);
    dc_set_a8(n);
    dc_set_flags_nz8(n);
    if (!n) return;
    n--;
    dc_set_a8(n);
    abs8_set(0x1A01 + o, n);
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    abs16_set(0x1800 + o, dc_adc16(abs16(0x1900 + o), abs16(0x1800 + o)));
    dc_set_mx(1, 1);
    g_cpu._flag_C = 0;
    u8 f = dc_adc8(abs8(0x1980 + o), abs8(0x1B81 + o));
    if (g_cpu._flag_C) abs8_set(0x1981 + o, (u8)(abs8(0x1981 + o) + 1));
    abs8_set(0x1980 + o, f);
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    u16 r = dc_adc16(abs16(0x1980 + o), abs16(0x1880 + o));
    g_cpu.A = r;
    abs16_set(0x1880 + o, r);
    dc_set_mx(1, 1);
}

/* C0:AAFD  Move the current object by its velocity for one frame while its
 * step counter 1A01 runs: 1800 += signed 1900, 1880 += signed 1980; with
 * fractional speed (1B80 set) see step_frac. */
DC_ENTRY(C0AAFD, RTS)
{
    u8 o = dp8(0x6D);
    g_cpu.X = o;
    if (abs8(0x1B80 + o)) { step_frac(o); return; }
    u8 n = abs8(0x1A01 + o);
    dc_set_a8(n);
    dc_set_flags_nz8(n);
    if (!n) return;
    n--;
    dc_set_a8(n);
    abs8_set(0x1A01 + o, n);
    u8 v = abs8(0x1900 + o);
    u16 dx = (v & 0x80) ? (u16)(0xFF00 | v) : v;
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    abs16_set(0x1800 + o, dc_adc16(dx, abs16(0x1800 + o)));
    dc_set_mx(1, 1);
    v = abs8(0x1980 + o);
    u16 dy = (v & 0x80) ? (u16)(0xFF00 | v) : v;
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    u16 r = dc_adc16(dy, abs16(0x1880 + o));
    g_cpu.A = r;
    abs16_set(0x1880 + o, r);
    dc_set_mx(1, 1);
}

/* One axis of C0:AA07: position byte at `lo` += signed v (dp D9 = |v| for a
 * negative v). With `wrap` the carry/borrow goes into the high byte; without
 * it an over/underflow stops the move (1A01 = 0) and leaves the position. */
static void axis_step(u16 lo, u8 v, bool wrap, u8 o)
{
    u8 a;
    if (v & 0x80) {
        u8 d = (u8)-v;
        dp8_set(0xD9, d);
        g_cpu._flag_C = 1;
        a = dc_sbc8(abs8(lo + o), d);
    } else {
        g_cpu._flag_C = 0;
        a = dc_adc8(v, abs8(lo + o));
    }
    dc_set_a8(a);
    bool out = (v & 0x80) ? !g_cpu._flag_C : g_cpu._flag_C;
    if (!out) { abs8_set(lo + o, a); return; }
    if (!wrap) { abs8_set(0x1A01 + o, 0); return; }
    abs8_set(lo + o, a);
    u8 h = (u8)(abs8(lo + 1 + o) + ((v & 0x80) ? -1 : 1));
    abs8_set(lo + 1 + o, h);
    dc_set_flags_nz8(h);
}

/* C0:AA07  Like C0:AAFD, but 7F:0B00 (per object) picks the mode: 0 keeps
 * the object inside its 256-pixel page (a step that would cross it ends the
 * move), non-zero lets it cross; a positive 7F:0B00 is a one-shot and is
 * cleared. */
DC_ENTRY(C0AA07, RTS)
{
    u8 o = dp8(0x6D);
    g_cpu.X = o;
    if (abs8(0x1B80 + o)) { step_frac(o); return; }
    u8 n = abs8(0x1A01 + o);
    dc_set_a8(n);
    dc_set_flags_nz8(n);
    if (!n) return;
    n--;
    abs8_set(0x1A01 + o, n);
    u8 m = ram8(0x10B00 + o);
    if (m && !(m & 0x80)) ram8_set(0x10B00 + o, 0);
    axis_step(0x1800, abs8(0x1900 + o), m != 0, o);
    axis_step(0x1880, abs8(0x1980 + o), m != 0, o);
}

/* C0:1CFC  Find an object the leader is facing / touching: run test A & 6
 * of the table at C0:1D56 (C0:1D84 ...) on each object from 7F:2000 down
 * to 1 that is on screen (0F00), present and not the leader (kind 7 objects
 * are tested even when they are the leader). C set with the object in dp
 * EB, else dp EB = 80 and C clear. 16-bit index registers on return. */
DC_ENTRY(C01CFC, RTS)
{
    dc_set_mx(0, 0);
    u16 t = g_cpu.A & 6;
    g_cpu.A = t;
    dp16_set(0xDB, t);
    dp16_set(0xEB, 0x0080);
    u16 l = dp16(0x97);
    g_cpu.X = l;
    dp16_set(0xC3, abs16(0x1800 + l));
    u16 ly = abs16(0x1880 + l);
    g_cpu.A = ly;
    dp16_set(0xC5, ly);
    dc_set_mx(1, 1);
    u8 n = ram8(0x12000);
    g_cpu._flag_C = n >> 7;
    u8 x = (u8)(n << 1);
    dc_set_a8(x);
    for (;;) {
        g_cpu.X = x;
        u8 v = abs8(0x0F00 + x);
        dc_set_a8(v);
        if (v) {
            u8 k = abs8(0x1100 + x);
            dc_set_a8(k);
            if (!(k & 0x80)) {
                dc_cmp8(k, 7);
                bool test = k == 7;
                if (!test) { dc_cmp8(x, dp8(0x97)); test = x != dp8(0x97); }
                if (test) {
                    dp8_set(0xEB, x);
                    u8 i = dp8(0xDB);
                    g_cpu.X = i;
                    dc_jsr(0xC00000 | bus16(0xC01D56 + i), k == 7 ? 0xC01D4C : 0xC01D3E);
                    x = dp8(0xEB);
                    g_cpu.X = x;
                    if (g_cpu._flag_C) { dc_set_mx(1, 0); return; }
                }
            }
        }
        x = (u8)(x - 2);
        g_cpu.X = x;
        if (!x) break;
    }
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
    dp8_set(0xEB, 0x80);
    dc_set_mx(1, 0);
    g_cpu._flag_C = 0;
}

/* C0:A66B  Build the map's exit table 7E:7080 (rows of 0x100 bytes, 80 =
 * none): for exit list E5:0000[0100 & 1FF] (7-byte entries) mark each
 * exit's run of tiles with its index. Byte 2 of an entry is the run length
 * - 1, going right (wrapping inside the row: only the low byte of the tile
 * is masked with dp 1E then) or, with bit 7, going down. */
DC_ENTRY(C0A66B, RTS)
{
    for (u16 x = 0; x < 0x40; x++) ram8_set(0x7080 + x, 0x80);
    g_cpu.X = 0x40;
    dc_set_a8(0x80);
    dc_set_mx(0, 0);
    g_cpu.Y = 0xAF80;
    do {
        g_cpu.X = 0x7080;
        g_cpu.A = 0x3F;
        dc_push8(g_cpu.DB);
        dc_mvn(0x7E, 0x7E);
        g_cpu.DB = dc_pull8();
        g_cpu._flag_C = 1;
        g_cpu.A = g_cpu.Y = dc_sbc16(g_cpu.Y, 0x0140);
        dc_cmp16(g_cpu.Y, 0x7080);
    } while (g_cpu.Y != 0x7080);
    u16 i = (u16)((abs16(0x0100) & 0x01FF) << 1);
    u16 x = bus16(0xE50000 + i);
    dp16_set(0x04, x);
    u16 end = bus16(0xE50002 + i);
    dp16_set(0x56, end);
    g_cpu.X = x;
    dp16_set(0x54, 0);
    for (;;) {
        dc_cmp16(x, dp16(0x56));
        if (x >= dp16(0x56)) break;
        u8 n = bus8(0xE50002 + x);
        bool down = n & 0x80;
        u16 y = down ? (n & 0x7F) : n;
        u16 a = bus16(0xE50000 + x);
        dp16_set(0x52, x);
        u16 m = dp16(0x1E);
        a &= m;
        for (;;) {
            x = a;
            g_cpu.X = x;
            u8 idx = dp8(0x54);
            g_cpu.A = (u16)((a & 0xFF00) | idx);
            ram8_set(0x7080 + x, idx);
            if (!y) break;
            y--;
            if (down) a = (u16)((x + 0x100) & m);
            else a = (u16)((a & 0xFF00) | ((u8)(x + 1) & (u8)m));
        }
        g_cpu.Y = 0;
        u8 n2 = (u8)(dp8(0x54) + 1);
        dp8_set(0x54, n2);
        g_cpu._flag_C = 0;
        x = dc_adc16(dp16(0x52), 7);
        g_cpu.A = g_cpu.X = x;
    }
    dc_set_mx(1, 0);
}

/* C0:CB3A  Fetch object X's current animation frame: [dp D6] (bank dp D8
 * set by the caller) = 1500 + 1480 * facing slot (0, 1, 2 or 3; any facing
 * >= 3 counts as 3) + 4 * animation + frame (1681). Animation 1680, or 1781
 * while the lock 1780 is 2. Frame FF ends the animation: a locked one
 * counts 7F:0B01 down (leaving it at 1) and returns C clear; otherwise the
 * frame goes back to 0 (1681 = FF) and the first frame is returned. C set
 * with the frame in A otherwise. */
static u8 frame_at(void) { return bus8((u32)dp8(0xD8) << 16 | dp16(0xD6)); }
DC_ENTRY(C0CB3A, RTS)
{
    u16 x = g_cpu.X;
    u8 dir = abs8(0x1600 + x);
    dc_set_a8(dir);
    dc_set_mx(0, 1);
    u16 step = abs16(0x1480 + x), base = abs16(0x1500 + x), d6;
    if (dir == 0) d6 = base;
    else if (dir == 2) { g_cpu._flag_C = step >> 15; g_cpu._flag_C = 0; d6 = dc_adc16((u16)(step << 1), base); }
    else if ((s8)(dir - 2) < 0) { g_cpu._flag_C = 0; d6 = dc_adc16(step, base); }
    else {
        dp16_set(0xD9, step);
        g_cpu._flag_C = 0;
        u16 a = dc_adc16(step, step);
        a = dc_adc16(a, step);
        d6 = dc_adc16(a, base);
    }
    g_cpu.A = d6;
    dp16_set(0xD6, d6);
    u16 fr = abs16(0x1681 + x) & 0xFF;
    dp16_set(0xD9, fr);
    u16 lock = abs16(0x1780 + x) & 0xFF;
    dc_cmp16(lock, 2);
    bool locked = lock == 2;
    u16 an = abs16((locked ? 0x1781 : 0x1680) + x) & 0xFF;
    g_cpu._flag_C = 0;
    u16 a = dc_adc16((u16)(an << 2), dp16(0xD9));
    a = dc_adc16(a, dp16(0xD6));
    g_cpu.A = a;
    dp16_set(0xD6, a);
    dc_set_mx(1, 1);
    u8 v = frame_at();
    dc_set_a8(v);
    dc_cmp8(v, 0xFF);
    if (v != 0xFF) { g_cpu._flag_C = 1; return; }
    if (locked) {
        u8 t = (u8)(ram8(0x10B01 + x) - 1);
        dc_set_a8(t);
        dc_set_flags_nz8(t);
        if (t) ram8_set(0x10B01 + x, t);
        g_cpu._flag_C = 0;
        return;
    }
    dc_set_mx(0, 1);
    g_cpu._flag_C = 1;
    u16 back = dc_sbc16(dp16(0xD6), dp16(0xD9));
    g_cpu.A = back;
    dp16_set(0xD6, back);
    dc_set_mx(1, 1);
    abs8_set(0x1681 + x, 0xFF);
    v = frame_at();
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    g_cpu._flag_C = 1;
}

/* C0:28F9  (16-bit) Open the closed door / chest drawn at layer 1 tile A
 * (7E:3000, rows of 0x100): its tile code E6 / EC / EE / FA / FC becomes
 * the next code and the tiles above / beside it switch to the open
 * picture. Steps: '+' / '-' next / previous column, 'u' the row above
 * (SBC #$100), otherwise a tile code written at the current tile. */
static const struct { u8 code; const char *steps; } door_open[] = {
    { 0xE6, "u\xE7" },
    { 0xEC, "+\xEF" "u\xEB" "-\xE9" },
    { 0xEE, "-\xED" "u\xE9" "+\xEB" },
    { 0xFA, "+\xFD" "u\xF9" "-\xF7" "u\xF3" "+\xF5" },
    { 0xFC, "-\xFB" "u\xF7" "+\xF9" "u\xF5" "-\xF3" },
};
DC_ENTRY(C028F9, RTS)
{
    dc_push16(g_cpu.X);
    u16 x = g_cpu.A;
    g_cpu.X = x;
    dc_set_mx(1, 0);
    u8 c = ram8(0x3000 + x);
    dc_set_a8(c);
    for (unsigned i = 0; i < sizeof door_open / sizeof door_open[0]; i++) {
        dc_cmp8(c, door_open[i].code);
        if (c != door_open[i].code) continue;
        c++;
        dc_set_a8(c);
        ram8_set(0x3000 + x, c);
        for (const char *s = door_open[i].steps; *s; s++) {
            if (*s == '+') x++;
            else if (*s == '-') x--;
            else if (*s == 'u') { g_cpu._flag_C = 1; x = dc_sbc16(x, 0x0100); }
            else { dc_set_a8((u8)*s); ram8_set(0x3000 + x, (u8)*s); }
        }
        g_cpu.X = x;
        break;
    }
    dc_set_mx(0, 0);
    g_cpu.X = dc_pull16();
    dc_set_flags_nz16(g_cpu.X);
}

/* C0:28E1  Open every door listed in 7F:1CC8 (up to 16 tile words, ending
 * at a negative one) with C0:28F9. */
DC_ENTRY(C028E1, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    u16 x = 0;
    for (;;) {
        g_cpu.X = x;
        u16 t = ram16(0x11CC8 + x);
        g_cpu.A = t;
        dc_set_flags_nz16(t);
        if (t & 0x8000) break;
        dc_jsr(0xC028F9, 0xC028EC);
        x = (u16)(g_cpu.X + 2);
        g_cpu.X = x;
        dc_cmp16(x, 0x20);
        if (x >= 0x20) break;
    }
    dc_set_mx(1, g_cpu.x_flag);
}

/* C0:C98A  Tick the current object's animation: count down the frame time
 * 1601; when it runs out, queue the object for a sprite redraw (party
 * objects, kind 0 or kinds 1/2 when dp 78 = 2 (clearing it), on the list
 * headed by dp 76; others on the list whose tail is dp 77; links 1081;
 * an empty list starts with the object as both 76 and 77), step the frame
 * 1681 and load the next frame time from E4:[1580 + 4 * animation + frame]
 * (animation 1781 while the lock 1780 is 2 or more). A zero time ends the
 * animation: the first frame's time is used, and a locked animation counts
 * its plays down in 7F:0B01 (stopping on the last frame at 1). */
static void inc_frame(u8 o)
{
    u8 f = (u8)(abs8(0x1681 + o) + 1);
    abs8_set(0x1681 + o, f);
    dc_set_flags_nz8(f);
}
static void queue_tail(u8 me)
{
    u8 t = dp8(0x77);
    if (t >= 0x80) { dp8_set(0x77, me); dp8_set(0x76, me); }
    else { abs8_set(0x1081 + t, me); dp8_set(0x77, me); }
    g_cpu.X = me;
    dc_set_a8(me);
    inc_frame(me);
}
DC_ENTRY(C0C98A, RTS)
{
    u8 me = dp8(0x6D);
    g_cpu.X = me;
    u8 t = abs8(0x1601 + me);
    dc_set_a8(t);
    dc_set_flags_nz8(t);
    if (t) { t--; abs8_set(0x1601 + me, t); dc_set_flags_nz8(t); if (t) return; }
    u8 k = abs8(0x1100 + me);
    dc_set_a8(k);
    dc_set_flags_nz8(k);
    bool head = false;
    if (k == 0) head = true;
    else if (k & 0x80) return;
    else if (k == 1 || k == 2) {
        dc_cmp8(k, k);
        u8 s = dp8(0x78);
        dc_set_a8(s);
        dc_cmp8(s, 2);
        if (s == 2) { dp8_set(0x78, 0); head = true; }
    } else dc_cmp8(k, 2);
    if (head) {
        dc_cmp8(me, dp8(0x76));
        if (me == dp8(0x76)) return;
        u8 h = dp8(0x76);
        dc_set_a8(h);
        if (h & 0x80) { dp8_set(0x77, me); dp8_set(0x76, me); dc_set_a8(me); }
        else { abs8_set(0x1081 + me, h); dp8_set(0x76, me); }
        g_cpu.X = me;
        inc_frame(me);
    } else {
        dc_cmp8(me, dp8(0x77));
        if (me == dp8(0x77)) return;
        u8 tl = dp8(0x77);
        dc_set_a8(tl);
        if (!(tl & 0x80)) {
            u8 l = abs8(0x1081 + me);
            dc_set_a8(l);
            dc_set_flags_nz8(l);
            if (!(l & 0x80)) return;
        }
        queue_tail(me);
    }
    g_cpu.X = me;
    u8 lk = abs8(0x1780 + me);
    u8 an = (lk == 0 || lk == 1) ? abs8(0x1680 + me) : abs8(0x1781 + me);
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    u16 c1 = dc_adc16((u16)(an << 2), abs16(0x1580 + me));
    dp16_set(0xC1, c1);
    u16 sum = dc_adc16(abs16(0x1681 + me) & 0xFF, c1);
    dc_set_mx(0, 0);
    g_cpu.X = sum;
    dc_set_mx(1, 0);
    u8 v = bus8(0xE40000 + sum);
    g_cpu.A = (u16)((sum & 0xFF00) | v);
    dc_set_flags_nz8(v);
    if (v) {
        dc_set_mx(1, 1);
        g_cpu.X = me;
        abs8_set(0x1601 + me, v);
        return;
    }
    v = bus8(0xE40000 + dp16(0xC1));
    dc_set_a8(v);
    dc_set_mx(1, 1);
    g_cpu.X = me;
    abs8_set(0x1601 + me, v);
    lk = abs8(0x1780 + me);
    dc_set_a8(lk);
    dc_cmp8(lk, 2);
    if (lk != 2) { abs8_set(0x1681 + me, 0); return; }
    u8 n = ram8(0x10B01 + me);
    n--;
    dc_set_a8(n); dc_set_flags_nz8(n);
    if (n) {
        n--;
        dc_set_a8(n); dc_set_flags_nz8(n);
        if (n) { ram8_set(0x10B01 + me, n); abs8_set(0x1681 + me, 0); return; }
        dc_set_a8(1); dc_set_flags_nz8(1);
        ram8_set(0x10B01 + me, 1);
    }
    u8 f = (u8)(abs8(0x1681 + me) - 1);
    abs8_set(0x1681 + me, f);
    dc_set_flags_nz8(f);
}

/* C0:9AD3  Which half of a sloped/split tile a point is on. dp 58 bits 2..7
 * pick the tile shape, dp 66/68 are the point's position inside the tile,
 * dp 5A/5C the tile's attribute bytes. Shape 0 (or the point on the "low"
 * side of a diagonal) takes the low attributes: 6A = 5C&3, 6C = 5C&4;
 * shape 1 (or the "high" side) takes 6A = (5C>>3)&3, 6C = 5C&20. Both set
 * the layer priorities 6E/6F from bit 6 of 5A/5C. Shapes 2..17 are
 * diagonals: (66 >> n, or its complement when the table entry has bit 7)
 * + offset - 68/4, signed against a side mask (tables C0:9BF5/9C0B/9C21).
 * Shapes 18..1E are quadrant tests on the signs of 66/68; 1E also sets
 * 5E = 4 and clamps dp 30 to +-10. dp 60 = shape - 2. */
static void tile_side(bool high)
{
    u8 c = dp8(0x5C);
    if (high) { dp8_set(0x6A, (u8)((c & 0x18) >> 3)); dp8_set(0x6C, c & 0x20); }
    else      { dp8_set(0x6A, c & 3);                 dp8_set(0x6C, c & 4); }
    dp8_set(0x6E, (u8)(((dp8(0x5A) >> 2) & 0x10) | 0x20));
    u8 v = (u8)(((c >> 2) & 0x10) | 0x20);
    dp8_set(0x6F, v);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
    g_cpu._flag_C = (c >> 1) & 1;
}

DC_ENTRY(C09AD3, RTS)
{
    dp8_set(0x5E, 0);
    dp8_set(0x60, 0);
    dp8_set(0x6A, 0);
    dp8_set(0x6C, 0);
    dp8_set(0x6E, 0);
    dp8_set(0x56, dp8(0x68) >> 2);
    u8 hi = (u8)(g_cpu.A >> 8);
    u8 shape = (dp8(0x58) & 0xFC) >> 2;
    if (shape == 0) { tile_side(false); return; }
    if (shape == 1) { tile_side(true); return; }
    u8 k = (u8)(shape - 2);
    dp8_set(0x60, k);
    bool high;
    if (k < 0x16) {
        u16 x = g_cpu.x_flag ? k : (u16)(hi << 8 | k);
        g_cpu.X = x;
        u8 e = bus8(0xC09BF5 + x);
        u8 n8 = e & 0x80 ? e & 0x7F : e;
        u16 n = g_cpu.x_flag ? n8 : (u16)(hi << 8 | n8);
        u32 cnt = n ? n : (g_cpu.x_flag ? 0x100 : 0x10000);
        u8 s = cnt >= 8 ? 0 : (u8)(dp8(0x66) >> cnt);
        if (e & 0x80) s ^= 0xFF;
        g_cpu.Y = 0;
        g_cpu._flag_C = 0;
        u8 t = dc_adc8(bus8(0xC09C0B + x), s);
        g_cpu._flag_C = 1;
        t = dc_sbc8(t, dp8(0x56));
        dp8_set(0x54, t);
        high = (bus8(0xC09C21 + x) ^ t) & 0x80;
    } else {
        bool xp = !(dp8(0x66) & 0x80), yp = !(dp8(0x68) & 0x80);
        switch (k) {
        case 0x16: high = xp; break;
        case 0x17: high = yp; break;
        case 0x18: high = xp && !yp; break;
        case 0x19: high = !xp && !yp; break;
        case 0x1A: high = !xp && yp; break;
        case 0x1B: high = xp && yp; break;
        case 0x1C: {
            dp8_set(0x5E, 4);
            u8 v = dp8(0x30);
            if (v) dp8_set(0x30, v & 0x80 ? 0xF0 : 0x10);
            high = false;
            break;
        }
        default: high = false; break;
        }
    }
    tile_side(high);
}

/* C0:8A9E  Tile check at the map index in dp 67 (B = dp 69), masked by
 * dp 1E: A = 1 with C set for a tile without the 7E:70C0 flag; otherwise
 * the tile's attributes are loaded, the side test (C0:9AD3) and the move
 * check (C0:9C37, its C is the result) run and A = 0. 8-bit A on return. */
DC_ENTRY(C08A9E, RTS)
{
    g_cpu.A = dp16(0x68);
    dc_set_mx(1, g_cpu.x_flag);
    u8 i = dp8(0x67);
    dc_set_a8(i);
    dc_set_flags_nz8(i);
    dc_jsr(0xC09AA1, 0xC08AA4);
    if (g_cpu._flag_C) { dc_set_a8(1); dc_set_flags_nz8(1); return; }
    dc_jsr(0xC09AD3, 0xC08AAC);
    dc_jsr(0xC09C37, 0xC08AAF);
    dc_set_a8(0);
    dc_set_flags_nz8(0);
}

/* C0:9C5C  The leader's step on the field map: the next position
 * (dp 66 = 62 + 2E, dp 68 = 64 + 30; dp 70/72 the same one tile row
 * further in the move's vertical direction) is tested against the tile
 * (C0:9AC8, 9AD3, 9C37); a blocked step zeroes the speed dp 2E/30. Then:
 * - an exit tile (7E:7080 = index into the room's 7-byte exit records at
 *   E5:0000 + dp 04) whose record matches the position (same X and |dY| <
 *   3F, or same Y) latches the exit: 0112..0116 from the record,
 *   0117 bit 7;
 * - a touch-trigger tile (7E:7000 bit 4) at dp 71/73 sets 015B to its index
 *   and 0117 bit 4;
 * - on a plain tile the layer (0155) and priorities (0C00/0C01 of the
 *   leader 0197) follow the tile; dp 34 = side flags dp 5E, and a stairs
 *   tile (5A bits 2..3: speed 8 / 10 / 20) adds the matching slope to the
 *   move (dp 2A/2B by direction 5A bits 0..1, 34 |= 8). */
static void leader_step(void)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    u16 r = dc_adc16(dp16(0x62), dp16(0x2E));
    dp16_set(0x66, r);
    dp16_set(0x70, r);
    g_cpu._flag_C = 0;
    u16 ny = dc_adc16(dp16(0x64), dp16(0x30));
    dp16_set(0x68, ny);
    if (dp16(0x30) & 0x80) { g_cpu._flag_C = 1; dp16_set(0x72, dc_sbc16(ny, 0x0100)); }
    else                   { g_cpu._flag_C = 0; dp16_set(0x72, dc_adc16(ny, 0x0100)); }
    g_cpu.A = dp16(0x68);
    dc_set_mx(1, g_cpu.x_flag);
    dc_set_a8(dp8(0x67));
    dc_jsr(0xC09AC8, 0xC09C8D);
    dc_jsr(0xC09AD3, 0xC09C90);
    dc_jsr(0xC09C37, 0xC09C93);
    if (g_cpu._flag_C) {
        dc_set_mx(0, g_cpu.x_flag);
        dp16_set(0x2E, 0);
        dp16_set(0x30, 0);
        dc_set_mx(1, g_cpu.x_flag);
        return;
    }
    u8 f = abs8(0x0117);
    if (!(f & 0x80)) {
        dc_set_mx(1, 0);
        u16 t = (u16)((dp8(0x69) & dp8(0x1F)) << 8 | (dp8(0x67) & dp8(0x1E)));
        g_cpu.A = t;
        g_cpu.X = t;
        u8 e = ram8(0x7080 + t);
        dc_set_a8(e);
        if (e & 0x80) goto touch;
        abs8_set(0x4202, e);
        abs8_set(0x4203, 7);
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        u16 x = dc_adc16(abs16(0x4216), dp16(0x04));
        g_cpu.X = x;
        dc_set_mx(1, 0);
        u32 rec = 0xE50000 + x;
        u8 ex = bus8(rec);
        dc_set_a8(ex);
        dc_cmp8(ex, dp8(0x67));
        bool hit;
        if (ex == dp8(0x67)) {
            g_cpu._flag_C = 1;
            u8 d = dc_sbc8(bus8(rec + 1), dp8(0x69));
            if (d & 0x80) d = (u8)-d;
            dc_set_a8(d);
            dc_cmp8(d, 0x3F);
            hit = d < 0x3F;
        } else {
            u8 ey = bus8(rec + 1);
            dc_set_a8(ey);
            dc_cmp8(ey, dp8(0x69));
            hit = ey == dp8(0x69);
        }
        if (hit) {
            u8 b4 = bus8(rec + 4);
            dp8_set(0x52, b4);
            g_cpu._flag_C = b4 & 1;
            abs8_set(0x0116, (b4 >> 1) & 0x0F);
            abs8_set(0x0116, abs8(0x0116) | (b4 & 0x80));
            abs8_set(0x0113, b4 & 1);
            abs8_set(0x0112, bus8(rec + 3));
            abs8_set(0x0114, bus8(rec + 5));
            abs8_set(0x0115, bus8(rec + 6));
            abs8_set(0x0117, abs8(0x0117) | 0x80);
        }
    }
touch:;
    u16 t = (u16)((dp8(0x73) & dp8(0x1F)) << 8 | (dp8(0x71) & dp8(0x1E)));
    g_cpu.A = t;
    u16 xi = g_cpu.x_flag ? (t & 0xFF) : t;
    g_cpu.X = xi;
    u8 w = ram8(0x7000 + xi);
    dc_set_a8(w);
    if (w & 0x10) {
        if (g_cpu.x_flag) abs8_set(0x015B, (u8)xi); else abs16_set(0x015B, xi);
        u8 v = abs8(0x0117) | 0x10;
        dc_set_a8(v);
        abs8_set(0x0117, v);
    }
    dc_set_mx(1, 1);
    if (!dp8(0x6C)) {
        abs8_set(0x0155, dp8(0x6A));
        u8 l = abs8(0x0197);
        g_cpu.X = l;
        abs8_set(0x0C00 + l, dp8(0x6E));
        abs8_set(0x0C01 + l, dp8(0x6F));
    }
    dp8_set(0x34, dp8(0x5E));
    u8 st = dp8(0x5A) & 0x0C;
    dc_set_a8(st);
    dc_set_flags_nz8(st);
    if (!st) { dp8_set(0x2A, 0); dp8_set(0x2B, 0); return; }
    dc_cmp8(st, 4);
    if (st != 4) dc_cmp8(st, 8);
    u8 sp = st == 4 ? 0x08 : st == 8 ? 0x10 : 0x20;
    dp8_set(0x52, sp);
    dc_set_a8(8);
    u8 m = dp8(0x34);
    dp8_set(0x34, m | 8);
    u8 dir = dp8(0x5A) & 3;
    dc_set_a8(dir);
    u8 a;
    if (dir == 3)      { a = sp; dp8_set(0x2A, a); dp8_set(0x2B, 0); }
    else if (dir == 2) { g_cpu._flag_C = 1; a = dc_sbc8(0, sp); dp8_set(0x2A, a); dp8_set(0x2B, 0); }
    else if (dir == 1) { a = sp; dp8_set(0x2B, a); dp8_set(0x2A, 0); }
    else               { g_cpu._flag_C = 1; a = dc_sbc8(0, sp); dp8_set(0x2B, a); dp8_set(0x2A, 0); }
    dc_set_a8(a);
    dc_set_flags_nz8(a);
}
DC_ENTRY(C09C5C, RTS) { leader_step(); }

/* C0:9E84  Pick the animation of object dp 6D from its movement 1D2C/1D2D
 * (dX/dY): when 1D34 has bit 2 set, a moving object (1D2D != 0) gets
 * animation 1B (restarted only when it changes) and a still one frame 0;
 * otherwise (dp E5 = 1 when 1D34 is non-zero) the handler at C0:9FF7 is
 * picked by the size of the movement, (|dX| >> 3 | |dY|) & 7E, where
 * |v| of a negative v is ~v + 21. */
static u8 move_mag(u8 v)
{
    if (v & 0x80) { g_cpu._flag_C = 0; v = dc_adc8((u8)~v, 0x21); }
    return v;
}
DC_ENTRY(C09E84, RTS)
{
    dp8_set(0xE5, 0);
    u8 o = dp8(0x6D);
    g_cpu.Y = o;
    u8 f = abs8(0x1D34);
    if (f) {
        if (!(f & 4)) {
            dp8_set(0xE5, 1);
        } else {
            u8 d = abs8(0x1D2D);
            if (!d) {
                dc_set_a8(0);
                dc_set_flags_nz8(0);
                abs8_set(0x1681 + o, 0);
                abs8_set(0x1601 + o, 0);
                return;
            }
            u8 m = d & 0x80 ? (u8)-d : d;
            dc_cmp8(m, 0x18);
            u8 a = abs8(0x1680 + o);
            dc_set_a8(a);
            dc_cmp8(a, 0x1B);
            if (a != 0x1B) {
                abs8_set(0x1680 + o, 0x1B);
                abs8_set(0x1681 + o, 0);
                abs8_set(0x1601 + o, 0);
                dc_set_a8(0);
                dc_set_flags_nz8(0);
            }
            return;
        }
    }
    u8 mx = move_mag(abs8(0x1D2C));
    g_cpu._flag_C = (mx >> 2) & 1;
    mx >>= 3;
    dp8_set(0xDB, mx);
    u8 x = (u8)((move_mag(abs8(0x1D2D)) | mx) & 0x7E);
    dc_set_a8(x);
    dc_set_flags_nz8(x);
    g_cpu.X = x;
    dc_jsr(0xC00000 | bus16(0xC09FF7 + x), 0xC09ECD);
}

/* C0:9E29  The leader's movement for this frame: pick the animation
 * (C0:9E84), then with a non-zero speed 1D32/1D33 set the leader's
 * velocity 1900/1980 and record the step in the 128-entry trail at 7F:0C00
 * (dp AB = newest; speed, 1D2C/1D2D, priorities 0C00/0C01, 1D34) that the
 * followers replay; without speed the velocity is zeroed. */
DC_ENTRY(C09E29, RTS)
{
    dc_jsr(0xC09E84, 0xC09E29);
    u8 vx = abs8(0x1D32), vy = abs8(0x1D33);
    if (!vx && !vy) {
        dc_set_a8(0);
        dc_set_flags_nz8(0);
        u8 o = dp8(0x6D);
        g_cpu.X = o;
        abs8_set(0x1900 + o, 0);
        abs8_set(0x1980 + o, 0);
        return;
    }
    u8 i = (u8)((dp8(0xAB) + 1) & 0x7F);
    dp8_set(0xAB, i);
    g_cpu.X = i;
    u8 o = dp8(0x6D);
    g_cpu.Y = o;
    ram8_set(0x10C00 + i, vx);
    abs8_set(0x1900 + o, vx);
    ram8_set(0x10C80 + i, vy);
    abs8_set(0x1980 + o, vy);
    ram8_set(0x10D00 + i, abs8(0x1D2C));
    ram8_set(0x10D80 + i, abs8(0x1D2D));
    ram8_set(0x10E00 + i, abs8(0x0C00 + o));
    ram8_set(0x10E80 + i, abs8(0x0C01 + o));
    u8 f = abs8(0x1D34);
    ram8_set(0x10F00 + i, f);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
}

/* Size of a trail movement for the C0:9FF7 handler index (as C0:9E84). */
static u8 trail_index(u8 dx, u8 dy)
{
    u8 mx = move_mag(dx);
    g_cpu._flag_C = (mx >> 2) & 1;
    mx >>= 3;
    dp8_set(0xDB, mx);
    u8 x = (u8)((move_mag(dy) | mx) & 0x7E);
    dc_set_a8(x);
    dc_set_flags_nz8(x);
    return x;
}
/* C0:9ED1 / 9F20 (shared tails of C0:A26B / A2CE)  C0:9E84 for a follower
 * replaying trail entry X (7F:0D00/0D80 = dX/dY, 7F:0F00 = flags). */
static void trail_anim(u8 x, u16 site)
{
    dp8_set(0xE5, 0);
    u8 o = dp8(0x6D);
    g_cpu.Y = o;
    u8 f = ram8(0x10F00 + x);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (f && (f & 4)) {
        u8 d = ram8(0x10D80 + x);
        dc_set_a8(d);
        dc_set_flags_nz8(d);
        if (!d) {
            dc_set_a8(0);
            dc_set_flags_nz8(0);
            abs8_set(0x1681 + o, 0);
            abs8_set(0x1601 + o, 0);
            return;
        }
        dc_cmp8(d & 0x80 ? (u8)-d : d, 0x18);
        set_walk_anim(o, 0x1B);
        return;
    }
    u8 i = trail_index(ram8(0x10D00 + x), ram8(0x10D80 + x));
    g_cpu.X = i;
    dc_jsr(0xC00000 | bus16(0xC09FF7 + i), 0xC00000 | site);
}
/* One axis of C0:9F6F: the follower's offset from the trail step: the
 * step's speed v (7F:0C00/0C80) rounded away from 0 by 8 when bit 3 is
 * set, negated, plus the trail movement d. */
static u8 lag_axis(u8 v, u8 d)
{
    if (v & 8) {
        if (v & 0x80) { g_cpu._flag_C = 1; v = dc_sbc8(v, 8); }
        else          { g_cpu._flag_C = 0; v = dc_adc8(v, 8); }
    }
    u8 a = (u8)-v;
    g_cpu._flag_C = 0;
    return dc_adc8(a, d);
}
/* C0:9F6F (shared tail of C0:A26B / A2CE)  A follower that is still
 * catching up with trail entry X: idle (C0:9FF2) on an empty entry; on a
 * stairs entry (bit 2) stop the animation unless both dY values are set
 * and differ; otherwise the handler by the remaining distance. */
static void trail_anim_lag(u8 x)
{
    dp8_set(0xE5, 0);
    u8 f = ram8(0x10F00 + x);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (!f) { idle_anim(); return; }
    u16 y = g_cpu.Y;
    if (f & 4) {
        u8 d = ram8(0x10D80 + x);
        u8 v = ram8(0x10C80 + x);
        bool stop = !d || !v;
        if (!stop) {
            g_cpu._flag_C = 1;
            u8 r = dc_sbc8(v, d);
            dc_set_a8(r);
            stop = !r;
        }
        if (stop) {
            dc_set_a8(0);
            dc_set_flags_nz8(0);
            abs8_set(0x1681 + y, 0);
            abs8_set(0x1601 + y, 0);
        }
        return;
    }
    u8 mx = move_mag(lag_axis(ram8(0x10C00 + x), ram8(0x10D00 + x)));
    g_cpu._flag_C = (mx >> 2) & 1;
    mx >>= 3;
    dp8_set(0xDB, mx);
    u8 i = (u8)((move_mag(lag_axis(ram8(0x10C80 + x), ram8(0x10D80 + x))) | mx) & 0x7E);
    dc_set_a8(i);
    dc_set_flags_nz8(i);
    g_cpu.X = i;
    dc_jsr(0xC00000 | bus16(0xC09FF7 + i), 0xC09FCF);
}
/* C0:A26B / A2CE  Move follower 1 / 2 (object dp 99 / 9B) along the
 * leader's trail: it stays 18 steps behind while the party moves (10 when
 * standing), reading the next entry (dp AC / AD, following dp AB / AC)
 * for its velocity and priorities; otherwise it stands. Then its
 * animation from the entry. */
static void follow(u8 obj_dp, u8 lead_dp, u8 me_dp, bool rezero, u16 anim_site)
{
    u8 y = dp8(obj_dp);
    g_cpu.Y = y;
    dc_set_a8(0);
    abs8_set(0x1900 + y, 0);
    abs8_set(0x1980 + y, 0);
    u8 moving = abs8(0x1D32) | abs8(0x1D33);
    dc_set_a8(moving);
    g_cpu._flag_C = 1;
    u8 d = (u8)(dc_sbc8(dp8(lead_dp), dp8(me_dp)) & 0x7F);
    dc_set_a8(d);
    u8 lag = moving ? 0x18 : 0x10;
    dc_cmp8(d, lag);
    if (d < lag) {
        if (!moving && rezero) {
            g_cpu.Y = y;
            dc_set_a8(0);
            abs8_set(0x1900 + y, 0);
            abs8_set(0x1980 + y, 0);
        }
        u8 x = dp8(me_dp);
        g_cpu.X = x;
        trail_anim_lag(x);
        return;
    }
    u8 x = (u8)((dp8(me_dp) + 1) & 0x7F);
    dp8_set(me_dp, x);
    g_cpu.X = x;
    g_cpu.Y = y;
    u8 vx = ram8(0x10C00 + x), vy = ram8(0x10C80 + x);
    if (vx) abs8_set(0x1900 + y, vx);
    if (!vx || vy) abs8_set(0x1980 + y, vy);
    abs8_set(0x0C00 + y, ram8(0x10E00 + x));
    abs8_set(0x0C01 + y, ram8(0x10E80 + x));
    trail_anim(x, anim_site);
}
DC_ENTRY(C0A26B, RTS) { follow(0x99, 0xAB, 0xAC, false, 0x9F1C); }
DC_ENTRY(C0A2CE, RTS) { follow(0x9B, 0xAC, 0xAD, true, 0x9F6B); }

/* ---- C0:8A6D  The leader's step against walls, by direction of the speed
 * dp 2E (x) / dp 30 (y): the box around the leader (x 62 +-70, y 64-70..64)
 * is tested at the corners ahead with C0:8A9E; a blocked corner either
 * stops that axis or slides the leader by 10 along the wall when the other
 * corner is free, and a free step goes through C0:9C5C. Objects in the way
 * (C0:9923) stop the step. The probe arithmetic keeps the original's carry
 * handling (some sums add the carry of the previous step), so a few probes
 * are one unit off. C0:9923 returns with 16-bit index registers, so a
 * move that falls back to a straight one reloads the leader 16-bit. */
static u16 clc_adc(u16 a, u16 b) { g_cpu._flag_C = 0; return dc_adc16(a, b); }
static u16 sec_sbc(u16 a, u16 b) { g_cpu._flag_C = 1; return dc_sbc16(a, b); }
static u16 adc_c(u16 a, u16 b) { return dc_adc16(a, b); }
static u16 put(u8 o, u16 v) { dp16_set(o, v); g_cpu.A = v; return v; }
static void zero(u8 o) { dp16_set(o, 0); }

enum { XR_ADC, XL_CLC, XL_ADC, XR_CLC };          /* x probes: 62 +-70 + 2E */
static void probe_x(int how)
{
    u16 x = dp16(0x62), s = dp16(0x2E);
    switch (how) {
    case XR_ADC: put(0x66, adc_c(clc_adc(x, 0x70), s)); break;
    case XL_CLC: put(0x66, clc_adc(sec_sbc(x, 0x70), s)); break;
    case XL_ADC: put(0x66, adc_c(sec_sbc(x, 0x70), s)); break;
    default:     put(0x66, clc_adc(clc_adc(x, 0x70), s)); break;
    }
}
enum { YT, YB, YBT };                              /* y probes with 30 */
static void probe_y(int how)
{
    u16 y = dp16(0x64), s = dp16(0x30);
    if (how == YT) put(0x68, clc_adc(sec_sbc(y, 0x70), s));
    else if (how == YB) put(0x68, clc_adc(y, s));
    else put(0x68, sec_sbc(clc_adc(y, s), 0x70));
}

static bool hit(u16 site) { dc_set_mx(0, g_cpu.x_flag); dc_jsr(0xC08A9E, 0xC00000 | site); return g_cpu._flag_C; }
static void done(void) { dc_set_mx(1, g_cpu.x_flag); }
static void step(void) { leader_step(); }

/* Load the leader's position (62, 64) and the speed signs (2F, 31). */
static void wall_setup(u8 sx, u8 sy)
{
    dp8_set(0x2F, sx);
    dp8_set(0x31, sy);
    u16 x = g_cpu.x_flag ? abs8(0x0197) : abs16(0x0197);   /* 16-bit after C0:9923 */
    g_cpu.X = x;
    dc_set_mx(0, g_cpu.x_flag);
    put(0x62, abs16(0x1800 + x));
}
static void wall_setup_y(void) { put(0x64, abs16(0x1880 + g_cpu.X)); }
static bool blocked_by_object(u16 site)
{
    dc_set_mx(1, g_cpu.x_flag);
    dc_jsr(0xC09923, 0xC00000 | site);
    return g_cpu._flag_C;
}

/* Moving along x only (C0:8AE4): test x ahead (66) at y 64 and 64-70. */
static void wall_x(void)
{
    dc_set_mx(0, g_cpu.x_flag);
    put(0x68, dp16(0x64));
    if (!hit(0x8AEA)) {
        put(0x68, sec_sbc(dp16(0x64), 0x70));
        if (!hit(0x8AF9)) { dc_jsr(0xC09C5C, 0xC08AFE); return; }
        /* top corner blocked: slide down */
        put(0x30, 0x0010);
        put(0x68, clc_adc(0x0010, dp16(0x64)));
        if (hit(0x8B59)) { zero(0x2E); zero(0x30); done(); return; }
        probe_y(YT);
        if (!hit(0x8B77)) { step(); return; }
        put(0x66, sec_sbc(dp16(0x66), dp16(0x2E)));
        zero(0x2E);
        put(0x68, clc_adc(dp16(0x64), dp16(0x30)));
        if (hit(0x8BB5)) { zero(0x30); done(); return; }
        step();
        return;
    }
    if (!g_cpu._flag_Z) { zero(0x2E); done(); return; }
    put(0x68, sec_sbc(dp16(0x64), 0x70));
    if (hit(0x8B0E)) { zero(0x2E); done(); return; }
    /* bottom corner blocked: slide up */
    put(0x30, 0xFFF0);
    put(0x68, clc_adc(0xFFF0, dp16(0x64)));
    if (!hit(0x8B26)) {
        if (!g_cpu._flag_Z) { zero(0x2E); done(); return; }
        probe_y(YT);
        if (hit(0x8B3C)) { zero(0x2E); zero(0x30); done(); return; }
        step();
        return;
    }
    put(0x66, sec_sbc(dp16(0x66), dp16(0x2E)));
    zero(0x2E);
    probe_y(YT);
    if (hit(0x8B94)) { zero(0x30); done(); return; }
    step();
}

/* Moving along y only (C0:8C25, copied at 8D35 = +110): test y ahead (68)
 * at x 62-70 and 62+70. */
static void wall_y(u16 o)
{
    dc_set_mx(0, g_cpu.x_flag);
    put(0x66, sec_sbc(dp16(0x62), 0x70));
    if (!hit(0x8C2F + o)) {
        put(0x66, clc_adc(dp16(0x62), 0x70));
        if (!hit(0x8C3E + o)) { step(); return; }
        if (!g_cpu._flag_Z) { zero(0x30); done(); return; }
        /* right corner blocked: slide left */
        put(0x2E, 0xFFF0);
        put(0x66, adc_c(clc_adc(0xFFF0, dp16(0x62)), 0x70));
        if (!hit(0x8CA3 + o)) {
            probe_x(XL_CLC);
            if (hit(0x8CB7 + o)) { zero(0x2E); zero(0x30); done(); return; }
            step();
            return;
        }
        put(0x68, sec_sbc(dp16(0x68), dp16(0x30)));
        zero(0x30);
        probe_x(XL_CLC);
        if (hit(0x8D02 + o)) { zero(0x2E); done(); return; }
        step();
        return;
    }
    if (!g_cpu._flag_Z) { zero(0x30); done(); return; }
    put(0x66, clc_adc(dp16(0x62), 0x70));
    if (hit(0x8C52 + o)) { zero(0x30); done(); return; }
    /* left corner blocked: slide right */
    put(0x2E, 0x0010);
    put(0x66, sec_sbc(clc_adc(0x0010, dp16(0x62)), 0x70));
    if (!hit(0x8C6E + o)) {
        probe_x(XR_ADC);
        if (hit(0x8C81 + o)) { zero(0x2E); zero(0x30); done(); return; }
        step();
        return;
    }
    put(0x68, sec_sbc(dp16(0x68), dp16(0x30)));
    zero(0x30);
    probe_x(XR_ADC);
    if (hit(0x8CDD + o)) { zero(0x2E); done(); return; }
    step();
}

static void walk_right(void)                      /* 8AB5 */
{
    wall_setup(0, 0);
    probe_x(XR_ADC);
    wall_setup_y();
    put(0x68, sec_sbc(dp16(0x64), 0x40));
    if (blocked_by_object(0x8AD8)) { dc_set_mx(0, g_cpu.x_flag); zero(0x2E); done(); return; }
    wall_x();
}
static void walk_left(void)                       /* 8BC4 */
{
    wall_setup(0xFF, 0);
    put(0x66, sec_sbc(clc_adc(dp16(0x62), dp16(0x2E)), 0x70));
    wall_setup_y();
    put(0x68, sec_sbc(dp16(0x64), 0x40));
    if (blocked_by_object(0x8BEA)) { dc_set_mx(0, g_cpu.x_flag); zero(0x2E); done(); return; }
    wall_x();
}
static void walk_up(void)                         /* 8BF9 */
{
    wall_setup(0, 0xFF);
    put(0x66, dp16(0x62));
    wall_setup_y();
    probe_y(YT);
    if (blocked_by_object(0x8C1B)) { dp8_set(0x30, 0); dp8_set(0x31, 0); return; }
    wall_y(0);
}
static void walk_down(void)                       /* 8D11 */
{
    wall_setup(0, 0);
    put(0x66, dp16(0x62));
    wall_setup_y();
    probe_y(YB);
    if (blocked_by_object(0x8D2D)) { dp8_set(0x30, 0); return; }
    wall_y(0x110);
}

/* Diagonals: corner ahead (x0, y0), then the other x corner on the same
 * row (xb), then the far row at the front x (x3, y3). A blocked far row
 * turns the move into the straight one along y (2E = 0), a blocked row
 * into the one along x (30 = 0) unless the front corner of the far row is
 * blocked too. */
typedef struct {
    u8 sx, sy;
    int x0, y0, xb, x3, y3, xp;
    u16 site[7];                  /* 9923, h1, hb(free), h3(free), hb(blocked), h3(blocked), hp */
    void (*along_y)(void), (*along_x)(void);
} Diag;

static void walk_diag(const Diag *d)
{
    wall_setup(d->sx, d->sy);
    probe_x(d->x0);
    wall_setup_y();
    probe_y(d->y0);
    if (blocked_by_object(d->site[0])) { dc_set_mx(0, g_cpu.x_flag); zero(0x30); zero(0x2E); done(); return; }
    dc_set_mx(0, g_cpu.x_flag);
    bool first = hit(d->site[1]);
    probe_x(d->xb);
    if (!hit(d->site[first ? 4 : 2])) {
        probe_x(d->x3);
        probe_y(d->y3);
        if (hit(d->site[first ? 5 : 3])) { zero(0x2E); done(); d->along_y(); return; }
        if (!first) { step(); return; }
        zero(0x2E); zero(0x30); done();
        return;
    }
    probe_x(d->xp);
    probe_y(d->y3);
    if (!hit(d->site[6])) { zero(0x30); done(); d->along_x(); return; }
    zero(0x30); zero(0x2E); done();
}

static const Diag k_right_up   = { 0x00, 0xFF, XR_ADC, YT, XL_CLC, XR_ADC, YB,  XR_ADC,
    { 0x8E49, 0x8E59, 0x8E6B, 0x8E83, 0x8E98, 0x8EB0, 0x8EDA }, walk_up, walk_right };
static const Diag k_left_up    = { 0xFF, 0xFF, XL_CLC, YT, XR_ADC, XL_ADC, YB,  XL_CLC,
    { 0x8F1A, 0x8F2A, 0x8F3B, 0x8F53, 0x8F67, 0x8F7F, 0x8FAA }, walk_up, walk_left };
static const Diag k_left_down  = { 0xFF, 0x00, XL_CLC, YB, XR_ADC, XL_ADC, YBT, XL_CLC,
    { 0x8FE6, 0x8FF6, 0x9007, 0x9023, 0x903A, 0x9056, 0x907C }, walk_down, walk_left };
static const Diag k_right_down = { 0x00, 0x00, XR_CLC, YB, XL_ADC, XR_ADC, YBT, XR_CLC,
    { 0x90BF, 0x90CF, 0x90E0, 0x90FC, 0x9113, 0x912F, 0x9155 }, walk_down, walk_right };

DC_ENTRY(C08A6D, RTS)
{
    u8 vy = dp8(0x30), vx = dp8(0x2E);
    if (vy == 0) {
        if (vx == 0) return;
        if (vx & 0x80) walk_left(); else walk_right();
    } else if (vy & 0x80) {
        if (vx == 0) walk_up();
        else walk_diag(vx & 0x80 ? &k_left_up : &k_right_up);
    } else {
        if (vx == 0) walk_down();
        else walk_diag(vx & 0x80 ? &k_left_down : &k_right_down);
    }
}

/* C0:881E  The field's per-frame movement, with the scroll state at 1D00
 * as direct page: camera setup (C0:88E5), the leader's input (C0:88EE,
 * unless 0162 locks it or 011F is off), the leader's step against walls
 * (C0:8A6D, when 0120 allows), then C0:9175, 99DE and the scroll update
 * (C0:91AC, 93E1). 01EB = 80: nothing touched yet. */
DC_ENTRY(C0881E, RTS)
{
    abs8_set(0x01EB, 0x80);
    dc_set_a8(0x80);
    dc_push16(g_cpu.D);
    g_cpu.A = 0x1D00;
    g_cpu.D = 0x1D00;
    dc_jsr(0xC088E5, 0xC0882C);
    dp8_set(0x2C, 0);
    dp8_set(0x2D, 0);
    u8 a = abs8(0x0162);
    dc_set_a8(a);
    if (!a) {
        a = abs8(0x011F);
        dc_set_a8(a);
        if (a) dc_jsr(0xC088EE, 0xC0883D);
    }
    a = abs8(0x0120);
    dc_set_a8(a);
    if (a) {
        dc_set_mx(1, 1);
        dc_jsr(0xC08A6D, 0xC08847);
        dc_set_mx(g_cpu.m_flag, 0);
    }
    dc_jsr(0xC09175, 0xC0884C);
    dc_jsr(0xC099DE, 0xC0884F);
    dc_jsr(0xC091AC, 0xC08852);
    dc_jsr(0xC093E1, 0xC08855);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
