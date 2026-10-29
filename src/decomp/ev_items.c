/* Field event script: items, gold and party queries (commands C7-CF, D7).
 * They call the inventory service C1:8003 (A = 0 has item, 1 add item,
 * 2 remove item, 3 has gold, 4 add gold, 5 remove gold; Y = item or amount,
 * A != 0 on return = yes) and the menu service C2:8004 (A = 3 is character
 * in the party, 0C count of item X). */
#include "ram.h"

static void call_service(u32 entry, u32 site, u8 op, u16 a_hi, u16 y)
{
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu.Y = y;
    g_cpu.A = (u16)(a_hi & 0xFF00) | op;
    dc_jsl(entry, site);
}
static void go_on(u16 next) { g_cpu.X = next; g_cpu._flag_C = 1; }
static u16 adc_v(u16 a, u16 b) { u16 r = (u16)(a + b); g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1; return r; }   /* TXA; CLC; ADC $D9 (V of the 16-bit add) */
/* Conditional jump: on `yes` go to the next command, else jump forward by
 * the byte at `at` (counted from there). */
static void branch(bool yes, u16 at)
{
    if (yes) { go_on((u16)(at + 1)); return; }
    u8 skip = ev_byte(at);
    ram16_set(EV_TMP_D9, skip);
    g_cpu.A = adc_v(at, skip);
    go_on(g_cpu.A);
}

/* C9 i j  If item i is held go on, else jump j. */
DC_ENTRY(C03780, RTS)
{
    DC_ASSUME_D(C03780, 0x0100);
    u16 pos = g_cpu.Y;
    u8 item = ev_byte((u16)(pos + 1));
    ram16_set(EV_TMP_C7, (u16)(pos + 2));
    call_service(0xC18003, 0xC0378E, 0, 0, item);
    branch((u8)g_cpu.A != 0, ram16(EV_TMP_C7));
}
/* CA i  Add item i;  C7 n: add item var[n];  CB i: remove item i. */
static void item_op(u8 op, u8 item, u32 site)
{
    ram16_set(EV_TMP_C7, (u16)(g_cpu.Y + 2));
    call_service(0xC18003, site, op, 0, item);
    go_on(ram16(EV_TMP_C7));
}
DC_ENTRY(C037B1, RTS) { DC_ASSUME_D(C037B1, 0x0100); item_op(1, ev_byte((u16)(g_cpu.Y + 1)), 0xC037BF); }
DC_ENTRY(C037C7, RTS) { DC_ASSUME_D(C037C7, 0x0100); item_op(1, ram8(EV_LOCAL + ev_byte((u16)(g_cpu.Y + 1)) * 2), 0xC037BF); }
DC_ENTRY(C037E3, RTS) { DC_ASSUME_D(C037E3, 0x0100); item_op(2, ev_byte((u16)(g_cpu.Y + 1)), 0xC037F1); }
/* D7 i n  var[n] = how many of item i are held. */
DC_ENTRY(C037F9, RTS)
{
    DC_ASSUME_D(C037F9, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, (u16)(pos + 2));
    g_cpu.X = ev_byte((u16)(pos + 1));
    g_cpu.A = 0x0C;
    dc_jsl(0xC28004, 0xC03807);
    u8 n = (u8)g_cpu.A;
    ram8_set(EV_TMP_D9, n);
    ram8_set(EV_LOCAL + ev_byte(ram16(EV_TMP_C7)) * 2, n);
    go_on((u16)(ram16(EV_TMP_C7) + 1));
}
/* CC g j  If at least g gold go on, else jump j;  CD g / CE g: add / remove gold. */
DC_ENTRY(C03827, RTS)
{
    DC_ASSUME_D(C03827, 0x0100);
    u16 pos = g_cpu.Y, g = ev_word((u16)(pos + 1));
    ram16_set(EV_TMP_C7, (u16)(pos + 3));
    call_service(0xC18003, 0xC03838, 3, g, g);
    branch((u8)g_cpu.A != 0, ram16(EV_TMP_C7));
}
static void gold_op(u8 op, u32 site)
{
    u16 g = ev_word((u16)(g_cpu.Y + 1));
    ram16_set(EV_TMP_C7, (u16)(g_cpu.Y + 3));
    call_service(0xC18003, site, op, g, g);
    go_on(ram16(EV_TMP_C7));
}
DC_ENTRY(C0385B, RTS) { DC_ASSUME_D(C0385B, 0x0100); gold_op(4, 0xC0386C); }
DC_ENTRY(C03874, RTS) { DC_ASSUME_D(C03874, 0x0100); gold_op(5, 0xC03885); }
/* CF c j  If character c is NOT in the party go on, else jump j. */
DC_ENTRY(C0388D, RTS)
{
    DC_ASSUME_D(C0388D, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, (u16)(pos + 2));
    call_service(0xC28004, 0xC0389B, 3, 0, ev_byte((u16)(pos + 1)));
    branch((u8)g_cpu.A == 0, ram16(EV_TMP_C7));
}

