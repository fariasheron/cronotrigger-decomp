/* Field event script: object property commands 84 87 89-8B (bank C0). */
#include "ram.h"

#define OBJ_1001(o)         (0x1001 + (o))
#define OBJ_SOLID(o)        (0x1A00 + (o))
#define OBJ_1B01(o)         (0x1B01 + (o))
#define OBJ_LAYER_FLAGS(o)  (0x0F80 + (o))   /* bit 7: priority from the map */
#define OBJ_0C00(o)         (0x0C00 + (o))
#define OBJ_0C01(o)         (0x0C01 + (o))
#define MAP_PRIO_TOP        0x7000           /* 7E:7000 + y*256 + x: bit 6 = drawn above sprites */
#define MAP_PRIO_BOTTOM     0x7040

static inline u16 cur_obj(void) { return ram16(EV_CUR_OBJ); }

/* Sprite priority bits (0C00 lower half, 0C01 upper half) from the map's
 * priority bits under tile t = y*256 + x. */
static void prio_from_map(u16 o, u16 t)
{
    u8 top = (u8)(((ram8(MAP_PRIO_TOP + t) & 0x40) >> 2) | 0x20);
    u8 bot = (u8)(((ram8(MAP_PRIO_BOTTOM + t) & 0x40) >> 2) | 0x20);
    ram8_set(EV_TMP_D9, top);
    ram8_set(OBJ_0C00(o), bot);
    ram8_set(OBJ_0C01(o), top);
    dc_set_a8(top);
}

