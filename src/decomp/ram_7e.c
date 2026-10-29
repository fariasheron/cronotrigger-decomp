/* Bank 7E: routines that run from WRAM. C3:0000 unpacks the code of the
 * C3 scene engine (the script machine at C3:0AFF and its helpers) to
 * 7E:3000.. before jumping there; the other blocks (7E:5978.., 7E:8538..)
 * belong to the scenes that load them. Absolute stores go through DB like
 * the original (the I/O writes assume DB = 00). Fuzzed from
 * tools/decomp/scen/ram7e.sav (tools/decomp/mkram7e.py), where the code is
 * present in WRAM. */
#include "ram.h"
#include "ops65.h"

/* DMA channel 7 (control 4370, B-bus register 4371, source 4372-4374,
 * count 4375-4376) started through 420B. */
static void dma7_regs(u8 ctl, u8 bbus, u16 src, u8 bank, u16 count)
{
    abs8_set(0x4370, ctl);
    abs8_set(0x4371, bbus);
    abs16_set(0x4372, src);
    abs8_set(0x4374, bank);
    abs16_set(0x4375, count);
}
static void dma7_start(void) { lda8(0x80); abs8_set(0x420B, 0x80); }

/* 7E:37F3  JSR to the WRAM routine whose address is in 00F0 (X = 0). */
DC_ENTRY(7E37F3, RTL)
{
    ldx(0);
    dc_jsr(0x7E0000 | bus16(0x7E00F0), 0x7E37F6);
}

/* 7E:9919  Copy the bytes 0064 / 0065 to 64D6 / 64DA (8-bit, back to 16-bit A). */
DC_ENTRY(7E9919, RTS)
{
    dc_set_mx(1, g_cpu.x_flag);
    lda8(abs8(0x0064)); abs8_set(0x64D6, (u8)g_cpu.A);
    lda8(abs8(0x0065)); abs8_set(0x64DA, (u8)g_cpu.A);
    dc_set_mx(0, g_cpu.x_flag);
}

/* 7E:90A7  VRAM fill: X bytes of the fixed source byte 7E:350B to 2118. */
DC_ENTRY(7E90A7, RTS)
{
    u16 n = g_cpu.X;
    g_cpu.Y = 0x1808;
    g_cpu.X = 0x350B;
    dma7_regs(0x08, 0x18, 0x350B, 0x7E, n);
    dma7_start();
}

/* 7E:8752  DMA 7F:2200, 2000 bytes, to the VRAM low bytes (2115 = 0). */
DC_ENTRY(7E8752, RTS)
{
    abs8_set(0x2115, 0);
    g_cpu.X = 0x2000;
    dma7_regs(0x00, 0x18, 0x2200, 0x7F, 0x2000);
    dma7_start();
}

/* 7E:90C1  0A04 = (0A04 + 0B10) & 03FF (a scroll position wrapping at 1024). */
DC_ENTRY(7E90C1, RTS)
{
    g_cpu._flag_C = 0;
    lda16(dc_adc16(abs16(0x0A04), abs16(0x0B10)) & 0x03FF);
    abs16_set(0x0A04, g_cpu.A);
}

/* 7E:38E7  DMA channel 7 with the parameters in WRAM: control 00F0, B-bus
 * register 00F2, source 0303-0305, count 0306. */
DC_ENTRY(7E38E7, RTL)
{
    g_cpu.X = abs16(0x0306);
    dma7_regs(abs8(0x00F0), abs8(0x00F2), abs16(0x0303), abs8(0x0305), g_cpu.X);
    dma7_start();
}

/* 7E:89ED  DMA source (0303-0305) = 7F:0200; A = 0002. */
DC_ENTRY(7E89ED, RTS)
{
    ram16_set(0x0304, 0x7F00);
    ram16_set(0x0303, 0x0200);
    g_cpu.A = 0x0002;
}

/* 7E:9702  Copy 7E:C995..CA95 (101 bytes) to 7E:2200. */
DC_ENTRY(7E9702, RTS)
{
    g_cpu.X = 0xC995; g_cpu.Y = 0x2200;
    lda16(0x0100);
    dc_mvn(0x7E, 0x7E);
}

/* 7E:388B  DMA 7E:2200, 200 bytes, to CGRAM from color 0. */
DC_ENTRY(7E388B, RTS)
{
    abs8_set(0x4370, 0x00);
    abs8_set(0x2121, 0x00);
    g_cpu.X = 0x0200;
    dma7_regs(0x00, 0x22, 0x2200, 0x7E, 0x0200);
    dma7_start();
}

/* 7E:9B46  Y = (dp 0C - 1) * 4, X = dp 10. */
DC_ENTRY(7E9B46, RTS)
{
    u16 a = (u16)(dp16(0x0C) - 1);
    g_cpu._flag_C = (a >> 14) & 1;
    lda16((u16)(a << 2));
    g_cpu.Y = g_cpu.A;
    ldx(dp16(0x10));
}

/* 7E:9080  Store 0A0C / 2 and 0A0C / 2 + carry (rounded up) into the HDMA
 * tables at 7E:902F/904D/9043/452B and 7E:9050/9032/9046/452E. */
DC_ENTRY(7E9080, RTS)
{
    u8 v = abs8(0x0A0C);
    g_cpu._flag_C = v & 1;
    v >>= 1;
    static const u16 lo[] = { 0x902F, 0x904D, 0x9043, 0x452B };
    static const u16 hi[] = { 0x9050, 0x9032, 0x9046, 0x452E };
    for (int k = 0; k < 4; k++) ram8_set(lo[k], v);
    u8 r = dc_adc8(v, 0);
    dc_set_a8(r);
    for (int k = 0; k < 4; k++) ram8_set(hi[k], r);
}

/* 7E:8772  With DB = 7E: 7E:21E2/21E4/21E6 = 77BD / 4631 / 1CE7 (three
 * colors), 7E:2000 = 0000. */
DC_ENTRY(7E8772, RTS)
{
    dc_push16(0x007E);        /* PEA $007E; PLB: DB = 7E, the 00 stays for the PLB at the end */
    g_cpu.DB = dc_pull8();
    abs16_set(0x21E2, 0x77BD);
    abs16_set(0x21E4, 0x4631);
    abs16_set(0x21E6, 0x1CE7);
    ldx(0x0000);
    abs16_set(0x2000, 0x0000);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}

/* 7E:3843  Mode 7 matrix A = 0001, B = E0 (high byte left at 0 after the
 * DMA), and DMA 00:0700, 200 bytes, to 2134 in mode 80 (B to A: the
 * multiply result read back into 00:0700). */