/* D0 c / D1 c  Character c joins / leaves the roster (C2:8004 ops 4 / 5).
 * The carry the service returns is kept. */
static void roster_op(u8 op, u32 site)
{
    ram16_set(EV_TMP_C7, (u16)(g_cpu.Y + 2));
    call_service(0xC28004, site, op, 0, ev_byte((u16)(g_cpu.Y + 1)));
    g_cpu.X = ram16(EV_TMP_C7);
}
DC_ENTRY(C038BE, RTS) { DC_ASSUME_D(C038BE, 0x0100); roster_op(4, 0xC038CC); }
DC_ENTRY(C038D3, RTS) { DC_ASSUME_D(C038D3, 0x0100); roster_op(5, 0xC038E1); }
/* D2 c j  If character c is not in the roster go on, else jump j. */
DC_ENTRY(C038E8, RTS)
{
    DC_ASSUME_D(C038E8, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, (u16)(pos + 2));
    call_service(0xC28004, 0xC038F6, 0, 0, ev_byte((u16)(pos + 1)));
    branch((u8)g_cpu.A == 0, ram16(EV_TMP_C7));
}
/* D5 c x  C2:8004 op 0A with Y = c, X = x (equipment?). */
DC_ENTRY(C03A68, RTS)
{
    DC_ASSUME_D(C03A68, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, (u16)(pos + 3));
    g_cpu.Y = ev_byte((u16)(pos + 1));
    g_cpu.X = ev_byte((u16)(pos + 2));
    g_cpu.A = 0x0A;
    dc_jsl(0xC28004, 0xC03A7C);
    g_cpu.X = ram16(EV_TMP_C7);
}

#define CHAR_OBJ(ch)    (0x018D + (ch))
#define SLOT_OBJ(s)     (0x0197 + (s) * 2)   /* objects of party slots 0-2 (bit 7 = empty) */
#define FIELD_0178      0x0178
#define OBJ_CHAR(o)     (0x1101 + (o))
static void copy_palette(u16 src, u16 dst) { for (int i = 0; i < 0x20; i++) ram8_set((u16)(dst + i), ram8((u16)(src + i))); }

/* D3 c  Character c joins the active party (C2:8004 op 1); its object, if
 * loaded, takes the first free follower slot. */
DC_ENTRY(C03919, RTS)
{
    DC_ASSUME_D(C03919, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, (u16)(pos + 2));
    u8 c = ev_byte((u16)(pos + 1));
    ram16_set(EV_TMP_D9, c);
    call_service(0xC28004, 0xC0392B, 1, 0, c);
    ram8_set(FIELD_0178, 1);
    u8 o = ram8(CHAR_OBJ(c));
    g_cpu.A = o;
    if (!(o & 0x80)) {
        ram8_set(EV_TMP_OBJ, o);
        ram8_set(ram8(SLOT_OBJ(1)) & 0x80 ? SLOT_OBJ(1) : SLOT_OBJ(2), o);
    }
    g_cpu.X = ram16(EV_TMP_C7);
}
/* Is slot object o (bit 7 = empty) character c? Leaves X and the carry as
 * the original's TAX / CMP do. */
