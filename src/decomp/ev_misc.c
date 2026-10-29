/* Field event script: screen/effect and input commands 29-32 (bank C0). */
#include "ram.h"

static inline void ev_return(u16 next, bool go_on) { g_cpu.X = next; g_cpu._flag_C = go_on; }
static u16 adc_v(u16 a, u16 b) { u16 r = (u16)(a + b); g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1; return r; }   /* TXA; CLC; ADC $D9 (V of the 16-bit add) */

#define JOY_NEW_LO          0x00F8   /* buttons, low byte (bit 1 = ?, bit 7 = B/A) */
#define JOY_NEW_HI          0x00F9
#define FIELD_DLG_FLAGS     0x0154   /* direct page $54 */
#define EV_STRING_INDEX     0x0BE1   /* 16-bit offset into the FD:A000 string table */
#define EV_STRING_MODE      0x0BE9
#define EV_0BE3             0x0BE3
#define EV_0BE4             0x0BE4
#define FIELD_1D35          0x1D35
#define FIELD_1D36          0x1D36
#define FIELD_1D39          0x1D39
#define FIELD_1D3A          0x1D3A
#define FIELD_1D3F          0x1D3F
#define FIELD_1D40          0x1D40
#define FIELD_SCROLL_DEST_X 0x1D8F   /* 16-bit, pixels */
#define FIELD_SCROLL_DEST_Y 0x1D91   /* 16-bit, pixels */
#define FIELD_1D92          0x1D92
#define EV_TMP_C7           0x01C7   /* 16-bit */
#define EV_TMP_C1           0x01C1   /* 16-bit */
#define PAL_EFFECTS         0x0520   /* 8 palette effect slots, 12 bytes each */
#define PAL_BUFFER          0x2000   /* 7E:2000 current palettes; 7E:2200 target palettes */

/* 29 n  Show string n of the FD:A000 table (bit 7: second style). The table
 * is scanned for the start of the string (entries end with a byte >= $80). */
DC_ENTRY(C03E84, RTS)
{
    DC_ASSUME_D(C03E84, 0x0100);
    u16 pos = g_cpu.Y;
    dc_push16(pos);
    ram8_set(FIELD_1D36, ram8(FIELD_1D36) | 4);
    ram8_set(FIELD_1D35, ram8(FIELD_1D35) | 4);
    ram8_set(FIELD_1D39, 0);
    ram8_set(FIELD_1D3A, 0);
    dc_jsr(0xC06E27, 0xC03E93);
    dc_jsr(0xC075A0, 0xC03E96);
    g_cpu.Y = dc_pull16();
    u8 n = ev_byte(pos + 1);
    if (n & 0x80) { n &= 0x7F; ram8_set(EV_STRING_MODE, 2); }
    else ram8_set(EV_STRING_MODE, 1);
    ram8_set(EV_TMP_D9, n);
    ram16_set(EV_TMP_C7, (u16)(pos + 1));
    u16 off = 0; u8 k = 0;
    for (; k != n; k++) {
        while (!(rom8(0xFDA000 + off) & 0x80)) off++;
        off++;
    }
    ram8_set(EV_TMP_ARG, k);
    ram16_set(EV_STRING_INDEX, off);
    dc_set_a8(n);
    ev_return((u16)(pos + 2), true);
}

/* 2A / 2B / 32  Set dialogue/field flag bit 2 / 3 / 4 at $0154; end the pass. */
static void ev_set_flag54(u8 bit)
{
    ram8_set(FIELD_DLG_FLAGS, ram8(FIELD_DLG_FLAGS) | bit);
    dc_set_a8(bit);
    ev_return((u16)(g_cpu.Y + 1), false);
}
DC_ENTRY(C03ED6, RTS) { DC_ASSUME_D(C03ED6, 0x0100); ev_set_flag54(0x04); }
DC_ENTRY(C03EDE, RTS) { DC_ASSUME_D(C03EDE, 0x0100); ev_set_flag54(0x08); }
DC_ENTRY(C03EE2, RTS) { DC_ASSUME_D(C03EE2, 0x0100); ev_set_flag54(0x10); }

