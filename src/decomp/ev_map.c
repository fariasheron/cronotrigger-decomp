/* Field event script: map changes (commands DC DD DE E0 E2 E3). A map
 * destination is 5 bytes: 16-bit map number (low 9 bits) whose high byte
 * also carries the arrival flags ((hi >> 1) & 0F: facing/half-tile,
 * bit 7), then tile x and y.
 *   $0100-$0104: destination used when the field restarts (C0:0000)
 *   $010A-$010E: second destination (DC)
 *   $0112-$0116: destination of a pending change ($0117 bit 7, done by the
 *                field loop; $0119 = 0F)
 * E1 and DF (change now: they reset the stack and jump to C0:0000) stay as
 * generated code until the field loop itself is rewritten. */
#include "ram.h"

#define MAP_DEST        0x0100
#define MAP_DEST2       0x010A
#define MAP_PENDING     0x0112
#define FIELD_REQUESTS  0x0117   /* bit 7 map change, bit 6 menu (C8), bit 0 battle (D8) */
#define FIELD_0119      0x0119
#define FIELD_011E      0x011E
#define FIELD_011F      0x011F

/* Map number word w and tile word xy into dest. */
static void set_dest(u32 dest, u16 w, u16 xy)
{
    ram16_set(EV_TMP_D9, w);
    ram16_set(dest, w & 0x01FF);
    ram16_set(dest + 2, xy);
    u8 hi = (u8)(w >> 8);
    ram8_set(dest + 4, (u8)(((hi >> 1) & 0x0F) | (hi & 0x80)));
    dc_set_a8(hi & 0x80);
}
static void set_dest_from_script(u32 dest)
{
    u16 pos = g_cpu.Y;
    set_dest(dest, ev_word((u16)(pos + 1)), ev_word((u16)(pos + 3)));
    g_cpu.X = (u16)(pos + 5);
    g_cpu._flag_C = 1;
}
/* DD m x y  Set the restart destination;  DE: the same and $011E = 1;
 * DC m x y: set the second destination. */
DC_ENTRY(C03B1B, RTS) { DC_ASSUME_D(C03B1B, 0x0100); set_dest_from_script(MAP_DEST); }
DC_ENTRY(C03B45, RTS) { DC_ASSUME_D(C03B45, 0x0100); ram8_set(FIELD_011E, 1); set_dest_from_script(MAP_DEST); }
DC_ENTRY(C03AF1, RTS) { DC_ASSUME_D(C03AF1, 0x0100); set_dest_from_script(MAP_DEST2); }

/* E0 m x y  Request a map change; the command then waits (the change
 * replaces this script). */
DC_ENTRY(C03AB7, RTS)
{
    DC_ASSUME_D(C03AB7, 0x0100);
    u16 pos = g_cpu.Y;
    g_cpu._flag_C = 0;
    if (ram8(FIELD_REQUESTS) & 0x80) { g_cpu.X = pos; return; }
    set_dest(MAP_PENDING, ev_word((u16)(pos + 1)), ev_word((u16)(pos + 3)));
    ram8_set(FIELD_REQUESTS, ram8(FIELD_REQUESTS) | 0x80);
    ram8_set(FIELD_0119, 0x0F);
    dc_set_a8(0x0F);
    g_cpu.X = pos;
}
/* E2 a b c d  The same with the map number from var[a] (16-bit, unmasked),
 * x = var[b], y = var[c], flags = var[d]. */
DC_ENTRY(C03B9E, RTS)
{
    DC_ASSUME_D(C03B9E, 0x0100);
    u16 pos = g_cpu.Y;
    g_cpu._flag_C = 0;
    if (ram8(FIELD_REQUESTS) & 0x80) { g_cpu.X = pos; return; }
    ram16_set(EV_TMP_C7, pos);
    ram16_set(MAP_PENDING, ram16(EV_LOCAL + ev_byte((u16)(pos + 1)) * 2));
    ram8_set(MAP_PENDING + 2, ram8(EV_LOCAL + ev_byte((u16)(pos + 2)) * 2));
    ram8_set(MAP_PENDING + 3, ram8(EV_LOCAL + ev_byte((u16)(pos + 3)) * 2));
    ram8_set(MAP_PENDING + 4, ram8(EV_LOCAL + ev_byte((u16)(pos + 4)) * 2));
    ram8_set(FIELD_REQUESTS, ram8(FIELD_REQUESTS) | 0x80);
    ram8_set(FIELD_0119, 0x0F);
    dc_set_a8(0x0F);
    g_cpu.Y = (u16)(pos + 4);
    g_cpu.X = pos;
}
/* E3 b  $011F = b. */
DC_ENTRY(C03C05, RTS)
{
    DC_ASSUME_D(C03C05, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    dc_set_a8(b);
    ram8_set(FIELD_011F, b);
    g_cpu.Y = g_cpu.X = (u16)(p + 1);
    g_cpu._flag_C = 1;
}
