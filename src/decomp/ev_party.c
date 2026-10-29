/* Field event script: loading playable characters as objects (commands 57 5C
 * 62 68 6A 6C 6D 80) and the shared sprite loader C0:448D (bank C0). */
#include "ram.h"

#define CHAR_OBJ(ch)        (0x018D + (ch))    /* character -> object that plays it */
#define LEADER_OBJ          0x0197             /* 16-bit: object of party slot 0 (bit 7 = none) */
#define SLOT1_OBJ           0x0199
#define SLOT2_OBJ           0x019B
#define EV_SPRITE_ROW       0x01BF             /* 16-bit: sprite id * 5 */
#define EV_TMP_C7           0x01C7
#define FIELD_0178          0x0178
#define FIELD_01AB          0x01AB
#define ENTRY_TILE_X        0x0102             /* where the party enters the map */
#define ENTRY_TILE_Y        0x0103
#define ENTRY_FLAGS         0x0104             /* bits 0-1 facing, 2 x half-tile, 3 y half-tile */
#define PARTY_SLOT_OBJ(s)   (0x0B89 + (s))     /* 0B89/0B8A/0B8B: object of party slot s (16-bit stores) */
#define SPRITE_TABLE        0xE4F000           /* 5 bytes per sprite */

#define OBJ_GFX(o)          (0x1200 + (o))
#define OBJ_1201(o)         (0x1201 + (o))
#define OBJ_GFX_PTR(o)      (0x1280 + (o))
#define OBJ_ANIM_BANK(o)    (0x1300 + (o))
#define OBJ_ANIM_PTR(o)     (0x1380 + (o))
#define OBJ_PAL_SRC(o)      (0x1400 + (o))
#define OBJ_1480(o)         (0x1480 + (o))
#define OBJ_1500(o)         (0x1500 + (o))
#define OBJ_1580(o)         (0x1580 + (o))
#define OBJ_1680(o)         (0x1680 + (o))
#define OBJ_1681(o)         (0x1681 + (o))
#define OBJ_0C00(o)         (0x0C00 + (o))
#define OBJ_0D00(o)         (0x0D00 + (o))
#define OBJ_1700(o)         (0x1700 + (o))
#define OBJ_1900(o)         (0x1900 + (o))
#define OBJ_1B01(o)         (0x1B01 + (o))
#define OBJ_CHAR(o)         (0x1101 + (o))

static inline u32 add24(u32 a, u16 y) { return (a + y) & 0xFFFFFF; }
static inline u16 far16(u32 a) { return (u16)(bus8(a) | bus8((a + 1) & 0xFFFFFF) << 8); }

/* Bytes 1-4 of a sprite row: animation pointer and bank (via $B2), palette
 * source (byte * 0x18), sizes (via $B5/$B8), 1201. Leaves V as the original's
 * SBC does. */
static void sprite_fields(u32 rowaddr, u16 obj)
{
    u16 y = (u16)(rom8(rowaddr + 1) * 3);
    ram16_set(EV_TMP_D9, rom8(rowaddr + 1));
    ram16_set(OBJ_ANIM_PTR(obj), far16(add24(ram24(0x01B2), y)));
    ram8_set(OBJ_ANIM_BANK(obj), bus8(add24(ram24(0x01B2), (u16)(y + 2))));

    u8 pal = rom8(rowaddr + 2);
    io8_set(0x4202, pal);
    io8_set(0x4203, 0x18);
    ram16_set(OBJ_PAL_SRC(obj), (u16)(pal * 0x18));

    y = (u16)(rom8(rowaddr + 3) * 2);
    u16 a0 = far16(add24(ram24(0x01B5), y));
    u16 a1 = far16(add24(ram24(0x01B5), (u16)(y + 2)));
    u16 d = (u16)(a1 - a0);
    g_cpu._flag_V = (((a1 ^ a0) & (a1 ^ d)) >> 15) & 1;
    ram16_set(OBJ_1480(obj), (u16)(d >> 2));
    ram16_set(OBJ_1500(obj), a0);
    ram16_set(OBJ_1580(obj), far16(add24(ram24(0x01B8), y)));

    ram8_set(OBJ_1201(obj), rom8(rowaddr + 4));
    g_cpu.Y = y;
}