/* 2C x, y  Scroll the screen to tile (x, y); end the pass. */
DC_ENTRY(C03EE6, RTS)
{
    u16 pos = g_cpu.Y;
    ram8_set(FIELD_1D3F, 0); ram8_set(FIELD_1D40, 0); ram8_set(FIELD_1D92, 0);
    ram16_set(FIELD_SCROLL_DEST_X, (u16)(ev_byte(pos + 1) << 3));
    u16 y = (u16)(ev_byte(pos + 2) << 3);
    ram16_set(FIELD_SCROLL_DEST_Y, y);
    g_cpu.A = y;
    ev_return((u16)(pos + 3), false);
}

/* 2F a, b  Set $0BE3 / $0BE4. */
DC_ENTRY(C03F12, RTS)
{
    u16 pos = g_cpu.Y;
    ram8_set(EV_0BE3, ev_byte(pos + 1));
    u8 b = ev_byte(pos + 2);
    ram8_set(EV_0BE4, b);
    dc_set_a8(b);
    ev_return((u16)(pos + 3), true);
}

/* C0:4B2C  Find a free palette effect slot: Y = its offset, carry clear;
 * carry set when all 8 are in use (Y = $60). */
static bool pal_effect_free_slot(u16 *slot)
{
    u16 y = 0;
    for (;;) {
        if (ram8(PAL_EFFECTS + y) == 0) { *slot = y; return true; }
        y = (u16)(y + 12);
        if (y >= 0x60) { *slot = y; return false; }
    }
}
DC_ENTRY(C04B2C, RTS)
{
    u16 y; bool ok = pal_effect_free_slot(&y);
    g_cpu.Y = y;
    g_cpu.A = (u16)((g_cpu.A & 0xFF00) | ram8(PAL_EFFECTS + (ok ? y : (u16)(y - 12))));
    g_cpu._flag_C = !ok;
}

/* 2E mode, ...  Palette command. Modes 4x/5x start a palette effect in a
 * free slot (waits for one: the command repeats while all are busy); 8x
 * copies colors from the script into the current and target palettes. */
DC_ENTRY(C04A4E, RTS)
{
    DC_ASSUME_D(C04A4E, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, pos);
    u8 mode = ev_byte(pos + 1);
    ram8_set(EV_TMP_D9, mode);
    u8 kind = mode & 0xF0;
    if (kind == 0x40 || kind == 0x50) {
        u16 y;
        bool ok = pal_effect_free_slot(&y);
        g_cpu.Y = y;
        if (!ok) { dc_set_a8(ram8(PAL_EFFECTS + y - 12)); ev_return((u16)(pos + 6), false); return; }
        u16 e = (u16)(PAL_EFFECTS + y);
        ram8_set(e + 0, kind);
        ram8_set(e + 6, mode & 0x0F);
        ram8_set(e + 1, ev_byte(pos + 2));
        ram8_set(e + 2, ev_byte(pos + 3));
        u8 c = ev_byte(pos + 4);
        ram8_set(EV_TMP_ARG, c);
        ram8_set(e + 5, ev_byte(pos + 5));
        ram8_set(e + 4, 0);
        ram8_set(e + 3, 8);
        ram8_set(EV_TMP_D9, (u8)((c & 0xF0) >> 4));
        ram8_set(e + 7, (u8)((c & 0xF0) | (c >> 4)));
        u8 lo = (u8)((c & 0x0F) << 4);
        ram8_set(EV_TMP_D9, lo);
        u8 v = (u8)((c & 0x0F) | lo);
        ram8_set(e + 8, v);
        dc_set_a8(v);
        ev_return((u16)(pos + 6), true);
        return;
    }
    if (kind == 0x80) {
        u16 first = (u16)(ev_byte(pos + 2) * 2);
        ram16_set(EV_TMP_ARG, first);
        u16 n = (u16)(ev_word(pos + 3) - 3);
        ram16_set(EV_TMP_C1, n);
        u16 src = (u16)(pos + 5 + 0x2001);
        ram16_set(EV_TMP_OBJ, src);
        u32 count = (u32)n + 1;
        for (u32 k = 0; k < count; k++)
            ram8_set((u16)(first + 0x2200 + k), ram8(0x10000 + (u16)(src + k)));
        for (u32 k = 0; k < count; k++)
            ram8_set((u16)(first + 0x2000 + k), ram8(0x10000 + (u16)(src + k)));
        g_cpu.Y = (u16)(first + 0x2000 + count);
        u16 x = (u16)(src + count - 0x2001);
        g_cpu.A = x;
        ev_return(x, true);
        return;
    }
    dc_set_a8(kind);
    ev_return((u16)(pos + 2), true);
}