static bool slot_is(u8 o, u8 c)
{
    if (o & 0x80) return false;
    g_cpu.X = o;
    u8 ch = ram8(OBJ_CHAR(o));
    g_cpu._flag_C = ch >= c;
    return ch == c;
}
static void copy_palette_mvn(u16 src, u16 dst)
{
    copy_palette(src, dst);
    g_cpu.X = (u16)(src + 0x20);
    g_cpu.Y = (u16)(dst + 0x20);
    g_cpu.A = 0xFFFF;
}

/* D4 c  Character c leaves the active party (C2:8004 op 2): its object
 * becomes a plain object (1100 = 3) and the followers move up a slot, with
 * their sprite palettes (7E:21A0/21C0/21E0). */
DC_ENTRY(C0394E, RTS)
{
    DC_ASSUME_D(C0394E, 0x0100);
    u16 pos = g_cpu.Y;
    u8 c = ev_byte((u16)(pos + 1));
    g_cpu.X = (u16)(pos + 2);
    ram16_set(EV_TMP_C7, g_cpu.X);
    ram8_set(EV_TMP_D9, c);
    if (slot_is(ram8(SLOT_OBJ(0)), c)) {
        ram8_set(OBJ_REMOVED(g_cpu.X), 3);
        u8 s1 = ram8(SLOT_OBJ(1));
        if (!(s1 & 0x80)) {
            ram8_set(OBJ_REMOVED(s1), 0);
            u8 s2 = ram8(SLOT_OBJ(2));
            if (!(s2 & 0x80)) ram8_set(OBJ_REMOVED(s2), 1);
        }
        ram8_set(SLOT_OBJ(0), ram8(SLOT_OBJ(1)));
        ram8_set(SLOT_OBJ(1), ram8(SLOT_OBJ(2)));
        ram8_set(SLOT_OBJ(2), 0x80);
        copy_palette_mvn(0x21C0, 0x21A0);
        copy_palette_mvn(0x21E0, 0x21C0);
    } else {
        u8 s1 = ram8(SLOT_OBJ(1));
        bool shift = true;
        if (!(s1 & 0x80)) {
            if (slot_is(s1, c)) {
                ram8_set(OBJ_REMOVED(s1), 3);
                u8 s2 = ram8(SLOT_OBJ(2));
                if (!(s2 & 0x80)) ram8_set(OBJ_REMOVED(s2), 1);
            } else {
                shift = false;
                u8 s2 = ram8(SLOT_OBJ(2));
                if (slot_is(s2, c)) {
                    ram8_set(OBJ_REMOVED(s2), 3);
                    ram8_set(SLOT_OBJ(2), s2 | 0x80);
                }
            }
        }
        if (shift) {
            ram8_set(SLOT_OBJ(1), ram8(SLOT_OBJ(2)));
            ram8_set(SLOT_OBJ(2), 0x80);
            copy_palette_mvn(0x21E0, 0x21C0);
        }
    }
    g_cpu.Y = c;
    g_cpu.A = 0x02;
    dc_jsl(0xC28004, 0xC039DA);
    g_cpu.X = ram16(EV_TMP_C7);
}
/* D6 c  Character c's object leaves its party slot (slot and 7E:2980 entry
 * emptied, object becomes plain). */
DC_ENTRY(C03A07, RTS)
{
    DC_ASSUME_D(C03A07, 0x0100);
    u16 pos = g_cpu.Y;
    u8 c = ev_byte((u16)(pos + 1));
    ram16_set(EV_TMP_C7, (u16)(pos + 2));
    ram8_set(EV_TMP_D9, c);
    for (int s = 0; s < 3; s++) {
        u8 o = ram8(SLOT_OBJ(s));
        g_cpu.A = o;
        if (!slot_is(o, c)) continue;
        ram8_set(OBJ_REMOVED(o), 3);
        ram8_set(SLOT_OBJ(s), 0x80);
        ram8_set(PARTY_MEMBER(s), 0x80);
        g_cpu.A = 0x80;
        break;
    }
    g_cpu.X = ram16(EV_TMP_C7);
}

/* C8 n  Open special dialogue/menu n ($0125) via flag $0117 bit 6 and wait
 * until $0118 bit 6 reports it closed. */