/* Sprite loader: fills the current object's graphics, animation and palette
 * fields from sprite id's 5-byte row at E4:F000 and the tables the map loader
 * pointed $AF/$B2/$B5/$B8 at. Called with Y = command position; returns
 * Y = X = position + 1. */
static void load_sprite(u8 id)
{
    io8_set(0x4202, id);
    g_cpu.Y++;
    io8_set(0x4203, 5);
    ram16_set(EV_TMP_C7, g_cpu.Y);
    u16 row = (u16)(id * 5);
    ram16_set(EV_SPRITE_ROW, row);
    u16 obj = ram16(EV_CUR_OBJ);

    sprite_fields(SPRITE_TABLE + row, obj);

    u8 g = rom8(SPRITE_TABLE + row);
    ram16_set(EV_TMP_D9, g);
    u16 y = (u16)(g * 3);
    ram16_set(OBJ_GFX_PTR(obj), far16(add24(ram24(0x01AF), y)));
    ram8_set(OBJ_GFX(obj), bus8(add24(ram24(0x01AF), (u16)(y + 2))));

    ram16_set(OBJ_0C00(obj), 0x2020);
    ram8_set(OBJ_1680(obj), 0);
    ram8_set(OBJ_1681(obj), 0);
    ram8_set(OBJ_FACING(obj), 1);
    g_cpu.A = 0x2001;
    g_cpu._flag_V = 0;
    g_cpu.Y = g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 1;
}
DC_ENTRY(C0448D, RTS) { DC_ASSUME_D(C0448D, 0x0100); load_sprite((u8)g_cpu.A); }

/* Object placed at the map entrance point. */
static void place_at_entry(u16 obj)
{
    u8 f = ram8(ENTRY_FLAGS);
    ram8_set(OBJ_TILE_X(obj), ram8(ENTRY_TILE_X));
    ram8_set(OBJ_TILE_Y(obj), ram8(ENTRY_TILE_Y));
    ram8_set(OBJ_FACING(obj), f & 3);
    ram8_set(OBJ_POS_Y(obj), (f & 8) ? 0x7F : 0xFF);
    ram8_set(OBJ_POS_X(obj), (f & 4) ? 0x00 : 0x80);
}

/* The current object becomes character ch: removed if ch is not in the party,
 * else it gets the character's sprite and its party slot's fields. Slot 0
 * starts at the map entrance; slots 1 and 2 start on the leader. */
static void load_character(u8 ch)
{
    static const u16 slot_obj[3] = { LEADER_OBJ, SLOT1_OBJ, SLOT2_OBJ };
    static const u16 v0d00[3] = { 0x08, 0x20, 0x28 }, v1700[3] = { 0x00, 0x20, 0x40 };
    u16 obj = ram16(EV_CUR_OBJ);
    int slot = -1;
    for (int s = 0; s < 3 && slot < 0; s++)
        if (ram8(PARTY_MEMBER(s)) == ch) slot = s;
    if (slot < 0) {
        ram8_set(OBJ_REMOVED(obj), 0x80);
        ram8_set(OBJ_CHAR(obj), ch);
        g_cpu.X = (u16)(g_cpu.Y + 1);
        g_cpu._flag_C = 1;
        return;
    }
    ram8_set(OBJ_REMOVED(obj), (u8)slot);
    ram8_set(OBJ_MOVE_PROPS(obj), 0);
    ram8_set(OBJ_1A81(obj), 1);
    ram8_set(OBJ_1B01(obj), 1);
    ram8_set(OBJ_CHAR(obj), ch);
    ram16_set(slot_obj[slot], obj);
    load_sprite(ch);
    if (slot > 0 && ram8(FIELD_0178)) { ram8_set(FIELD_0178, 2); ram8_set(OBJ_1601(obj), 0); }
    ram16_set(PARTY_SLOT_OBJ(slot), obj);
    ram16_set(OBJ_0D00(obj), v0d00[slot]);
    ram16_set(OBJ_1700(obj), v1700[slot]);
    ram8_set(OBJ_EV_DISABLED(obj), (u8)((ram8(OBJ_EV_DISABLED(obj)) & 0x80) | 1));
    if (slot > 0) {
        ram8_set(FIELD_01AB, 0); ram8_set(FIELD_01AB + 1, 0); ram8_set(FIELD_01AB + 2, 0);
    }
    u8 leader = ram8(LEADER_OBJ);
    if (slot == 0 || (leader & 0x80)) {
        place_at_entry(obj);
    } else {
        ram16_set(OBJ_POS_X(obj), ram16(OBJ_POS_X(leader)));
        ram16_set(OBJ_POS_Y(obj), ram16(OBJ_POS_Y(leader)));
        ram16_set(OBJ_1900(obj), 0);
        ram8_set(OBJ_FACING(obj), ram8(OBJ_FACING(leader)));
    }
    g_cpu.X = g_cpu.Y;
    g_cpu._flag_C = 1;
}

