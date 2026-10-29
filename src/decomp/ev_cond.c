/* Field event script: conditions, facing, result register (bank C0). */
#include "ram.h"

static inline void ev_return(u16 next, bool go_on) { g_cpu.X = next; g_cpu._flag_C = go_on; }
static u16 adc_v(u16 a, u16 b) { u16 r = (u16)(a + b); g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1; return r; }   /* TXA; CLC; ADC $D9 (V of the 16-bit add) */

/* ---- comparison operators (C0:6467 bytes, C0:6477 words) -------------
 * Operands in $01D9 / $01DB. Return true when the condition holds (the
 * original returns carry clear). Ops: 0 ==, 1 !=, 2 >, 3 <, 4 >=, 5 <=,
 * 6 any common bit, 7 any bit set in either. */
static bool ev_compare(int op, u16 a, u16 b)
{
    switch (op) {
    case 0: return a == b;
    case 1: return a != b;
    case 2: return a > b;
    case 3: return a < b;
    case 4: return a >= b;
    case 5: return a <= b;
    case 6: return (a & b) != 0;
    default: return (a | b) != 0;
    }
}
static void ev_compare_glue(int op, bool wide)
{
    u16 a = wide ? ram16(EV_TMP_D9) : ram8(EV_TMP_D9);
    u16 b = wide ? ram16(EV_TMP_OBJ) : ram8(EV_TMP_OBJ);
    if (wide) g_cpu.A = a; else dc_set_a8((u8)a);
    g_cpu._flag_C = !ev_compare(op, a, b);
}
DC_ENTRY(C06487, RTS) { ev_compare_glue(0, false); }
DC_ENTRY(C0648F, RTS) { ev_compare_glue(1, false); }
DC_ENTRY(C06497, RTS) { ev_compare_glue(2, false); }
DC_ENTRY(C064A1, RTS) { ev_compare_glue(3, false); }
DC_ENTRY(C064A9, RTS) { ev_compare_glue(4, false); }
DC_ENTRY(C064B5, RTS) { ev_compare_glue(5, false); }
DC_ENTRY(C064BF, RTS) { ev_compare_glue(6, false); }
DC_ENTRY(C064C7, RTS) { ev_compare_glue(7, false); }
DC_ENTRY(C064CF, RTS) { ev_compare_glue(0, true); }
DC_ENTRY(C064DB, RTS) { ev_compare_glue(1, true); }
DC_ENTRY(C064E7, RTS) { ev_compare_glue(2, true); }
DC_ENTRY(C064F5, RTS) { ev_compare_glue(3, true); }
DC_ENTRY(C06503, RTS) { ev_compare_glue(4, true); }
DC_ENTRY(C06511, RTS) { ev_compare_glue(5, true); }
DC_ENTRY(C06523, RTS) { ev_compare_glue(6, true); }
DC_ENTRY(C0652F, RTS) { ev_compare_glue(7, true); }

/* Run comparison `op` through the original operator table, as the commands
 * do (JSR ($6467,X) with 8-bit X): op values past 7 reach whatever the
 * table bytes point at, so those go through the generic call. */
static bool ev_test(u8 op, bool wide)
{
    u8 x = (u8)(op << 1);
    u32 table = wide ? 0xC06477 : 0xC06467;
    u16 target = rom16(table + x);
    static const u16 known8[8] = { 0x6487, 0x648F, 0x6497, 0x64A1, 0x64A9, 0x64B5, 0x64BF, 0x64C7 };
    static const u16 known16[8] = { 0x64CF, 0x64DB, 0x64E7, 0x64F5, 0x6503, 0x6511, 0x6523, 0x652F };
    const u16 *known = wide ? known16 : known8;
    dc_set_mx(1, 1);
    g_cpu.X = x;
    for (int k = 0; k < 8; k++)
        if (known[k] == target) { ev_compare_glue(k, wide); return !g_cpu._flag_C; }
    dc_jsr(0xC00000 | target, wide ? 0xC0633D : 0xC06307);
    return !g_cpu._flag_C;
}

/* Finish a conditional: true -> next command after the jump byte at `next`;
 * false -> skip forward by the jump byte. */
static void ev_cond_finish(bool holds)
{
    dc_set_mx(1, 0);
    u16 next = ram16(EV_TMP_NEXT);
    if (holds) { ev_return((u16)(next + 1), true); return; }
    u8 skip = ev_byte(next);
    ram16_set(EV_TMP_D9, skip);
    g_cpu.A = adc_v(next, skip);
    ev_return(g_cpu.A, true);
}