/* 2D / 30 / 31 jump  If any button / button bit 1 / button bit 7 is pressed
 * go on, else skip by jump. */
static void ev_if_buttons(bool pressed)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    if (pressed) { g_cpu.Y = (u16)(pos + 1); ev_return(g_cpu.Y, true); return; }
    g_cpu.Y = pos;
    u8 skip = ev_byte(pos);
    ram16_set(EV_TMP_D9, skip);
    g_cpu.A = adc_v(pos, skip);
    ev_return(g_cpu.A, true);
}
DC_ENTRY(C066A5, RTS)
{
    u8 lo = ram8(JOY_NEW_LO), hi = ram8(JOY_NEW_HI);
    dc_set_a8(lo ? lo : hi);
    ev_if_buttons(lo || hi);
}
DC_ENTRY(C066B2, RTS) { u8 lo = ram8(JOY_NEW_LO); dc_set_a8(lo); ev_if_buttons(lo & 0x02); }
DC_ENTRY(C066BC, RTS) { u8 lo = ram8(JOY_NEW_LO); dc_set_a8(lo); ev_if_buttons(lo & 0x80); }

/* 34-39  If a held button (word $00F2, low byte) is down, go on; else jump. */
#define JOY_HELD_LO         0x00F2
static void ev_if_held(u8 mask) { u8 v = ram8(JOY_HELD_LO); dc_set_a8(v); ev_if_buttons(v & mask); }
DC_ENTRY(C066C6, RTS) { ev_if_held(0x80); }
DC_ENTRY(C066E5, RTS) { ev_if_held(0x08); }
DC_ENTRY(C066EF, RTS) { ev_if_held(0x40); }
DC_ENTRY(C066F9, RTS) { ev_if_held(0x04); }
DC_ENTRY(C06705, RTS) { ev_if_held(0x20); }
DC_ENTRY(C0670F, RTS) { ev_if_held(0x10); }

/* 3B 3C 3F  Like above on the "pressed since last check" latches $0150/$0151;
 * a hit consumes the latch bit. */
#define JOY_LATCH_LO        0x0150
#define JOY_LATCH_HI        0x0151
static void ev_if_latch(u32 addr, u8 mask)
{
    u8 hit = ram8(addr) & mask;
    dc_set_a8(hit);
    if (hit) ram8_set(addr, ram8(addr) & ~hit);
    ev_if_buttons(hit);
}
DC_ENTRY(C06719, RTS) { DC_ASSUME_D(C06719, 0x0100); ev_if_latch(JOY_LATCH_LO, 0x02); }
DC_ENTRY(C06724, RTS) { DC_ASSUME_D(C06724, 0x0100); ev_if_latch(JOY_LATCH_LO, 0x80); }
DC_ENTRY(C06732, RTS) { DC_ASSUME_D(C06732, 0x0100); ev_if_latch(JOY_LATCH_HI, 0x80); }

/* 33  Load palette: arg selects one of 8 sprite palettes (0F81[obj] = arg*2&0E);
 * 0x18 bytes from E4:[$1400+obj] go into both copies of the palette buffer
 * (7E:2100+pal*16+2 and +0x200). */