DC_ENTRY(7E3843, RTS)
{
    abs8_set(0x211B, 0x01);
    abs8_set(0x211B, 0x00);
    abs8_set(0x211C, 0xE0);
    abs8_set(0x4374, 0x00);
    abs8_set(0x4371, 0x34);
    abs16_set(0x4372, 0x0700);
    g_cpu.X = 0x0200;
    abs16_set(0x4375, 0x0200);
    lda8(0x80);
    abs8_set(0x4370, 0x80);
    abs8_set(0x420B, 0x80);
    abs8_set(0x211C, 0x00);
}

/* 7E:37FA  Call FD:FFF7 (palette animation tick), keeping the multiplier
 * operands 4202/4203 (read back through the bus, saved in dp F0). */
DC_ENTRY(7E37FA, RTS)
{
    dc_set_mx(1, g_cpu.x_flag);
    ldx(abs_x(0x4202));
    stx_dp(0xF0);
    dc_jsl(0xFDFFF7, 0x7E3801);
    ldx(dp_x(0xF0));
    stx_abs(0x4202);
}

/* Run the C3 script machine (C3:0AFF) on the script state at direct page d, DB = 7E.
 * The original (LDA #d; TCD; PHK; PLB; JSL C3:0AFF; RTS) only returns after the
 * call, so an unwind through the script machine (dc_jsl_y) can be handed on. */
static void run_script(u16 d, u32 site)
{
    lda16(d);
    g_cpu.D = d;
    g_cpu.DB = 0x7E;
    dc_set_flags_nz8(0x7E);
    dc_jsl_y(0xC30AFF, site);
}
/* 7E:3749  Script machine on the state at 0A00. */
DC_ENTRY(7E3749, RTS) { run_script(0x0A00, 0x7E374F); }
/* 7E:3754  Script machine on the state at 0B00. */
DC_ENTRY(7E3754, RTS) { run_script(0x0B00, 0x7E375A); }

/* 7E:6212  DMA 00:0C00, 80 bytes, as words to VRAM 7000; then BG3 horizontal
 * scroll (2111) = word 0044. */
DC_ENTRY(7E6212, RTS)
{
    abs16_set(0x2116, 0x7000);
    g_cpu.X = 0x0080;
    dma7_regs(0x01, 0x18, 0x0C00, 0x00, 0x0080);
    lda8(0x80);
    abs8_set(0x2115, 0x80);
    abs8_set(0x420B, 0x80);
    abs8_set(0x2111, abs8(0x0044));
    lda8(abs8(0x0045));
    abs8_set(0x2111, (u8)g_cpu.A);
}

/* Signed sine of angle A (0-3FF): the magnitude byte from the table at
 * C0:F900, negated for the first half (X = angle). */
static void sine(void)
{
    u16 x = g_cpu.A & 0x03FF;
    g_cpu.X = x;
    u16 v = bus16(0xC0F900 + x) & 0x00FF;
    dc_cmp16(x, 0x0200);
    if (!g_cpu._flag_C) { v = (u16)-v; dc_set_flags_nz16(v); }
    g_cpu.A = v;
}
/* 7E:3461  Cosine: the sine of A + 100. */
DC_ENTRY(7E3461, RTS)
{
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(g_cpu.A, 0x0100);
    sine();
}
/* 7E:3465  Sine of A. */
DC_ENTRY(7E3465, RTS) { sine(); }

/* 7E:3691  Copy the 3 bytes at FE:0000+X to 0300-0302 and call C3:0557. */
DC_ENTRY(7E3691, RTS)
{
    php();
    dc_set_mx(0, g_cpu.x_flag);
    ram16_set(0x0300, bus16(0xFE0000 + g_cpu.X));
    g_cpu.A = bus16(0xFE0001 + g_cpu.X);
    ram16_set(0x0301, g_cpu.A);
    dc_jsl(0xC30557, 0x7E36A4);
    plp();
}

/* 7E:89C0  When dp 21 is set: DMA 00:0C00, dp 2B bytes, as words to VRAM
 * 5800, and clear dp 21. */
DC_ENTRY(7E89C0, RTS)
{
    lda8(dp8(0x21));
    if (g_cpu._flag_Z) return;
    g_cpu.X = 0x5800;
    abs16_set(0x2116, 0x5800);
    g_cpu.Y = dp16(0x2B);
    g_cpu.X = 0x0C00;
    dma7_regs(0x01, 0x18, 0x0C00, 0x00, g_cpu.Y);
    lda8(0x80);
    abs8_set(0x2115, 0x80);
    abs8_set(0x420B, 0x80);
    dp8_set(0x21, 0);
}

/* 7E:38AF  Copy the 200-byte palette buffer 7E:2000 to 7E:2200. */
DC_ENTRY(7E38AF, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    u8 db = g_cpu.DB;
    g_cpu.X = 0x2000; g_cpu.Y = 0x2200;
    g_cpu.A = 0x01FF;
    dc_mvn(0x7E, 0x7E);
    g_cpu.A = 0;
    g_cpu.DB = db;
    dc_set_flags_nz8(db);
    dc_set_mx(1, g_cpu.x_flag);
}

/* 7E:3448  Next random byte: 039B += 1, A = C0:FE00[039B] (16-bit A; the
 * high byte is the low byte of D, from TDC / XBA). */
DC_ENTRY(7E3448, RTS)
{
    u8 n = (u8)(ram8(0x039B) + 1);
    ram8_set(0x039B, n);
    u16 i = (u16)(g_cpu.D << 8 | n);
    g_cpu.A = (u16)((g_cpu.D << 8) | bus8(0xC0FE00 + i));
}

/* 7E:891B  004E = 1200 + ((0A0A + 10 + C) & 03E0) * 32: the start of a
 * tile row in a 32-column map. */
DC_ENTRY(7E891B, RTS)
{
    g_cpu.A = dc_adc16(abs16(0x0A0A), 0x0010) & 0x03E0;
    asl_a(5);
    g_cpu.A = dc_adc16(g_cpu.A, 0x1200);
    abs16_set(0x004E, g_cpu.A);
}

/* 7E:5C57  Mode 7 matrix (211B-211E) from dp 5A-61 and BG1 scroll
 * (210D / 210E) from 0A02 / 0A04. */
DC_ENTRY(7E5C57, RTS)
{
    for (int k = 0; k < 8; k++) abs8_set(0x211B + k / 2, dp8(0x5A + k));
    for (int k = 0; k < 4; k++) {
        lda8(abs8(0x0A02 + k));
        abs8_set(0x210D + k / 2, (u8)g_cpu.A);
    }
}