/* 57 5C 62 6A 68 6C 6D  Load character 0..6 (Crono, Marle, Lucca, Robo, Frog, Ayla, Magus). */
static void ev_load_char(u8 ch)
{
    ram8_set(CHAR_OBJ(ch), ram8(EV_CUR_OBJ));
    ram8_set(EV_TMP_D9, ch);
    load_character(ch);
}
DC_ENTRY(C041E4, RTS) { DC_ASSUME_D(C041E4, 0x0100); ev_load_char(0); }
DC_ENTRY(C041EC, RTS) { DC_ASSUME_D(C041EC, 0x0100); ev_load_char(1); }
DC_ENTRY(C041F4, RTS) { DC_ASSUME_D(C041F4, 0x0100); ev_load_char(2); }
DC_ENTRY(C04204, RTS) { DC_ASSUME_D(C04204, 0x0100); ev_load_char(3); }
DC_ENTRY(C041FC, RTS) { DC_ASSUME_D(C041FC, 0x0100); ev_load_char(4); }
DC_ENTRY(C0420C, RTS) { DC_ASSUME_D(C0420C, 0x0100); ev_load_char(5); }
DC_ENTRY(C04214, RTS) { DC_ASSUME_D(C04214, 0x0100); ev_load_char(6); }
/* 80 ch  Load character ch (ch above 6: no character -> object entry kept). */
DC_ENTRY(C0421E, RTS)
{
    DC_ASSUME_D(C0421E, 0x0100);
    g_cpu.Y++;
    u8 ch = ev_byte(g_cpu.Y);
    ram8_set(EV_TMP_D9, ch);
    if (ch <= 6) ram8_set(CHAR_OBJ(ch), ram8(EV_CUR_OBJ));
    load_character(ch);
}
/* 81 ch  The current object shows character ch's sprite, as a non-party
 * object (1100 = 3). */
DC_ENTRY(C04476, RTS)
{
    DC_ASSUME_D(C04476, 0x0100);
    u16 obj = ram16(EV_CUR_OBJ);
    ram8_set(OBJ_REMOVED(obj), 3);
    ram8_set(OBJ_1B01(obj), 1);
    g_cpu.Y++;
    u8 ch = ev_byte(g_cpu.Y);
    ram8_set(OBJ_CHAR(obj), ch);
    load_sprite(ch);
}

/* Fatal error: C0:2E1E blanks the screen to colour `color` and hangs. */
void fatal_error(u16 color, u32 site) { g_cpu.X = color; dc_jsr(0xC02E1E, site); }

/* Sprite graphics cache: 16 slots of (graphics id, object that owns the
 * decompressed copy). */
#define GFX_CACHE_ID(i)     (0x0920 + (i))
#define GFX_CACHE_OWNER(i)  (0x0930 + (i))
#define GFX_NEXT_ADDR       0x0171   /* 16-bit: next free address in 7F for sprite graphics */
#define GFX_LOADED          0x0173   /* sprites decompressed so far on this map */
#define EV_GFX_ID           0x01E3

/* C0:5C90  Find graphics $E3 in the cache: found -> carry set, A = owner.
 * Else claim a free slot for the current object (carry clear); none free is fatal. */