DC_ENTRY(C03757, RTS)
{
    DC_ASSUME_D(C03757, 0x0100);
    u16 pos = g_cpu.Y;
    if (ram8(0x0117) & 0x40) { g_cpu.X = pos; g_cpu._flag_C = 0; return; }
    if (ram8(0x0118) & 0x40) {
        ram8_set(0x0118, ram8(0x0118) & ~0x40);
        dc_set_a8(0x40);
        go_on((u16)(pos + 2));
        return;
    }
    ram8_set(0x0125, ev_byte((u16)(pos + 1)));
    ram8_set(0x0117, ram8(0x0117) | 0x40);
    ram8_set(0x0118, ram8(0x0118) & ~0x40);
    dc_set_a8(0x40);
    g_cpu.X = pos;
    g_cpu._flag_C = 0;
}
/* D8 a b  Start a battle (7E:2989 = a, 7E:2A1F = b; $0117 |= 21) and wait
 * until $0118 bit 0 reports it over. */
DC_ENTRY(C03A83, RTS)
{
    DC_ASSUME_D(C03A83, 0x0100);
    u16 pos = g_cpu.Y;
    g_cpu._flag_C = 0;
    if (ram8(0x0117) & 0x01) { g_cpu.X = pos; return; }
    if (ram8(0x0118) & 0x01) {
        ram8_set(0x0118, ram8(0x0118) & ~0x01);
        dc_set_a8(0x01);
        g_cpu.X = (u16)(pos + 3);
        return;
    }
    ram8_set(0x2989, ev_byte((u16)(pos + 1)));
    ram8_set(0x2A1F, ev_byte((u16)(pos + 2)));
    u8 f = ram8(0x0117) | 0x21;
    ram8_set(0x0117, f);
    ram8_set(0x011F, 0);
    dc_set_a8(f);
    g_cpu.X = pos;
}

/* ---- moving the party with resident scripts ----------------------------
 * The map script area holds small routines at 7F:2001 + 16FF / 171F / 173F /
 * 175F. D9 and DA make the party objects run them: each object's current
 * position is pushed on its saved-position stack (7F:0580 + prio*0x80 + obj)
 * and the routine becomes its priority-0 function. */
#define PARTY_MOVE_STATE  0x103FC   /* 7F:03FC: D9 state (0 idle, below 4 = still moving) */
#define FOLLOW_STATE      0x103FE   /* 7F:03FE: DA state (below 3 = still moving) */
#define FIELD_011F        0x011F
#define FIELD_DLG_FLAGS   0x0154
#define OBJ_SCRIPT_CNT(o) (0x1001 + (o))

static void redirect_to(u16 slot_addr, bool word_index, u16 entry)
{
    u16 w = ram16(slot_addr);
    u16 i = word_index ? w : (u16)(w & 0xFF);
    g_cpu.X = i;
    u8 prio = ram8(OBJ_PRIORITY(i));
    io8_set(0x4202, prio);
    io8_set(0x4203, 0x80);
    u16 a = (u16)(prio * 0x80), d = (u16)(a + w);
    g_cpu._flag_V = ((~(a ^ w) & (a ^ d)) >> 15) & 1;
    ram16_set(EV_TMP_D9, d);
    ram16_set(0x10580 + d, ram16(OBJ_SCRIPT_POS(i)));
    ram16_set(OBJ_SCRIPT_POS(w), entry);
    dc_set_mx(1, 1);
    u8 o = (u8)w;
    ram8_set(OBJ_PRIORITY(o), 0);
    ram8_set(0x1A80 + o, 0);
    ram8_set(0x1A01 + o, 0);
    ram8_set(OBJ_SCRIPT_CNT(o), 0);
    dc_set_a8(0);
    dc_set_mx(1, 0);
}
/* Script bytes x, y (bit 7 = half tile) into the routine's walk operands at
 * `at` and its half-tile code at at+3. */