#define OBJ_PALETTE(o)      (0x0F81 + (o))
#define OBJ_PAL_SRC(o)      (0x1400 + (o))
DC_ENTRY(C03711, RTS)
{
    DC_ASSUME_D(C03711, 0x0100);
    ram8_set(FIELD_DLG_FLAGS, ram8(FIELD_DLG_FLAGS) | 0x80);
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 pal = (u8)((ev_byte(pos) << 1) & 0x0E);
    ram16_set(0x01C7, (u16)(pos + 1));
    u16 obj = ram16(EV_CUR_OBJ);
    ram8_set(OBJ_PALETTE(obj), pal);
    u16 dst = (u16)(pal * 16 + 2 + 0x2100);
    u16 src = ram16(OBJ_PAL_SRC(obj));
    for (int i = 0; i < 0x18; i++) {
        u8 b = bus8(0xE40000 | (u16)(src + i));
        ram8_set((u16)(dst + i), b);
        ram8_set((u16)(dst + 0x200 + i), b);
    }
    u16 dst2 = (u16)(dst + 0x200);
    g_cpu._flag_V = ((~(dst ^ 0x0200) & (dst ^ dst2)) & 0x8000) != 0;
    g_cpu.A = 0xFFFF;
    g_cpu.Y = (u16)(dst2 + 0x18);
    g_cpu.X = ram16(0x01C7);
    dc_set_flags_nz16(g_cpu.X);
    g_cpu._flag_C = 1;
}
DC_ENTRY(C06740, RTS) { DC_ASSUME_D(C06740, 0x0100); ev_if_latch(JOY_LATCH_HI, 0x08); }
DC_ENTRY(C0674E, RTS) { DC_ASSUME_D(C0674E, 0x0100); ev_if_latch(JOY_LATCH_HI, 0x40); }
DC_ENTRY(C0675C, RTS) { DC_ASSUME_D(C0675C, 0x0100); ev_if_latch(JOY_LATCH_HI, 0x04); }
DC_ENTRY(C0676A, RTS) { DC_ASSUME_D(C0676A, 0x0100); ev_if_latch(JOY_LATCH_HI, 0x20); }
DC_ENTRY(C06778, RTS) { DC_ASSUME_D(C06778, 0x0100); ev_if_latch(JOY_LATCH_HI, 0x10); }

/* 47  $016B = arg (meaning not known yet). */
DC_ENTRY(C06788, RTS)
{
    DC_ASSUME_D(C06788, 0x0100);
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 v = ev_byte(pos);
    dc_set_a8(v);
    ram8_set(0x016B, v);
    g_cpu.Y = (u16)(pos + 1);
    ev_return(g_cpu.Y, true);
}

/* 88 mode ...  Palette effects on the current object's sprite palette
 * (colours $80 + palette*8 on):
 *   00         stop its effect and reload the palette from ROM
 *   20/30 c d  start effect `mode` in a free slot on colours c (hi: first, lo: count)
 *   4x/5x ...  like command 2E 4x/5x, relative to the object's palette
 *   8x len ..  copy colours from the script to colour x of the palette
 * Starting an effect waits (repeats) while all 8 slots are busy. */