/* 7E:3870  The byte at (dp 13): shifted right 2 bits at a time with a 1
 * coming in, 4 - ((dp 11 >> 2) & 3) times (X counts up to 4; its high byte
 * is the low byte of D). */
DC_ENTRY(7E3870, RTS)
{
    g_cpu.A = (u16)(g_cpu.D << 8 | g_cpu.D >> 8);
    dc_set_mx(1, g_cpu.x_flag);
    u8 a = (u8)((dp8(0x11) >> 2) & 0x03);
    g_cpu.X = (u16)((g_cpu.A & 0xFF00) | a);
    u32 p = dba(dp16(0x13));
    a = bus8(p);
    while (g_cpu.X != 4) {
        g_cpu.X++;
        a = (u8)(0x40 | a >> 2);
    }
    g_cpu._flag_C = 1; g_cpu._flag_Z = 1; g_cpu._flag_N = 0;
    dc_set_a8(a);
    bus8_set(p, a);
}

/* 7E:66B0  X 2-bit-plane tiles from the WRAM port (address 0303, bank as
 * set) to VRAM as 4-plane tiles: 16 bytes each, then 16 zero bytes. */
DC_ENTRY(7E66B0, RTS)
{
    dc_push16(g_cpu.D);
    g_cpu.D = 0x2100;
    g_cpu.Y = abs16(0x0303);
    io8_set(0x2181, (u8)g_cpu.Y);
    io8_set(0x2182, (u8)(g_cpu.Y >> 8));
    do {
        for (int k = 0; k < 8; k++) {
            io8_set(0x2118, io8(0x2180));
            dc_set_a8(io8(0x2180));
            io8_set(0x2119, (u8)g_cpu.A);
        }
        g_cpu.Y = 0;
        for (int k = 0; k < 8; k++) { io8_set(0x2118, 0); io8_set(0x2119, 0); }
        g_cpu.X--;
    } while (g_cpu.X);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* 16-bit ROL of a direct-page word (the carry goes in at bit 0). */
static void rol_dp16(u8 o)
{
    u16 w = dp16(o);
    u8 c = (u8)(w >> 15);
    w = (u16)(w << 1 | g_cpu._flag_C);
    dp16_set(o, w);
    g_cpu._flag_C = c;
    dc_set_flags_nz16(w);
}
static void dec_dp16(u8 o) { u16 w = (u16)(dp16(o) - 1); dp16_set(o, w); dc_set_flags_nz16(w); }

/* 7E:37A6  Double a 64x64 picture vertically: each 40-byte row of
 * 7F:0200..11FF is copied twice into 7F:2200..41FF. */
DC_ENTRY(7E37A6, RTL)
{
    u8 db = g_cpu.DB;
    dc_set_mx(0, g_cpu.x_flag);
    dp16_set(0xF0, 0x0040);
    g_cpu.Y = 0x2200; g_cpu.X = 0x0200;
    do {
        u16 row = g_cpu.X;
        g_cpu.A = 0x003F; dc_mvn(0x7F, 0x7F);
        ldx(row);
        g_cpu.A = 0x003F; dc_mvn(0x7F, 0x7F);
        dec_dp16(0xF0);
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.DB = db;
    dc_set_flags_nz8(db);
}

/* 7E:37CA  The same rows with a zero row after each one (interlaced). */
DC_ENTRY(7E37CA, RTL)
{
    u8 db = g_cpu.DB;
    dc_set_mx(0, g_cpu.x_flag);
    dp16_set(0xF0, 0x0040);
    g_cpu.Y = 0x2200; g_cpu.X = 0x0200;
    do {
        g_cpu.A = 0x003F; dc_mvn(0x7F, 0x7F);
        u16 next = g_cpu.X;
        g_cpu.X = g_cpu.Y++;
        abs16_set(g_cpu.X, 0x0000);
        g_cpu.A = 0x003E; dc_mvn(0x7F, 0x7F);   /* overlapping copy: spreads the zero over the row */
        ldx(next);
        dec_dp16(0xF0);
    } while (!g_cpu._flag_Z);
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.DB = db;
    dc_set_flags_nz8(db);
}

/* 7E:9D48  Every 8th frame (dp 08 & 7 = 0): swap pairs of colors in the
 * palette buffers 7E:2000 and 7E:2200 (a color cycle); leaves DB = 7E. */
DC_ENTRY(7E9D48, RTS)
{
    lda16(dp16(0x08) & 0x0007);
    if (!g_cpu._flag_Z) return;
    g_cpu.DB = 0x7E;
    static const u16 pairs[][2] = {
        { 0x2228, 0x222A }, { 0x2222, 0x2224 }, { 0x221A, 0x225A }, { 0x221C, 0x225C },
        { 0x2028, 0x202A }, { 0x2022, 0x2024 }, { 0x201A, 0x205A }, { 0x201C, 0x205C },
    };
    for (int k = 0; k < 8; k++) {
        g_cpu.A = ram16(pairs[k][0]);
        ldx(ram16(pairs[k][1]));
        ram16_set(pairs[k][0], g_cpu.X);
        ram16_set(pairs[k][1], g_cpu.A);
    }
}

/* 7E:398A / 7E:39BE (the task lists at 0950 / 0940) stay generated: a task
 * can leave through 7E:39A7 / 39B1 (39DE / 39E8), which drop its return
 * address and the saved registers and go on with the loop, so the call does
 * not come back to the caller. */

/* 7E:36AA  X = FE:0000[X]; call C3:09A0 when bit 0 of 0305 is set, else
 * C3:099C, with A = dp F0 - 1. */
DC_ENTRY(7E36AA, RTS)
{
    php();
    dc_push8(g_cpu.DB);
    dc_set_mx(0, g_cpu.x_flag);
    ldx(bus16(0xFE0000 + g_cpu.X));
    bool odd = abs16(0x0305) & 1;
    g_cpu._flag_C = odd;
    lda16((u16)(dp16(0xF0) - 1));
    if (odd) dc_jsl(0xC309A0, 0x7E36BC);
    else dc_jsl(0xC3099C, 0x7E36C5);
    g_cpu.DB = dc_pull8();
    plp();
}

/* 7E:31BF  dp F4..F7 = word F0 * word F2 (32-bit) with the hardware
 * multiplier. Quirk: the 16-bit ROL $F7 that keeps the middle carry also
 * shifts dp F8 left, and F8's old bit 7 is added into the next sum. */
DC_ENTRY(7E31BF, RTS)
{
    php();
    u8 db = g_cpu.DB;
    u16 x = g_cpu.X, y = g_cpu.Y;
    g_cpu.DB = 0x00;
    u8 f0 = dp8(0xF0), f1 = dp8(0xF1), f2 = dp8(0xF2), f3 = dp8(0xF3);
    abs8_set(0x4202, f2); abs8_set(0x4203, f0);         /* lo * lo */
    dp16_set(0xF6, 0);
    u16 a = abs16(0x4216);
    abs8_set(0x4203, f1);                               /* F2 * F1 */
    dp16_set(0xF4, a);
    a = abs16(0x4216);
    abs8_set(0x4202, f3); abs8_set(0x4203, f0);         /* F3 * F0 */
    g_cpu._flag_C = 0;
    a = dc_adc16(a, abs16(0x4216));
    rol_dp16(0xF7);
    abs8_set(0x4203, f1);                               /* hi * hi */
    a = dc_adc16(a, dp16(0xF5));
    dp16_set(0xF5, a);
    a = dc_adc16(abs16(0x4216), dp16(0xF6));
    dp16_set(0xF6, a);
    g_cpu.A = a;
    g_cpu.X = x; g_cpu.Y = y; g_cpu.DB = db;
    plp();
}

/* Word v at direct page D + X + o (bank 0, the I/O registers when D = 4300). */
static void dpx16_set(u8 o, u16 v)
{
    u16 a = (u16)(g_cpu.D + o + g_cpu.X);
    b0_8_set(a, (u8)v); b0_8_set((u16)(a + 1), (u8)(v >> 8));
}
/* 7E:36CC  DMA channel setup from a list at 7E:Y: 5 bytes per channel
 * (control, B-bus register, source address and bank), FF ends; the bank also
 * goes to the indirect HDMA bank (43x7). Leaves DB = 00. */
DC_ENTRY(7E36CC, RTS)
{
    dc_push16(g_cpu.D);
    g_cpu.D = 0x4300;
    dc_push16(0x007E);          /* PEA $007E; PLB: the 00 is pulled into DB at the end */
    g_cpu.DB = dc_pull8();
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.X = 0;
    for (;;) {
        g_cpu.A = abs16(g_cpu.Y) & 0x00FF;
        dc_cmp16(g_cpu.A, 0x00FF);
        if (g_cpu._flag_Z) break;
        dpx16_set(0, abs16(g_cpu.Y));
        dpx16_set(2, abs16(g_cpu.Y + 2));
        u16 bank = abs16(g_cpu.Y + 4);
        dpx16_set(4, bank);
        dpx16_set(7, bank);
        g_cpu._flag_C = 0; g_cpu.X = dc_adc16(g_cpu.X, 0x0010);
        g_cpu._flag_C = 0; g_cpu.Y = dc_adc16(g_cpu.Y, 0x0005);
    }
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.DB = dc_pull8();
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* 7E:339F  32-bit division: dp F8..FB = F0..F3 / F4..F7, remainder in
 * FC..FF (the entry carry is shifted into the dividend first). */
DC_ENTRY(7E339F, RTS)
{
    dp16_set(0xFA, 0); dp16_set(0xF8, 0); dp16_set(0xFE, 0); dp16_set(0xFC, 0);
    ldx(0x0020);
    do {
        rol_dp16(0xF0); rol_dp16(0xF2); rol_dp16(0xFC); rol_dp16(0xFE);
        g_cpu._flag_C = 1;
        g_cpu.A = dc_sbc16(dp16(0xFC), dp16(0xF4)); dp16_set(0xFC, g_cpu.A);
        g_cpu.A = dc_sbc16(dp16(0xFE), dp16(0xF6)); dp16_set(0xFE, g_cpu.A);
        if (!g_cpu._flag_C) {          /* did not fit: add the divisor back */
            g_cpu.A = dc_adc16(dp16(0xFC), dp16(0xF4)); dp16_set(0xFC, g_cpu.A);
            g_cpu.A = dc_adc16(dp16(0xFE), dp16(0xF6)); dp16_set(0xFE, g_cpu.A);
            g_cpu._flag_C = 0;
        }
        rol_dp16(0xF8); rol_dp16(0xFA);
        dex();
    } while (!g_cpu._flag_Z);
}

/* 7E:32E2  Division A / dp F2 with 16 quotient bits in dp F4: the high byte
 * of A (and the entry carry) is shifted in for 8 steps, zeros for the next 8
 * (A = (A >> 8 << 8) / F2 in 8.8 fixed point); remainder in A. The divisor
 * minus 1 is kept in F8 and subtracted with the carry from the shift. */
DC_ENTRY(7E32E2, RTS)
{
    dp16_set(0xF0, g_cpu.A);
    u16 d = (u16)(dp16(0xF2) - 1);
    dp16_set(0xF8, d);
    u16 a = 0;
    dp16_set(0xF4, 0);
    for (int i = 0; i < 16; i++) {
        u8 c;
        if (i < 8) { rol_dp16(0xF0); c = (u8)(a >> 15); a = (u16)(a << 1 | g_cpu._flag_C); }
        else { c = (u8)(a >> 15); a = (u16)(a << 1); }
        g_cpu._flag_C = c;
        a = dc_sbc16(a, dp16(0xF8));
        if (!g_cpu._flag_C) { a = dc_adc16(a, dp16(0xF2)); g_cpu._flag_C = 0; }
        rol_dp16(0xF4);
    }
    g_cpu.A = a;
}

/* 7E:3ED3  8 rows of 20 bytes read backwards from DB:(Y + 0200), each
 * followed by 20 bytes of the low byte of D, to dp 80 (the WRAM port with
 * D = 2100); Y += 40 per row. */
DC_ENTRY(7E3ED3, RTS)
{
    ldx(0x0008);
    do {
        phx();                          /* real pushes: the reads below may hit the stack */
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        ldx(dc_adc16(g_cpu.Y, abs16(0x0200)));
        dc_set_mx(1, g_cpu.x_flag);
        dc_push16(g_cpu.Y);
        for (int k = 0; k < 0x20; k++) {
            lda8(abs8(g_cpu.X));
            dp8_set(0x80, (u8)g_cpu.A);
            g_cpu.X--;
        }
        g_cpu.Y = dc_pull16();
        g_cpu.X = 4;
        g_cpu.A = g_cpu.D;
        for (int k = 0; k < 0x20; k++) dp8_set(0x80, (u8)g_cpu.D);
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.Y, 0x0040);
        g_cpu.Y = g_cpu.A;
        dc_set_mx(1, g_cpu.x_flag);
        plx();
        dex();
    } while (!g_cpu._flag_Z);
}

/* 7E:8850  Window / HDMA values for a centered box: from 0B04 - dp 4C / 2
 * (dp 17, halves to 7E:9070 / 9073) and 0B02 - dp 4C (dp 15, 7E:9077 /
 * 907A, + 40 capped at FF to 9078 / 907B); line count 10 or 20 (dp 7B) to
 * 9076 / 9079; then C3:0D5E with the list 7E:88B9 or 7E:88DA. */
DC_ENTRY(7E8850, RTS)
{
    dc_set_mx(1, g_cpu.x_flag);
    u8 v = dp8(0x4C);
    g_cpu._flag_C = v & 1;
    dp8_set(0xF4, v >> 1);
    g_cpu._flag_C = 1;
    v = dc_sbc8(abs8(0x0B04), dp8(0xF4));
    dp8_set(0x17, v);
    g_cpu._flag_C = v & 1;
    v >>= 1;
    ram8_set(0x9070, v);
    v = dc_adc8(v, 0);
    ram8_set(0x9073, v);
    g_cpu._flag_C = 1;
    v = dc_sbc8(abs8(0x0B02), dp8(0x4C));
    dp8_set(0x15, v);
    ram8_set(0x9077, v); ram8_set(0x907A, v);
    g_cpu._flag_C = 0;
    v = dc_adc8(v, 0x40);
    if (g_cpu._flag_C) { v = 0xFF; dc_set_flags_nz8(v); }
    ram8_set(0x9078, v); ram8_set(0x907B, v);
    dp8_set(0x16, 0); dp8_set(0x18, 0);
    bool narrow = dp8(0x7B) == 0;
    v = narrow ? 0x10 : 0x20;
    ram8_set(0x9076, v); ram8_set(0x9079, v);
    dc_set_mx(0, g_cpu.x_flag);
    lda16(narrow ? 0x88B9 : 0x88DA);
    dc_jsl(0xC30D5E, narrow ? 0x7E88A0 : 0x7E88B4);
}

/* 7E:5E54  A 4-plane 8x8 tile (planes 0/1 at DB:X, 2/3 at X+10) to Mode 7
 * pixels: one byte per pixel to 2119, (low byte of D) << 4 | p3 p2 p1 p0. */
DC_ENTRY(7E5E54, RTS)
{
    lda8(0x08);
    dp8_set(0x3F, 0x08);
    u8 a = 0;
    do {
        dc_set_mx(0, g_cpu.x_flag);
        dp16_set(0x3B, abs16(g_cpu.X));
        dp16_set(0x3D, abs16(g_cpu.X + 0x10));
        g_cpu.X += 2;
        dc_set_mx(1, g_cpu.x_flag);
        for (int px = 0; px < 8; px++) {
            a = (u8)g_cpu.D;
            for (u8 p = 0x3E; p >= 0x3B; p--) {
                u8 b = dp8(p);
                dp8_set(p, (u8)(b << 1));
                u8 c = a >> 7;
                a = (u8)(a << 1 | b >> 7);
                g_cpu._flag_C = c;
            }
            bus8_set(0x002119, a);
        }
        u8 n = (u8)(dp8(0x3F) - 1);
        dp8_set(0x3F, n);
        dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
    g_cpu.A = (u16)((g_cpu.D & 0xFF00) | a);
}

/* 7E:31B0  Wait for the start of the next vertical blank (4212 bit 7). */
DC_ENTRY(7E31B0, RTS)
{
    u8 v;
    while ((v = abs8(0x4212)) & 0x80) dc_spend(64);
    while (!((v = abs8(0x4212)) & 0x80)) dc_spend(64);
    lda8(v);
}

/* 7E:33D6  Signed multiply: dp F4..F7 = F0 * F2 (16-bit signed operands,
 * 32-bit result) through the unsigned multiply 7E:31BF on the magnitudes. */
DC_ENTRY(7E33D6, RTS)
{
    php();
    bool neg = (dp16(0xF0) ^ dp16(0xF2)) & 0x8000;
    for (u8 o = 0xF0; o <= 0xF2; o += 2) {
        lda16(dp16(o));
        if (g_cpu._flag_N) lda16((u16)-g_cpu.A);
        dp16_set(o, g_cpu.A);
    }
    dc_7E31BF_glue();
    if (neg) {
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16((u16)~dp16(0xF4), 1); dp16_set(0xF4, g_cpu.A);
        g_cpu.A = dc_adc16((u16)~dp16(0xF6), 0); dp16_set(0xF6, g_cpu.A);
    }
    plp();
}

/* 7E:380B  Pack the sprite size bits (7E:3870), DMA the sprite table
 * 00:0700..091F to OAM, priority rotation from sprite dp 10, and reset the
 * size-bit packer (dp 11 = 0, pointer dp 13 = 0900). */
DC_ENTRY(7E380B, RTS)
{
    dc_7E3870_glue();
    abs16_set(0x2102, 0x0000);
    abs8_set(0x4374, 0x00);
    abs16_set(0x4370, 0x0400);
    abs16_set(0x4372, 0x0700);
    abs16_set(0x4375, 0x0220);
    dma7_start();
    abs8_set(0x2103, 0x80);
    lda8(dp8(0x10));
    abs8_set(0x2102, (u8)g_cpu.A);
    dp16_set(0x11, 0x0000);
    ldx(0x0900);
    dp16_set(0x13, 0x0900);
}

/* 7E:5AB2  DMA from the WRAM parameters (7E:38E7), then 32 rows of 32
 * bytes from every other byte at the WRAM port (address 0200) to the VRAM
 * low bytes from 0000, each row padded with 60 zeros; then VRAM fill of 1000
 * bytes of 7E:350B (DMA as 7E:90A7). */
DC_ENTRY(7E5AB2, RTS)
{
    dc_jsl(0x7E38E7, 0x7E5AB2);     /* a real JSL: the DMA may read the stack it pushes */
    abs16_set(0x2181, 0x0200);
    abs8_set(0x2115, 0x00);
    abs16_set(0x2116, 0x0000);
    for (int row = 0; row < 0x20; row++) {
        for (int k = 0; k < 0x20; k++) {
            abs8_set(0x2118, abs8(0x2180));
            (void)abs8(0x2180);
        }
        for (int k = 0; k < 0x60; k++) abs8_set(0x2118, 0x00);
    }
    lda8(0x00);
    g_cpu.Y = 0;
    g_cpu.X = 0x1000;
    dma7_regs(0x08, 0x18, 0x350B, 0x7E, 0x1000);
    dma7_start();
}

/* 7E:5E3C  Y 4-plane tiles from 7F:X (X = 0200 first, 20 bytes apart) to
 * Mode 7 pixels (7E:5E54); leaves DB = 00. */
DC_ENTRY(7E5E3C, RTS)
{
    dc_push16(0x007F);          /* PEA $007F; PLB: the 00 is pulled at the end */
    g_cpu.DB = dc_pull8();
    g_cpu.X = 0x0200;
    do {
        dc_7E5E54_glue();
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.X, 0x0010);
        g_cpu.X = g_cpu.A;
        dc_set_mx(1, g_cpu.x_flag);
        g_cpu.Y--;
    } while (g_cpu.Y);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}

/* 7E:9BD8  dp 8F+X = A60A when dp 9F is set, else one of the 4 words at
 * 9C4E picked with the random byte (7E:3448; the retry while the pick is
 * >= 5 never happens); dp 87+X = 1. */
DC_ENTRY(7E9BD8, RTS)
{
    lda16(dp16(0x9F));
    if (!g_cpu._flag_Z) {
        lda16(0xA60A);
    } else {
        do {
            dc_7E3448_glue();
            lda16(g_cpu.A & 0x0003);
            dc_cmp16(g_cpu.A, 0x0005);
        } while (!g_cpu._flag_N);
        asl_a(1);
        g_cpu.Y = g_cpu.A;
        lda16(abs16(0x9C4E + g_cpu.Y));
    }
    dpx16_set(0x8F, g_cpu.A);
    lda16(0x0001);
    dpx16_set(0x87, 0x0001);
}

/* 7E:9BB8  Two objects (X = 0, 2) set up by 7E:9BD8, with dp 97+X =
 * 7080 / 60A0; DB = 7E while it runs, 00 after. */
DC_ENTRY(7E9BB8, RTS)
{
    dc_push16(0x007E);
    g_cpu.DB = dc_pull8();
    dc_set_mx(0, g_cpu.x_flag);
    static const u16 pos[2] = { 0x7080, 0x60A0 };
    for (int k = 0; k < 2; k++) {
        ldx((u16)(k * 2));
        dc_7E9BD8_glue();
        lda16(pos[k]);
        dpx16_set(0x97, pos[k]);
    }
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}

/* A = -A when the N flag is set (EOR #FFFF; INC). */
static void neg_if_minus(void) { if (g_cpu._flag_N) lda16((u16)-g_cpu.A); }
/* Signed A * dp src (unsigned) / 100 with the multiply 7E:31BF: |A| goes to
 * F0, the sign (N flag) is kept with PHP around the call; returns with the
 * caller's flags from before the call. out = word F5 >> shift. */
static void smul_f5(u16 src, bool abs_src, int shift)
{
    php();
    neg_if_minus();
    dp16_set(0xF0, g_cpu.A);
    lda16(abs_src ? abs16(src) : dp16((u8)src));
    dp16_set(0xF2, g_cpu.A);
    dc_7E31BF_glue();
    lda16(dp16(0xF5));
    if (shift) lsr_a(shift);
    plp();
    neg_if_minus();
}

/* 7E:5BCF  Mode 7 matrix for the spinning, zooming plane: angle dp 66 += 7;
 * dp FE = 200 + cos(angle) * dp 6C / 400 (the distance); A = D = cos(FE) *
 * 0A00 / 100 + 1 (5A / 60), B = sin(FE) * 0A00 / 100 (5C, C = -B in 5E);
 * 0A04 = 10000 / 0A00 - 100 (vertical scroll); dp 6C counts down to 1. */
DC_ENTRY(7E5BCF, RTS)
{
    g_cpu._flag_C = 0;
    lda16(dc_adc16(dp16(0x66), 0x0007));
    dp16_set(0x66, g_cpu.A);
    dc_7E3461_glue();
    smul_f5(0x6C, false, 2);
    g_cpu._flag_C = 0;
    g_cpu.A = dc_adc16(g_cpu.A, 0x0200);
    dp16_set(0xFE, g_cpu.A);
    dc_7E3461_glue();
    dp16_set(0xF0, g_cpu.A);
    lda16(abs16(0x0A00));
    dp16_set(0xF2, g_cpu.A);
    dc_7E31BF_glue();
    lda16((u16)(dp16(0xF5) + 1));
    dp16_set(0x5A, g_cpu.A);
    dp16_set(0x60, g_cpu.A);
    lda16(dp16(0xFE));
    dc_7E3465_glue();
    smul_f5(0x0A00, true, 0);
    dp16_set(0x5C, g_cpu.A);
    lda16((u16)-g_cpu.A);
    dp16_set(0x5E, g_cpu.A);
    dp16_set(0xF0, 0);
    dp16_set(0xF6, 0);
    lda16(0x0001);
    dp16_set(0xF2, 0x0001);
    lda16(abs16(0x0A00));
    dp16_set(0xF4, g_cpu.A);
    dc_7E339F_glue();
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc16(dp16(0xF8), 0x0100);
    abs16_set(0x0A04, g_cpu.A);
    dec_dp16(0x6C);
    if (g_cpu._flag_Z) {
        u16 w = (u16)(dp16(0x6C) + 1);
        dp16_set(0x6C, w);
        dc_set_flags_nz16(w);
    }
}

/* 7E:5FFE  Wavy scroll table at 0C00: words 0C40..0C7C move down one entry
 * and 0C40 = (sin(dp 08 * 16) * 0A0C / 100 - 4A) & 3FF | 2000. */
DC_ENTRY(7E5FFE, RTS)
{
    ldx(0x007C);
    do {
        lda16(abs16(0x0C00 + g_cpu.X));
        abs16_set(0x0C02 + g_cpu.X, g_cpu.A);
        g_cpu.X -= 2;
        cpx(0x003E);
    } while (!g_cpu._flag_Z);
    lda16(dp16(0x08));
    asl_a(4);
    dc_7E3465_glue();
    smul_f5(0x0A0C, true, 0);
    g_cpu._flag_C = 1;
    g_cpu.A = dc_sbc16(g_cpu.A, 0x004A);
    lda16((g_cpu.A & 0x03FF) | 0x2000);
    abs16_set(0x0C40, g_cpu.A);
}

/* 7E:99B5  dp 6D = sin(dp 08 * 4) * dp 6B / 100, dp 6F = cos(dp 08 * 2)
 * * dp 6B / 100 (dp FE = dp 08 * 2). */
DC_ENTRY(7E99B5, RTS)
{
    lda16(dp16(0x08));
    asl_a(1);
    dp16_set(0xFE, g_cpu.A);
    asl_a(1);
    dc_7E3465_glue();
    smul_f5(0x6B, false, 0);
    dp16_set(0x6D, g_cpu.A);
    lda16(dp16(0xFE));
    dc_7E3461_glue();
    smul_f5(0x6B, false, 0);
    dp16_set(0x6F, g_cpu.A);
}

/* 7E:9AF3  With D = 0A00 (left so): 0A1E = 0B0C; 1412 = cos(0A0A) * 0A1E /
 * 100 and 1410 = sin(0A0A) * 0A1E / 100 (signed, 7E:33D6); 1400 = 0A02 +
 * 1410, 1408 = that + 80, 1402 = 0A04 + 1412, 140A = that + E0. */
DC_ENTRY(7E9AF3, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    lda16(abs16(0x0B0C));
    abs16_set(0x0A1E, g_cpu.A);
    lda16(0x0A00);
    g_cpu.D = 0x0A00;
    lda16(dp16(0x0A));
    dc_7E3461_glue();
    dp16_set(0xF0, g_cpu.A);
    lda16(dp16(0x1E));
    dp16_set(0xF2, g_cpu.A);
    dc_7E33D6_glue();
    lda16(dp16(0xF5));
    abs16_set(0x1412, g_cpu.A);
    lda16(dp16(0x0A));
    dc_7E3465_glue();
    dp16_set(0xF0, g_cpu.A);
    lda16(dp16(0x1E));
    dp16_set(0xF2, g_cpu.A);
    dc_7E33D6_glue();
    lda16(dp16(0xF5));
    abs16_set(0x1410, g_cpu.A);
    static const struct { u8 base; u16 off, out1, add, out2; } pt[2] = {
        { 0x02, 0x1410, 0x1400, 0x0080, 0x1408 },
        { 0x04, 0x1412, 0x1402, 0x00E0, 0x140A },
    };
    for (int k = 0; k < 2; k++) {
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(dp16(pt[k].base), abs16(pt[k].off));
        abs16_set(pt[k].out1, g_cpu.A);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(g_cpu.A, pt[k].add);
        abs16_set(pt[k].out2, g_cpu.A);
    }
}

/* 7E:8790  Scene setup: hook 3024 = 7E:8930, HDMA off; palette copy,
 * 7E:4562 = 5; graphics FE:0084 unpacked to 7F:0200 (via the WRAM port)
 * and doubled into 7F:2200 (7E:37A6), written twice into the Mode 7 map
 * (VRAM low bytes 0000-3FFF, 7E:8752);
 * graphics FE:00D8 unpacked to 7F:0A00; 15 x 2 calls of 7E:3ED3 build
 * mirrored rows at 7F:5600 (sources from 4E00 down by 400); then two
 * blocks FE:00A5 (20 bytes) to 7E:2100 / 7E:2300 (7E:36AA). Ends with
 * DB = 7E, 16-bit A. */
DC_ENTRY(7E8790, RTS)
{
    u16 d = g_cpu.D;
    ldx(0x8930);
    abs16_set(0x3024, 0x8930);
    g_cpu.DB = 0x00;            /* PEA $7E00; PLB: the 7E is pulled at the end */
    dc_set_mx(1, g_cpu.x_flag);
    tdc();
    abs8_set(0x420C, (u8)g_cpu.A);
    dc_7E38AF_glue();
    lda8(0x05);
    ram8_set(0x4562, 0x05);
    dc_7E89ED_glue();
    ldx(0x0084);
    dc_7E3691_glue();
    dc_7E37A6_glue();
    tdc();
    static const u16 vregs[4] = { 0x2115, 0x2116, 0x2117, 0x211A };   /* VRAM increment / address, Mode 7 settings = 0 */
    for (int k = 0; k < 4; k++) abs8_set(vregs[k], (u8)g_cpu.A);
    dc_7E8752_glue();
    dc_7E8752_glue();
    dc_7E89ED_glue();
    ldx(0x0A00);
    abs16_set(0x0303, 0x0A00);
    ldx(0x00D8);
    dc_7E3691_glue();
    g_cpu.DB = 0x7F;
    u16 d2 = g_cpu.D;
    g_cpu.D = 0x2100;
    abs16_set(0x0200, 0x0020);  /* row stride for 7E:3ED3 */
    lda8(0x01);
    dp8_set(0x83, 0x01);        /* WRAM port 7F:5600 */
    dp16_set(0x81, 0x5600);
    abs16_set(0x0202, 0x4E00);
    abs16_set(0x0204, 0x000F);
    do {
        g_cpu.Y = abs16(0x0202);
        dc_7E3ED3_glue();
        dc_7E3ED3_glue();
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        g_cpu.A = dc_sbc16(abs16(0x0202), 0x03FF);
        abs16_set(0x0202, g_cpu.A);
        dc_set_mx(1, g_cpu.x_flag);
        u8 n = (u8)(abs8(0x0204) - 1);
        abs8_set(0x0204, n);
        dc_set_flags_nz8(n);
    } while (!g_cpu._flag_Z);
    g_cpu.D = d2;
    g_cpu.DB = 0x00;
    abs16_set(0x0037, 0x0A00);
    abs8_set(0x0305, 0x00);
    dp16_set(0xF0, 0x0020);
    static const u16 dest[2] = { 0x2100, 0x2300 };
    for (int k = 0; k < 2; k++) {
        g_cpu.Y = dest[k];
        ldx(0x00A5);
        dc_7E36AA_glue();
    }
    g_cpu.DB = 0x7E;
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.D = d;
    dc_set_flags_nz16(d);
}

void snes_refresh_charge(void);   /* DRAM refresh tax (common_cpu_infra.c) */
/* Time the original's instructions take (master clocks, code in WRAM),
 * with the refresh tax the interpreter charges, so the sound CPU answers
 * after the same number of polls. */
static void spend(unsigned mc) { dc_spend(mc); snes_refresh_charge(); }

/* 7E:377E  Send the sound command 1E00-1E03 to the APU ports 2143..2140 and
 * wait until port 0 echoes 1E00; after 400 polls the command is sent again.
 * X = polls left. The time of its entry SEP is spent by the callers in
 * this file (a direct call from the fuzz harness matches without it). */
DC_ENTRY(7E377E, RTS)
{
    dc_set_mx(1, g_cpu.x_flag);
    for (;;) {
        spend(24); ldx(0x0400);                 /* LDX # */
        for (int p = 3; p >= 0; p--) {
            spend(32); lda8(abs8(0x1E00 + p));  /* LDA abs */
            spend(24); abs8_set(0x2140 + p, (u8)g_cpu.A); spend(6);
        }
        for (;;) {
            spend(24);                          /* CMP abs: fetch, then the port read */
            dc_cmp8((u8)g_cpu.A, abs8(0x2140));
            spend(6);
            if (g_cpu._flag_Z) return;
            spend(16 + 14);                     /* BEQ not taken, DEX */
            dex();
            if (g_cpu._flag_Z) { spend(16 + 22); break; }   /* BNE not taken, BRA */
            spend(22);                          /* BNE taken */
        }
    }
}

/* 7E:3177  Unless 0384 bit 5 is set: sound commands 85, 89, F2, then F2
 * with 1E01 = 1, then 81 with 1E01/1E02 = 00/FF (sound driver reset). The
 * time of the instructions between the commands is spent (see spend). */
DC_ENTRY(7E3177, RTS)
{
    spend(32 + 16);                             /* LDA abs; BIT # */
    lda8(abs8(0x0384));
    g_cpu._flag_Z = !(g_cpu.A & 0x20);
    if (!g_cpu._flag_Z) return;
    spend(16 + 32 + 24 + 40 + 46 + 16);         /* BNE; STZ; LDX #; STX; JSR; SEP in 7E:377E */
    abs8_set(0x1E02, 0x00);
    ldx(0x0085);
    abs16_set(0x1E00, 0x0085);
    dc_7E377E_glue();
    static const u8 cmd[2] = { 0x89, 0xF2 };
    for (int k = 0; k < 2; k++) {
        spend(42 + 16 + 32 + 46 + 16);          /* RTS; LDA #; STA; JSR; SEP */
        lda8(cmd[k]);
        abs8_set(0x1E00, cmd[k]);
        dc_7E377E_glue();
    }
    spend(42 + 16 + 32 + 46 + 16);
    lda8(0x01);
    abs8_set(0x1E01, 0x01);
    dc_7E377E_glue();
    spend(42 + 24 + 40 + 16 + 32 + 24 + 16);    /* RTS; LDX #; STX; LDA #; STA; JMP; SEP */
    ldx(0xFF00);
    abs16_set(0x1E01, 0xFF00);
    lda8(0x81);
    abs8_set(0x1E00, 0x81);
    dc_7E377E_glue();
}

/* 7E:3063  Scene engine start: sound / state bytes in 0380-0396 reset,
 * direct page 0000-00FF and 0900 (0920 when 0384 bit 7 is set)..1DFF
 * cleared by DMA from the multiply result (2134 = 0404 * 00); unless bit 7:
 * the sound driver reset (7E:3177, unless bit 5), the Mode 7 setup of
 * 7E:3843, dp 13 = 0900, and the PPU registers reset (forced blank, OBJ
 * size, all others 0). */
DC_ENTRY(7E3063, RTS)
{
    lda8(0x04);
    abs8_set(0x038E, 0x04);
    abs8_set(0x038F, 0x04);
    static const u16 clr[] = { 0x0380, 0x0381, 0x0390, 0x0392, 0x0394, 0x0391, 0x0393,
                               0x0395, 0x0396, 0x0388, 0x038D, 0x038C, 0x0366 };
    for (unsigned k = 0; k < sizeof clr / sizeof clr[0]; k++) abs8_set(clr[k], 0x00);
    g_cpu.D = 0x4300;
    abs8_set(0x211B, 0x04);
    abs8_set(0x211B, 0x04);
    tdc();
    abs8_set(0x211C, (u8)g_cpu.A);
    abs8_set(0x211C, (u8)g_cpu.A);
    ldx(0x3480); dp16_set(0x70, 0x3480);   /* DMA 7: B to A, from 2134 */
    ldx(0x0000); dp16_set(0x72, 0x0000);
    dp8_set(0x74, 0x00);
    ldx(0x0100); dp16_set(0x75, 0x0100);
    lda8(0x80);
    abs8_set(0x420B, 0x80);
    g_cpu.D = 0x0000;
    lda8(abs8(0x0384));
    bool keep = g_cpu.A & 0x80;
    g_cpu._flag_Z = !keep;
    ldx(keep ? 0x0920 : 0x0900);
    abs16_set(0x4372, g_cpu.X);
    ldx(keep ? 0x14E0 : 0x1500);
    abs16_set(0x4375, g_cpu.X);
    lda8(0x80);
    abs8_set(0x420B, 0x80);
    if (keep) { tdc(); xba(); return; }
    lda8(abs8(0x0384));
    g_cpu._flag_Z = !(g_cpu.A & 0x20);
    if (g_cpu._flag_Z) {
        abs8_set(0x038A, 0x00);
        dc_7E3177_glue();
    }
    dc_7E3843_glue();
    ldx(0x0900);
    dp16_set(0x13, 0x0900);
    dc_set_mx(1, 0);
    g_cpu.D = 0x2100;
    lda8(0x80); dp8_set(0x00, 0x80);
    lda8(0x02); dp8_set(0x01, 0x02);
    lda8(0xE0); dp8_set(0x32, 0xE0);
    ldx(0x0000);
    tdc(); xba();
    lda8((u8)g_cpu.X);
    /* PPU registers 21xx = 0, in the original's order (W = word store) */
    static const struct { u8 r, w; } z[] = {
        {0x02,1},{0x05,0},{0x06,0},{0x07,1},{0x09,1},{0x0B,1},{0x0D,0},{0x0D,0},{0x0E,0},{0x0E,0},
        {0x0F,0},{0x0F,0},{0x10,0},{0x10,0},{0x11,0},{0x11,0},{0x12,0},{0x12,0},{0x13,0},{0x13,0},
        {0x14,0},{0x14,0},{0x15,0},{0x1A,0},{0x1B,0},{0x1B,0},{0x1C,0},{0x1C,0},{0x1D,0},{0x1D,0},
        {0x1E,0},{0x1E,0},{0x1F,0},{0x1F,0},{0x20,0},{0x20,1},{0x23,1},{0x25,0},{0x26,1},{0x28,1},
        {0x2A,1},{0x2C,1},{0x2E,1},{0x30,1},{0x32,1},
    };
    for (unsigned k = 0; k < sizeof z / sizeof z[0]; k++) {
        if (z[k].w) dp16_set(z[k].r, 0x0000); else dp8_set(z[k].r, 0x00);
    }
    g_cpu.D = 0x0000;
    dc_set_flags_nz16(0);
}