static bool gfx_cache_find(void)
{
    u8 id = ram8(EV_GFX_ID);
    for (u16 i = 0; i < 0x10; i++)
        if (ram8(GFX_CACHE_ID(i)) == id) {
            g_cpu.X = i; dc_set_a8(ram8(GFX_CACHE_OWNER(i))); g_cpu._flag_C = 1;
            return true;
        }
    for (u16 i = 0; i < 0x10; i++)
        if (ram8(GFX_CACHE_ID(i)) == 0xFF) {
            ram8_set(GFX_CACHE_ID(i), id);
            ram8_set(GFX_CACHE_OWNER(i), ram8(EV_CUR_OBJ));
            g_cpu.X = i; dc_set_a8(ram8(EV_CUR_OBJ)); g_cpu._flag_C = 0;
            return false;
        }
    dc_set_a8(0xFF);
    fatal_error(0x4010, 0xC05CB3);
    return false;
}
DC_ENTRY(C05C90, RTS) { DC_ASSUME_D(C05C90, 0x0100); gfx_cache_find(); }

/* C0:4557  Start an object with a sprite of its own: 1100 = A, character
 * byte from the script, graphics at the next free 7F address. */
static void npc_begin(u8 kind)
{
    u16 obj = ram16(EV_CUR_OBJ);
    ram8_set(OBJ_REMOVED(obj), kind);
    g_cpu.Y++;
    u8 id = ev_byte(g_cpu.Y);
    ram8_set(OBJ_CHAR(obj), id);
    io8_set(0x4202, id);
    ram16_set(OBJ_GFX_PTR(obj), ram16(GFX_NEXT_ADDR));
    ram16_set(OBJ_0C00(obj), 0x2020);
    ram8_set(OBJ_GFX(obj), 0x7F);
    ram8_set(OBJ_FACING(obj), 1);
    ram8_set(OBJ_1680(obj), 0);
    ram8_set(OBJ_1681(obj), 0);
    g_cpu.Y++;
    g_cpu.X = obj;
    g_cpu.A = 0x2000;
    g_cpu._flag_C = 0;
}
DC_ENTRY(C04557, RTS) { DC_ASSUME_D(C04557, 0x0100); npc_begin((u8)g_cpu.A); }

/* Remember where this map's n-th decompressed sprite starts: 7E:29C8, 29D4, 29E0. */
static void gfx_note_start(void)
{
    u8 n = ram8(GFX_LOADED);
    u32 a = n == 1 ? 0x29D4 : ((u8)(n - 1) & 0x80) ? 0x29C8 : 0x29E0;
    ram16_set(a, ram16(GFX_NEXT_ADDR));
}

/* Decompress sprite graphics $E3 (pointer table $AF) to 7F at the next free
 * address and advance it; 7F running out is fatal. */
static void gfx_decompress(bool note_start)
{
    u8 g = ram8(EV_GFX_ID);
    ram16_set(EV_TMP_D9, g);
    u16 y = (u16)(g * 3);
    u32 src = ram24(0x01AF);
    ram16_set(0x0300, far16(add24(src, y)));
    ram16_set(0x0303, ram16(GFX_NEXT_ADDR));
    if (note_start) gfx_note_start();
    ram8_set(0x0302, bus8(add24(src, (u16)(y + 2))));
    ram8_set(0x0305, 0x7F);
    g_cpu.Y = (u16)(y + 2);
    g_cpu.A = (u16)((ram16(GFX_NEXT_ADDR) & 0xFF00) | 0x7F);
    dc_jsl(0xC30002, note_start ? 0xC046BC : 0xC04825);   /* decompress 0300 -> 7F:[0303], size in 0306 */
    dc_set_mx(0, g_cpu.x_flag);
    u16 a = ram16(GFX_NEXT_ADDR), b = ram16(0x0306), sum = (u16)(a + b);
    g_cpu._flag_V = ((~(a ^ b) & (a ^ sum)) >> 15) & 1;
    ram16_set(GFX_NEXT_ADDR, sum);
    g_cpu.A = sum;
    dc_set_mx(1, g_cpu.x_flag);
    if (sum < 0x8000) fatal_error(0x7C1F, note_start ? 0xC046D4 : 0xC0483D);   /* 7F is full */
}

/* 82 n  The current object becomes NPC n (sprite row n + 7): its graphics
 * are decompressed into 7F at the next free address, or shared with an
 * object that already has them. */
