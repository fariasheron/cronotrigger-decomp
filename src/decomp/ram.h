/* Chrono Trigger WRAM map, as far as the decomp has identified it.
 * Offsets are 17-bit WRAM offsets (7E:xxxx = 0x0xxxx, 7F:xxxx = 0x1xxxx).
 * Arrays indexed by "obj" use the game's own index, object number * 2. */
#ifndef CT_DECOMP_RAM_H
#define CT_DECOMP_RAM_H
#include "dc.h"

/* ---- field objects (64 slots, index = slot * 2) ---------------------- */
/* Position: high byte = tile, low byte = sub-tile fraction. */
#define OBJ_POS_X(obj)          (0x1800 + (obj))
#define OBJ_POS_Y(obj)          (0x1880 + (obj))
#define OBJ_TILE_X(obj)         (0x1801 + (obj))
#define OBJ_TILE_Y(obj)         (0x1881 + (obj))

#define OBJ_0F00(obj)           (0x0F00 + (obj))   /* nonzero: object is active (drawn) */
/* Per-object state used by the event script engine. */
#define OBJ_EV_DISABLED(obj)    (0x1000 + (obj))   /* bit 7: object's script is not run */
#define OBJ_REMOVED(obj)        (0x1100 + (obj))   /* bit 7: object removed from the scene */
#define OBJ_SCRIPT_POS(obj)     (0x1180 + (obj))   /* 16-bit position in EV_SCRIPT */
#define OBJ_1A01(obj)           (0x1A01 + (obj))
#define OBJ_1A80(obj)           (0x1A80 + (obj))
#define OBJ_1A81(obj)           (0x1A81 + (obj))
#define OBJ_FACING(obj)         (0x1600 + (obj))   /* 0 up, 1 down, 2 left, 3 right */
#define OBJ_1601(obj)           (0x1601 + (obj))
#define OBJ_PRIORITY(obj)       (0x1C00 + (obj))   /* running function's priority 0 (highest) - 7 */
#define OBJ_CALLS_BLOCKED(obj)  (0x1C01 + (obj))   /* nonzero: calls from other objects are refused */
#define OBJ_MOVE_PROPS(obj)     (0x1C80 + (obj))
#define OBJ_POS_PROPS(obj)      (0x1C81 + (obj))

/* ---- event scripts --------------------------------------------------- */
/* The location's event, decompressed at 7F:2000: a count byte, then for each
 * object 16 function pointers (2 bytes each, relative to 7F:2001), then the
 * code. Script positions are offsets from 7F:2001. */
#define EV_SCRIPT               0x12001
static inline u8  ev_byte(u16 pos) { return ram8(EV_SCRIPT + pos); }
static inline u16 ev_word(u16 pos) { return ram16(EV_SCRIPT + pos); }
/* Entry point of function fn (0-15) of object obj. */
static inline u16 ev_function_entry(u16 obj, u8 fn) { return ev_word((u16)(((obj & 0xFF) << 4) + (fn & 0x0F) * 2)); }
/* Script position saved when a function of priority prio was interrupted
 * (0 = none), one 0x80 table per priority level: 7F:0580 + prio * 0x80 + obj. */
#define EV_SAVED_POS(prio, obj) (0x10580 + (prio) * 0x80 + (obj))
#define EV_WAITING_CALL(obj)    (0x10980 + (obj))  /* 7F:0980: waiting for a called function to finish */
#define EV_RESULT(obj)          (0x10A80 + (obj))  /* 7F:0A80: per-object result register (16-bit) */
#define STORY_MEM               0x10000            /* 7F:0000: story progress and flags; 7F:0000 = storyline */
#define EV_LOCAL                0x10200            /* 7F:0200: event variables, addressed by index * 2 */
#define EV_CUR_OBJ              0x016D             /* object whose script is running (index) */
#define PARTY_OBJ(slot)         (0x0197 + (slot))  /* party member -> object index, bit 7 = absent */
#define PARTY_MEMBER(n)         (0x2980 + (n))     /* 7E:2980: characters in the party (0 Crono ... 6 Magus) */
/* Scratch used by the event commands (direct page $0100). */
#define EV_TMP_D9               0x01D9
#define EV_TMP_OBJ              0x01DB  /* 16-bit */
#define EV_TMP_ARG              0x01DD
#define EV_TMP_PRIO             0x01DF
#define EV_TMP_SLOT             0x01E1  /* 16-bit */
#define EV_TMP_FNTAB            0x01E3  /* 16-bit */
#define EV_TMP_NEXT             0x01C3  /* 16-bit: position after a conditional's operands */
#define EV_TMP_C7               0x01C7  /* 16-bit: saved script position */

/* ---- field scroll/screen bounds (tiles) ------------------------------ */
#define FIELD_SCROLL_X_HALF     0x1D0A             /* camera X (tiles * 2) */
#define FIELD_SCROLL_Y_HALF     0x1D0E             /* camera Y (tiles * 2) */
#define FIELD_BOUND_LEFT        0x1D1A
#define FIELD_BOUND_RIGHT       0x1D1B
#define FIELD_BOUND_TOP         0x1D1C
#define FIELD_BOUND_BOTTOM      0x1D1D
#define SCRATCH_01D9            0x01D9

/* C0:2E1E: blank the screen to colour `color` and hang (ev_party.c). */
void fatal_error(u16 color, u32 site);

#endif