/* 84 b  1B01[obj] = b */
DC_ENTRY(C04867, RTS)
{
    DC_ASSUME_D(C04867, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    dc_set_a8(b);
    ram8_set(OBJ_1B01(cur_obj()), b);
    g_cpu.Y = g_cpu.X = (u16)(p + 1);
    g_cpu._flag_C = 1;
}
/* 87 b  Script speed: 1000/1001[obj] low bits = b + 1 (1000 bit 7 kept). */
DC_ENTRY(C04876, RTS)
{
    DC_ASSUME_D(C04876, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 v = (u8)(ev_byte(p) + 1);
    ram8_set(EV_TMP_D9, v);
    u16 o = cur_obj();
    u8 r = (u8)((ram8(OBJ_EV_DISABLED(o)) & 0x80) | v);
    ram8_set(OBJ_EV_DISABLED(o), r);
    ram8_set(OBJ_1001(o), r);
    dc_set_a8(r);
    g_cpu.Y = p;
    g_cpu.X = (u16)(p + 1);
    g_cpu._flag_C = 1;
}
/* 89 b  Solidity/collision byte 1A00[obj] = b;  8A n: = var[n] */
DC_ENTRY(C04B49, RTS)
{
    DC_ASSUME_D(C04B49, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    dc_set_a8(b);
    ram8_set(OBJ_SOLID(cur_obj()), b);
    g_cpu.Y = p;
    g_cpu.X = (u16)(p + 1);
    g_cpu._flag_C = 1;
}
DC_ENTRY(C04B58, RTS)
{
    DC_ASSUME_D(C04B58, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ram8(EV_LOCAL + ev_byte(p) * 2);
    dc_set_a8(b);
    ram8_set(OBJ_SOLID(cur_obj()), b);
    g_cpu.Y = p;
    g_cpu.X = (u16)(p + 1);
    g_cpu._flag_C = 1;
}
/* 8B x y  Put the object on tile (x, y), centred; objects that take their
 * priority from the map read it from the tile. */
DC_ENTRY(C04B74, RTS)
{
    DC_ASSUME_D(C04B74, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u16 o = cur_obj();
    u8 tx = ev_byte(p), ty = ev_byte((u16)(p + 1));
    ram8_set(OBJ_TILE_X(o), tx); ram8_set(EV_TMP_D9, tx); ram8_set(OBJ_POS_X(o), 0x80);
    ram8_set(OBJ_TILE_Y(o), ty); ram8_set(EV_TMP_D9 + 1, ty); ram8_set(OBJ_POS_Y(o), 0xFF);
    dc_set_a8(0xFF);
    if (ram8(OBJ_LAYER_FLAGS(o)) & 0x80) prio_from_map(o, (u16)(ty << 8 | tx));
    g_cpu.Y = (u16)(p + 1);
    g_cpu.X = (u16)(p + 2);
    g_cpu._flag_C = 1;
}
/* 8C n m  Put the object on tile (var[n], var[m]). */
DC_ENTRY(C04BC3, RTS)
{
    DC_ASSUME_D(C04BC3, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u16 o = cur_obj();
    u8 tx = ram8(EV_LOCAL + ev_byte(p) * 2), ty = ram8(EV_LOCAL + ev_byte((u16)(p + 1)) * 2);
    ram8_set(OBJ_TILE_X(o), tx); ram8_set(EV_TMP_D9, tx);
    ram8_set(OBJ_TILE_Y(o), ty); ram8_set(EV_TMP_D9 + 1, ty); ram8_set(OBJ_POS_Y(o), 0xFF);
    dc_set_a8(0xFF);
    if (ram8(OBJ_LAYER_FLAGS(o)) & 0x80) prio_from_map(o, (u16)(ty << 8 | tx));
    g_cpu.Y = (u16)(p + 1);
    g_cpu.X = (u16)(p + 2);
    g_cpu._flag_C = 1;
}
/* 8D x y  Put the object at position (x, y), 16-bit (high byte = tile). */
DC_ENTRY(C04C27, RTS)
{
    DC_ASSUME_D(C04C27, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u16 o = cur_obj();
    ram16_set(OBJ_POS_X(o), ev_word(p));
    u16 y = ev_word((u16)(p + 2));
    ram16_set(OBJ_POS_Y(o), y);
    g_cpu.A = y;
    if (ram8(OBJ_LAYER_FLAGS(o)) & 0x80) prio_from_map(o, (u16)(ram8(OBJ_TILE_Y(o)) << 8 | ram8(OBJ_TILE_X(o))));
    g_cpu.Y = (u16)(p + 2);
    g_cpu.X = (u16)(p + 4);
    g_cpu._flag_C = 1;
}
/* 8E b  Sprite priority mode: bit 7 = follow the map under the object, else
 * bits 4-5 upper half / bits 0-1 lower half. */
DC_ENTRY(C04C74, RTS)
{
    DC_ASSUME_D(C04C74, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u16 o = cur_obj();
    u8 b = ev_byte(p);
    ram8_set(OBJ_LAYER_FLAGS(o), b);
    if (b & 0x80) {
        prio_from_map(o, (u16)(ram8(OBJ_TILE_Y(o)) << 8 | ram8(OBJ_TILE_X(o))));
    } else {
        ram8_set(EV_TMP_D9, b);
        ram8_set(OBJ_0C01(o), b & 0x30);
        ram8_set(OBJ_0C00(o), (u8)(b << 4) & 0x30);
    }
    dc_set_a8(0xFF);
    ram8_set(0x0F01 + o, 0xFF);
    ram8_set(OBJ_1601(o), 0);
    g_cpu.Y = p;
    g_cpu.X = (u16)(p + 1);
    g_cpu._flag_C = 1;
}
/* 90 / 91 / 7E  1A81[obj] = 1 / 0 / 80 (drawing on / off / ?), then pause
 * until the next frame. 7C n / 7D n: the same (1 / 0) for object n. */
static void set_1a81(u16 o, u8 v, u16 next)
{
    ram8_set(OBJ_1A81(o), v);
    dc_set_a8(v);
    g_cpu.X = next;
    g_cpu._flag_C = 0;
}
DC_ENTRY(C04CD5, RTS) { DC_ASSUME_D(C04CD5, 0x0100); set_1a81(cur_obj(), 0x01, (u16)(g_cpu.Y + 1)); }
DC_ENTRY(C04CE0, RTS) { DC_ASSUME_D(C04CE0, 0x0100); set_1a81(cur_obj(), 0x00, (u16)(g_cpu.Y + 1)); }
DC_ENTRY(C04CE6, RTS) { DC_ASSUME_D(C04CE6, 0x0100); set_1a81(cur_obj(), 0x80, (u16)(g_cpu.Y + 1)); }
DC_ENTRY(C04CEC, RTS) { g_cpu.Y++; set_1a81(ev_byte(g_cpu.Y), 0x01, (u16)(g_cpu.Y + 1)); }
DC_ENTRY(C04CF9, RTS) { g_cpu.Y++; set_1a81(ev_byte(g_cpu.Y), 0x00, (u16)(g_cpu.Y + 1)); }

/* ---- movement commands ------------------------------------------------ */
#define OBJ_SPEED_X(o)      (0x1900 + (o))   /* signed per-frame step */
#define OBJ_SPEED_Y(o)      (0x1980 + (o))
#define OBJ_MOVING(o)       (0x1A80 + (o))   /* a timed move is running */
#define OBJ_MOVE_LEFT(o)    (0x1A01 + (o))   /* frames left */
#define OBJ_1B80(o)         (0x1B80 + (o))
#define OBJ_MOVE_SPEED(o)   (0x1B81 + (o))
#define OBJ_0B00_7F(o)      (0x10B00 + (o))  /* 7F:0B00 */
#define EV_DIRECTION        0x01EE           /* 16-bit: direction 00-FF (00 = ?) */
#define EV_TMP_C7           0x01C7

/* A timed move already running: wait (repeat the command) until its frames
 * run out, then finish it and go on to `next`. Returns false when none runs. */
static bool move_running(u16 o, u16 next, bool clear_1b80)
{
    if (!ram8(OBJ_MOVING(o))) return false;
    u8 left = ram8(OBJ_MOVE_LEFT(o));
    dc_set_a8(left);
    if (left) { g_cpu.X = g_cpu.Y; g_cpu._flag_C = 0; return true; }
    ram8_set(OBJ_MOVING(o), 0);
    if (clear_1b80) ram8_set(OBJ_1B80(o), 0);
    g_cpu.X = next;
    g_cpu._flag_C = 1;
    return true;
}

/* 7B sx sy speed n  Move by (sx, sy) per step for n frames. */
DC_ENTRY(C04E25, RTS)
{
    DC_ASSUME_D(C04E25, 0x0100);
    u16 o = cur_obj(), pos = g_cpu.Y;
    if (move_running(o, (u16)(pos + 5), true)) return;
    ram16_set(EV_TMP_C7, pos);
    ram8_set(OBJ_SPEED_X(o), ev_byte((u16)(pos + 1)));
    ram8_set(OBJ_SPEED_Y(o), ev_byte((u16)(pos + 2)));
    ram8_set(OBJ_MOVE_SPEED(o), ev_byte((u16)(pos + 3)));
    ram8_set(OBJ_MOVE_LEFT(o), ev_byte((u16)(pos + 4)));
    ram8_set(OBJ_MOVING(o), 1);
    ram8_set(OBJ_1B80(o), 1);
    dc_set_a8(1);
    g_cpu.Y = o;
    g_cpu.X = pos;
    g_cpu._flag_C = 0;
}

/* 92 dir n  Walk in direction dir (00-FF) for n frames; the object turns to
 * face it (facing table DB:F700). */
DC_ENTRY(C04E73, RTS)
{
    DC_ASSUME_D(C04E73, 0x0100);
    u16 o = cur_obj(), pos = g_cpu.Y;
    if (ram8(OBJ_MOVING(o))) {
        u8 left = ram8(OBJ_MOVE_LEFT(o));
        dc_set_a8(left);
        if (left) { g_cpu.X = pos; g_cpu._flag_C = 0; return; }
        ram8_set(OBJ_MOVING(o), 0);
        g_cpu.X = o;
        dc_jsr(0xC0568B, 0xC04E85);          /* stop the walk animation */
        g_cpu.X = (u16)(g_cpu.Y + 3);
        g_cpu._flag_C = 1;
        return;
    }
    u8 dir = ev_byte((u16)(pos + 1));
    ram16_set(EV_DIRECTION, dir);
    ram8_set(OBJ_FACING(o), bus8(((u32)g_cpu.DB << 16) + 0xF700 + dir));
    ram8_set(OBJ_0B00_7F(o), 0x80);
    u8 n = ev_byte((u16)(pos + 2));
    ram8_set(OBJ_MOVE_LEFT(o), n);
    ram8_set(OBJ_MOVING(o), (u8)(ram8(OBJ_MOVING(o)) + 1));
    ram16_set(EV_TMP_C7, pos);
    dc_set_a8(n);
    g_cpu.X = o;
    dc_jsr(0xC0ACFD, 0xC04EBD);              /* speed from direction */
    dc_jsr(0xC05614, 0xC04EC0);              /* walk animation */
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 0;
}

/* Speed step for a move: frames = distance*256 / speed (hardware divider,
 * low byte), then 1980 (16-bit) is lowered by frames*speed2/2. */
#define EV_MOVE_SPEED2  0x01C1
static void move_steps(u16 o, u8 frames, u8 speed2)
{
    ram8_set(OBJ_MOVE_LEFT(o), frames);
    io8_set(0x4202, frames);
    io8_set(0x4203, speed2);
    ram8_set(OBJ_MOVE_SPEED(o), speed2);
    u16 a = ram16(OBJ_SPEED_Y(o)), b = (u16)-(u16)((frames * speed2) >> 1), r = (u16)(a + b);
    g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1;
    ram16_set(OBJ_SPEED_Y(o), r);
}
static bool move_axis(u16 o, u8 from, u8 to, u32 speed_addr)
{
    if (to == from) return false;
    u8 d = to > from ? (u8)(to - from) : (u8)(from - to);
    io16_set(0x4204, (u16)(d << 8));
    u8 s = ram8(speed_addr);
    if (!s) return false;
    if (s & 0x80) s = (u8)-s;
    io8_set(0x4206, s);
    move_steps(o, (u8)((d << 8) / s), ram8(EV_MOVE_SPEED2));
    return true;
}
/* 7A x y speed  Walk to tile (x, y). */
DC_ENTRY(C04D06, RTS)
{
    DC_ASSUME_D(C04D06, 0x0100);
    u16 o = cur_obj(), pos = g_cpu.Y;
    if (move_running(o, (u16)(pos + 4), true)) return;
    ram16_set(EV_TMP_C7, pos);
    u8 tx = ev_byte((u16)(pos + 1)), ty = ev_byte((u16)(pos + 2));
    ram8_set(0x01F2, tx);
    ram8_set(0x01F3, ty);
    ram8_set(EV_MOVE_SPEED2, ev_byte((u16)(pos + 3)));
    u8 fx = ram8(OBJ_TILE_X(o)), fy = ram8(OBJ_TILE_Y(o));
    ram8_set(0x01F0, fx);
    ram8_set(0x01F1, fy);
    dc_set_a8(fy);
    g_cpu.X = o;
    dc_jsr(0xC0ABA2, 0xC04D46);              /* direction from (F0,F1) to (F2,F3) -> $EE */
    g_cpu.X = o;
    dc_jsr(0xC0AC69, 0xC04D4B);              /* speeds 1900/1980 for that direction */
    if (!move_axis(o, ram8(0x01F0), ram8(0x01F2), OBJ_SPEED_X(o)) &&
        !move_axis(o, ram8(0x01F1), ram8(0x01F3), OBJ_SPEED_Y(o))) {
        u8 c = ram8(EV_MOVE_SPEED2);
        move_steps(o, c, c);
    }
    ram8_set(OBJ_MOVING(o), 1);
    ram8_set(OBJ_1B80(o), 1);
    dc_set_a8(1);
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 0;
}

/* 9C dir n  Walk in direction dir for n frames without turning;
 * 9D a b: the same with dir = var[a], n = var[b]. */
static void walk_dir(u16 o, u16 pos, u8 dir, u8 n)
{
    ram16_set(EV_DIRECTION, dir);
    ram8_set(OBJ_MOVE_LEFT(o), n);
    ram8_set(OBJ_0B00_7F(o), 0x80);
    ram8_set(OBJ_MOVING(o), (u8)(ram8(OBJ_MOVING(o)) + 1));
    ram16_set(EV_TMP_C7, pos);
    dc_set_a8(0x80);
    g_cpu.X = o;
    dc_jsr(0xC0ACFD, 0xC04F03);
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C04EC7, RTS)
{
    DC_ASSUME_D(C04EC7, 0x0100);
    u16 o = cur_obj(), pos = g_cpu.Y;
    if (move_running(o, (u16)(pos + 3), false)) return;
    walk_dir(o, pos, ev_byte((u16)(pos + 1)), ev_byte((u16)(pos + 2)));
}
DC_ENTRY(C04F0A, RTS)
{
    DC_ASSUME_D(C04F0A, 0x0100);
    u16 o = cur_obj(), pos = g_cpu.Y;
    if (move_running(o, (u16)(pos + 3), false)) return;
    walk_dir(o, pos, ram8(EV_LOCAL + ev_byte((u16)(pos + 1)) * 2), ram8(EV_LOCAL + ev_byte((u16)(pos + 2)) * 2));
}

/* ---- walking to a tile or to another object ------------------------------
 * One step per call: while the object is between tiles (1A01 frames left)
 * the command repeats; once on the target it goes on. Variants: target is a
 * tile (exact) or an object (any of the 8 tiles around it, or a removed
 * object ends the walk); with or without turning/walk animation; step time
 * from the object's speed (1000) or a frame count n from the script (those
 * keep 1A80 set while running). */
#define WALK_FROM_X 0x01F0
#define WALK_FROM_Y 0x01F1
#define WALK_TO_X   0x01F2
#define WALK_TO_Y   0x01F3
#define WALK_TARGET 0x01E9   /* 16-bit: target object */
#define EV_TMP_DB   0x01DB
#define EV_TMP_DD   0x01DD
typedef struct WalkSites { u32 center, center_anim, stop, dir, block, go, go_anim; } WalkSites;
typedef struct Walk {
    bool to_object, animate, counted;
    bool until_on_screen;   /* 8F: also done once inside the screen's inner area */
    u8 len;
    WalkSites at;
    bool quiet_done;        /* 98/99: a finished counted walk does not stop the animation */
} Walk;
static u8 facing_for(u16 dir) { return bus8(((u32)g_cpu.DB << 16) + 0xF700 + dir); }
static bool near(u8 a, u8 b) { return a == b || (u8)(a + 1) == b || (u8)(a - 1) == b; }

static void walk_finish2(const Walk *w, u16 o, bool anim)
{
    if (w->counted) ram8_set(OBJ_MOVING(o), 0);
    if (w->animate && anim) dc_jsr(0xC0568B, w->at.stop);      /* stop the walk animation */
    g_cpu.X = (u16)(g_cpu.Y + w->len);
    g_cpu._flag_C = 1;
}
static void walk_finish(const Walk *w, u16 o) { walk_finish2(w, o, true); }
static void walk_wait(void) { g_cpu.X = g_cpu.Y; g_cpu._flag_C = 0; }

/* target: tile (tx, ty), or object tx when w->to_object. */
static void walk(const Walk *w, u8 tx, u8 ty)
{
    u16 o = cur_obj(), pos = g_cpu.Y;
    if (w->counted && ram8(OBJ_MOVING(o))) {
        u8 left = ram8(OBJ_MOVE_LEFT(o));
        dc_set_a8(left);
        if (left) { walk_wait(); return; }
        g_cpu.X = o;
        walk_finish2(w, o, !w->quiet_done);
        return;
    }
    if (!w->counted) {
        u8 left = ram8(OBJ_MOVE_LEFT(o));
        dc_set_a8(left);
        if (left) { walk_wait(); return; }
    }
    if (w->to_object) {
        u16 t = tx;
        g_cpu.X = t;
        u8 r = ram8(OBJ_REMOVED(t));
        dc_set_a8(r);
        if (r & 0x80) { walk_finish(w, o); return; }
        tx = ram8(OBJ_TILE_X(t));
        ty = ram8(OBJ_TILE_Y(t));
        ram16_set(WALK_TARGET, t);
    }
    ram8_set(WALK_TO_X, tx);
    ram8_set(WALK_TO_Y, ty);
    u8 fx = ram8(OBJ_TILE_X(o)), fy = ram8(OBJ_TILE_Y(o));
    ram8_set(WALK_FROM_X, fx);
    ram8_set(WALK_FROM_Y, fy);
    g_cpu.X = o;
    dc_set_a8(fy);
    bool there = w->to_object ? near(fy, ty) && near(fx, tx) : fy == ty && fx == tx;
    if (w->until_on_screen) {
        if (there) { walk_finish(w, o); return; }
        /* scroll position in tiles: $1D0A/$1D0E halved */
        u8 left = (u8)(ram8(FIELD_SCROLL_X_HALF) >> 1), top = (u8)(ram8(FIELD_SCROLL_Y_HALF) >> 1);
        ram8_set(EV_TMP_DB, left);
        ram8_set(EV_TMP_DD, top);
        u8 sx = (u8)(fx - left), sy = (u8)(fy - top);
        dc_set_a8(sx);
        if (sx > 1 && sx < 0x0E) {
            dc_set_a8(sy);
            if (sy > 1 && sy < 0x0D) { g_cpu.X = o; walk_finish(w, o); return; }
        }
    } else if (there) {
        u8 f = ram8(OBJ_POS_PROPS(o)), bit = w->to_object ? 0x02 : 0x01;
        dc_set_a8(f);
        if (f & bit) {
            /* first centre on the tile (C0:305D / C0:30B3: carry set while moving) */
            dc_jsr(w->to_object ? 0xC030B3 : 0xC0305D, w->at.center);
            if (g_cpu._flag_C) {
                if (w->animate) dc_jsr(0xC05614, w->at.center_anim);
                walk_wait();
                return;
            }
        }
        g_cpu.X = o;
        walk_finish(w, o);
        return;
    }
    /* turn toward the target (direction $EE from C0:ABA2); when the way is
     * blocked (C0:5B95) try the direction 0x30 further */
    dc_jsr(0xC0ABA2, w->at.dir);
    if (w->animate) {
        u16 dir = ram16(EV_DIRECTION);
        g_cpu.A = dir;
        g_cpu.X = dir;
        ram8_set(OBJ_FACING(o), facing_for(dir));
        dc_set_a8(ram8(OBJ_FACING(o)));
        g_cpu.X = o;
    }
    dc_jsr(0xC05B95, w->at.block);
    if (g_cpu._flag_C) {
        u8 a = ram8(EV_DIRECTION), r = (u8)(a + 0x30);
        g_cpu._flag_V = ((~(a ^ 0x30) & (a ^ r)) >> 7) & 1;
        ram8_set(EV_DIRECTION, r);
        dc_set_a8(r);
        if (w->animate) {
            ram8_set(OBJ_FACING(o), facing_for(g_cpu.A));
            dc_set_a8(ram8(OBJ_FACING(o)));
        }
    }
    u8 n = w->counted ? ev_byte((u16)(g_cpu.Y + w->len - 1)) : ram8(OBJ_EV_DISABLED(o));
    dc_set_a8(n);
    ram8_set(OBJ_MOVE_LEFT(o), n);
    ram8_set(OBJ_0B00_7F(o), n);
    if (w->counted) ram8_set(OBJ_MOVING(o), (u8)(ram8(OBJ_MOVING(o)) + 1));
    ram16_set(EV_TMP_C7, g_cpu.Y);
    g_cpu.X = o;
    dc_jsr(0xC0ACFD, w->at.go);                     /* speed from direction */
    if (w->animate) dc_jsr(0xC05614, w->at.go_anim);  /* walk animation */
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 0;
    (void)pos;
}

static u8 var8(u8 n) { return ram8(EV_LOCAL + n * 2); }
static u8 party_obj(u8 slot) { return ram8(PARTY_OBJ(slot)); }

/* 96 x y      walk to tile (x, y)                       */
/* 97 a b      walk to tile (var[a], var[b])             */
/* 9A x y n    walk to tile (x, y), n frames per step    */
/* A0 x y      slide to tile (x, y) (no turning)         */
/* A1 a b      slide to tile (var[a], var[b])            */
/* 94 o        walk next to object o                     */
/* 95 s        walk next to party member s's object      */
/* 98 o n      walk next to object o, n frames per step  */
/* 99 s n      walk next to party member s, n frames     */
/* 9E o / 9F s slide next to object o / party member s   */
static const Walk W96 = { false, true, false, false, 3, { 0xC04F97, 0xC04FA5, 0xC04F9C, 0xC04FAB, 0xC04FBD, 0xC04FE0, 0xC04FE3 } };
static const Walk W97 = { false, true, false, false, 3, { 0xC050D6, 0xC050E4, 0xC050DB, 0xC050EA, 0xC050FC, 0xC0511F, 0xC05122 } };
static const Walk W9A = { false, true, true, false, 4, { 0xC05025, 0xC05037, 0xC0502D, 0xC0503D, 0xC0504F, 0xC0507A, 0xC0507D } };
static const Walk WA0 = { false, false, false, false, 3, { 0xC0515F, 0, 0, 0xC0516D, 0xC05170, 0xC0518A, 0 } };
static const Walk WA1 = { false, false, false, false, 3, { 0xC051E3, 0, 0, 0xC051F1, 0xC051F4, 0xC0520E, 0 } };
static const Walk W94 = { true, true, false, false, 2, { 0xC0526E, 0xC0527B, 0xC05273, 0xC05281, 0xC05293, 0xC052B6, 0xC052B9 } };
static const Walk W98 = { true, true, true, false, 3, { 0xC053B1, 0xC053C4, 0xC053BB, 0xC053CA, 0xC053DC, 0xC05406, 0xC05409 }, true };
static const Walk W9E = { true, false, false, false, 2, { 0xC05319, 0, 0, 0xC05326, 0xC05329, 0xC05343, 0 } };
static const Walk W8F = { true, true, false, true, 2, { 0, 0, 0xC054A8, 0xC054B6, 0xC054C8, 0xC054EB, 0xC054EE } };
#define WALK_ENTRY(pc, spec, tx, ty) DC_ENTRY(pc, RTS) { DC_ASSUME_D(pc, 0x0100); u16 p = g_cpu.Y; (void)p; walk(&spec, tx, ty); }
WALK_ENTRY(C04F61, W96, ev_byte((u16)(p + 1)), ev_byte((u16)(p + 2)))
WALK_ENTRY(C05084, W97, var8(ev_byte((u16)(p + 1))), var8(ev_byte((u16)(p + 2))))
WALK_ENTRY(C04FEA, W9A, ev_byte((u16)(p + 1)), ev_byte((u16)(p + 2)))
WALK_ENTRY(C05129, WA0, ev_byte((u16)(p + 1)), ev_byte((u16)(p + 2)))
WALK_ENTRY(C05191, WA1, var8(ev_byte((u16)(p + 1))), var8(ev_byte((u16)(p + 2))))
WALK_ENTRY(C05215, W94, ev_byte((u16)(p + 1)), 0)
WALK_ENTRY(C05410, W94, party_obj(ev_byte((u16)(p + 1))), 0)
WALK_ENTRY(C0534A, W98, ev_byte((u16)(p + 1)), 0)
WALK_ENTRY(C0550E, W98, party_obj(ev_byte((u16)(p + 1))), 0)
WALK_ENTRY(C052C0, W9E, ev_byte((u16)(p + 1)), 0)
WALK_ENTRY(C054F5, W9E, party_obj(ev_byte((u16)(p + 1))), 0)
/* 8F s  walk toward party member s until next to them or well inside the screen */
WALK_ENTRY(C05429, W8F, party_obj(ev_byte((u16)(p + 1))), 0)


/* A6 f  Face f (0 up, 1 down, 2 left, 3 right);  A7 n: face var[n]. Both
 * pause until the next frame. */
static void face(u8 f)
{
    u16 o = cur_obj();
    dc_set_a8(f);
    ram8_set(OBJ_FACING(o), f);
    ram8_set(OBJ_1601(o), 0);
    g_cpu.X = (u16)(g_cpu.Y + 1);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C05545, RTS) { DC_ASSUME_D(C05545, 0x0100); g_cpu.Y++; face(ev_byte(g_cpu.Y)); }
DC_ENTRY(C05557, RTS) { DC_ASSUME_D(C05557, 0x0100); g_cpu.Y++; face(var8(ev_byte(g_cpu.Y))); }

/* A8 o  Face object o;  A9 s: face party member s. On the same tile the
 * pixel offsets inside the tile decide. A removed object is skipped. */
static void face_object(u16 t)
{
    g_cpu.X = t;
    u8 r = ram8(OBJ_REMOVED(t));
    dc_set_a8(r);
    if (r & 0x80) { g_cpu.X = (u16)(g_cpu.Y + 2); g_cpu._flag_C = 1; return; }
    u16 o = cur_obj();
    ram8_set(WALK_TO_X, ram8(OBJ_TILE_X(t)));
    ram8_set(WALK_TO_Y, ram8(OBJ_TILE_Y(t)));
    ram16_set(EV_TMP_D9, t);
    ram8_set(WALK_FROM_X, ram8(OBJ_TILE_X(o)));
    ram8_set(WALK_FROM_Y, ram8(OBJ_TILE_Y(o)));
    g_cpu.X = o;
    dc_set_a8(ram8(WALK_FROM_Y));
    if (ram8(WALK_FROM_Y) == ram8(WALK_TO_Y) && ram8(WALK_FROM_X) == ram8(WALK_TO_X)) {
        ram8_set(WALK_FROM_X, ram8(OBJ_POS_X(o)));
        ram8_set(WALK_FROM_Y, ram8(OBJ_POS_Y(o)));
        ram8_set(WALK_TO_X, ram8(OBJ_POS_X(t)));
        ram8_set(WALK_TO_Y, ram8(OBJ_POS_Y(t)));
        g_cpu.X = t;
        dc_set_a8(ram8(WALK_TO_Y));
    } else {
        dc_set_a8(ram8(ram8(WALK_FROM_Y) == ram8(WALK_TO_Y) ? WALK_FROM_X : WALK_FROM_Y));
    }
    dc_jsr(0xC0ABA2, 0xC055E6);
    u16 dir = ram16(EV_DIRECTION);
    g_cpu.A = dir;
    u8 f = facing_for(dir);
    dc_set_a8(f);
    ram8_set(OBJ_FACING(o), f);
    ram8_set(OBJ_1601(o), 0);
    g_cpu.X = (u16)(g_cpu.Y + 2);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C055A0, RTS) { DC_ASSUME_D(C055A0, 0x0100); face_object(ev_byte((u16)(g_cpu.Y + 1))); }
DC_ENTRY(C05605, RTS) { DC_ASSUME_D(C05605, 0x0100); face_object(party_obj(ev_byte((u16)(g_cpu.Y + 1)))); }

/* ---- animation --------------------------------------------------------- */
#define OBJ_ANIM(o)         (0x1680 + (o))   /* animation number (FF = none) */
#define OBJ_ANIM_FRAME(o)   (0x1681 + (o))
#define OBJ_ANIM_MODE(o)    (0x1780 + (o))   /* 0 off, 1 loop, 2 once (AB), 3 static frame (AC) */
#define OBJ_ANIM_ONCE(o)    (0x1781 + (o))   /* animation AB is playing */
#define OBJ_ANIM_STATE(o)   (0x10B01 + (o))  /* 7F:0B01: AB progress, 1 = finished */
#define OBJ_STATIC_FRAME(o) (0x1301 + (o))
#define OBJ_WAIT(o)         (0x10A01 + (o))  /* 7F:0A01: frames waited (AD) */

static void anim_reset(u16 o, u8 mode)
{
    ram8_set(OBJ_ANIM_MODE(o), mode);
    ram8_set(OBJ_1601(o), 0);
    ram8_set(OBJ_ANIM_STATE(o), 0);
    ram8_set(OBJ_ANIM_FRAME(o), 0);
    dc_set_a8(0);
    g_cpu.X = (u16)(g_cpu.Y + 1);
    g_cpu._flag_C = 1;
}
/* AA a  Loop animation a. */
DC_ENTRY(C02E67, RTS)
{
    DC_ASSUME_D(C02E67, 0x0100);
    g_cpu.Y++;
    u16 o = cur_obj();
    ram8_set(OBJ_ANIM(o), ev_byte(g_cpu.Y));
    anim_reset(o, 1);
}
/* AE  Animation off (animation 0). */
DC_ENTRY(C02E8F, RTS)
{
    DC_ASSUME_D(C02E8F, 0x0100);
    u16 o = cur_obj();
    ram8_set(OBJ_ANIM(o), 0);
    anim_reset(o, 0);
}
/* AB a  Play animation a once and wait until it has finished;
 * B7 a n: play it n times (7F:0B01 counts down to 1 = finished). */
static void anim_play(u8 len, bool counted)
{
    u16 o = cur_obj(), pos = g_cpu.Y;
    u8 s = ram8(OBJ_ANIM_STATE(o)), a = ev_byte((u16)(pos + 1));
    if (s == 1) {
        ram8_set(OBJ_ANIM_STATE(o), 0);
        ram8_set(OBJ_1601(o), 0);
        ram8_set(OBJ_ANIM_FRAME(o), 0);
        u8 m = 1;
        if (ram8(OBJ_ANIM(o)) == 0xFF) { ram8_set(OBJ_ANIM(o), 0); m = 0; }
        ram8_set(OBJ_ANIM_MODE(o), m);
        dc_set_a8(m);
        g_cpu.X = (u16)(pos + len);
        g_cpu._flag_C = 1;
        return;
    }
    dc_set_a8(a);
    if (s == 0 || a != ram8(OBJ_ANIM_ONCE(o))) {
        ram8_set(OBJ_ANIM_ONCE(o), a);
        ram8_set(OBJ_ANIM_FRAME(o), 0);
        ram8_set(OBJ_1601(o), 0);
        if (ram8(OBJ_ANIM_MODE(o)) == 0) ram8_set(OBJ_ANIM(o), 0xFF);
        ram8_set(OBJ_ANIM_MODE(o), 2);
        u8 st = counted ? (u8)(ev_byte((u16)(pos + 2)) + 1) : 2;
        ram8_set(OBJ_ANIM_STATE(o), st);
        dc_set_a8(st);
    }
    g_cpu.X = pos;
    g_cpu._flag_C = 0;
}
DC_ENTRY(C02E9A, RTS) { DC_ASSUME_D(C02E9A, 0x0100); anim_play(2, false); }
DC_ENTRY(C02F01, RTS) { DC_ASSUME_D(C02F01, 0x0100); anim_play(3, true); }
/* AC f  Show static frame f. */
DC_ENTRY(C02F71, RTS)
{
    DC_ASSUME_D(C02F71, 0x0100);
    u16 o = cur_obj(), pos = g_cpu.Y;
    ram8_set(OBJ_STATIC_FRAME(o), ev_byte((u16)(pos + 1)));
    ram8_set(OBJ_ANIM_FRAME(o), 0);
    ram8_set(OBJ_1601(o), 0);
    if (ram8(OBJ_ANIM_MODE(o)) == 0) ram8_set(OBJ_ANIM(o), 0xFF);
    ram8_set(OBJ_ANIM_MODE(o), 3);
    dc_set_a8(3);
    g_cpu.X = (u16)(pos + 2);
    g_cpu._flag_C = 0;
}
/* AD n  Wait n frames (n+1 calls); B9 BA BC BD: wait 4, 8, 16, 32. */
static void wait_frames(u8 n, u8 len)
{
    u16 o = cur_obj(), pos = g_cpu.Y;
    u8 c = ram8(OBJ_WAIT(o));
    if (n > c) {
        ram8_set(OBJ_WAIT(o), (u8)(c + 1));
        dc_set_a8((u8)(c + 1));
        g_cpu.X = pos;
        g_cpu._flag_C = 0;
        return;
    }
    ram8_set(OBJ_WAIT(o), 0);
    dc_set_a8(0);
    g_cpu.X = n == c ? (u16)(pos + len) : pos;   /* past the count: restart at 0 now */
    g_cpu._flag_C = 1;
}
DC_ENTRY(C02F96, RTS) { DC_ASSUME_D(C02F96, 0x0100); wait_frames(ev_byte((u16)(g_cpu.Y + 1)), 2); }
DC_ENTRY(C02FC6, RTS) { DC_ASSUME_D(C02FC6, 0x0100); wait_frames(0x04, 1); }
DC_ENTRY(C02FCA, RTS) { DC_ASSUME_D(C02FCA, 0x0100); wait_frames(0x08, 1); }
DC_ENTRY(C02FD2, RTS) { DC_ASSUME_D(C02FD2, 0x0100); wait_frames(0x10, 1); }
DC_ENTRY(C02FCE, RTS) { DC_ASSUME_D(C02FCE, 0x0100); wait_frames(0x20, 1); }
/* B1  End this frame's turn. */
DC_ENTRY(C0353F, RTS) { g_cpu.X = (u16)(g_cpu.Y + 1); g_cpu._flag_C = 0; }
/* B3 / B4  Loop animation 0 / 1. */
DC_ENTRY(C02E87, RTS) { DC_ASSUME_D(C02E87, 0x0100); u16 o = cur_obj(); ram8_set(OBJ_ANIM(o), 0); anim_reset(o, 1); }
DC_ENTRY(C02E8B, RTS) { DC_ASSUME_D(C02E8B, 0x0100); u16 o = cur_obj(); ram8_set(OBJ_ANIM(o), 1); anim_reset(o, 1); }
/* B5 o  Keep walking next to object o (command 94 run forever). */
DC_ENTRY(C03546, RTS)
{
    DC_ASSUME_D(C03546, 0x0100);
    ram16_set(0x01E3, g_cpu.Y);
    dc_jsr(0xC05215, 0xC03548);
    g_cpu.X = ram16(0x01E3);
    g_cpu._flag_C = 0;
}
/* B8 a24  $012B = 24-bit address (probably the dialogue string table; not checked yet). */
DC_ENTRY(C03557, RTS)
{
    DC_ASSUME_D(C03557, 0x0100);
    u16 pos = g_cpu.Y;
    for (int i = 0; i < 3; i++) ram8_set(0x012B + i, ev_byte((u16)(pos + 1 + i)));
    dc_set_a8(ev_byte((u16)(pos + 3)));
    g_cpu.X = (u16)(pos + 4);
    g_cpu._flag_C = 1;
}

/* B0  Controlled by the player (repeats forever); AF: one frame of it.
 * Party slot 0 (1100 = 0) gets the leader's control (C0:9E29) and, when
 * facing something it can talk to or touch (C0:5B8D), its check (C0:3154);
 * slots 1 and 2 follow (C0:A26B, C0:A2CE). Nothing while $0138 is set. */
#define FIELD_CONTROL_OFF 0x0138
static void player_control(u32 site_base)
{
    if (ram8(FIELD_CONTROL_OFF)) { dc_set_a8(ram8(FIELD_CONTROL_OFF)); g_cpu.X = g_cpu.Y; g_cpu._flag_C = 0; return; }
    ram16_set(EV_TMP_C7, g_cpu.Y);
    dc_set_mx(1, 1);
    u8 o = ram8(EV_CUR_OBJ);
    u8 slot = ram8(OBJ_REMOVED(o));
    if (slot <= 2) {
        ram8_set(OBJ_MOVE_LEFT(o), 1);
        dc_set_a8(1);
        g_cpu.X = o;
    }
    if (slot == 0) {
        dc_jsr(0xC09E29, site_base + 0x45);
        g_cpu.X = ram8(EV_CUR_OBJ);
        dc_set_a8(ram8(OBJ_FACING(g_cpu.X)));
        dc_jsr(0xC05B8D, site_base + 0x4D);
        if (g_cpu._flag_C) dc_jsr(0xC03154, site_base + 0x52);
    } else if (slot == 1) {
        dc_jsr(0xC0A26B, site_base + 0x35);
    } else if (slot == 2) {
        dc_jsr(0xC0A2CE, site_base + 0x25);
    } else {
        dc_set_a8((u8)(slot - 2));
    }
    dc_set_mx(1, 0);
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C03002, RTS) { DC_ASSUME_D(C03002, 0x0100); player_control(0xC03002); }
DC_ENTRY(C02FFD, RTS)
{
    DC_ASSUME_D(C02FFD, 0x0100);
    dc_push16(0x2FFF);                 /* the JSR $3002 return address */
    player_control(0xC03002);
    dc_pull16();
    g_cpu.X++;
    dc_set_flags_nz16(g_cpu.X);
}

/* ---- dialogue ---------------------------------------------------------- */
#define FIELD_DLG_FLAGS 0x0154
#define DLG_ACTIVE      0x0129   /* a dialogue box is open */
#define DLG_INDEX       0x012A   /* string number in the table at $012B */
#define DLG_TABLE_BANK  0x012D   /* 0 = no table set (B8 missing): fatal */
#define DLG_OWNER       0x012E   /* object that opened it */
#define DLG_0130        0x0130
#define DLG_0132        0x0132
#define OBJ_DLG_OPENED(o) (0x10A00 + (o))   /* 7F:0A00: this object's box is up */

#define DLG_CHOICE      0x0162   /* the box asks a question */
#define DLG_CHOICE_LAST 0x0164
#define DLG_CHOICE_FIRST 0x0165
#define DLG_CHOICE_PICK 0x0166   /* answer chosen */
#define EV_RESULT_OBJ(o) (0x10A80 + (o))

/* BB n / C1 n / C2 n  Show dialogue string n (box auto / top / bottom) and
 * wait until it is closed. C0 n c / C3 n c / C4 n c: the same as a question
 * with answers (c & 3) to (c >> 2 & 3); the answer goes to the object's
 * result byte (7F:0A80). */
static void dialogue(u8 box, bool question, u32 fatal_site)
{
    u16 pos = g_cpu.Y;
    if (!ram8(DLG_TABLE_BANK)) { dc_set_a8(0); fatal_error(0x7FE0, fatal_site); return; }
    if (ram8(DLG_ACTIVE)) { dc_set_a8(ram8(DLG_ACTIVE)); g_cpu.X = pos; g_cpu._flag_C = 0; return; }
    u16 o = cur_obj();
    if (ram8(OBJ_DLG_OPENED(o))) {
        ram8_set(OBJ_DLG_OPENED(o), 0);
        dc_set_a8(0);
        if (question) {
            u8 pick = ram8(DLG_CHOICE_PICK);
            ram8_set(EV_RESULT_OBJ(o), pick);
            ram8_set(DLG_CHOICE, 0);
            dc_set_a8(pick);
        }
        g_cpu.X = (u16)(pos + (question ? 3 : 2));
        g_cpu._flag_C = 1;
        return;
    }
    ram8_set(OBJ_DLG_OPENED(o), 1);
    ram8_set(DLG_INDEX, ev_byte((u16)(pos + 1)));
    if (question) {
        u8 c = ev_byte((u16)(pos + 2));
        ram8_set(EV_TMP_D9, c);
        ram8_set(DLG_CHOICE_FIRST, c & 3);
        ram8_set(DLG_CHOICE_LAST, (c >> 2) & 3);
        ram8_set(DLG_CHOICE, 1);
    }
    ram8_set(DLG_OWNER, ram8(EV_CUR_OBJ));
    ram8_set(DLG_0132, 0);
    ram8_set(DLG_ACTIVE, 1);
    ram8_set(DLG_0130, box);
    ram8_set(FIELD_DLG_FLAGS, ram8(FIELD_DLG_FLAGS) & ~0x20);
    dc_set_a8(0x20);
    g_cpu.X = pos;
    g_cpu._flag_C = 0;
}
DC_ENTRY(C03570, RTS) { DC_ASSUME_D(C03570, 0x0100); dialogue(0, false, 0xC03577); }
DC_ENTRY(C035B5, RTS) { DC_ASSUME_D(C035B5, 0x0100); dialogue(1, false, 0xC035BC); }
DC_ENTRY(C035FC, RTS) { DC_ASSUME_D(C035FC, 0x0100); dialogue(2, false, 0xC03603); }
DC_ENTRY(C03643, RTS) { DC_ASSUME_D(C03643, 0x0100); dialogue(0, true, 0xC0364A); }
DC_ENTRY(C036AA, RTS) { DC_ASSUME_D(C036AA, 0x0100); dialogue(1, true, 0xC036B1); }
DC_ENTRY(C036DD, RTS) { DC_ASSUME_D(C036DD, 0x0100); dialogue(2, true, 0xC036E4); }