/* 12 var, value, op, jump  If (byte event variable op value). */
DC_ENTRY(C062DE, RTS)
{
    DC_ASSUME_D(C062DE, 0x0100);
    u16 pos = g_cpu.Y;
    ram8_set(EV_TMP_D9, ram8(EV_LOCAL + ev_byte(pos + 1) * 2));
    ram8_set(EV_TMP_OBJ, ev_byte(pos + 2));
    ram16_set(EV_TMP_NEXT, (u16)(pos + 4));
    g_cpu.Y = (u16)(pos + 2);
    ev_cond_finish(ev_test(ev_byte(pos + 3), false));
}
/* 13 var, value16, op, jump  The same on words. */
DC_ENTRY(C06313, RTS)
{
    DC_ASSUME_D(C06313, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_D9, ram16(EV_LOCAL + ev_byte(pos + 1) * 2));
    ram16_set(EV_TMP_OBJ, ev_word(pos + 2));
    ram16_set(EV_TMP_NEXT, (u16)(pos + 5));
    g_cpu.Y = (u16)(pos + 2);
    ev_cond_finish(ev_test(ev_byte(pos + 4), true));
}
/* 14 var1, var2, op, jump  If (byte variable op byte variable). */
DC_ENTRY(C06361, RTS)
{
    DC_ASSUME_D(C06361, 0x0100);
    u16 pos = g_cpu.Y;
    ram8_set(EV_TMP_D9, ram8(EV_LOCAL + ev_byte(pos + 1) * 2));
    ram8_set(EV_TMP_OBJ, ram8(EV_LOCAL + ev_byte(pos + 2) * 2));
    ram16_set(EV_TMP_NEXT, (u16)(pos + 4));
    g_cpu.Y = (u16)(pos + 3);
    ev_cond_finish(ev_test(ev_byte(pos + 3), false));
}
/* 15 var1, var2, op, jump  The same on words. */
DC_ENTRY(C063A4, RTS)
{
    DC_ASSUME_D(C063A4, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_D9, ram16(EV_LOCAL + ev_byte(pos + 1) * 2));
    ram16_set(EV_TMP_OBJ, ram16(EV_LOCAL + ev_byte(pos + 2) * 2));
    ram16_set(EV_TMP_NEXT, (u16)(pos + 4));
    g_cpu.Y = (u16)(pos + 3);
    ev_cond_finish(ev_test(ev_byte(pos + 3), true));
}
/* 16 addr, value, op, jump  If (story memory byte op value); op bit 7 selects
 * 7F:0100-01FF instead of 7F:0000-00FF. */
DC_ENTRY(C063E6, RTS)
{
    DC_ASSUME_D(C063E6, 0x0100);
    u16 pos = g_cpu.Y;
    u8 addr = ev_byte(pos + 1), op = ev_byte(pos + 3);
    ram8_set(EV_TMP_OBJ, ev_byte(pos + 2));
    u16 at = addr;
    if (op & 0x80) { op &= 0x7F; at |= 0x100; }
    ram8_set(EV_TMP_ARG, op);
    ram8_set(EV_TMP_D9, ram8(STORY_MEM + at));
    ram16_set(EV_TMP_NEXT, (u16)(pos + 4));
    g_cpu.Y = (u16)(pos + 1);
    ev_cond_finish(ev_test(op, false));
}

/* The conditionals above switch to 8-bit index registers to call the
 * operator; that also drops the high byte of Y, which the original keeps. */
/* (handled in ev_test via dc_set_mx(1, 1), which clears X/Y high bytes) */

/* 0F / 17 / 1B / 1D  Face up / down / left / right; ends this pass. */
static void ev_face(u8 dir)
{
    u16 obj = ram16(EV_CUR_OBJ);
    ram8_set(OBJ_FACING(obj), dir);
    ram8_set(OBJ_1601(obj), 0);
    dc_set_a8(dir);
    ev_return((u16)(g_cpu.Y + 1), false);
}
DC_ENTRY(C05535, RTS) { ev_face(0); }
DC_ENTRY(C05539, RTS) { ev_face(1); }
DC_ENTRY(C0553D, RTS) { ev_face(2); }
DC_ENTRY(C05541, RTS) { ev_face(3); }

/* 18 value, jump  If the storyline counter (7F:0000) is below value, go on;
 * otherwise skip by jump. */
DC_ENTRY(C06442, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    g_cpu.Y = pos;
    u8 story = ram8(STORY_MEM);
    dc_set_a8(story);
    if (story < ev_byte(pos)) { ev_return((u16)(pos + 2), true); return; }
    u8 skip = ev_byte(pos + 1);
    ram16_set(EV_TMP_D9, skip);
    g_cpu.A = adc_v((u16)(pos + 1), skip);
    ev_return(g_cpu.A, true);
}

/* 19 var  Result = event variable (word). */
DC_ENTRY(C0653B, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    g_cpu.Y = pos;
    u16 v = ram16(EV_LOCAL + ev_byte(pos) * 2);
    ram16_set(EV_RESULT(ram16(EV_CUR_OBJ)), v);
    g_cpu.A = v;
    ev_return((u16)(pos + 1), true);
}
/* 1C addr16  Result = story memory word at 7F:addr. */
DC_ENTRY(C0654E, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u16 v = ram16(STORY_MEM + ev_word(pos));
    g_cpu.Y = (u16)(pos + 1);
    ram16_set(EV_RESULT(ram16(EV_CUR_OBJ)), v);
    g_cpu.A = v;
    ev_return((u16)(pos + 2), true);
}
/* 1A value, jump  If result == value go on, else skip by jump. */
DC_ENTRY(C06568, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    g_cpu.Y = pos;
    u8 r = ram8(EV_RESULT(ram16(EV_CUR_OBJ)));
    dc_set_a8(r);
    if (r == ev_byte(pos)) { ev_return((u16)(pos + 2), true); return; }
    u8 skip = ev_byte(pos + 1);
    ram16_set(EV_TMP_D9, skip);
    g_cpu.A = adc_v((u16)(pos + 1), skip);
    ev_return(g_cpu.A, true);
}