DC_ENTRY(C04590, RTS)
{
    DC_ASSUME_D(C04590, 0x0100);
    npc_begin(4);
    u16 obj = ram16(EV_CUR_OBJ);
    u8 id = ram8(OBJ_CHAR(obj));
    io8_set(0x4203, 5);
    ram16_set(EV_TMP_C7, g_cpu.Y);
    ram8_set(OBJ_1B01(obj), 1);
    u16 row = (u16)(id * 5);
    ram16_set(EV_SPRITE_ROW, row);
    u32 r = SPRITE_TABLE + 0x23 + row;
    sprite_fields(r, obj);
    ram8_set(EV_GFX_ID, rom8(r));
    if (gfx_cache_find()) {
        u8 owner = (u8)g_cpu.A;
        ram16_set(OBJ_GFX_PTR(obj), ram16(OBJ_GFX_PTR(owner)));
        gfx_note_start();
    } else {
        gfx_decompress(true);
    }
    ram8_set(GFX_LOADED, (u8)(ram8(GFX_LOADED) + 1));
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 1;
}

/* 83 n s  The current object becomes enemy-type sprite n (10-byte rows at
 * E4:F600). s & 7F: on-screen sprite slot (0B88 + s+1 = object; sets the
 * object's VRAM slot fields); s bit 7: object kind 6 instead of 5. Rows with
 * id F8-FF use their graphics in place, without decompressing. */
#define SPRITE_SLOT_OBJ(k)  (0x0B88 + (k))
#define EV_SPRITE_ARG       0x01E1
#define EV_OBJ_AE           0x01AE
DC_ENTRY(C046DF, RTS)
{
    DC_ASSUME_D(C046DF, 0x0100);
    npc_begin(5);
    u16 obj = ram16(EV_CUR_OBJ);
    u8 id = ram8(OBJ_CHAR(obj));
    io8_set(0x4203, 0x0A);
    ram16_set(EV_TMP_C7, g_cpu.Y);
    ram8_set(OBJ_1B01(obj), 1);
    u16 row = (u16)(id * 10);
    ram16_set(EV_SPRITE_ROW, row);
    u32 r = 0xE4F600 + row;
    sprite_fields(r, obj);
    ram8_set(EV_GFX_ID, rom8(r));
    if (id >= 0xF8) {
        u8 g = ram8(EV_GFX_ID);
        ram16_set(EV_TMP_D9, g);
        u16 y = (u16)(g * 3);
        ram16_set(OBJ_GFX_PTR(obj), far16(add24(ram24(0x01AF), y)));
        ram8_set(OBJ_GFX(obj), bus8(add24(ram24(0x01AF), (u16)(y + 2))));
        g_cpu.Y = (u16)(y + 2);
        g_cpu._flag_V = 0;
    } else if (gfx_cache_find()) {
        ram16_set(OBJ_GFX_PTR(obj), ram16(OBJ_GFX_PTR((u8)g_cpu.A)));
    } else {
        gfx_decompress(false);
    }

    u8 s = ev_byte(ram16(EV_TMP_C7));
    ram8_set(EV_SPRITE_ARG, s);
    u8 k = (u8)((s & 0x7F) + 1);
    dc_set_mx(1, 1);
    ram8_set(SPRITE_SLOT_OBJ(k), ram8(EV_CUR_OBJ));
    ram16_set(EV_TMP_D9, (u16)((k & 0xFE) << 4));
    ram8_set(EV_TMP_D9, k);
    u8 o = ram8(EV_CUR_OBJ);
    ram16_set(OBJ_0D00(o), (u16)(((k & 0xFE) << 4) + ((k & 1) << 3)));
    ram16_set(OBJ_1700(o), (u16)(k << 5));
    g_cpu._flag_V = 0;
    if ((ram8(OBJ_1201(o)) & 3) >= 3) {
        g_cpu.X = o;
        ram8_set(EV_OBJ_AE, o);
        dc_jsr(0xC0E12A, 0xC047E9);
    }
    if (s & 0x80) ram8_set(OBJ_REMOVED(ram8(EV_CUR_OBJ)), 6);
    dc_set_mx(1, 0);
    g_cpu.X = (u16)(ram16(EV_TMP_C7) + 1);
    g_cpu._flag_C = 1;
}
