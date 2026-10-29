/* Bank C2: the menu system (menu script interpreter, text drawing, windows). */
#include "ram.h"
#include <stdio.h>
#include <stdlib.h>

/* [dp] long pointer. */
static u32 lp(u8 o) { return dp16(o) | (u32)dp8((u8)(o + 2)) << 16; }
/* dp,X with a 16-bit index (bank 0). */
static u16 dpx(u8 o) { return (u16)(g_cpu.D + o + g_cpu.X); }
static void php(void) { dc_sync_p(); dc_push8(g_cpu.P); }
static void plp(void)
{
    g_cpu.P = dc_pull8();
    cpu_p_to_mirrors(&g_cpu);
    if (g_cpu.x_flag) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
}
static void lda8(u8 v)   { dc_set_a8(v); dc_set_flags_nz8(v); }
static void lda16(u16 v) { g_cpu.A = v; dc_set_flags_nz16(v); }
static void ldx16(u16 v) { g_cpu.X = v; dc_set_flags_nz16(v); }
static void ldy16(u16 v) { g_cpu.Y = v; dc_set_flags_nz16(v); }
static u8 rol8(u8 v) { u8 c = g_cpu._flag_C; g_cpu._flag_C = v >> 7; v = (u8)(v << 1 | c); dc_set_flags_nz8(v); return v; }
static void tsb_dp(u8 o, u8 v) { u8 m = dp8(o); g_cpu._flag_Z = (m & v) == 0; dp8_set(o, m | v); }
static void trb_dp(u8 o, u8 v) { u8 m = dp8(o); g_cpu._flag_Z = (m & v) == 0; dp8_set(o, m & ~v); }

/* ---- bare returns (ends of routines other code jumps into) ------------- */
DC_ENTRY(C2338E, RTS) {}
DC_ENTRY(C25B14, RTS) {}
DC_ENTRY(C28C5A, RTS) {}
DC_ENTRY(C2A517, RTS) {}
DC_ENTRY(C2A696, RTS) {}
DC_ENTRY(C2EFD3, RTS) {}
DC_ENTRY(C2F6D1, RTS) {}
DC_ENTRY(C25893, RTS) { g_cpu._flag_C = 1; }   /* "yes" */
DC_ENTRY(C25895, RTS) { g_cpu._flag_C = 0; }   /* "no" */

/* ---- tiny helpers -------------------------------------------------------- */
DC_ENTRY(C24202, RTS) { lda16(dp16(0x0E)); }
DC_ENTRY(C2F003, RTS) { ldx16(g_cpu.A); }
DC_ENTRY(C2F6CE, RTS) { lda16(0); }
DC_ENTRY(C2F6D2, RTS) { g_cpu._flag_C = g_cpu.A >> 15; lda16((u16)(g_cpu.A << 1)); }
DC_ENTRY(C2F6D4, RTS)                            /* A = A*2 + $14 */
{
    g_cpu._flag_C = g_cpu.A >> 15;
    g_cpu.A = dc_adc16((u16)(g_cpu.A << 1), dp16(0x14));
}
DC_ENTRY(C20F35, RTS) { lda16(g_cpu.D); g_cpu._flag_C = 1; }
DC_ENTRY(C21C81, RTS) { lda16(g_cpu.D); g_cpu._flag_C = 1; }
DC_ENTRY(C23485, RTS) { dp8_set(0x02, (u8)g_cpu.A); g_cpu._flag_C = 0; }
DC_ENTRY(C2594F, RTS) { lda8((u8)g_cpu.Y); dp8_set(0x15, (u8)g_cpu.Y); }
DC_ENTRY(C25C32, RTS) { lda8(0x10); dp8_set(0x15, 0x10); }
DC_ENTRY(C25C30, RTS) { dp8_set(0x30, 0); lda8(0x10); dp8_set(0x15, 0x10); }
DC_ENTRY(C283FC, RTS)
{
    dc_cmp8((u8)g_cpu.A, abs8(0x0FA9 + g_cpu.X));
    u8 m = abs8(0x0D10);
    g_cpu._flag_Z = (m & (u8)g_cpu.A) == 0;
    abs8_set(0x0D10, m | (u8)g_cpu.A);
}
DC_ENTRY(C28D63, RTS) { lda8(0xFF); bus8_set(0x307FE2, 0xFF); }   /* SRAM marker */
DC_ENTRY(C2F332, RTS) { ldx16(dp16(0x5B)); dp16_set(0x61, g_cpu.X); }
DC_ENTRY(C2232D, RTS)
{
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0x1B30, 0);
    abs8_set(0x1B31, 0);
}
DC_ENTRY(C26263, RTS) { ldx16(abs16(0x1B32)); lda8(bus8(0x7E7E02 + g_cpu.X)); abs8_set(0x1B58, (u8)g_cpu.A); }
DC_ENTRY(C26286, RTS) { ldx16(abs16(0x1B36)); lda8(bus8(0x7E8183 + g_cpu.X)); abs8_set(0x1B47, (u8)g_cpu.A); }
DC_ENTRY(C204F0, RTS)
{
    dc_set_mx(1, 0);
    lda8(dp8(0x01));
    abs8_set(0x0009 + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x000A + g_cpu.X, 0);
}
/* Byte 1 of the menu script at [$58] replaces the pointer; A = 1. */
DC_ENTRY(C2136A, RTS)
{
    g_cpu.Y = 1;
    dp16_set(0x58, bus16(lp(0x58) + 1));
    lda16(1);
}
static void set_29(u8 v)
{
    ldx16(dp16(0x4E));
    lda8(v);
    abs8_set(0x0029 + g_cpu.X, v);
    dc_set_mx(0, 0);
}
DC_ENTRY(C2404C, RTS) { set_29(0x22); }
DC_ENTRY(C24056, RTS) { set_29(0x12); }
static void next_byte(u8 o)
{
    dc_set_mx(1, 0);
    lda8(abs8(g_cpu.Y));
    dp8_set(o, (u8)g_cpu.A);
    ldy16((u16)(g_cpu.Y + 1));
}
DC_ENTRY(C2F068, RTS) { next_byte(0x7E); }
DC_ENTRY(C2F0B8, RTS) { next_byte(0x88); }
DC_ENTRY(C211A5, RTS)
{
    g_cpu.Y = 4;
    u16 v = bus16(lp(0x58) + 4);
    bus16_set(lp(0x10), v);
    lda16(5);
}
DC_ENTRY(C2185F, RTS)
{
    ldx16(dp16(0x4E));
    dp16_set(0x58, (u16)(abs16(0x0005 + g_cpu.X) - 1));
    lda16(1);
}
/* Next byte of the C0:FE00 random table. */
DC_ENTRY(C22336, RTS)
{
    dc_set_mx(1, 1);
    g_cpu.X = abs8(0x1B30);
    dc_set_a8(bus8(0xC0FE00 + g_cpu.X));
    u8 n = (u8)(abs8(0x1B30) + 1);
    abs8_set(0x1B30, n);
    dc_set_flags_nz8(n);
    dc_set_mx(1, 0);
}
DC_ENTRY(C2D0DB, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    dc_set_a8(0x19);
    abs8_set(0x0D13, 0x19);
    plp();
}
DC_ENTRY(C2F01E, RTS) { ldy16((u16)(g_cpu.Y + 4)); }
DC_ENTRY(C26453, RTS)                            /* top nibble of (10),Y into $00 */
{
    u8 a = bus8(dba(dp16(0x10)) + g_cpu.Y);
    for (int k = 0; k < 4; k++) a = rol8(a);
    dc_set_a8(a);
    dp8_set(0x00, rol8(dp8(0x00)));
}
DC_ENTRY(C28249, RTS)
{
    php();
    dc_set_mx(0, g_cpu.x_flag);
    u16 a = dpx(0xB2);
    b0_8_set(a, (u8)g_cpu.A); b0_8_set((u16)(a + 1), (u8)(g_cpu.A >> 8));
    lda16(0x0101);
    a = dpx(0xB0);
    b0_8_set(a, 0x01); b0_8_set((u16)(a + 1), 0x01);
    plp();
}
DC_ENTRY(C283A2, RTS)
{
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    ldy16(dc_adc16(abs16(0x0D11) & 0xFF, 0x0C00));
    lda16(7);
}
static void flag_byte(u16 at)
{
    u8 a = abs8(at + g_cpu.Y);
    lda8(a);
    dp8_set(0x54, a);
    if (a & 0x80) {
        lda8(0x80);
        tsb_dp(0x5A, 0x80);
        trb_dp(0x54, 0x80);
    }
}
DC_ENTRY(C2EA36, RTS) { flag_byte(0x9382); }
DC_ENTRY(C2EA3B, RTS) { flag_byte(0x9383); }
DC_ENTRY(C2EA40, RTS) { flag_byte(0x9384); }
DC_ENTRY(C2EA45, RTS) { flag_byte(0x9385); }
/* Byte 1 of the script at [$52], sign-extended. */
DC_ENTRY(C20E91, RTS)
{
    g_cpu.Y = 1;
    u16 a = bus16(lp(0x52) + 1) & 0xFF;
    g_cpu._flag_N = 0;
    g_cpu._flag_Z = 1;
    if (a & 0x80) { a |= 0xFF00; dc_set_flags_nz16(a); }
    g_cpu.A = a;
    g_cpu._flag_C = 0;
}
/* Script ops: clear / increment / decrement the byte the script names. */
static void script_byte(int op)
{
    g_cpu.Y = 1;
    ldx16(bus16(lp(0x58) + 1));
    dc_set_mx(1, 0);
    u8 v = op == 0 ? 0 : (u8)(abs8(g_cpu.X) + op);
    abs8_set(g_cpu.X, v);
    if (op) dc_set_flags_nz8(v);
    dc_set_mx(0, 0);
    lda16(3);
}
DC_ENTRY(C21285, RTS) { script_byte(0); }
DC_ENTRY(C21296, RTS) { script_byte(1); }
DC_ENTRY(C212A7, RTS) { script_byte(-1); }
DC_ENTRY(C25361, RTS)
{
    ldx16(dp16(0x4E));
    abs16_set(0x002A + g_cpu.X, abs16(0x029A));
    lda16(abs16(0x029C));
    abs16_set(0x002C + g_cpu.X, g_cpu.A);
    dc_set_mx(1, 0);
}
DC_ENTRY(C2545C, RTS)
{
    dp8_set(0x09, 0);
    ldy16(dp16(0x4E));
    abs8_set(0x0027 + g_cpu.Y, abs8(0x0028 + g_cpu.Y));
    ldx16(dp16(0x08));
    lda8(bus8(0xC0F700 + g_cpu.X));
    abs8_set(0x0028 + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C25945, RTS)
{
    dp8_set(0x16, bus8(lp(0x31)));
    dp16_set(0x31, (u16)(dp16(0x31) + 1));
    lda8((u8)g_cpu.Y);
    dp8_set(0x15, (u8)g_cpu.Y);
}
/* Store A (low, then high) at [$73] + the two offsets the table gives for $6E. */
static void put_pair(u16 lo_tab, u16 hi_tab)
{
    dc_set_mx(1, 0);
    g_cpu.X = dp16(0x6E);
    g_cpu.Y = abs16(lo_tab + g_cpu.X);
    bus8_set(lp(0x73) + g_cpu.Y, (u8)g_cpu.A);
    g_cpu.Y = abs16(hi_tab + g_cpu.X);
    g_cpu.A = (u16)(g_cpu.A << 8 | g_cpu.A >> 8);
    dc_set_flags_nz8((u8)g_cpu.A);
    bus8_set(lp(0x73) + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C25EBF, RTS) { put_pair(0x5FE8, 0x5FE6); }
DC_ENTRY(C25F90, RTS) { put_pair(0x6068, 0x6066); }
DC_ENTRY(C2F005, RTS)
{
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(abs16(g_cpu.Y), dp16(0x61));
    dp16_set(0x65, g_cpu.A);
    g_cpu.X = g_cpu.A;
    ldy16((u16)(g_cpu.Y + 2));
}

/* ---- batch 2 ------------------------------------------------------------- */
static void plb(void) { g_cpu.DB = dc_pull8(); dc_set_flags_nz8(g_cpu.DB); }

/* Script ops on the menu object ($4E): byte 1 of the script names a field. */
static void obj_field_zero(u8 script)
{
    dc_set_mx(1, 0);
    g_cpu.Y = 1;
    dc_set_a8(bus8(lp(script) + 1));
    g_cpu.Y = g_cpu.A;                       /* TAY with 16-bit index: B comes along */
    g_cpu.A = g_cpu.D;
    bus8_set(dba(dp16(0x4E)) + g_cpu.Y, (u8)g_cpu.D);
    dc_set_mx(0, 0);
    lda16(2);
}
DC_ENTRY(C20E5A, RTS) { obj_field_zero(0x52); }
DC_ENTRY(C2120C, RTS) { obj_field_zero(0x58); }
static void obj_field_step(u8 script, int d)
{
    dc_set_mx(1, 0);
    g_cpu.Y = 1;
    dc_set_a8(bus8(lp(script) + 1));
    g_cpu.Y = g_cpu.A;
    u32 at = dba(dp16(0x4E)) + g_cpu.Y;
    u8 v = (u8)(bus8(at) + d);
    lda8(v);
    bus8_set(at, v);
    dc_set_mx(0, 0);
    lda16(2);
}
DC_ENTRY(C20E6B, RTS) { obj_field_step(0x52, 1); }
DC_ENTRY(C20E7E, RTS) { obj_field_step(0x52, -1); }
DC_ENTRY(C2121D, RTS) { obj_field_step(0x58, 1); }
DC_ENTRY(C21230, RTS) { obj_field_step(0x58, -1); }
DC_ENTRY(C21243, RTS)                            /* field (byte 1) = byte 2 */
{
    dc_set_mx(1, 0);
    dc_set_a8(bus8(lp(0x58) + 1));
    g_cpu.X = g_cpu.A;
    g_cpu.Y = 2;
    dc_set_a8(bus8(lp(0x58) + 2));
    ldy16(g_cpu.X);
    bus8_set(dba(dp16(0x4E)) + g_cpu.Y, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    lda16(3);
}
/* Two script words into object fields; A = 5. */
static void obj_pair(u8 f1, u8 f2, int zero)
{
    ldx16(dp16(0x4E));
    abs16_set(f1 + g_cpu.X, bus16(lp(0x58) + 1));
    if (zero) abs16_set(f1 - 2 + g_cpu.X, 0);
    g_cpu.Y = 3;
    abs16_set(f2 + g_cpu.X, bus16(lp(0x58) + 3));
    if (zero) abs16_set(f2 - 2 + g_cpu.X, 0);
    lda16(5);
}
DC_ENTRY(C215E3, RTS) { obj_pair(0x1A, 0x1C, 0); }
DC_ENTRY(C215F9, RTS) { obj_pair(0x1E, 0x20, 0); }
DC_ENTRY(C215C3, RTS) { obj_pair(0x14, 0x18, 1); }
/* Signed sine of A (10 bits, 0x400 = full turn) from the C0:F900 table. */
static void sine(void)
{
    ldx16(g_cpu.A & 0x03FF);
    u16 a = bus16(0xC0F900 + g_cpu.X) & 0xFF;
    dc_cmp16(g_cpu.X, 0x0200);
    if (g_cpu._flag_C) { a = (u16)(~a + 1); dc_set_flags_nz16(a); }
    g_cpu.A = a;
}
DC_ENTRY(C2225E, RTL) { sine(); }
DC_ENTRY(C2225A, RTL) { g_cpu._flag_C = 0; g_cpu.A = dc_adc16(g_cpu.A, 0x0100); sine(); }   /* cosine */
DC_ENTRY(C226F0, RTS)                            /* save 00:1BA7.. (16 bytes) to 7F:01F0 */
{
    dc_set_mx(0, 0);
    dc_push8(g_cpu.DB);
    g_cpu.X = 0x1BA7; g_cpu.Y = 0x01F0; g_cpu.A = 0x000F;
    dc_mvn(0x7F, 0x00);
    plb();
    dc_set_mx(1, 0);
}
DC_ENTRY(C226D6, RTS)                            /* and back; 1BA6 = 7F:0000 */
{
    dc_set_mx(0, 0);
    dc_push8(g_cpu.DB);
    g_cpu.X = 0x01F0; g_cpu.Y = 0x1BA7; g_cpu.A = 0x000F;
    dc_mvn(0x00, 0x7F);
    plb();
    dc_set_mx(1, 0);
    lda8(bus8(0x7F0000));
    abs8_set(0x1BA6, (u8)g_cpu.A);
}
DC_ENTRY(C23FA7, RTS)
{
    g_cpu._flag_C = 1;
    abs16_set(0x002E + g_cpu.X, dc_sbc16(abs16(0x002A + g_cpu.X), abs16(0x0014 + g_cpu.X)));
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc16(abs16(0x002C + g_cpu.X), abs16(0x0018 + g_cpu.X));
    abs16_set(0x0030 + g_cpu.X, g_cpu.A);
}
/* Length of the string at [$37] (stops at `end`, at most `max`). */
static void str_len(u8 end, u16 max)
{
    g_cpu.Y = 0;
    for (;;) {
        u8 c = bus8(lp(0x37) + g_cpu.Y);
        dc_cmp8(c, end);
        dc_set_a8(c);
        if (c == end) break;
        g_cpu.Y++;
        dc_cmp16(g_cpu.Y, max);
        if (g_cpu._flag_C) break;
    }
    lda8((u8)g_cpu.Y);
}
DC_ENTRY(C25D23, RTS) { str_len(0xEF, 10); }
DC_ENTRY(C25D34, RTS) { str_len(0xEF, 11); }
DC_ENTRY(C25D45, RTS) { str_len(0x00, 5); }
DC_ENTRY(C2626E, RTS)
{
    ldx16(abs16(0x1B34));
    u8 a = bus8(0x7E8302 + g_cpu.X);
    g_cpu._flag_C = a >> 7;
    dc_set_a8((u8)(a << 1));
    g_cpu.X = g_cpu.A;
    lda8(bus8(0x7E8480 + g_cpu.X)); abs8_set(0x1B45, (u8)g_cpu.A);
    lda8(bus8(0x7E8481 + g_cpu.X)); abs8_set(0x1B46, (u8)g_cpu.A);
}
DC_ENTRY(C2EFF4, RTS)
{
    u16 m = dp16(0x87), a = 0x0080;
    g_cpu._flag_N = m >> 15; g_cpu._flag_V = m >> 14 & 1; g_cpu._flag_Z = (a & m) == 0;
    if (m & 0x8000) a = 0x0040;
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(a, dp16(0x65));
    dp16_set(0x65, g_cpu.A);
    g_cpu.X = g_cpu.A;
}
static void obj_flags(u8 keep)
{
    dc_set_mx(1, 0);
    ldx16(dp16(0x4E));
    g_cpu.Y = 1;
    lda8((u8)((abs8(0x000F + g_cpu.X) & keep) | bus8(lp(0x58) + 1)));
    abs8_set(0x000F + g_cpu.X, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    lda16(2);
}
DC_ENTRY(C21091, RTS) { obj_flags(0xF1); }
DC_ENTRY(C210A8, RTS) { obj_flags(0x4F); }
/* Script ops on any byte: [addr] = v, |= v, &= ~v. */
static void mem_op(int op)
{
    g_cpu.Y = 1;
    ldx16(bus16(lp(0x58) + 1));
    dc_set_mx(1, 0);
    g_cpu.Y = 3;
    u8 v = bus8(lp(0x58) + 3);
    if (op == 1) v |= abs8(g_cpu.X);
    if (op == 2) v = (u8)~v & abs8(g_cpu.X);
    lda8(v);
    abs8_set(g_cpu.X, v);
    dc_set_mx(0, 0);
    lda16(4);
}
DC_ENTRY(C212B8, RTS) { mem_op(0); }
DC_ENTRY(C212CE, RTS) { mem_op(1); }
DC_ENTRY(C212E7, RTS) { mem_op(2); }
/* Next script byte n: $10 = base + 3n. */
static void table3(u16 base)
{
    ldy16((u16)(g_cpu.Y + 1));
    dc_set_a8(bus8(lp(0x58) + g_cpu.Y));
    dc_set_mx(0, 0);
    dp16_set(0x00, g_cpu.A);
    g_cpu._flag_C = g_cpu.A >> 15;
    u16 a = dc_adc16((u16)(g_cpu.A << 1), dp16(0x00));
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(a, base);
    dp16_set(0x10, g_cpu.A);
}
DC_ENTRY(C21A8A, RTS) { table3(0x8300); }
DC_ENTRY(C21A9B, RTS) { table3(0x8180); }
static void neg32(u16 lo)
{
    g_cpu._flag_C = 0;
    abs16_set(lo + g_cpu.X, dc_adc16((u16)~abs16(lo + g_cpu.X), 1));
    g_cpu.A = dc_adc16((u16)~abs16(lo + 2 + g_cpu.X), 0);
    abs16_set(lo + 2 + g_cpu.X, g_cpu.A);
}
DC_ENTRY(C21C84, RTS) { neg32(0x1A); }
DC_ENTRY(C21C9E, RTS) { neg32(0x1E); }
/* Palette/graphics request through C3:0002 from a C6 table entry. */
static void c6_request(u32 tab, u32 site)
{
    g_cpu.X = g_cpu.A;
    dc_set_mx(0, 0);
    lda16(bus16(tab + g_cpu.X));
    abs16_set(0x0300, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(bus8(tab + 2 + g_cpu.X));
    abs8_set(0x0302, (u8)g_cpu.A);
    dc_jsl(0xC30002, site);
}
DC_ENTRY(C22868, RTS)
{
    dc_set_a8(dc_adc8((u8)g_cpu.A, bus8(lp(0xED) + g_cpu.Y)));
    c6_request(0xC6FF40, 0xC2287D);
}
DC_ENTRY(C22914, RTS)
{
    u8 a = (u8)(g_cpu.A + 1);
    g_cpu._flag_C = a >> 7;
    dc_set_a8(dc_adc8((u8)(a << 1), bus8(lp(0xED) + g_cpu.Y)));
    c6_request(0xC6FE20, 0xC2292B);
}
DC_ENTRY(C27B7F, RTS)
{
    abs16_set(0x0303, g_cpu.X);
    abs8_set(0x0305, (u8)g_cpu.A);
    g_cpu.X = 9;
    dc_set_mx(0, 0);
    lda16(bus16(0xC6FEA9));
    abs16_set(0x0300, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(bus8(0xC6FEAB));
    abs8_set(0x0302, (u8)g_cpu.A);
    dc_jsl(0xC30002, 0xC27B9A);
}
DC_ENTRY(C239C3, RTS)
{
    dc_set_mx(0, 0);
    ldx16(dp16(0x4E));
    lda16(abs16(0x0022 + g_cpu.X) & 0x0303);
    dc_set_mx(1, 0);
    g_cpu._flag_C = !g_cpu._flag_Z;
}
DC_ENTRY(C239AE, RTS)
{
    dc_set_mx(0, 0);
    ldx16(dp16(0x4E));
    g_cpu.A = abs16(0x0022 + g_cpu.X) & 0x0404;
    dc_cmp16(g_cpu.A, 0x0404);
    dc_set_mx(1, 0);
    g_cpu._flag_C = g_cpu._flag_Z;
}
static void unit_dir(u16 src, u16 dst, u16 z1, u16 z2, u16 z3)
{
    u16 a = (abs16(src + g_cpu.X) & 0x8000) ? 0xFFFF : 0x0001;
    lda16(a);
    abs16_set(dst + g_cpu.X, a);
    abs16_set(z1 + g_cpu.X, 0);
    abs16_set(z2 + g_cpu.X, 0);
    abs16_set(z3 + g_cpu.X, 0);
}
DC_ENTRY(C24138, RTS) { unit_dir(0x2E, 0x1C, 0x1A, 0x20, 0x1E); }
DC_ENTRY(C24152, RTS) { unit_dir(0x30, 0x20, 0x1E, 0x1C, 0x1A); }
/* A >> n, then the low byte to one offset and the high byte ORed into the other. */
static void put_pair_or(int n, u16 lo_tab, u16 hi_tab)
{
    for (int k = 0; k < n; k++) { g_cpu._flag_C = g_cpu.A & 1; g_cpu.A >>= 1; }
    dc_set_mx(1, 0);
    g_cpu.X = dp16(0x6E);
    g_cpu.Y = abs16(lo_tab + g_cpu.X);
    bus8_set(lp(0x73) + g_cpu.Y, (u8)g_cpu.A);
    g_cpu.Y = abs16(hi_tab + g_cpu.X);
    g_cpu.A = (u16)(g_cpu.A << 8 | g_cpu.A >> 8);
    lda8((u8)g_cpu.A | bus8(lp(0x73) + g_cpu.Y));
    bus8_set(lp(0x73) + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C25ED2, RTS) { put_pair_or(1, 0x5FE8, 0x5FE6); }
DC_ENTRY(C25ED1, RTS) { put_pair_or(2, 0x5FE8, 0x5FE6); }
DC_ENTRY(C25ED0, RTS) { put_pair_or(3, 0x5FE8, 0x5FE6); }
DC_ENTRY(C25FA3, RTS) { put_pair_or(1, 0x6068, 0x6066); }
DC_ENTRY(C25FA2, RTS) { put_pair_or(2, 0x6068, 0x6066); }
DC_ENTRY(C25FA1, RTS) { put_pair_or(3, 0x6068, 0x6066); }
/* Shift-subtract division: Y quotient bits of A / $6A into $65 (A or 8/16-bit). */
DC_ENTRY(C2614B, RTS)
{
    int w8 = g_cpu.m_flag;
    u16 mask = w8 ? 0xFF : 0xFFFF;
    u16 a = g_cpu.A & mask;
    if (w8) dp8_set(0x65, 0); else dp16_set(0x65, 0);
    do {
        u16 d = w8 ? dp8(0x6A) : dp16(0x6A);
        u16 q = w8 ? dp8(0x65) : dp16(0x65);
        if (a >= d) { g_cpu._flag_C = 1; a = w8 ? dc_sbc8((u8)a, (u8)d) : dc_sbc16(a, d); }
        else g_cpu._flag_C = 0;
        q = (u16)(q << 1 | g_cpu._flag_C);
        if (w8) dp8_set(0x65, (u8)q); else dp16_set(0x65, q);
        if (w8) dp8_set(0x6A, (u8)(d >> 1)); else dp16_set(0x6A, (u16)(d >> 1));
        g_cpu._flag_C = d & 1;
        g_cpu.Y = (u16)(g_cpu.Y - 1);
        if (g_cpu.x_flag) g_cpu.Y &= 0xFF;
    } while (g_cpu.Y);
    g_cpu.A = (u16)((g_cpu.A & ~mask) | a);
    ldy16(4);
}
DC_ENTRY(C29121, RTS)
{
    php();
    dc_set_mx(0, 0);
    lda16(abs16(0x0D7E));
    if (g_cpu.A) {
        g_cpu.X = 0x9B96; g_cpu.Y = 0x2400; g_cpu.A = 0x01FF;
        dc_mvn(0x7E, 0x7E);
    }
    plp();
}
DC_ENTRY(C29678, RTS)
{
    for (g_cpu.Y = 8; g_cpu.Y; g_cpu.Y--) {
        g_cpu.A = (abs16(g_cpu.X) | abs16(g_cpu.X + 1)) & 0xFF;
        abs16_set(0x0010 + g_cpu.X, g_cpu.A);
        g_cpu.X += 2;
    }
    dc_set_flags_nz16(0);
}
DC_ENTRY(C2F2CC, RTS)                            /* two digits at Y: 10*[Y+1] + [Y] */
{
    u8 a = abs8(0x0001 + g_cpu.Y);
    g_cpu._flag_C = a >> 7; a <<= 1;
    dp8_set(0x8A, a);
    g_cpu._flag_C = a >> 7; a <<= 1;
    g_cpu._flag_C = a >> 7; a <<= 1;
    a = dc_adc8(a, dp8(0x8A));
    a = dc_adc8(a, abs8(g_cpu.Y));
    dc_set_a8(a);
    ldy16((u16)(g_cpu.Y + 2));
}
DC_ENTRY(C203EF, RTS)
{
    dp8_set(0x5E, 0); dp8_set(0x5F, 0);
    dp16_set(0xE0, 0xF000);
    dc_set_a8(0x7E);
    dp8_set(0xE2, 0x7E);
    for (g_cpu.X = 0x7F; g_cpu.X != 0xFFFF; g_cpu.X--) b0_8_set(dpx(0x60), 0);
    dc_set_flags_nz16(0xFFFF);
}
DC_ENTRY(C21848, RTS)                            /* script call: save return, jump */
{
    ldx16(dp16(0x4E));
    g_cpu._flag_C = 0;
    abs16_set(0x0005 + g_cpu.X, dc_adc16(dp16(0x58), 3));
    g_cpu.Y = 1;
    dp16_set(0x58, (u16)(bus16(lp(0x58) + 1) - 1));
    lda16(1);
}
DC_ENTRY(C22BBC, RTS)                            /* clear 1 KB of 7F:9000 + 0x400*A */
{
    dc_push8(g_cpu.DB);
    u16 a = (u16)((g_cpu.A & 0xFF) << 8);
    g_cpu._flag_C = a >> 15; a <<= 1;
    g_cpu._flag_C = a >> 15; a <<= 1;
    g_cpu.X = dc_adc16(a, 0x9000);
    g_cpu.A = 0x03FF;
    dc_mvn(0x7F, 0x7F);
    plb();
}
DC_ENTRY(C2391C, RTS)
{
    g_cpu.X = 0;
    for (g_cpu.Y = 16; g_cpu.Y; g_cpu.Y--) {
        abs16_set(0x1B5B + g_cpu.X, abs16(0x0283));
        g_cpu.A = abs16(0x0285);
        abs16_set(0x1B7B + g_cpu.X, g_cpu.A);
        g_cpu.X += 2;
    }
    dc_set_flags_nz16(0);
}
DC_ENTRY(C25798, RTS)                            /* clear 7E:8621..8A20 */
{
    dc_set_mx(0, 0);
    dc_push8(g_cpu.DB);
    bus16_set(0x7E8621, g_cpu.D);
    g_cpu.X = 0x8621; g_cpu.Y = 0x8622; g_cpu.A = 0x03FE;
    dc_mvn(0x7E, 0x7E);
    plb();
    dc_set_mx(1, 0);
}
DC_ENTRY(C2EA27, RTS)                            /* X = 6 * (A & FF), A = 2 * (A & FF) */
{
    php();
    dc_set_mx(0, 0);
    u16 t = (u16)((g_cpu.A & 0xFF) << 1);
    dc_push16(t);
    g_cpu.X = (u16)(t * 3);
    g_cpu.A = dc_pull16();
    plp();
}
DC_ENTRY(C2F269, RTS)
{
    dc_push16(g_cpu.X);
    dc_set_a8((u8)g_cpu.A & 0x0F);
    g_cpu.X = g_cpu.A;
    abs8_set(g_cpu.Y, bus8(0xC2F27D + g_cpu.X));
    dc_set_a8(dp8(0x7E));
    abs8_set(0x0001 + g_cpu.Y, (u8)g_cpu.A);
    g_cpu.Y += 2;
    ldx16(dc_pull16());
}
DC_ENTRY(C20110, RTS)                            /* JML C2:015E / C2:0345 stubs at 0500/0504 */
{
    abs8_set(0x0500, 0x5C); abs8_set(0x0504, 0x5C);
    abs16_set(0x0501, 0x015E);
    abs16_set(0x0505, 0x0345);
    g_cpu.X = 0x0345;
    abs8_set(0x0503, 0xC2); abs8_set(0x0507, 0xC2);
    lda8(0xC2);
}
DC_ENTRY(C21CDA, RTS)                            /* object script pointer = C3:E000[A] */
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    g_cpu.X = (u16)((g_cpu.A & 0xFF) << 1);
    g_cpu.A = bus16(0xC3E000 + g_cpu.X);
    ldx16(dp16(0x4E));
    abs16_set(0x000B + g_cpu.X, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(0xC3);
    abs8_set(0x000D + g_cpu.X, 0xC3);
    abs8_set(0x000E + g_cpu.X, 0);
}
DC_ENTRY(C29223, RTS)
{
    dp8_set(0x01, 0);
    dp8_set(0x00, abs8(0x0DB5));
    g_cpu.A = (u16)((g_cpu.D & 0xFF00) | abs8(0x9B20));
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    u16 a = dc_adc16(g_cpu.A, dp16(0x00));
    g_cpu._flag_C = a >> 15;
    g_cpu.A = (u16)(a << 1);
    bus16_set(0x004204, g_cpu.A);
    dc_set_mx(1, 1);
    lda8(3);
}
DC_ENTRY(C2F0A2, RTS)
{
    dc_set_mx(1, 0);
    dc_push16(g_cpu.X);
    g_cpu.X = abs16(g_cpu.Y);
    g_cpu.Y += 2;
    dc_set_a8(0x1C);
    trb_dp(0x7E, 0x1C);
    u8 a = bus8(0x7E0000 + g_cpu.X) & 0x1C;
    dc_set_a8(a);
    tsb_dp(0x7E, a);
    ldx16(dc_pull16());
}

/* ---- batch 3 ------------------------------------------------------------- */
static u8 asl8(u8 v) { g_cpu._flag_C = v >> 7; v = (u8)(v << 1); dc_set_flags_nz8(v); return v; }
static u16 asl16(u16 v) { g_cpu._flag_C = v >> 15; v = (u16)(v << 1); dc_set_flags_nz16(v); return v; }
static u16 lsr16(u16 v) { g_cpu._flag_C = v & 1; v >>= 1; dc_set_flags_nz16(v); return v; }

DC_ENTRY(C20471, RTS)                            /* clear the first word of 64 menu objects */
{
    ldx16(0x0B30);
    dp16_set(0x4E, 0x0B30);
    dc_set_mx(0, 0);
    for (g_cpu.Y = 0x40; g_cpu.Y; g_cpu.Y--) {
        abs16_set(g_cpu.X, 0);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0040);
        g_cpu.X = g_cpu.A;
    }
    dc_set_flags_nz16(0);
    dc_set_mx(1, 0);
}
/* Script ops on an object field: &= ~v, += v, -= v (field = byte 1, v = byte 2). */
static void field_op(int op)
{
    dc_set_mx(1, 0);
    dc_set_a8(bus8(lp(0x58) + 1));
    g_cpu.X = g_cpu.A;
    u8 v = bus8(lp(0x58) + 2), a;
    ldy16(g_cpu.X);
    u32 at = dba(dp16(0x4E)) + g_cpu.Y;
    if (op == 0) { a = (u8)~v & bus8(at); dc_set_flags_nz8(a); }
    else if (op == 1) { g_cpu._flag_C = 0; a = dc_adc8(v, bus8(at)); }
    else { g_cpu._flag_C = 1; a = dc_sbc8(bus8(at), v); }
    dc_set_a8(a);
    bus8_set(at, a);
    dc_set_mx(0, 0);
    lda16(3);
}
DC_ENTRY(C2126D, RTS) { field_op(0); }
DC_ENTRY(C21ACA, RTS) { field_op(1); }
DC_ENTRY(C21AE1, RTS) { field_op(2); }
DC_ENTRY(C21A76, RTS)                            /* $10 = 7E00 + 7n */
{
    ldy16((u16)(g_cpu.Y + 1));
    dc_set_a8(bus8(lp(0x58) + g_cpu.Y));
    dc_set_mx(0, 0);
    dp16_set(0x00, g_cpu.A);
    u16 a = asl16(asl16(asl16(g_cpu.A)));
    g_cpu._flag_C = 1;
    a = dc_sbc16(a, dp16(0x00));
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(a, 0x7E00);
    dp16_set(0x10, g_cpu.A);
}
DC_ENTRY(C2274D, RTS)                            /* [$ED] = C6:FD00[(0100 & 1FF) - 1F0] */
{
    dc_set_mx(0, 0);
    g_cpu._flag_C = 1;
    u16 a = dc_sbc16(abs16(0x0100) & 0x01FF, 0x01F0);
    g_cpu.X = asl16(a);
    lda16(bus16(0xC6FD00 + g_cpu.X));
    dp16_set(0xED, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(0xC6);
    dp8_set(0xEF, 0xC6);
}
DC_ENTRY(C228ED, RTS)                            /* copy 16 bytes from 7F:8E00 + 32*A to Y */
{
    dc_push8(g_cpu.DB);
    u16 a = g_cpu.A & 0xFF;
    for (int k = 0; k < 5; k++) a = asl16(a);
    g_cpu.X = dc_adc16(a, 0x8E00);
    g_cpu.A = 0x000F;
    dc_mvn(0x7F, 0x7F);
    plb();
}
DC_ENTRY(C22D70, RTS)                            /* DMA channel 7: A bytes from 7F:X to VRAM Y */
{
    abs16_set(0x4372, g_cpu.X);
    abs16_set(0x2116, g_cpu.Y);
    abs16_set(0x4375, g_cpu.A);
    dc_set_mx(1, 0);
    abs8_set(0x2115, 0x80);
    ldx16(0x1801);
    abs16_set(0x4370, 0x1801);
    abs8_set(0x4374, 0x7F);
    lda8(0x80);
    abs8_set(0x420B, 0x80);
}
DC_ENTRY(C23ACB, RTS)                            /* pixel position (0283/0285) to tiles (0102/0103) */
{
    dc_set_mx(0, 0);
    abs16_set(0x0102, lsr16(lsr16(lsr16(abs16(0x0283)))));
    g_cpu.A = lsr16(lsr16(lsr16(abs16(0x0285))));
    dc_set_mx(1, 0);
    abs8_set(0x0103, (u8)g_cpu.A);
}
DC_ENTRY(C25605, RTS)
{
    lda16(abs16(0x0024 + g_cpu.X));
    u16 to = (g_cpu.A & 1) ? 0x02A6 : 0x02AA;
    abs16_set(to, abs16(0x0014 + g_cpu.X));
    lda16(abs16(0x0018 + g_cpu.X));
    abs16_set(to + 2, g_cpu.A);
}
DC_ENTRY(C25775, RTS)                            /* 7E:8600.. = C2:57C2.. (29 bytes) */
{
    dc_set_mx(0, 0);
    bus16_set(0x7E8600, bus16(0xC257C2));
    bus16_set(0x7E8602, bus16(0xC257C4));
    dc_push8(g_cpu.DB);
    g_cpu.X = 0x57C6; g_cpu.Y = 0x8604; g_cpu.A = 0x0018;
    dc_mvn(0x7E, 0xC2);
    plb();
    dc_set_mx(1, 0);
}
static void text_width(u8 w)
{
    dc_set_a8(w);
    dp8_set(0x34, w);
    dp8_set(0x17, 0);
    u8 a = dp8(0x14) & 0x0F;
    dc_set_a8(a);
    dc_cmp8(a, 2);
    if (a == 2) {
        g_cpu._flag_C = 1;
        a = dc_sbc8(dp8(0x34), 8);
        dc_set_a8(a);
        dp8_set(0x34, a);
    }
    g_cpu._flag_C = 0;
}
DC_ENTRY(C2589B, RTS) { text_width(0x14); }
DC_ENTRY(C25897, RTS) { text_width(0x08); }
DC_ENTRY(C25DAD, RTS)                            /* drop leading zero digits of 40-42; A = digits */
{
    g_cpu.Y = 3;
    if (dp8(0x40) == 0) {
        dp16_set(0x40, dp16(0x41));
        g_cpu.X = dp16(0x40);
        g_cpu.Y = 2;
        if (dp8(0x40) == 0) {
            dp8_set(0x40, dp8(0x41));
            g_cpu.Y = 1;
        }
    }
    lda8((u8)g_cpu.Y);
}
DC_ENTRY(C25ECF, RTS) { put_pair_or(4, 0x5FE8, 0x5FE6); }
DC_ENTRY(C25FA0, RTS) { put_pair_or(4, 0x6068, 0x6066); }
/* A << n (into $63), then three bytes out through the offset tables. */
static void put_three(int n, u16 tab)
{
    for (int k = 0; k < n; k++) {
        g_cpu.A = asl16(g_cpu.A);
        u16 m = dp16(0x63);
        u8 c = g_cpu._flag_C;
        g_cpu._flag_C = m >> 15;
        m = (u16)(m << 1 | c);
        dc_set_flags_nz16(m);
        dp16_set(0x63, m);
    }
    dc_set_mx(1, 0);
    g_cpu.X = dp16(0x6E);
    g_cpu.Y = abs16(tab + 4 + g_cpu.X);
    bus8_set(lp(0x73) + g_cpu.Y, (u8)g_cpu.A);
    g_cpu.Y = abs16(tab + 2 + g_cpu.X);
    g_cpu.A = (u16)(g_cpu.A << 8 | g_cpu.A >> 8);
    bus8_set(lp(0x73) + g_cpu.Y, (u8)g_cpu.A);
    g_cpu.Y = abs16(tab + g_cpu.X);
    lda8(dp8(0x63) | bus8(lp(0x73) + g_cpu.Y));
    bus8_set(lp(0x73) + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C25EEB, RTS) { put_three(1, 0x5FE6); }
DC_ENTRY(C25EE8, RTS) { put_three(2, 0x5FE6); }
DC_ENTRY(C25FBC, RTS) { put_three(1, 0x6066); }
DC_ENTRY(C25FB9, RTS) { put_three(2, 0x6066); }
DC_ENTRY(C27B5A, RTS)                            /* decompress C6:FDF0 entry 3 to 7F:9000 */
{
    abs16_set(0x0303, 0x9000);
    abs8_set(0x0305, 0x7F);
    g_cpu.X = 9;
    dc_set_mx(0, 0);
    lda16(bus16(0xC6FDF9));
    abs16_set(0x0300, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(bus8(0xC6FDFB));
    abs8_set(0x0302, (u8)g_cpu.A);
    dc_jsl(0xC30002, 0xC27B7A);
}
/* Slot (0..n-1) of character $C0 in the party list 2980, else FF. */
static void party_slot(u8 n)
{
    dp8_set(0xC2, n);
    g_cpu.Y = 0; g_cpu.X = 0;
    for (;;) {
        if (abs8(0x2980 + g_cpu.Y) == dp8(0xC0)) break;
        g_cpu.Y++;
        if (g_cpu.Y >= dp8(0xC2)) { g_cpu.X = 0xFF; break; }
    }
    g_cpu._flag_C = 1;
    lda8((u8)g_cpu.X);
}
DC_ENTRY(C28CCF, RTS) { party_slot(9); }
DC_ENTRY(C28C75, RTS) { party_slot(3); }
DC_ENTRY(C2BC59, RTS)                            /* halve A (rounding up) once or twice by 9ABA */
{
    php();
    dc_set_mx(1, 1);
    g_cpu.Y = abs8(0x9ABA);
    u8 a = (u8)g_cpu.A;
    if (g_cpu.Y == 0xA3) a = (u8)((a >> 1) + (a & 1));
    if (g_cpu.Y == 0xA2 || g_cpu.Y == 0xA3) a = (u8)((a >> 1) + (a & 1));
    dc_set_a8(a);
    plp();
}
DC_ENTRY(C2F364, RTS)                            /* copy the FF:[$5B] string (length byte first) to 7E:[$61] */
{
    php();
    dc_set_a8(0xFF);
    dc_push8(0xFF);
    g_cpu.DB = dc_pull8();
    g_cpu.A = g_cpu.D;
    g_cpu.X = dp16(0x5B);
    dc_set_a8((u8)(abs8(g_cpu.X) - 1));
    g_cpu.X++;
    g_cpu.Y = dp16(0x61);
    dc_mvn(0x7E, 0xFF);
    plp();
}
/* Wrap an object's x into 0..5FF and its y into 0..3FF. */
static void wrap_pos(u16 xf, u16 yf)
{
    ldx16(dp16(0x4E));
    u16 a = abs16(xf + g_cpu.X);
    if (a & 0x8000) { g_cpu._flag_C = 0; a = dc_adc16(a, 0x0600); }
    else { dc_cmp16(a, 0x0600); if (g_cpu._flag_C) a = dc_sbc16(a, 0x0600); }
    abs16_set(xf + g_cpu.X, a);
    lda16(abs16(yf + g_cpu.X) & 0x03FF);
    abs16_set(yf + g_cpu.X, g_cpu.A);
}
DC_ENTRY(C21CB8, RTS) { wrap_pos(0x14, 0x18); }
DC_ENTRY(C23A8F, RTS) { wrap_pos(0x2A, 0x2C); }
DC_ENTRY(C23934, RTS)                            /* push an object position on the 16-entry trail */
{
    g_cpu.Y = abs16(0x1B5A) & 0xFF;
    abs16_set(0x1B5B + g_cpu.Y, abs16(0x0014 + g_cpu.X));
    abs16_set(0x1B7B + g_cpu.Y, abs16(0x0018 + g_cpu.X));
    g_cpu.Y += 2;
    lda16(g_cpu.Y & 0x1F);
    dc_set_mx(1, 0);
    abs8_set(0x1B5A, (u8)g_cpu.A);
}
DC_ENTRY(C28255, RTS)                            /* clear $B0+8k for each set bit k of A */
{
    php();
    dc_set_mx(1, 1);
    dp8_set(0x00, (u8)g_cpu.A);
    u8 a = 0;
    do {
        u8 m = dp8(0x00);
        dp8_set(0x00, m >> 1);
        if (m & 1) { g_cpu.X = a; b0_8_set((u16)(g_cpu.D + 0xB0 + a), 0); }
        a += 8;
    } while (a < 0x40);
    dc_set_a8(a);
    plp();
}
DC_ENTRY(C287FA, RTS)                            /* find A in 7E:2400 with a nonzero 2500 entry */
{
    u8 a = (u8)g_cpu.A;
    g_cpu.X = 0;
    do {
        if (a == abs8(0x2400 + g_cpu.X)) {
            u8 v = abs8(0x2500 + g_cpu.X);
            if (v) {
                lda8(v);
                abs8_set(0x04C7, (u8)g_cpu.X);
                abs8_set(0x04C8, 0);
                g_cpu._flag_C = 0;
                return;
            }
            a = abs8(0x04C9);
        }
        g_cpu.X = (u8)(g_cpu.X + 1);
    } while (g_cpu.X);
    dc_set_a8(a);
    dc_set_flags_nz8(0);
    g_cpu._flag_C = 1;
}
DC_ENTRY(C28D45, RTS)
{
    dc_set_mx(0, 0);
    g_cpu.Y = 0x2600;
    do {
        abs8_set(0x0007 + g_cpu.Y, abs8(0x0009 + g_cpu.Y));
        g_cpu._flag_C = 0;
        g_cpu.Y = dc_adc16(g_cpu.Y, 0x0050);
        dc_cmp16(g_cpu.Y, 0x2830);
    } while (!g_cpu._flag_C);
    g_cpu.A = 0;
    dc_set_flags_nz16(0);
}
DC_ENTRY(C2923C, RTS)
{
    bus8_set(0x004202, 0x10);
    bus8_set(0x004203, abs8(0x9B1C));
    g_cpu.X = asl8(abs8(0x9AA2));
    dc_set_a8((u8)g_cpu.X);
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(bus16(0xCC2D69 + g_cpu.X), bus16(0x004216));
    bus16_set(0x004204, g_cpu.A);
    dc_set_mx(1, 1);
    lda8(9);
}
DC_ENTRY(C2D306, RTS)                            /* shift 16 columns of two 2bpp tiles right by 2 */
{
    g_cpu.X = abs16(0x0DC5);
    for (g_cpu.Y = 0x10; g_cpu.Y; g_cpu.Y--) {
        for (int h = 0; h < 0x200; h += 0x100)
            for (int k = 0; k < 2; k++) {
                u8 a = abs8(h + g_cpu.X);
                g_cpu._flag_C = a & 1;
                abs8_set(h + g_cpu.X, a >> 1);
                u8 b = abs8(h + 0x10 + g_cpu.X), c = g_cpu._flag_C;
                g_cpu._flag_C = b & 1;
                abs8_set(h + 0x10 + g_cpu.X, (u8)(b >> 1 | c << 7));
            }
        g_cpu.X++;
    }
    dc_set_flags_nz16(0);
}
static void cursor_row(u16 var, u8 v, u8 mul_base, u16 out, int clear0a)
{
    dc_set_a8(v);
    abs8_set(var, v);
    bus8_set(0x004203, v);
    if (clear0a) abs8_set(0x0F0A, 0);
    g_cpu._flag_C = 0;
    u8 a = dc_adc8(mul_base, bus8(0x004216));
    dc_set_a8(a);
    abs8_set(out, a);
    if (clear0a) abs8_set(0x0F0B, a);
}
DC_ENTRY(C2E2A0, RTS) { u8 v = (u8)(abs8(0x0F09) - 1); if (v & 0x80) v = 7; cursor_row(0x0F09, v, 0x53, 0x9381, 1); }
DC_ENTRY(C2E2AA, RTS) { u8 v = (u8)(abs8(0x0F09) + 1); if (v >= 8) v = 0; cursor_row(0x0F09, v, 0x53, 0x9381, 1); }
DC_ENTRY(C2E2CC, RTS) { u8 v = (u8)(abs8(0x0F08) - 1); if (v & 0x80) v = 9; cursor_row(0x0F08, v, 0x24, 0x9380, 0); }
DC_ENTRY(C2E2D6, RTS) { u8 v = (u8)(abs8(0x0F08) + 1); if (v >= 10) v = 0; cursor_row(0x0F08, v, 0x24, 0x9380, 0); }
DC_ENTRY(C2E34A, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    u8 a = abs8(0x0F06);
    if (a >= 5) a = 4;
    a = (u8)(a * 8 + 0x9C);
    dc_set_a8(a);
    abs8_set(0x0880, a);
    abs8_set(0x0884, a);
    plp();
}
DC_ENTRY(C2EF2B, RTS)                            /* copy $8C rows of $8A+1 bytes, 64 bytes apart */
{
    do {
        g_cpu.X = dp16(0x63); g_cpu.Y = dp16(0x65); g_cpu.A = dp16(0x8A);
        dc_mvn(0x7E, 0x7E);
        g_cpu._flag_C = 0;
        dp16_set(0x65, dc_adc16(dp16(0x65), 0x0040));
        dp16_set(0x63, dc_adc16(dp16(0x63), 0x0040));
        g_cpu.A = dp16(0x63);
        u16 n = (u16)(dp16(0x8C) - 1);
        dp16_set(0x8C, n);
        dc_set_flags_nz16(n);
    } while (dp16(0x8C));
}
/* Y tile pairs (top, bottom = +0x10) of a window frame row. */
static void tile_rows(u16 a)
{
    php();
    dc_set_mx(0, 0);
    do {
        abs16_set(g_cpu.X, a);
        a |= 0x0010;
        abs16_set(0x0040 + g_cpu.X, a);
        a = (u16)((a + 1) & 0xFFEF);
        g_cpu.X += 2;
        g_cpu.Y--;
    } while (g_cpu.Y);
    g_cpu.A = a;
    plp();
}
DC_ENTRY(C2FBB4, RTS) { tile_rows(g_cpu.A & 0x33EF); }
DC_ENTRY(C2FB97, RTS) { tile_rows((g_cpu.A & 0x00EF) | 0x3100); }
DC_ENTRY(C21DB5, RTS)
{
    dc_set_mx(0, 0);
    for (g_cpu.X = 0; g_cpu.X != 0x0200; g_cpu.X += 2) abs16_set(0x0920 + g_cpu.X, 0);
    for (g_cpu.X = 0; g_cpu.X != 0x0010; g_cpu.X += 2) abs16_set(0x0B20 + g_cpu.X, 0);
    dc_set_flags_nz16(0);
    g_cpu._flag_C = 1;
    dc_set_mx(1, 0);
}
DC_ENTRY(C22703, RTS)                            /* tiles (0102/0103) to pixels (0283/0285) */
{
    dc_set_mx(0, 0);
    abs16_set(0x0283, (u16)((abs16(0x0102) & 0xFF) << 3));
    g_cpu.A = (u16)((abs16(0x0103) & 0xFF) << 3);
    abs16_set(0x0285, g_cpu.A);
    g_cpu._flag_C = 0;
    dc_set_flags_nz16(g_cpu.A);
    dc_set_mx(1, 0);
}
DC_ENTRY(C290FC, RTS)                            /* save 7E:2400 to 9B96 and clear it */
{
    php();
    dc_set_mx(0, 0);
    lda16(abs16(0x0D7E));
    if (g_cpu.A) {
        g_cpu.X = 0x2400; g_cpu.Y = 0x9B96; g_cpu.A = 0x01FF;
        dc_mvn(0x7E, 0x7E);
        abs16_set(0x2400, 0);
        g_cpu.X = 0x2400; g_cpu.Y = 0x2402; g_cpu.A = 0x00FD;
        dc_mvn(0x7E, 0x7E);
    }
    plp();
}
DC_ENTRY(C2ED90, RTS)                            /* $63 = $61 + 2*(x & 1F) + 64*y for the tile at $5D */
{
    php();
    dc_set_mx(0, g_cpu.x_flag);
    u16 a = lsr16(lsr16(dp16(0x5D) & 0xFF00));
    dp16_set(0x63, a);
    a = asl16(dp16(0x5D) & 0x001F);
    a |= dp16(0x63);
    dp16_set(0x63, dc_adc16(a, dp16(0x61)));
    g_cpu.A = dp16(0x63);
    plp();
}
DC_ENTRY(C2F20B, RTS)                            /* a digit tile (D4+) or blank (FF) and attribute $7E */
{
    u8 a = (u8)g_cpu.A & 0x0F, m = dp8(0x8F);
    g_cpu._flag_Z = a == 0;
    if (!a && (g_cpu._flag_V = m >> 6 & 1, g_cpu._flag_N = m >> 7, m & 0x80)) a = 0xFF;
    else { dp8_set(0x8F, 0); g_cpu._flag_C = 0; a = dc_adc8(a, 0xD4); }
    abs8_set(g_cpu.Y, a);
    dc_set_a8(dp8(0x7E));
    abs8_set(0x0001 + g_cpu.Y, (u8)g_cpu.A);
    ldy16((u16)(g_cpu.Y + 2));
}
DC_ENTRY(C2F626, RTS)                            /* Y = 0D2C = A * 64 */
{
    u8 a0 = (u8)g_cpu.A;
    dc_push8(a0);
    php();
    dc_set_mx(1, g_cpu.x_flag);
    bus8_set(0x004202, a0);
    bus8_set(0x004203, 0x40);
    dc_set_mx(0, 0);
    g_cpu.A = bus16(0x004216);
    abs16_set(0x0D2C, g_cpu.A);
    g_cpu.Y = g_cpu.A;
    plp();
    lda8(dc_pull8());
}
DC_ENTRY(C2960B, RTS)                            /* expand A 1bpp... rows: 16 bytes copied, 8 bytes widened */
{
    dp16_set(0x02, g_cpu.A);
    do {
        g_cpu.A = 0x000F;
        dc_mvn(0x7E, 0xFF);
        dp16_set(0x00, 8);
        do {
            g_cpu.A = bus16(0xFF0000 + g_cpu.X) & 0xFF;
            abs16_set(g_cpu.Y, g_cpu.A);
            g_cpu.X++; g_cpu.Y += 2;
            dp16_set(0x00, (u16)(dp16(0x00) - 1));
        } while (dp16(0x00));
        dp16_set(0x02, (u16)(dp16(0x02) - 1));
    } while (dp16(0x02));
    dc_set_flags_nz16(0);
}
DC_ENTRY(C2ED77, RTS)                            /* fill $5B bytes at [$61] with the word $5D */
{
    php();
    dc_set_mx(0, 0);
    g_cpu.X = dp16(0x61);
    g_cpu.A = dp16(0x5D);
    dp16_set(0x7D, g_cpu.A);
    abs16_set(g_cpu.X, g_cpu.A);
    g_cpu.Y = (u16)(g_cpu.X + 2);
    g_cpu.A = (u16)(dp16(0x5B) - 3);
    dc_mvn(0x7E, 0x7E);
    plp();
}

/* ---- batch 4 ------------------------------------------------------------- */
DC_ENTRY(C20F38, RTS)                            /* object position += velocity (two 32-bit adds) */
{
    dc_set_mx(0, 0);
    ldx16(dp16(0x4E));
    for (u16 f = 0x12; f <= 0x16; f += 4) {
        g_cpu._flag_C = 0;
        abs16_set(f + g_cpu.X, dc_adc16(abs16(f + g_cpu.X), abs16(f + 8 + g_cpu.X)));
        g_cpu.A = dc_adc16(abs16(f + 2 + g_cpu.X), abs16(f + 10 + g_cpu.X));
        abs16_set(f + 2 + g_cpu.X, g_cpu.A);
    }
}
/* Script branch: A = signed offset from script byte `at`, C=0 (taken). */
static void branch_off(u8 at, u16 hi)
{
    u8 a = bus8(lp(0x58) + at);
    g_cpu.Y = at;
    dc_set_mx(0, 0);
    g_cpu.A = (u16)(hi | a);
    dc_set_flags_nz8(a);
    if (a & 0x80) lda16(g_cpu.A | 0xFF00);
    g_cpu._flag_C = 0;
}
/* Script ops: branch when the byte the script names is zero / nonzero. */
static void branch_byte(int when_zero)
{
    g_cpu.Y = 1;
    ldx16(bus16(lp(0x58) + 1));
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    u8 v = abs8(g_cpu.X);
    dc_set_a8(v);
    if ((v == 0) != when_zero) { dc_set_mx(0, 0); lda16(4); return; }
    branch_off(3, g_cpu.D & 0xFF00);
}
DC_ENTRY(C21469, RTS) { branch_byte(1); }
DC_ENTRY(C2148B, RTS) { branch_byte(0); }
/* Branch when (byte & mask) is zero / nonzero. */
static void branch_mask(int when_zero)
{
    g_cpu.Y = 1;
    ldx16(bus16(lp(0x58) + 1));
    dc_set_mx(1, 0);
    g_cpu.Y = 3;
    u8 v = bus8(lp(0x58) + 3) & abs8(g_cpu.X);
    dc_set_a8(v);
    if ((v == 0) != when_zero) { dc_set_mx(0, 0); lda16(5); return; }
    g_cpu.A = g_cpu.D;
    branch_off(4, g_cpu.D & 0xFF00);
}
DC_ENTRY(C214F7, RTS) { branch_mask(0); }
DC_ENTRY(C2151C, RTS) { branch_mask(1); }
/* Branch when byte < v / >= v. */
static void branch_cmp(int when_less)
{
    g_cpu.Y = 1;
    ldx16(bus16(lp(0x58) + 1));
    g_cpu.Y = 3;
    g_cpu.A = abs16(g_cpu.X);
    dc_set_mx(1, 0);
    dc_cmp8((u8)g_cpu.A, bus8(lp(0x58) + 3));
    if (g_cpu._flag_C == when_less) { dc_set_mx(0, 0); lda16(5); return; }
    g_cpu.A = g_cpu.D;
    branch_off(4, g_cpu.D & 0xFF00);
}
DC_ENTRY(C21541, RTS) { branch_cmp(1); }
DC_ENTRY(C21566, RTS) { branch_cmp(0); }
DC_ENTRY(C21375, RTS)                            /* loop: decrement a field, branch back while nonzero */
{
    dc_set_mx(1, 0);
    dc_set_a8(bus8(lp(0x58) + 1));
    g_cpu.Y = g_cpu.A;
    u32 at = dba(dp16(0x4E)) + g_cpu.Y;
    u8 v = (u8)(bus8(at) - 1);
    dc_set_a8(v);
    bus8_set(at, v);
    if (!v) { dc_set_mx(0, 0); lda16(3); return; }
    branch_off(2, g_cpu.A & 0xFF00);
}
/* Wait op: count a field (`f`) down from the script's byte 1; A = D while waiting. */
static void wait_field(u8 script, u8 f, int clc_done)
{
    dc_set_mx(1, 0);
    ldx16(dp16(0x4E));
    u8 v = abs8(f + g_cpu.X);
    if (!v) {
        g_cpu.Y = 1;
        lda8(bus8(lp(script) + 1));
        abs8_set(f + g_cpu.X, (u8)g_cpu.A);
    } else {
        v--;
        abs8_set(f + g_cpu.X, v);
        dc_set_flags_nz8(v);
        if (!v) { dc_set_mx(0, 0); lda16(2); if (clc_done) g_cpu._flag_C = 0; return; }
    }
    dc_set_mx(0, 0);
    lda16(g_cpu.D);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C2186B, RTS) { wait_field(0x58, 0x0A, 0); }
DC_ENTRY(C20EDC, RTS) { wait_field(0x52, 0x0E, 1); }
DC_ENTRY(C21759, RTS)                            /* clear velocity high words unless moving left/up */
{
    for (u16 f = 0x14; f <= 0x18; f += 4) {
        u16 v = abs16(f + g_cpu.X);
        dc_set_flags_nz16(v);
        if (v & 0x8000) {
            u16 lo = abs16(f - 2 + g_cpu.X);
            dc_set_flags_nz16(lo);
            if (lo) { lda16(0xFFFF); abs16_set(f + g_cpu.X, 0xFFFF); g_cpu.A = 0xFFFF; continue; }
            g_cpu.A = lo;
        } else g_cpu.A = v;
        abs16_set(f + g_cpu.X, 0);
    }
}
/* Is the 32-bit velocity at (lo, hi) at least one whole unit? C=1 and $01 = the high byte. */
static void moving(u16 lo, u16 hi)
{
    u16 v = abs16(hi + g_cpu.X);
    g_cpu.A = v;
    dc_set_flags_nz16(v);
    if (v & 0x8000) {
        g_cpu._flag_C = 0;
        dc_adc16((u16)~abs16(lo + g_cpu.X), 1);
        g_cpu.A = dc_adc16((u16)~v, 0);
    }
    if (g_cpu._flag_Z) { g_cpu._flag_C = 0; return; }
    dc_set_mx(1, 0);
    lda8(abs8(hi + g_cpu.X));
    dp8_set(0x01, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 1;
}
DC_ENTRY(C21784, RTS) { moving(0x12, 0x14); }
DC_ENTRY(C217AB, RTS) { moving(0x16, 0x18); }
/* Signed sine from the table at F800 (8 bits, 0x100 = full turn). */
static void sine8(void)
{
    u16 a = g_cpu.A & 0xFF;
    dc_cmp16(a, 0x40);
    if (a == 0x40) { lda16(0x0080); return; }
    dc_cmp16(a, 0xC0);
    if (a == 0xC0) { lda16(0xFF80); return; }
    g_cpu.X = a;
    u16 v = abs16(0xF800 + g_cpu.X);
    lda16((v & 0x80) ? (v | 0xFF00) : (v & 0xFF));
}
DC_ENTRY(C22277, RTL) { sine8(); }
DC_ENTRY(C22273, RTL) { g_cpu._flag_C = 0; g_cpu.A = dc_adc16(g_cpu.A, 0x0040); sine8(); }
DC_ENTRY(C226A8, RTS)                            /* DMA C2:26D4 (fixed word) over all of VRAM */
{
    abs16_set(0x2116, 0);
    abs8_set(0x2115, 0x80);
    abs8_set(0x4370, 0x09);
    abs8_set(0x4371, 0x18);
    abs16_set(0x4372, 0x26D4);
    abs8_set(0x4374, 0xC2);
    ldx16(0);
    abs16_set(0x4375, 0);
    lda8(0x80);
    abs8_set(0x420B, 0x80);
}
DC_ENTRY(C23F6D, RTS)                            /* follow the trail: position 1 or 2 entries back */
{
    u16 a = abs16(0x1B5A);
    if (abs16(0x0024 + g_cpu.X) & 1) a -= 2;
    else { g_cpu._flag_C = 1; a = dc_sbc16(a, 4); }
    g_cpu.Y = a & 0x1F;
    abs16_set(0x002A + g_cpu.X, abs16(0x1B5B + g_cpu.Y));
    lda16(abs16(0x1B7B + g_cpu.Y));
    abs16_set(0x002C + g_cpu.X, g_cpu.A);
}
DC_ENTRY(C285AE, RTS)                            /* tick the play-time counter at 0400 (D stays 0400) */
{
    dc_set_mx(1, 1);
    dc_push16(0x0400);
    g_cpu.D = dc_pull16();
    lda8(dp8(0x07));
    if (g_cpu.A & 0xFF) return;
    for (g_cpu.X = 0; g_cpu.X < 6; g_cpu.X++) {
        u16 at = (u16)(g_cpu.D + g_cpu.X);
        u8 v = (u8)(b0_8(at) + 1);
        b0_8_set(at, v);
        dc_set_a8(v);
        dc_cmp8(v, bus8(0xC285D0 + g_cpu.X));
        if (!g_cpu._flag_C) return;
        b0_8_set(at, 0);
    }
    dc_cmp8(6, 6);
    lda8(1);
    dp8_set(0x06, 1);
}
DC_ENTRY(C288D8, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    bus8_set(0x004202, abs8(0x04CE));
    g_cpu.X = abs16(0x04CC);
    bus8_set(0x004203, bus8(0xFFCF27 + g_cpu.X));
    dc_set_mx(0, 0);
    g_cpu.X = asl16(g_cpu.X);
    g_cpu.A = dc_adc16(bus16(0xFFCF1D + g_cpu.X), bus16(0x004216));
    abs16_set(0x0D2A, g_cpu.A);
    g_cpu.X = g_cpu.A;
    plp();
}
DC_ENTRY(C2A1EF, RTS)
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x0D5D, 0);
    u16 a = abs16(0x9A90) & 7;
    abs16_set(0x0D4D, a);
    g_cpu.X = (u16)(a << 1);
    if (abs16(0x0D38 + g_cpu.X) >= abs16(0x9A93)) abs16_set(0x0D5D, 4);
    g_cpu.A = 0x0020;
    abs16_set(0x0D4D, abs16(0x0D4D) | 0x0020);
    plp();
}
DC_ENTRY(C25EE5, RTS) { put_three(3, 0x5FE6); }
DC_ENTRY(C25FB6, RTS) { put_three(3, 0x6066); }
DC_ENTRY(C28D1C, RTS)                            /* entry 3 = min(3E7, value 5 * 1.25 / 1.5 by kind 2A) */
{
    g_cpu.X = abs16(0x002A + g_cpu.Y) & 0xFF;
    u16 a = abs16(0x0005 + g_cpu.Y);
    if (g_cpu.X == 0xA1) a = lsr16(a);
    else if (g_cpu.X == 0xA0) a = lsr16(lsr16(a));
    else a = lsr16(lsr16(0));
    g_cpu._flag_C = 0;
    a = dc_adc16(a, abs16(0x0005 + g_cpu.Y));
    dc_cmp16(a, 0x03E7);
    if (g_cpu._flag_C) lda16(0x03E7); else g_cpu.A = a;
    abs16_set(0x0003 + g_cpu.Y, g_cpu.A);
}
DC_ENTRY(C2984A, RTS)                            /* reset the OAM buffer (y = E0, size bits AA) */
{
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(0, 0);
    abs16_set(0x0700, 0xE000);
    g_cpu.X = 0x0700; g_cpu.Y = 0x0702; g_cpu.A = 0x01FD;
    dc_mvn(0x7E, 0x7E);
    abs16_set(0x0900, 0xAAAA);
    g_cpu.X = 0x0900; g_cpu.Y = 0x0902; g_cpu.A = 0x001D;
    dc_mvn(0x7E, 0x7E);
    plp();
    plb();
}
/* Mark sprite slots (0x40 bytes each, from `top` down to 1800) free. */
static void free_slots(u16 top)
{
    php();
    dc_push16(g_cpu.D);
    dc_push16(top);
    g_cpu.D = dc_pull16();
    for (;;) {
        dc_set_mx(1, 1);
        dc_set_a8(0xFF);
        dp8_set(0x00, 0xFF);
        dp8_set(0x11, 0);
        dp8_set(0x18, 0);
        dc_set_mx(0, 1);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_sbc16(g_cpu.D, 0x003F);
        g_cpu.D = g_cpu.A;
        dc_cmp16(g_cpu.A, 0x1800);
        if (!g_cpu._flag_C) break;
    }
    g_cpu.D = dc_pull16();
    plp();
}
DC_ENTRY(C2CE2A, RTS) { free_slots(0x1980); }
DC_ENTRY(C2F588, RTS) { free_slots(0x19C0); }
/* Decompress entry [$ED],y of a C6 pointer table (unless negative) to dst. */
static void dec_req(u16 dst, u8 bank, u16 y, u32 tab, u32 site)
{
    abs16_set(0x0303, dst);
    abs8_set(0x0305, bank);
    g_cpu.X = dst;
    g_cpu.Y = y;
    g_cpu.A = g_cpu.D;
    u8 a = bus8(lp(0xED) + y);
    lda8(a);
    if (a & 0x80) return;
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)(a << 1), a));
    c6_request(tab, site);
}
DC_ENTRY(C22826, RTS) { dec_req(0x9000, 0x7F, 0x08, 0xC6FE20, 0xC2284F); }
DC_ENTRY(C22854, RTS) { dec_req(0x9000, 0x7F, 0x14, 0xC6FF40, 0xC2287D); }
DC_ENTRY(C22902, RTS) { dec_req(0xB800, 0x7E, 0x07, 0xC6FE20, 0xC2292B); }
DC_ENTRY(C22930, RTS) { dec_req(0xC000, 0x7E, 0x0B, 0xC6FEA0, 0xC22959); }
DC_ENTRY(C2295E, RTS) { dec_req(0x3000, 0x7E, 0x10, 0xC6FF00, 0xC22987); }
DC_ENTRY(C2298C, RTS) { dec_req(0x4000, 0x7E, 0x11, 0xC6FF20, 0xC229B5); }
DC_ENTRY(C229BA, RTS) { dec_req(0x7000, 0x7E, 0x12, 0xC6FF80, 0xC229E3); }
DC_ENTRY(C229E8, RTS) { dec_req(0x7200, 0x7E, 0x13, 0xC6FFA0, 0xC22A11); }
DC_ENTRY(C22A16, RTS) { dec_req(0x0400, 0x7F, 0x16, 0xC6FFE0, 0xC22A3F); }
DC_ENTRY(C22BFB, RTS)                            /* clear 7E:8621 (2 KB) through the WRAM port */
{
    dc_push16(g_cpu.D);
    g_cpu.A = 0x2100;
    g_cpu.D = 0x2100;
    g_cpu.X = 0x8621;
    dp16_set(0x81, 0x8621);
    dp8_set(0x83, 0x7E);
    for (g_cpu.X = 0x0200; g_cpu.X; g_cpu.X--)
        for (int k = 0; k < 4; k++) dp8_set(0x80, 0);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
DC_ENTRY(C28881, RTS)                            /* split A by the C2:88A8 thresholds: 04CC = band, 04CE = rest */
{
    u16 x0 = g_cpu.X;
    dc_push16(x0);
    php();
    dc_set_mx(1, 1);
    u8 a = (u8)g_cpu.A;
    g_cpu.X = 0;
    while (a >= bus8(0xC288A8 + g_cpu.X) && ++g_cpu.X < 4) {}
    g_cpu._flag_C = 1;
    a = dc_sbc8(a, bus8(0xC288A7 + g_cpu.X));
    abs8_set(0x04CE, a);
    abs8_set(0x04CF, 0);
    dc_set_a8((u8)g_cpu.X);
    abs8_set(0x04CC, (u8)g_cpu.X);
    abs8_set(0x04CD, 0);
    plp();
    ldx16(dc_pull16());
}
DC_ENTRY(C2EF97, RTS)                            /* store tile A + attribute $7E at 7E:X (FF also above) */
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    u8 m = dp8(0x88);
    g_cpu._flag_N = m >> 7; g_cpu._flag_V = m >> 6 & 1; g_cpu._flag_Z = (m & (u8)g_cpu.A) == 0;
    if (!(m & 0x80) && (u8)g_cpu.A == 0xFF) {
        bus8_set(0x7DFFC0 + g_cpu.X, 0xFF);
        bus8_set(0x7DFFC1 + g_cpu.X, dp8(0x7E));
        g_cpu.A = (u16)(dp8(0x7E) << 8 | 0xFF);
    }
    bus8_set(0x7E0000 + g_cpu.X, (u8)g_cpu.A);
    dc_set_a8(dp8(0x7E));
    bus8_set(0x7E0001 + g_cpu.X, (u8)g_cpu.A);
    g_cpu.X += 2;
    plp();
}
DC_ENTRY(C2F071, RTS)
{
    dc_push16(g_cpu.Y);
    dc_push8(g_cpu.DB);
    g_cpu.Y = abs16(g_cpu.Y);
    dc_push16(0x7E7E);
    g_cpu.DB = dc_pull8();
    g_cpu.DB = dc_pull8();
    g_cpu.A = abs16(g_cpu.Y);
    abs16_set(g_cpu.X, g_cpu.A);
    g_cpu.DB = dc_pull8();
    g_cpu.Y = dc_pull16();
    dc_set_mx(1, 0);
    g_cpu.X += 2;
    u8 n = (u8)(dp8(0x87) - 1);
    dp8_set(0x87, n);
    ldy16((u16)(g_cpu.Y + 2));
}

/* ---- batch 5 ------------------------------------------------------------- */
static void set_db0(void) { dc_push16(0x0000); g_cpu.DB = dc_pull8(); g_cpu.DB = dc_pull8(); }

DC_ENTRY(C220DF, RTS)                            /* object timer 3 reached 0A? then count $1D down */
{
    dc_set_mx(1, 0);
    ldx16(dp16(0x4E));
    u8 a = abs8(0x0003 + g_cpu.X);
    dc_set_a8(a);
    dc_cmp8(a, abs8(0x000A + g_cpu.X));
    if (!g_cpu._flag_C) { g_cpu._flag_C = 0; return; }
    abs8_set(0x0003 + g_cpu.X, 0);
    a = dp8(0x1D);
    lda8(a);
    if (a) {
        u8 n = (u8)(a - 1);
        dp8_set(0x1D, n);
        dc_set_flags_nz8(n);
        if (n) { g_cpu._flag_C = 0; return; }
    }
    dc_set_a8(0x80);
    dp8_set(0x1C, 0x80);
    dc_set_a8(0x02);
    u8 m = abs8(0x1BF6);
    g_cpu._flag_Z = (m & 2) == 0;
    abs8_set(0x1BF6, m & ~2);
    g_cpu._flag_C = 1;
}
/* Y tiles of 8 rows: OR neighbouring 2bpp planes into an outline. */
static void outline(int three)
{
    php();
    do {
        dc_set_mx(1, 0);
        dc_set_a8(8);
        dp8_set(0x00, 8);
        do {
            u8 a = abs8(g_cpu.X) | abs8(0x0001 + g_cpu.X);
            if (three) a |= abs8(0x0010 + g_cpu.X);
            else abs8_set(0x0010 + g_cpu.X, a);
            dc_set_a8(a);
            abs8_set(0x0011 + g_cpu.X, a);
            g_cpu.X += 2;
            dp8_set(0x00, (u8)(dp8(0x00) - 1));
        } while (dp8(0x00));
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0010);
        g_cpu.X = g_cpu.A;
        g_cpu.Y--;
    } while (g_cpu.Y);
    plp();
}
DC_ENTRY(C2962E, RTS) { outline(1); }
DC_ENTRY(C29653, RTS) { outline(0); }
DC_ENTRY(C22720, RTS)                            /* scroll origin = tile position - (16, 14), wrapped */
{
    dc_set_mx(0, 0);
    g_cpu._flag_C = 1;
    u16 a = dc_sbc16(abs16(0x0102) & 0xFF, 0x0010);
    if (a & 0x8000) { g_cpu._flag_C = 0; a = dc_adc16(a, 0x00C0); }
    dp16_set(0xE3, a); dp16_set(0xE5, a);
    g_cpu._flag_C = 1;
    a = dc_sbc16(abs16(0x0103) & 0xFF, 0x000E);
    if (a & 0x8000) { g_cpu._flag_C = 0; a = dc_adc16(a, 0x0080); }
    dp16_set(0xE7, a); dp16_set(0xE9, a);
    g_cpu.A = a;
    dc_set_mx(1, 0);
}
DC_ENTRY(C2816A, RTS)                            /* list the present characters: 0D5F = who, 0D68 = slot */
{
    php();
    dc_set_mx(1, 1);
    g_cpu.X = 0; g_cpu.Y = 0;
    dp8_set(0x73, 0); dp8_set(0x75, 0);
    for (; g_cpu.X < 9; g_cpu.X++) {
        u8 a = abs8(0x2980 + g_cpu.X);
        dc_set_a8(a);
        if (a & 0x80) continue;
        abs8_set(0x0D5F + g_cpu.Y, a);
        dc_set_a8((u8)g_cpu.X);
        abs8_set(0x0D68 + g_cpu.Y, (u8)g_cpu.X);
        g_cpu.Y++;
        if (g_cpu.X < 3) dp8_set(0x73, (u8)(dp8(0x73) + 1));
        dp8_set(0x75, (u8)(dp8(0x75) + 1));
    }
    plp();
}
DC_ENTRY(C29542, RTS)                            /* DMA ch0: Y bytes from FF:A to WRAM 7E:X */
{
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(0, 0);
    set_db0();
    abs16_set(0x4302, g_cpu.A);
    abs16_set(0x2181, g_cpu.X);
    abs16_set(0x4305, g_cpu.Y);
    g_cpu.X = 0x8000;
    abs16_set(0x4300, 0x8000);
    dc_set_mx(1, 0);
    abs8_set(0x2183, 0);
    abs8_set(0x4304, 0xFF);
    dc_set_a8(0x01);
    abs8_set(0x420B, 0x01);
    plp();
    plb();
}
DC_ENTRY(C29988, RTS)                            /* drop empty / unusable entries from the 2400/2500 list */
{
    php();
    dc_set_mx(1, 1);
    g_cpu.X = 0;
    do {
        u8 n = abs8(0x2500 + g_cpu.X), a = n;
        int drop = n == 0;
        if (!drop) {
            a = abs8(0x2400 + g_cpu.X);
            drop = a == 0 || a == 0x5A || a == 0x7B || a == 0x94 || a >= 0xF2;
        }
        dc_set_a8(a);
        if (drop) { abs8_set(0x2400 + g_cpu.X, 0); abs8_set(0x2500 + g_cpu.X, 0); }
        g_cpu.X = (u8)(g_cpu.X + 1);
    } while (g_cpu.X);
    plp();
}
DC_ENTRY(C2EE58, RTS)                            /* a column of $8C alternating frame tiles at $65 */
{
    ldx16(dp16(0x8C));
    if (g_cpu.X) {
        g_cpu.Y = dp16(0x65);
        do {
            u16 a = dp16(0x08);
            abs16_set(g_cpu.Y, a);
            a++;
            bus16_set(dba(dp16(0x06)) + g_cpu.Y, a);
            a ^= 3;
            dp16_set(0x08, a);
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(g_cpu.Y, 0x0040);
            g_cpu.Y = g_cpu.A;
            g_cpu.X--;
        } while (g_cpu.X);
        dp16_set(0x65, g_cpu.A);
    }
    lda16((u16)((dp16(0x08) | 3) + 1));
    dp16_set(0x08, g_cpu.A);
}
DC_ENTRY(C2F566, RTS)                            /* reset sprite slots 1980..1800 (owner = index) */
{
    php();
    dc_push16(g_cpu.D);
    dc_set_mx(1, 1);
    dc_push16(0x1980);
    g_cpu.D = dc_pull16();
    for (g_cpu.X = 6; g_cpu.X != 0xFF; g_cpu.X = (u8)(g_cpu.X - 1)) {
        dc_set_mx(1, 1);
        dp8_set(0x00, (u8)g_cpu.X);
        dp8_set(0x11, 0);
        dc_set_a8(0x40);
        dp8_set(0x18, 0x40);
        dc_set_mx(0, 1);
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(g_cpu.D, 0x0040);
        g_cpu.D = g_cpu.A;
    }
    g_cpu.D = dc_pull16();
    plp();
}
DC_ENTRY(C22ED9, RTS)                            /* latch 1BE8..EB into 1E00 when allowed; C = refused */
{
    u8 a = abs8(0x1BEC);
    if (a) {
        if (a & 0x80) { dc_set_a8(a); dc_set_flags_nz8(a); g_cpu._flag_C = 1; return; }
        a = abs8(0x1BEE);
        dc_set_a8(a);
        dc_cmp8(a, abs8(0x1BED));
        if (!g_cpu._flag_C) { g_cpu._flag_C = 1; return; }
    }
    for (int k = 0; k < 4; k++) abs8_set(0x1E00 + k, abs8(0x1BE8 + k));
    abs8_set(0x1BEE, abs8(0x1BED));
    lda8(1);
    abs8_set(0x1BEC, 1);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C291EB, RTS)                            /* divide 5 * 0DB5 + 12 * 9B1C (dividend for 4204) */
{
    bus8_set(0x004202, 0x05);
    u8 v = abs8(0x0DB5);
    bus8_set(0x004203, v);
    dc_set_mx(0, 1);
    g_cpu.A = bus16(0x004216);
    dp16_set(0x00, g_cpu.A);
    dc_set_mx(1, 1);
    bus8_set(0x004202, 0x0C);
    bus8_set(0x004203, abs8(0x9B1C));
    dc_set_mx(0, 1);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(dp16(0x00), bus16(0x004216));
    bus16_set(0x004204, g_cpu.A);
    dc_set_mx(1, 1);
    lda8(9);
}
DC_ENTRY(C29512, RTS)                            /* DMA ch0: fill Y bytes of WRAM 7E:X with C2:9510 */
{
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(1, g_cpu.x_flag);
    set_db0();
    abs8_set(0x420C, 0);
    abs16_set(0x2181, g_cpu.X);
    abs8_set(0x2183, 0);
    abs16_set(0x4305, g_cpu.Y);
    abs16_set(0x4300, 0x8008);
    g_cpu.X = 0x9510;
    abs16_set(0x4302, 0x9510);
    abs8_set(0x4304, 0xC2);
    dc_set_a8(0x01);
    abs8_set(0x420B, 0x01);
    plp();
    plb();
}
DC_ENTRY(C2ED08, RTS)                            /* copy a 16-byte palette from FF:B210 + 16*(0D8C&7) */
{
    php();
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16((u16)((abs16(0x0D8C) & 7) << 4), 0xB210);
    g_cpu._flag_C = 0;
    g_cpu.Y = dc_adc16((u16)((abs16(0x0D48) & 0x1C) << 3), 0x9490);
    g_cpu.A = 0x000F;
    dc_mvn(0x7E, 0xFF);
    abs16_set(0x0D15, (u16)(abs16(0x0D15) + 1));
    plp();
}
DC_ENTRY(C2012F, RTS)                            /* clear 00:0000-00EF and set the menu defaults */
{
    dc_push8(g_cpu.DB);
    dc_set_mx(0, 0);
    dp16_set(0x00, 0);
    g_cpu.X = 0; g_cpu.Y = 1; g_cpu.A = 0x00EE;
    dc_mvn(0x00, 0x00);
    dc_set_mx(1, 0);
    plb();
    dp8_set(0x1C, 0x80);
    dp8_set(0x1E, 0x09);
    dp8_set(0x2D, 0x01);
    dp8_set(0x33, 0x01);
    dp8_set(0x3B, (u8)(dp8(0x3B) + 1));
    dp8_set(0x3D, (u8)(dp8(0x3D) + 1));
    dp8_set(0x41, 0x17);
    lda8(0x13);
    dp8_set(0x42, 0x13);
}
DC_ENTRY(C23A56, RTS)                            /* target = position + 8 in the direction of motion */
{
    dc_set_mx(0, 0);
    ldx16(dp16(0x4E));
    abs16_set(0x002A + g_cpu.X, abs16(0x0014 + g_cpu.X));
    abs16_set(0x002C + g_cpu.X, abs16(0x0018 + g_cpu.X));
    for (int k = 0; k < 2; k++) {
        u16 v = abs16((k ? 0x0020 : 0x001C) + g_cpu.X), f = k ? 0x002C : 0x002A;
        g_cpu.A = 0x0008;
        ldy16(v);
        if (!v) { g_cpu.A = 0x0008; continue; }
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16((v & 0x8000) ? 0xFFF8 : 0x0008, abs16(f + g_cpu.X));
        abs16_set(f + g_cpu.X, g_cpu.A);
    }
}
DC_ENTRY(C2E316, RTS)                            /* append the character under the name cursor */
{
    php();
    dc_set_mx(1, 1);
    bus8_set(0x004202, 0x0A);
    bus8_set(0x004203, abs8(0x0F09));
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc8(abs8(0x0F08), bus8(0x004216));
    u8 a = bus8(0xFFC9AC + g_cpu.X);
    dc_set_a8(a);
    g_cpu.Y = abs8(0x0F06);
    if (g_cpu.Y >= 5) g_cpu.Y = 4;
    abs8_set(0x0F00 + g_cpu.Y, a);
    g_cpu.Y++;
    if (g_cpu.Y < 6) abs8_set(0x0F06, (u8)g_cpu.Y);
    plp();
}
DC_ENTRY(C2F337, RTS)                            /* fill a window record at [$5B] */
{
    dc_push8(g_cpu.DB);
    php();
    set_db0();
    g_cpu.Y = dp16(0x5B);
    abs8_set(0x0004 + g_cpu.Y, 0x7E);
    abs8_set(0x0007 + g_cpu.Y, 0x7E);
    dc_set_mx(0, 0);
    abs16_set(g_cpu.Y, dp16(0x5D));
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16((u16)((dp16(0x5B) & 0xF0) << 3), 0x9000);
    dp16_set(0x61, g_cpu.A);
    abs16_set(0x0002 + g_cpu.Y, g_cpu.A);
    plp();
    plb();
}
DC_ENTRY(C22BCF, RTS)                            /* copy two 64-byte rows from 7F:AC00 + 64*A to Y */
{
    dc_push8(g_cpu.DB);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16((u16)((g_cpu.A & 0xFF) << 6), 0xAC00);
    dp16_set(0x08, g_cpu.A);
    dp16_set(0x0A, g_cpu.Y);
    g_cpu.X = g_cpu.A;
    g_cpu.A = 0x003F;
    dc_mvn(0x7F, 0x7F);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16(dp16(0x08), 0x0200);
    g_cpu._flag_C = 0;
    g_cpu.Y = dc_adc16(dp16(0x0A), 0x0200);
    g_cpu.A = 0x003F;
    dc_mvn(0x7F, 0x7F);
    plb();
}
DC_ENTRY(C286A9, RTS)                            /* upload the palette buffer 7E:9480 when 0D15 says so */
{
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(1, g_cpu.x_flag);
    set_db0();
    dc_set_a8(0xFF);
    u8 m = abs8(0x0D15);
    abs8_set(0x0D15, 0);
    if (m) {
        abs16_set(0x4370, 0x2200);
        abs16_set(0x4372, 0x9480);
        g_cpu.X = 0x0200;
        abs16_set(0x4375, 0x0200);
        abs8_set(0x4374, 0x7E);
        abs8_set(0x2121, 0x00);
        dc_set_a8(0x80);
        abs8_set(0x420B, 0x80);
    }
    plp();
    plb();
}

/* ---- batch 6: small routines that call other C2 routines ------------------ */
DC_ENTRY(C25C6B, RTS) { lda8(0x10); dp8_set(0x15, 0x10); }
DC_ENTRY(C25CB4, RTS) { lda8(0x10); dp8_set(0x15, 0x10); }
DC_ENTRY(C25C69, RTS) { dp8_set(0x30, 0); lda8(0x10); dp8_set(0x15, 0x10); }
DC_ENTRY(C25CB2, RTS) { dp8_set(0x30, 0); lda8(0x10); dp8_set(0x15, 0x10); }
DC_ENTRY(C2338F, RTS)
{
    ldx16(dp16(0x4E));
    lda8(1);
    abs8_set(0x0002 + g_cpu.X, 1);
    dc_jsr(0xC26263, 0xC23396);
}
DC_ENTRY(C2339A, RTS)
{
    ldx16(dp16(0x4E));
    lda8(1);
    abs8_set(0x0002 + g_cpu.X, 1);
    dc_jsr(0xC26286, 0xC233A1);
    lda8(4);
    abs8_set(0x027C, 4);
}
DC_ENTRY(C2160F, RTS)                            /* script op: object script = C3:E000[byte 1] */
{
    g_cpu.Y = 1;
    lda16(bus16(lp(0x58) + 1));
    dc_jsr(0xC21CDA, 0xC21614);
    dc_set_mx(0, 0);
    lda16(2);
}
DC_ENTRY(C28CF7, RTS)
{
    dc_jsr(0xC28CCF, 0xC28CF7);
    if (!g_cpu._flag_Z) return;
    lda8(abs8(0x2980 + g_cpu.Y) | 0x80);
    abs8_set(0x2980 + g_cpu.Y, (u8)g_cpu.A);
}
DC_ENTRY(C28E1E, RTS)
{
    dc_set_a8(dp8(0xC9));
    dc_jsr(0xC287FA, 0xC28E20);
    lda8(0);
    if (g_cpu._flag_C) return;
    g_cpu.X = dp8(0xC7);
    lda8(abs8(0x2500 + g_cpu.X));
}
DC_ENTRY(C2ADC3, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    dc_jsr(0xC29121, 0xC2ADC6);
    dc_set_a8(0x19);
    abs8_set(0x0D13, 0x19);
    plp();
}
DC_ENTRY(C24125, RTS)
{
    dc_set_mx(1, 0);
    dc_set_a8(abs8(0x0029 + g_cpu.X));
    g_cpu._flag_Z = (g_cpu.A & 0x20) == 0;
    dc_set_mx(0, 0);
    if (!g_cpu._flag_Z) dc_jsr(0xC24152, 0xC24134);
    else dc_jsr(0xC24138, 0xC24130);
}
DC_ENTRY(C2F643, RTS)
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x0D2E, 0);
    abs16_set(0x0D30, 0);
    g_cpu.A = 0xF657;
    g_cpu.X = 0x0028;
    dc_jsr(0xC28249, 0xC2F652);
    plp();
}
static void row_copy(u8 src, u32 site)
{
    php();
    dc_jsr(0xC283A2, site);
    dc_mvn(0x7E, src);
    dc_set_mx(1, 0);
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8(abs8(0x0D11), 0x08));
    abs8_set(0x0D11, (u8)g_cpu.A);
    plp();
}
DC_ENTRY(C2838E, RTS) { row_copy(0x7E, 0xC2838F); }
DC_ENTRY(C28385, RTS) { row_copy(0xC2, 0xC28386); }
DC_ENTRY(C28D05, RTS)                            /* recompute entry 3 for each of the 7 records at 2600 */
{
    dc_set_mx(0, 0);
    g_cpu.Y = 0x2600;
    do {
        dc_jsr(0xC28D1C, 0xC28D0A);
        g_cpu._flag_C = 0;
        g_cpu.Y = dc_adc16(g_cpu.Y, 0x0050);
        dc_cmp16(g_cpu.Y, 0x2830);
    } while (!g_cpu._flag_C);
    lda16(0);
}
DC_ENTRY(C2A101, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.A = (u16)((g_cpu.D & 0xFF00) | 0x0F);
    g_cpu.X = 0xCBC4; g_cpu.Y = 0x9560;
    dc_mvn(0x7E, 0xFF);
    dc_jsr(0xC29121, 0xC2A110);
    dc_set_a8(0x64);
    abs8_set(0x0D13, 0x64);
    plp();
}
DC_ENTRY(C2F08D, RTS)                            /* n blank tiles (n = byte at Y) */
{
    u16 n = abs16(g_cpu.Y);
    g_cpu.Y++;
    dc_push16(g_cpu.Y);
    g_cpu.A = n & 0xFF;
    g_cpu.Y = g_cpu.A;
    dc_set_mx(1, 0);
    do {
        dc_set_a8(0xFF);
        dc_jsr(0xC2EF97, 0xC2F09A);
        g_cpu.Y--;
    } while (g_cpu.Y);
    ldy16(dc_pull16());
}
DC_ENTRY(C2BAFC, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    dc_jsr(0xC2F626, 0xC2BAFF);
    abs8_set(0x1800 + g_cpu.Y, (u8)g_cpu.A);
    abs8_set(0x1818 + g_cpu.Y, 0);
    abs8_set(0x180F + g_cpu.Y, 0x1C);
    g_cpu.X = dp16(0x22);
    dc_set_a8(bus8(0xC2BB1A + g_cpu.X));
    abs8_set(0x180E + g_cpu.Y, (u8)g_cpu.A);
    plp();
}
/* Script ops: request a sound/latch (1BE8 = kind, then two script bytes or FF FF). */
static void latch_req(u8 kind, int one_byte, u32 site)
{
    dc_set_mx(1, 0);
    abs8_set(0x1BE8, kind);
    g_cpu.Y = 1;
    u8 a = bus8(lp(0x58) + 1);
    abs8_set(0x1BE9, a);
    if (one_byte) {
        abs8_set(0x02AE, a);
        abs8_set(0x1BEA, 0xFF);
        abs8_set(0x1BEB, 0xFF);
        dc_set_a8(0xFF);
    } else {
        g_cpu.Y = 2;
        a = bus8(lp(0x58) + 2);
        dc_set_a8(a);
        abs8_set(0x1BEA, a);
    }
    abs8_set(0x1BED, 0);
    dc_jsr(0xC22ED9, site);
    dc_set_mx(0, 0);
    lda16(one_byte ? 2 : 3);
}
DC_ENTRY(C218D0, RTS) { latch_req(0x19, 0, 0xC218E8); }
DC_ENTRY(C218C7, RTS) { latch_req(0x18, 0, 0xC218E8); }
DC_ENTRY(C218F9, RTS) { latch_req(0x10, 1, 0xC21916); }
DC_ENTRY(C2615E, RTS)                            /* split $68 into hundreds / tens / ones digits */
{
    dc_set_mx(1, 0);
    dc_set_a8(dp8(0x68));
    g_cpu.Y = 2;
    g_cpu.X = 0x00C8;
    dp16_set(0x6A, 0x00C8);
    dc_jsr(0xC2614B, 0xC2616A);
    g_cpu.X = dp16(0x65);
    dp16_set(0x60, g_cpu.X);
    g_cpu.X = 0x0050;
    dp16_set(0x6A, 0x0050);
    dc_jsr(0xC2614B, 0xC26176);
    g_cpu.X = dp16(0x65);
    dp16_set(0x61, g_cpu.X);
    dp8_set(0x62, (u8)g_cpu.A);
}
DC_ENTRY(C28CE5, RTS)                            /* put character $C0 in the first free reserve slot */
{
    dc_jsr(0xC28CCF, 0xC28CE5);
    if (g_cpu._flag_Z) return;
    for (g_cpu.Y = 3; g_cpu.Y < 9; g_cpu.Y++, g_cpu._flag_C = g_cpu.Y >= 9) {
        u8 a = abs8(0x2980 + g_cpu.Y);
        lda8(a);
        if (a & 0x80) {
            g_cpu.X = dp8(0xC0);
            u8 b = bus8(0xFFF9BB + g_cpu.X), m = abs8(0x29AF);
            g_cpu._flag_Z = (m & b) == 0;
            abs8_set(0x29AF, m | b);
            lda8(dp8(0xC0));
            abs8_set(0x2980 + g_cpu.Y, (u8)g_cpu.A);
            return;
        }
    }
    dc_cmp8(9, 9);
}

/* ---- batch 7 ------------------------------------------------------------- */
static u16 dpx_x(u8 o) { return (u16)(g_cpu.D + o + g_cpu.X); }

DC_ENTRY(C218F1, RTS)                            /* like C2:18F9 unless 7F:01ED is set */
{
    dc_set_mx(1, 0);
    u8 a = bus8(0x7F01ED);
    if (a) { dc_set_a8(a); dc_set_mx(0, 0); lda16(2); return; }
    latch_req(0x10, 1, 0xC21916);
}
DC_ENTRY(C288AD, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    dc_jsr(0xC28881, 0xC288B0);
    bus8_set(0x004202, abs8(0x04CE));
    g_cpu.X = abs16(0x04CC);
    bus8_set(0x004203, bus8(0xFFCF18 + g_cpu.X));
    dc_set_mx(0, 0);
    g_cpu.X = asl16(g_cpu.X);
    g_cpu.A = dc_adc16(bus16(0xFFCF0E + g_cpu.X), bus16(0x004216));
    abs16_set(0x0D28, g_cpu.A);
    g_cpu.X = g_cpu.A;
    plp();
}
DC_ENTRY(C2CE49, RTS)                            /* place the portrait sprite of party entry $00 */
{
    php();
    g_cpu.A = g_cpu.D;
    g_cpu.X = dp16(0x00);
    u8 a = abs8(0x0F02 + g_cpu.X);
    dc_set_a8(a);
    if (a & 0x80) { dp8_set(0x00, (u8)(dp8(0x00) + 1)); plp(); return; }
    dc_set_a8(a & 0xBF);
    g_cpu.Y = g_cpu.A;
    dc_set_a8(abs8(0x2980 + g_cpu.Y));
    dc_jsr(0xC2F626, 0xC2CE58);
    abs8_set(0x1800 + g_cpu.Y, (u8)g_cpu.A);
    g_cpu.X = dp16(0x02);
    dc_set_mx(0, 0);
    abs16_set(0x180E + g_cpu.Y, bus16(0xC2CE6E + g_cpu.X));
    g_cpu.A = g_cpu.D;
    dp16_set(0x00, (u16)(dp16(0x00) + 1));
    plp();
}
DC_ENTRY(C2EDD3, RTS)
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    u8 a = abs8(0x005B);
    if (!(a & 0x80)) a = abs8(0x0D8C) & 7;
    a = asl8(asl8(a));
    dp8_set(0x00, a);
    a = asl8(asl8(a));
    a = dc_adc8(a, dp8(0x00));
    abs8_set(0x0D47, a);
    a = abs8(0x005C);
    dc_set_a8(a);
    abs8_set(0x0D48, a);
    dc_jsr(0xC2ED90, 0xC2EDF1);
    plp();
}
DC_ENTRY(C2EF05, RTS)                            /* copy a window-sized block between two tile positions */
{
    php();
    dc_set_mx(0, 0);
    dp16_set(0x8C, dp16(0x60) & 0xFF);
    dp16_set(0x8A, (u16)(((dp16(0x5F) & 0xFF) << 1) - 1));
    dc_jsr(0xC2ED90, 0xC2EF18);
    dp16_set(0x65, dp16(0x63));
    dp16_set(0x5D, dp16(0x5B));
    dc_jsr(0xC2ED90, 0xC2EF23);
    dc_jsr(0xC2EF2B, 0xC2EF26);
    plp();
}
DC_ENTRY(C2F24C, RTS)                            /* print the hex digits of $8A.. ($8E digits) */
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.A = g_cpu.D;
    u8 n = dp8(0x8E);
    int odd = n & 1;
    dc_set_a8(n >> 1);
    g_cpu.X = g_cpu.A;
    for (;;) {
        if (odd) {
            dc_set_a8(b0_8(dpx_x(0x8A)) >> 4);
            dc_jsr(0xC2F269, 0xC2F25C);
        }
        odd = 1;
        dc_set_a8(b0_8(dpx_x(0x8A)));
        dc_jsr(0xC2F269, 0xC2F261);
        g_cpu.X--;
        if (g_cpu.X & 0x8000) break;
    }
    plp();
}
DC_ENTRY(C28CA2, RTS)                            /* move character $C0 from the party to the reserve */
{
    dc_jsr(0xC28C75, 0xC28CA2);
    if (!g_cpu._flag_Z) return;
    while (g_cpu.Y != 2) {
        abs8_set(0x2980 + g_cpu.Y, abs8(0x2981 + g_cpu.Y));
        g_cpu.Y = (u8)(g_cpu.Y + 1);
    }
    abs8_set(0x2980 + g_cpu.Y, abs8(0x2980 + g_cpu.Y) | 0x80);
    u8 a = dp8(0xC0);
    for (g_cpu.Y = 3; ; ) {
        g_cpu.X = abs8(0x2980 + g_cpu.Y);
        abs8_set(0x2980 + g_cpu.Y, a);
        a = (u8)g_cpu.X;
        lda8(a);
        if (a & 0x80) return;
        g_cpu.Y++;
        dc_cmp8((u8)g_cpu.Y, 9);
        if (g_cpu._flag_C) return;
    }
}
DC_ENTRY(C2C949, RTS)                            /* PHX ... PLX at the entry X width (entered as M0X0, M1X0, M1X1) */
{
    if (g_cpu.x_flag) dc_push8((u8)g_cpu.X); else dc_push16(g_cpu.X);   /* PHX at the entry width */
    php();
    dc_set_mx(1, 1);
    g_cpu.X = abs8(0x0F0C);
    do {
        g_cpu.Y = abs8(0x0F04 + g_cpu.X);
        dc_set_a8(abs8(0x2980 + g_cpu.Y));
        dc_set_mx(1, 0);
        dc_jsr(0xC2F626, 0xC2C959);
        g_cpu._flag_C = 1;
        abs8_set(0x180E + g_cpu.Y, dc_sbc8(abs8(0x180E + g_cpu.Y), abs8(0x0D22)));
        dc_set_a8(0);
        abs8_set(0x1811 + g_cpu.Y, 0);
        dc_set_mx(1, 1);
        g_cpu.X = (u8)(g_cpu.X - 1);
    } while (g_cpu.X);
    plp();
    if (g_cpu.x_flag) { u8 v = dc_pull8(); g_cpu.X = v; dc_set_flags_nz8(v); }
    else ldx16(dc_pull16());
}
DC_ENTRY(C287D5, RTS)                            /* use one of item 04C9 (count 04CB): A = 0 ok, FE too few, FF none */
{
    dc_push16(g_cpu.X);
    php();
    dc_set_mx(1, 1);
    u8 a = abs8(0x04C9);
    dc_set_a8(a);
    if (a) {
        dc_jsr(0xC287FA, 0xC287DE);
        dc_set_a8(0xFF);
        if (!g_cpu._flag_C) {
            g_cpu._flag_C = 1;
            a = dc_sbc8(abs8(0x2500 + g_cpu.X), abs8(0x04CB));
            if (!g_cpu._flag_C) dc_set_a8(0xFE);
            else {
                if (!a) abs8_set(0x2400 + g_cpu.X, 0);
                abs8_set(0x2500 + g_cpu.X, a);
                dc_set_a8(0);
            }
        }
    }
    plp();
    g_cpu.X = dc_pull16();
}
DC_ENTRY(C25470, RTS)                            /* object animation script from the C2:548C table */
{
    g_cpu.Y = dp16(0x4E);
    u8 a = abs8(0x0028 + g_cpu.Y);
    dc_set_a8(a);
    dc_cmp8(a, abs8(0x0027 + g_cpu.Y));
    if (g_cpu._flag_Z) return;
    a = asl8(asl8((u8)(abs8(0x0024 + g_cpu.Y) - 1)));
    dc_set_a8(dc_adc8(a, abs8(0x0028 + g_cpu.Y)));
    g_cpu.X = g_cpu.A;
    dc_set_a8(bus8(0xC2548C + g_cpu.X));
    dc_C21CDA_glue();
}
DC_ENTRY(C2E545, RTS)                            /* lay out the name-entry character grid at 9890 */
{
    dc_set_mx(1, 0);
    g_cpu.X = dp16(0x51);
    g_cpu.Y = g_cpu.X;
    dp8_set(0x01, 8);
    do {
        dp8_set(0x00, 10);
        do {
            abs8_set(0x9890 + g_cpu.Y, bus8(0xFFC9AC + g_cpu.X));
            g_cpu.X++; g_cpu.Y++;
            abs8_set(0x9890 + g_cpu.Y, 0xFF);
            g_cpu.Y++;
            dp8_set(0x00, (u8)(dp8(0x00) - 1));
        } while (dp8(0x00));
        abs8_set(0x9890 + g_cpu.Y, 0x01);
        g_cpu.Y++;
        dp8_set(0x01, (u8)(dp8(0x01) - 1));
    } while (dp8(0x01));
    dc_set_a8(0);
    abs8_set(0x9890 + g_cpu.Y, 0);
    ldy16((u16)(g_cpu.Y + 1));
}
DC_ENTRY(C20AAE, RTS)                            /* 16x16 map tile (4 words from table $14) into the tilemap ($E0) */
{
    g_cpu.Y = dp16(0x04);
    u16 a = bus16(dba(dp16(0x18)) + g_cpu.Y) & 0xFF;
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16((u16)(a << 3), dp16(0x14));
    u32 to = dba(dp16(0xE0));
    g_cpu.Y = dp16(0x06);
    bus16_set(to + g_cpu.Y, abs16(g_cpu.X));
    g_cpu.Y += 2;
    bus16_set(to + g_cpu.Y, abs16(0x0004 + g_cpu.X));
    g_cpu._flag_C = 0;
    g_cpu.Y = dc_adc16(g_cpu.Y, 0x003E);
    bus16_set(to + g_cpu.Y, abs16(0x0002 + g_cpu.X));
    g_cpu.Y += 2;
    lda16(abs16(0x0006 + g_cpu.X));
    bus16_set(to + g_cpu.Y, g_cpu.A);
}
DC_ENTRY(C26231, RTS)                            /* 24-bit $68 / $6C, Y quotient bits into $6B */
{
    dp8_set(0x6B, 0);
    do {
        g_cpu._flag_C = 1;
        u8 t = dc_sbc8(dp8(0x68), dp8(0x6C));
        t = dc_sbc8(dp8(0x69), dp8(0x6D));
        t = dc_sbc8(dp8(0x6A), dp8(0x6E));
        dc_set_a8(t);
        if (g_cpu._flag_C) {
            dp8_set(0x68, dc_sbc8(dp8(0x68), dp8(0x6C)));
            dp8_set(0x69, dc_sbc8(dp8(0x69), dp8(0x6D)));
            dp8_set(0x6A, dc_sbc8(dp8(0x6A), dp8(0x6E)));
        }
        u8 c = g_cpu._flag_C;
        dp8_set(0x6B, (u8)(dp8(0x6B) << 1 | c));
        u8 e = dp8(0x6E), d = dp8(0x6D), l = dp8(0x6C);
        dp8_set(0x6E, e >> 1);
        dp8_set(0x6D, (u8)(d >> 1 | (e & 1) << 7));
        dp8_set(0x6C, (u8)(l >> 1 | (d & 1) << 7));
        g_cpu._flag_C = l & 1;
        g_cpu.Y--;
    } while (g_cpu.Y);
    ldy16(4);
}
DC_ENTRY(C20F00, RTS)                            /* script op: add an 8-byte entry to the list at $60+$5F */
{
    dc_set_mx(1, 1);
    g_cpu.X = dp8(0x5F);
    dp8_set(0x5E, (u8)(dp8(0x5E) + 1));
    g_cpu.Y = 1;
    abs8_set(0x0062 + g_cpu.X, bus8(lp(0x52) + 1));
    b0_8_set(dpx_x(0x67), 0x80);
    dc_set_mx(0, 1);
    for (u8 y = 2, o = 0x60; y <= 6; y += 2, o = o == 0x60 ? 0x63 : 0x65) {
        g_cpu.Y = y;
        g_cpu.A = bus16(lp(0x52) + y);
        b0_8_set(dpx_x(o), (u8)g_cpu.A);
        b0_8_set((u16)(dpx_x(o) + 1), (u8)(g_cpu.A >> 8));
    }
    dc_set_mx(1, 1);
    dc_set_a8((u8)g_cpu.X);
    g_cpu._flag_C = 0;
    dp8_set(0x5F, dc_adc8((u8)g_cpu.X, 8));
    dp8_set(0x5E, 0);
    dc_set_mx(0, 0);
    lda16(8);
}
DC_ENTRY(C23FF0, RTS)                            /* facing from the larger of |dx| ($2E) and |dy| ($30) */
{
    dc_set_mx(0, 0);
    ldx16(dp16(0x4E));
    u16 dx = abs16(0x002E + g_cpu.X), dy, v;
    if (!dx) v = 0x21;
    else {
        if (dx & 0x8000) dx = (u16)(~dx + 1);
        dp16_set(0x08, dx);
        dy = abs16(0x0030 + g_cpu.X);
        if (!dy) v = 0x11;
        else {
            if (dy & 0x8000) dy = (u16)(~dy + 1);
            dc_cmp16(dy, dp16(0x08));
            v = g_cpu._flag_C ? 0x22 : 0x12;
        }
    }
    lda16(v);
    abs8_set(0x0029 + g_cpu.X, (u8)v);
}
DC_ENTRY(C240E4, RTS)                            /* direction bits from the velocity signs */
{
    dc_set_mx(1, 0);
    u8 d = abs8(0x0028 + g_cpu.X);
    abs8_set(0x0027 + g_cpu.X, d & 0x7F);
    abs8_set(0x0028 + g_cpu.X, abs8(0x0028 + g_cpu.X) & 0x80);
    dc_set_mx(0, 0);
    u16 v, s = abs16(0x001C + g_cpu.X);
    if (s) v = (s & 0x8000) ? 2 : 3;
    else {
        s = abs16(0x0020 + g_cpu.X);
        if (!s) for (;;) dc_spend(6);             /* the original hangs here (BRA to itself) */
        v = (s & 0x8000) ? 0 : 1;
    }
    g_cpu.A = v;
    dc_set_mx(1, 0);
    lda8((u8)v | abs8(0x0028 + g_cpu.X));
    abs8_set(0x0028 + g_cpu.X, (u8)g_cpu.A);
    dc_set_mx(0, 0);
}
DC_ENTRY(C25D91, RTS)                            /* drop leading zero digits of 40-44; A = digits */
{
    g_cpu.Y = 5;
    if (!dp8(0x40)) {
        g_cpu.X = dp16(0x41); dp16_set(0x40, g_cpu.X);
        g_cpu.X = dp16(0x43); dp16_set(0x42, g_cpu.X);
        g_cpu.Y = 4;
        if (!dp8(0x40)) {
            dp8_set(0x40, dp8(0x41));
            g_cpu.X = dp16(0x42); dp16_set(0x41, g_cpu.X);
            g_cpu.Y = 3;
            if (!dp8(0x40)) {
                g_cpu.X = dp16(0x41); dp16_set(0x40, g_cpu.X);
                g_cpu.Y = 2;
                if (!dp8(0x40)) { dp8_set(0x40, dp8(0x41)); g_cpu.Y = 1; }
            }
        }
    }
    lda8((u8)g_cpu.Y);
}
DC_ENTRY(C26180, RTS)                            /* split the 16-bit $68 into five decimal digits */
{
    dc_set_mx(0, 0);
    g_cpu.A = dp16(0x68);
    g_cpu.Y = 3;
    static const u16 div[4] = { 0x9C40, 0x1F40, 0x0320, 0x0050 };
    static const u32 site[4] = { 0xC2618C, 0xC26198, 0xC261A4, 0xC261B0 };
    for (int k = 0; k < 4; k++) {
        g_cpu.X = div[k];
        dp16_set(0x6A, div[k]);
        dc_jsr(0xC2614B, site[k]);
        g_cpu.X = dp16(0x65);
        dp16_set((u8)(0x60 + k), g_cpu.X);
    }
    dp16_set(0x64, g_cpu.A);
    lda16(g_cpu.D);
    dc_set_mx(1, 0);
}
DC_ENTRY(C262ED, RTS)                            /* nibble ($00) of row $01 (48 bytes) at 7E:7200 */
{
    dc_set_mx(1, 0);
    dp8_set(0x12, 0x7E);
    abs8_set(0x4202, dp8(0x01));
    dc_set_a8(0x30);
    abs8_set(0x4203, 0x30);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(0x7200, abs16(0x4216));
    dp16_set(0x10, g_cpu.A);
    g_cpu.Y = lsr16(dp16(0x00) & 0xFF);
    g_cpu.A = g_cpu.Y;
    dc_set_mx(1, 0);
    u8 a = bus8(lp(0x10) + g_cpu.Y), m = dp8(0x00);
    dp8_set(0x00, m >> 1);
    g_cpu._flag_C = m & 1;
    if (m & 1) lda8(a & 0x0F);
    else { g_cpu._flag_C = (a >> 3) & 1; lda8(a >> 4); }
}

/* ---- batch 8 ------------------------------------------------------------- */
static u16 xba16(u16 a) { a = (u16)(a << 8 | a >> 8); dc_set_flags_nz8((u8)a); return a; }
static u16 dpx16(u8 o) { u16 at = dpx_x(o); return (u16)(b0_8(at) | b0_8((u16)(at + 1)) << 8); }

/* Map column setup: tile row/column of the scroll position for the layer $0E. */
static void map_setup(int col)
{
    u16 a;
    if (col) dp16_set(0x04, dp16(0x08));
    a = lsr16(lsr16(xba16(dp16(0x0A))));
    dp16_set(0x19, a);
    a = dc_adc16(lsr16(a), dp16(0x19));
    if (col) { g_cpu._flag_C = 0; dp16_set(0x02, dc_adc16(a, dp16(0x13))); }
    else { dp16_set(0x04, a); g_cpu._flag_C = 0; dp16_set(0x02, dc_adc16(dp16(0x13), dp16(0x08))); }
    dp16_set(0x06, abs16(0x00E0));
    ldx16(dp16(0x0E));
    dp16_set(0x08, 0);
    lsr16(abs16((col ? 0x00E3 : 0x00E7) + g_cpu.X));
    if (!g_cpu._flag_C) dp16_set(0x08, 1);
    g_cpu._flag_C = 1;
    a = dc_sbc16(dpx16(col ? 0x20 : 0x26), 0x0008);
    a = lsr16(lsr16(a & (col ? 0x01F8 : 0x00F8)));
    g_cpu.A = a;
    dp16_set(0x0A, a);
}
DC_ENTRY(C20811, RTS) { map_setup(0); }
DC_ENTRY(C208DB, RTS) { map_setup(1); }
DC_ENTRY(C2F1DA, RTS)                            /* print the decimal digits of $8A.. ($8E digits) at Y */
{
    dc_set_mx(1, 0);
    dc_push16(0x7E7E);
    g_cpu.DB = dc_pull8(); g_cpu.DB = dc_pull8();
    g_cpu.Y = dp16(0x90);
    u8 n = dp8(0x8E);
    g_cpu.A = n >> 1;
    g_cpu.X = g_cpu.A;
    int odd = n & 1;
    for (;;) {
        if (odd) {
            dc_set_a8(b0_8(dpx_x(0x8A)) >> 4);
            dc_jsr(0xC2F20B, 0xC2F1F2);
        }
        odd = 1;
        if (g_cpu.X == 0 && !(dp8(0x8F) & 0x08)) dp8_set(0x8F, 0);
        dc_set_a8(b0_8(dpx_x(0x8A)));
        dc_jsr(0xC2F20B, 0xC2F204);
        g_cpu.X--;
        dc_set_flags_nz16(g_cpu.X);
        if (g_cpu.X & 0x8000) break;
    }
}
DC_ENTRY(C2F5ED, RTS)                            /* X = sprite data offset of character record A (0x50 bytes) */
{
    php();
    dc_set_mx(1, g_cpu.x_flag);
    u8 a = (u8)g_cpu.A;
    dc_push8(a);
    bus8_set(0x004202, 0x50);
    dc_pull8();
    bus8_set(0x004203, a);
    dc_set_mx(0, 0);
    if (!(a & 0x80)) {
        g_cpu.X = bus16(0x004216);
        u16 v = abs16(0x2629 + g_cpu.X) & 0xFF;
        abs16_set(0x041B, v);
        g_cpu.X = v;
        u16 t = asl16(bus16(0xCD6CEC + g_cpu.X) & 0xFF);
        dc_push16(t);
        u16 u = asl16(t);
        u = dc_adc16(u, t);
        g_cpu.X = dc_adc16(u, 0x4B00);
        dc_pull16();
    }
    g_cpu.A = 5;
    plp();
}
DC_ENTRY(C23987, RTS)                            /* object script for facing (28 & 3), +12 in mode 6 */
{
    dc_set_mx(1, 0);
    g_cpu.X = dp16(0x4E);
    g_cpu.A = g_cpu.D;
    dc_set_a8(abs8(0x0028 + g_cpu.X) & 3);
    g_cpu.X = g_cpu.A;
    dp8_set(0x01, bus8(0xC239AA + g_cpu.X));
    u8 m = dp8(0x00);
    dc_set_a8(m);
    dc_cmp8(m, 6);
    if (m == 6) { g_cpu._flag_C = 0; dp8_set(0x01, dc_adc8(dp8(0x01), 0x0C)); }
    dc_set_a8(dp8(0x01));
    dc_C21CDA_glue();
}
DC_ENTRY(C282E1, RTS)                            /* mark owned items AE..B2 in 27A9 for each record */
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x2857, 0);
    g_cpu.X = 0x2857; g_cpu.Y = 0x2859; g_cpu.A = 0x0002;
    dc_mvn(0x7E, 0x7E);
    g_cpu.X = 0x2600;
    do {
        g_cpu.A = 0;
        dc_set_mx(1, 0);
        u8 a = abs8(0x002A + g_cpu.X);
        dc_set_a8(a);
        if (a >= 0xAE && a < 0xB3) {
            g_cpu.Y = a;
            if (abs8(0x03F6 + g_cpu.Y)) { dc_set_a8(0x80); abs8_set(0x27A9 + g_cpu.Y, 0x80); }
        }
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0050);
        g_cpu.X = g_cpu.A;
        dc_cmp16(g_cpu.A, 0x2830);
    } while (!g_cpu._flag_C);
    plp();
}
DC_ENTRY(C28839, RTS)                            /* load character record A into the work copy at 9A90 */
{
    u8 a = (u8)g_cpu.A;
    bus8_set(0x004202, a);
    dc_set_flags_nz8(a);
    if (a & 0x80) return;
    dc_set_a8(0x50);
    bus8_set(0x004203, 0x50);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(0x2600, bus16(0x004216));
    dp16_set(0x6F, g_cpu.A);
    g_cpu.X = g_cpu.A; g_cpu.Y = 0x9A90; g_cpu.A = 0x004F;
    dc_mvn(0x7E, 0x7E);
    dc_set_mx(1, 0);
    bus8_set(0x004202, abs8(0x9A90) & 7);
    dc_set_a8(6);
    bus8_set(0x004203, 6);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16(0x2C23, bus16(0x004216));
    g_cpu.A = 0x0005;
    dc_mvn(0x7E, 0x7E);
    lda16((u16)(g_cpu.A + 1));
}
/* Stat bonus: for each set bit of the CC:29D7 entry, add $01 to the stat it names. */
static void stat_bonus(void)
{
    php();
    dc_set_mx(0, 1);
    u16 a = abs16(0x7B00 + g_cpu.X) & 0xFF;
    if (a) {
        a = asl16(a);
        g_cpu.X = a & 0xFF;
        dp16_set(0x00, bus16(0xCC29D7 + g_cpu.X) & 0xFFFE);
        dc_set_mx(1, 1);
        g_cpu.X = 0;
        do {
            u8 m = dp8(0x00);
            dp8_set(0x00, (u8)(m << 1));
            if (m & 0x80) {
                g_cpu.Y = bus8(0xFFCF2C + g_cpu.X);
                g_cpu._flag_C = 0;
                abs8_set(0x0DAE + g_cpu.Y, dc_adc8(dp8(0x01), abs8(0x0DAE + g_cpu.Y)));
            }
            g_cpu.X = (u8)(g_cpu.X + 1);
        } while (dp8(0x00));
    }
    plp();
}
DC_ENTRY(C29276, RTS)
{
    g_cpu.X = abs8(0x9AB9);
    u8 a = abs8(0x7C00 + g_cpu.X);
    dc_set_a8(a);
    abs8_set(0x0DB5, a);
    stat_bonus();
}
static void stat_add(u16 idx)
{
    g_cpu.X = abs8(idx);
    g_cpu._flag_C = 0;
    u8 a = dc_adc8(abs8(0x7C00 + g_cpu.X), abs8(0x0DB6));
    dc_set_a8(a);
    abs8_set(0x0DB6, a);
    stat_bonus();
}
DC_ENTRY(C29281, RTS) { stat_add(0x9AB8); }
DC_ENTRY(C29286, RTS) { stat_add(0x9AB7); }
DC_ENTRY(C295BB, RTS)                            /* new game: clear the work RAM and set defaults */
{
    php();
    dc_set_mx(0, 0);
    dc_push8(g_cpu.DB);
    bus16_set(0x7F0000, 0);
    g_cpu.X = 0; g_cpu.Y = 2; g_cpu.A = 0x01FD;
    dc_mvn(0x7F, 0x7F);
    plb();
    abs16_set(0x0400, 0);
    g_cpu.X = 0x0400; g_cpu.Y = 0x0402; g_cpu.A = 0x00FD;
    dc_mvn(0x7E, 0x7E);
    abs16_set(0x2990, 0);
    g_cpu.X = 0x2990; g_cpu.Y = 0x2992; g_cpu.A = 0x001D;
    dc_mvn(0x7E, 0x7E);
    g_cpu.X = 0xFCA6; g_cpu.Y = 0x2990; g_cpu.A = 0x000B;
    dc_mvn(0x7E, 0xC2);
    g_cpu.X = 0xFCA9; g_cpu.Y = 0x0408; g_cpu.A = 0x0008;
    dc_mvn(0x7E, 0xC2);
    plp();
}
DC_ENTRY(C2CFFB, RTS)
{
    php();
    dc_set_mx(0, 0);
    g_cpu.X = 0xD044; g_cpu.Y = 0x9720; g_cpu.A = 0x0020;
    dc_mvn(0x7E, 0xC2);
    g_cpu.X = 0x9710; g_cpu.Y = 0x969A; g_cpu.A = 0x000F;
    dc_mvn(0x7E, 0x7E);
    abs16_set(0x9694, 0x969A);
    abs16_set(0x9696, 0x9300);
    abs16_set(0x9698, 0x0010);
    dc_set_mx(1, 1);
    u8 a = asl8(dp8(0x7F));
    g_cpu.X = dc_adc8(a, dp8(0x7F));
    abs8_set(0x969D + g_cpu.X, 0x20);
    abs8_set(0x0D13, abs8(0x0D13) | 0xC0);
    dc_set_mx(0, 1);
    g_cpu.A = 0x9740;
    abs16_set(0x969E + g_cpu.X, 0x9740);
    plp();
}
DC_ENTRY(C21037, RTS)                            /* script op: reset the menu object's fields */
{
    ldx16(dp16(0x4E));
    abs16_set(0x0005 + g_cpu.X, 0);
    abs16_set(0x000B + g_cpu.X, 0);
    abs16_set(0x000D + g_cpu.X, 0);
    for (u16 f = 0x0E; f <= 0x3E; f += 2) abs16_set(f + g_cpu.X, 0);
    lda16(1);
}
DC_ENTRY(C227DE, RTS)                            /* decompress the 7 graphics named at [$ED] to 7F:9000.. */
{
    abs16_set(0x0303, 0x9000);
    g_cpu.X = 0x9000;
    abs8_set(0x0305, 0x7F);
    dp8_set(0x08, 0); dp8_set(0x09, 0);
    do {
        dc_set_mx(1, 0);
        g_cpu.Y = dp16(0x08);
        g_cpu.A = g_cpu.D;
        u8 a = bus8(lp(0xED) + g_cpu.Y);
        lda8(a);
        if (!(a & 0x80)) {
            g_cpu._flag_C = 0;
            dc_set_a8(dc_adc8((u8)(a << 1), a));
            c6_request(0xC6FE20, 0xC2280C);
        }
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        abs16_set(0x0303, dc_adc16(abs16(0x0303), 0x1000));
        dp16_set(0x08, (u16)(dp16(0x08) + 1));
        g_cpu.A = dp16(0x08);
        dc_cmp16(g_cpu.A, 7);
    } while (!g_cpu._flag_Z);
}
DC_ENTRY(C2531D, RTS)                            /* object target = map position + step (direction 24, frame 0104) */
{
    g_cpu.Y = dp16(0x4E);
    u8 a = asl8(abs8(0x0104) & 3);
    dp8_set(0x00, a);
    a = asl8(asl8((u8)((abs8(0x0024 + g_cpu.Y) - 1) | dp8(0x00))));
    dc_set_a8(a);
    g_cpu.X = g_cpu.A;
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    u16 v = dc_adc16(bus16(0xC2543C + g_cpu.X), abs16(0x029A)) & 0xFFF8;
    dc_set_flags_nz16(v);
    if (v & 0x8000) { g_cpu._flag_C = 0; v = dc_adc16(v, 0x0600); }
    else { dc_cmp16(v, 0x0600); if (g_cpu._flag_C) v = dc_sbc16(v, 0x0600); }
    abs16_set(0x002A + g_cpu.Y, v);
    g_cpu._flag_C = 0;
    lda16(dc_adc16(bus16(0xC2543E + g_cpu.X), abs16(0x029C)) & 0x03F8);
    abs16_set(0x002C + g_cpu.Y, g_cpu.A);
    dc_set_mx(1, 0);
}
DC_ENTRY(C2EE23, RTS)                            /* a window frame row: left, middle (repeated), right */
{
    g_cpu.X = dp16(0x65);
    u16 a = dp16(0x08);
    abs16_set(g_cpu.X, a); a++;
    abs16_set(0x0002 + g_cpu.X, a); a++;
    abs16_set(0x0004 + g_cpu.X, a);
    g_cpu.X += 2;
    g_cpu.Y = (u16)(g_cpu.X + 4);
    a = asl16((u16)(dp16(0x8A) - 2));
    a--;
    g_cpu.A = a;
    if (!(a & 0x8000)) dc_mvn(0x7E, 0x7E);
    a = dp16(0x08) | 3;
    abs16_set(g_cpu.Y, a);
    dp16_set(0x08, (u16)(a + 1));
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(dp16(0x65), 0x0040);
    dp16_set(0x65, g_cpu.A);
}
DC_ENTRY(C2E92D, RTS)                            /* clear the menu OAM and place the cursor sprite */
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x0740, 0xE000);
    g_cpu.X = 0x0740; g_cpu.Y = 0x0742; g_cpu.A = 0x013D;
    dc_mvn(0x7E, 0x7E);
    abs16_set(0x0904, 0xAAAA);
    g_cpu.X = 0x0904; g_cpu.Y = 0x0906; g_cpu.A = 0x0011;
    dc_mvn(0x7E, 0x7E);
    dc_set_mx(1, 0);
    dc_set_a8(0xFF);
    u8 m = abs8(0x0D9B);
    abs8_set(0x0D9B, 0);
    if (!m) {
        dc_set_a8(dp8(0x54));
        dc_jsr(0xC2EA27, 0xC2E95F);
        dc_set_mx(0, 0);
        abs16_set(0x0770, abs16(0x9380 + g_cpu.X));
        g_cpu.A = dp16(0x58);
        abs16_set(0x0772, g_cpu.A);
        dc_set_mx(1, 0);
        dc_set_a8(dp8(0x56));
        if ((u8)g_cpu.A) { dc_set_a8(0x40); abs8_set(0x0773, abs8(0x0773) | 0x40); }
    }
    plp();
}
DC_ENTRY(C2116B, RTS)                            /* script op: store byte 4 into table (1) row (3) column (2) */
{
    g_cpu.Y = 1;
    g_cpu.X = asl16((u16)((bus16(lp(0x58) + 1) - 1) & 0xFF));
    dp16_set(0x10, bus16(0xC2055C + g_cpu.X));
    dc_set_mx(1, 0);
    dp8_set(0x12, 0x7E);
    g_cpu.Y = 3;
    abs8_set(0x4202, bus8(lp(0x58) + 3));
    abs8_set(0x4203, 0x60);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.Y = 2;
    u16 a = dc_adc16(bus16(lp(0x58) + 2) & 0xFF, abs16(0x4216));
    g_cpu._flag_C = 0;
    dp16_set(0x10, dc_adc16(a, dp16(0x10)));
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    g_cpu.Y = 4;
    u8 v = bus8(lp(0x58) + 4);
    dc_set_a8(v);
    bus8_set(lp(0x10), v);
    dc_set_mx(0, 0);
    lda16(5);
}

/* ---- batch 9 -------------------------------------------------------------- */
/* 16-bit ADC in decimal mode, as the interpreter does it (carry in C). */
static u16 adc16_bcd(u16 a, u16 b)
{
    int r = (a & 0xF) + (b & 0xF) + g_cpu._flag_C;
    if (r > 0x9) r = ((r + 0x6) & 0xF) + 0x10;
    r = (a & 0xF0) + (b & 0xF0) + r;
    if (r > 0x9F) r = ((r + 0x60) & 0xFF) + 0x100;
    r = (a & 0xF00) + (b & 0xF00) + r;
    if (r > 0x9FF) r = ((r + 0x600) & 0xFFF) + 0x1000;
    r = (a & 0xF000) + (b & 0xF000) + r;
    g_cpu._flag_V = ((a & 0x8000) == (b & 0x8000)) && ((b & 0x8000) != (r & 0x8000));
    if (r > 0x9FFF) r += 0x6000;
    g_cpu._flag_C = r > 0xFFFF;
    dc_set_flags_nz16((u16)r);
    return (u16)r;
}
DC_ENTRY(C28816, RTS)                            /* character record (8839) keeping X and the widths */
{
    dc_push16(g_cpu.X);
    php();
    dc_set_mx(1, 0);
    dc_jsr(0xC28839, 0xC2881A);
    plp();
    ldx16(dc_pull16());
}
DC_ENTRY(C28820, RTS)                            /* record of the party member at $71 ($78 = FF past $73) */
{
    dc_push16(g_cpu.X);
    dc_push16(g_cpu.Y);
    php();
    dc_set_mx(1, 1);
    dp8_set(0x78, 0);
    g_cpu.X = dp8(0x71);
    if (g_cpu.X >= dp8(0x73)) dp8_set(0x78, 0xFF);
    lda8(abs8(0x0D5F + g_cpu.X));
    dc_jsr(0xC28839, 0xC28832);
    plp();
    ldy16(dc_pull16());
    ldx16(dc_pull16());
}
DC_ENTRY(C2CAE6, RTS)                            /* menu state 01, then F643 */
{
    php();
    dc_set_mx(1, 0);
    lda8(0x01);
    abs8_set(0x0D13, 0x01);
    dc_jsr(0xC2F643, 0xC2CAEE);
    plp();
}
DC_ENTRY(C2C397, RTS)                            /* menu state 1C: clear the four scroll words, run script FBE3 */
{
    php();
    dc_set_mx(1, 0);
    dc_set_a8(0x1C);
    abs8_set(0x0D13, 0x1C);
    dc_set_mx(0, 0);
    abs16_set(0x3446, 0);
    abs16_set(0x3448, 0);
    abs16_set(0x3486, 0);
    abs16_set(0x3488, 0);
    ldx16(0xFBE3);
    dc_jsr(0xC28385, 0xC2C3B0);
    plp();
}
DC_ENTRY(C2EDF6, RTS)                            /* draw a window frame: top row, sides, bottom row */
{
    php();
    dc_set_mx(0, 0);
    dp16_set(0x65, dp16(0x63));
    u16 w = (u16)((dp16(0x5F) & 0x001F) - 1);
    dp16_set(0x06, w);
    dp16_set(0x8A, (u16)(w - 1));
    dp16_set(0x06, asl16(dp16(0x06)));
    dp16_set(0x8C, (u16)((dp16(0x60) & 0x003F) - 2));
    lda16(abs16(0x0D47));
    dp16_set(0x08, g_cpu.A);
    dc_jsr(0xC2EE23, 0xC2EE18);
    dc_jsr(0xC2EE58, 0xC2EE1B);
    dc_jsr(0xC2EE23, 0xC2EE1E);
    plp();
}
DC_ENTRY(C2EDB0, RTS)                            /* EDD3, then the window frame */
{
    dc_jsr(0xC2EDD3, 0xC2EDB0);
    dc_C2EDF6_glue();
}
DC_ENTRY(C2F227, RTS)                            /* F24C on the 4-byte header at A:Y, output at X */
{
    dc_push16(g_cpu.Y);
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(0, 0);
    dc_push16(g_cpu.A);
    plb(); plb();
    dp16_set(0x90, g_cpu.X);
    dp16_set(0x8E, g_cpu.A);
    dp16_set(0x8A, abs16(g_cpu.Y));
    dp16_set(0x8C, abs16((u16)(g_cpu.Y + 2)));
    dc_push16(0x7E7E);
    plb(); plb();
    g_cpu.Y = dp16(0x90);
    dc_jsr(0xC2F24C, 0xC2F244);
    ldx16(g_cpu.Y);
    plp();
    plb();
    ldy16(dc_pull16());
}
DC_ENTRY(C28C79, RTS)                            /* add character $C0 to the party (or swap into the reserve) */
{
    dc_jsr(0xC28CF7, 0xC28C79);
    for (g_cpu.Y = 0; ; ) {
        if (abs8(0x2980 + g_cpu.Y) & 0x80) {
            g_cpu.X = dp8(0xC0);
            u8 m = bus8(0xFFF9BB + g_cpu.X);
            dc_set_a8(m);
            u8 f = abs8(0x29AF);
            g_cpu._flag_Z = (f & m) == 0;
            abs8_set(0x29AF, f | m);
            lda8(dp8(0xC0));
            abs8_set(0x2980 + g_cpu.Y, (u8)g_cpu.A);
            return;
        }
        g_cpu.Y++;
        dc_cmp8((u8)g_cpu.Y, 3);
        if (g_cpu._flag_C) break;
    }
    g_cpu.X = abs8(0x2982);
    abs8_set(0x2982, dp8(0xC0));
    u8 a = (u8)g_cpu.X;
    for (g_cpu.Y = 3; ; ) {
        g_cpu.X = abs8(0x2980 + g_cpu.Y);
        abs8_set(0x2980 + g_cpu.Y, a);
        a = (u8)g_cpu.X;
        lda8(a);
        if (a & 0x80) return;
        g_cpu.Y++;
        dc_cmp8((u8)g_cpu.Y, 9);
        if (g_cpu.Y >= 9) return;
    }
}
DC_ENTRY(C2BC22, RTS)                            /* tile at 9890 from the CC:253B table */
{
    php();
    dc_set_mx(1, 0);
    if (!abs8(0x0418)) {
        g_cpu.A = g_cpu.D;
        dc_set_a8(abs8(0x0F4C));
        g_cpu.X = g_cpu.A;
    } else {
        dc_set_mx(0, 0);
        u16 a = lsr16(lsr16(abs16(0x9A8F) & 0x0F00));
        dc_push16(a);
        a = dc_adc16(abs16(0x0F4C) & 0x00FF, a);
        g_cpu.X = a;
        dc_pull16();
        g_cpu.X = abs16(0x1607 + g_cpu.X) & 0x00FF;
    }
    if (g_cpu.m_flag) lda8(bus8(0xCC253B + g_cpu.X)); else lda16(bus16(0xCC253B + g_cpu.X));
    dc_jsr(0xC2BC59, 0xC2BC51);
    if (g_cpu.m_flag) abs8_set(0x9890, (u8)g_cpu.A); else abs16_set(0x9890, g_cpu.A);
    plp();
}
DC_ENTRY(C2956E, RTS)                            /* new game: clear inventory, party, load defaults from CC:0000 */
{
    php();
    dc_set_mx(0, 0);
    dc_jsr(0xC295BB, 0xC29571);
    abs16_set(0x2400, 0);
    g_cpu.X = 0x2400; g_cpu.Y = 0x2402; g_cpu.A = 0x01FD;
    dc_mvn(0x7E, 0x7E);
    g_cpu.X = 0x0000; g_cpu.Y = 0x2600; g_cpu.A = 0x027F;
    dc_mvn(0x7E, 0xCC);
    abs16_set(0x2C53, 0);
    abs16_set(0x2C54, 0);
    abs16_set(0x2980, 0x0080);
    abs16_set(0x29AF, 0x0080);
    g_cpu.X = 0x2980; g_cpu.Y = 0x2981; g_cpu.A = 0x0007;
    dc_mvn(0x7E, 0x7E);
    abs16_set(0x2C7C, 0);
    g_cpu.X = 0x2C7C; g_cpu.Y = 0x2C7E; g_cpu.A = 0x001B;
    dc_mvn(0x7E, 0x7E);
    plp();
}
DC_ENTRY(C2834D, RTS)                            /* copy the three party members' records from D1 to 21BA */
{
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(1, 0);
    dc_push16(0x7E7E);
    plb(); plb();
    g_cpu.Y = 0x21BA;
    for (g_cpu.X = 0; g_cpu.X < 3; g_cpu.X++) {
        dc_set_mx(1, 0);
        lda8(abs8(0x2980 + g_cpu.X));
        if (g_cpu.A & 0x80) continue;
        dc_push16(g_cpu.X);
        dc_jsr(0xC2F5ED, 0xC28366);
        dc_mvn(0x7E, 0xD1);
        g_cpu.X = dc_pull16();
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        g_cpu.Y = dc_adc16(g_cpu.Y, 0x001A);
        g_cpu.A = g_cpu.Y;
    }
    dc_cmp16(g_cpu.X, 3);
    dc_jsl(0xFFF958, 0xC2837B);
    dc_jsr(0xC282E1, 0xC2837F);
    plp();
    plb();
}
DC_ENTRY(C28791, RTS)                            /* add $04CB of item $04C9 to the inventory (max 99); A = FF if full */
{
    dc_push16(g_cpu.X);
    php();
    dc_set_mx(1, 1);
    u8 it = abs8(0x04C9);
    dc_set_a8(it);
    if (it && it != 0x5A && it != 0x7B && it != 0x94) {
        dc_jsr(0xC287FA, 0xC287A6);
        bool add = true;
        if (g_cpu._flag_C) {
            while (abs8(0x2400 + g_cpu.X)) {
                g_cpu.X = (u8)(g_cpu.X + 1);
                if (!g_cpu.X) { dc_set_a8(0xFF); add = false; break; }
            }
            if (add) {
                abs8_set(0x2400 + g_cpu.X, abs8(0x04C9));
                abs8_set(0x2500 + g_cpu.X, 0);
            }
        }
        if (add) {
            g_cpu._flag_C = 0;
            u8 n = dc_adc8(abs8(0x2500 + g_cpu.X), abs8(0x04CB));
            if (n >= 0x63) n = 0x63;
            abs8_set(0x2500 + g_cpu.X, n);
            dc_set_a8(0);
        }
    }
    plp();
    ldx16(dc_pull16());
}
DC_ENTRY(C2F13F, RTS)                            /* BCD sum of the C2:F17A values for every set bit of $92 bytes at Y */
{
    php();
    dc_set_mx(0, 0);
    dp16_set(0x8A, 0);
    dp16_set(0x8C, 0);
    g_cpu.X = dp16(0x51);
    do {
        dc_push16(g_cpu.Y);
        lda16(abs16(g_cpu.Y) & 0x00FF);
        dp16_set(0x8E, g_cpu.A);
        for (g_cpu.Y = 8; g_cpu.Y; g_cpu.Y--) {
            dp16_set(0x8E, lsr16(dp16(0x8E)));
            if (g_cpu._flag_C) {
                g_cpu._flag_C = 0;
                g_cpu.A = adc16_bcd(bus16(0xC2F17A + g_cpu.X), dp16(0x8A));
                dp16_set(0x8A, g_cpu.A);
                g_cpu.A = adc16_bcd(bus16(0xC2F17C + g_cpu.X), dp16(0x8C));
                dp16_set(0x8C, g_cpu.A);
            }
            g_cpu.X += 4;
        }
        g_cpu.Y = (u16)(dc_pull16() + 1);
        dp16_set(0x92, (u16)(dp16(0x92) - 1));
    } while (dp16(0x92));
    plp();
}
DC_ENTRY(C23953, RTS)                            /* object animation from facing and frame (C2:397F; +0C for kind 6) */
{
    dc_set_mx(1, 0);
    g_cpu.X = dp16(0x4E);
    g_cpu.A = g_cpu.D;
    u8 a = asl8(abs8(0x0028 + g_cpu.X));
    dp8_set(0x01, a);
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8(abs8(0x0026 + g_cpu.X) & 1, dp8(0x01)));
    g_cpu.X = g_cpu.A;
    dp8_set(0x01, bus8(0xC2397F + g_cpu.X));
    dc_cmp8(dp8(0x00), 0x06);
    if (g_cpu._flag_Z) {
        g_cpu._flag_C = 0;
        dp8_set(0x01, dc_adc8(dp8(0x01), 0x0C));
    }
    lda8(dp8(0x01));
    dc_C21CDA_glue();
}
/* C = the point at 0283/0285 lies within r px of object X (box 18/14 - r .. + r+1). */
static void near_point(u16 r)
{
    g_cpu._flag_C = 1; dp16_set(0x00, dc_sbc16(abs16(0x0018 + g_cpu.X), r));
    g_cpu._flag_C = 0; dp16_set(0x02, dc_adc16(abs16(0x0018 + g_cpu.X), (u16)(r + 1)));
    g_cpu._flag_C = 1; dp16_set(0x04, dc_sbc16(abs16(0x0014 + g_cpu.X), r));
    g_cpu._flag_C = 0; dp16_set(0x06, dc_adc16(abs16(0x0014 + g_cpu.X), (u16)(r + 1)));
    dp16_set(0x08, abs16(0x0283));
    dp16_set(0x0A, abs16(0x0285));
    bool in = false;
    lda16(dp16(0x0A));
    dc_cmp16(g_cpu.A, dp16(0x00));
    if (g_cpu._flag_C) {
        dc_cmp16(g_cpu.A, dp16(0x02));
        if (!g_cpu._flag_C) {
            lda16(dp16(0x08));
            dc_cmp16(g_cpu.A, dp16(0x04));
            if (g_cpu._flag_C) {
                dc_cmp16(g_cpu.A, dp16(0x06));
                in = !g_cpu._flag_C;
            }
        }
    }
    g_cpu._flag_C = in;
}
DC_ENTRY(C23EE1, RTS) { near_point(0x10); }
DC_ENTRY(C23F27, RTS) { near_point(0x20); }
/* Text tile pair from C2:5D03 for one hex digit; a value >= 100 is two tiles. */
static void hex_tile(u16 i)
{
    g_cpu.X = i;
    u16 a = bus16(0xC25D03 + g_cpu.X);
    g_cpu.X = dp16(0x6A);
    dc_cmp16(a, 0x0100);
    u16 at = dpx_x(0x40);
    if (a >= 0x0100) {
        a = xba16(a);
        b0_8_set(at, (u8)a); b0_8_set((u16)(at + 1), (u8)(a >> 8));
        dp16_set(0x6A, (u16)(dp16(0x6A) + 2));
    } else {
        b0_8_set(at, (u8)a); b0_8_set((u16)(at + 1), (u8)(a >> 8));
        dp16_set(0x6A, (u16)(dp16(0x6A) + 1));
    }
    g_cpu.A = a;
}
DC_ENTRY(C25CC0, RTS)                            /* text: the byte at $60 as two hex digits */
{
    dc_set_mx(0, g_cpu.x_flag);
    hex_tile(lsr16(lsr16(lsr16(dp16(0x60) & 0x00F0))));
    hex_tile(asl16(dp16(0x60) & 0x000F));
    dc_set_flags_nz16(dp16(0x6A));
}
DC_ENTRY(C262AE, RTS)                            /* 4-bit cell (x $00, y $01) of a packed 2x2 map at [$10]/[$13] */
{
    dc_set_mx(1, 0);
    dp8_set(0x02, 0); dp8_set(0x03, 0); dp8_set(0x04, 0);
    u8 v = dp8(0x00); g_cpu._flag_C = v & 1; dp8_set(0x00, v >> 1);
    dp8_set(0x02, rol8(dp8(0x02)));
    v = dp8(0x01); g_cpu._flag_C = v & 1; dp8_set(0x01, v >> 1);
    dp8_set(0x03, rol8(dp8(0x03)));
    lda8(dp8(0x01));
    abs8_set(0x4202, (u8)g_cpu.A);
    dc_set_a8(0x60);
    abs8_set(0x4203, 0x60);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.Y = dc_adc16(dp16(0x00) & 0x00FF, abs16(0x4216));
    u16 a = asl16(bus16(lp(0x10) + g_cpu.Y) & 0x00FF);
    g_cpu.Y = dc_adc16(a, dp16(0x03));
    g_cpu.A = g_cpu.Y;
    dc_set_mx(1, 0);
    u8 c = bus8(lp(0x13) + g_cpu.Y);
    v = dp8(0x02); g_cpu._flag_C = v & 1; dp8_set(0x02, v >> 1);
    if (g_cpu._flag_C) { c &= 0x0F; dc_set_flags_nz8(c); }
    else { g_cpu._flag_C = (c >> 3) & 1; c >>= 4; dc_set_flags_nz8(c); }
    dc_set_a8(c);
}
DC_ENTRY(C2A518, RTS)                            /* scroll the two list columns by $26 and redraw */
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    u16 a = dc_adc16(dp16(0x22), dp16(0x26));
    dp16_set(0x22, a);
    abs16_set(0x5CC2, a);
    g_cpu.X = 0x5CC2; g_cpu.Y = 0x5CC4; g_cpu.A = 0x0015;
    dc_mvn(0x7E, 0x7E);
    g_cpu._flag_C = 0;
    a = dc_adc16(dp16(0x24), dp16(0x26));
    dp16_set(0x24, a);
    abs16_set(0x5D42, a);
    g_cpu.X = 0x5D42; g_cpu.Y = 0x5D44; g_cpu.A = 0x0035;
    dc_mvn(0x7E, 0x7E);
    ldx16(0xFC45);
    dc_jsr(0xC28385, 0xC2A549);
    dc_set_mx(1, 0);
    g_cpu._flag_C = 1;
    abs8_set(0x96A0, dc_sbc8(abs8(0x96A0), dp8(0x26)));
    g_cpu._flag_C = 0;
    abs8_set(0x96A3, dc_adc8(abs8(0x96A3), dp8(0x26)));
    lda8(0xE5);
    abs8_set(0x0D13, 0xE5);
    abs8_set(0x0D9A, 0xE5);
}
DC_ENTRY(C2CDE3, RTS)                            /* draw the party rows (CE49) and, outside battle, the reserve */
{
    php();
    dc_set_mx(0, 0);
    u16 a = abs16(0x0D36) & 0x00FF;
    if (a) a = 0x0012;
    dp16_set(0x02, a);
    dp16_set(0x00, 0);
    dc_set_mx(1, 0);
    dc_jsr(0xC2CE2A, 0xC2CDF7);
    do {
        dc_jsr(0xC2CE49, 0xC2CDFA);
        if (!(u8)g_cpu.A) dp8_set(0x02, (u8)(dp8(0x02) + 2));
        dc_set_a8(dp8(0x00));
    } while (dp8(0x00) < 3);
    dc_set_a8(abs8(0x0D36));
    if (!(u8)g_cpu.A) {
        dp8_set(0x02, 0x06);
        g_cpu._flag_C = 0;
        dp8_set(0x00, dc_adc8(dp8(0x80), 0x03));
        do {
            dc_jsr(0xC2CE49, 0xC2CE1B);
            dp8_set(0x02, (u8)(dp8(0x02) + 2));
            dc_set_a8(dp8(0x02));
        } while (dp8(0x02) < 0x0E);
    }
    plp();
}
DC_ENTRY(C2048A, RTS)                            /* new menu object: first free 40-byte slot at 0C30, copy of [$4E]+5 */
{
    dp16_set(0x08, g_cpu.X);
    dc_set_mx(0, 0);
    g_cpu.X = 0x0C30;
    for (g_cpu.Y = 0x3C; ; ) {
        if (!abs16(g_cpu.X)) break;
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0040);
        g_cpu.X = g_cpu.A;
        g_cpu.Y--;
        dc_set_flags_nz16(g_cpu.Y);
        if (!g_cpu.Y) { dc_set_mx(1, 0); return; }
    }
    abs16_set(g_cpu.X, dp16(0x08));
    abs16_set(0x0003 + g_cpu.X, 0);
    dc_set_mx(1, 0);
    abs8_set(0x0002 + g_cpu.X, 0);
    dc_push8(g_cpu.DB);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.Y = dc_adc16(g_cpu.X, 0x0005);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16(dp16(0x4E), 0x0005);
    g_cpu.A = 0x003A;
    u8 c = g_cpu._flag_C, v = g_cpu._flag_V;
    dc_mvn(0x00, 0x00);
    g_cpu.X = g_cpu.Y;
    dc_set_mx(1, 0);
    plb();
    g_cpu._flag_C = c; g_cpu._flag_V = v;
}
DC_ENTRY(C20A6C, RTS)                            /* sixteen rows of the map strip (0AAE) */
{
    dp16_set(0x00, 0x0010);
    g_cpu._flag_C = 0;
    dp16_set(0x18, dc_adc16(dp16(0x08), dp16(0x16)));
    u16 a = lsr16(lsr16(xba16(dp16(0x0A))));
    dp16_set(0x1A, a);
    a = dc_adc16(lsr16(a), dp16(0x1A));
    dp16_set(0x04, a);
    dp16_set(0x06, lsr16(lsr16(dp16(0x0E) & 0x00F0)));
    do {
        dc_jsr(0xC20AAE, 0xC20A8D);
        g_cpu._flag_C = 0;
        a = dc_adc16(dp16(0x04), 0x0060);
        dc_cmp16(a, 0x1800);
        if (g_cpu._flag_C) a = g_cpu.D;
        dp16_set(0x04, a);
        g_cpu._flag_C = 0;
        dp16_set(0x06, dc_adc16(dp16(0x06), 0x0004) & 0x003F);
        u16 n = (u16)(dp16(0x00) - 1);
        dp16_set(0x00, n);
        dc_set_flags_nz16(n);
    } while (dp16(0x00));
    g_cpu.A = dp16(0x06);
}
DC_ENTRY(C28663, RTS)                            /* queued VRAM uploads: 8-byte records from 0C00 up to 0BF8+$0D11 */
{
    dc_push8(g_cpu.DB);
    dc_push16(g_cpu.D);
    php();
    dc_set_mx(1, 1);
    dc_push16(0x0000);
    plb(); plb();
    g_cpu.X = abs8(0x0D11);
    if (g_cpu.X) {
        abs8_set(0x0D11, 0);
        dc_set_mx(0, 1);
        abs16_set(0x4370, 0x1801);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0BF8);
        g_cpu.X = 0x80;
        do {
            g_cpu.D = g_cpu.A;
            abs16_set(0x2116, dp16(0x00));
            abs16_set(0x4372, dp16(0x02));
            g_cpu.Y = dp8(0x04);
            abs8_set(0x4374, (u8)g_cpu.Y);
            abs16_set(0x4375, dp16(0x05));
            abs8_set(0x420B, (u8)g_cpu.X);
            g_cpu._flag_C = 1;
            g_cpu.A = dc_sbc16(g_cpu.D, 0x0008);
            dc_cmp16(g_cpu.A, 0x0C00);
        } while (g_cpu._flag_C);
    }
    plp();
    g_cpu.D = dc_pull16();
    plb();
}
DC_ENTRY(C257DF, RTL)                            /* text: start string $0C of the table at [$0D] (D = 0200) */
{
    php();
    dc_set_mx(0, g_cpu.x_flag);
    dc_push16(g_cpu.D);
    g_cpu.D = 0x0200;
    g_cpu.Y = asl16(dp16(0x0C) & 0x00FF);
    dp16_set(0x31, bus16(lp(0x0D) + g_cpu.Y));
    g_cpu.A = 0;
    dc_set_mx(1, g_cpu.x_flag);
    dp8_set(0x33, dp8(0x0F));
    dp8_set(0x30, 0);
    u8 k = dp8(0x14);
    dp8_set(0x34, k == 2 ? 0x00 : 0x08);
    if (k & 0x80) dp8_set(0x34, 0);
    dp8_set(0x15, 0x04);
    g_cpu.X = 0x0200;
    dp16_set(0x3D, 0x0200);
    dp8_set(0x3F, 0);
    dp8_set(0x17, 0);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
    plp();
}
DC_ENTRY(C20003, RTL) { dc_C257DF_glue(); }
DC_ENTRY(C2BBD6, RTS)                            /* list of the techs the current character can combine with ($17..) */
{
    php();
    dp16_set(0x17, 0xFFFF);
    dc_set_mx(1, 1);
    dc_jsr(0xC2F566, 0xC2BBDE);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc8(abs8(0x0F48), abs8(0x0F4A));
    abs8_set(0x0F4C, abs8(0x0F00 + g_cpu.X));
    g_cpu.X = abs8(0x0F30 + g_cpu.X);
    dp8_set(0x00, bus8(0xCC2963 + g_cpu.X));
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc8(abs8(0x0419), abs8(0x041A));
    u8 who = abs8(0x0D5F + g_cpu.X);
    dp8_set(0x16, who);
    g_cpu.X = who;
    u8 m = bus8(0xFFF9BB + g_cpu.X);
    dp8_set(0x00, dp8(0x00) & ~m);
    dp8_set(0x01, 0);
    g_cpu.X = 0;
    do {
        u8 v = dp8(0x00);
        dp8_set(0x00, (u8)(v << 1));
        if (v & 0x80) {
            b0_8_set(dpx_x(0x17), dp8(0x01));
            g_cpu.X = (u8)(g_cpu.X + 1);
        }
        dp8_set(0x01, (u8)(dp8(0x01) + 1));
    } while (dp8(0x00));
    dc_set_a8(0);
    plp();
}
DC_ENTRY(C22768, RTS)                            /* four decompress requests from [$ED]+0C through C6:FDF0 */
{
    dp8_set(0x12, dp8(0xEF));
    abs8_set(0x0305, 0x7F);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    dp16_set(0x10, dc_adc16(dp16(0xED), 0x000C));
    abs16_set(0x0303, 0x9000);
    dp16_set(0x08, 0);
    u16 a;
    do {
        dc_set_mx(1, 0);
        g_cpu.Y = dp16(0x08);
        g_cpu.A = g_cpu.D;
        u8 n = bus8(lp(0x10) + g_cpu.Y);
        dc_set_a8(n);
        if (!(n & 0x80)) {
            dc_set_a8(dc_adc8(asl8(n), bus8(lp(0x10) + g_cpu.Y)));
            g_cpu.X = g_cpu.A;
            dc_set_mx(0, 0);
            abs16_set(0x0300, bus16(0xC6FDF0 + g_cpu.X));
            dc_set_mx(1, 0);
            dc_set_a8(bus8(0xC6FDF2 + g_cpu.X));
            abs8_set(0x0302, (u8)g_cpu.A);
            dc_jsl(0xC30002, 0xC227A2);
        }
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        abs16_set(0x0303, dc_adc16(abs16(0x0303), 0x1000));
        a = (u16)(dp16(0x08) + 1);
        dp16_set(0x08, a);
        lda16(a);
        dc_cmp16(a, 0x0004);
    } while (a != 4);
}
DC_ENTRY(C20405, RTS)                            /* run the queued VRAM DMAs (8-byte records at $60, $5F bytes) */
{
    lda8(dp8(0x5E));
    if ((u8)g_cpu.A) return;
    lda8(dp8(0x5F));
    if (!(u8)g_cpu.A) return;
    dc_set_mx(1, 1);
    g_cpu.X = 0;
    do {
        dc_set_mx(0, 1);
        abs16_set(0x4302, dpx16(0x60));
        abs16_set(0x2116, dpx16(0x63));
        g_cpu.A = dpx16(0x65);
        abs16_set(0x4305, g_cpu.A);
        dc_set_mx(1, 1);
        abs8_set(0x4304, b0_8(dpx_x(0x62)));
        abs8_set(0x2115, b0_8(dpx_x(0x67)));
        abs8_set(0x4301, 0x18);
        abs8_set(0x4300, 0x01);
        abs8_set(0x420B, 0x01);
        g_cpu._flag_C = 0;
        dc_set_a8(dc_adc8((u8)g_cpu.X, 0x08));
        g_cpu.X = (u8)g_cpu.A;
        dc_cmp8((u8)g_cpu.X, dp8(0x5F));
    } while (!g_cpu._flag_Z);
    dp8_set(0x5F, 0);
    dc_set_mx(1, 0);
    dp16_set(0xE0, 0xF000);
    g_cpu.X = 0xF000;
    lda8(0x7E);
    dp8_set(0xE2, 0x7E);
}
DC_ENTRY(C2090D, RTS)                            /* queue a 40-byte VRAM upload from [$E0] for map column $08 */
{
    u16 a = lsr16(lsr16(lsr16(dp16(0x08)))) & 0x003F;
    if (a >= 0x0020) { g_cpu._flag_C = 0; a = dc_adc16(a, 0x03E0); }
    g_cpu._flag_C = 0;
    a = dc_adc16(a, dp16(0x16));
    dp16_set(0x08, a);
    g_cpu.A = a;
    dc_set_mx(1, 1);
    abs8_set(0x005E, (u8)(abs8(0x005E) + 1));
    g_cpu.X = abs8(0x005F);
    abs8_set(0x0062 + g_cpu.X, 0x7E);
    abs8_set(0x0067 + g_cpu.X, 0x81);
    dc_set_mx(0, 1);
    abs16_set(0x0060 + g_cpu.X, abs16(0x00E0));
    abs16_set(0x0063 + g_cpu.X, dp16(0x08));
    abs16_set(0x0065 + g_cpu.X, 0x0040);
    dc_set_mx(1, 1);
    g_cpu._flag_C = 0;
    g_cpu.A = 0x0000;
    dc_set_a8(dc_adc8((u8)g_cpu.X, 0x08));
    abs8_set(0x005F, (u8)g_cpu.A);
    abs8_set(0x005E, 0);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(abs16(0x00E0), 0x0040);
    abs16_set(0x00E0, g_cpu.A);
}
DC_ENTRY(C28555, RTS)                            /* one bit per stat byte at 2993.. that the mask at $F0,X fully covers */
{
    static const u8 off[8] = { 0x98, 0x96, 0x97, 0x94, 0x99, 0x9A, 0x95, 0x93 };
    bool w16 = !g_cpu.m_flag;
    u16 at = dpx_x(0xF6);
    u16 f = 0;
    for (int i = 0; i < 8; i++) {
        u16 m = dpx16(0xF0), t = abs16(0x2900 + off[i] + g_cpu.Y);
        if (!w16) { m &= 0xFF; t &= 0xFF; }
        g_cpu._flag_C = (m & t) == t;
        u8 c = f & 1;
        f = (u16)(f >> 1 | g_cpu._flag_C << (w16 ? 15 : 7));
        g_cpu._flag_C = c;
    }
    b0_8_set(at, (u8)f);
    if (w16) b0_8_set((u16)(at + 1), (u8)(f >> 8));
    u16 v = dpx16(0xF1);
    u16 to = dpx_x(0xF7);
    if (w16) { lda16(v); b0_8_set(to, (u8)v); b0_8_set((u16)(to + 1), (u8)(v >> 8)); }
    else { lda8((u8)v); b0_8_set(to, (u8)v); }
    if (g_cpu.x_flag) { g_cpu.X = (u8)(g_cpu.X + 2); dc_set_flags_nz8((u8)g_cpu.X); }
    else { g_cpu.X = (u16)(g_cpu.X + 2); dc_set_flags_nz16(g_cpu.X); }
}
DC_ENTRY(C2416C, RTS)                            /* clear the object's bit 7 flag, animation from the party member's frame */
{
    dc_set_mx(1, 0);
    lda8(abs8(0x0028 + g_cpu.X) & 0x7F);
    abs8_set(0x0028 + g_cpu.X, (u8)g_cpu.A);
    g_cpu.A = g_cpu.D;
    dc_set_a8(abs8(0x0024 + g_cpu.X));
    g_cpu.X = g_cpu.A;
    lda8(bus8(0x7E2980 + g_cpu.X));
    dp8_set(0x00, (u8)g_cpu.A);
    dc_C23987_glue();
}
DC_ENTRY(C220AF, RTS)                            /* timed script step: C = finished */
{
    u8 f = abs8(0x1BF6);
    lda8(f);
    if (f & 0x02) { g_cpu._flag_Z = 0; g_cpu._flag_C = 1; return; }
    dc_set_a8(0x02);
    g_cpu._flag_Z = (f & 0x02) == 0;
    abs8_set(0x1BF6, f | 0x02);
    g_cpu.X = dp16(0x4E);
    abs8_set(0x0002 + g_cpu.X, (u8)(abs8(0x0002 + g_cpu.X) + 1));
    g_cpu.Y = abs16(0x0007 + g_cpu.X);
    dp16_set(0x10, g_cpu.Y);
    dp8_set(0x12, abs8(0x0009 + g_cpu.X));
    g_cpu.Y = 1;
    u8 n = bus8(lp(0x10) + 1);
    lda8(n);
    if (!n) {
        lda8(0x80);
        dp8_set(0x1C, 0x80);
        dp8_set(0x1D, 0);
        g_cpu._flag_C = 1;
        return;
    }
    abs8_set(0x000A + g_cpu.X, n);
    abs8_set(0x0003 + g_cpu.X, 0);
    g_cpu.X = dp16(0x4E);
    lda8(abs8(0x0003 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, abs8(0x000A + g_cpu.X));
    if (!g_cpu._flag_C) return;
    abs8_set(0x0003 + g_cpu.X, 0);
    lda8(dp8(0x1D));
    if ((u8)g_cpu.A) {
        u8 d = (u8)(dp8(0x1D) - 1);
        dp8_set(0x1D, d);
        dc_set_flags_nz8(d);
        if (d) { g_cpu._flag_C = 0; return; }
    }
    lda8(0x80);
    dp8_set(0x1C, 0x80);
    dc_set_a8(0x02);
    f = abs8(0x1BF6);
    g_cpu._flag_Z = (f & 0x02) == 0;
    abs8_set(0x1BF6, f & ~0x02);
    g_cpu._flag_C = 1;
}
DC_ENTRY(C221A5, RTS)                            /* timed script step with a fading level in $1F: C = finished */
{
    u16 at = dpx_x(0x21);
    u16 c = (u16)((b0_8(at) | b0_8((u16)(at + 1)) << 8) - 1);
    b0_8_set(at, (u8)c); b0_8_set((u16)(at + 1), (u8)(c >> 8));
    g_cpu.X = dp16(0x4E);
    g_cpu.A = abs16(0x0007 + g_cpu.X);
    dp16_set(0x10, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(abs8(0x0009 + g_cpu.X));
    dp8_set(0x12, (u8)g_cpu.A);
    abs8_set(0x0002 + g_cpu.X, (u8)(abs8(0x0002 + g_cpu.X) + 1));
    lda8(dp8(0x1F) | bus8(lp(0x10) + 1));
    dp8_set(0x1F, (u8)g_cpu.A);
    g_cpu.Y = 2;
    u8 n = bus8(lp(0x10) + 2);
    lda8(n);
    if (!n) {
        lda8(dp8(0x1F) & 0x0F);
        dp8_set(0x1F, (u8)g_cpu.A);
        g_cpu._flag_C = 1;
        return;
    }
    abs8_set(0x000A + g_cpu.X, n);
    abs8_set(0x0003 + g_cpu.X, 0);
    g_cpu.X = dp16(0x4E);
    lda8(abs8(0x0003 + g_cpu.X));
    dc_cmp8((u8)g_cpu.A, abs8(0x000A + g_cpu.X));
    if (!g_cpu._flag_C) return;
    abs8_set(0x0003 + g_cpu.X, 0);
    lda8(dp8(0x1F) & 0xF0);
    if (!(u8)g_cpu.A) { g_cpu._flag_C = 1; return; }
    g_cpu._flag_C = 1;
    dc_set_a8(dc_sbc8(dp8(0x1F), 0x10));
    dp8_set(0x1F, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
}
/* ASL A then ROL a byte: move A's top bit into bit 0 of the byte. */
static void asl_rol(u16 at)
{
    dc_set_a8(asl8((u8)g_cpu.A));
    abs8_set(at, rol8(abs8(at)));
}
DC_ENTRY(C292F4, RTS)                            /* unpack the settings bytes 2990..2992 into 0D86..0D90 */
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x0D86, 0);
    g_cpu.X = 0x0D86; g_cpu.Y = 0x0D88; g_cpu.A = 0x0009;
    dc_mvn(0x7E, 0x7E);
    dc_set_mx(1, 0);
    dc_set_a8(abs8(0x2990));
    asl_rol(0x0D8F); asl_rol(0x0D89); asl_rol(0x0D88); asl_rol(0x0D87); asl_rol(0x0D86);
    abs8_set(0x0D8B, abs8(0x2990) & 0x07);
    abs8_set(0x0D8C, abs8(0x2991) & 0x07);
    abs8_set(0x0D90, (abs8(0x2991) >> 3) & 0x07);
    dc_set_a8(abs8(0x2991));
    asl_rol(0x0D8E); asl_rol(0x0D8D);
    lda8(abs8(0x2992) & 0x03);
    abs8_set(0x0D8A, (u8)g_cpu.A);
    plp();
}
DC_ENTRY(C2F378, RTS)                            /* swap the tilemap rectangles at $63 and $5B (size $5F x $60) */
{
    php();
    dc_set_mx(0, 0);
    dc_jsr(0xC2ED90, 0xC2F37B);
    dp16_set(0x65, dp16(0x63));
    dp16_set(0x5D, dp16(0x5B));
    dc_jsr(0xC2ED90, 0xC2F386);
    dp16_set(0x8A, dp16(0x5F) & 0x001F);
    dp16_set(0x8C, dp16(0x60) & 0x003F);
    do {
        g_cpu.X = dp16(0x63);
        g_cpu.Y = dp16(0x65);
        dp16_set(0x00, dp16(0x8A));
        do {
            u16 a = abs16(g_cpu.X);
            abs16_set(g_cpu.X, abs16(g_cpu.Y));
            abs16_set(g_cpu.Y, a);
            g_cpu.X += 2; g_cpu.Y += 2;
            dp16_set(0x00, (u16)(dp16(0x00) - 1));
        } while (dp16(0x00));
        g_cpu._flag_C = 0;
        dp16_set(0x63, dc_adc16(dp16(0x63), 0x0040));
        g_cpu.A = dc_adc16(dp16(0x65), 0x0040);
        dp16_set(0x65, g_cpu.A);
        u16 n = (u16)(dp16(0x8C) - 1);
        dp16_set(0x8C, n);
        dc_set_flags_nz16(n);
    } while (dp16(0x8C));
    plp();
}
/* 22 tilemap entries of the map strip: tile words at +off/+off+2 of the
 * 8-byte record ($10 + index*8) for even/odd columns. */
static void map_strip(u16 off, u32 site)
{
    dc_jsr(0xC208DB, site);
    dc_set_mx(1, 0);
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    dc_push8(0x7E);
    plb();
    dc_set_mx(0, 0);
    dp16_set(0x19, 0x0022);
    do {
        g_cpu.Y = dp16(0x04);
        u16 a = asl16(asl16(asl16(bus16(dba(dp16(0x02)) + g_cpu.Y) & 0x00FF)));
        g_cpu.X = dc_adc16(a, dp16(0x10));
        g_cpu.Y = dp16(0x0A);
        if (dp16(0x08) & 1) {
            bus16_set(dba(dp16(0x06)) + g_cpu.Y, abs16(off + 2 + g_cpu.X));
            a = (u16)(dp16(0x04) + 1);
            dc_cmp16(a, 0x0060);
            if (g_cpu._flag_C) a = g_cpu.D;
            dp16_set(0x04, a);
        } else {
            g_cpu._flag_C = 0;
            bus16_set(dba(dp16(0x06)) + g_cpu.Y, abs16(off + g_cpu.X));
        }
        g_cpu.A = (u16)((g_cpu.Y + 2) & 0x007F);
        dp16_set(0x0A, g_cpu.A);
        dp16_set(0x08, (u16)(dp16(0x08) + 1));
        dp16_set(0x19, (u16)(dp16(0x19) - 1));
    } while (dp16(0x19));
    plb();
}
DC_ENTRY(C20843, RTS) { map_strip(0, 0xC20843); }
DC_ENTRY(C2088F, RTS) { map_strip(4, 0xC2088F); }

/* ---- the text engine (D = 0200) -------------------------------------------
 * 58B2 reads the string at [$31]: bytes A0+ are drawn (5DC4), 21..9F are
 * dictionary words (DE:FA00), and 00..20 are control codes dispatched
 * through the C2:5903 table. Words, numbers and names are then drawn by one
 * of three inner loops over [$37] ($3A characters): 5BF5 (text, with the
 * 01/02 two-byte prefixes), 5C3E (digits, +D4) and 5C77 (hex digits). Each
 * inner loop leaves through a 4-entry table by (word ended?, line full?):
 * line full stops with $15 = 10 ($30 keeps the inner loop to resume), word
 * ended goes back to the string. The originals chain these by JMP; here the
 * tail jumps are C calls. */
static void dc_C258B2_glue(void);
static void dc_C25BF5_glue(void);
static void dc_C25C3E_glue(void);
static void dc_C25C77_glue(void);
static void dc_C25B06_glue(void);
static void dc_C25953_glue(void);
static void dc_C25985_glue(void);
static void dc_C259C8_glue(void);
static void dc_C25A26_glue(void);
static void dc_C25A4C_glue(void);
static void dc_C25A8E_glue(void);
static void dc_C25AD0_glue(void);
static void dc_C25B15_glue(void);
static void dc_C25B3B_glue(void);
static void dc_C25B56_glue(void);
static void dc_C25B70_glue(void);
static void dc_C25B9B_glue(void);
static void dc_C25BD6_glue(void);

static void bad_jump(u32 at, u16 to)
{
    fprintf(stderr, "[decomp] %06X: JMP (table,X) to unknown %04X\n", (unsigned)at, to);
    abort();
}
static void inc16_dp(u8 o) { u16 v = (u16)(dp16(o) + 1); dp16_set(o, v); dc_set_flags_nz16(v); }
static u8 dec8_dp(u8 o) { u8 v = (u8)(dp8(o) - 1); dp8_set(o, v); dc_set_flags_nz8(v); return v; }
static void xba_a(void) { g_cpu.A = (u16)(g_cpu.A << 8 | g_cpu.A >> 8); dc_set_flags_nz8((u8)g_cpu.A); }

/* Draw the character in A ($35 = A), count it; true while the line has room. */
static bool text_put(u32 site)
{
    dc_set_mx(0, 0);
    dp16_set(0x35, g_cpu.A);
    dc_set_mx(1, 0);
    dc_jsr(0xC25DC4, site);
    return true;
}
/* Common end of the three inner loops: X = 2 * (word not ended | line not full << 1). */
static void inner_next(u32 tab, u32 at, int kind)
{
    lda8(0x00);
    if (dec8_dp(0x3A)) lda8((u8)(g_cpu.A | 0x01));
    if (dec8_dp(0x13)) lda8((u8)(g_cpu.A | 0x02));
    dc_set_a8(asl8((u8)g_cpu.A));
    g_cpu.X = g_cpu.A;
    u16 to = bus16(tab + g_cpu.X);
    static const u16 ends[3][4] = {
        { 0x5C30, 0x5C32, 0x5C37, 0x5C3C },
        { 0x5C69, 0x5C6B, 0x5C70, 0x5C75 },
        { 0x5CB2, 0x5CB4, 0x5CB9, 0x5CBE },
    };
    if (to == ends[kind][0]) { dp8_set(0x30, 0); lda8(0x10); dp8_set(0x15, 0x10); }
    else if (to == ends[kind][1]) { lda8(0x10); dp8_set(0x15, 0x10); }
    else if (to == ends[kind][2]) { dp8_set(0x30, 0); dc_C258B2_glue(); }
    else if (to == ends[kind][3]) {
        if (kind == 0) dc_C25BF5_glue(); else if (kind == 1) dc_C25C3E_glue(); else dc_C25C77_glue();
    }
    else bad_jump(at, to);
}
/* Text characters at [$37] (01/02 prefix a second byte). */
static void inner_text(u32 tab, u32 at, u32 site, int kind)
{
    dc_set_a8(bus8(lp(0x37)));
    dc_set_mx(0, 0); inc16_dp(0x37); dc_set_mx(1, 0);
    u8 c = (u8)g_cpu.A;
    dc_cmp8(c, 0x01);
    if (c != 1) dc_cmp8(c, 0x02);
    if (c == 1 || c == 2) {
        xba_a();
        lda8(bus8(lp(0x37)));
        dc_set_mx(0, 0); inc16_dp(0x37);
    }
    text_put(site);
    inner_next(tab, at, kind);
}
DC_ENTRY(C25BF5, RTS) { inner_text(0xC25C28, 0xC25C25, 0xC25C12, 0); }
DC_ENTRY(C25C77, RTS) { inner_text(0xC25CAA, 0xC25CA7, 0xC25C94, 2); }
DC_ENTRY(C25C3C, RTS) { dc_C25BF5_glue(); }
DC_ENTRY(C25CBE, RTS) { dc_C25C77_glue(); }
DC_ENTRY(C25C3E, RTS)                            /* digit characters at [$37] (tile D4 + n) */
{
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8(bus8(lp(0x37)), 0xD4));
    dc_set_mx(0, 0);
    dp16_set(0x35, g_cpu.A);
    inc16_dp(0x37);
    dc_set_mx(1, 0);
    dc_jsr(0xC25DC4, 0xC25C4B);
    inner_next(0xC25C61, 0xC25C5E, 1);
}
DC_ENTRY(C25C75, RTS) { dc_C25C3E_glue(); }
DC_ENTRY(C25C37, RTS) { dp8_set(0x30, 0); dc_C258B2_glue(); }
DC_ENTRY(C25C70, RTS) { dp8_set(0x30, 0); dc_C258B2_glue(); }
DC_ENTRY(C25CB9, RTS) { dp8_set(0x30, 0); dc_C258B2_glue(); }

/* Start an inner loop over a word of the DE:FA00 dictionary (index in A). */
static void dict_word(void)
{
    dc_set_mx(0, 0);
    g_cpu.X = g_cpu.A;
    dp16_set(0x37, bus16(0xDEFA00 + g_cpu.X));
    g_cpu.A = 0x0000;
    dc_set_mx(1, 0);
    dc_set_a8(0xDE);
    dp8_set(0x39, 0xDE);
    dc_set_a8(bus8(lp(0x37)));
    dp8_set(0x3A, (u8)g_cpu.A);
    dc_set_mx(0, 0); inc16_dp(0x37); dc_set_mx(1, 0);
    lda8(0x01);
    dp8_set(0x30, 0x01);
    dc_C25BF5_glue();
}
/* Inner text loop over a 7E/C2/CC string set up by a control code. */
static void start_text(void)
{
    lda8(0x01);
    dp8_set(0x30, 0x01);
    dc_set_a8(0x00);
    xba_a();
    dc_C25BF5_glue();
}
DC_ENTRY(C258B2, RTS)                            /* draw the string at [$31] until the line is full */
{
    for (;;) {
        dc_set_a8(bus8(lp(0x31)));
        dc_set_mx(0, 0); inc16_dp(0x31); dc_set_mx(1, 0);
        u8 c = (u8)g_cpu.A;
        dc_cmp8(c, 0xA0);
        if (c >= 0xA0) {
draw:
            text_put(0xC258C4);
            if (dec8_dp(0x13)) continue;
            lda8(0x10);
            dp8_set(0x15, 0x10);
            return;
        }
        dc_cmp8(c, 0x21);
        if (c >= 0x21) {
            dp8_set(0x3B, c);
            g_cpu._flag_C = 1;
            dc_set_a8(asl8(dc_sbc8(c, 0x21)));
            dict_word();
            return;
        }
        g_cpu.Y = g_cpu.A;
        dc_set_a8(asl8(c));
        g_cpu.X = g_cpu.A;
        u16 to = bus16(0xC25903 + g_cpu.X);
        switch (to) {
        case 0x594F: dc_C2594F_glue(); return;
        case 0x5945: dc_C25945_glue(); return;
        case 0x5B14: return;
        case 0x5B06:                             /* 01/02: two-byte character */
            dc_set_a8((u8)g_cpu.Y);
            xba_a();
            lda8(bus8(lp(0x31)));
            dc_set_mx(0, 0);
            abs16_set(0x0231, (u16)(abs16(0x0231) + 1));
            dc_set_flags_nz16(abs16(0x0231));
            dc_set_mx(1, 0);
            goto draw;
        case 0x5953: dc_C25953_glue(); return;
        case 0x5985: dc_C25985_glue(); return;
        case 0x59C8: dc_C259C8_glue(); return;
        case 0x5A26: dc_C25A26_glue(); return;
        case 0x5A4C: dc_C25A4C_glue(); return;
        case 0x5B15: dc_C25B15_glue(); return;
        case 0x5B3B: dc_C25B3B_glue(); return;
        case 0x5B70: dc_C25B70_glue(); return;
        case 0x5B56: dc_C25B56_glue(); return;
        case 0x5B9B: dc_C25B9B_glue(); return;
        case 0x5BD6: dc_C25BD6_glue(); return;
        default: bad_jump(0xC25900, to);
        }
    }
}
DC_ENTRY(C25B06, RTS)                            /* control 01/02: a two-byte character */
{
    dc_set_a8((u8)g_cpu.Y);
    xba_a();
    lda8(bus8(lp(0x31)));
    dc_set_mx(0, 0);
    abs16_set(0x0231, (u16)(abs16(0x0231) + 1));
    dc_set_flags_nz16(abs16(0x0231));
    dc_set_mx(1, 0);
    text_put(0xC258C4);
    if (dec8_dp(0x13)) { dc_C258B2_glue(); return; }
    lda8(0x10);
    dp8_set(0x15, 0x10);
}
/* Number of digits in A: draw the digits at 0240 (5C3E) or hex digits (5C77). */
static void start_digits(int hex)
{
    dp8_set(0x3A, (u8)g_cpu.A);
    g_cpu.X = 0x0240;
    dp16_set(0x37, 0x0240);
    dc_set_a8(0x00);
    dp8_set(0x39, 0x00);
    lda8(hex ? 0x03 : 0x02);
    dp8_set(0x30, (u8)g_cpu.A);
    dc_set_a8(0x00);
    xba_a();
    if (hex) dc_C25C77_glue(); else dc_C25C3E_glue();
}
DC_ENTRY(C25953, RTS)                            /* control 0D: 8-bit number (decimal, or hex when $A1) */
{
    dc_set_a8(bus8(lp(0x3D)));
    dp8_set(0x68, (u8)g_cpu.A);
    dc_set_mx(0, 0); inc16_dp(0x3D); dc_set_mx(1, 0);
    lda8(dp8(0xA1));
    if (!(u8)g_cpu.A) {
        dc_jsr(0xC2615E, 0xC25961);
        g_cpu.X = dp16(0x60);
        dp16_set(0x40, g_cpu.X);
        dc_set_a8(dp8(0x62));
        dp8_set(0x42, (u8)g_cpu.A);
        dc_jsr(0xC25DAD, 0xC2596C);
        start_digits(0);
        return;
    }
    dc_set_mx(0, 0);
    dp16_set(0x6A, 0);
    g_cpu.A = dp16(0x68);
    dp16_set(0x60, g_cpu.A);
    dc_jsr(0xC25CC0, 0xC2597A);
    lda16(0x0002);
    dc_set_mx(1, 0);
    start_digits(1);
}
DC_ENTRY(C25985, RTS)                            /* control 0E: 16-bit number */
{
    dc_set_mx(0, 0);
    g_cpu.A = bus16(lp(0x3D));
    dp16_set(0x68, g_cpu.A);
    dp16_set(0x3D, (u16)(dp16(0x3D) + 2));
    lda16(0x0000);
    dc_set_mx(1, 0);
    lda8(dp8(0xA1));
    if (!(u8)g_cpu.A) {
        dc_jsr(0xC26180, 0xC25998);
        dp16_set(0x40, dp16(0x60));
        dp16_set(0x42, dp16(0x62));
        g_cpu.X = dp16(0x62);
        dc_set_a8(dp8(0x64));
        dp8_set(0x44, (u8)g_cpu.A);
        dc_jsr(0xC25D91, 0xC259A7);
        start_digits(0);
        return;
    }
    dc_set_mx(0, 0);
    dp16_set(0x6A, 0);
    g_cpu.A = xba16(dp16(0x68));
    dp16_set(0x60, g_cpu.A);
    dc_jsr(0xC25CC0, 0xC259B6);
    g_cpu.A = dp16(0x68);
    dp16_set(0x60, g_cpu.A);
    dc_jsr(0xC25CC0, 0xC259BD);
    lda16(0x0004);
    dc_set_mx(1, 0);
    start_digits(1);
}
DC_ENTRY(C259C8, RTS)                            /* control 0F: 24-bit number */
{
    dc_set_mx(0, 0);
    g_cpu.A = bus16(lp(0x3D));
    dp16_set(0x68, g_cpu.A);
    dc_set_mx(1, 0);
    g_cpu.Y = 2;
    dc_set_a8(bus8(lp(0x3D) + 2));
    dp8_set(0x6A, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    dp16_set(0x3D, dc_adc16(dp16(0x3D), 0x0003));
    lda16(0x0000);
    dc_set_mx(1, 0);
    dc_jsr(0xC261BD, 0xC259E6);
    dp16_set(0x40, dp16(0x60));
    dp16_set(0x42, dp16(0x62));
    dp16_set(0x44, dp16(0x64));
    g_cpu.X = dp16(0x66);
    dp16_set(0x46, g_cpu.X);
    dc_jsr(0xC25D56, 0xC259F9);
    start_digits(0);
}
/* A control code's string at 7E:(C2:5FD8 + X), length from 5D45. */
static void name_text(u32 site)
{
    dc_set_mx(0, 0);
    g_cpu.A = bus16(0xC25FD8 + g_cpu.X);
    abs16_set(0x0237, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(0x7E);
    abs8_set(0x0239, 0x7E);
    lda8(0x01);
    dp8_set(0x30, 0x01);
    dc_jsr(0xC25D45, site);
    abs8_set(0x023A, (u8)g_cpu.A);
    dc_set_a8(0x00);
    xba_a();
    dc_C25BF5_glue();
}
DC_ENTRY(C25A26, RTS)                            /* control 11: name number (byte) */
{
    dc_set_a8(bus8(lp(0x3D)));
    dc_set_mx(0, 0);
    inc16_dp(0x3D);
    g_cpu.X = g_cpu.A = asl16(g_cpu.A);
    name_text(0xC25A40);
}
DC_ENTRY(C25B15, RTS)                            /* controls 13-19: the names in C2:5FD8 */
{
    dc_set_a8((u8)g_cpu.Y);
    g_cpu._flag_C = 1;
    dc_set_a8(asl8(dc_sbc8((u8)g_cpu.A, 0x13)));
    g_cpu.X = g_cpu.A;
    name_text(0xC25B2F);
}
DC_ENTRY(C25B70, RTS)                            /* controls 1B-1D: name of party member n */
{
    dc_set_a8((u8)g_cpu.Y);
    g_cpu._flag_C = 1;
    dc_set_a8(dc_sbc8((u8)g_cpu.A, 0x1B));
    g_cpu.X = g_cpu.A;
    dc_set_a8(asl8(bus8(0x7E2980 + g_cpu.X)));
    g_cpu.X = g_cpu.A;
    name_text(0xC25B8F);
}
/* A 7E string at a fixed address, length from 5D45. */
static void fixed_text(u16 addr, u32 site)
{
    g_cpu.X = addr;
    abs16_set(0x0237, addr);
    dc_set_a8(0x7E);
    abs8_set(0x0239, 0x7E);
    dc_jsr(0xC25D45, site);
    abs8_set(0x023A, (u8)g_cpu.A);
    start_text();
}
DC_ENTRY(C25B3B, RTS) { fixed_text(0x2C23, 0xC25B46); }   /* control 1A: 7E:2C23 */
DC_ENTRY(C25BD6, RTS)                            /* control 20: 7E:2C4D */
{
    dc_set_mx(0, 0);
    lda16(0x2C4D);
    abs16_set(0x0237, 0x2C4D);
    dc_set_mx(1, 0);
    dc_set_a8(0x7E);
    abs8_set(0x0239, 0x7E);
    lda8(0x01);
    dp8_set(0x30, 0x01);
    dc_jsr(0xC25D45, 0xC25BE9);
    abs8_set(0x023A, (u8)g_cpu.A);
    dc_set_a8(0x00);
    xba_a();
    dc_C25BF5_glue();
}
DC_ENTRY(C25B56, RTS)                            /* control 1E: the 5-character C2:6146 string */
{
    g_cpu.X = 0x6146;
    dc_set_a8(0xC2);
    abs16_set(0x0237, 0x6146);
    abs8_set(0x0239, 0xC2);
    dc_set_a8(0x05);
    abs8_set(0x023A, 0x05);
    start_text();
}
/* 11-byte CC record n (4202 x 0B + base); the string length from `len`. */
static void cc_record(u8 n, u16 base)
{
    bus8_set(0x004202, n);
    dc_set_a8(0x0B);
    bus8_set(0x004203, 0x0B);
    g_cpu._flag_C = 0;
    dc_set_mx(0, 0);
    g_cpu.A = dc_adc16(bus16(0x004216), base);
}
DC_ENTRY(C25B9B, RTS)                            /* control 1F: name of item 7F:0200 */
{
    dc_set_mx(0, 0);
    lda16(bus16(0x7F0200) & 0x00FF);
    dc_set_mx(1, 0);
    cc_record((u8)g_cpu.A, 0x0B5E);
    lda16((u16)(g_cpu.A + 1));
    abs16_set(0x0237, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(0xCC);
    abs8_set(0x0239, 0xCC);
    dc_jsr(0xC25D23, 0xC25BC6);
    abs8_set(0x023A, (u8)g_cpu.A);
    start_text();
}
/* 5AFC: start the text loop on the CC string set up above. */
static void cc_text(void)
{
    start_text();
}
DC_ENTRY(C25A8E, RTS)                            /* control 12 00: name of tech n (CC:15C4, skip a leading 2F) */
{
    dc_set_a8(bus8(lp(0x3D)));
    dc_set_mx(0, 0); inc16_dp(0x3D); dc_set_mx(1, 0);
    cc_record((u8)g_cpu.A, 0x15C4);
    abs16_set(0x0237, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(0xCC);
    abs8_set(0x0239, 0xCC);
    dc_jsr(0xC25D34, 0xC25AB5);
    abs8_set(0x023A, (u8)g_cpu.A);
    lda8(bus8(lp(0x37)));
    dc_cmp8((u8)g_cpu.A, 0x2F);
    if (g_cpu._flag_Z) {
        dc_set_mx(0, 0);
        abs16_set(0x0237, (u16)(abs16(0x0237) + 1));
        dc_set_flags_nz16(abs16(0x0237));
        dc_set_mx(1, 0);
        dc_jsr(0xC25D23, 0xC25AC8);
        abs8_set(0x023A, (u8)g_cpu.A);
    }
    cc_text();
}
DC_ENTRY(C25AD0, RTS)                            /* control 12 01: 11-character CC:6500 record n */
{
    dc_set_a8(bus8(lp(0x3D)));
    dc_set_mx(0, 0); inc16_dp(0x3D); dc_set_mx(1, 0);
    cc_record((u8)g_cpu.A, 0x6500);
    abs16_set(0x0237, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(0xCC);
    abs8_set(0x0239, 0xCC);
    dc_set_a8(0x0B);
    abs8_set(0x023A, 0x0B);
    cc_text();
}
DC_ENTRY(C25A4C, RTS)                            /* control 12: next byte 08+ is a dictionary word, else a sub-code */
{
    dc_set_a8(bus8(lp(0x31)));
    dc_set_mx(0, 0); inc16_dp(0x31); dc_set_mx(1, 0);
    u8 c = (u8)g_cpu.A;
    dc_cmp8(c, 0x08);
    if (c >= 0x08) {
        dp8_set(0x3B, c);
        g_cpu._flag_C = 1;
        dc_set_a8(dc_sbc8(c, 0x08));
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        g_cpu.A = asl16(dc_adc16(g_cpu.A, 0x001F));
        dict_word();
        return;
    }
    dc_set_a8(asl8(c));
    g_cpu.X = g_cpu.A;
    u16 to = bus16(0xC25A8A + g_cpu.X);
    if (to == 0x5A8E) dc_C25A8E_glue();
    else if (to == 0x5AD0) dc_C25AD0_glue();
    else bad_jump(0xC25A87, to);
}
DC_ENTRY(C2EE7F, RTS)                            /* fill a window body: tile pattern C2:EEF5 rows, the frame's inner rows */
{
    php();
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    dp16_set(0x65, dc_adc16(dp16(0x63), 0x0042));
    dp16_set(0x08, abs16(0x0D47));
    dp16_set(0x0A, 0);
    for (;;) {
        dp16_set(0x06, dp16(0x8A));
        g_cpu.Y = dp16(0x65);
        g_cpu.X = dp16(0x0A);
        for (;;) {
            dc_set_mx(1, 0);
            dc_set_a8(dc_adc8(bus8(0xC2EEF5 + g_cpu.X), dp8(0x08)));
            abs8_set(g_cpu.Y, (u8)g_cpu.A);
            dc_set_a8(dp8(0x09));
            abs8_set((u16)(g_cpu.Y + 1), (u8)g_cpu.A);
            g_cpu.Y += 2;
            dc_set_mx(0, 0);
            u16 n = (u16)(dp16(0x06) - 1);
            dp16_set(0x06, n);
            if (!n) break;
            g_cpu.A = (u16)(((g_cpu.X + 1) & 0x0003) | dp16(0x0A));
            g_cpu.X = g_cpu.A;
            if (g_cpu.X) continue;
            g_cpu._flag_C = 1;
            g_cpu.X = dc_sbc16(g_cpu.Y, 0x0008);
            g_cpu.A = (u16)((dp16(0x06) << 1) - 1);
            dc_mvn(0x7E, 0x7E);
            break;
        }
        g_cpu._flag_C = 0;
        dp16_set(0x65, dc_adc16(dp16(0x65), 0x0040));
        lda16(dc_adc16(dp16(0x0A), 0x0004) & 0x000C);
        dp16_set(0x0A, g_cpu.A);
        u16 n = (u16)(dp16(0x8C) - 1);
        dp16_set(0x8C, n);
        if (!g_cpu.A) {
            if (n) {
                u16 w = dp16(0x8A);
                g_cpu._flag_C = w >> 15;
                dp16_set(0x8A, (u16)((w << 1) - 1));
                g_cpu.A = dc_adc16(dp16(0x63), 0x0042);
                dp16_set(0x63, g_cpu.A);
                dc_jsr(0xC2EF2B, 0xC2EEF0);
            }
            break;
        }
        if (!n) break;
    }
    plp();
}
DC_ENTRY(C2EDAA, RTS)                            /* EDB0 (frame), then the window body */
{
    dc_jsr(0xC2EDB0, 0xC2EDAA);
    dc_C2EE7F_glue();
}
/* Script ops F0C1/F011: call `fn` with Y = the first word at Y, A = the second; Y += 4. */
static void word_pair_op(u32 fn, u32 site)
{
    dc_push16(g_cpu.Y);
    u16 first = abs16(g_cpu.Y);
    g_cpu.A = abs16((u16)(g_cpu.Y + 2));
    g_cpu.Y = first;
    dc_set_flags_nz16(first);
    dc_jsr(fn, site);
    g_cpu.Y = dc_pull16();
    ldy16((u16)(g_cpu.Y + 4));
}
DC_ENTRY(C2F0C1, RTS) { word_pair_op(0xC2F227, 0xC2F0CA); }   /* script op: F227 on the two words at Y */
DC_ENTRY(C2F011, RTS) { word_pair_op(0xC2F114, 0xC2F01A); }   /* script op: F114 on the two words at Y */
/* New menu object with the script at X (048A); A = script op size. */
static void new_obj(u16 script, u32 site, u16 size)
{
    dc_set_mx(1, 0);
    g_cpu.X = script;
    dc_jsr(0xC2048A, site);
    dc_set_mx(0, 0);
    lda16(size);
}
DC_ENTRY(C210BF, RTS) { new_obj(0x1CF5, 0xC210C4, 0x000A); }
DC_ENTRY(C2158B, RTS) { new_obj(0x20A2, 0xC21590, 0x0002); }
DC_ENTRY(C21599, RTS) { new_obj(0x2105, 0xC2159E, 0x0002); }
DC_ENTRY(C21839, RTS)                            /* script op: new menu object, script from operand 1 */
{
    g_cpu.Y = 1;
    g_cpu.A = bus16(lp(0x58) + 1);
    g_cpu.X = g_cpu.A;
    dc_jsr(0xC2048A, 0xC2183F);
    dc_set_mx(0, 0);
    lda16(0x0003);
}
DC_ENTRY(C210CD, RTS)                            /* script op: new 1DD4 object with position/params from operands 1-5 */
{
    g_cpu.X = dp16(0x4E);
    g_cpu.Y = 1;
    g_cpu.A = bus16(lp(0x58) + 1);
    abs16_set(0x0035 + g_cpu.X, g_cpu.A);
    dc_set_mx(1, 0);
    g_cpu.Y = 3; dc_set_a8(bus8(lp(0x58) + 3)); abs8_set(0x0037 + g_cpu.X, (u8)g_cpu.A);
    g_cpu.Y = 4; dc_set_a8(bus8(lp(0x58) + 4)); abs8_set(0x0032 + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x0033 + g_cpu.X, 0);
    g_cpu.Y = 5; dc_set_a8(bus8(lp(0x58) + 5)); abs8_set(0x0034 + g_cpu.X, (u8)g_cpu.A);
    g_cpu.X = 0x1DD4;
    dc_jsr(0xC2048A, 0xC210F7);
    dc_set_mx(0, 0);
    lda16(0x0006);
}
DC_ENTRY(C204D9, RTS)                            /* new 0F63 object pointing at A:X */
{
    dp8_set(0x01, (u8)g_cpu.A);
    dp16_set(0x0A, g_cpu.X);
    g_cpu.X = 0x0F63;
    dc_jsr(0xC2048A, 0xC204E0);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc16(g_cpu.X, 0x0040);
    g_cpu.X = g_cpu.A;
    g_cpu.A = dp16(0x0A);
    abs16_set(0x0007 + g_cpu.X, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(dp8(0x01));
    abs8_set(0x0009 + g_cpu.X, (u8)g_cpu.A);
    abs8_set(0x000A + g_cpu.X, 0);
}
DC_ENTRY(C24184, RTS)                            /* set the object's bit 7 flag and its animation from the party member */
{
    dc_set_mx(1, 0);
    g_cpu.Y = g_cpu.X;
    dc_set_a8(abs8(0x0024 + g_cpu.Y));
    g_cpu.X = g_cpu.A;
    abs8_set(0x02B1 + g_cpu.X, 0);
    g_cpu.X = g_cpu.Y;
    dc_set_a8(abs8(0x0028 + g_cpu.X) & 0x7F);
    dc_cmp8((u8)g_cpu.A, abs8(0x0027 + g_cpu.X));
    if (g_cpu._flag_Z) {
        lda8(abs8(0x0028 + g_cpu.X));
        if (g_cpu.A & 0x80) return;
    }
    lda8(abs8(0x0028 + g_cpu.X) | 0x80);
    abs8_set(0x0028 + g_cpu.X, (u8)g_cpu.A);
    g_cpu.A = g_cpu.D;
    dc_set_a8(abs8(0x0024 + g_cpu.X));
    g_cpu.X = g_cpu.A;
    lda8(bus8(0x7E2980 + g_cpu.X));
    dp8_set(0x00, (u8)g_cpu.A);
    dc_jsr(0xC23953, 0xC241B1);
}
/* Script ops 1A52/1AAC: pick a flag byte through the C2:1A70 handlers, then set/clear its bit 7. */
static void flag_op(u32 site, bool set)
{
    dc_set_mx(1, 0);
    g_cpu.Y = 1;
    dc_set_a8(asl8(bus8(lp(0x58) + 1)));
    g_cpu.X = g_cpu.A;
    dc_jsr(0xC20000 | bus16(0xC21A70 + g_cpu.X), site);
    dc_set_mx(1, 0);
    dc_set_a8(0x7E);
    dp8_set(0x12, 0x7E);
    u8 v = bus8(lp(0x10));
    lda8(set ? (u8)(v | 0x80) : (u8)(v & 0x7F));
    bus8_set(lp(0x10), (u8)g_cpu.A);
    dc_set_mx(0, 0);
    lda16(0x0003);
}
DC_ENTRY(C21A52, RTS) { flag_op(0xC21A5B, true); }
DC_ENTRY(C21AAC, RTS) { flag_op(0xC21AB5, false); }
DC_ENTRY(C2034D, RTS)                            /* hide all 128 sprites (Y = E0) and clear the high OAM table */
{
    dc_set_mx(1, 1);
    for (int x = 0x60; ; ) {
        for (int k = 0; k < 8; k++)
            for (int r = 0; r < 4; r++) abs8_set((u32)(0x0701 + r * 0x80 + k * 4 + x), 0xE0);
        g_cpu._flag_C = 1;
        g_cpu.A = (u16)((g_cpu.A & 0xFF00) | (u8)x);
        x = dc_sbc8((u8)x, 0x20);
        dc_set_a8((u8)x);
        g_cpu.X = (u8)x;
        if (x & 0x80) break;
    }
    dc_set_mx(0, 0);
    for (u16 a = 0x0900; a < 0x0920; a += 2) abs16_set(a, 0);
    dc_set_mx(1, 0);
}
DC_ENTRY(C2F114, RTS)                            /* BCD sum (F13F) of the A:X bit list, then print it (F1DA) */
{
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(0, 0);
    dc_push16(g_cpu.A);
    plb(); plb();
    dp16_set(0x90, g_cpu.X);
    dc_set_mx(1, 0);
    dc_push8((u8)g_cpu.A);
    dp8_set(0x92, (u8)(g_cpu.A & 0x03));
    dp8_set(0x93, 0);
    dc_jsr(0xC2F13F, 0xC2F126);
    u8 a = dc_pull8() ^ 0x80;
    dp8_set(0x8F, a);
    lda8((a >> 4) & 0x07);
    dp8_set(0x8E, (u8)g_cpu.A);
    dc_push16(g_cpu.Y);
    dc_jsr(0xC2F1DA, 0xC2F137);
    g_cpu.X = g_cpu.Y;
    g_cpu.Y = dc_pull16();
    plp();
    plb();
}
/* One glyph row: pixels [$76],Y in the high byte, the 4-bit plane [$79],Y
 * (high or low nibble by $62 bit 0) in the low byte, shifted by $6A through
 * the 8 tile writers of the table. */
static void glyph_row(u32 tab, const u16 *to8)
{
    g_cpu.Y = dp16(0x60);
    dc_set_a8(bus8(lp(0x76) + g_cpu.Y));
    xba_a();
    dc_set_a8(dp8(0x62));
    u8 c = dp8(0x62);
    g_cpu._flag_C = c & 1;
    u8 p = bus8(lp(0x79) + g_cpu.Y);
    if (c & 1) lda8(asl8(asl8(asl8(asl8(p)))));
    else lda8(p & 0xF0);
    dc_set_mx(0, 0);
    g_cpu.Y = g_cpu.A;
    dp16_set(0x63, 0);
    g_cpu.X = asl16(dp16(0x6A) & 0x00FF);
    g_cpu.A = g_cpu.Y;
    dc_set_flags_nz16(g_cpu.A);
    u16 to = bus16(tab + g_cpu.X);
    int i = 0;
    while (i < 8 && to8[i] != to) i++;
    switch (i) {
    case 0: if (to8[0] == 0x5EBF) dc_C25EBF_glue(); else dc_C25F90_glue(); break;
    case 1: if (to8[0] == 0x5EBF) dc_C25ED2_glue(); else dc_C25FA3_glue(); break;
    case 2: if (to8[0] == 0x5EBF) dc_C25ED1_glue(); else dc_C25FA2_glue(); break;
    case 3: if (to8[0] == 0x5EBF) dc_C25ED0_glue(); else dc_C25FA1_glue(); break;
    case 4: if (to8[0] == 0x5EBF) dc_C25ECF_glue(); else dc_C25FA0_glue(); break;
    case 5: if (to8[0] == 0x5EBF) dc_C25EE5_glue(); else dc_C25FB6_glue(); break;
    case 6: if (to8[0] == 0x5EBF) dc_C25EE8_glue(); else dc_C25FB9_glue(); break;
    case 7: if (to8[0] == 0x5EBF) dc_C25EEB_glue(); else dc_C25FBC_glue(); break;
    default: bad_jump(tab - 3, to);
    }
}
DC_ENTRY(C25E89, RTS)
{
    static const u16 to8[8] = { 0x5EBF, 0x5ED2, 0x5ED1, 0x5ED0, 0x5ECF, 0x5EE5, 0x5EE8, 0x5EEB };
    glyph_row(0xC25EAF, to8);
}
DC_ENTRY(C25F5A, RTS)
{
    static const u16 to8[8] = { 0x5F90, 0x5FA3, 0x5FA2, 0x5FA1, 0x5FA0, 0x5FB6, 0x5FB9, 0x5FBC };
    glyph_row(0xC25F80, to8);
}
DC_ENTRY(C22B61, RTS)                            /* decompress the portraits (C6:FDF0 #0) and copy the party's three */
{
    dc_set_a8(0x7F);
    abs8_set(0x0305, 0x7F);
    g_cpu.X = 0x9000;
    abs16_set(0x0303, 0x9000);
    dc_set_a8(bus8(0xC6FDF2));
    abs8_set(0x0302, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu.A = bus16(0xC6FDF0);
    abs16_set(0x0300, g_cpu.A);
    dc_jsl(0xC30002, 0xC22B7C);
    static const struct { u16 who, y; bool face; u16 site; } part[5] = {
        { 0x2980, 0xB000, false, 0x2B87 }, { 0x2980, 0xBC00, true, 0x2B91 },
        { 0x2981, 0xB400, false, 0x2B9B }, { 0x2981, 0xBC40, true, 0x2BA5 },
        { 0x2982, 0xB800, false, 0x2BAF },
    };
    for (int i = 0; i < 5; i++) {
        g_cpu.A = bus16(0x7E0000 | part[i].who);
        g_cpu.Y = part[i].y;
        dc_jsr(part[i].face ? 0xC22BCF : 0xC22BBC, 0xC20000 | part[i].site);
    }
    lda16(bus16(0x7E2982));
    g_cpu.Y = 0xBC80;
    dc_C22BCF_glue();
}
DC_ENTRY(C20961, RTS)                            /* queue two 40-byte VRAM uploads from [$E0] for map row $0A */
{
    u16 a = asl16(asl16(dp16(0x0A) & 0x00F8));
    a = dc_adc16(a, dp16(0x16));
    dp16_set(0x08, a);
    g_cpu._flag_C = 0;
    dp16_set(0x0A, dc_adc16(a, 0x0400));
    dc_set_mx(1, 1);
    abs8_set(0x005E, (u8)(abs8(0x005E) + 1));
    g_cpu.X = abs8(0x005F);
    abs8_set(0x0062 + g_cpu.X, 0x7E);
    abs8_set(0x006A + g_cpu.X, 0x7E);
    abs8_set(0x0067 + g_cpu.X, 0x80);
    abs8_set(0x006F + g_cpu.X, 0x80);
    dc_set_mx(0, 1);
    abs16_set(0x0065 + g_cpu.X, 0x0040);
    abs16_set(0x006D + g_cpu.X, 0x0040);
    a = abs16(0x00E0);
    abs16_set(0x0060 + g_cpu.X, a);
    g_cpu._flag_C = 0;
    abs16_set(0x0068 + g_cpu.X, dc_adc16(a, 0x0040));
    abs16_set(0x0063 + g_cpu.X, dp16(0x08));
    g_cpu.A = dp16(0x0A);
    abs16_set(0x006B + g_cpu.X, g_cpu.A);
    dc_set_mx(1, 1);
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8((u8)g_cpu.X, 0x10));
    abs8_set(0x005F, (u8)g_cpu.A);
    abs8_set(0x005E, 0);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(abs16(0x00E0), 0x0080);
    abs16_set(0x00E0, g_cpu.A);
}
DC_ENTRY(C2FA49, RTS)                            /* reset the text window buffer and start text 0010 of the FA9F list */
{
    php();
    dc_set_mx(1, 0);
    abs8_set(0x0214, 0);
    dc_set_a8(0x7E);
    abs8_set(0x0212, 0x7E);
    abs8_set(0x0DC3, 0);
    dc_set_mx(0, 0);
    abs16_set(0x0DC1, 0);
    u16 b = abs16(0x0DC5) & 0xFFC0;
    g_cpu.X = b;
    abs16_set(g_cpu.X, 0);
    g_cpu.Y = (u16)(b + 2);
    g_cpu.A = (u16)(abs16(0x0DD0) - 3);
    dc_mvn(0x7E, 0x7E);
    abs16_set(0x0210, 0x5E00);
    abs16_set(0x0DC7, 0x3101);
    abs16_set(0x0DD2, 0x0008);
    abs16_set(0x0DD4, 0x7800);
    abs16_set(0x0DD6, 0x5E00);
    lda16(0xFA9F);
    ldx16(0x0010);
    dc_jsr(0xC28249, 0xC2FA96);
    dc_jsl(0xC20003, 0xC2FA99);
    plp();
}
DC_ENTRY(C21100, RTS)                            /* script op: save the camera, new scroll target and mode from operands 1-4 */
{
    g_cpu.A = abs16(0x0100);
    abs16_set(0x0105, g_cpu.A);
    abs16_set(0x04FC, g_cpu.A);
    g_cpu._flag_C = 0;
    u16 a = dc_adc16(dp16(0xE5), 0x0010);
    dc_cmp16(a, 0x00C0);
    if (g_cpu._flag_C) a = dc_sbc16(a, 0x00C0);
    abs16_set(0x0107, a);
    abs16_set(0x04FE, a);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(dp16(0xE9), 0x0010) & 0x007F;
    dc_set_mx(1, 0);
    abs8_set(0x0108, (u8)g_cpu.A);
    abs8_set(0x04FF, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu.Y = 1;
    g_cpu.A = bus16(lp(0x58) + 1) & 0x01FF;
    abs16_set(0x0100, g_cpu.A);
    dc_set_mx(1, 0);
    g_cpu.Y = 2; u8 m = bus8(lp(0x58) + 2); g_cpu._flag_C = m & 1;
    dc_set_a8((u8)(m >> 1)); abs8_set(0x0104, (u8)g_cpu.A);
    g_cpu.Y = 3; dc_set_a8(bus8(lp(0x58) + 3)); abs8_set(0x0102, (u8)g_cpu.A);
    g_cpu.Y = 4; dc_set_a8(bus8(lp(0x58) + 4)); abs8_set(0x0103, (u8)g_cpu.A);
    g_cpu.A = g_cpu.D;
    dc_set_a8(abs8(0x0104) & 0x03);
    g_cpu.X = g_cpu.A;
    abs8_set(0x0109, bus8(0xC22509 + g_cpu.X));
    dc_set_a8(0x02);
    abs8_set(0x027C, 0x02);
    dc_set_mx(0, 0);
    lda16(0x0005);
}
DC_ENTRY(C2D466, RTS)                            /* 2x2 icon $00 in palette $24 at row $61; its tiles from FF:CC84 */
{
    php();
    dc_set_mx(1, 0);
    g_cpu.Y = dp16(0x61);
    u8 pal = (u8)(asl8(asl8(dp8(0x24))) | 0x11);
    abs8_set(0x005F + g_cpu.Y, pal);
    abs8_set(0x0061 + g_cpu.Y, pal);
    abs8_set(0x009F + g_cpu.Y, pal);
    abs8_set(0x00A1 + g_cpu.Y, pal);
    dp8_set(0x01, 0);
    g_cpu.X = dp16(0x00);
    u8 t = bus8(0xC2D4B4 + g_cpu.X);
    abs8_set(0x005E + g_cpu.Y, t);
    abs8_set(0x0060 + g_cpu.Y, (u8)(t + 1));
    abs8_set(0x009E + g_cpu.Y, (u8)(t + 2));
    abs8_set(0x00A0 + g_cpu.Y, (u8)(t + 3));
    dc_set_mx(0, 0);
    u16 a = asl16(asl16(asl16(asl16(asl16(dp16(0x00))))));
    g_cpu.X = dc_adc16(a, 0xCC84);
    a = asl16(asl16(asl16(asl16(asl16(dp16(0x24))))));
    g_cpu.Y = dc_adc16(a, 0x9500);
    g_cpu.A = 0x001F;
    dc_mvn(0x7E, 0xFF);
    plp();
}
DC_ENTRY(C284EC, RTS)                            /* read the joypad: held $F2, new presses $F0/$F4 with auto-repeat ($FD/$FC) */
{
    dc_push16(0x0000);
    g_cpu.D = dc_pull16();
    dc_push16(0x0000);
    plb(); plb();
    dc_set_mx(1, 0);
    dc_set_a8(0x01);
    u8 st;
    while ((st = abs8(0x4212)) & 0x01) dc_spend(56);
    g_cpu._flag_Z = 1; g_cpu._flag_N = st >> 7; g_cpu._flag_V = (st >> 6) & 1;
    g_cpu.X = dp16(0xF2);
    dp16_set(0xFE, g_cpu.X);
    dp8_set(0xF2, abs8(0x4218) & 0xF0);
    dp8_set(0xF3, abs8(0x4219) & 0x0F);
    u8 hi = abs8(0x4219) >> 4;
    dc_set_a8(hi);
    dp8_set(0xF2, dp8(0xF2) | hi);
    dc_set_mx(0, 0);
    g_cpu.A = abs16(0x4218);
    dc_cmp16(g_cpu.A, 0x3030);
    if (g_cpu._flag_Z) {                         /* L+R+Select+Start: soft reset (JML, never returns) */
        dc_jsl(0xC0FF00, 0xC28523);
        return;
    }
    lda16((dp16(0xF2) ^ dp16(0xFE)) & dp16(0xF2));
    dp16_set(0xF0, g_cpu.A);
    dp16_set(0xF4, g_cpu.A);
    dc_set_mx(0, 1);
    g_cpu.X = dp8(0xFD);
    lda16(dp16(0xF2) & dp16(0xFE));
    if (g_cpu.A) {
        g_cpu.X = (u8)(g_cpu.X - 1);
        dc_set_flags_nz8((u8)g_cpu.X);
        if (g_cpu.X) { dp8_set(0xFD, (u8)g_cpu.X); return; }
        u16 f4 = dp16(0xF4);
        g_cpu._flag_Z = (f4 & g_cpu.A) == 0;
        dp16_set(0xF4, f4 | g_cpu.A);
    }
    g_cpu.X = dp8(0xFC);
    dc_set_flags_nz8((u8)g_cpu.X);
    dp8_set(0xFD, (u8)g_cpu.X);
}
DC_ENTRY(C28545, RTS)                            /* the three characters' equipment-fit bit masks (8555 x3) */
{
    dc_set_mx(1, 1);
    dc_push16(0x7E7E);
    plb(); plb();
    g_cpu.Y = 0;
    g_cpu.X = 0;
    dc_jsr(0xC28555, 0xC2854F);
    dc_jsr(0xC28555, 0xC28552);
    dc_C28555_glue();
}
/* One 64-byte VRAM row (two 40-byte halves) by DMA channel 7. */
DC_ENTRY(C20ADC, RTS)                            /* upload map column $0C (two halves from [$E0]) to VRAM $10 */
{
    u16 a = lsr16(lsr16(lsr16(dp16(0x0C))));
    dc_cmp16(a, 0x0020);
    if (g_cpu._flag_C) { g_cpu._flag_C = 0; a = dc_adc16(a, 0x03E0); }
    g_cpu._flag_C = 0;
    a = dc_adc16(a, dp16(0x10));
    dp16_set(0x12, a);
    g_cpu.X = a;
    dc_set_mx(1, 0);
    dc_push8(g_cpu.DB);
    g_cpu.A = g_cpu.D;
    dc_push8((u8)g_cpu.A);
    plb();
    abs16_set(0x2116, g_cpu.X);
    abs8_set(0x2115, 0x81);
    abs8_set(0x4370, 0x01);
    abs8_set(0x4371, 0x18);
    g_cpu.X = dp16(0xE0);
    abs16_set(0x4372, g_cpu.X);
    abs8_set(0x4374, dp8(0xE2));
    g_cpu.X = 0x0040;
    abs16_set(0x4375, 0x0040);
    abs8_set(0x420B, 0x80);
    dc_set_mx(0, 0);
    abs16_set(0x2116, (u16)(dp16(0x12) + 1));
    g_cpu._flag_C = 0;
    abs16_set(0x4372, dc_adc16(dp16(0xE0), 0x0040));
    g_cpu.A = dp16(0xE2);
    abs16_set(0x4374, g_cpu.A);
    dc_set_mx(1, 0);
    abs8_set(0x2115, 0x81);
    abs8_set(0x4370, 0x01);
    abs8_set(0x4371, 0x18);
    abs16_set(0x4375, 0x0040);
    dc_set_a8(0x80);
    abs8_set(0x420B, 0x80);
    dc_set_mx(0, 0);
    plb();
}
DC_ENTRY(C20C4D, RTS)                            /* lay out the 8x8 block of 4-tile cells from 7E:B400 into B000 */
{
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    dc_push8(0x7E);
    plb();
    dc_set_mx(0, 0);
    g_cpu.X = 0xB500;
    abs16_set(0xB500, 0xB500);
    abs16_set(0xB502, 0xB500);
    g_cpu.X = 0xB504;
    abs16_set(0xB504, 0xB3F0);
    abs16_set(0xB506, 0xB000);
    g_cpu.X = 0xB400;
    g_cpu.Y = 0xB000;
    dp16_set(0x08, 0x0008);
    do {
        for (int r = 0; r < 8; r++) {
            abs16_set((u16)(g_cpu.Y + r * 0x10), abs16((u16)(g_cpu.X + r * 4)));
            abs16_set((u16)(g_cpu.Y + r * 0x10 + 2), abs16((u16)(g_cpu.X + r * 4 + 2)));
        }
        g_cpu._flag_C = 0;
        g_cpu.X = dc_adc16(g_cpu.X, 0x0020);
        g_cpu._flag_C = 0;
        g_cpu.Y = g_cpu.A = dc_adc16(g_cpu.Y, 0x0080);
        dp16_set(0x08, (u16)(dp16(0x08) - 1));
    } while (dp16(0x08));
    plb();
}
DC_ENTRY(C20BF6, RTS)                            /* the 7E:B400 pointer list: B504, then B000/B010 + 0x10 steps */
{
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    dc_push8(0x7E);
    plb();
    dc_set_mx(0, 0);
    abs16_set(0xB400, 0xB504);
    for (int h = 0; h < 2; h++) {
        g_cpu.X = h ? 0xB402 : 0xB404;
        dp16_set(0x08, h ? 0xB010 : 0xB000);
        for (g_cpu.Y = 0x3F; g_cpu.Y; g_cpu.Y--) {
            abs16_set(g_cpu.X, dp16(0x08));
            g_cpu.X += 4;
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(dp16(0x08), 0x0010);
            dp16_set(0x08, g_cpu.A);
        }
    }
    g_cpu.A = 0xB504;
    abs16_set(g_cpu.X, 0xB504);
    plb();
}
/* |v| into the divider (high byte first, / 0x30). */
static void div_abs(u16 v)
{
    if (v & 0x8000) v = (u16)((v ^ 0xFFFF) + 1);
    g_cpu.A = xba16(v);
    abs16_set(0x4204, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(0x30);
    abs8_set(0x4206, 0x30);
    dc_set_mx(0, 0);
}
DC_ENTRY(C253D3, RTS)                            /* object speed toward its target (2A/2C) in 1/48 steps */
{
    dc_set_mx(0, 0);
    g_cpu.X = dp16(0x4E);
    g_cpu._flag_C = 1;
    u16 dx = dc_sbc16(abs16(0x002A + g_cpu.X), abs16(0x0014 + g_cpu.X));
    dp16_set(0x08, dx);
    div_abs(dx);
    abs16_set(0x001A + g_cpu.X, 0);
    abs16_set(0x001C + g_cpu.X, 0);
    abs16_set(0x001E + g_cpu.X, 0);
    abs16_set(0x0020 + g_cpu.X, 0);
    abs16_set(0x001B + g_cpu.X, abs16(0x4214));
    g_cpu._flag_C = 1;
    u16 dy = dc_sbc16(abs16(0x002C + g_cpu.X), abs16(0x0018 + g_cpu.X));
    dp16_set(0x0A, dy);
    div_abs(dy);
    lda16(dp16(0x08));
    if (g_cpu.A & 0x8000) dc_jsr(0xC21C84, 0xC25429);
    g_cpu.A = abs16(0x4214);
    abs16_set(0x001F + g_cpu.X, g_cpu.A);
    lda16(dp16(0x0A));
    if (g_cpu.A & 0x8000) dc_jsr(0xC21C9E, 0xC25436);
    dc_set_mx(1, 0);
}
DC_ENTRY(C25D56, RTS)                            /* drop leading zero digits of 40-47; A = digits */
{
    dc_set_mx(1, 0);
    g_cpu.Y = 8;
    static const u8 n[3] = { 7, 6, 5 };
    for (int s = 0; s < 3; s++) {
        lda8(dp8(0x40));
        if ((u8)g_cpu.A) { lda8((u8)g_cpu.Y); return; }
        for (int i = 0; i < n[s]; i++) dp8_set((u8)(0x40 + i), dp8((u8)(0x41 + i)));
        static const u8 lastx[3] = { 0x45, 0x44, 0x43 };
        g_cpu.X = (u16)(dp8(lastx[s]) | dp8((u8)(lastx[s] + 1)) << 8);
        if (s < 2) g_cpu.Y--;
    }
    dc_C25D91_glue();
}
DC_ENTRY(C2EC38, RTS)                            /* portrait frame: 6 rows of 6 tiles at $61, palette from FF:CE78 */
{
    php();
    dc_set_mx(1, 1);
    g_cpu.X = abs8(0x0D46);
    dp8_set(0x00, bus8(0xFFCE70 + g_cpu.X));
    g_cpu.Y = 6;
    dc_set_mx(0, 0);
    g_cpu.X = dp16(0x61);
    do {
        u16 a = dp16(0x00);
        for (int i = 0; i < 6; i++) abs16_set((u16)(g_cpu.X + 2 * i), (u16)(a + i));
        dp16_set(0x00, (u16)(a + 6));
        g_cpu._flag_C = 0;
        g_cpu.X = dc_adc16(g_cpu.X, 0x0040);
    } while (--g_cpu.Y);
    g_cpu.X = asl16(abs16(0x0D46) & 0x0007);
    g_cpu.X = bus16(0xFFCE78 + g_cpu.X);
    u16 a = asl16(asl16(asl16(dp16(0x01) & 0x001C)));
    g_cpu.Y = dc_adc16(a, 0x9480);
    g_cpu.A = 0x001F;
    dc_mvn(0x7E, 0xFF);
    abs16_set(0x0D15, (u16)(abs16(0x0D15) + 1));
    plp();
}
/* 30 tilemap entries of the vertical map strip: tile words at +even/+odd of
 * the 8-byte record ($10 + index*8) for even/odd rows. */
static void map_strip_v(u16 even, u16 odd, u32 site)
{
    dc_jsr(0xC20811, site);
    dc_set_mx(1, 0);
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    dc_push8(0x7E);
    plb();
    dc_set_mx(0, 0);
    dp16_set(0x19, 0x001E);
    do {
        g_cpu.Y = dp16(0x04);
        u16 a = asl16(asl16(asl16(bus16(dba(dp16(0x02)) + g_cpu.Y) & 0x00FF)));
        g_cpu.X = dc_adc16(a, dp16(0x10));
        g_cpu.Y = dp16(0x0A);
        if (dp16(0x08) & 1) {
            bus16_set(dba(dp16(0x06)) + g_cpu.Y, abs16(odd + g_cpu.X));
            g_cpu._flag_C = 0;
            a = dc_adc16(dp16(0x04), 0x0060);
            dc_cmp16(a, 0x1800);
            if (g_cpu._flag_C) a = g_cpu.D;
            dp16_set(0x04, a);
        } else {
            g_cpu._flag_C = 0;
            bus16_set(dba(dp16(0x06)) + g_cpu.Y, abs16(even + g_cpu.X));
        }
        g_cpu.A = (u16)((g_cpu.Y + 2) & 0x003F);
        dp16_set(0x0A, g_cpu.A);
        dp16_set(0x08, (u16)(dp16(0x08) + 1));
        dp16_set(0x19, (u16)(dp16(0x19) - 1));
    } while (dp16(0x19));
    plb();
}
DC_ENTRY(C20773, RTS) { map_strip_v(0, 4, 0xC20773); }
DC_ENTRY(C207C2, RTS) { map_strip_v(2, 6, 0xC207C2); }
DC_ENTRY(C20043, RTS)                            /* PPU and CPU I/O registers to their menu defaults */
{
    dc_push16(g_cpu.D);
    g_cpu.D = 0x2100;
    static const u8 ppu[][2] = {
        {0x01,0x60}, {0x02,0x00}, {0x03,0x00}, {0x05,0x09}, {0x06,0x00}, {0x07,0x61}, {0x08,0x69},
        {0x09,0x79}, {0x0A,0x79}, {0x0B,0x22}, {0x0C,0x77},
        {0x0D,0x00}, {0x0D,0x00}, {0x0E,0x00}, {0x0E,0x00}, {0x0F,0x00}, {0x0F,0x00}, {0x10,0x00}, {0x10,0x00},
        {0x11,0x00}, {0x11,0x00}, {0x12,0x00}, {0x12,0x00}, {0x13,0x00}, {0x13,0x00}, {0x14,0x00}, {0x14,0x00},
        {0x15,0x80}, {0x16,0x00}, {0x17,0x00}, {0x1A,0x00}, {0x1B,0x00}, {0x1B,0x01},
        {0x1C,0x00}, {0x1C,0x00}, {0x1D,0x00}, {0x1D,0x00}, {0x1E,0x00}, {0x1E,0x01}, {0x1F,0x00}, {0x1F,0x00},
        {0x20,0x00}, {0x20,0x00}, {0x21,0x00}, {0x23,0x00}, {0x24,0x00}, {0x25,0x00}, {0x26,0x00}, {0x27,0x00},
        {0x28,0x00}, {0x29,0x00}, {0x2A,0x00}, {0x2B,0x00}, {0x2C,0x17}, {0x2D,0x13}, {0x2E,0x00}, {0x2F,0x00},
        {0x30,0x02}, {0x31,0x44}, {0x32,0xE0}, {0x33,0x00},
    };
    for (unsigned i = 0; i < sizeof ppu / sizeof ppu[0]; i++) dp8_set(ppu[i][0], ppu[i][1]);
    g_cpu.X = 0;
    dc_push16(0x4200);
    g_cpu.D = dc_pull16();
    dp8_set(0x01, 0xFF);
    for (u8 r = 0x02; r <= 0x0A; r++) dp8_set(r, 0x00);
    dc_set_a8(0x00);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
DC_ENTRY(C24205, RTS)                            /* C = a wall (4-bit cell & 3) between $05 and $07 on row $06-1 */
{
    dp16_set(0x10, 0x5800); dp8_set(0x12, 0x7E);
    dp16_set(0x13, 0x7000); dp8_set(0x15, 0x7E);
    g_cpu.X = 0x7000;
    dc_set_a8(0x7E);
    dp8_set(0x06, (u8)(dp8(0x06) - 1));
    g_cpu._flag_C = 1;
    u8 d = dc_sbc8(dp8(0x07), dp8(0x05));
    dc_set_a8(d);
    if (d) {
        bool up = !(d & 0x80);
        do {
            dp8_set(0x05, (u8)(dp8(0x05) + (up ? 1 : -1)));
            dc_set_a8(dp8(0x05));
            dp8_set(0x00, (u8)g_cpu.A);
            dc_set_a8(dp8(0x06));
            dp8_set(0x01, (u8)g_cpu.A);
            dc_jsr(0xC262AE, up ? 0xC2422C : 0xC24245);
            lda8((u8)(g_cpu.A & 0x03));
            if ((u8)g_cpu.A) {
                u8 r = (u8)(dp8(0x06) + 1); dp8_set(0x06, r); dc_set_flags_nz8(r);
                g_cpu._flag_C = 1;
                return;
            }
            dc_set_a8(dp8(0x05));
            dc_cmp8((u8)g_cpu.A, dp8(0x07));
        } while (!g_cpu._flag_Z);
    }
    u8 r = (u8)(dp8(0x06) + 1); dp8_set(0x06, r); dc_set_flags_nz8(r);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C261BD, RTS)                            /* split the 24-bit $68 into eight decimal digits at $60-$67 */
{
    dc_set_mx(1, 0);
    g_cpu.Y = 1;
    static const struct { u16 lo; u8 hi; u8 dst; u16 site; } d[7] = {
        { 0x9680, 0x98, 0x60, 0x61CB }, { 0x1200, 0x7A, 0x61, 0x61DB }, { 0x3500, 0x0C, 0x62, 0x61EB },
        { 0x3880, 0x01, 0x63, 0x61FB }, { 0x1F40, 0x00, 0x64, 0x6209 }, { 0x0320, 0x00, 0x65, 0x6217 },
        { 0x0050, 0x00, 0x66, 0x6225 },
    };
    for (int i = 0; i < 7; i++) {
        g_cpu.X = d[i].lo;
        dp16_set(0x6C, d[i].lo);
        if (d[i].hi) dc_set_a8(d[i].hi);
        dp8_set(0x6E, d[i].hi);
        dc_jsr(0xC26231, 0xC20000 | d[i].site);
        g_cpu.X = dp16(0x6B);
        dp16_set(d[i].dst, g_cpu.X);
    }
    lda8(dp8(0x68));
    dp8_set(0x67, (u8)g_cpu.A);
}
DC_ENTRY(C29295, RTS)                            /* max HP with the accessory bonus (9ABA A0/A1: +1/4, +1/2), then stat bonuses */
{
    dc_set_mx(0, 1);
    g_cpu.X = abs8(0x9ABA);
    u16 a = abs16(0x9A95);
    if (g_cpu.X == 0xA1) a = lsr16(a);
    else if (g_cpu.X == 0xA0) a = lsr16(lsr16(a));
    else a = lsr16(lsr16(0));
    g_cpu._flag_C = 0;
    a = dc_adc16(a, abs16(0x9A95));
    dc_cmp16(a, 0x03E7);
    if (g_cpu._flag_C) a = 0x03E7;
    g_cpu.A = a;
    abs16_set(0x9B25, a);
    dc_cmp16(a, abs16(0x9A93));
    if (!g_cpu._flag_C) abs16_set(0x9A93, a);
    php();
    dc_set_mx(0, 1);
    lda16(abs16(0x7B00 + g_cpu.X) & 0x00FF);
    if (g_cpu.A) {
        g_cpu.X = (u8)asl16(g_cpu.A);
        g_cpu.A = bus16(0xCC29D7 + g_cpu.X) & 0xFFFE;
        dp16_set(0x00, g_cpu.A);
        dc_set_mx(1, 1);
        g_cpu.X = 0;
        do {
            u8 m = dp8(0x00);
            dp8_set(0x00, (u8)(m << 1));
            if (m & 0x80) {
                g_cpu.Y = bus8(0xFFCF2C + g_cpu.X);
                g_cpu._flag_C = 0;
                dc_set_a8(dc_adc8(dp8(0x01), abs8(0x0DAE + g_cpu.Y)));
                abs8_set(0x0DAE + g_cpu.Y, (u8)g_cpu.A);
            }
            g_cpu.X = (u8)(g_cpu.X + 1);
            lda8(dp8(0x00));
        } while ((u8)g_cpu.A);
    }
    plp();
}
DC_ENTRY(C22882, RTS)                            /* decompress the character graphics: one by [$ED]+0A, then the party's three */
{
    g_cpu.X = 0x8C00;
    abs16_set(0x0303, 0x8C00);
    dc_set_a8(0x7F);
    abs8_set(0x0305, 0x7F);
    g_cpu.Y = 0x000A;
    g_cpu.A = g_cpu.D;
    u8 n = bus8(lp(0xED) + 0x0A);
    dc_set_a8(n);
    if (!(n & 0x80)) {
        dc_set_a8(dc_adc8(asl8(n), bus8(lp(0xED) + 0x0A)));
        g_cpu.X = g_cpu.A;
        dc_set_mx(0, 0);
        g_cpu.A = bus16(0xC6FEA0 + g_cpu.X);
        abs16_set(0x0300, g_cpu.A);
        dc_set_mx(1, 0);
        dc_set_a8(bus8(0xC6FEA2 + g_cpu.X));
        abs8_set(0x0302, (u8)g_cpu.A);
        dc_jsl(0xC30002, 0xC228AB);
    }
    dc_set_mx(0, 0);
    g_cpu.A = bus16(0xC6FEA0);
    abs16_set(0x0300, g_cpu.A);
    abs16_set(0x0303, 0x8E00);
    dc_set_mx(1, 0);
    dc_set_a8(bus8(0xC6FEA2));
    abs8_set(0x0302, (u8)g_cpu.A);
    dc_set_a8(0x7F);
    abs8_set(0x0305, 0x7F);
    dc_jsl(0xC30002, 0xC228CC);
    dc_set_mx(0, 0);
    g_cpu.A = bus16(0x7E2980); g_cpu.Y = 0x8D80; dc_jsr(0xC228ED, 0xC228D9);
    g_cpu.A = bus16(0x7E2981); g_cpu.Y = 0x8DA0; dc_jsr(0xC228ED, 0xC228E3);
    lda16(bus16(0x7E2982)); g_cpu.Y = 0x8DC0;
    dc_C228ED_glue();
}
DC_ENTRY(C211F6, RTS)                            /* script op: new 0F63 object for the pointer in operands 1-3 */
{
    g_cpu.Y = 1;
    g_cpu.A = bus16(lp(0x58) + 1);
    g_cpu.X = g_cpu.A;
    dc_set_mx(1, 0);
    g_cpu.Y = 3;
    dc_set_a8(bus8(lp(0x58) + 3));
    dc_jsr(0xC204D9, 0xC21203);
    dc_set_mx(0, 0);
    lda16(0x0004);
}
DC_ENTRY(C233AA, RTS)
{
    dc_jsr(0xC2626E, 0xC233AA);
    dc_set_a8(0x7F);
    g_cpu.X = abs16(0x1B45);
    dc_jsr(0xC204D9, 0xC233B2);
    g_cpu.X = 0;
    abs16_set(0x1B3B, 0); abs16_set(0x1B3D, 0); abs16_set(0x1B3F, 0);
    g_cpu.X = abs16(0x1B34);
    dc_set_a8(bus8(0x7E8300 + g_cpu.X) & 0x7F);
    bus8_set(0x7E8300 + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01);
    abs8_set(0x0280, 0x01);
}
DC_ENTRY(C233D4, RTS)
{
    dc_jsr(0xC26263, 0xC233D4);
    dc_jsr(0xC2626E, 0xC233D7);
    dc_set_a8(0x7F);
    g_cpu.X = abs16(0x1B45);
    dc_jsr(0xC204D9, 0xC233DF);
    g_cpu.X = 0;
    abs16_set(0x1B3B, 0); abs16_set(0x1B3D, 0); abs16_set(0x1B3F, 0);
    g_cpu.X = abs16(0x1B34);
    dc_set_a8(bus8(0x7E8300 + g_cpu.X) & 0x7F);
    bus8_set(0x7E8300 + g_cpu.X, (u8)g_cpu.A);
    lda8(0x01);
    abs8_set(0x0280, 0x01);
    abs8_set(0x1B43, 0);
}
DC_ENTRY(C2E5D3, RTS)                            /* open text window $54+1 (FA49) unless it is already the one shown */
{
    php();
    dc_set_mx(1, 0);
    u8 a = (u8)(dp8(0x54) + 1);
    dc_set_a8(a);
    dc_cmp8(a, abs8(0x0D77));
    if (!g_cpu._flag_Z) {
        abs8_set(0x0D77, a);
        abs8_set(0x020C, a);
        abs16_set(0x0DC5, 0x5248);
        abs16_set(0x0DCC, 0x6A20);
        abs16_set(0x020D, 0xCF3B);
        dc_set_a8(0xFF);
        abs8_set(0x020F, 0xFF);
        dc_set_a8(0x02);
        abs8_set(0x0DC9, 0x02);
        g_cpu.X = 0x0200;
        abs16_set(0x0DD0, 0x0200);
        dc_jsr(0xC2FA49, 0xC2E606);
    }
    plp();
}
DC_ENTRY(C2EDB6, RTS)                            /* EDD3, then a window body of $5F x $60 */
{
    php();
    dc_set_mx(0, 0);
    dc_jsr(0xC2EDD3, 0xC2EDB9);
    dp16_set(0x8A, (u16)((dp16(0x5F) & 0x001F) - 2));
    g_cpu.A = (u16)((dp16(0x60) & 0x003F) - 2);
    dp16_set(0x8C, g_cpu.A);
    dc_jsr(0xC2EE7F, 0xC2EDCE);
    plp();
}
DC_ENTRY(C29261, RTS)                            /* run the handler of menu mode 0DAD (C2:926E table) */
{
    php();
    dc_set_mx(1, 1);
    dc_set_a8(asl8(abs8(0x0DAD)));
    g_cpu.X = (u8)g_cpu.A;
    dc_jsr(0xC20000 | bus16(0xC2926E + g_cpu.X), 0xC29269);
    plp();
}
DC_ENTRY(C2ECDB, RTS)                            /* full window in the configured colour (0D8C) at row $79 */
{
    dc_push8(g_cpu.DB);
    php();
    dc_set_mx(1, 0);
    dc_push16(0x7E7E);
    plb(); plb();
    u8 a = asl8(asl8(abs8(0x0D8C)));
    u8 b = asl8(asl8(a));
    dc_set_a8(dc_adc8(b, a));
    abs8_set(0x0D47, (u8)g_cpu.A);
    dc_set_a8(a);
    dc_set_a8((u8)(asl8(asl8((u8)(dp8(0x79) + 1))) | 0x03));
    abs8_set(0x0D48, (u8)g_cpu.A);
    dc_jsr(0xC2EDF6, 0xC2ECFC);
    dc_jsr(0xC2EE7F, 0xC2ECFF);
    dc_jsr(0xC2ED08, 0xC2ED02);
    plp();
    plb();
}
DC_ENTRY(C2F28D, RTS)                            /* "nn-nn" from the BCD bytes at 22/23 (default 63/3B, or F2CC twice) */
{
    php();
    dc_set_mx(1, 0);
    dp8_set(0x22, 0x63);
    dc_set_a8(0x3B);
    dp8_set(0x23, 0x3B);
    lda8(abs8(0x0004 + g_cpu.Y));
    if (!(u8)g_cpu.A) {
        dc_jsr(0xC2F2CC, 0xC2F29D);
        dp8_set(0x23, (u8)g_cpu.A);
        dc_jsr(0xC2F2CC, 0xC2F2A2);
        dp8_set(0x22, (u8)g_cpu.A);
    }
    dc_set_mx(0, 0);
    g_cpu.Y = 0x0022;
    g_cpu.A = 0x7E91;
    dc_jsr(0xC2F114, 0xC2F2AF);
    g_cpu.A = (u16)((dp16(0x7D) & 0xFF00) | 0x002D);
    abs16_set(g_cpu.X, g_cpu.A);
    g_cpu.X += 2;
    dc_set_mx(0, 0);
    g_cpu.Y = 0x0023;
    g_cpu.A = 0x7E91;
    dc_jsr(0xC2F114, 0xC2F2C7);
    plp();
}
DC_ENTRY(C2F0D1, RTS)                            /* script op: number at [word 1] if below the FF:CF33 limit, else "//" */
{
    dc_push8(g_cpu.DB);
    dc_push16(g_cpu.Y);
    dc_push16(g_cpu.X);
    g_cpu.X = abs16(g_cpu.Y) & 0x00FF;
    g_cpu.A = abs16((u16)(g_cpu.Y + 1));
    g_cpu.Y = g_cpu.A;
    dc_set_mx(1, 0);
    dc_push16(0x7E7E);
    plb(); plb();
    dc_set_a8(abs8(g_cpu.Y));
    dc_cmp8((u8)g_cpu.A, bus8(0xFFCF33 + g_cpu.X));
    ldx16(dc_pull16());
    if (!g_cpu._flag_C) {
        dc_set_a8(0x7E);
        xba_a();
        dc_set_a8(0x11);
        dc_jsr(0xC2F114, 0xC2F0F5);
    } else {
        dc_set_a8(0x2F);
        abs8_set(g_cpu.X, 0x2F);
        abs8_set((u16)(g_cpu.X + 2), 0x2F);
        dc_set_a8(dp8(0x7E));
        abs8_set((u16)(g_cpu.X + 1), (u8)g_cpu.A);
        abs8_set((u16)(g_cpu.X + 3), (u8)g_cpu.A);
        g_cpu.X += 4;
    }
    g_cpu.Y = dc_pull16();
    plb();
    ldy16((u16)(g_cpu.Y + 3));
}
/* One character of 16x12 glyph rows (5E89 / 5F5A) into the tile buffer. */
static void glyph_char(u32 row, u16 skip, u32 s1, u32 s2, u32 s3, u32 s4)
{
    dc_push8(g_cpu.DB);
    dc_set_a8(0xC2);
    dc_push8(0xC2);
    plb();
    for (int part = 0; part < 2; part++) {
        lda8(part ? 0x08 : 0x04);
        dp8_set(0x68, (u8)g_cpu.A);
        if (!part) { dp8_set(0x60, 0); dp8_set(0x61, 0); }
        do {
            for (int k = 0; k < 2; k++) {
                dc_jsr(row, k ? (part ? s4 : s2) : (part ? s3 : s1));
                dc_set_mx(0, 0);
                dp16_set(0x60, (u16)(dp16(0x60) + 1));
                dp16_set(0x73, (u16)(dp16(0x73) + 1));
                dc_set_mx(1, 0);
            }
            u8 n = (u8)(dp8(0x68) - 1);
            dp8_set(0x68, n);
            dc_set_flags_nz8(n);
        } while (dp8(0x68));
        if (!part) {
            dc_set_mx(0, 0);
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(dp16(0x73), skip);
            dp16_set(0x73, g_cpu.A);
            dc_set_mx(1, 0);
        }
    }
    plb();
}
DC_ENTRY(C25E36, RTS) { glyph_char(0xC25E89, 0x00F0, 0xC25E43, 0xC25E4E, 0xC25E6D, 0xC25E78); }
DC_ENTRY(C25F07, RTS) { glyph_char(0xC25F5A, 0x01F0, 0xC25F14, 0xC25F1F, 0xC25F3E, 0xC25F49); }
DC_ENTRY(C20B53, RTS)                            /* take a free sprite node (B504 list) and link it into the B500 list by depth */
{
    dc_set_mx(1, 0);
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    dc_push8(0x7E);
    plb();
    dc_set_mx(0, 0);
    g_cpu.Y = 0xB504;
    g_cpu.X = abs16(0xB506);
    g_cpu.Y = abs16(g_cpu.X);
    g_cpu.A = abs16((u16)(g_cpu.X + 2));
    abs16_set((u16)(g_cpu.Y + 2), g_cpu.A);
    g_cpu.Y = g_cpu.A;
    g_cpu.A = abs16(g_cpu.X);
    abs16_set(g_cpu.Y, g_cpu.A);
    g_cpu.Y = dp16(0x4E);
    abs16_set((u16)(g_cpu.X + 4), abs16((u16)(g_cpu.Y + 0x14)));
    abs16_set((u16)(g_cpu.X + 6), abs16((u16)(g_cpu.Y + 0x18)));
    abs16_set((u16)(g_cpu.X + 0x0B), dp16(0x55));
    abs16_set((u16)(g_cpu.X + 0x09), abs16((u16)(g_cpu.Y + 0x10)));
    dc_set_mx(1, 0);
    abs8_set((u16)(g_cpu.X + 0x0D), dp8(0x57));
    abs8_set((u16)(g_cpu.X + 0x08), abs8((u16)(g_cpu.Y + 0x0F)));
    g_cpu.Y = 0xB500;
    u8 fl = abs8((u16)(g_cpu.X + 0x08));
    if (fl & 0x01) {
        dc_set_mx(0, 0);
        for (;;) {
            g_cpu.Y = abs16((u16)(g_cpu.Y + 2));
            dc_cmp16(g_cpu.Y, 0xB500);
            if (g_cpu._flag_Z) break;
            if (!(abs16((u16)(g_cpu.Y + 8)) & 1)) break;
        }
    } else {
        u8 v = abs8((u16)(g_cpu.X + 0x0A));
        dc_set_mx(0, 0);
        if (!(v & 0x80)) {
            for (;;) {
                g_cpu.Y = abs16((u16)(g_cpu.Y + 2));
                dc_cmp16(g_cpu.Y, 0xB500);
                if (g_cpu._flag_Z) break;
                if (abs16((u16)(g_cpu.Y + 8)) & 1) continue;
                dc_cmp16(abs16((u16)(g_cpu.X + 6)), abs16((u16)(g_cpu.Y + 6)));
                if (g_cpu._flag_Z || !g_cpu._flag_C) continue;
                break;
            }
        }
    }
    g_cpu.Y = abs16(g_cpu.Y);
    abs16_set((u16)(g_cpu.X + 2), abs16((u16)(g_cpu.Y + 2)));
    abs16_set((u16)(g_cpu.Y + 2), g_cpu.X);
    g_cpu.Y = abs16((u16)(g_cpu.X + 2));
    abs16_set(g_cpu.X, abs16(g_cpu.Y));
    g_cpu.A = g_cpu.X;
    abs16_set(g_cpu.Y, g_cpu.X);
    plb();
}
DC_ENTRY(C239D5, RTS)                            /* object's 4 surrounding map cells (62AE) into 22-25 */
{
    dc_set_mx(0, 0);
    dp16_set(0x08, lsr16(lsr16(lsr16(dp16(0x08)))));
    dp16_set(0x0A, lsr16(lsr16(lsr16(dp16(0x0A)))));
    dc_set_mx(1, 0);
    dp16_set(0x10, 0x5800); dp8_set(0x12, 0x7E);
    dp16_set(0x13, 0x7000); dp8_set(0x15, 0x7E);
    g_cpu.Y = 0x7000;
    g_cpu.X = dp16(0x4E);
    dc_set_mx(0, 0);
    u16 a = (u16)(dp16(0x08) - 1);
    if (a & 0x8000) a = 0x00BF;
    dp16_set(0x00, a);
    g_cpu.A = (u16)((dp16(0x0A) - 1) & 0x007F);
    dp16_set(0x01, g_cpu.A);
    dc_jsr(0xC262AE, 0xC23A0F);
    ldx16(dp16(0x4E));
    abs8_set(0x0022 + g_cpu.X, (u8)g_cpu.A);
    dc_set_a8(dp8(0x08));
    dp8_set(0x00, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu.A = (u16)((dp16(0x0A) - 1) & 0x007F);
    dp16_set(0x01, g_cpu.A);
    dc_jsr(0xC262AE, 0xC23A25);
    ldx16(dp16(0x4E));
    abs8_set(0x0023 + g_cpu.X, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    a = (u16)(dp16(0x08) - 1);
    if (a & 0x8000) a = 0x00BF;
    dp16_set(0x00, a);
    g_cpu.A = dp16(0x0A);
    dp16_set(0x01, g_cpu.A);
    dc_jsr(0xC262AE, 0xC23A3D);
    ldx16(dp16(0x4E));
    abs8_set(0x0024 + g_cpu.X, (u8)g_cpu.A);
    dp8_set(0x00, dp8(0x08));
    dc_set_a8(dp8(0x0A));
    dp8_set(0x01, (u8)g_cpu.A);
    dc_jsr(0xC262AE, 0xC23A4D);
    ldx16(dp16(0x4E));
    abs8_set(0x0025 + g_cpu.X, (u8)g_cpu.A);
}
DC_ENTRY(C29AAD, RTS)                            /* draw the frame of the big menu window (5780.. / 3E00..) and run FBF8 */
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x5780, 0x3FA4); abs16_set(0x5782, 0x3FA5);
    g_cpu.X = 0x5782; g_cpu.Y = 0x5784; g_cpu.A = 0x0019; dc_mvn(0x7E, 0x7E);
    abs16_set(0x579E, 0x3FA6); abs16_set(0x57C0, 0x3FA7); abs16_set(0x57C2, 0x3FAC);
    g_cpu.X = 0x57C2; g_cpu.Y = 0x57C4; g_cpu.A = 0x0019; dc_mvn(0x7E, 0x7E);
    abs16_set(0x57DE, 0x3FA8); abs16_set(0x58C0, 0x3FA9); abs16_set(0x58C2, 0x3FAA);
    g_cpu.X = 0x58C2; g_cpu.Y = 0x58C4; g_cpu.A = 0x0019; dc_mvn(0x7E, 0x7E);
    abs16_set(0x58DE, 0x3FAB);
    g_cpu.X = 0x57C0; g_cpu.Y = 0x5800; g_cpu.A = 0x00BF; dc_mvn(0x7E, 0x7E);
    abs16_set(0x3E00, 0x1FA0); abs16_set(0x3E02, 0x1FA1); abs16_set(0x3E40, 0x1FA2); abs16_set(0x3E42, 0x1FA3);
    g_cpu.X = 0x3E00; g_cpu.Y = 0x3E04; g_cpu.A = 0x003B; dc_mvn(0x7E, 0x7E);
    g_cpu.X = 0x3E40; g_cpu.Y = 0x3E44; g_cpu.A = 0x003B; dc_mvn(0x7E, 0x7E);
    g_cpu.X = 0x3E00; g_cpu.Y = 0x3E80; g_cpu.A = 0x077F; dc_mvn(0x7E, 0x7E);
    ldx16(0xFBF8);
    dc_jsr(0xC28385, 0xC29B4F);
    plp();
}
/* ---- map scrolling by n pixels (4 directions); every 8 pixels a new
 * strip of tiles is drawn (0773/07C2 columns, 0843/088F rows) and queued
 * for VRAM (090D/0961). Scroll positions live at $20,X (x, wraps 600)
 * and $26,X (y, wraps 400); the strip cursors at 00E3,X / 00E7,X. */
static void dpx16_set(u8 o, u16 v) { u16 at = dpx_x(o); b0_8_set(at, (u8)v); b0_8_set((u16)(at + 1), (u8)(v >> 8)); }
static u16 dec_wrap(u16 v, u16 top) { v = (u16)(v - 1); dc_set_flags_nz16(v); return (v & 0x8000) ? top : v; }
static u16 inc_wrap(u16 v, u16 lim) { v = (u16)(v + 1); dc_cmp16(v, lim); return g_cpu._flag_C ? g_cpu.D : v; }
static bool scroll_count_at(u8 o)                /* DEC $o (16-bit); true while not done */
{
    u16 n = (u16)(dp16(o) - 1);
    dp16_set(o, n);
    dc_set_flags_nz16(n);
    return n != 0;
}
static bool scroll_count(void) { return scroll_count_at(0x0C); }
static void scroll_right(bool reload_x)          /* 060E loop */
{
    do {
        if (reload_x) g_cpu.X = dp16(0x0E);
        reload_x = true;
        u16 a = inc_wrap(dpx16(0x20), 0x0600);
        dpx16_set(0x20, a);
        lda16(a & 0x0007);
        if (g_cpu.A) continue;
        a = inc_wrap(abs16(0x00E3 + g_cpu.X), 0x00C0);
        abs16_set(0x00E3 + g_cpu.X, a);
        g_cpu._flag_C = 0;
        a = dc_adc16(a, 0x0020);
        dc_cmp16(a, 0x00C0);
        if (g_cpu._flag_C) { g_cpu._flag_C = 1; a = dc_sbc16(a, 0x00C0); }
        dp16_set(0x08, lsr16(a));
        a = dec_wrap(abs16(0x00E7 + g_cpu.X), 0x007F);
        dp16_set(0x0A, lsr16(a));
        lda16(dpx16(0x20) & 0x0008);
        if (!g_cpu.A) dc_jsr(0xC20773, 0xC20655); else dc_jsr(0xC207C2, 0xC2065A);
        g_cpu.X = dp16(0x0E);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(dpx16(0x20), 0x0100);
        dp16_set(0x08, g_cpu.A);
        dc_jsr(0xC2090D, 0xC20667);
    } while (scroll_count());
}
DC_ENTRY(C20607, RTS)
{
    dc_set_mx(0, 0);
    g_cpu.A &= 0x00FF;
    dc_set_flags_nz16(g_cpu.A);
    dp16_set(0x0C, g_cpu.A);
    scroll_right(true);
}
DC_ENTRY(C20610, RTS) { scroll_right(false); }
DC_ENTRY(C2059D, RTS)                            /* scroll left by -A pixels */
{
    dc_set_mx(0, 0);
    g_cpu.A = (u16)(((g_cpu.A | 0xFF00) ^ 0xFFFF) + 1);
    dp16_set(0x0C, g_cpu.A);
    do {
        g_cpu.X = dp16(0x0E);
        if (!(dpx16(0x20) & 0x0007))
            abs16_set(0x00E3 + g_cpu.X, dec_wrap(abs16(0x00E3 + g_cpu.X), 0x00BF));
        u16 a = dec_wrap(dpx16(0x20), 0x05FF);
        dpx16_set(0x20, a);
        lda16(a & 0x0007);
        if (g_cpu.A) continue;
        a = dec_wrap(abs16(0x00E3 + g_cpu.X), 0x00BF);
        dp16_set(0x08, lsr16(a));
        a = dec_wrap(abs16(0x00E7 + g_cpu.X), 0x007F);
        dp16_set(0x0A, lsr16(a));
        lda16(dpx16(0x20) & 0x0008);
        if (!g_cpu.A) dc_jsr(0xC207C2, 0xC205F0); else dc_jsr(0xC20773, 0xC205F5);
        g_cpu.X = dp16(0x0E);
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(dpx16(0x20), 0x0008);
        dp16_set(0x08, g_cpu.A);
        dc_jsr(0xC2090D, 0xC20602);
    } while (scroll_count());
}
DC_ENTRY(C206A1, RTS)                            /* scroll up by -A pixels */
{
    dc_set_mx(0, 0);
    g_cpu.A = (u16)(((g_cpu.A | 0xFF00) ^ 0xFFFF) + 1);
    dp16_set(0x0C, g_cpu.A);
    do {
        g_cpu.X = dp16(0x0E);
        if (!(dpx16(0x26) & 0x0007))
            abs16_set(0x00E7 + g_cpu.X, dec_wrap(abs16(0x00E7 + g_cpu.X), 0x007F));
        u16 a = dec_wrap(dpx16(0x26), 0x03FF);
        dpx16_set(0x26, a);
        lda16(a & 0x0007);
        if (g_cpu.A) continue;
        a = dec_wrap(abs16(0x00E3 + g_cpu.X), 0x00BF);
        dp16_set(0x08, lsr16(a));
        a = dec_wrap(abs16(0x00E7 + g_cpu.X), 0x007F);
        dp16_set(0x0A, lsr16(a));
        lda16(dpx16(0x26) & 0x0008);
        if (!g_cpu.A) dc_jsr(0xC2088F, 0xC206F4); else dc_jsr(0xC20843, 0xC206F9);
        g_cpu.X = dp16(0x0E);
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(dpx16(0x26), 0x0008);
        dp16_set(0x0A, g_cpu.A);
        dc_jsr(0xC20961, 0xC20706);
    } while (scroll_count());
}
DC_ENTRY(C2070B, RTS)                            /* scroll down by A pixels */
{
    dc_set_mx(0, 0);
    g_cpu.A &= 0x00FF;
    dc_set_flags_nz16(g_cpu.A);
    dp16_set(0x0C, g_cpu.A);
    do {
        g_cpu.X = dp16(0x0E);
        u16 a = inc_wrap(dpx16(0x26), 0x0400);
        dpx16_set(0x26, a);
        lda16(a & 0x0007);
        if (g_cpu.A) continue;
        a = inc_wrap(abs16(0x00E7 + g_cpu.X), 0x0080);
        abs16_set(0x00E7 + g_cpu.X, a);
        a = dec_wrap(abs16(0x00E3 + g_cpu.X), 0x00BF);
        dp16_set(0x08, lsr16(a));
        g_cpu._flag_C = 0;
        a = dc_adc16(abs16(0x00E7 + g_cpu.X), 0x001C);
        dc_cmp16(a, 0x0080);
        if (g_cpu._flag_C) { g_cpu._flag_C = 1; a = dc_sbc16(a, 0x0080); }
        dp16_set(0x0A, lsr16(a));
        lda16(dpx16(0x26) & 0x0008);
        if (!g_cpu.A) dc_jsr(0xC20843, 0xC2075C); else dc_jsr(0xC2088F, 0xC20761);
        g_cpu.X = dp16(0x0E);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(dpx16(0x26), 0x00E0);
        dp16_set(0x0A, g_cpu.A);
        dc_jsr(0xC20961, 0xC2076E);
    } while (scroll_count());
}
/* ---- scroll a map layer: $00 bits 0-1 = layer (1-3; 0 hangs), $01 =
 * signed pixel count. Layer tables C2:0556/055C/0562 give the VRAM
 * bases put in $10/$13/$16. */
static void scroll_layer(bool vert)
{
    dc_set_mx(0, 0);
    lda16(dp16(0x00) & 0x0003);
    if (!g_cpu.A) for (;;) dc_spend(24);       /* BRA * */
    g_cpu.A = asl16((u16)(g_cpu.A - 1));
    dp16_set(0x0E, g_cpu.A);
    g_cpu.X = g_cpu.A;
    g_cpu.A = bus16(0xC20556 + g_cpu.X); dp16_set(0x10, g_cpu.A);
    g_cpu.A = bus16(0xC2055C + g_cpu.X); dp16_set(0x13, g_cpu.A);
    g_cpu.A = bus16(0xC20562 + g_cpu.X); dp16_set(0x16, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(dp8(0x01));
    if (!(u8)g_cpu.A) return;
    if (g_cpu._flag_N) dc_jsr(vert ? 0xC206A1 : 0xC2059D, vert ? 0xC20694 : 0xC20590);
    else dc_jsr(vert ? 0xC2070B : 0xC20607, vert ? 0xC20699 : 0xC20595);
    dc_set_mx(1, 0);
}
DC_ENTRY(C20568, RTS) { scroll_layer(false); }
DC_ENTRY(C2066C, RTS) { scroll_layer(true); }
DC_ENTRY(C20580, RTS)                            /* tail of 0568 with $10 and X already set */
{
    dp16_set(0x13, g_cpu.A);
    g_cpu.A = bus16(0xC20562 + g_cpu.X); dp16_set(0x16, g_cpu.A);
    dc_set_mx(1, 0);
    lda8(dp8(0x01));
    if (!(u8)g_cpu.A) return;
    if (g_cpu._flag_N) dc_jsr(0xC2059D, 0xC20590); else dc_jsr(0xC20607, 0xC20595);
    dc_set_mx(1, 0);
}
DC_ENTRY(C23349, RTS)                            /* which of 1B33/1B37/1B35 are set (bit 7 clear) -> 1B44, then dispatch */
{
    dc_set_mx(1, 0);
    abs8_set(0x1B58, 0);
    u8 bits = 0;
    if (!(abs8(0x1B33) & 0x80)) bits |= 1;
    if (!(abs8(0x1B37) & 0x80)) bits |= 2;
    if (!(abs8(0x1B35) & 0x80)) bits |= 4;
    dp8_set(0x00, bits);
    abs8_set(0x1B44, bits);
    g_cpu.A = g_cpu.D;
    dc_set_a8(asl8(bits));
    g_cpu.X = g_cpu.A;
    u16 to = bus16(0xC20000 | (u16)(0x337E + g_cpu.X));
    switch (to) {
    case 0x338E: dc_C2338E_glue(); break;
    case 0x338F: dc_C2338F_glue(); break;
    case 0x339A: dc_C2339A_glue(); break;
    case 0x33AA: dc_C233AA_glue(); break;
    case 0x33D4: dc_C233D4_glue(); break;
    default: bad_jump(0xC2337B, to);
    }
}
DC_ENTRY(C20EA3, RTS)                            /* sprite timer: count down 0E,X (reload from script +3); at 0 return 4, else add a node */
{
    dc_set_mx(1, 0);
    ldx16(dp16(0x4E));
    u8 fl = abs8((u16)(g_cpu.X + 0x0F));
    dc_set_a8(fl);
    if (!(fl & 0x40)) {
        u8 t = abs8((u16)(g_cpu.X + 0x0E));
        if (!t) {
            g_cpu.Y = 0x0003;
            dc_set_a8(bus8(lp(0x52) + 3));
            abs8_set((u16)(g_cpu.X + 0x0E), (u8)g_cpu.A);
        } else {
            t--;
            abs8_set((u16)(g_cpu.X + 0x0E), t);
            if (!t) {
                dc_set_mx(0, 0);
                lda16(0x0004);
                g_cpu._flag_C = 0;
                return;
            }
        }
    }
    dc_set_a8(dp8(0x54));
    dp8_set(0x57, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu.Y = 0x0001;
    g_cpu.A = bus16(lp(0x52) + 1);
    dp16_set(0x55, g_cpu.A);
    dc_jsr(0xC20B53, 0xC20ED6);
    lda16(g_cpu.D);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C2584A, RTS)                            /* text answer / width by $15 (table 5851) */
{
    dc_set_a8(asl8(dp8(0x15)));
    g_cpu.X = g_cpu.A;
    u16 to = bus16(0xC20000 | (u16)(0x5851 + g_cpu.X));
    switch (to) {
    case 0x5893: dc_C25893_glue(); break;
    case 0x5895: dc_C25895_glue(); break;
    case 0x5897: dc_C25897_glue(); break;
    case 0x589B: dc_C2589B_glue(); break;
    default: bad_jump(0xC2584E, to);
    }
}
static void d32c(void)                           /* character's portrait row: 0D79,$79 -> 0D8C, frame at 3E8C.., then 2991 & 7 */
{
    php();
    dc_set_mx(1, 1);
    g_cpu.X = dp8(0x79);
    dc_set_a8(abs8((u16)(0x0D79 + g_cpu.X)));
    abs8_set(0x0D8C, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(dp16(0x78) & 0x0300, 0x3E8C);
    dp16_set(0x63, g_cpu.A);
    g_cpu.X = 0x0418;
    dp16_set(0x5F, 0x0418);
    dc_jsr(0xC2ECDB, 0xC2D348);
    g_cpu.A = (u16)((dp16(0x63) & 0xFFC0) | 0x0004);
    dp16_set(0x63, g_cpu.A);
    g_cpu.A = 0x0404;
    dp16_set(0x5F, 0x0404);
    dc_jsr(0xC2EDF6, 0xC2D35A);
    dc_jsr(0xC2EE7F, 0xC2D35D);
    dc_set_mx(1, 0);
    lda8(abs8(0x2991) & 0x07);
    abs8_set(0x0D8C, (u8)g_cpu.A);
    plp();
}
DC_ENTRY(C2D131, RTS)                            /* character slot $79: number, 0DC7 value, then portrait row */
{
    g_cpu.X = dp16(0x61);
    g_cpu.Y = 0x0002;
    u8 v = (u8)(dp8(0x79) << 1);
    g_cpu.A = (u16)(0x3000 | (u8)(v + 1));
    g_cpu._flag_C = dp8(0x79) >> 7;
    dc_set_flags_nz8((u8)(v + 1));
    dc_jsr(0xC2FBB4, 0xC2D13D);
    ldx16((u16)(g_cpu.X + 10));
    g_cpu.Y = 0x0010;
    dc_set_a8(abs8(0x0DC7));
    dc_set_flags_nz8((u8)g_cpu.A);
    dc_jsr(0xC2FB97, 0xC2D150);
    d32c();
}
DC_ENTRY(C25DC4, RTS)                            /* draw character $35 at pixel $34: glyph pointers, 16x12 rows, advance by width table */
{
    dc_set_mx(0, 0);
    u16 t = asl16(asl16(asl16(dp16(0x35))));
    dp16_set(0x6C, t);
    g_cpu.A = dc_adc16(asl16(t), dp16(0x6C));
    g_cpu._flag_C = 0;
    dp16_set(0x76, dc_adc16(g_cpu.A, 0x2060));
    t = asl16(asl16(asl16(lsr16(dp16(0x35)))));
    dp16_set(0x6C, t);
    g_cpu.A = dc_adc16(asl16(t), dp16(0x6C));
    g_cpu._flag_C = 0;
    dp16_set(0x79, dc_adc16(g_cpu.A, 0x3860));
    dp16_set(0x6A, dp16(0x34) & 0x0007);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(dp16(0x10), 0x0008);
    dp16_set(0x73, g_cpu.A);
    dc_set_mx(1, 0);
    dp8_set(0x75, dp8(0x12));
    dp8_set(0x78, 0xFF);
    dp8_set(0x7B, 0xFF);
    dp8_set(0x6E, (u8)((dp8(0x34) & 0xF8) >> 2));
    dp8_set(0x6F, 0);
    dp8_set(0x62, dp8(0x35));
    dc_set_a8(dp8(0x14) & 0x0F);
    if (g_cpu.A & 0xFF) dc_jsr(0xC25F07, 0xC25E1B); else dc_jsr(0xC25E36, 0xC25E16);
    g_cpu.A = 0;
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc8(dp8(0x35), 0xA0);
    g_cpu.X = g_cpu.A;
    g_cpu._flag_C = 0;
    dc_set_a8(dc_adc8(bus8(0xC260E6 + g_cpu.X), dp8(0x34)));
    dp8_set(0x34, (u8)g_cpu.A);
    u8 c = (u8)(dp8(0x17) + 1);
    dp8_set(0x17, c);
    g_cpu.A = 0;
    dc_set_flags_nz8(0);
}
DC_ENTRY(C286DD, RTS)                            /* build the HDMA table for the menu palette fade (9D99 list -> 9E19/9F19, double buffered) */
{
    php();
    dc_push8(g_cpu.DB);
    dc_set_mx(0, 0);
    g_cpu.X = 0x8771; g_cpu.Y = 0x9DE9; g_cpu.A = 0x001F;
    dc_mvn(0x7E, 0xC2);
    bus16_set(0x004300, 0x3101);
    bus16_set(0x004304, 0x007E);
    g_cpu.Y = 0x0000;
    g_cpu.A = 0x9E19;
    ldx16(abs16(0xA01D));
    if (!g_cpu.X) { g_cpu.Y = 0x0100; g_cpu.A = 0x9F19; }
    abs16_set(0xA01D, g_cpu.Y);
    abs16_set(0xA01B, 0);
    g_cpu.X = 0;
    for (;;) {
        dc_set_mx(1, 0);
        lda8(abs8((u16)(0x9D99 + g_cpu.X)));
        if (g_cpu._flag_N) break;
        dc_set_mx(0, 0);
        if (g_cpu._flag_Z) {                     /* 0: one line of the fixed colour */
            g_cpu.A = abs16((u16)(0x9D9A + g_cpu.X));
            abs16_set((u16)(0x9E19 + g_cpu.Y), g_cpu.A);
            g_cpu.A = abs16(0x9DF9);
            abs16_set((u16)(0x9E1A + g_cpu.Y), g_cpu.A);
            g_cpu.Y += 3;
        } else {                                 /* n: 16 colours stepping by n/16 lines */
            g_cpu.A = asl16(asl16(asl16(asl16(abs16((u16)(0x9D9A + g_cpu.X)) & 0x00FF))));
            abs16_set(0xA019, g_cpu.A);
            dc_push16(g_cpu.X);
            for (g_cpu.X = 0; g_cpu.X < 0x20; g_cpu.X += 2) {
                g_cpu._flag_C = 0;
                g_cpu.A = dc_adc16(abs16(0xA01B), abs16(0xA019));
                abs16_set(0xA01B, g_cpu.A);
                dc_set_mx(1, 0);
                g_cpu.A = (u16)(g_cpu.A << 8 | g_cpu.A >> 8);
                abs8_set((u16)(0x9E19 + g_cpu.Y), (u8)g_cpu.A);
                abs8_set(0xA01C, 0);
                dc_set_mx(0, 0);
                g_cpu.A = abs16((u16)(0x9DE9 + g_cpu.X));
                abs16_set((u16)(0x9E1A + g_cpu.Y), g_cpu.A);
                g_cpu.Y += 3;
            }
            dc_cmp16(g_cpu.X, 0x0020);
            g_cpu.X = dc_pull16();
        }
        g_cpu.X += 2;
    }
    plb();
    plp();
}
/* Search a 7E list of n entries (step bytes) for the word $00 among the
 * used ones (bit 7 set, compared without bits 7 and 14-15); FFFF if none. */
static u16 find_used(u32 base, u16 step, u16 n)
{
    g_cpu.Y = n;
    g_cpu.X = 0;
    do {
        g_cpu.A = bus16(base + g_cpu.X);
        if (g_cpu.A & 0x0080) {
            g_cpu.A &= 0x3F7F;
            dc_cmp16(g_cpu.A, dp16(0x00));
            if (g_cpu._flag_Z) return g_cpu.X;
        }
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, step);
        g_cpu.X = g_cpu.A;
        g_cpu.Y--;
        dc_set_flags_nz16(g_cpu.Y);
    } while (g_cpu.Y);
    ldx16(0xFFFF);
    return 0xFFFF;
}
DC_ENTRY(C232CD, RTS)                            /* find character $00 in the three tech lists -> 1B32 / 1B34 / 1B36 */
{
    abs16_set(0x1B32, find_used(0x7E7E00, 7, abs16(0x1B38) & 0x00FF));
    abs16_set(0x1B34, find_used(0x7E8300, 3, abs16(0x1B39) & 0x00FF));
    abs16_set(0x1B36, find_used(0x7E8180, 3, abs16(0x1B3A) & 0x00FF));
}
DC_ENTRY(C28D8F, RTS)                            /* shop slot $C0 (x 0x50 at 2600): new item C9 or add count */
{
    bus8_set(0x004202, dp8(0xC0));
    bus8_set(0x004203, 0x50);
    dp8_set(0xCB, 0x01);
    lda8(dp8(0xC9));
    dp8_set(0xC5, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    if (g_cpu._flag_Z) {
        dp16_set(0xCC, 0);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(bus16(0x004216), 0x2600);
        dp16_set(0xC3, g_cpu.A);
        if (g_cpu.A == 0x2790) dp16_set(0xCC, (u16)(dp16(0xCC) + 1));
        do {
            g_cpu.X = dp16(0xCC);
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(bus8(0xC2894F + g_cpu.X), dp16(0xC3));
            g_cpu.Y = g_cpu.A;
            dc_set_mx(1, 0);
            dc_set_a8(bus8(0xC28DDF + g_cpu.X));
            abs8_set(g_cpu.Y, (u8)g_cpu.A);
            dp8_set(0xCC, (u8)(dp8(0xCC) + 1));
            dc_set_a8(dp8(0xCC));
            dc_cmp8((u8)g_cpu.A, 0x04);
            dc_set_mx(0, 0);
        } while (!g_cpu._flag_C);
        g_cpu.Y = dp16(0xC3);
        dc_C28D1C_glue();
        return;
    }
    dc_jsr(0xC28881, 0xC28DE3);
    ldx16(dp16(0xCC));
    dc_cmp16(g_cpu.X, 0x0004);
    if (g_cpu._flag_C) return;
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(bus16(0x004216), 0x2600);
    dp16_set(0xC3, g_cpu.A);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(bus8(0xC2894F + g_cpu.X), dp16(0xC3));
    g_cpu.X = g_cpu.A;
    dc_set_mx(1, 0);
    dc_set_a8(abs8(g_cpu.X));
    dp8_set(0xC9, (u8)g_cpu.A);
    dc_jsr(0xC28791, 0xC28E09);
    dc_set_a8(dp8(0xC5));
    dp8_set(0xC9, (u8)g_cpu.A);
    abs8_set(g_cpu.X, (u8)g_cpu.A);
    dc_jsr(0xC287D5, 0xC28E13);
    dc_set_mx(0, 0);
    g_cpu.Y = dp16(0xC3);
    dc_jsr(0xC28D1C, 0xC28E1A);
}
/* Walk $06 one cell at a time toward $07 (row $05) until the cell or its
 * left neighbour (62AE) has a wall bit (0-1): C set if blocked. */
static void wall_step(bool down)
{
    if (down) dp8_set(0x06, (u8)(dp8(0x06) + 1)); else dp8_set(0x06, (u8)(dp8(0x06) - 1));
    lda8((u8)(dp8(0x05) - 1));
    dp8_set(0x00, (u8)g_cpu.A);
    dp8_set(0x01, dp8(0x06));
    dc_jsr(0xC262AE, down ? 0xC24284 : 0xC242B3);
    g_cpu.X = dp16(0x4E);
    abs8_set((u16)(0x0022 + g_cpu.X), (u8)g_cpu.A);
    dp8_set(0x00, dp8(0x05));
    dp8_set(0x01, dp8(0x06));
    dc_jsr(0xC262AE, down ? 0xC24294 : 0xC242C3);
    g_cpu.X = dp16(0x4E);
    lda8((u8)((g_cpu.A | abs8((u16)(0x0022 + g_cpu.X))) & 0x03));
}
DC_ENTRY(C2425A, RTS)
{
    g_cpu.X = 0x5800; dp16_set(0x10, 0x5800);
    dc_set_a8(0x7E); dp8_set(0x12, 0x7E);
    g_cpu.X = 0x7000; dp16_set(0x13, 0x7000);
    dp8_set(0x15, 0x7E);
    dp8_set(0x06, (u8)(dp8(0x06) - 1));
    dp8_set(0x07, (u8)(dp8(0x07) - 1));
    g_cpu._flag_C = 1;
    dc_set_a8(dc_sbc8(dp8(0x07), dp8(0x06)));
    if (!g_cpu._flag_Z) {
        bool down = !g_cpu._flag_N;
        for (;;) {
            wall_step(down);
            if (!g_cpu._flag_Z) {
                u8 v = (u8)(dp8(0x06) + 1);
                dp8_set(0x06, v); dc_set_flags_nz8(v);
                g_cpu._flag_C = 1;
                return;
            }
            dc_set_a8(dp8(0x06));
            dc_cmp8((u8)g_cpu.A, dp8(0x07));
            if (g_cpu._flag_Z) break;
        }
    }
    u8 v = (u8)(dp8(0x06) + 1);
    dp8_set(0x06, v); dc_set_flags_nz8(v);
    g_cpu._flag_C = 0;
}
static u16 sext8(u8 v) { u16 a = v; if (a & 0x80) a |= 0xFF00; return a; }
/* $00 sprite pieces from [$55] (dx, dy, tile word) at ($0C,$0E) into OAM 0700.. */
static void sprite_pieces(void)
{
    do {
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(sext8(bus8(lp(0x55))), dp16(0x0C));
        dp16_set(0x08, g_cpu.A);
        dp16_set(0x04, g_cpu.A);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.A, 0x000F);
        dc_cmp16(g_cpu.A, 0x010F);
        if (g_cpu._flag_C) goto next;
        g_cpu.Y = 0x0001;
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(sext8(bus8(lp(0x55) + 1)), dp16(0x0E));
        dp16_set(0x09, g_cpu.A);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.A, 0x000F);
        dc_cmp16(g_cpu.A, 0x00EF);
        if (g_cpu._flag_C) goto next;
        g_cpu.Y = 0x0002;
        g_cpu._flag_C = 0;
        g_cpu.Y = (u16)(dc_adc16(bus16(lp(0x55) + 2), dp16(0x06)) | dp16(0x02));
        g_cpu.X = (u16)((dp16(0x4D) & 0x007F) << 2);
        abs16_set((u16)(0x0700 + g_cpu.X), dp16(0x08));
        abs16_set((u16)(0x0702 + g_cpu.X), g_cpu.Y);
        g_cpu.A = g_cpu.Y;
        dc_set_mx(1, 1);
        dp8_set(0x16, dp8(0x13));
        u8 a = dp8(0x4D) & 0x03;
        u8 v16 = dp8(0x16); g_cpu._flag_C = v16 & 1; dp8_set(0x16, (u8)(v16 >> 1));
        a = rol8(a);
        u8 v05 = dp8(0x05); g_cpu._flag_C = v05 & 1; dp8_set(0x05, (u8)(v05 >> 1));
        a = rol8(a);
        g_cpu.X = a;
        g_cpu.Y = (u8)(dp8(0x4D) >> 2);
        dc_set_a8((u8)(bus8(0xC20E0D + g_cpu.X) | abs8((u16)(0x0900 + g_cpu.Y))));
        abs8_set((u16)(0x0900 + g_cpu.Y), (u8)g_cpu.A);
        dc_set_mx(0, 0);
        dp16_set(0x4D, (u16)(dp16(0x4D) + 1));
    next:
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(dp16(0x55), 0x0004);
        dp16_set(0x55, g_cpu.A);
        u16 n = (u16)(dp16(0x00) - 1);
        dp16_set(0x00, n);
        dc_set_flags_nz16(n);
    } while (dp16(0x00));
    dc_set_mx(1, 0);
}
DC_ENTRY(C20D80, RTS) { dp16_set(0x0E, g_cpu.A); sprite_pieces(); }
DC_ENTRY(C2940D, RTS)                            /* reset the PPU and CPU I/O registers, clear VRAM, then 9512 three times */
{
    static const u8 ppu[][2] = {
        {0x00, 0x80}, {0x01, 0x02}, {0x05, 0x09}, {0x07, 0x5A}, {0x08, 0x62}, {0x09, 0x6A}, {0x0A, 0x00},
        {0x0B, 0x00}, {0x0C, 0x07}, {0x06, 0x00}, {0x0D, 0x00}, {0x0D, 0x00}, {0x0E, 0x00}, {0x0E, 0x00},
        {0x0F, 0x00}, {0x0F, 0x00}, {0x10, 0x00}, {0x10, 0x00}, {0x11, 0x00}, {0x11, 0x00}, {0x12, 0x00},
        {0x12, 0x00}, {0x13, 0x00}, {0x13, 0x00}, {0x14, 0x00}, {0x14, 0x00}, {0x15, 0x80}, {0x16, 0x00},
        {0x17, 0x00}, {0x1A, 0x00}, {0x1B, 0x00}, {0x1B, 0x01}, {0x1C, 0x00}, {0x1C, 0x00}, {0x1D, 0x00},
        {0x1D, 0x00}, {0x1E, 0x00}, {0x1E, 0x01}, {0x1F, 0x00}, {0x1F, 0x00}, {0x20, 0x00}, {0x20, 0x00},
        {0x21, 0x00}, {0x23, 0x00}, {0x24, 0x00}, {0x25, 0x00}, {0x26, 0x00}, {0x27, 0x00}, {0x28, 0x00},
        {0x29, 0x00}, {0x2A, 0x00}, {0x2B, 0x00}, {0x2C, 0x00}, {0x2D, 0x00}, {0x2E, 0x00}, {0x2F, 0x00},
        {0x30, 0x00}, {0x31, 0x00}, {0x33, 0x00}, {0x32, 0xE0},
    };
    static const u8 cpu[][2] = {
        {0x00, 0x00}, {0x07, 0x00}, {0x08, 0x00}, {0x09, 0x00}, {0x0A, 0x00}, {0x0B, 0x00}, {0x0C, 0x00},
        {0x0D, 0x01}, {0x16, 0x00}, {0x17, 0x00}, {0x18, 0x00}, {0x19, 0x00},
    };
    dc_push8(g_cpu.DB);
    dc_push16(g_cpu.D);
    php();
    dc_set_mx(1, 0);
    g_cpu.DB = 0x00;
    g_cpu.D = 0x2100;
    for (unsigned k = 0; k < sizeof ppu / sizeof ppu[0]; k++) dp8_set(ppu[k][0], ppu[k][1]);
    g_cpu.D = 0x4200;
    g_cpu.X = 0x0000;
    for (unsigned k = 0; k < sizeof cpu / sizeof cpu[0]; k++) dp8_set(cpu[k][0], cpu[k][1]);
    dc_set_a8(0x01);
    g_cpu.X = 0x1809; abs16_set(0x4300, 0x1809);   /* fixed-source DMA of C2:9510 into VRAM */
    g_cpu.X = 0xFFFF; abs16_set(0x4305, 0xFFFF);
    g_cpu.X = 0x9510; abs16_set(0x4302, 0x9510);
    abs8_set(0x4304, 0xC2);
    abs8_set(0x420B, 0x01);
    g_cpu.X = 0x0C00; g_cpu.Y = 0x1200; dc_jsr(0xC29512, 0xC294F7);
    g_cpu.X = 0x2E00; g_cpu.Y = 0xD200; dc_jsr(0xC29512, 0xC29500);
    g_cpu.X = 0x0000; g_cpu.Y = 0x0100; dc_jsr(0xC29512, 0xC29509);
    plp();
    g_cpu.D = dc_pull16();
    plb();
}
DC_ENTRY(C2916E, RTS)                            /* character stats: equipment bonuses (9261 x4) + base, capped by FF:CF33; then 91DB by kind */
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x0DAE, 0);
    g_cpu.X = 0x0DAE; g_cpu.Y = 0x0DB0; g_cpu.A = 0x0006;
    dc_mvn(0x7E, 0x7E);
    dc_set_mx(1, 1);
    abs8_set(0x0DAD, 0x03);
    do {
        dc_jsr(0xC29261, 0xC29187);
        u8 n = (u8)(abs8(0x0DAD) - 1);
        abs8_set(0x0DAD, n);
        dc_set_flags_nz8(n);
    } while (!(abs8(0x0DAD) & 0x80));
    for (int x = 6; x >= 0; x--) {
        g_cpu.X = (u16)x;
        g_cpu.Y = bus8(0xFFCF2C + x);
        g_cpu._flag_C = 0;
        u8 v = dc_adc8(abs8((u16)(0x9A9B + g_cpu.Y)), abs8((u16)(0x0DAE + g_cpu.Y)));
        u8 cap = bus8(0xFFCF33 + x);
        dc_cmp8(v, cap);
        if (g_cpu._flag_C) v = cap;
        dc_set_a8(v);
        abs8_set((u16)(0x9B1C + g_cpu.Y), v);
    }
    dc_set_a8(asl8(abs8(0x9A90) & 0x07));
    g_cpu.X = (u8)g_cpu.A;
    u16 to = bus16(0xC20000 | (u16)(0x91DB + g_cpu.X));
    dc_jsr(0xC20000 | to, 0xC291B4);           /* 91EB / 9223 / 923C */
    bus8_set(0x004206, (u8)g_cpu.A);
    g_cpu._flag_C = 0;
    u8 v = dc_adc8(abs8(0x9B1D), abs8(0x0DB6));
    if (g_cpu._flag_C) v = 0xFF;
    dc_set_a8(v);
    abs8_set(0x9B24, v);
    g_cpu.X = bus8(0x004214);
    lda8(bus8(0x004215));
    if (!g_cpu._flag_Z) g_cpu.X = 0xFF;
    abs8_set(0x9B23, (u8)g_cpu.X);
    plp();
}
/* Source of a block: bank byte [$58]+b+1 (1 -> 4000, else 5800) plus
 * row [$58]+b+3 times 0x60 plus column [$58]+b+2, into $o. */
static void block_ptr(u8 b, u8 o)
{
    g_cpu.Y = (u16)(b + 1);
    dc_set_a8(bus8(lp(0x58) + g_cpu.Y));
    dc_cmp8((u8)g_cpu.A, 0x01);
    g_cpu.X = g_cpu._flag_Z ? 0x4000 : 0x5800;
    dp16_set(o, g_cpu.X);
    g_cpu.Y = (u16)(b + 3);
    dc_set_a8(bus8(lp(0x58) + g_cpu.Y));
    abs8_set(0x4202, (u8)g_cpu.A);
    dc_set_a8(0x60);
    abs8_set(0x4203, 0x60);
    g_cpu.Y--;
    dc_set_a8(bus8(lp(0x58) + g_cpu.Y));
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(g_cpu.A, abs16(0x4216));
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(g_cpu.A, dp16(o));
    dp16_set(o, g_cpu.A);
    dc_set_mx(1, 0);
    g_cpu.A = g_cpu.D;
}
DC_ENTRY(C21B2F, RTS)                            /* copy a rectangle of tiles (rows of 0x60 bytes) between the 4000/5800 maps by WRAM port */
{
    dc_set_mx(1, 0);
    g_cpu.A = g_cpu.D;
    dc_set_a8(0x7E);
    dp8_set(0x12, 0x7E);
    dp8_set(0x15, 0x7E);
    block_ptr(0, 0x10);
    block_ptr(3, 0x13);
    g_cpu.Y = 0x0007;
    dc_set_a8(bus8(lp(0x58) + 7));
    dp8_set(0x00, (u8)g_cpu.A);
    g_cpu.Y = 0x0008;
    dc_set_a8(bus8(lp(0x58) + 8));
    dp8_set(0x01, (u8)g_cpu.A);
    do {
        dp8_set(0x02, dp8(0x00));
        g_cpu.X = dp16(0x13);
        abs16_set(0x2181, g_cpu.X);
        abs8_set(0x2183, 0x7E);
        g_cpu.Y = 0;
        do {
            dc_set_a8(bus8(lp(0x10) + g_cpu.Y));
            abs8_set(0x2180, (u8)g_cpu.A);
            g_cpu.Y++;
        } while (dec8_dp(0x02));
        dc_set_mx(0, 0);
        g_cpu._flag_C = 0;
        dp16_set(0x10, dc_adc16(dp16(0x10), 0x0060));
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(dp16(0x13), 0x0060);
        dp16_set(0x13, g_cpu.A);
        g_cpu.A = g_cpu.D;
        dc_set_mx(1, 0);
    } while (dec8_dp(0x01));
    dc_set_mx(0, 0);
    lda16(0x0009);
}
DC_ENTRY(C2FAE9, RTS)                            /* menu scroll bar: rebuild the bar tiles when 0217 changed, then upload (838E) */
{
    php();
    dc_set_mx(1, 0);
    u8 v = abs8(0x0217);
    dc_set_a8(v);
    dc_cmp8(v, abs8(0x0DC3));
    if (!g_cpu._flag_Z) {
        abs8_set(0x0DC3, v);
        dc_set_mx(0, 0);
        abs16_set(0x9890, abs16(0x0DD4));
        abs16_set(0x9892, abs16(0x0DD6));
        abs16_set(0x9894, 0x007E);
        g_cpu.A = 0x0400;
        abs16_set(0x9895, 0x0400);
        g_cpu.X = 0x9890;
        dc_jsr(0xC2838E, 0xC2FB17);
        dc_set_mx(1, 0);
        u8 n = abs8(0x0234);                     /* bar length: n / 8 rounded at each halving */
        for (int k = 0; k < 3; k++) { g_cpu._flag_C = n & 1; n = dc_adc8((u8)(n >> 1), 0x00); }
        dc_set_a8(n);
        abs8_set(0x0000, n);
        abs8_set(0x0001, 0);
        dc_set_mx(0, 0);
        u16 a = lsr16(lsr16(abs16(0x0DD2) & 0x00F8));
        dc_push16(a);
        g_cpu.A = dc_adc16(dc_adc16(a, abs16(0x0DC1)), abs16(0x0DC5));
        g_cpu.X = g_cpu.A;
        g_cpu.A = lsr16(dc_pull16());
        abs16_set(0x0002, g_cpu.A);
        g_cpu._flag_C = 1;
        g_cpu.Y = dc_sbc16(abs16(0x0000), abs16(0x0002));
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16((u16)(abs16(0x0002) - 1), abs16(0x0DC7));
        g_cpu.A = dc_adc16(g_cpu.A, 0x0010);
        do {
            g_cpu.A &= 0xFFEF;
            abs16_set(g_cpu.X, g_cpu.A);
            g_cpu.A |= 0x0010;
            abs16_set((u16)(g_cpu.X + 0x40), g_cpu.A);
            g_cpu.A++;
            g_cpu.X += 2;
            g_cpu.Y--;
        } while (g_cpu.Y);
        dc_set_mx(1, 0);
        abs8_set(0x0DD2, abs8(0x0234));
    }
    dc_set_mx(0, 0);
    abs16_set(0x9890, abs16(0x0DCC));
    abs16_set(0x9892, abs16(0x0DC5) & 0xFFC0);
    abs16_set(0x9894, 0x007E);
    g_cpu.A = abs16(0x0DD0);
    abs16_set(0x9895, g_cpu.A);
    g_cpu.X = 0x9890;
    dc_jsr(0xC2838E, 0xC2FB92);
    plp();
}
/* One axis of 528A: if the distance from the target (wrapped at `wrap`
 * 0x400 when beyond `half`) is below the current speed, snap to it. */
static void snap_axis(u8 lo, u8 hi, u8 pos, u8 tgt, u16 half, u32 hook, u32 site)
{
    dp16_set(0x0A, 0);
    lda16(abs16((u16)(hi + g_cpu.X)));
    if (g_cpu._flag_N) {
        dp16_set(0x0A, (u16)(dp16(0x0A) + 1));
        g_cpu._flag_C = 0;
        dc_adc16((u16)(abs16((u16)(lo + g_cpu.X)) ^ 0xFFFF), 0x0001);
        g_cpu.A = dc_adc16((u16)(abs16((u16)(hi + g_cpu.X)) ^ 0xFFFF), 0x0000);
    }
    dp16_set(0x08, g_cpu.A);
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc16(abs16((u16)(pos + g_cpu.X)), abs16((u16)(tgt + g_cpu.X)));
    if (g_cpu._flag_N) { g_cpu.A = (u16)((g_cpu.A ^ 0xFFFF) + 1); dc_set_flags_nz16(g_cpu.A); }
    dc_cmp16(g_cpu.A, half);
    if (g_cpu._flag_C) {
        g_cpu._flag_C = 1;
        g_cpu.A = (u16)((dc_sbc16(g_cpu.A, 0x0400) ^ 0xFFFF) + 1);
        dc_set_flags_nz16(g_cpu.A);
    }
    dc_cmp16(g_cpu.A, dp16(0x08));
    if (g_cpu._flag_C) return;
    abs16_set((u16)(hi + g_cpu.X), g_cpu.A);
    abs16_set((u16)(lo + g_cpu.X), 0);
    lda16(dp16(0x0A));
    if (!g_cpu._flag_Z) dc_jsr(hook, site);
}
DC_ENTRY(C2528A, RTS)                            /* object X: stop at the target when closer than one step (x then y) */
{
    snap_axis(0x1A, 0x1C, 0x2A, 0x14, 0x0300, 0xC21C84, 0xC252D0);
    snap_axis(0x1E, 0x20, 0x2C, 0x18, 0x0200, 0xC21C9E, 0xC25319);
}
DC_ENTRY(C209C5, RTS)                            /* redraw the whole layer $00 (1-3) around its scroll position: 18 columns */
{
    dc_set_mx(1, 0);
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    g_cpu.DB = 0x7E;
    dc_set_mx(0, 0);
    lda16(dp16(0x00) & 0x0003);
    if (!g_cpu.A) for (;;) dc_spend(24);       /* BRA * */
    g_cpu.A = asl16((u16)(g_cpu.A - 1));
    g_cpu.X = g_cpu.A;
    dpx16_set(0x20, asl16(asl16(asl16(dpx16(0xE3)))));
    dpx16_set(0x26, asl16(asl16(asl16(dpx16(0xE7)))));
    dp16_set(0x14, bus16(0xC20556 + g_cpu.X));
    dp16_set(0x16, bus16(0xC2055C + g_cpu.X));
    dp16_set(0x10, bus16(0xC20562 + g_cpu.X));
    u16 a = lsr16(dpx16(0xE3));
    if (!g_cpu._flag_C) { a--; if (a & 0x8000) a = 0x005F; }
    dp16_set(0x08, a);
    a = lsr16(dpx16(0xE7));
    if (!g_cpu._flag_C) { a--; if (a & 0x8000) a = 0x003F; }
    dp16_set(0x0A, a);
    a = dpx16(0x20);
    g_cpu._flag_C = 1;
    a = dc_sbc16(a, (a & 0x0008) ? 0x0008 : 0x0010);
    dp16_set(0x0C, a & 0x01FF);
    a = dpx16(0x26);
    g_cpu._flag_C = 1;
    a = dc_sbc16(a, (a & 0x0008) ? 0x0008 : 0x0010);
    dp16_set(0x0E, a & 0x00FF);
    dp16_set(0x02, 0x0012);
    do {
        dc_jsr(0xC20A6C, 0xC20A46);
        dc_jsr(0xC20ADC, 0xC20A49);
        dp16_set(0x08, inc_wrap(dp16(0x08), 0x0060));
        g_cpu._flag_C = 0;
        g_cpu.A = (u16)(dc_adc16(dp16(0x0C), 0x0010) & 0x01FF);
        dc_set_flags_nz16(g_cpu.A);
        dp16_set(0x0C, g_cpu.A);
    } while (scroll_count_at(0x02));
    dc_set_mx(1, 0);
    plb();
}
/* Distance a - b on a ring of 2*half, wrapped into -half..half. */
static u16 ring_delta(u16 a, u16 b, u16 lo, u16 span)
{
    g_cpu._flag_C = 1;
    u16 d = dc_sbc16(a, b);
    if (!g_cpu._flag_N) {
        dc_cmp16(d, (u16)(span / 2));
        if (g_cpu._flag_C) d = dc_sbc16(d, span);
    } else {
        dc_cmp16(d, lo);
        if (!g_cpu._flag_C) d = dc_adc16(d, span);
    }
    return d;
}
DC_ENTRY(C20D08, RTS)                            /* draw object X's sprite: frame [0B,X] (count, then 4-byte pieces) at its position */
{
    g_cpu.Y = abs16((u16)(g_cpu.X + 0x0B));
    dp16_set(0x55, g_cpu.Y);
    dc_set_a8(abs8((u16)(g_cpu.X + 0x0D)));
    dp8_set(0x57, (u8)g_cpu.A);
    dc_set_a8(bus8(lp(0x55)));
    dp8_set(0x00, (u8)g_cpu.A);
    dp8_set(0x01, 0);
    g_cpu.Y = (abs8((u16)(g_cpu.X + 0x0A)) & 0x40) ? 1 : 0;
    dp16_set(0x13, g_cpu.Y);
    dc_set_mx(0, 0);
    dp16_set(0x55, (u16)(dp16(0x55) + 1));
    g_cpu.A = abs16((u16)(g_cpu.X + 0x08));
    dp16_set(0x02, (u16)(g_cpu.A << 8 | g_cpu.A >> 8) & 0x3E00);
    dp16_set(0x06, abs16((u16)(g_cpu.X + 0x09)) & 0x3FFF);
    dp16_set(0x0C, abs16((u16)(g_cpu.X + 0x04)));
    dp16_set(0x0E, abs16((u16)(g_cpu.X + 0x06)));
    if (abs16((u16)(g_cpu.X + 0x08)) & 0x0080) {  /* map-relative: subtract the scroll, wrapped */
        dp16_set(0x0C, ring_delta(dp16(0x0C), dp16(0x22), 0xFD00, 0x0600));
        dp16_set(0x0E, ring_delta(dp16(0x0E), dp16(0x28), 0xFE00, 0x0400));
    }
    sprite_pieces();
}
/* Shared tail of 2D91: load the C6:FED6 table entry into 7E:C600 (C3:0002). */
static void load_c6_36(void)
{
    lda8(0x7E);
    abs8_set(0x0305, 0x7E);
    g_cpu.X = 0xC600;
    abs16_set(0x0303, 0xC600);
    g_cpu.X = 0x0036;
    dc_set_mx(0, 0);
    g_cpu.A = bus16(0xC6FEA0 + 0x36);
    abs16_set(0x0300, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(bus8(0xC6FEA2 + 0x36));
    abs8_set(0x0302, (u8)g_cpu.A);
    dc_jsl(0xC30002, 0xC27BBF);
}
DC_ENTRY(C22D91, RTS)                            /* after the ending (7F:0000 >= D4), reload graphics for locations 1F0/1F1/1F2/1F6 */
{
    dc_set_a8(bus8(0x7F0000));
    dc_cmp8((u8)g_cpu.A, 0xD4);
    if (!g_cpu._flag_C) return;
    u16 loc = abs16(0x0100);
    g_cpu.X = loc;
    dc_cmp16(loc, 0x01F0);
    if (loc != 0x01F0) { dc_cmp16(loc, 0x01F1);
    if (loc != 0x01F1) { dc_cmp16(loc, 0x01F2);
    if (loc != 0x01F2) { dc_cmp16(loc, 0x01F6);
    if (loc != 0x01F6) return; } } }
    u16 at = (u16)(loc == 0x01F0 ? 0x2DB1 : loc == 0x01F1 ? 0x2DCD : loc == 0x01F2 ? 0x2DF3 : 0x2E0F);
    dc_jsr(0xC27B5A, 0xC20000 | at);
    dc_set_mx(0, 0);
    g_cpu.X = 0x9000;
    g_cpu.Y = loc == 0x01F6 ? 0x0000 : 0x0C00;
    g_cpu.A = 0x0800;
    dc_jsr(0xC22D70, 0xC20000 | (u16)(at + 0x0E));
    if (loc != 0x01F6) {
        if (loc == 0x01F1) {
            g_cpu.X = 0x0A80;
            dc_set_a8(abs8(0x1BA8));
            if (g_cpu.A & 0x80) g_cpu.X = 0x0A60;
        } else g_cpu.X = 0x0A40;
        lda8(0x00);
        dc_jsr(0xC27B7F, loc == 0x01F1 ? 0xC22DED : (loc == 0x01F0 ? 0xC22DC7 : 0xC22E09));
    }
    load_c6_36();
}
DC_ENTRY(C2229D, RTS)                            /* direction from ($08,$0A) to ($0C,$0E) on the 600x400 ring map, via the F300 table */
{
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(dp16(0x0A), dp16(0x0E)));
    dp16_set(0x00, g_cpu.A);
    if (g_cpu._flag_N) lda16((u16)((g_cpu.A ^ 0xFFFF) + 1));
    dp16_set(0x02, g_cpu.A);
    dc_cmp16(g_cpu.A, 0x0200);
    if (g_cpu._flag_C) {
        lda16((u16)((dc_sbc16(g_cpu.A, 0x0400) ^ 0xFFFF) + 1));
        dp16_set(0x02, g_cpu.A);
        lda16((u16)((dp16(0x00) ^ 0xFFFF) + 1));
        dp16_set(0x00, g_cpu.A);
    }
    g_cpu._flag_C = 1;
    lda16(dc_sbc16(dp16(0x08), dp16(0x0C)));
    dp16_set(0x04, g_cpu.A);
    if (g_cpu._flag_N) lda16((u16)((g_cpu.A ^ 0xFFFF) + 1));
    dp16_set(0x06, g_cpu.A);
    dc_cmp16(g_cpu.A, 0x0300);
    if (g_cpu._flag_C) {
        lda16((u16)((dc_sbc16(g_cpu.A, 0x0600) ^ 0xFFFF) + 1));
        dp16_set(0x06, g_cpu.A);
        lda16((u16)((dp16(0x04) ^ 0xFFFF) + 1));
        dp16_set(0x04, g_cpu.A);
    }
    dp16_set(0x06, lsr16(lsr16(dp16(0x06))));
    g_cpu.A = asl16(asl16(asl16(dp16(0x02) & 0xFFFC)));
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16(g_cpu.A, dp16(0x06));
    dc_set_mx(1, 0);
    dc_set_a8(abs8(0xF300 + (u32)g_cpu.X));
    dp8_set(0x06, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    bool dy_neg = dp16(0x04) & 0x8000;
    if (!((dp16(0x00) ^ dp16(0x04)) & 0x8000)) {
        if (dy_neg) { lda16(dp16(0x06)); return; }
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(0x0080, dp16(0x06));
    } else {
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(dy_neg ? 0x0100 : 0x0080, dp16(0x06));
    }
}
DC_ENTRY(C22A44, RTS)                            /* load tech list set [$ED]+15 (C6:FFC0, decompressed to 7F:0400) into 7E00 / 8300 / 8180 / 8480 */
{
    g_cpu.X = 0x0400;
    abs16_set(0x0303, 0x0400);
    dc_set_a8(0x7F);
    abs8_set(0x0305, 0x7F);
    g_cpu.Y = 0x0015;
    g_cpu.A = g_cpu.D;
    u8 v = bus8(lp(0xED) + 0x15);
    dc_set_a8(v);
    if (!(v & 0x80)) {
        dc_set_a8(dc_adc8(asl8(v), v));
        g_cpu.X = g_cpu.A;
        dc_set_mx(0, 0);
        g_cpu.A = bus16(0xC6FFC0 + g_cpu.X);
        abs16_set(0x0300, g_cpu.A);
        dc_set_mx(1, 0);
        dc_set_a8(bus8(0xC6FFC2 + g_cpu.X));
        abs8_set(0x0302, (u8)g_cpu.A);
        dc_jsl(0xC30002, 0xC22A6D);
    }
    static const struct { u16 to, stat; u8 size; } part[4] = {
        {0x7E00, 0x1B38, 7}, {0x8300, 0x1B39, 3}, {0x8180, 0x1B3A, 3}, {0x8480, 0, 2},
    };
    g_cpu.X = 0x0000;
    for (int k = 0; k < 4; k++) {
        if (k == 0) { g_cpu.X = 0x7E00; abs16_set(0x2181, 0x7E00); g_cpu.X = 0; }
        else { g_cpu.Y = part[k].to; abs16_set(0x2181, part[k].to); }
        dc_set_a8(0x7E);
        abs8_set(0x2183, 0x7E);
        dc_set_mx(0, 0);
        g_cpu.A = (u16)(bus16(0x7F0400 + g_cpu.X) & 0x00FF);
        g_cpu.X++;
        g_cpu.Y = g_cpu.A;
        dc_set_mx(1, 0);
        if (part[k].stat) abs8_set(part[k].stat, (u8)g_cpu.A);
        do {
            for (int b = 0; b < part[k].size; b++) {
                dc_set_a8(bus8(0x7F0400 + g_cpu.X));
                abs8_set(0x2180, (u8)g_cpu.A);
                g_cpu.X++;
            }
            g_cpu.Y--;
        } while (g_cpu.Y);
    }
    dc_set_flags_nz16(0);
}
static void mvn_n(u8 dst, u8 src, u16 from, u16 to, u16 n)
{
    g_cpu.X = from; g_cpu.Y = to; g_cpu.A = (u16)(n - 1);
    dc_mvn(dst, src);
}
/* C511 (reached by JMP): character portrait 0D8B bits 8-10 from FF:9E10 into 8800, outlined, uploaded by C548. */
static void menu_portrait(void)
{
    php();
    dc_set_mx(0, 0);
    abs16_set(0x9890, 0x3DC0);
    u16 a = lsr16(abs16(0x0D8B) & 0x0700);
    dc_push16(a);
    g_cpu.A = asl16(asl16(a));
    g_cpu.A = dc_adc16(g_cpu.A, a);
    g_cpu.X = dc_adc16(g_cpu.A, 0x9E10);
    g_cpu.A = dc_pull16();
    mvn_n(0x7E, 0xFF, g_cpu.X, 0x8800, 0x0281);
    g_cpu.X = 0x8800; g_cpu.Y = 0x0014;
    dc_jsr(0xC2962E, 0xC2C53A);
    g_cpu.X = 0xC548;
    dc_jsr(0xC28385, 0xC2C540);
    dc_jsr(0xC2ED08, 0xC2C543);
    plp();
}
/* 9758: menu frame graphics, cursors and palettes into WRAM and VRAM. */
static void menu_gfx_tail(void)
{
    dc_jsr(0xC28663, 0xC29758);
    dc_set_mx(0, 0);
    mvn_n(0x7E, 0xFF, 0x9C68, 0x9480, 8);
    mvn_n(0x7E, 0xFF, 0x9C68, 0x9488, 8);
    mvn_n(0x7E, 0xFF, 0x9C78, 0x94A0, 8);
    mvn_n(0x7E, 0xFF, 0xCBBC, 0x94A8, 8);
    mvn_n(0x7E, 0xFF, 0xCBC4, 0x9560, 0x20);
    g_cpu.X = 0x2E00; g_cpu.Y = 0x1000;
    dc_jsr(0xC29512, 0xC297A1);
    mvn_n(0x7E, 0x7E, 0x5C00, 0x2E80, 0x180);
    mvn_n(0x7E, 0x7E, g_cpu.X, 0x3080, 0x180);
    mvn_n(0x7E, 0x7E, 0x2E00, 0x3200, 0x400);
    g_cpu.X = 0x3200; g_cpu.Y = 0x0020;
    dc_jsr(0xC2962E, 0xC297CB);
    mvn_n(0x7E, 0xFF, 0x9CF0, 0x3200, 0x40);
    mvn_n(0x7E, 0xFF, 0x9D30, 0x3400, 0x40);
    mvn_n(0x7E, 0xFF, 0x9D70, 0x3240, 0x40);
    mvn_n(0x7E, 0xFF, 0x9DB0, 0x3440, 0x40);
    g_cpu.X = 0x3200; g_cpu.Y = 0x0004;
    dc_jsr(0xC29653, 0xC29804);
    g_cpu.X = 0x3400; g_cpu.Y = 0x0004;
    dc_jsr(0xC29653, 0xC2980D);
    g_cpu.X = 0xFC87;
    dc_jsr(0xC28385, 0xC29813);
    dc_jsr(0xC28663, 0xC29816);
    g_cpu.X = 0x2E00; g_cpu.Y = 0x4000;
    dc_jsr(0xC29512, 0xC2981F);
    mvn_n(0x7E, 0xFF, 0x9DF0, 0x9580, 0x20);
    g_cpu.A = 0x0001;
    u16 m = abs16(0x0D15);
    g_cpu._flag_Z = !(m & 1);
    abs16_set(0x0D15, m | 1);
    dc_jsr(0xC286A9, 0xC29834);
    dc_set_mx(1, 0);
    g_cpu.X = 0x0010;
    abs16_set(0x0D47, 0x0010);
    lda8(abs8(0x2991) & 0x07);
    abs8_set(0x0D8C, (u8)g_cpu.A);
    menu_portrait();
}
DC_ENTRY(C29758, RTS) { menu_gfx_tail(); }
/* 9692: the menu's font and window graphics (A = font source in FF), then 9758. */
static void menu_gfx(void)
{
    g_cpu.X = 0x5000; g_cpu.Y = 0x0E00;
    dc_jsr(0xC29542, 0xC29698);
    g_cpu.A = 0xC9FC; g_cpu.X = 0x5100; g_cpu.Y = 0x0100;
    dc_jsr(0xC29542, 0xC296A4);
    dp16_set(0x00, 0x00F0);
    g_cpu.X = 0x4F00; g_cpu.Y = 0x3000;
    do {                                         /* 16 bytes copied, 16 skipped */
        g_cpu.A = 0x000F;
        dc_mvn(0x7E, 0x7E);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.Y, 0x0010);
        g_cpu.Y = g_cpu.A;
    } while (scroll_count_at(0x00));
    g_cpu.X = 0x33E0;
    dc_jsr(0xC29678, 0xC296C5);
    g_cpu.X = 0x4880;
    do {
        dc_jsr(0xC29678, 0xC296CB);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0010);
        g_cpu.X = g_cpu.A;
        dc_cmp16(g_cpu.X, 0x49C0);
    } while (!g_cpu._flag_C);
    mvn_n(0x7E, 0xC2, 0xFC6F, 0x0C00, 0x10);
    g_cpu.A = 0x0010;
    abs16_set(0x0D11, 0x0010);
    dc_jsr(0xC28663, 0xC296EB);
    mvn_n(0x7E, 0xFF, 0xD5E4, 0x4E00, 0x600);
    g_cpu.Y = 0x5400; g_cpu.X = 0xDBE4; g_cpu.A = 0x0095;
    dc_jsr(0xC2960B, 0xC29703);
    g_cpu.X = 0x5400; g_cpu.Y = 0x0008;
    dc_jsr(0xC2962E, 0xC2970C);
    mvn_n(0x7E, 0x7E, 0x4E00, 0x2E00, 0x12A0);
    g_cpu.X = 0xFC7F;
    dc_jsr(0xC28385, 0xC2971E);
    dc_jsr(0xC28663, 0xC29721);
    g_cpu.A = 0x9E10; g_cpu.X = 0x2E00; g_cpu.Y = 0x1400;
    dc_jsr(0xC29542, 0xC2972D);
    mvn_n(0x7E, 0x7E, 0x5F00, 0x4200, 0x1A0);
    g_cpu.X = 0x2E00; g_cpu.Y = 0x00A4;
    dc_jsr(0xC2962E, 0xC29742);
    mvn_n(0x7E, 0xC2, 0xFC8F, 0x0C00, 0x18);
    dc_set_mx(1, 0);
    lda8(0x18);
    abs8_set(0x0D11, 0x18);
    menu_gfx_tail();
}
DC_ENTRY(C29692, RTS) { menu_gfx(); }
DC_ENTRY(C2968D, RTS) { dc_set_mx(0, 0); g_cpu.A = 0x8E60; menu_gfx(); }
/* F422: load the battle-menu style $00 / $01 (E4 tables): five header
 * bytes into 9890, the palette (24 bytes into 9582+) and, when $01 != 0,
 * the compressed graphics into 7E:B000+. */
DC_ENTRY(C2F422, RTS)
{
    php();
    dc_set_mx(1, 1);
    dp8_set(0x18, 0);
    dp8_set(0x12, 0xFF);
    dp8_set(0x03, 0xFF);
    dp8_set(0x1D, 0xE4);
    dp8_set(0x20, 0xE4);
    abs8_set(0x0D15, (u8)(abs8(0x0D15) + 1));
    bus8_set(0x004202, dp8(0x00));
    g_cpu.Y = 0x05;
    u8 a = asl8(dp8(0x01));
    g_cpu.X = a;
    if (a >= 0x04) g_cpu.Y = 0x0A;
    dc_set_a8((u8)g_cpu.Y);
    bus8_set(0x004203, (u8)g_cpu.Y);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16(bus16(0xC2F55C + g_cpu.X), bus16(0x004216));
    g_cpu.Y = 0x9890; g_cpu.A = 0x0004;
    dc_mvn(0x7E, 0xE4);
    dc_set_mx(1, 1);
    g_cpu.X = abs8(0x9894) & 0x03;
    dp8_set(0x02, bus8(0xC2F562 + g_cpu.X));
    u8 m = abs8(0x9894);
    g_cpu._flag_Z = !(m & 0xEF);
    abs8_set(0x9894, m & 0x10);
    abs8_set(0x9895, 0);
    dc_set_a8(abs8(0x9890));
    dc_cmp8((u8)g_cpu.A, 0xFF);
    if (g_cpu._flag_Z) { plp(); return; }
    dc_set_mx(0, 0);
    u16 v = (u16)(g_cpu.A & 0x00FF);
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16((u16)(v * 3), bus16(0xE4FFE0));
    dp16_set(0x05, bus16(0xE40001 + g_cpu.X));
    dp16_set(0x04, bus16(0xE40000 + g_cpu.X));
    v = abs16(0x9891) & 0x00FF;
    g_cpu._flag_C = 0;
    g_cpu.X = dc_adc16((u16)(v * 3), bus16(0xE4FFE2));
    dp16_set(0x08, bus16(0xE40001 + g_cpu.X));
    dp16_set(0x07, bus16(0xE40000 + g_cpu.X));
    g_cpu.A = asl16(abs16(0x9893) & 0x00FF);
    abs16_set(0x0000, g_cpu.A);
    g_cpu.X = dc_adc16(g_cpu.A, bus16(0xE4FFE4));
    dp16_set(0x0A, bus16(0xE40000 + g_cpu.X));
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc16(bus16(0xE40002 + g_cpu.X), dp16(0x0A));
    if (g_cpu._flag_Z) { plp(); return; }
    dp16_set(0x14, lsr16(lsr16(g_cpu.A)));
    g_cpu.X = dc_adc16(abs16(0x0000), bus16(0xE4FFE6));
    dp16_set(0x0C, bus16(0xE40000 + g_cpu.X));
    g_cpu.A = asl16(abs16(0x9892) & 0x00FF);
    g_cpu.X = dc_adc16(g_cpu.A, bus16(0xE4FFE8));
    g_cpu.A = bus16(0xE40000 + g_cpu.X);
    g_cpu.X = abs16(0x9894);
    if (g_cpu.X) { g_cpu._flag_C = 0; g_cpu.A = dc_adc16(g_cpu.A, 0x0200); }
    g_cpu.X = g_cpu.A;
    g_cpu.A = lsr16(lsr16(lsr16(lsr16(dp16(0x19) & 0x0E00))));
    g_cpu.Y = dc_adc16(g_cpu.A, 0x9582);
    g_cpu.A = 0x0017;
    dc_mvn(0x7E, 0xE4);
    dc_set_mx(1, 0);
    lda8(dp8(0x01));
    if (!g_cpu._flag_Z) {
        abs8_set(0x0302, dp8(0x06));
        dc_set_a8(0x7E);
        abs8_set(0x0305, 0x7E);
        dp8_set(0x06, 0x7E);
        dc_set_mx(0, 0);
        abs16_set(0x0300, dp16(0x04));
        g_cpu.A = lsr16(lsr16(lsr16(g_cpu.D & 0x03C0)));
        g_cpu.A = (u16)(g_cpu.A << 8 | g_cpu.A >> 8);
        g_cpu.A = dc_adc16(g_cpu.A, 0xB000);
        abs16_set(0x0303, g_cpu.A);
        dp16_set(0x04, g_cpu.A);
        dc_jsl(0xC30002, 0xC2F542);
    }
    plp();
}
DC_ENTRY(C21947, RTS)                            /* script op: redraw layer [$58]+1 (09C5); advance 2 */
{
    dc_set_mx(1, 0);
    g_cpu.Y = 0x0001;
    dc_set_a8(bus8(lp(0x58) + 1));
    dp8_set(0x00, (u8)g_cpu.A);
    dc_jsr(0xC209C5, 0xC21950);
    dc_set_mx(0, 0);
    lda16(0x0002);
}
DC_ENTRY(C2D10D, RTS)                            /* the three party slots (D131), rows at 4EC6 */
{
    g_cpu.X = 0x4EC6;
    dp16_set(0x61, 0x4EC6);
    dp8_set(0x79, 0);
    dc_set_a8(0x20);
    abs8_set(0x0DC7, 0x20);
    do {
        dc_jsr(0xC2D131, 0xC2D119);
        abs8_set(0x0062, (u8)(abs8(0x0062) + 1));
        g_cpu._flag_C = 0;
        dc_set_a8(dc_adc8(abs8(0x0DC7), 0x20));
        abs8_set(0x0DC7, (u8)g_cpu.A);
        dp8_set(0x79, (u8)(dp8(0x79) + 1));
        dc_set_a8(dp8(0x79));
        dc_cmp8((u8)g_cpu.A, 0x03);
    } while (!g_cpu._flag_C);
}
DC_ENTRY(C20CEA, RTS)                            /* draw every sprite in the B500 list (0D08) */
{
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    g_cpu.DB = 0x7E;
    g_cpu.Y = 0xB500;
    g_cpu.X = abs16(0xB502);
    for (;;) {
        dc_cmp16(g_cpu.X, 0xB500);
        if (g_cpu._flag_Z) break;
        dp16_set(0x10, g_cpu.X);
        dc_jsr(0xC20D08, 0xC20CFC);
        g_cpu.Y = dp16(0x10);
        g_cpu.X = abs16((u16)(g_cpu.Y + 2));
    }
    plb();
}
DC_ENTRY(C29137, RTS)                            /* stat totals (916E), double 9B21 for weapon A8, copy them to the character ($6F) and 9AC6 */
{
    php();
    dc_jsr(0xC2916E, 0xC29138);
    dc_set_mx(1, 0);
    dc_set_a8(abs8(0x9ABA));
    dc_cmp8((u8)g_cpu.A, 0xA8);
    if (g_cpu._flag_Z) abs8_set(0x9B21, asl8(abs8(0x9B21)));
    dc_set_mx(0, 0);
    g_cpu.Y = dp16(0x6F);
    g_cpu.A = abs16(0x9A93);
    abs16_set((u16)(g_cpu.Y + 3), g_cpu.A);
    g_cpu._flag_C = 0;
    mvn_n(0x7E, 0x7E, 0x9B1C, dc_adc16(g_cpu.Y, 0x0036), 0x0B);
    mvn_n(0x7E, 0x7E, 0x9B1C, 0x9AC6, 0x0B);
    plp();
}
DC_ENTRY(C241B5, RTS)                            /* blocked sides of the box ($08,$0A)-($09,$0B): bit 0 first side, bit 1 second */
{
    dc_set_mx(1, 0);
    dp8_set(0x0E, 0);
    dp8_set(0x05, dp8(0x08)); dp8_set(0x06, dp8(0x0A)); dp8_set(0x07, dp8(0x09));
    dc_jsr(0xC24205, 0xC241C5);
    if (!g_cpu._flag_C) {
        dp8_set(0x07, dp8(0x0B));
        dc_jsr(0xC2425A, 0xC241D4);
    }
    if (g_cpu._flag_C) { dc_set_a8(0x01); dp8_set(0x0E, (u8)(dp8(0x0E) | 0x01)); }
    dp8_set(0x05, dp8(0x08)); dp8_set(0x06, dp8(0x0A)); dp8_set(0x07, dp8(0x0B));
    dc_jsr(0xC2425A, 0xC241E9);
    if (!g_cpu._flag_C) {
        dp8_set(0x07, dp8(0x09));
        dc_jsr(0xC24205, 0xC241F8);
    }
    if (g_cpu._flag_C) { dc_set_a8(0x02); dp8_set(0x0E, (u8)(dp8(0x0E) | 0x02)); }
    g_cpu.A = g_cpu.D;
    lda8(dp8(0x0E));
}
DC_ENTRY(C20E1D, RTS)                            /* run object $4E's sprite script at [0B,X] until an op returns 0 */
{
    dc_set_mx(1, 0);
    ldx16(dp16(0x4E));
    dc_set_a8(abs8((u16)(g_cpu.X + 0x0D)));
    dp8_set(0x54, (u8)g_cpu.A);
    dc_set_mx(0, 0);
    g_cpu.A = abs16((u16)(g_cpu.X + 0x0B));
    dp16_set(0x52, g_cpu.A);
    for (;;) {
        g_cpu.A = asl16(bus16(lp(0x52)) & 0x00FF);
        g_cpu.X = g_cpu.A;
        dc_jsr(0xC20000 | bus16(0xC20E4A + g_cpu.X), 0xC20E34);   /* 0E5A..0F35 */
        if (g_cpu._flag_Z) break;
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.A, dp16(0x52));
        dp16_set(0x52, g_cpu.A);
    }
    ldx16(dp16(0x4E));
    g_cpu.A = dp16(0x52);
    dc_set_flags_nz16(g_cpu.A);
    abs16_set((u16)(g_cpu.X + 0x0B), g_cpu.A);
    dc_set_mx(1, 0);
}
DC_ENTRY(C25374, RTS)                            /* aim object $4E at its target (229D): speed from the direction; C clear if already there */
{
    dc_set_mx(0, 0);
    g_cpu.X = dp16(0x4E);
    abs16_set((u16)(g_cpu.X + 0x1A), 0);
    abs16_set((u16)(g_cpu.X + 0x1C), 0);
    abs16_set((u16)(g_cpu.X + 0x1E), 0);
    abs16_set((u16)(g_cpu.X + 0x20), 0);
    dp16_set(0x08, abs16((u16)(g_cpu.X + 0x14)));
    dp16_set(0x0A, abs16((u16)(g_cpu.X + 0x18)));
    dp16_set(0x0C, abs16((u16)(g_cpu.X + 0x2A)));
    dp16_set(0x0E, abs16((u16)(g_cpu.X + 0x2C)));
    dc_jsr(0xC2229D, 0xC25398);
    dp16_set(0x08, g_cpu.A);
    g_cpu.Y = g_cpu.X;
    if (!g_cpu.X) {
        dc_set_flags_nz16(0);
        dc_set_mx(1, 0);
        g_cpu._flag_C = 0;
        return;
    }
    g_cpu.Y = dp16(0x4E);
    dc_jsl(0xC22273, 0xC253A6);
    g_cpu.A = asl16(asl16(g_cpu.A));
    abs16_set((u16)(g_cpu.Y + 0x1B), g_cpu.A);
    g_cpu.A = dp16(0x08);
    dc_jsl(0xC22277, 0xC253B1);
    g_cpu.A = asl16(asl16(g_cpu.A));
    abs16_set((u16)(g_cpu.Y + 0x1F), g_cpu.A);
    g_cpu.A = g_cpu.D;
    dc_set_mx(1, 0);
    lda8(abs8((u16)(g_cpu.Y + 0x1C)));
    if (g_cpu._flag_N) { lda8(0xFF); abs8_set((u16)(g_cpu.Y + 0x1D), 0xFF); }
    lda8(abs8((u16)(g_cpu.Y + 0x20)));
    if (g_cpu._flag_N) { lda8(0xFF); abs8_set((u16)(g_cpu.Y + 0x21), 0xFF); }
    g_cpu._flag_C = 1;
}
/* Add sign-extended $01 to scroll $o unless 1BA7 bit 4, 1BAA bit 5 or 1BAF bit 1 forbid it. */
static void scroll_add(u8 o)
{
    lda8(abs8(0x1BA7));
    if (g_cpu.A & 0x10) return;
    lda8(abs8(0x1BAA));
    if (g_cpu.A & 0x20) return;
    lda8(abs8(0x1BAF));
    if (g_cpu.A & 0x02) return;
    g_cpu.A = g_cpu.D;
    lda8(dp8(0x01));
    dc_set_mx(0, 0);
    if (g_cpu._flag_N) g_cpu.A |= 0xFF00;
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(g_cpu.A, dp16(o));
    dp16_set(o, g_cpu.A);
}
/* Shared part of the camera script ops: count down 0A,X (reloaded from
 * [$58]+n); move (1759/0F38), then scroll horizontally (1784) and
 * vertically (17AB). */
static bool cam_count(u8 n, u16 done)
{
    dc_set_mx(1, 0);
    ldx16(dp16(0x4E));
    u8 t = abs8((u16)(g_cpu.X + 0x0A));
    dc_set_a8(t);
    if (!t) {
        g_cpu.Y = n;
        dc_set_a8(bus8(lp(0x58) + n));
        abs8_set((u16)(g_cpu.X + 0x0A), (u8)g_cpu.A);
        dc_set_mx(0, 0);
        abs16_set((u16)(g_cpu.X + 0x12), 0);
        abs16_set((u16)(g_cpu.X + 0x14), 0);
        abs16_set((u16)(g_cpu.X + 0x16), 0);
        abs16_set((u16)(g_cpu.X + 0x18), 0);
    } else {
        t--;
        abs8_set((u16)(g_cpu.X + 0x0A), t);
        if (!t) { dc_set_mx(0, 0); lda16(done); return false; }
    }
    dc_set_mx(0, 0);
    return true;
}
DC_ENTRY(C2164B, RTS)                            /* script op: move the camera [$58]+1 frames, scrolling layers 1 and 2 */
{
    if (!cam_count(1, 0x0002)) return;
    dc_jsr(0xC21759, 0xC21679);
    dc_jsr(0xC20F38, 0xC2167C);
    dc_jsr(0xC21784, 0xC2167F);
    if (g_cpu._flag_C) {
        dc_set_mx(1, 0);
        lda8(0x01); dp8_set(0x00, 0x01);
        dc_jsr(0xC20568, 0xC2168A);
        lda8(0x02); dp8_set(0x00, 0x02);
        dc_jsr(0xC20568, 0xC21691);
        scroll_add(0x24);
    }
    dc_set_mx(0, 0);
    ldx16(dp16(0x4E));
    dc_jsr(0xC217AB, 0xC216BC);
    if (g_cpu._flag_C) {
        dc_set_mx(1, 0);
        lda8(0x01); dp8_set(0x00, 0x01);
        dc_jsr(0xC2066C, 0xC216C7);
        lda8(0x02); dp8_set(0x00, 0x02);
        dc_jsr(0xC2066C, 0xC216CE);
        scroll_add(0x2A);
    }
    dc_set_mx(0, 0);
    lda16(g_cpu.D);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C216FA, RTS)                            /* script op: scroll layer [$58]+1 for [$58]+2 frames */
{
    if (!cam_count(2, 0x0003)) return;
    dc_jsr(0xC21759, 0xC21728);
    dc_jsr(0xC20F38, 0xC2172B);
    dc_jsr(0xC21784, 0xC2172E);
    if (g_cpu._flag_C) {
        dc_set_mx(1, 0);
        g_cpu.Y = 0x0001;
        dc_set_a8(bus8(lp(0x58) + 1));
        dp8_set(0x00, (u8)g_cpu.A);
        dc_jsr(0xC20568, 0xC2173C);
    }
    dc_set_mx(0, 0);
    ldx16(dp16(0x4E));
    dc_jsr(0xC217AB, 0xC21743);
    if (g_cpu._flag_C) {
        dc_set_mx(1, 0);
        g_cpu.Y = 0x0001;
        dc_set_a8(bus8(lp(0x58) + 1));
        dp8_set(0x00, (u8)g_cpu.A);
        dc_jsr(0xC2066C, 0xC21751);
    }
    dc_set_mx(0, 0);
    lda16(g_cpu.D);
    g_cpu._flag_C = 0;
}
/* 27BC (reached by JMP): decompress the C6:FDF3 entry into 7E:C800. */
static void load_c6fdf3(void)
{
    g_cpu.X = 0xC800;
    abs16_set(0x0303, 0xC800);
    dc_set_a8(0x7E);
    abs8_set(0x0305, 0x7E);
    dc_set_mx(0, 0);
    g_cpu.A = bus16(0xC6FDF3);
    abs16_set(0x0300, g_cpu.A);
    dc_set_mx(1, 0);
    dc_set_a8(bus8(0xC6FDF5));
    abs8_set(0x0302, (u8)g_cpu.A);
    dc_jsl(0xC30002, 0xC227D9);
}
DC_ENTRY(C22CC1, RTS)                            /* ending-mode graphics: decompress (2768..2882) and DMA to VRAM, palette, then 2D91 */
{
    static const struct { u32 load, site; u16 vram, size; } part[5] = {
        {0xC22768, 0xC22CC1, 0x0000, 0x4000}, {0xC22B61, 0xC22CD0, 0x1000, 0x1000},
        {0xC227DE, 0xC22CDF, 0x2000, 0x7000}, {0xC22826, 0xC22CEE, 0x7000, 0x1000},
        {0xC22854, 0xC22CFF, 0x7800, 0x1000},
    };
    static const u16 dma_site[5] = {0x2CCD, 0x2CDC, 0x2CEB, 0x2CFC, 0x2D0D};
    for (int k = 0; k < 5; k++) {
        dc_jsr(part[k].load, part[k].site);
        if (k >= 3) dc_set_mx(0, g_cpu.x_flag);
        g_cpu.X = k == 1 ? 0xB000 : 0x9000;
        g_cpu.Y = part[k].vram;
        g_cpu.A = part[k].size;
        dc_jsr(0xC22D70, 0xC20000 | dma_site[k]);
    }
    dc_jsr(0xC22882, 0xC22D10);
    g_cpu.A = g_cpu.D;
    bus16_set(0x7F8D00, g_cpu.A);
    g_cpu.A = 0x14A5; bus16_set(0x7F8D02, 0x14A5);
    g_cpu.A = 0x294A; bus16_set(0x7F8D04, 0x294A);
    g_cpu.A = 0x6B5A; bus16_set(0x7F8D06, 0x6B5A);
    dc_push8(g_cpu.DB);
    mvn_n(0x00, 0x7F, 0x8C00, 0x0920, 0x0201);
    mvn_n(0x00, 0xC6, 0xF620, 0x0B00, 0x0021);
    plb();
    dc_set_mx(1, 0);
    lda8(0x00);
    abs8_set(0x2121, 0x00);
    g_cpu.X = 0x2200; abs16_set(0x4370, 0x2200);   /* palette 7F:8C00 by DMA */
    g_cpu.X = 0x8C00; abs16_set(0x4372, 0x8C00);
    dc_set_a8(0x7F); abs8_set(0x4374, 0x7F);
    g_cpu.X = 0x0200; abs16_set(0x4375, 0x0200);
    dc_set_a8(0x80); abs8_set(0x420B, 0x80);
    dc_jsr(0xC22D91, 0xC22D6A);
    load_c6fdf3();
}
DC_ENTRY(C299B2, RTS)                            /* stat totals (8816 + 9137) for all 7 characters */
{
    php();
    dc_set_mx(0, 0);
    dp16_set(0x02, 0);
    do {
        g_cpu.A = dp16(0x02);
        dc_jsr(0xC28816, 0xC299B9);
        dc_jsr(0xC29137, 0xC299BC);
        dp16_set(0x02, (u16)(dp16(0x02) + 1));
        g_cpu.A = dp16(0x02);
        dc_cmp16(g_cpu.A, 0x0007);
    } while (!g_cpu._flag_C);
    plp();
}
/* Script-op frame counter at 0A,X (loaded from [$58]+1): false (A = 2,
 * op done) when it runs out. */
static bool op_count(void)
{
    dc_set_mx(1, 0);
    ldx16(dp16(0x4E));
    u8 t = abs8((u16)(g_cpu.X + 0x0A));
    dc_set_a8(t);
    if (!t) {
        g_cpu.Y = 0x0001;
        dc_set_a8(bus8(lp(0x58) + 1));
        abs8_set((u16)(g_cpu.X + 0x0A), (u8)g_cpu.A);
        return true;
    }
    t--;
    abs8_set((u16)(g_cpu.X + 0x0A), t);
    if (t) return true;
    dc_set_mx(0, 0);
    lda16(0x0002);
    return false;
}
DC_ENTRY(C2188E, RTS)                            /* script op: run the sprite script for [$58]+1 frames */
{
    if (!op_count()) return;
    dc_jsr(0xC20E1D, 0xC218AC);
    dc_set_mx(0, 0);
    lda16(g_cpu.D);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C2161D, RTS)                            /* script op: move (0F38, 1CB8) and animate for [$58]+1 frames */
{
    if (!op_count()) return;
    dc_set_mx(0, 0);
    dc_jsr(0xC20F38, 0xC2163D);
    dc_jsr(0xC21CB8, 0xC21640);
    dc_jsr(0xC20E1D, 0xC21643);
    dc_set_mx(0, 0);
    lda16(g_cpu.D);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C22C1D, RTS)                            /* enter the tech/ending screen: clear state, load all graphics, draw layers 1-2 */
{
    static const u16 zero8[] = {0x027C, 0x027D};
    static const u16 zero8b[] = {0x0280, 0x0281, 0x0282, 0x1B58, 0x1B59, 0x1BF7, 0x02B1, 0x02B2, 0x02B3};
    for (unsigned k = 0; k < 2; k++) abs8_set(zero8[k], 0);
    dp8_set(0x47, 0); dp8_set(0x48, 0); dp8_set(0x49, 0);
    for (unsigned k = 0; k < sizeof zero8b / sizeof zero8b[0]; k++) abs8_set(zero8b[k], 0);
    g_cpu.X = 0xFFFF;
    abs16_set(0x1B32, 0xFFFF); abs16_set(0x1B34, 0xFFFF); abs16_set(0x1B36, 0xFFFF);
    dc_set_a8(bus8(0x7E29AE));
    abs8_set(0x02AE, (u8)g_cpu.A);
    dc_set_a8(0x04);
    u8 m = abs8(0x0294);
    g_cpu._flag_Z = !(m & 0x04);
    abs8_set(0x0294, m & ~0x04);
    static const u16 load[] = {0x274D, 0x2CC1, 0x295E, 0x298C, 0x29BA, 0x29E8, 0x2A44, 0x2A16, 0x2902, 0x2930, 0x2BFB};
    for (unsigned k = 0; k < sizeof load / sizeof load[0]; k++)
        dc_jsr(0xC20000 | load[k], 0xC22C5C + 3 * k);
    lda8(0x01); dp8_set(0x00, 0x01);
    dc_jsr(0xC209C5, 0xC22C81);
    lda8(0x02); dp8_set(0x00, 0x02);
    dc_jsr(0xC209C5, 0xC22C88);
    g_cpu.X = 0x0400;
    lda8(0x7F);
    dc_C204D9_glue();
}
/* 3FE3: JMP (3FE8,X) on A = 41B5's result; true for 4060 (search again). */
static bool path_dispatch(void)
{
    dc_set_a8(asl8((u8)g_cpu.A));
    g_cpu.X = g_cpu.A;
    u16 to = bus16(0xC20000 | (u16)(0x3FE8 + g_cpu.X));
    switch (to) {
    case 0x3FF0: dc_C23FF0_glue(); break;
    case 0x404C: dc_C2404C_glue(); break;
    case 0x4056: dc_C24056_glue(); break;
    case 0x4060: return true;
    default: bad_jump(0xC23FE5, to);
    }
    return false;
}
/* Path search: try up to 16 earlier points of the 1B5B/1B7B trail (index
 * $19 stepping back by 2) until the box to it is not fully blocked (41B5
 * != 3), then dispatch on the result through 3FE8. */
DC_ENTRY(C24060, RTS)
{
again:
    dc_set_a8(0x10);
    dp8_set(0x16, 0x10);
    dc_set_a8((u8)((abs8(0x1B5A) - 2) & 0x1F));
    dp8_set(0x19, (u8)g_cpu.A);
    dp8_set(0x1A, 0);
    for (;;) {
        dc_set_mx(0, 0);
        g_cpu.X = dp16(0x4E);
        g_cpu.Y = dp16(0x19);
        dp16_set(0x08, lsr16(lsr16(lsr16(abs16((u16)(g_cpu.X + 0x14))))));
        dp16_set(0x09, lsr16(lsr16(lsr16(abs16((u16)(0x1B5B + g_cpu.Y))))));
        dp16_set(0x0A, lsr16(lsr16(lsr16(abs16((u16)(g_cpu.X + 0x18))))));
        g_cpu.A = lsr16(lsr16(lsr16(abs16((u16)(0x1B7B + g_cpu.Y)))));
        dp16_set(0x0B, g_cpu.A);
        dc_set_mx(1, 0);
        dc_jsr(0xC241B5, 0xC24097);
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (!g_cpu._flag_Z) {
            dp8_set(0x1B, (u8)g_cpu.A);
            dc_set_mx(0, 0);
            g_cpu.Y = dp16(0x19);
            g_cpu.X = dp16(0x4E);
            g_cpu.A = abs16((u16)(0x1B5B + g_cpu.Y)) & 0xFFF8;
            abs16_set((u16)(g_cpu.X + 0x2A), g_cpu.A);
            g_cpu.A = abs16((u16)(0x1B7B + g_cpu.Y)) & 0xFFF8;
            abs16_set((u16)(g_cpu.X + 0x2C), g_cpu.A);
            dc_jsr(0xC23FA7, 0xC240D9);
            g_cpu.A = g_cpu.D;
            dc_set_mx(1, 0);
            lda8(dp8(0x1B));
            break;
        }
        dc_set_mx(0, 0);
        g_cpu.A = (u16)((dp16(0x19) - 2) & 0x001F);
        dp16_set(0x19, g_cpu.A);
        dc_set_mx(1, 0);
        if (dec8_dp(0x16)) continue;
        dc_set_mx(0, 0);
        g_cpu.X = dp16(0x4E);
        dc_jsr(0xC23F6D, 0xC240B3);
        dc_jsr(0xC23FA7, 0xC240B6);
        g_cpu.A = g_cpu.D;
        dc_set_mx(1, 0);
        break;
    }
    if (path_dispatch()) goto again;
}
DC_ENTRY(C23FBC, RTS)                            /* box from object $4E to the target 0283/0285 (41B5), then dispatch */
{
    g_cpu.X = dp16(0x4E);
    dp16_set(0x08, lsr16(lsr16(lsr16(abs16((u16)(g_cpu.X + 0x14))))));
    dp16_set(0x09, lsr16(lsr16(lsr16(abs16(0x0283)))));
    dp16_set(0x0A, lsr16(lsr16(lsr16(abs16((u16)(g_cpu.X + 0x18))))));
    g_cpu.A = lsr16(lsr16(lsr16(abs16(0x0285))));
    dp16_set(0x0B, g_cpu.A);
    dc_set_mx(1, 0);
    dc_jsr(0xC241B5, 0xC23FE0);
    if (path_dispatch()) dc_C24060_glue();
}
DC_ENTRY(C28D6A, RTS)                            /* shop: try the 3 party slots (FF:F9C4) until one accepts; 7F:0061 = 2 if so, else 0 */
{
    dc_set_mx(1, 1);
    dp8_set(0x79, 0);
    for (;;) {
        dc_jsl(0xFFF9C4, 0xC28D6E);
        lda8((u8)g_cpu.A);
        if (g_cpu._flag_Z) {
            dc_jsr(0xC295BB, 0xC28D85);
            dc_set_a8(0x02);
            break;
        }
        dp8_set(0x79, (u8)(dp8(0x79) + 1));
        dc_set_a8(dp8(0x79));
        dc_cmp8((u8)g_cpu.A, 0x03);
        if (!g_cpu._flag_C) continue;
        dc_jsr(0xC2956E, 0xC28D7E);
        dc_set_a8(0x00);
        break;
    }
    lda8((u8)g_cpu.A);
    bus8_set(0x7F0061, (u8)g_cpu.A);
}
/* Shop/inventory service A (table 8C5B), item X, count Y; D = 0400, DB = 7E. Returns A = 00:result. */
static void shop_service(void)
{
    dc_push8(g_cpu.DB);
    dc_push16(g_cpu.D);
    php();
    dc_set_mx(1, 1);
    g_cpu.DB = 0x7E;
    g_cpu.D = 0x0400;
    dc_set_flags_nz16(0x0400);
    dp8_set(0xC0, (u8)g_cpu.Y);
    dp8_set(0xC9, (u8)g_cpu.X);
    dc_set_a8(asl8((u8)g_cpu.A));
    g_cpu.X = (u8)g_cpu.A;
    dc_jsr(0xC20000 | bus16(0xC20000 | (u16)(0x8C5B + g_cpu.X)), 0xC28C4A);
    if (g_cpu.m_flag) dp8_set(0xC1, (u8)g_cpu.A);
    else dp16_set(0xC1, g_cpu.A);            /* 8D8F returns with A 16-bit */
    dc_jsr(0xC2834D, 0xC28C4F);
    if (g_cpu.m_flag) {
        g_cpu.A = (u16)(g_cpu.A >> 8);
        lda8(dp8(0xC1));
    } else
        lda16(dp16(0xC1));                   /* A9 00 EB = LDA #$EB00, then LDA $C1 */
    plp();
    g_cpu.D = dc_pull16();
    plb();
}
DC_ENTRY(C28C36, RTS) { shop_service(); }
DC_ENTRY(C28004, RTL) { dc_jsr(0xC28C36, 0xC2800A); }
DC_ENTRY(C28008, RTL) { dp16_set(0x6B, g_cpu.Y); dc_jsr(0xC28C36, 0xC2800A); }
DC_ENTRY(C29875, RTS)                            /* build the item tables (7700..7D00) from CC:0000 and the save flags */
{
    php();
    dc_set_mx(0, 0);
    dc_jsr(0xC29988, 0xC29878);
    abs16_set(0x0D7C, 0);
    abs16_set(0x0D7E, 0);
    g_cpu.A = bus16(0x7F00BA);
    if (g_cpu.A & 0x0001) {
        abs16_set(0x0D7C, (u16)(abs16(0x0D7C) - 1));
        if (!(g_cpu.A & 0x0002)) abs16_set(0x0D7E, (u16)(abs16(0x0D7E) - 1));
    }
    abs16_set(0x0D32, 0);
    abs16_set(0x0D34, 0);
    g_cpu.A = 0xFFFF;
    abs16_set(0x0D36, 0xFFFF);
    g_cpu.A = abs16(0x0100);
    dc_cmp16(g_cpu.A, 0x01F0);
    if (!g_cpu._flag_C) {
        abs16_set(0x0D32, (u16)(abs16(0x0D32) - 1));
        lda16(bus16(0x7F01CE));
        if (!g_cpu._flag_N) abs16_set(0x0D34, (u16)(abs16(0x0D34) - 1));
    }
    dp16_set(0x00, 0);
    do {
        dc_set_mx(1, 0);
        dc_set_a8(dp8(0x00));
        dc_jsr(0xC288AD, 0xC298BB);
        g_cpu.Y = dp16(0x00);
        abs8_set((u16)(0x7700 + g_cpu.Y), bus8(0xCC0000 + g_cpu.X) & 0x1E);
        abs8_set((u16)(0x7A00 + g_cpu.Y), bus8(0xCC0003 + g_cpu.X));
        dc_set_a8(bus8(0xCC0004 + g_cpu.X));
        abs8_set((u16)(0x7B00 + g_cpu.Y), (u8)g_cpu.A);
        dc_set_mx(0, 0);
        g_cpu.Y = asl16(dp16(0x00));
        g_cpu.A = bus16(0xCC0001 + g_cpu.X);
        abs16_set((u16)(0x7800 + g_cpu.Y), g_cpu.A);
        dp16_set(0x00, (u16)(dp16(0x00) + 1));
        g_cpu.A = dp16(0x00);
        dc_cmp16(g_cpu.A, 0x00F2);
    } while (!g_cpu._flag_C);
    dp16_set(0x00, 0x0B5E);
    g_cpu.Y = dp16(0x51);
    do {
        g_cpu.A = dp16(0x00);
        abs16_set((u16)(0x7D00 + g_cpu.Y), g_cpu.A);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.A, 0x000B);
        dp16_set(0x00, g_cpu.A);
        g_cpu.Y += 2;
        dc_cmp16(g_cpu.Y, 0x01E4);
    } while (!g_cpu._flag_C);
    abs16_set(0x7ABC, 0);
    mvn_n(0x7E, 0x7E, 0x7ABC, 0x7ABE, 0x34);
    abs16_set(0x7B94, 0);
    mvn_n(0x7E, 0x7E, 0x7B94, 0x7B96, 0x5C);
    g_cpu.A = 0x0094;
    dp16_set(0x00, 0x0094);
    dc_set_mx(1, 0);
    do {
        dc_set_a8(dp8(0x00));
        dc_jsr(0xC28881, 0xC2992D);
        dc_jsr(0xC288D8, 0xC29930);
        g_cpu.Y = dp16(0x00);
        dc_set_a8(bus8(0xCC0001 + g_cpu.X));
        if (g_cpu.A & 0x40) {
            dc_set_a8(bus8(0xCC0002 + g_cpu.X));
            abs8_set((u16)(0x7B00 + g_cpu.Y), (u8)g_cpu.A);
        }
        dp8_set(0x00, (u8)(dp8(0x00) + 1));
        dc_set_a8(dp8(0x00));
        dc_cmp8((u8)g_cpu.A, 0xBC);
    } while (!g_cpu._flag_C);
    dp8_set(0x00, 0);
    dp8_set(0x01, 0);
    do {
        dc_set_a8(dp8(0x00));
        dc_jsr(0xC28881, 0xC29952);
        dc_jsr(0xC288D8, 0xC29955);
        g_cpu.Y = dp16(0x00);
        dc_set_a8(bus8(0xCC0000 + g_cpu.X));
        abs8_set((u16)(0x7C00 + g_cpu.Y), (u8)g_cpu.A);
        dp8_set(0x00, (u8)(dp8(0x00) + 1));
        dc_set_a8(dp8(0x00));
        dc_cmp8((u8)g_cpu.A, 0x94);
    } while (!g_cpu._flag_C);
    lda8(abs8(0x0D34));
    if (!g_cpu._flag_Z) { dc_set_a8(0x02); abs8_set(0x77C8, (u8)(abs8(0x77C8) & ~0x02)); }
    dc_jsl(0xFFF813, 0xC29973);
    dc_jsr(0xC299B2, 0xC29977);
    g_cpu.X = abs16(0x2C53);
    abs16_set(0x9D96, g_cpu.X);
    dc_set_a8(abs8(0x2C55));
    abs8_set(0x9D98, (u8)g_cpu.A);
    plp();
}