/* Skip forward by the byte at `at` (the conditionals' false branch). */
static void ev_skip_by(u16 at)
{
    u8 skip = ev_byte(at);
    ram16_set(EV_TMP_D9, skip);
    g_cpu.A = adc_v(at, skip);
    ev_return(g_cpu.A, true);
}

/* 1E / 1F / 25 / 26 obj  Make object obj face up / down / left / right. */
static void ev_face_obj(u8 dir)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 obj = ev_byte(pos);
    g_cpu.Y = pos;
    ram8_set(OBJ_FACING(obj), dir);
    ram8_set(OBJ_1601(obj), 0);
    dc_set_a8(dir);
    ev_return((u16)(pos + 1), false);
}
DC_ENTRY(C0556C, RTS) { ev_face_obj(0); }
DC_ENTRY(C05579, RTS) { ev_face_obj(1); }
DC_ENTRY(C05586, RTS) { ev_face_obj(2); }
DC_ENTRY(C05593, RTS) { ev_face_obj(3); }

/* 20 var  Variable = the first party member. */
DC_ENTRY(C0658F, RTS)
{
    u16 pos = g_cpu.Y;
    u8 v = ram8(PARTY_MEMBER(0));
    ram8_set(EV_LOCAL + ev_byte(pos + 1) * 2, v);
    dc_set_a8(v);
    g_cpu.Y = (u16)(pos + 2);
    ev_return(g_cpu.Y, true);
}

/* 21 obj, varx, vary / 22 pc, varx, vary  Store an object's (party
 * member's) tile coordinates in two variables. */
static void ev_store_coords(u16 pos, u16 obj)
{
    u8 tx = ram8(OBJ_TILE_X(obj)), ty = ram8(OBJ_TILE_Y(obj));
    ram8_set(EV_TMP_D9, tx);
    ram8_set(EV_TMP_OBJ, ty);
    ram8_set(EV_LOCAL + ev_byte(pos + 2) * 2, tx);
    ram8_set(EV_LOCAL + ev_byte(pos + 3) * 2, ty);
    dc_set_a8(ty);
    g_cpu.Y = (u16)(pos + 4);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C065AA, RTS) { u16 pos = g_cpu.Y; ev_store_coords(pos, ev_byte(pos + 1)); }
DC_ENTRY(C065EB, RTS) { u16 pos = g_cpu.Y; ev_store_coords(pos, ram8(PARTY_OBJ(ev_byte(pos + 1)))); }

/* 23 obj, var / 24 pc, var  Store an object's (party member's) facing. */
static void ev_store_facing(u16 pos, u16 obj)
{
    u8 f = ram8(OBJ_FACING(obj));
    ram8_set(EV_TMP_D9, f);
    ram8_set(EV_LOCAL + ev_byte(pos + 2) * 2, f);
    dc_set_a8(f);
    g_cpu.Y = (u16)(pos + 3);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C065F8, RTS) { u16 pos = g_cpu.Y; ev_store_facing(pos, ev_byte(pos + 1)); }
DC_ENTRY(C0661F, RTS) { u16 pos = g_cpu.Y; ev_store_facing(pos, ram8(PARTY_OBJ(ev_byte(pos + 1)))); }

/* 27 obj, jump  If the object is active go on, else skip. */
DC_ENTRY(C0662C, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    g_cpu.Y = pos;
    u8 a = ram8(OBJ_0F00(ev_byte(pos)));
    dc_set_a8(a);
    if (a) { ev_return((u16)(pos + 2), true); return; }
    ev_skip_by((u16)(pos + 1));
}

/* 28 obj, jump  If the object is well inside the screen go on, else skip:
 * its tile must be 2..13 columns and 2..12 rows from the camera corner. */
DC_ENTRY(C06655, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    g_cpu.Y = pos;
    u8 obj = ev_byte(pos);
    u8 cx = ram8(FIELD_SCROLL_X_HALF) >> 1, cy = ram8(FIELD_SCROLL_Y_HALF) >> 1;
    ram8_set(EV_TMP_OBJ, cx);
    ram8_set(EV_TMP_ARG, cy);
    u8 dx = (u8)(ram8(OBJ_TILE_X(obj)) - cx);
    bool inside = false;
    if (dx >= 2 && dx < 0x0E) {
        u8 ty = ram8(OBJ_TILE_Y(obj)), dy = (u8)(ty - cy);
        g_cpu._flag_V = (((ty ^ cy) & (ty ^ dy)) >> 7) & 1;   /* SEC; SBC $DD (V reaches the caller on the "inside" path) */
        inside = dy >= 2 && dy < 0x0D;
    }
    if (inside) { ev_return((u16)(pos + 2), true); return; }
    ev_skip_by((u16)(pos + 1));
}