#define OBJ_PAL_EFFECT(o)   (0x10B80 + (o))   /* 7F:0B80: 16-bit slot offset, FFFF = none */
#define OBJ_PALETTE(o)      (0x0F81 + (o))
#define EV_TMP_DB           0x01DB
#define EV_TMP_DD           0x01DD
static u8 add8v(u8 a, u8 b) { u8 r = (u8)(a + b); g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 7) & 1; return r; }
static u8 obj_first_colour(u16 obj) { return (u8)(ram8(OBJ_PALETTE(obj)) << 3); }   /* + $80 by the caller */
DC_ENTRY(C04892, RTS)
{
    DC_ASSUME_D(C04892, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, pos);
    u8 mode = ev_byte((u16)(pos + 1));
    u16 obj = ram16(EV_CUR_OBJ);
    if (mode == 0) {
        u16 slot = ram16(OBJ_PAL_EFFECT(obj));
        if (!(slot & 0x8000)) {
            ram16_set(OBJ_PAL_EFFECT(obj), 0xFFFF);
            ram8_set(PAL_EFFECTS + slot, 0);
            u16 dst = (u16)((ram8(OBJ_PALETTE(obj)) << 4) + 2 + 0x2100);
            u16 src = ram16(0x1400 + obj);
            for (int i = 0; i < 0x18; i++) {
                u8 b = bus8(0xE40000 | (u16)(src + i));
                ram8_set((u16)(dst + i), b);
                ram8_set((u16)(dst + 0x200 + i), b);
            }
            u16 dst2 = (u16)(dst + 0x200);
            g_cpu._flag_V = ((~(dst ^ 0x0200) & (dst ^ dst2)) & 0x8000) != 0;
            g_cpu.A = 0xFFFF;
            g_cpu.Y = (u16)(dst2 + 0x18);
        } else {
            g_cpu.A = slot;
        }
        ev_return((u16)(pos + 2), true);
        return;
    }
    ram8_set(EV_TMP_D9, mode);
    u8 kind = mode & 0xF0;
    if (mode == 0x20 || mode == 0x30 || kind == 0x40 || kind == 0x50) {
        bool simple = mode == 0x20 || mode == 0x30;
        u16 y;
        bool ok = pal_effect_free_slot(&y);
        g_cpu.Y = y;
        if (!ok) {
            dc_set_a8(ram8(PAL_EFFECTS + y - 12));
            ev_return((u16)(pos + (simple ? 4 : 5)), false);
            return;
        }
        u16 e = (u16)(PAL_EFFECTS + y);
        if (simple) {
            ram16_set(OBJ_PAL_EFFECT(obj), y);
            ram8_set(e + 0, mode);
        } else {
            ram8_set(e + 0, kind);
            ram8_set(e + 6, mode & 0x0F);
        }
        u8 base = add8v(obj_first_colour(obj), 0x80);
        ram8_set(EV_TMP_DB, base);
        u8 c = ev_byte((u16)(pos + 2));
        ram8_set(EV_TMP_D9, c);
        u8 d = 0;
        if (!simple) { d = ev_byte((u16)(pos + 3)); ram8_set(EV_TMP_DD, d); }
        ram8_set(e + 5, ev_byte((u16)(pos + (simple ? 3 : 4))));
        ram8_set(e + 4, 0);
        if (!simple) ram8_set(e + 3, 8);
        ram8_set(e + 2, c & 0x0F);
        u8 first = add8v((u8)(c >> 4), base);
        ram8_set(e + 1, first);
        dc_set_a8(first);
        if (!simple) {
            ram8_set(EV_TMP_D9, (u8)((d & 0xF0) >> 4));
            ram8_set(e + 7, (u8)((d & 0xF0) | (d >> 4)));
            u8 lo = (u8)((d & 0x0F) << 4);
            ram8_set(EV_TMP_D9, lo);
            u8 v = (u8)((d & 0x0F) | lo);
            ram8_set(e + 8, v);
            dc_set_a8(v);
        }
        ev_return((u16)(pos + (simple ? 4 : 5)), true);
        return;
    }
    if (kind == 0x80) {
        u8 base = add8v(obj_first_colour(obj), 0x80);
        u16 dd = (u16)(((mode & 0x0F) | base) * 2);
        ram16_set(EV_TMP_DD, dd);
        u16 n = (u16)(ev_word((u16)(pos + 2)) - 3);
        ram16_set(EV_TMP_C1, n);
        u16 src = (u16)(pos + 4 + 0x2001);
        ram16_set(EV_TMP_DB, src);
        u32 count = (u32)n + 1;
        for (u32 k = 0; k < count; k++)
            ram8_set((u16)(dd + 0x2200 + k), ram8(0x10000 + (u16)(src + k)));
        for (u32 k = 0; k < count; k++)
            ram8_set((u16)(dd + 0x2000 + k), ram8(0x10000 + (u16)(src + k)));
        g_cpu.Y = (u16)(dd + 0x2000 + count);
        u16 xe = (u16)(src + count), x = (u16)(xe - 0x2001);
        g_cpu._flag_V = (((xe ^ 0x2001) & (xe ^ x)) >> 15) & 1;
        g_cpu.A = x;
        ev_return(x, true);
        return;
    }
    ev_return((u16)(pos + 2), true);
}