static void patch_target(u16 pos, u32 at)
{
    u8 x = ev_byte(pos), y = ev_byte((u16)(pos + 1));
    ram8_set(EV_TMP_OBJ, x);
    ram8_set(at, x & 0x7F);
    ram8_set(EV_TMP_ARG, y);
    ram8_set(at + 1, y & 0x7F);
    u8 h = (u8)(((x & 0x80) ? 2 : 0) | ((y & 0x80) ? 1 : 0));
    ram8_set(at + 3, h);
    dc_set_a8(h);
}
static void clear_ab(void)
{
    u8 v = ram8(0x01AB);
    ram8_set(0x01AC, v);
    ram8_set(0x01AD, v);
    g_cpu.X = v;
    ram8_set(0x10C00 + v, 0);
    ram8_set(0x10D00 + v, 0);
    ram8_set(0x10F00 + v, 0);
    dc_set_a8(0);
}

/* D9 x0 y0 x1 y1 x2 y2  Move the party members to these tiles; waits until
 * all are back in their own scripts. */
DC_ENTRY(C0326C, RTS)
{
    DC_ASSUME_D(C0326C, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, pos);
    u8 st = ram8(PARTY_MOVE_STATE);
    dc_set_a8(st);
    if (st) {
        if (st < 4) { g_cpu.X = pos; g_cpu._flag_C = 0; return; }
        clear_ab();
        for (int s = 0; s < 3; s++) {
            u8 o = ram8(SLOT_OBJ(s));
            dc_set_a8(o);
            if (o & 0x80) continue;
            g_cpu.X = o;
            dc_set_a8(ram8(OBJ_PRIORITY(o)));
            if (!ram8(OBJ_PRIORITY(o))) { g_cpu.X = pos; g_cpu._flag_C = 0; return; }
        }
        ram8_set(PARTY_MOVE_STATE, 0);
        dc_set_a8(0);
        g_cpu.Y = (u16)(pos + 7);
        g_cpu.X = g_cpu.Y;
        g_cpu._flag_C = 0;
        return;
    }
    ram8_set(FIELD_011F, 0);
    static const u16 entry[3] = { 0x171F, 0x173F, 0x175F };
    for (int s = 0; s < 3; s++) {
        if (s > 0 && (ram8(SLOT_OBJ(s)) & 0x80)) {
            ram8_set(PARTY_MOVE_STATE, (u8)(4 - s));
            dc_set_a8((u8)(4 - s));
            g_cpu.X = ram16(EV_TMP_C7);
            g_cpu._flag_C = 0;
            return;
        }
        redirect_to(SLOT_OBJ(s), s == 0, entry[s]);
        patch_target((u16)(ram16(EV_TMP_C7) + 1 + s * 2), 0x10000 + 0x2001 + entry[s] + 8);
    }
    ram8_set(PARTY_MOVE_STATE, 1);
    dc_set_a8(1);
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 0;
}

/* DA  The followers walk back to the leader (resident routine 16FF). */
DC_ENTRY(C0345A, RTS)
{
    DC_ASSUME_D(C0345A, 0x0100);
    u16 pos = g_cpu.Y;
    ram16_set(EV_TMP_C7, pos);
    u8 s1 = ram8(SLOT_OBJ(1));
    g_cpu.A = s1;
    if (s1 & 0x80) { g_cpu.X = (u16)(pos + 1); g_cpu._flag_C = 0; return; }
    u8 f = ram8(FIELD_DLG_FLAGS);
    if (f & 1) {
        u8 st = ram8(FOLLOW_STATE);
        dc_set_a8(st);
        if (st < 3) { g_cpu.X = pos; g_cpu._flag_C = 0; return; }
        ram8_set(FIELD_DLG_FLAGS, f & ~1);
        clear_ab();
        g_cpu.X = (u16)(pos + 1);
        g_cpu._flag_C = 1;
        return;
    }
    ram16_set(EV_TMP_NEXT, pos);
    redirect_to(SLOT_OBJ(1), false, 0x16FF);
    u8 n = 1;
    if (!(ram8(SLOT_OBJ(2)) & 0x80)) redirect_to(SLOT_OBJ(2), false, 0x16FF);
    else n = 2;
    ram8_set(FOLLOW_STATE, n);
    ram8_set(FIELD_DLG_FLAGS, ram8(FIELD_DLG_FLAGS) | 1);
    dc_set_a8(1);
    g_cpu.X = pos;
    g_cpu._flag_C = 0;
}
