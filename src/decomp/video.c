/* Video helpers: DMA to VRAM, PPU setup. Bank C0. */
#include "ram.h"
#include "ops65.h"

/* dp 48 = VRAM word address, 4A = DMA mode, 4B/4D = source (24-bit),
 * 4E = byte count; transfers run on DMA channel 7. */
#define DMA_VRAM_DEST  0x48
#define DMA_MODE       0x4A
#define DMA_SRC        0x4B
#define DMA_SRC_BANK   0x4D
#define DMA_SIZE       0x4E

static void dma7_start(u8 mode, u8 breg)
{
    abs8_set(0x4370, mode);
    abs8_set(0x4371, breg);
    abs16_set(0x4372, dp16(DMA_SRC));
    abs8_set(0x4374, dp8(DMA_SRC_BANK));
    u16 n = dp16(DMA_SIZE);
    abs16_set(0x4375, n);
    g_cpu.X = n;
    abs8_set(0x420B, 0x80);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
}
/* C0:2DC8  DMA the block described at dp 48-4F to VRAM (word writes,
 * increment after the high byte). */
static void vram_dma(void)
{
    abs16_set(0x2116, dp16(DMA_VRAM_DEST));
    abs8_set(0x2115, 0x80);
    dma7_start(dp8(DMA_MODE), 0x18);
}
DC_ENTRY(C02DC8, RTS) { vram_dma(); }

/* C0:2DF1  Clear the mode 7 matrix A/B and DMA the multiplication result
 * register ($2134) into memory at dp 4B (B bus -> A bus). */
static void read_mult_dma(void)
{
    abs8_set(0x211B, 0); abs8_set(0x211B, 0);
    abs8_set(0x211C, 0); abs8_set(0x211C, 0);
    dma7_start(0x80, 0x34);
}
DC_ENTRY(C02DF1, RTS) { read_mult_dma(); }

/* C0:2329  Empty routine. */
DC_ENTRY(C02329, RTS) { }

/* C0:23E2  Reset screen-effect state: $0139, 7F:1522-1523, 0BD9-0BDB = 0,
 * 0BDE = 2, dp 21/22 = E0, clear bit 5 of dp 28. */
DC_ENTRY(C023E2, RTS)
{
    dp8_set(0x39, 0);
    ram8_set(0x11522, 0);
    ram8_set(0x11523, 0);
    abs8_set(0x0BD9, 0); abs8_set(0x0BDA, 0); abs8_set(0x0BDB, 0);
    abs8_set(0x0BDE, 2);
    dp8_set(0x21, 0xE0);
    dp8_set(0x22, 0xE0);
    u8 v = dp8(0x28);
    g_cpu._flag_Z = (v & 0x20) == 0;
    dp8_set(0x28, v & ~0x20);
    dc_set_a8(0x20);
}

/* C0:7F58 / 7F5D / 7F62 / 7F77  Copy the buffer 7E:C800 to VRAM: 2 KB to
 * $7400 / $7000 / the address already in dp 48, or 4 KB to dp 48. */
static void buf_to_vram(int dest, u16 size)
{
    if (dest >= 0) dp16_set(DMA_VRAM_DEST, (u16)dest);
    dp16_set(DMA_SIZE, size);
    dp8_set(DMA_MODE, 1);
    dp16_set(DMA_SRC, 0xC800);
    dp8_set(DMA_SRC_BANK, 0x7E);
    vram_dma();
}
DC_ENTRY(C07F58, RTS) { buf_to_vram(0x7400, 0x0800); }
DC_ENTRY(C07F5D, RTS) { buf_to_vram(0x7000, 0x0800); }
DC_ENTRY(C07F62, RTS) { buf_to_vram(-1, 0x0800); }
DC_ENTRY(C07F77, RTS) { buf_to_vram(-1, 0x1000); }

/* Empty routines. */
DC_ENTRY(C06FB9, RTS) { }
DC_ENTRY(C07154, RTS) { }
DC_ENTRY(C08924, RTS) { }
DC_ENTRY(C0A508, RTS) { }

/* C0:7155  Mark the 8 entries at 0B80-0B87 unused (80). */
DC_ENTRY(C07155, RTS)
{
    for (u16 i = 0; i < 8; i++) abs8_set(0x0B80 + i, 0x80);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
}
/* C0:A509  Copy the main/sub screen layer masks (0BD7/0BD8) and colour
 * math setting (0BDF) to the PPU; mosaic off. */
DC_ENTRY(C0A509, RTS)
{
    abs8_set(0x212C, abs8(0x0BD7));
    abs8_set(0x212D, abs8(0x0BD8));
    abs8_set(0x2131, abs8(0x0BDF));
    abs8_set(0x2106, 0);
    dc_set_a8(0);
    dc_set_flags_nz8(0);
}
/* C0:B262  Hide four sprites: OAM buffer entries 0-3 get y = E0. */
DC_ENTRY(C0B262, RTS)
{
    for (u16 i = 0; i < 4; i++) abs8_set(0x0701 + i * 4, 0xE0);
    dc_set_a8(0xE0);
    dc_set_flags_nz8(0xE0);
}

/* C0:88E5  Copy dp 2A/2B to dp 2E/30. */
DC_ENTRY(C088E5, RTS)
{
    dp8_set(0x2E, dp8(0x2A));
    u8 v = dp8(0x2B);
    dp8_set(0x30, v);
    dc_set_a8(v);
    dc_set_flags_nz8(v);
}
/* C0:0B64 / 0B75  Write a JML instruction into the RAM vectors 0500 (to
 * C0:EA63) / 0504 (to C0:ECCC). */
static void set_ram_vector(u16 at, u16 target)
{
    abs8_set(at, 0x5C);
    abs16_set(at + 1, target);
    abs8_set(at + 3, 0xC0);
    g_cpu.X = target;
    dc_set_a8(0xC0);
    dc_set_flags_nz8(0xC0);
}
DC_ENTRY(C00B64, RTS) { set_ram_vector(0x0500, 0xEA63); }
DC_ENTRY(C00B75, RTS) { set_ram_vector(0x0504, 0xECCC); }

/* C0:84A7 / 86DD  Start a DMA on channel 7 from bank 7E to VRAM (address,
 * source and size already set), incrementing after the high byte by 1 / 32
 * words. */
static void vram_dma7_preset(u8 vmain)
{
    abs8_set(0x2115, vmain);
    abs8_set(0x4370, 0x01);
    abs8_set(0x4371, 0x18);
    abs8_set(0x4374, 0x7E);
    abs8_set(0x420B, 0x80);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
}
DC_ENTRY(C084A7, RTS) { vram_dma7_preset(0x80); }
DC_ENTRY(C086DD, RTS) { vram_dma7_preset(0x81); }

/* C0:86AB 86F7 8729 875B 878D 87BF  Upload a tile-map column (VRAM increment
 * 32) in up to two parts: n1 bytes from 7E:base to VRAM addr1, then n2 bytes
 * from 7E:base+n1 to addr2 (dp words; a zero count skips its part). */
static void map_column(u8 n1_dp, u8 a1_dp, u8 n2_dp, u8 a2_dp, u16 base, u16 site1, u16 site2)
{
    u16 n1 = dp16(n1_dp);
    g_cpu.X = n1;
    if (n1) {
        abs16_set(0x4375, n1);
        abs16_set(0x2116, dp16(a1_dp));
        abs16_set(0x4372, base);
        g_cpu.X = base;
        dc_jsr(0xC086DD, 0xC00000 | site1);   /* the pushed return address can be DMA source */
    }
    u16 n2 = dp16(n2_dp);
    g_cpu.X = n2;
    dc_set_flags_nz16(n2);
    if (!n2) return;
    abs16_set(0x4375, n2);
    u16 a2 = dp16(a2_dp);
    g_cpu.X = a2;
    abs16_set(0x2116, a2);
    u16 m = dp16(n1_dp);
    u32 sum = (u32)m + base;
    g_cpu._flag_C = sum > 0xFFFF;
    g_cpu._flag_V = ((~(m ^ base) & (m ^ (u16)sum)) >> 15) & 1;
    abs16_set(0x4372, (u16)sum);
    g_cpu.A = (u16)sum;
    dc_jsr(0xC086DD, 0xC00000 | site2);
}
DC_ENTRY(C086AB, RTS) { map_column(0xE1, 0xAF, 0xE3, 0xB1, 0xC880, 0x86BD, 0x86D9); }
DC_ENTRY(C086F7, RTS) { map_column(0xEB, 0xC3, 0xED, 0xC5, 0xD080, 0x8709, 0x8725); }
DC_ENTRY(C08729, RTS) { map_column(0xF5, 0xD7, 0xF7, 0xD9, 0xD880, 0x873B, 0x8757); }
DC_ENTRY(C0875B, RTS) { map_column(0xE1, 0xAB, 0xE3, 0xAD, 0xC880, 0x876D, 0x8789); }
DC_ENTRY(C0878D, RTS) { map_column(0xEB, 0xBF, 0xED, 0xC1, 0xD080, 0x879F, 0x87BB); }
DC_ENTRY(C087BF, RTS) { map_column(0xF5, 0xD3, 0xF7, 0xD5, 0xD880, 0x87D1, 0x87ED); }

/* C0:0B4E  Interrupts off, data bank 0, forced blank, NMI/IRQ/auto-joypad
 * off, no DMA or HDMA. */
DC_ENTRY(C00B4E, RTS)
{
    g_cpu._flag_I = 1;
    dc_push8(0);
    g_cpu.DB = dc_pull8();
    abs8_set(0x2100, 0x80);
    abs8_set(0x4200, 0);
    abs8_set(0x420B, 0);
    abs8_set(0x420C, 0);
    dc_set_a8(0);
    dc_set_flags_nz8(0);
}

/* C0:5929  Empty the NPC graphics cache (ids 0920-092F = FF). */
DC_ENTRY(C05929, RTS)
{
    for (u16 i = 0; i < 16; i++) abs8_set(0x0920 + i, 0xFF);
    dc_set_a8(0xFF);
    dc_set_flags_nz8(0xFF);
}
/* C0:712E  Upload the whole palette (512 bytes at 7E:2200) to CGRAM. */
DC_ENTRY(C0712E, RTS)
{
    abs8_set(0x2121, 0);
    abs8_set(0x4370, 0x00);
    abs8_set(0x4371, 0x22);
    abs16_set(0x4372, 0x2200);
    abs8_set(0x4374, 0x7E);
    abs16_set(0x4375, 0x0200);
    g_cpu.X = 0x0200;
    abs8_set(0x420B, 0x80);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
}
/* C0:2C32  Count A up (wrapping) until it equals the byte at 01D9, storing
 * A and then Y to dp 80 at each step and once more at the end. */
DC_ENTRY(C02C32, RTS)
{
    /* With D = $2100 dp 80 is the WRAM data port: every A and Y gets written. */
    u8 a = (u8)g_cpu.A, t;
    for (;;) {
        dp8_set(0x80, a);
        if (g_cpu.x_flag) dp8_set(0x80, (u8)g_cpu.Y); else dp16_set(0x80, g_cpu.Y);
        a++;
        t = abs8(0x01D9);
        if (a == t) break;
    }
    dp8_set(0x80, a);
    if (g_cpu.x_flag) dp8_set(0x80, (u8)g_cpu.Y); else dp16_set(0x80, g_cpu.Y);
    dc_set_a8(a);
    dc_cmp8(a, t);
}

/* C0:ECA3  Upload the OAM buffer (0700, 0x220 bytes) to the PPU. */
DC_ENTRY(C0ECA3, RTS)
{
    abs8_set(0x2102, 0); abs8_set(0x2103, 0);
    abs8_set(0x4370, 0x00);
    abs8_set(0x4371, 0x04);
    abs16_set(0x4372, 0x0700);
    abs8_set(0x4374, 0x00);
    abs16_set(0x4375, 0x0220);
    g_cpu.X = 0x0220;
    abs8_set(0x420B, 0x80);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
}
/* C0:EC77  DMA 7E:D800 to VRAM at 0BE5, 0BE7 bytes. */
DC_ENTRY(C0EC77, RTS)
{
    abs16_set(0x2116, abs16(0x0BE5));
    abs8_set(0x2115, 0x80);
    abs8_set(0x4370, 0x01);
    abs8_set(0x4371, 0x18);
    abs16_set(0x4372, 0xD800);
    abs8_set(0x4374, 0x7E);
    u16 n = abs16(0x0BE7);
    abs16_set(0x4375, n);
    g_cpu.X = n;
    abs8_set(0x420B, 0x80);
    dc_set_a8(0x80);
    dc_set_flags_nz8(0x80);
}
/* C0:F110 / F12B / F142 / F159  Write two tile-map words (t, t+1) at each of
 * two VRAM addresses; called with D = $2100 so dp 16 / 18 are the VRAM
 * address and data ports. */
static void vram_put_pairs(u16 a1, u16 t1, u16 a2, u16 t2)
{
    dp16_set(0x16, a1); dp16_set(0x18, t1); dp16_set(0x18, (u16)(t1 + 1));
    dp16_set(0x16, a2); dp16_set(0x18, t2); dp16_set(0x18, (u16)(t2 + 1));
    g_cpu.X = (u16)(t2 + 1);
    dc_set_flags_nz16(g_cpu.X);
}
DC_ENTRY(C0F110, RTS) { vram_put_pairs(0x1CC2, 0x29C2, 0x1CE2, 0x29D2); }
DC_ENTRY(C0F12B, RTS) { vram_put_pairs(0x1C82, 0x2982, 0x1CA2, 0x2992); }
DC_ENTRY(C0F142, RTS) { vram_put_pairs(0x1C42, 0x2942, 0x1C62, 0x2952); }
DC_ENTRY(C0F159, RTS) { vram_put_pairs(0x1C02, 0x2902, 0x1C22, 0x2912); }

/* C0:E935  Mark the 8 entries 0BC0-0BC7 unused (80). */
static void clear_0bc0(void)
{
    for (u16 i = 0; i < 8; i++) abs8_set(0x0BC0 + i, 0x80);   /* dp C0+ with D = 0B00 */
    g_cpu.A = 0x0B80;                                         /* LDA #$0B00 / TCD ... LDA #$80 */
    dc_set_flags_nz16(g_cpu.D);                               /* PLD */
}
DC_ENTRY(C0E935, RTS) { clear_0bc0(); }
/* C0:B192  Zero the low byte of the words 1B00+2i for i < n (n = 7F:2000,
 * 0 = 256), then clear 0BC0-0BC7. */
DC_ENTRY(C0B192, RTS)
{
    dc_set_mx(1, 1);
    u8 n = ram8(0x12000), y = 0;
    do { b0_8_set((u16)(0x1B00 + y), 0); y += 2; } while (--n);
    g_cpu.A = (u16)(g_cpu.A & 0xFF00);
    g_cpu.X = 0;
    g_cpu.Y = y;
    dc_set_mx(1, 0);
    clear_0bc0();
}

/* C0:6EB0 / 6EF1 / 6F5E  DMA a fixed block to VRAM: 7E:F000 -> $1D00
 * (0x280 bytes) / 7E:F000 -> $1C00 (0x200) / FF:9260 -> $57E0 (0x40). */
static void vram_block(u32 src, u16 dest, u16 size)
{
    dp8_set(DMA_MODE, 1);
    dp16_set(DMA_SRC, (u16)src);
    dp8_set(DMA_SRC_BANK, (u8)(src >> 16));
    dp16_set(DMA_VRAM_DEST, dest);
    dp16_set(DMA_SIZE, size);
    vram_dma();
}
DC_ENTRY(C06EB0, RTS) { vram_block(0x7EF000, 0x1D00, 0x0280); }
DC_ENTRY(C06EF1, RTS) { vram_block(0x7EF000, 0x1C00, 0x0200); }
DC_ENTRY(C06F5E, RTS) { vram_block(0xFF9260, 0x57E0, 0x0040); }

/* C0:0905 / 0918  Save / restore 7E:0920-1DFF (0x14E0 bytes) to / from
 * 7F:2000. */
static void block_move(u16 src, u16 dst, u16 count_m1, u8 dbank, u8 sbank)
{
    u8 db = g_cpu.DB;
    g_cpu.X = src; g_cpu.Y = dst;
    dc_set_mx(0, 0);
    g_cpu.A = count_m1;
    dc_mvn(dbank, sbank);
    g_cpu.DB = db;
    dc_set_flags_nz8(db);   /* PLB */
    dc_set_mx(1, 0);
}
DC_ENTRY(C00905, RTS) { block_move(0x2000, 0x0920, 0x14DF, 0x00, 0x7F);}
DC_ENTRY(C00918, RTS) { block_move(0x0920, 0x2000, 0x14DF, 0x7F, 0x00);}
/* C0:39E1 / 39F4  Copy 32 bytes of palette 7E:21C0 -> 21A0 / 21E0 -> 21C0. */
DC_ENTRY(C039E1, RTS) { block_move(0x21C0, 0x21A0, 0x001F, 0x7E, 0x7E);}
DC_ENTRY(C039F4, RTS) { block_move(0x21E0, 0x21C0, 0x001F, 0x7E, 0x7E);}

/* C0:28AA  Fill 7F:1CC8-1CE7 with 80. */
DC_ENTRY(C028AA, RTS)
{
    for (u16 x = 0; x < 0x20; x++) ram8_set(0x11CC8 + x, 0x80);
    g_cpu.A = 0x8080;
    g_cpu.X = 0x20;
    dc_cmp16(0x20, 0x20);
}

/* C0:7F7E  With D = $0100: fill 7E:1D00-1DFF with the (cleared) mode 7
 * product, i.e. zeros, by DMA. */
DC_ENTRY(C07F7E, RTS)
{
    u16 d = g_cpu.D;
    g_cpu.D = 0x0100;
    dp16_set(DMA_SIZE, 0x0100);
    dp16_set(DMA_SRC, 0x1D00);
    dp8_set(DMA_SRC_BANK, 0);
    read_mult_dma();
    g_cpu.A = 0x0180;
    g_cpu.D = d;
    dc_set_flags_nz16(d);
}

/* C0:6F79  With D = $2100, fill 7E:0B88.. with 0x1E bytes of 80 through the
 * WRAM data port. */
static void wram_port_begin(u16 addr)
{
    dp16_set(0x81, addr);
    dp8_set(0x83, 0);
}
DC_ENTRY(C06F79, RTS)
{
    u16 d = g_cpu.D;
    g_cpu.D = 0x2100;
    wram_port_begin(0x0B88);
    dc_set_mx(1, 1);
    for (int i = 0; i < 0x1E; i++) dp8_set(0x80, 0x80);
    g_cpu.X = 0;
    g_cpu.A = 0x2180;
    dc_set_mx(1, 0);
    g_cpu.D = d;
    dc_set_flags_nz16(d);
}
/* C0:A7E9  With D = $2100, write n pairs 00 FF to 7E:0F00.. (n = 7F:2000,
 * 0 = 256) through the WRAM data port: every object slot's 0F00 = 0
 * (hidden), 0F01 = FF. */
DC_ENTRY(C0A7E9, RTS)
{
    u16 d = g_cpu.D;
    g_cpu.D = 0x2100;
    wram_port_begin(0x0F00);
    dc_set_mx(1, 1);
    u8 n = ram8(0x12000);
    do { dp8_set(0x80, 0x00); dp8_set(0x80, 0xFF); } while (--n);
    g_cpu.X = 0;
    g_cpu.Y = 0xFF;
    g_cpu.A = 0x2100;
    dc_set_mx(1, 0);
    g_cpu.D = d;
    dc_set_flags_nz16(d);
}
/* C0:6ECB  DMA 7E:F000 (1 KB) to VRAM $5800 + dp31 * $200. */
DC_ENTRY(C06ECB, RTS)
{
    dp16_set(DMA_SRC, 0xF000);
    u8 v = dp8(0x31);
    u16 s9 = (u16)(v << 9);
    u32 sum = (u32)s9 + 0x5800 + (v >> 7);
    u16 dest = (u16)sum;
    g_cpu._flag_C = sum > 0xFFFF;
    g_cpu._flag_V = ((~(s9 ^ 0x5800) & (s9 ^ dest)) >> 15) & 1;
    dp16_set(DMA_VRAM_DEST, dest);
    g_cpu.A = dest;
    dp16_set(DMA_SIZE, 0x0400);
    dp8_set(DMA_MODE, 1);
    dp8_set(DMA_SRC_BANK, 0x7E);
    vram_dma();
}
/* C0:28C0  Find dp 5B (16-bit) among the 16 words at 7F:1CC8, or store it
 * in the first free (negative) one. X = its offset (0x20 when full). */
DC_ENTRY(C028C0, RTS)
{
    u16 key = dp16(0x5B), x = 0;
    for (;;) {
        u16 v = ram16(0x11CC8 + x);
        if (v & 0x8000) {
            ram16_set(0x11CC8 + x, key);
            g_cpu.A = key;
            dc_set_flags_nz16(key);
            break;
        }
        g_cpu.A = v;
        dc_cmp16(v, key);
        if (v == key) break;
        x += 2;
        dc_cmp16(x, 0x20);
        if (x >= 0x20) break;
    }
    g_cpu.X = x;
}

/* C0:092B  dp FE = dp 00 * 14, plus 0x0E00 when dp 01 bit 0 is set
 * (hardware multiplier). */
DC_ENTRY(C0092B, RTS)
{
    bool hi = dp8(0x01) & 1;
    abs8_set(0x4202, dp8(0x00));
    abs8_set(0x4203, 0x0E);
    u16 p = abs16(0x4216);
    if (hi) {
        u32 s = (u32)p + 0x0E00;
        g_cpu._flag_C = s > 0xFFFF;
        g_cpu._flag_V = ((~(p ^ 0x0E00) & (p ^ (u16)s)) >> 15) & 1;
        p = (u16)s;
    }
    dc_set_flags_nz16(p);
    dp16_set(0xFE, p);
    g_cpu.A = p;
}

/* C0:8925 / 8944 / 8963 / 8982  Scroll step: dp 2C or 2D = +-10 (+-20 with
 * button bit 1 of 00F8 held), added to dp 2E or 30. */
static void scroll_step(u8 speed_dp, u8 pos_dp, bool neg)
{
    u8 b = abs8(0x00F8);
    u8 step = (b & 2) ? 0x20 : 0x10;
    dp8_set(speed_dp, neg ? (u8)-step : step);
    u8 p = dp8(pos_dp), r;
    if (neg) {
        r = (u8)(p - step);
        g_cpu._flag_C = p >= step;
        g_cpu._flag_V = (((p ^ step) & (p ^ r)) >> 7) & 1;
    } else {
        r = (u8)(p + step);
        g_cpu._flag_C = (p + step) > 0xFF;
        g_cpu._flag_V = ((~(p ^ step) & (p ^ r)) >> 7) & 1;
    }
    dp8_set(pos_dp, r);
    dc_set_a8(r);
    dc_set_flags_nz8(r);
}
DC_ENTRY(C08925, RTS) { scroll_step(0x2C, 0x2E, false); }
DC_ENTRY(C08944, RTS) { scroll_step(0x2C, 0x2E, true); }
DC_ENTRY(C08963, RTS) { scroll_step(0x2D, 0x30, true); }
DC_ENTRY(C08982, RTS) { scroll_step(0x2D, 0x30, false); }

/* C0:E9E2 / E9FF  Free the current object's entry in 0BC0-0BC3 (one byte) /
 * 0BC0-0BC2 (and the byte after it). */
static void free_0bc0(u8 n, bool two)
{
    dc_set_mx(1, 1);
    u8 me = dp8(0x6D), x = 0;
    for (;;) {
        u8 v = abs8(0x0BC0 + x);
        dc_set_a8(v);
        dc_cmp8(v, me);
        if (v == me) {
            abs8_set(0x0BC0 + x, 0x80);
            if (two) abs8_set(0x0BC1 + x, 0x80);
            dc_set_a8(0x80);
            dc_set_flags_nz8(0x80);
            break;
        }
        x++;
        dc_cmp8(x, n);
        if (x == n) break;
    }
    g_cpu.X = x;
    dc_set_mx(1, 0);
}
DC_ENTRY(C0E9E2, RTS) { free_0bc0(4, false); }
/* C0:EA1F  Same for a three-byte entry in 0BC0-0BC1. */
DC_ENTRY(C0EA1F, RTS)
{
    dc_set_mx(1, 1);
    u8 me = dp8(0x6D), x = 0;
    for (;;) {
        u8 v = abs8(0x0BC0 + x);
        dc_set_a8(v);
        dc_cmp8(v, me);
        if (v == me) {
            for (int k = 0; k < 3; k++) abs8_set(0x0BC0 + x + k, 0x80);
            dc_set_a8(0x80);
            dc_set_flags_nz8(0x80);
            break;
        }
        x++;
        dc_cmp8(x, 2);
        if (x == 2) break;
    }
    g_cpu.X = x;
    dc_set_mx(1, 0);
}
/* C0:EA42  (8-bit index) Free every entry of the current object in the run
 * that starts at its first one in 0BC0-0BC3. */
DC_ENTRY(C0EA42, RTS)
{
    u8 me = dp8(0x6D), x = 0;
    for (;;) {
        u8 v = abs8(0x0BC0 + x);
        dc_set_a8(v);
        dc_cmp8(v, me);
        if (v == me) break;
        x++;
        dc_cmp8(x, 4);
        if (x == 4) { g_cpu.X = x; return; }
    }
    for (;;) {
        dc_set_a8(0x80);
        abs8_set(0x0BC0 + x, 0x80);
        x++;
        dc_cmp8(x, 4);
        if (x == 4) break;
        u8 v = abs8(0x0BC0 + x);
        dc_set_a8(v);
        dc_cmp8(v, me);
        if (v != me) break;
    }
    g_cpu.X = x;
}
/* C0:B1B2  Run the queued VRAM uploads (D = $0900, left set): for each
 * nonzero byte at 09A0+2i, DMA 7F:[0940+2i] -> VRAM [0950+2i], [0980+2i]
 * bytes, then 7F:[0960+2i] -> [0970+2i], [0990+2i] bytes; clear 09A0. */
DC_ENTRY(C0B1B2, RTS)
{
    g_cpu.D = 0x0900;
    abs8_set(0x2115, 0x80);
    abs8_set(0x4371, 0x18);
    abs8_set(0x4370, 0x01);
    abs8_set(0x4374, 0x7F);
    u16 x = 0;
#define DPX16(o) ((u16)(b0_8((u16)(0x0900 + (o) + x)) | b0_8((u16)(0x0900 + (o) + x + 1)) << 8))
    for (;;) {
        u8 f = b0_8((u16)(0x09A0 + x));
        if (!f) break;
        abs16_set(0x2116, DPX16(0x50)); abs16_set(0x4372, DPX16(0x40)); abs16_set(0x4375, DPX16(0x80));
        abs8_set(0x420B, 0x80);
        u16 y = DPX16(0x90);
        abs16_set(0x2116, DPX16(0x70)); abs16_set(0x4372, DPX16(0x60)); abs16_set(0x4375, y);
        abs8_set(0x420B, 0x80);
        g_cpu.Y = y;
        x += 2;
    }
#undef DPX16
    g_cpu.X = x;
    g_cpu.A = 0x0900;
    dc_set_flags_nz8(0);
    dp8_set(0xA0, 0);
}
/* C0:27EB  Next row of a map upload: 0BE1 = X + 1; VRAM address 0BE5 =
 * $7000 + row(0BE4 & 3F) * 32 + column(0BE3 & 1F), count 0BE7 = 0x40 -
 * 2 * column; next row; request it (dp 46 bit 5). */
static void map_row_next(void)
{
    abs16_set(0x0BE1, (u16)(g_cpu.X + 1));
    g_cpu.X = (u16)(g_cpu.X + 1);
    u8 b = (u8)(g_cpu.A >> 8);
    u16 col = (u16)(((b << 8) | abs8(0x0BE3)) & 0x1F);
    dp16_set(0xD9, col);
    u16 row = (u16)((abs16(0x0BE4) & 0x3F) << 5);
    abs16_set(0x0BE5, (u16)(row + col + 0x7000));
    u16 t = (u16)(0x40 - col), n = (u16)(t - col);
    g_cpu._flag_C = t >= col;
    g_cpu._flag_V = (((t ^ col) & (t ^ n)) >> 15) & 1;
    abs16_set(0x0BE7, n);
    abs8_set(0x0BE4, (u8)(abs8(0x0BE4) + 1));
    u8 v = dp8(0x46);
    g_cpu._flag_Z = (v & 0x20) == 0;
    g_cpu._flag_N = 0;
    dp8_set(0x46, v | 0x20);
    g_cpu.A = (u16)((n & 0xFF00) | 0x20);
}
DC_ENTRY(C027EB, RTS) { map_row_next(); }

DC_ENTRY(C0E9FF, RTS) { free_0bc0(3, true); }

/* C0:E952 / E97A  (8-bit index) Give the current object a free sprite slot
 * in 0BC0-0BC3 (one entry) / two adjacent entries in 0BC0-0BC3; its VRAM
 * address 0D80+obj = $3800 + slot * $200. C set on success. */
static void alloc_0bc0(u8 n, u8 width)
{
    u8 x = 0;
    for (;;) {
        bool free = true;
        for (u8 k = 0; k < width && free; k++) {
            u8 v = abs8(0x0BC0 + x + k);
            dc_set_a8(v);
            free = v & 0x80;
        }
        if (free) break;
        x++;
        dc_set_flags_nz8((u8)(x - n));
        g_cpu._flag_C = x >= n;
        if (!((u8)(x - n) & 0x80)) { g_cpu.X = x; g_cpu._flag_C = 0; return; }
    }
    u8 me = dp8(0x6D);
    for (u8 k = 0; k < width; k++) abs8_set(0x0BC0 + x + k, me);
    u16 s9 = (u16)(x << 9), addr = (u16)(s9 + 0x3800);
    g_cpu._flag_V = ((~(s9 ^ 0x3800) & (s9 ^ addr)) >> 15) & 1;
    dc_set_flags_nz16(addr);
    g_cpu.X = me;
    abs16_set(0x0D80 + me, addr);
    g_cpu.A = addr;
    g_cpu._flag_C = 1;
}
DC_ENTRY(C0E952, RTS) { alloc_0bc0(4, 1); }
DC_ENTRY(C0E97A, RTS) { alloc_0bc0(3, 2); }
DC_ENTRY(C0E9AA, RTS) { alloc_0bc0(2, 3); }

/* C0:89A1  Diagonal scroll step: dp 2C = 2D = 10 (20 with button bit 1 of
 * 00F8), added to both dp 2E and dp 30. */
DC_ENTRY(C089A1, RTS)
{
    u8 step = (abs8(0x00F8) & 2) ? 0x20 : 0x10;
    dp8_set(0x2C, step);
    dp8_set(0x2D, step);
    dp8_set(0x2E, (u8)(dp8(0x2E) + step));
    u8 p = dp8(0x30), r = (u8)(p + step);
    g_cpu._flag_C = (p + step) > 0xFF;
    g_cpu._flag_V = ((~(p ^ step) & (p ^ r)) >> 7) & 1;
    dp8_set(0x30, r);
    dc_set_a8(r);
    dc_set_flags_nz8(r);
}

/* C0:8A3C  Diagonal scroll step the other way (2C = 2D = F0 / E0). */
DC_ENTRY(C08A3C, RTS)
{
    u8 step = (abs8(0x00F8) & 2) ? 0x20 : 0x10;
    dp8_set(0x2C, (u8)-step);
    dp8_set(0x2D, (u8)-step);
    dp8_set(0x2E, (u8)(dp8(0x2E) - step));
    u8 p = dp8(0x30), r = (u8)(p - step);
    g_cpu._flag_C = p >= step;
    g_cpu._flag_V = (((p ^ step) & (p ^ r)) >> 7) & 1;
    dp8_set(0x30, r);
    dc_set_a8(r);
    dc_set_flags_nz8(r);
}
/* C0:1B36  (16-bit) Tile-map word address of column Y, row A in a 64-wide
 * map made of two 32x32 screens: A*32 + Y, plus $400 for Y >= 32. dp D9 =
 * A*32. */
static u16 adc16(u16 a, u16 b)
{
    u32 s = (u32)a + b + g_cpu._flag_C;
    u16 r = (u16)s;
    g_cpu._flag_C = s > 0xFFFF;
    g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1;
    dc_set_flags_nz16(r);
    return r;
}
static u16 sbc16v(u16 a, u16 b) { return adc16(a, (u16)~b); }
DC_ENTRY(C01B36, RTS)
{
    u16 row = (u16)(g_cpu.A << 5);
    dp16_set(0xD9, row);
    u16 y = g_cpu.Y;
    g_cpu._flag_C = 0;
    if (y < 0x20) { g_cpu.A = adc16(y, row); return; }
    u16 a = (u16)(y - 0x20);
    a = adc16(a, row);
    g_cpu._flag_C = 0;
    g_cpu.A = adc16(a, 0x0400);
}
/* C0:70E9  Load the window palette chosen in the settings (7E:2991 & 7) from
 * FF:B212 + 16*n into 7E:2002 (30 bytes), fix colours 9-11, and copy the
 * whole palette 7E:2000 to the upload buffer 7E:2200. */
DC_ENTRY(C070E9, RTS)
{
    u8 db = g_cpu.DB;
    dc_set_mx(0, 0);
    g_cpu.X = (u16)(((ram8(0x2991) & 7) << 4) + 0xB212);
    g_cpu._flag_C = 0;   /* ADC #$B210 never carries */
    g_cpu._flag_V = 0;
    g_cpu.Y = 0x2002;
    g_cpu.A = 0x001D;
    dc_mvn(0x7E, 0xFF);
    ram16_set(0x2012, 0x0C63);
    ram16_set(0x2014, 0x18C6);
    ram16_set(0x2016, 0x739C);
    g_cpu.X = 0x2000; g_cpu.Y = 0x2200; g_cpu.A = 0x01FF;
    dc_mvn(0x7E, 0x7E);
    g_cpu.DB = db;
    dc_set_flags_nz8(db);
    dc_set_mx(1, 0);
}

/* C0:89D2 / 8A07  Diagonal scroll steps with opposite signs on the two
 * axes (2C/2E and 2D/30). */
static void scroll_diag(bool neg_x, bool neg_y)
{
    u8 step = (abs8(0x00F8) & 2) ? 0x20 : 0x10;
    dp8_set(0x2C, neg_x ? (u8)-step : step);
    dp8_set(0x2E, (u8)(neg_x ? dp8(0x2E) - step : dp8(0x2E) + step));
    dp8_set(0x2D, neg_y ? (u8)-step : step);
    u8 p = dp8(0x30), r;
    if (neg_y) { r = (u8)(p - step); g_cpu._flag_C = p >= step; g_cpu._flag_V = (((p ^ step) & (p ^ r)) >> 7) & 1; }
    else       { r = (u8)(p + step); g_cpu._flag_C = (p + step) > 0xFF; g_cpu._flag_V = ((~(p ^ step) & (p ^ r)) >> 7) & 1; }
    dp8_set(0x30, r);
    dc_set_a8(r);
    dc_set_flags_nz8(r);
}
DC_ENTRY(C089D2, RTS) { scroll_diag(true, false); }
DC_ENTRY(C08A07, RTS) { scroll_diag(false, true); }

/* C0:6E5C  Upload the window frame graphics chosen in the settings (7E:2991
 * & 7; source FF:[C0:6EA0 + 2n]): 0x200 bytes to VRAM $1F00 and the next
 * 0x80 bytes to $4FC0. */
DC_ENTRY(C06E5C, RTS)
{
    u16 n = (u16)((ram8(0x2991) & 7) << 1);
    u16 src = rom16(0xC06EA0 + n);
    dp16_set(DMA_SRC, src);
    dp16_set(DMA_VRAM_DEST, 0x1F00);
    dp16_set(DMA_SIZE, 0x0200);
    dp8_set(DMA_MODE, 1);
    dp8_set(DMA_SRC_BANK, 0xFF);
    g_cpu.A = 0x0200;
    dc_jsr(0xC02DC8, 0xC06E84);
    u16 s = dp16(DMA_SRC), z = dp16(DMA_SIZE);
    u32 sum = (u32)s + z;
    g_cpu._flag_C = sum > 0xFFFF;
    g_cpu._flag_V = ((~(s ^ z) & (s ^ (u16)sum)) >> 15) & 1;
    dp16_set(DMA_SRC, (u16)sum);
    dp16_set(DMA_VRAM_DEST, 0x4FC0);
    dp16_set(DMA_SIZE, 0x0080);
    g_cpu.A = 0x0080;
    dc_jsr(0xC02DC8, 0xC06E9C);
}

/* C0:20B2  Fill 7E:F000-F3FF with the 32-byte pattern at D2:0000 (copied
 * twice, then doubled four times with overlapping block moves). */
DC_ENTRY(C020B2, RTS)
{
    u8 db = g_cpu.DB;
    dc_set_mx(0, 0);
    g_cpu.X = 0x0000; g_cpu.Y = 0xF000; g_cpu.A = 0x001F; dc_mvn(0x7E, 0xD2);
    g_cpu.X = 0x0000; g_cpu.A = 0x001F; dc_mvn(0x7E, 0xD2);
    static const u16 len[4] = { 0x003F, 0x007F, 0x00FF, 0x01FF };
    for (int i = 0; i < 4; i++) { g_cpu.X = 0xF000; g_cpu.A = len[i]; dc_mvn(0x7E, 0x7E); }
    g_cpu.DB = db;
    dc_set_flags_nz8(db);
    dc_set_mx(1, 0);
}
/* C0:20F2  Text string dp 2A drawn into 7E:F000 by C2:0003 (string 020C of the table at
 * [020D]): the table is DE:FF00 when dp 54 bit 5 is set, else the long pointer dp 2B..2D. */
DC_ENTRY(C020F2, RTS)
{
    lda8(dp8(0x2A)); abs8_set(0x020C, (u8)g_cpu.A);
    lda8(dp8(0x54));
    g_cpu._flag_Z = (g_cpu.A & 0x20) == 0;
    if (!g_cpu._flag_Z) { ldx(0xFF00); abs16_set(0x020D, g_cpu.X); lda8(0xDE); }
    else { ldx(dp16(0x2B)); abs16_set(0x020D, g_cpu.X); lda8(dp8(0x2D)); }
    abs8_set(0x020F, (u8)g_cpu.A);
    ldx(0xF000); abs16_set(0x0210, 0xF000);
    lda8(0x7E); abs8_set(0x0212, 0x7E);
    lda8(0x00); abs8_set(0x0214, 0x00);
    dc_jsl(0xC20003, 0xC02121);
}

/* C0:6F0C  Upload four 64-byte blocks FF:9CF0/9D30/9D70/9DB0 to VRAM
 * $0000/$0100/$0030/$0130. */
DC_ENTRY(C06F0C, RTS)
{
    static const u16 src[4] = { 0x9CF0, 0x9D30, 0x9D70, 0x9DB0 }, dst[4] = { 0x0000, 0x0100, 0x0030, 0x0130 };
    for (int i = 0; i < 4; i++) {
        dp16_set(DMA_SRC, src[i]);
        dp16_set(DMA_VRAM_DEST, dst[i]);
        dp16_set(DMA_SIZE, 0x0040);
        dp8_set(DMA_MODE, 1);
        dp8_set(DMA_SRC_BANK, 0xFF);
        vram_dma();
    }
}

/* C0:947D 94DF 944B 94AD  Scroll the map n pixels (n = dp 41 / 43 / 42 / 44)
 * left / up / right / down: position dp 87 or 89 moves by one per pixel; the
 * pixel-in-tile counter dp 93 / 96 wraps every 8 pixels, and then the tile
 * coordinates (dp 0C+0A / 0E+10) move and a column/row redraw flag is set
 * (dp 77 / 75 / 76 / 74). */
static void scroll_pixels(u8 n_dp, u8 pos_dp, u8 frac_dp, u8 t1, u8 t2, u8 flag_dp, bool fwd)
{
    u8 n = dp8(n_dp);
    dc_set_a8(n);
    dc_set_flags_nz8(n);
    if (!n) return;
    dp8_set(0x66, n);
    do {
        if (g_cpu.x_flag) { u8 p = (u8)(dp8(pos_dp) + (fwd ? 1 : -1)); dp8_set(pos_dp, p); g_cpu.X = p; }
        else { u16 p = (u16)(dp16(pos_dp) + (fwd ? 1 : -1)); dp16_set(pos_dp, p); g_cpu.X = p; }
        u8 a = dp8(frac_dp);
        u8 edge1 = fwd ? 0x0F : 0x00, edge2 = fwd ? 0x07 : 0x08;
        if (a == edge1) {
            dp8_set(t1, (u8)(dp8(t1) + (fwd ? 1 : -1)));
            dp8_set(t2, (u8)(dp8(t2) + (fwd ? 1 : -1)));
            dp8_set(flag_dp, 1);
            a = fwd ? 0xFF : 0x10;
        }
        dc_cmp8(a, edge2);
        if (a == edge2) {
            dp8_set(t1, (u8)(dp8(t1) + (fwd ? 1 : -1)));
            dp8_set(t2, (u8)(dp8(t2) + (fwd ? 1 : -1)));
            dp8_set(flag_dp, 1);
        }
        a = (u8)(a + (fwd ? 1 : -1));
        dp8_set(frac_dp, a);
        dc_set_a8(a);
        dp8_set(0x66, (u8)(dp8(0x66) - 1));
    } while (dp8(0x66));
    dc_set_flags_nz8(0);
}
DC_ENTRY(C0947D, RTS) { scroll_pixels(0x41, 0x87, 0x93, 0x0C, 0x0A, 0x77, false); }
DC_ENTRY(C094DF, RTS) { scroll_pixels(0x43, 0x89, 0x96, 0x0E, 0x10, 0x75, false); }
DC_ENTRY(C0944B, RTS) { scroll_pixels(0x42, 0x87, 0x93, 0x0A, 0x0C, 0x76, true); }
DC_ENTRY(C094AD, RTS) { scroll_pixels(0x44, 0x89, 0x96, 0x0E, 0x10, 0x74, true); }
/* C0:8218  (16-bit) Tile-map word address of column A, row Y in a 64-wide
 * map: X = A + Y*32, or A - 32 + Y*32 + $400 for A >= 32 (second screen),
 * where A comes back as the column, plus $3E0 on the second screen.
 * dp 62 = column, dp 52 = Y*32. */
DC_ENTRY(C08218, RTS)
{
    u16 col = g_cpu.A;
    dp16_set(0x62, col);
    u16 row = (u16)(g_cpu.Y << 5);
    dp16_set(0x52, row);
    if (col < 0x20) {
        g_cpu._flag_C = 0;
        g_cpu.X = adc16(col, row);
        g_cpu.A = col;
        dc_set_flags_nz16(col);
        return;
    }
    g_cpu._flag_C = 0;
    u16 a = adc16((u16)(col - 0x20), row);
    g_cpu._flag_C = 0;
    g_cpu.X = adc16(a, 0x0400);
    g_cpu._flag_C = 0;
    g_cpu.A = adc16(col, 0x03E0);
}
/* C0:75A0  Fill 7E:C800..CFFF (2 KB) with the 32-byte pattern at D2:0000,
 * doubling the filled part with overlapping MVNs. */
DC_ENTRY(C075A0, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    dc_push8(g_cpu.DB);
    g_cpu.X = 0x0000; g_cpu.Y = 0xC800; g_cpu.A = 0x001F;
    dc_mvn(0x7E, 0xD2);
    for (u16 len = 0x20; len <= 0x400; len *= 2) {
        g_cpu.X = 0xC800;
        g_cpu.A = (u16)(len - 1);
        dc_mvn(0x7E, 0x7E);
    }
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
    dc_set_mx(1, g_cpu.x_flag);
}
/* C0:95E3 / 9619 / 964D / 9683  Advance a layer's pixel scroll counter
 * (dp 94 for X, 97 for Y) dp 46 / 45 / 48 / 47 times, forward (+1) or back
 * (-1) within 0..1F. Each time it crosses a tile column/row (0F/1F going
 * forward, 10/00 going back) the two tile positions move one step and bit 1
 * of the redraw flags is set; every second pixel moves the 16-bit position
 * at dp 8B / 8D too. */
static void scroll_counter(bool fwd, u8 cnt, u8 ctr, u8 t1, u8 t2, u8 flags, u8 pos)
{
    u8 n = dp8(cnt);
    dc_set_a8(n);
    dc_set_flags_nz8(n);
    if (!n) return;
    dp8_set(0x66, n);
    do {
        u8 a = dp8(ctr);
        u8 edge1 = fwd ? 0x1F : 0x00, edge2 = fwd ? 0x0F : 0x10;
        for (int k = 0; k < 2; k++) {
            u8 e = k ? edge2 : edge1;
            if (k) dc_cmp8(a, e);
            if (a != e) continue;
            dp8_set(t1, (u8)(dp8(t1) + (fwd ? 1 : -1)));
            dp8_set(t2, (u8)(dp8(t2) + (fwd ? 1 : -1)));
            dp8_set(flags, dp8(flags) | 2);
            a = k ? e : (fwd ? 0xFF : 0x20);
        }
        a = (u8)(a + (fwd ? 1 : -1));
        dp8_set(ctr, a);
        a &= 1;
        dc_set_a8(a);
        if (!a) {
            if (g_cpu.x_flag) { u8 x = (u8)(dp8(pos) + (fwd ? 1 : -1)); g_cpu.X = x; dp8_set(pos, x); }
            else { u16 x = (u16)(dp16(pos) + (fwd ? 1 : -1)); g_cpu.X = x; dp16_set(pos, x); }
        }
        n = (u8)(dp8(0x66) - 1);
        dp8_set(0x66, n);
    } while (n);
    dc_set_flags_nz8(0);
}
DC_ENTRY(C095E3, RTS) { scroll_counter(true,  0x46, 0x94, 0x12, 0x14, 0x76, 0x8B); }
DC_ENTRY(C09619, RTS) { scroll_counter(false, 0x45, 0x94, 0x14, 0x12, 0x77, 0x8B); }
DC_ENTRY(C0964D, RTS) { scroll_counter(true,  0x48, 0x97, 0x16, 0x18, 0x74, 0x8D); }
DC_ENTRY(C09683, RTS) { scroll_counter(false, 0x47, 0x97, 0x16, 0x18, 0x75, 0x8D); }
/* C0:950F / 9545 / 9579 / 95AF / 96B7 / 96ED / 9721 / 975F  Same for the
 * 8-pixel counters (dp 94 / 97 / 95 / 98, 0..F): move the 16-bit position
 * first, then the counter; crossing 0F/07 forward or 00/08 back sets the
 * layer's redraw bit (2 or 4) in the flags unless dp 35 masks it. Layer 3
 * Y (9721/975F) also moves dp FB by 4 each step. */
static void scroll_counter8(bool fwd, u8 cnt, u8 ctr, u8 flags, u8 bit, u8 pos, bool fb)
{
    u8 n = dp8(cnt);
    dc_set_a8(n);
    dc_set_flags_nz8(n);
    if (!n) return;
    dp8_set(0x66, n);
    do {
        if (fb) dp8_set(0xFB, (u8)(dp8(0xFB) + (fwd ? 4 : -4)));
        if (g_cpu.x_flag) { u8 x = (u8)(dp8(pos) + (fwd ? 1 : -1)); g_cpu.X = x; dp8_set(pos, x); }
        else { u16 x = (u16)(dp16(pos) + (fwd ? 1 : -1)); g_cpu.X = x; dp16_set(pos, x); }
        u8 a = dp8(ctr);
        for (int k = 0; k < 2; k++) {
            u8 e = fwd ? (k ? 0x07 : 0x0F) : (k ? 0x08 : 0x00);
            if (fwd || k) dc_cmp8(a, e);
            if (a != e) continue;
            if (!(dp8(0x35) & bit)) dp8_set(flags, dp8(flags) | bit);
            a = k ? e : (fwd ? 0xFF : 0x10);
        }
        a = (u8)(a + (fwd ? 1 : -1));
        dc_set_a8(a);
        dp8_set(ctr, a);
        n = (u8)(dp8(0x66) - 1);
        dp8_set(0x66, n);
    } while (n);
    dc_set_flags_nz8(0);
}
DC_ENTRY(C0950F, RTS) { scroll_counter8(true,  0x46, 0x94, 0x76, 2, 0x8B, false); }
DC_ENTRY(C09545, RTS) { scroll_counter8(false, 0x45, 0x94, 0x77, 2, 0x8B, false); }
DC_ENTRY(C09579, RTS) { scroll_counter8(true,  0x48, 0x97, 0x74, 2, 0x8D, false); }
DC_ENTRY(C095AF, RTS) { scroll_counter8(false, 0x47, 0x97, 0x75, 2, 0x8D, false); }
DC_ENTRY(C096B7, RTS) { scroll_counter8(true,  0x4A, 0x95, 0x76, 4, 0x8F, false); }
DC_ENTRY(C096ED, RTS) { scroll_counter8(false, 0x49, 0x95, 0x77, 4, 0x8F, false); }
DC_ENTRY(C09721, RTS) { scroll_counter8(true,  0x4C, 0x98, 0x74, 4, 0x91, true); }
DC_ENTRY(C0975F, RTS) { scroll_counter8(false, 0x4B, 0x98, 0x75, 4, 0x91, true); }
/* C0:9175  Camera follow: dp 32 / 33 = the wanted scroll step dp 2E / 30
 * when the object in 0197 is still inside the scroll bounds on that side
 * (C0:5B86 / 5B79 / 5B71 / 5B63), else 0. */
DC_ENTRY(C09175, RTS)
{
    dp8_set(0x32, 0);
    dp8_set(0x33, 0);
    u16 d = g_cpu.D;
    g_cpu.A = (u16)(d << 8 | abs8(0x0197));
    g_cpu.X = g_cpu.x_flag ? (u8)g_cpu.A : g_cpu.A;
    s8 sx = (s8)dp8(0x2E);
    dc_set_a8((u8)sx);
    dc_set_flags_nz8((u8)sx);
    if (sx) {
        dc_jsr(sx < 0 ? 0xC05B86 : 0xC05B79, sx < 0 ? 0xC09185 : 0xC0918C);
        if (!g_cpu._flag_C) { u8 v = dp8(0x2E); dc_set_a8(v); dc_set_flags_nz8(v); dp8_set(0x32, v); }
    }
    s8 sy = (s8)dp8(0x30);
    dc_set_a8((u8)sy);
    dc_set_flags_nz8((u8)sy);
    if (sy) {
        dc_jsr(sy < 0 ? 0xC05B71 : 0xC05B63, sy < 0 ? 0xC0919B : 0xC091A2);
        if (!g_cpu._flag_C) { u8 v = dp8(0x30); dc_set_a8(v); dc_set_flags_nz8(v); dp8_set(0x33, v); }
    }
}
/* C0:C6E7  (16-bit) Move four sprites' X (4BC2/4BCA/4BD2/4BDA + X) by dp C3
 * into the OAM buffer (4BC0/4BC8/4BD0/4BD8 + X), keeping each X bit 8 in
 * dp E5..E7 and returning the four bits packed for the high OAM table, with
 * the size bits set (A = x8 | AA). 8-bit A on return. */
DC_ENTRY(C0C6E7, RTS)
{
    u16 x = g_cpu.X;
    u8 hi[4];
    for (int k = 0; k < 4; k++) {
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu._flag_C = 0;
        u16 v = adc16(abs16(0x4BC2 + 8 * k + x), dp16(0xC3));
        dc_set_mx(1, g_cpu.x_flag);
        abs8_set(0x4BC0 + 8 * k + x, (u8)v);
        hi[k] = (u8)(v >> 8) & 1;
        g_cpu.A = (u16)((v << 8) | hi[k]);
        if (k < 3) dp8_set(0xE5 + k, hi[k]);
    }
    u8 a = hi[3];
    u8 ors[3] = { dp8(0xE7), dp8(0xE6), dp8(0xE5) };
    for (int k = 0; k < 3; k++) {
        g_cpu._flag_C = (a >> 6) & 1;
        a = (u8)(a << 2);
        a |= ors[k];
    }
    a |= 0xAA;
    dc_set_a8(a);
    dc_set_flags_nz8(a);
}
/* C0:B204  Clear the sprite row lists: 7E:0E00..0EFF = 80 (through the
 * WRAM port, then doubled with MVNs), then n = 7F:2000 rounds of 16-bit
 * port writes starting at 0F01 (each also rewrites the port address low
 * byte, as the original's STX/STY to $2180 do). */
DC_ENTRY(C0B204, RTS)
{
    dc_push16(g_cpu.D);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.D = 0x2100;
    dc_set_mx(1, g_cpu.x_flag);
    dp16_set(0x81, 0x0E00);
    dp8_set(0x83, 0);
    for (int i = 0; i < 0x10; i++) { dp8_set(0x80, 0x80); dp8_set(0x80, 0x80); }
    g_cpu.Y = 0x0E20;
    g_cpu.X = 0x0E00;
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0x001F;
    dc_push8(g_cpu.DB);
    dc_mvn(0, 0);
    g_cpu.A = 0x003F; g_cpu.X = 0x0E00; dc_mvn(0, 0);
    g_cpu.A = 0x007F; g_cpu.X = 0x0E00; dc_mvn(0, 0);
    dc_set_mx(1, g_cpu.x_flag);
    g_cpu.DB = dc_pull8();
    dp16_set(0x81, 0x0F01);
    dp8_set(0x83, 0);
    g_cpu.X = 0x00FF;
    g_cpu.Y = 0x0000;
    u8 n = ram8(0x12000);
    do {
        dp16_set(0x80, g_cpu.X);
        dp16_set(0x80, g_cpu.Y);
    } while (--n);
    dc_set_a8(0);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
/* C0:7506 / 7553  Visible tile window on one axis: start dp 0A / 0E and end
 * dp 0C / 10, span 20 / 1C, centred on the camera tile 0102 / 0103 but kept
 * inside the map bounds dp 1A..1B / 1C..1D (all in 8-pixel tiles, doubled
 * for 16x16 tiles). */
static void tile_window(u8 lo, u8 hi, u16 cam, u8 out0, u8 out1, u8 span, u8 margin)
{
    g_cpu._flag_C = 0;
    u8 s = dc_adc8(dp8(lo), 8);
    dp8_set(0x52, s);
    u8 c = abs8(cam);
    dc_cmp8(c, s);
    if (c <= s) {
        u8 l = dp8(lo);
        g_cpu._flag_C = l >> 7;
        l = (u8)(l << 1);
        dp8_set(out0, l);
        g_cpu._flag_C = 0;
        u8 e = dc_adc8(l, span);
        dp8_set(out1, e);
        dc_set_a8(e);
        return;
    }
    g_cpu._flag_C = 1;
    u8 a = dc_sbc8(c, 8);
    g_cpu._flag_C = a >> 7;
    a = (u8)(a << 1);
    dp8_set(out0, a);
    g_cpu._flag_C = 1;
    u8 h = dc_sbc8(dp8(hi), margin);
    dc_cmp8(h, abs8(cam));
    if (g_cpu._flag_C) {
        g_cpu._flag_C = 0;
        u8 e = dc_adc8(dp8(out0), span);
        dp8_set(out1, e);
        dc_set_a8(e);
        return;
    }
    u8 e = (u8)(dp8(hi) + 1);
    g_cpu._flag_C = e >> 7;
    e = (u8)(e << 1);
    dp8_set(out1, e);
    g_cpu._flag_C = 1;
    u8 st = dc_sbc8(e, span);
    dp8_set(out0, st);
    dc_set_a8(st);
}
DC_ENTRY(C07506, RTS) { tile_window(0x1A, 0x1B, 0x0102, 0x0A, 0x0C, 0x20, 8); }
DC_ENTRY(C07553, RTS) { tile_window(0x1C, 0x1D, 0x0103, 0x0E, 0x10, 0x1C, 6); }
/* C0:8445 84C1 8523 8585 85E7 8649  Upload a tile-map row (VRAM increment
 * 1) in up to three parts from 7E:base: n1 bytes to addr1, n2 to addr2,
 * n3 to addr3 (dp words; a zero count skips its part). */
static void map_row3(const u8 n_dp[3], const u8 a_dp[3], u16 base, const u16 site[3])
{
    for (int k = 0; k < 3; k++) {
        u16 n = dp16(n_dp[k]);
        g_cpu.X = n;
        dc_set_flags_nz16(n);
        if (!n) continue;
        g_cpu.A = n;
        abs16_set(0x4375, n);
        u16 a = dp16(a_dp[k]);
        g_cpu.X = a;
        abs16_set(0x2116, a);
        u16 src = base;
        if (k == 0) g_cpu.X = base;
        else {
            g_cpu._flag_C = 0;
            u16 off = dp16(n_dp[0]);
            if (k == 2) off = adc16(off, dp16(n_dp[1]));
            src = adc16(off, base);
            g_cpu.A = src;
        }
        abs16_set(0x4372, src);
        dc_jsr(0xC084A7, 0xC00000 | site[k]);
    }
}
#define MAP_ROW3(pc, n1, n2, n3, a1, a2, a3, base, s1, s2, s3) \
    DC_ENTRY(pc, RTS) { static const u8 n[3] = { n1, n2, n3 }, a[3] = { a1, a2, a3 }; \
                        static const u16 s[3] = { s1, s2, s3 }; map_row3(n, a, base, s); }
MAP_ROW3(C08445, 0xDB, 0xDD, 0xDF, 0xA5, 0xA7, 0xA9, 0xC800, 0x845D, 0x847F, 0x84A3)
MAP_ROW3(C084C1, 0xE5, 0xE7, 0xE9, 0xB9, 0xBB, 0xBD, 0xD000, 0x84D9, 0x84FB, 0x851F)
MAP_ROW3(C08523, 0xEF, 0xF1, 0xF3, 0xCD, 0xCF, 0xD1, 0xD800, 0x853B, 0x855D, 0x8581)
MAP_ROW3(C08585, 0xDB, 0xDD, 0xDF, 0x9F, 0xA1, 0xA3, 0xC800, 0x859D, 0x85BF, 0x85E3)
MAP_ROW3(C085E7, 0xE5, 0xE7, 0xE9, 0xB3, 0xB5, 0xB7, 0xD000, 0x85FF, 0x8621, 0x8645)
MAP_ROW3(C08649, 0xEF, 0xF1, 0xF3, 0xC7, 0xC9, 0xCB, 0xD800, 0x8661, 0x8683, 0x86A7)

/* C0:81D6  (16-bit) Tile-map address of column A, row Y (as C0:8218) in X,
 * the column (+3E0 on the second screen) in Y, and the split of a column
 * upload at row Y: dp 6C = (32 - Y) * 2 bytes before the wrap, dp 6E =
 * Y * 2 after it. */
DC_ENTRY(C081D6, RTS)
{
    u16 col = g_cpu.A, row = g_cpu.Y;
    dp16_set(0x62, col);
    dp16_set(0x64, row);
    u16 r32 = (u16)(row << 5);
    dp16_set(0x52, r32);
    g_cpu._flag_C = 0;
    if (col < 0x20) {
        g_cpu.X = adc16(col, r32);
        g_cpu.Y = col;
    } else {
        u16 a = adc16((u16)(col - 0x20), r32);
        g_cpu._flag_C = 0;
        g_cpu.X = adc16(a, 0x0400);
        g_cpu._flag_C = 0;
        g_cpu.Y = adc16(col, 0x03E0);
    }
    g_cpu._flag_C = 1;
    u16 first = sbc16v(0x20, row);
    dp16_set(0x6C, first);
    g_cpu._flag_C = 1;
    u16 rest = sbc16v(0x20, first);
    g_cpu._flag_C = rest >> 15;
    rest = (u16)(rest << 1);
    g_cpu.A = rest;
    dp16_set(0x6E, rest);
    g_cpu._flag_C = first >> 15;
    first = (u16)(first << 1);
    dp16_set(0x6C, first);
    dc_set_flags_nz16(first);
}
/* C0:81A1  (16-bit) Tile-map address of column A, row Y in X, the address
 * 31 columns back in Y, and in A the row's offset on the other screen
 * (Y*32 + $400 on the first screen, Y*32 on the second). */
DC_ENTRY(C081A1, RTS)
{
    u16 col = g_cpu.A;
    dp16_set(0x62, col);
    u16 r32 = (u16)(g_cpu.Y << 5);
    dp16_set(0x52, r32);
    g_cpu._flag_C = 0;
    u16 x;
    if (col < 0x20) x = adc16(col, r32);
    else { x = adc16((u16)(col - 0x20), r32); g_cpu._flag_C = 0; x = adc16(x, 0x0400); }
    g_cpu.X = x;
    g_cpu._flag_C = 1;
    g_cpu.Y = sbc16v(x, 0x1F);
    if (col < 0x20) { g_cpu._flag_C = 0; g_cpu.A = adc16(r32, 0x0400); }
    else { g_cpu.A = r32; dc_set_flags_nz16(r32); }
}
/* C0:1F5A  Step the screen brightness dp 21 one level toward dp 22 every
 * dp 23 + 1 calls (dp 24 counts down); when it is reached, clear bit 3 of
 * dp 18 (fade running). */
DC_ENTRY(C01F5A, RTS)
{
    u8 cur = dp8(0x21), want = dp8(0x22);
    dc_set_a8(cur);
    dc_cmp8(cur, want);
    if (cur == want) {
        dc_set_a8(8);
        u8 f = dp8(0x18);
        g_cpu._flag_Z = !(f & 8);
        dp8_set(0x18, f & ~8);
        return;
    }
    u8 wait = dp8(0x24);
    dc_set_a8(wait);
    if (wait) { wait--; dp8_set(0x24, wait); dc_set_flags_nz8(wait); return; }
    u8 r = dp8(0x23);
    dp8_set(0x24, r);
    dc_set_a8(cur);
    u8 n = (u8)(cur > want ? cur - 1 : cur + 1);
    dp8_set(0x21, n);
    dc_set_flags_nz8(n);
}
/* C0:1F24  Step the mosaic size (dp 19 bits 0-3) one level toward dp 1B
 * every dp 1C + 1 calls (dp 1D counts down); when it reaches the target
 * (or 0 going down), clear bit 1 of dp 18. */
DC_ENTRY(C01F24, RTS)
{
    u8 cur = dp8(0x19) & 0x0F, want = dp8(0x1B);
    dc_set_a8(cur);
    dc_cmp8(cur, want);
    if (cur != want) {
        u8 wait = dp8(0x1D);
        dc_set_a8(wait);
        if (wait) { wait--; dp8_set(0x1D, wait); dc_set_flags_nz8(wait); return; }
        dp8_set(0x1D, dp8(0x1C));
        if (cur < want) {
            dc_set_a8(cur);
            u8 n = (u8)(dp8(0x19) + 1);
            dp8_set(0x19, n);
            dc_set_flags_nz8(n);
            return;
        }
        u8 n = (u8)(dp8(0x19) - 1);
        dc_set_a8(n);
        dp8_set(0x19, n);
        dc_set_flags_nz8(n);
        if (n) return;
    }
    dc_set_a8(2);
    u8 f = dp8(0x18);
    g_cpu._flag_Z = !(f & 2);
    dp8_set(0x18, f & ~2);
}
/* C0:B096  Copy a rectangle inside 7E with a 256-byte row stride: rows
 * dp 3F..41 (dp DD + 1 rows) of columns dp 3E..40 (dp DB + 1 bytes), from
 * source offset dp 3E + F0 to destination dp 42 + F0. dp DF / E1 = the
 * low bytes of the two start offsets. */
DC_ENTRY(C0B096, RTS)
{
    dp8_set(0xDC, 0);
    dp8_set(0xDE, 0);
    g_cpu._flag_C = 1;
    dp8_set(0xDB, dc_sbc8(dp8(0x40), dp8(0x3E)));
    g_cpu._flag_C = 1;
    dp8_set(0xDD, dc_sbc8(dp8(0x41), dp8(0x3F)));
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu._flag_C = 0;
    u16 src = adc16(dp16(0x3E), dp16(0xF0));
    g_cpu.X = src;
    dp16_set(0xDF, src & 0xFF);
    g_cpu._flag_C = 0;
    u16 dst = adc16(dp16(0x42), dp16(0xF0));
    g_cpu.Y = dst;
    dp16_set(0xE1, dst & 0xFF);
    for (;;) {
        u16 w = dp16(0xDB);
        g_cpu.A = w;
        dc_push8(g_cpu.DB);
        dc_mvn(0x7E, 0x7E);
        g_cpu.DB = dc_pull8();
        u16 rows = dp16(0xDD);
        g_cpu.A = rows;
        dc_set_flags_nz16(rows);
        if (!rows) break;
        dp16_set(0xDD, (u16)(rows - 1));
        g_cpu._flag_C = 1;
        u16 a = (u16)(sbc16v(g_cpu.X, dp16(0xDB)) - 1);
        g_cpu._flag_C = 0;
        g_cpu.X = adc16(a, 0x0100);
        g_cpu._flag_C = 1;
        a = (u16)(sbc16v(g_cpu.Y, dp16(0xDB)) - 1);
        g_cpu._flag_C = 0;
        g_cpu.A = g_cpu.Y = adc16(a, 0x0100);
    }
    dc_set_mx(1, g_cpu.x_flag);
}
/* C0:88EE  Run this frame's layer scroll step: handler (F9 & 0F) of the
 * table at C0:8902 (C0:8925 ... 8A3C, 8924 = none), with 8-bit index
 * registers, returning with 16-bit ones; nothing while 0138 is set. */
DC_ENTRY(C088EE, RTS)
{
    u8 busy = abs8(0x0138);
    dc_set_a8(busy);
    dc_set_flags_nz8(busy);
    if (busy) return;
    u8 i = (u8)((abs8(0x00F9) & 0x0F) << 1);
    dc_set_a8(i);
    g_cpu._flag_C = 0;
    dc_set_mx(1, 1);
    g_cpu.X = i;
    dc_jsr(0xC00000 | bus16(0xC08902 + i), 0xC088FC);
    dc_set_mx(g_cpu.m_flag, 0);
}
/* C0:7084  Load the map's seven 15-colour background palettes: palette set
 * F6:0003[dp FE] (0xD2 bytes each, from F6:24C0) into 7E:2022, 2042 ...
 * 20E2 (colour 0 of each palette left alone). */
DC_ENTRY(C07084, RTS)
{
    u16 i = dp16(0xFE);
    g_cpu.X = i;
    u8 set = bus8(0xF60003 + i);
    abs8_set(0x4202, set);
    abs8_set(0x4203, 0xD2);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    u16 src = dc_adc16(abs16(0x4216), 0x24C0);
    g_cpu.X = src;
    g_cpu.Y = 0x2022;
    for (int k = 0; k < 7; k++) {
        if (k) g_cpu.Y += 2;
        g_cpu.A = 0x1D;
        dc_push8(g_cpu.DB);
        dc_mvn(0x7E, 0xF6);
        g_cpu.DB = dc_pull8();
    }
    dc_set_flags_nz8(g_cpu.DB);
    dc_set_mx(1, 0);
}
/* C0:813D  (16-bit) Split a tile-map row upload at column A, row Y: X = the
 * address of the column, Y = the start of the other screen's row; dp 6C =
 * bytes before the screen edge, dp 6E = bytes after it (or a full row of
 * 0x40 with dp 6A = 2 and dp 5E = X - 1F when the row wraps a third time),
 * else dp 6A = 0. */
DC_ENTRY(C0813D, RTS)
{
    u16 col = g_cpu.A;
    dp16_set(0x62, col);
    u16 r32 = (u16)(g_cpu.Y << 5);
    dp16_set(0x52, r32);
    u16 first;
    g_cpu._flag_C = 0;
    if (col < 0x20) {
        g_cpu.X = dc_adc16(col, r32);
        g_cpu._flag_C = 0;
        g_cpu.Y = dc_adc16(r32, 0x0400);
        g_cpu._flag_C = 1;
        first = dc_sbc16(0x20, col);
    } else {
        u16 a = dc_adc16((u16)(col - 0x20), r32);
        g_cpu._flag_C = 0;
        g_cpu.X = dc_adc16(a, 0x0400);
        g_cpu.Y = r32;
        g_cpu._flag_C = 1;
        first = dc_sbc16(0x40, col);
    }
    dp16_set(0x6C, first);
    g_cpu._flag_C = 1;
    u16 rest = dc_sbc16(0x22, first);
    dc_cmp16(rest, 0x21);
    if (g_cpu._flag_C) {
        dp16_set(0x6E, 0x40);
        dp16_set(0x6C, (u16)(first << 1));
        dp16_set(0x6A, 2);
        g_cpu._flag_C = 1;
        u16 e = dc_sbc16(g_cpu.X, 0x1F);
        g_cpu.A = e;
        dp16_set(0x5E, e);
        return;
    }
    g_cpu._flag_C = rest >> 15;
    rest = (u16)(rest << 1);
    g_cpu.A = rest;
    dp16_set(0x6E, rest);
    g_cpu._flag_C = first >> 15;
    dp16_set(0x6C, (u16)(first << 1));
    dc_set_flags_nz16((u16)(first << 1));
    dp16_set(0x6A, 0);
}
/* C0:039B  Snapshot for the menu: the 16x16 tiles of both collision layers
 * around the camera (7E:7000 / 7040, rows of 0x100, at camera/2 masked by
 * 1D1E/1D1F) into 7E:2A23 / 2B23, and the palette 7E:2000 into 7E:2200. */
static void copy_16x16(u16 src, u16 dst)
{
    g_cpu.X = src;
    g_cpu.Y = dst;
    dp16_set(0xD9, 0x0F);
    for (;;) {
        g_cpu.A = 0x0F;
        dc_push8(g_cpu.DB);
        dc_mvn(0x7E, 0x7E);
        g_cpu.DB = dc_pull8();
        u16 n = dp16(0xD9);
        g_cpu.A = n;
        dc_set_flags_nz16(n);
        if (!n) return;
        dp16_set(0xD9, (u16)(n - 1));
        g_cpu._flag_C = 1;
        u16 a = dc_sbc16(g_cpu.X, 0x10);
        g_cpu._flag_C = 0;
        g_cpu.X = g_cpu.A = dc_adc16(a, 0x0100);
    }
}
DC_ENTRY(C0039B, RTS)
{
    u8 cx = (u8)((abs8(0x1D0A) >> 1) & abs8(0x1D1E));
    dp8_set(0xDB, cx);
    u8 cy = (u8)((abs8(0x1D0E) >> 1) & abs8(0x1D1F));
    dp8_set(0xDD, cy);
    u16 at = (u16)(cy << 8 | cx);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    copy_16x16(dc_adc16(at, 0x7000), 0x2A23);
    at = (u16)(dp8(0xDD) << 8 | dp8(0xDB));
    g_cpu._flag_C = 0;
    copy_16x16(dc_adc16(at, 0x7040), 0x2B23);
    g_cpu.X = 0x2000; g_cpu.Y = 0x2200; g_cpu.A = 0x01FF;
    dc_push8(g_cpu.DB);
    dc_mvn(0x7E, 0x7E);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
    dc_set_mx(1, 0);
}
/* C0:2B78  (long) Fill 7E:F000 with 256 tile-map words (tile n, attribute
 * 29) through the WRAM port, 16 at a time (C0:2C32 from n*16 to n*16 + F)
 * for n = 0 2 1 3 4 6 5 7 8 A 9 B C E D F. */
DC_ENTRY(C02B78, RTL)
{
    dc_push16(g_cpu.D);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.D = 0x2100;
    dc_set_mx(1, 0);
    g_cpu.X = 0xF000;
    dp16_set(0x81, 0xF000);
    dp8_set(0x83, 0);
    dc_set_mx(1, 1);
    g_cpu.Y = 0x29;
    static const u8 order[16] = { 0, 2, 1, 3, 4, 6, 5, 7, 8, 0xA, 9, 0xB, 0xC, 0xE, 0xD, 0xF };
    for (int k = 0; k < 16; k++) {
        u8 n = (u8)(order[k] << 4);
        abs8_set(0x01D9, (u8)(n | 0x0F));
        dc_set_a8(n);
        dc_set_flags_nz8(n);
        dc_jsr(0xC02C32, 0xC02B95 + 10 * k);
    }
    dc_set_mx(1, 0);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
/* C0:7D66 / 7E60  Write one row / column of a layer's tile map through the
 * WRAM port (address set by the caller): dp 5C 8x8 tiles starting at
 * 16x16-tile position (dp 58, dp 5A) / 2 in the layer 3 map 7E:3080 (rows of 0x100,
 * wrapping with the size masks dp 22 / 23), each 16x16 tile's four words
 * taken from 7E:C000 + 8 * tile. Odd dp 58 / 5A start in the right / bottom
 * half of the first tile. The X index truncation of the 8-bit index entry
 * is kept: with 8-bit index registers the map index and the tile offset
 * are their low bytes. */
static u16 map_tile(int layer, u16 xi)
{
    if (layer == 0) return ram8(0x3080 + xi);
    u8 hi = ram8(0x30C0 + xi);
    if (layer == 2) hi >>= 1;
    return (u16)((hi & 1) << 8 | ram8((layer == 1 ? 0x3000 : 0x3040) + xi));
}
static void meta_line(bool vertical, int layer, u32 meta, u8 xmask_dp, u8 ymask_dp)
{
    u8 hy = dp8(0x5A) & 1, hx = dp8(0x58) & 1;
    u8 y = (u8)((dp8(0x5A) >> 1) & dp8(ymask_dp));
    dp8_set(0x54, y);
    u8 x = (u8)((dp8(0x58) >> 1) & dp8(xmask_dp));
    dp8_set(0x52, x);
    /* Offsets inside the tile's 8 bytes: words TL 0, TR 2, BL 4, BR 6. */
    u8 first = vertical ? (u8)(hx * 2) : (u8)(hy * 4);
    u8 step = vertical ? 4 : 2;
    bool half = vertical ? hy : hx;
    for (bool lead = half;; lead = false) {
        u16 a = (u16)(y << 8 | x);
        u16 xi = g_cpu.x_flag ? (a & 0xFF) : a;
        u16 t = (u16)(map_tile(layer, xi) << 3);
        xi = g_cpu.x_flag ? (t & 0xFF) : t;
        g_cpu.X = xi;
        g_cpu._flag_C = 0;
        u8 o = (u8)(first + (lead ? step : 0));
        u16 w = ram16(meta + xi + o);
        abs8_set(0x2180, (u8)w);
        abs8_set(0x2180, (u8)(w >> 8));
        if (!lead) {
            abs8_set(0x2180, ram8(meta + xi + o + step));
            abs8_set(0x2180, ram8(meta + xi + o + step + 1));
        }
        if (vertical) {
            if (!lead) {
                u8 n = (u8)(dp8(0x5C) - 1);
                dp8_set(0x5C, n);
                if (!n) { dc_set_a8(y); dc_set_flags_nz8(0); return; }
            }
            y = (u8)((y + 1) & dp8(ymask_dp));
            dp8_set(0x54, y);
        } else {
            x = (u8)((x + 1) & dp8(xmask_dp));
            dp8_set(0x52, x);
            if (!lead) {
                u8 n = (u8)(dp8(0x5C) - 1);
                dp8_set(0x5C, n);
                if (!n) { dc_set_a8(x); dc_set_flags_nz8(0); return; }
            }
        }
    }
}
DC_ENTRY(C07D66, RTS) { meta_line(false, 0, 0xC000, 0x22, 0x23); }
DC_ENTRY(C07E60, RTS) { meta_line(true,  0, 0xC000, 0x22, 0x23); }
/* C0:77E4 / 7BA9  Columns of layers 1 / 2: 9-bit tiles from 7E:3000 /
 * 3040 with bit 8 from 7E:30C0 bit 0 / 1, tiles at 7E:B000, masks dp
 * 1E/1F / 20/21. */
DC_ENTRY(C077E4, RTS) { meta_line(true, 1, 0xB000, 0x1E, 0x1F); }
DC_ENTRY(C07BA9, RTS) { meta_line(true, 2, 0xB000, 0x20, 0x21); }
/* C0:7612 / 79CF  Rows of layers 1 / 2 (masks dp 1E/1F / 20/21) that may
 * start on a tile's right half: dp 5C tiles from (dp 58, dp 5A) in half
 * tiles; an odd start writes the first tile's right half, dp 5C - 1 whole
 * tiles and the last tile's left half. */
static u16 meta_emit(int layer, u8 x, u8 y, u8 off, bool whole)
{
    u16 a = (u16)(y << 8 | x);
    u16 xi = g_cpu.x_flag ? (a & 0xFF) : a;
    u16 t = (u16)(map_tile(layer, xi) << 3);
    xi = g_cpu.x_flag ? (t & 0xFF) : t;
    g_cpu.X = xi;
    u16 w = ram16(0xB000 + xi + off);
    abs8_set(0x2180, (u8)w);
    abs8_set(0x2180, (u8)(w >> 8));
    if (whole) {
        abs8_set(0x2180, ram8(0xB000 + xi + off + 2));
        abs8_set(0x2180, ram8(0xB000 + xi + off + 3));
    }
    return w;
}
static void meta_row_split(int layer, u8 xmask_dp, u8 ymask_dp)
{
    u8 off = (u8)((dp8(0x5A) & 1) * 4);
    u8 y = (u8)((dp8(0x5A) >> 1) & dp8(ymask_dp));
    dp8_set(0x54, y);
    bool odd = dp8(0x58) & 1;
    u8 x = (u8)((dp8(0x58) >> 1) & dp8(xmask_dp));
    dp8_set(0x52, x);
    u16 w;
    g_cpu._flag_C = 0;
    if (odd) {
        meta_emit(layer, x, y, (u8)(off + 2), false);
        dp8_set(0x5C, (u8)(dp8(0x5C) - 1));
        x = (u8)((x + 1) & dp8(xmask_dp));
        dp8_set(0x52, x);
    }
    for (;;) {
        w = meta_emit(layer, x, y, off, true);
        x = (u8)((x + 1) & dp8(xmask_dp));
        dp8_set(0x52, x);
        u8 n = (u8)(dp8(0x5C) - 1);
        dp8_set(0x5C, n);
        if (!n) break;
    }
    if (!odd) {
        g_cpu.A = (u16)((w & 0xFF) << 8 | x);
        dc_set_flags_nz8(0);
        return;
    }
    w = meta_emit(layer, x, y, (u8)(off + 2), false);
    g_cpu.A = (u16)(w << 8 | w >> 8);
    dc_set_flags_nz8((u8)(w >> 8));
}
DC_ENTRY(C07612, RTS) { meta_row_split(1, 0x1E, 0x1F); }
DC_ENTRY(C079CF, RTS) { meta_row_split(2, 0x20, 0x21); }
/* C0:82A5 8325 8345 ... 8425  Queue one tile-map row or column of a layer:
 * point the WRAM port at the layer's buffer, start at (dp sx [- 1], dp sy
 * [- 1]) for n tiles, run the row/column writer and set the layer's bit in
 * the upload flags. Written for 16-bit index registers (the 8-bit entries
 * the analysis found decode as garbage). */
static void line_job(u16 buf, u8 sx, bool xdec, u8 n, u8 sy, bool ydec, u32 fn, u16 site, u8 bit, u8 flags)
{
    g_cpu.X = buf;
    abs16_set(0x2181, buf);
    abs8_set(0x2183, 0);
    dp8_set(0x58, (u8)(dp8(sx) - xdec));
    dp8_set(0x5C, n);
    dp8_set(0x5A, (u8)(dp8(sy) - ydec));
    dc_jsr(fn, 0xC00000 | site);
    dc_set_a8(bit);
    u8 f = dp8(flags);
    g_cpu._flag_Z = !(f & bit);
    dp8_set(flags, f | bit);
}
#define LINE_JOB(pc, buf, sx, xdec, n, sy, ydec, fn, bit, flags) \
    DC_ENTRY(pc, RTS) { line_job(buf, sx, xdec, n, sy, ydec, fn, (u16)(0x##pc + 0x18), bit, flags); }
LINE_JOB(C08243, 0xC800, 0x0A, 1, 0x12, 0x10, 0, 0xC07612, 1, 0x78)
/* C0:8263 has an extra SEP #$20 before the port bank write. */
DC_ENTRY(C08263, RTS) { line_job(0xD000, 0x0A, 1, 0x12, 0x10, 0, 0xC079CF, 0x827D, 2, 0x78); }
LINE_JOB(C08285, 0xD000, 0x12, 1, 0x12, 0x18, 0, 0xC079CF, 2, 0x78)
LINE_JOB(C082A5, 0xD800, 0x0A, 1, 0x12, 0x10, 0, 0xC07D66, 4, 0x78)
LINE_JOB(C082C5, 0xC800, 0x0A, 1, 0x12, 0x0E, 0, 0xC07612, 1, 0x79)
LINE_JOB(C082E5, 0xD000, 0x0A, 1, 0x12, 0x0E, 0, 0xC079CF, 2, 0x79)
LINE_JOB(C08305, 0xD000, 0x12, 1, 0x12, 0x16, 0, 0xC079CF, 2, 0x79)
LINE_JOB(C08325, 0xD800, 0x0A, 1, 0x12, 0x0E, 0, 0xC07D66, 4, 0x79)
LINE_JOB(C08345, 0xC880, 0x0C, 0, 0x0F, 0x0E, 1, 0xC077E4, 1, 0x7A)
LINE_JOB(C08365, 0xD080, 0x0C, 0, 0x0F, 0x0E, 1, 0xC07BA9, 2, 0x7A)
LINE_JOB(C08385, 0xD080, 0x14, 0, 0x0F, 0x16, 1, 0xC07BA9, 2, 0x7A)
LINE_JOB(C083A5, 0xD880, 0x0C, 0, 0x0F, 0x0E, 1, 0xC07E60, 4, 0x7A)
LINE_JOB(C083C5, 0xC880, 0x0A, 0, 0x0F, 0x0E, 1, 0xC077E4, 1, 0x7B)
LINE_JOB(C083E5, 0xD080, 0x0A, 0, 0x0F, 0x0E, 1, 0xC07BA9, 2, 0x7B)
LINE_JOB(C08405, 0xD080, 0x12, 0, 0x0F, 0x16, 1, 0xC07BA9, 2, 0x7B)
LINE_JOB(C08425, 0xD880, 0x0A, 0, 0x0F, 0x0E, 1, 0xC07E60, 4, 0x7B)
/* C0:274D  Per-frame layer 3 work from dp 54: bit 4 writes the next line of
 * the text at FD:A000 + 0BE1 (letters as tiles - 40, '.' = 1B, space = 0,
 * attribute 2C; '/' ends the line, a byte >= 80 ends the text and clears
 * bit 4) into 7E:D800 after clearing it from D2:0000, and queues it
 * (C0:27EB); bits 2-3 move to dp 46 bits 3-4 and reload the blank pattern
 * (C0:75A0); with bit 5 alone, dp 29 = 0D (leave) once the point dp 5D/5E
 * is outside the camera box 1D0A..1D10. */
DC_ENTRY(C0274D, RTS)
{
    u8 v = dp8(0x54);
    dc_set_a8(v);
    if (!(v & 0x1C)) {
        g_cpu._flag_Z = !(v & 0x20);
        if (!(v & 0x20)) return;
        u8 px = dp8(0x5D), py = dp8(0x5E);
        u8 l = abs8(0x1D0A) >> 1, r = abs8(0x1D0C) >> 1, tp = abs8(0x1D0E) >> 1, b = abs8(0x1D10) >> 1;
        bool out;
        dc_set_a8(l); dc_cmp8(l, px); out = l >= px;
        if (!out) { dc_set_a8(r); dc_cmp8(r, px); out = r < px; }
        if (!out) { dc_set_a8(tp); dc_cmp8(tp, py); out = tp >= py; }
        if (!out) { dc_set_a8(b); dc_cmp8(b, py); out = b < py; }
        if (out) { dc_set_a8(0x0D); dc_set_flags_nz8(0x0D); dp8_set(0x29, 0x0D); }
        return;
    }
    if (!(v & 0x10)) {
        u8 a = (u8)((v & 0x0C) << 1);
        g_cpu._flag_C = 0;
        dc_set_a8(a);
        u8 f = dp8(0x46);
        g_cpu._flag_Z = !(f & a);
        dp8_set(0x46, f | a);
        dc_jsr(0xC075A0, 0xC02787);
        dc_set_a8(0x0C);
        f = dp8(0x54);
        g_cpu._flag_Z = !(f & 0x0C);
        dp8_set(0x54, f & ~0x0C);
        return;
    }
    dc_set_mx(0, 0);
    dc_push8(g_cpu.DB);
    g_cpu.X = 0; g_cpu.Y = 0xD800; g_cpu.A = 0x1F;
    dc_mvn(0x7E, 0xD2);
    g_cpu.X = 0; g_cpu.A = 0x1F;
    dc_mvn(0x7E, 0xD2);
    g_cpu.DB = dc_pull8();
    dc_set_mx(1, 0);
    g_cpu.X = 0xD800;
    abs16_set(0x2181, 0xD800);
    abs8_set(0x2183, 0);
    u16 x = abs16(0x0BE1);
    for (;;) {
        g_cpu.X = x;
        u8 c = bus8(0xFDA000 + x);
        dc_set_a8(c);
        dc_set_flags_nz8(c);
        if (c & 0x80) break;
        dc_cmp8(c, 0x2F);
        if (c == 0x2F) { map_row_next(); return; }
        if (c == 0x2E) c = 0x1B;
        else if (c == 0x20) c = 0;
        else { g_cpu._flag_C = 1; c = dc_sbc8(c, 0x40); }
        abs8_set(0x2180, c);
        abs8_set(0x2180, 0x2C);
        x++;
    }
    dc_set_a8(0x10);
    u8 f = dp8(0x54);
    g_cpu._flag_Z = !(f & 0x10);
    dp8_set(0x54, f & ~0x10);
    dc_jsr(0xC027EB, 0xC027E4);
    u8 n = (u8)(abs8(0x0BE4) + 1);
    abs8_set(0x0BE4, n);
    dc_set_flags_nz8(n);
}
/* C0:979B / 97CC / 97FD / 982E  Run the queued row/column jobs of layers
 * 1-3 (bits 0-2 of dp 76 / 77) and step each layer's line counter dp 99 /
 * 9B / 9D forward (rows) or back (columns), modulo 64. C0:985F / 9890 /
 * 98C1 / 98F2 do the same for the split-row jobs (dp 74 / 75) with the
 * counters dp 9A / 9C / 9E, modulo 32. */
static void layer_jobs(u8 flags, const u32 fn[3], u16 pc, bool fwd, u8 cnt, u8 mask)
{
    u8 f = dp8(flags);
    dc_set_a8(f);
    for (int k = 0; k < 3; k++) {
        u8 bit = (u8)(1 << k);
        g_cpu._flag_Z = !(f & bit);
        if (!(f & bit)) continue;
        dc_jsr(fn[k], 0xC00000 | (u16)(pc + 6 + 16 * k));
        u8 c = (u8)((dp8(cnt + 2 * k) + (fwd ? 1 : -1)) & mask);
        dp8_set(cnt + 2 * k, c);
        dc_set_flags_nz8(c);
        if (k < 2) { f = dp8(flags); dc_set_a8(f); } else dc_set_a8(c);
    }
}
#define LAYER_JOBS(pc, flags, f1, f2, f3, fwd) \
    DC_ENTRY(pc, RTS) { static const u32 fn[3] = { f1, f2, f3 }; layer_jobs(flags, fn, (u16)0x##pc, fwd, 0x99, 0x3F); }
#define SPLIT_JOBS(pc, flags, f1, f2, f3, fwd) \
    DC_ENTRY(pc, RTS) { static const u32 fn[3] = { f1, f2, f3 }; layer_jobs(flags, fn, (u16)0x##pc, fwd, 0x9A, 0x1F); }
LAYER_JOBS(C0979B, 0x76, 0xC08345, 0xC08365, 0xC083A5, true)
LAYER_JOBS(C097CC, 0x76, 0xC08345, 0xC08385, 0xC083A5, true)
LAYER_JOBS(C097FD, 0x77, 0xC083C5, 0xC083E5, 0xC08425, false)
LAYER_JOBS(C0982E, 0x77, 0xC083C5, 0xC08405, 0xC08425, false)
SPLIT_JOBS(C0985F, 0x74, 0xC08243, 0xC08263, 0xC082A5, true)
SPLIT_JOBS(C09890, 0x74, 0xC08243, 0xC08285, 0xC082A5, true)
SPLIT_JOBS(C098C1, 0x75, 0xC082C5, 0xC082E5, 0xC08325, false)
SPLIT_JOBS(C098F2, 0x75, 0xC082C5, 0xC08305, 0xC08325, false)
/* C0:7CB5  Build the whole layer 3 tile map into 7E:C800 (WRAM port): 32
 * rows of 16 tiles from (dp 0A, dp 0E); with 1D35 bit 2 (fixed layer) rows
 * 0-31 from column 0, then a second block: columns 32+ when 0BD3 != 10,
 * rows 32-63 when 0BD5 != 10 (dp 86 = 2), else rows 0-31 again. */
static void l3_rows(u8 col, u8 from, u8 to, u16 site)
{
    u8 r = from;
    do {
        dp8_set(0x5A, r);
        dp8_set(0x5C, 0x10);
        dc_jsr(0xC07D66, 0xC00000 | site);
        dp8_set(0x58, col);
        r = (u8)(dp8(0x5A) + 1);
        dc_set_a8(r);
        dc_cmp8(r, to);
    } while (r <= to);
}
DC_ENTRY(C07CB5, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0xC800;
    abs16_set(0x2181, 0xC800);
    dc_set_mx(1, g_cpu.x_flag);
    abs8_set(0x2183, 0);
    u8 f = abs8(0x1D35);
    dc_set_a8(f);
    if (!(f & 4)) {
        dp8_set(0x5A, dp8(0x0E));
        dp8_set(0x5E, 0x20);
        u8 n;
        do {
            dp8_set(0x58, dp8(0x0A));
            dp8_set(0x5C, 0x10);
            dc_jsr(0xC07D66, 0xC07CDD);
            dp8_set(0x5A, (u8)(dp8(0x5A) + 1));
            n = (u8)(dp8(0x5E) - 1);
            dp8_set(0x5E, n);
        } while (n);
        dc_set_flags_nz8(0);
        return;
    }
    dp8_set(0x58, 0);
    l3_rows(0, 0x00, 0x1F, 0x7CF1);
    u8 a = abs8(0x0BD3);
    dc_set_a8(a);
    dc_cmp8(a, 0x10);
    if (a != 0x10) { dp8_set(0x58, 0x20); l3_rows(0x20, 0x00, 0x1F, 0x7D34); return; }
    a = abs8(0x0BD5);
    dc_set_a8(a);
    dc_cmp8(a, 0x10);
    if (a != 0x10) {
        dp8_set(0x58, 0);
        l3_rows(0, 0x20, 0x3F, 0x7D51);
        dc_set_a8(2);
        dc_set_flags_nz8(2);
        dp8_set(0x86, 2);
        return;
    }
    dp8_set(0x58, 0);
    l3_rows(0, 0x00, 0x1F, 0x7D19);
}
/* C0:F05E  Draw the cursor of a 4-choice window (choice 0163): erase the
 * other three cursor cells (C0:F159 / F142 / F12B / F110 for choices 0-3)
 * and put the 2x2 cursor tiles 28FC..28FF at VRAM 1C02 + 0x40 * choice. A choice >= 4
 * erases all four and sets dp 63 = 80. */
DC_ENTRY(C0F05E, RTS)
{
    static const u32 erase[4] = { 0xC0F110, 0xC0F12B, 0xC0F142, 0xC0F159 };
    dc_push16(g_cpu.D);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0x2100;
    g_cpu.D = 0x2100;
    dc_set_mx(1, g_cpu.x_flag);
    dc_set_a8(0x80);
    dp8_set(0x15, 0x80);
    u8 c = abs8(0x0163);
    dc_set_a8(c);
    if (c >= 4) {
        static const u16 sites[4] = { 0xF079, 0xF07C, 0xF07F, 0xF082 };
        for (int k = 0; k < 4; k++) dc_jsr(erase[3 - k], 0xC00000 | sites[k]);
        g_cpu.D = dc_pull16();
        dc_set_a8(0x80);
        dc_set_flags_nz8(0x80);
        dp8_set(0x63, 0x80);
        return;
    }
    dc_set_a8(0);   /* the DEC A chain that picked the branch */
    static const u16 base[4] = { 0xF08B, 0xF0F0, 0xF0D0, 0xF0AB };
    int k = 0;
    for (int e = 3; e >= 0; e--) {
        if (e == 3 - c) continue;   /* choice c's own cell */
        dc_jsr(erase[e], 0xC00000 | (u16)(base[c] + 3 * k));
        k++;
    }
    u16 at = (u16)(0x1C02 + 0x40 * c);
    for (int r = 0; r < 2; r++) {
        g_cpu.X = (u16)(at + 0x20 * r);
        dp16_set(0x16, g_cpu.X);
        u16 t = (u16)(0x28FC + 2 * r);
        g_cpu.X = t;
        dp16_set(0x18, t);
        g_cpu.X = ++t;
        dp16_set(0x18, t);
    }
    dc_set_flags_nz16(g_cpu.X);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
/* C0:7399  Map size masks and scroll bounds: dp 1E..23 = the wrap masks of
 * the layer sizes 0BCB, 0BCD ... 0BD5 (40 or 30 -> 3F, 20 -> 1F, else 0F);
 * dp 1A/1B/1C/1D = the camera bounds left/right/top/bottom from the map
 * header F6:000A + [01FE], or 0..size-1 when that header says none (bit 7). */
DC_ENTRY(C07399, RTS)
{
    for (int k = 0; k < 6; k++) {
        u8 s = abs8(0x0BCB + 2 * k);
        dc_set_a8(s);
        u8 m;
        dc_cmp8(s, 0x40);
        if (s == 0x40) m = 0x3F;
        else { dc_cmp8(s, 0x20); if (s == 0x20) m = 0x1F; else { dc_cmp8(s, 0x30); m = s == 0x30 ? 0x3F : 0x0F; } }
        dc_set_a8(m);
        dc_set_flags_nz8(m);
        dp8_set(0x1E + k, m);
    }
    u16 x = abs16(0x01FE);
    g_cpu.X = x;
    u8 l = bus8(0xF6000A + x);
    dc_set_a8(l);
    dc_set_flags_nz8(l);
    if (!(l & 0x80)) {
        dp8_set(0x1A, l);
        dp8_set(0x1C, bus8(0xF6000B + x));
        dp8_set(0x1B, bus8(0xF6000C + x));
        u8 b = bus8(0xF6000D + x);
        dc_set_a8(b);
        dc_set_flags_nz8(b);
        dp8_set(0x1D, b);
        return;
    }
    dp8_set(0x1A, 0);
    dp8_set(0x1C, 0);
    dp8_set(0x1B, (u8)(abs8(0x0BCB) - 1));
    u8 b = (u8)(abs8(0x0BCD) - 1);
    dc_set_a8(b);
    dc_set_flags_nz8(b);
    dp8_set(0x1D, b);
}
/* C0:7F9A  (16-bit inside) Work out the VRAM addresses of the next tile-map
 * column and row uploads for layers 1-3 (VRAM bases dp 7C / 7E / 80, column
 * dp 99 / 9B / 9D, row dp 9A / 9C / 9E): for each layer, the column to the
 * left (C0:813D: 3 addresses + split dp 6C/6E/6A), the column on the other
 * screen (C0:81A1), the row above (C0:81D6, split) and the column 32 to the
 * right (C0:8218), stored in dp 9F.. (0x14 bytes per layer) and the split
 * sizes in dp DB.. (10 bytes per layer). */
static void layer_addrs(void)
{
    dc_set_mx(0, 0);
    for (int k = 0; k < 3; k++) {
        u8 o = (u8)(0x14 * k), s = (u8)(0x0A * k);
        u16 site = (u16)(0x7FA9 + 0x8A * k);
        u8 base = (u8)(0x7C + 2 * k), cdp = (u8)(0x99 + 2 * k), rdp = (u8)(0x9A + 2 * k);
        #define ADDB(v) (g_cpu._flag_C = 0, dc_adc16((v), dp16(base)))
        g_cpu.Y = dp16(rdp) & 0x1F;
        g_cpu.A = (u16)((dp16(cdp) - 1) & 0x3F);
        dc_push16(g_cpu.A);
        dc_jsr(0xC0813D, 0xC00000 | site);
        dp16_set(0x9F + o, ADDB(g_cpu.X));
        dp16_set(0xA1 + o, ADDB(g_cpu.Y));
        dp16_set(0xA3 + o, ADDB(dp16(0x5E)));
        dp16_set(0xDB + s, dp16(0x6C));
        dp16_set(0xDD + s, dp16(0x6E));
        dp16_set(0xDF + s, dp16(0x6A));
        g_cpu._flag_C = 0;
        g_cpu.Y = dc_adc16(dp16(rdp), 0x1C) & 0x1F;
        g_cpu.A = dc_pull16();
        dc_jsr(0xC081A1, 0xC00000 | (u16)(site + 0x2D));
        dp16_set(0xA7 + o, ADDB(g_cpu.A));
        dp16_set(0xA5 + o, ADDB(g_cpu.X));
        dp16_set(0xA9 + o, ADDB(g_cpu.Y));
        u16 up = (u16)((dp16(rdp) - 1) & 0x1F);
        dc_push16(up);
        g_cpu.Y = up;
        g_cpu.A = dp16(cdp) & 0x3F;
        dc_jsr(0xC081D6, 0xC00000 | (u16)(site + 0x4E));
        dp16_set(0xAB + o, ADDB(g_cpu.X));
        dp16_set(0xAD + o, ADDB(g_cpu.Y));
        dp16_set(0xE1 + s, dp16(0x6C));
        dp16_set(0xE3 + s, dp16(0x6E));
        g_cpu.Y = dc_pull16();
        g_cpu._flag_C = 0;
        g_cpu.A = dc_adc16(dp16(cdp), 0x20) & 0x3F;
        dc_jsr(0xC08218, 0xC00000 | (u16)(site + 0x6F));
        dp16_set(0xB1 + o, ADDB(g_cpu.A));
        g_cpu.A = ADDB(g_cpu.X);
        dp16_set(0xAF + o, g_cpu.A);
        #undef ADDB
    }
    dc_set_mx(1, 0);
}
DC_ENTRY(C07F9A, RTS) { layer_addrs(); }
/* C0:74A6  Set up the map view: visible tile windows (C0:7506 / 7553), the
 * initial redraw window dp 12..18 from them unless 0BC9 bit 7 is clear,
 * column/row counters dp 99..9E = 0, then the VRAM addresses (C0:7F9A). */
DC_ENTRY(C074A6, RTS)
{
    dc_jsr(0xC07506, 0xC074A6);
    dc_jsr(0xC07553, 0xC074A9);
    u8 f = abs8(0x0BC9);
    dc_set_a8(f);
    if (f & 0x80) {
        u8 l = (u8)((dp8(0x0A) >> 1) & 0xFE);
        dp8_set(0x12, l);
        g_cpu._flag_C = 0;
        dp8_set(0x14, dc_adc8(l, 0x20));
        u8 t = dp8(0x0E) >> 1;
        dp8_set(0x16, t);
        g_cpu._flag_C = 0;
        dp8_set(0x18, dc_adc8(t, 0x1C));
    }
    dp16_set(0x99, 0);
    dp16_set(0x9B, 0);
    dp16_set(0x9D, 0);
    layer_addrs();
}
/* C0:29F7  (long) Draw the checkered window frame into 7E:F000 through the
 * WRAM port as tile-map words (tile, attribute 20 / 23 = flipped): a top
 * edge F0 F1 F2 .. F3, four pairs of rows of alternating FC/FD and FE/FF
 * squares between F4/F5 and F6/F7 edges, and a bottom edge F8 F9 FA .. FB. */
static void port_word(u8 tile, u8 attr) { dp8_set(0x80, tile); dp8_set(0x80, attr); }
static void checker_row(u8 l, u8 a, u8 b, bool flip, u8 r)
{
    u8 f1 = flip ? 0x23 : 0x20, f2 = flip ? 0x20 : 0x23;
    port_word(l, 0x20);
    for (int k = 0; k < 7; k++) { port_word(a, f1); port_word(b, f1); port_word(a, f2); port_word(b, f2); }
    port_word(a, f1); port_word(b, f1);
    port_word(r, 0x20);
}
DC_ENTRY(C029F7, RTL)
{
    dc_push16(g_cpu.D);
    dc_set_mx(0, 0);
    g_cpu.A = 0x2100;
    g_cpu.D = 0x2100;
    dc_set_mx(1, 0);
    dp16_set(0x81, 0xF000);
    dp8_set(0x83, 0);
    port_word(0xF0, 0x20);
    for (int k = 0; k < 15; k++) { port_word(0xF1, 0x20); port_word(0xF2, 0x20); }
    port_word(0xF3, 0x20);
    for (int k = 0; k < 2; k++) {
        checker_row(0xF4, 0xFC, 0xFD, false, 0xF5);
        checker_row(0xF6, 0xFE, 0xFF, false, 0xF7);
        checker_row(0xF4, 0xFC, 0xFD, true, 0xF5);
        checker_row(0xF6, 0xFE, 0xFF, true, 0xF7);
    }
    port_word(0xF8, 0x20);
    for (int k = 0; k < 15; k++) { port_word(0xF9, 0x20); port_word(0xFA, 0x20); }
    port_word(0xFB, 0x20);
    g_cpu.A = 0x2120;
    g_cpu.X = 0;
    g_cpu.Y = 0;
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
/* C0:A521  Unpack the map from the WRAM stream at 7E:B506 (read through the
 * port): layer 1 and 2 tiles into 7E:3000 / 3040, layer 3 into 3080 when
 * 0BCA bit 0, each width 0BCB.. x height 0BCD.. in rows of 0x100; then the
 * per-tile attributes (7E:30C0, 7000, 7040), where a first byte with bit 7
 * starts a run of the same three bytes (count byte, 0 = 256). The row
 * counter is dp 55, the high byte of the width word dp 54, so X runs to
 * (row << 8) | width. */
static bool next_tile(u16 *x)
{
    (*x)++;
    if (*x < dp16(0x54)) return true;
    u8 row = (u8)(dp8(0x55) + 1);
    dp8_set(0x55, row);
    *x = (u16)(row << 8);
    g_cpu.A = *x;
    dc_cmp16(*x, dp16(0x52));
    return *x < dp16(0x52);
}
static void map_size(u16 w, u16 h)
{
    dp16_set(0x52, (u16)(abs8(h) << 8));
    dp16_set(0x54, abs8(w));
}
static void unpack_layer(u32 dst, u16 w, u16 h)
{
    map_size(w, h);
    u16 x = 0;
    do ram8_set(dst + x, abs8(0x2180)); while (next_tile(&x));
}
DC_ENTRY(C0A521, RTS)
{
    abs16_set(0x2181, 0xB506);
    abs8_set(0x2183, 0);
    unpack_layer(0x3000, 0x0BCB, 0x0BCD);
    unpack_layer(0x3040, 0x0BCF, 0x0BD1);
    if (abs8(0x0BCA) & 1) unpack_layer(0x3080, 0x0BD3, 0x0BD5);
    map_size(0x0BCB, 0x0BCD);
    u16 x = 0;
    for (;;) {
        g_cpu.X = x;
        u8 b = abs8(0x2180);
        if (!(b & 0x80)) {
            ram8_set(0x30C0 + x, b);
            ram8_set(0x7000 + x, abs8(0x2180));
            ram8_set(0x7040 + x, abs8(0x2180));
            if (!next_tile(&x)) break;
            continue;
        }
        b &= 0x7F;
        dp8_set(0x58, b);
        ram8_set(0x30C0 + x, b);
        u8 c1 = abs8(0x2180);
        dp8_set(0x5A, c1);
        ram8_set(0x7000 + x, c1);
        u8 c2 = abs8(0x2180);
        dp8_set(0x5C, c2);
        ram8_set(0x7040 + x, c2);
        u8 n = abs8(0x2180);
        u16 y = n ? (u16)((g_cpu.D << 8) | n) : 0x100;
        bool more = true;
        while (--y) {
            if (!next_tile(&x)) { more = false; break; }
            ram8_set(0x30C0 + x, dp8(0x58));
            ram8_set(0x7000 + x, dp8(0x5A));
            ram8_set(0x7040 + x, dp8(0x5C));
        }
        g_cpu.Y = y;
        if (!more) break;
        if (!next_tile(&x)) break;
    }
    g_cpu.X = x;
}
/* C0:74D4 / 74E8 / 74F7  Restart layer 1 / 2 / 3's column-row counters
 * (1D99 / 1D9B / 1D9D) and its scroll (1D87+4k, 1D89+4k) and recompute the
 * VRAM addresses (C0:7F9A, run with D = 1D00). */
static void layer_reset(int k)
{
    dc_push16(g_cpu.D);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0x1D00;
    g_cpu.D = 0x1D00;
    dp16_set((u8)(0x99 + 2 * k), 0);
    dp16_set((u8)(0x87 + 4 * k), 0);
    dp16_set((u8)(0x89 + 4 * k), 0);
    dc_set_mx(1, g_cpu.x_flag);
    dc_jsr(0xC07F9A, 0xC074E3);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
DC_ENTRY(C074D4, RTS) { layer_reset(0); }
DC_ENTRY(C074E8, RTS) { layer_reset(1); }
DC_ENTRY(C074F7, RTS) { layer_reset(2); }
/* C0:87F1  (D = 1D00) Queue the first column of each layer: layer 1, layer
 * 2 unless 1D35 bit 1 (C0:8385 when 0BC9 bit 7, else 8365), layer 3 unless
 * 1D35 bit 2, then recompute the VRAM addresses (C0:7F9A). */
DC_ENTRY(C087F1, RTS)
{
    dc_push16(g_cpu.D);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0x1D00;
    g_cpu.D = 0x1D00;
    dc_set_mx(1, g_cpu.x_flag);
    dc_jsr(0xC08345, 0xC087FA);
    u8 f = dp8(0x35);
    dc_set_a8(f);
    g_cpu._flag_Z = !(f & 2);
    if (!(f & 2)) {
        u8 v = abs8(0x0BC9);
        dc_set_a8(v);
        dc_set_flags_nz8(v);
        if (v & 0x80) dc_jsr(0xC08385, 0xC08808);
        else dc_jsr(0xC08365, 0xC0880D);
    }
    f = dp8(0x35);
    dc_set_a8(f);
    g_cpu._flag_Z = !(f & 4);
    if (!(f & 4)) dc_jsr(0xC083A5, 0xC08816);
    dc_jsr(0xC07F9A, 0xC08819);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}
/* C0:0A50  Map screen load: map setup (C0:A712, D = 1D00), FD:C0D7, VRAM bases of the four
 * layers (1D7C.. = 6000/6800/7000/7800) and their tile maps drawn (C0:75E9 / 78EC / 7CB5,
 * layer 3 only when 0BCA bit 0 and not 01BB) and uploaded through dp 48 (C0:7F62, or 7F77
 * when 1D35 bit 1 / 2), then the first columns queued (C0:87F1); 0BE9 overrides 1D86. */
static void d_1d00(void)
{
    dc_push16(g_cpu.D);
    dc_set_mx(0, 0); lda16(0x1D00); g_cpu.D = 0x1D00; dc_set_flags_nz16(0x1D00);
    dc_set_mx(1, 0);
}
static void d_restore(void) { g_cpu.D = dc_pull16(); dc_set_flags_nz16(g_cpu.D); }

/* VRAM base of a layer to dp 48 and its map uploaded (C0:7F77 when 1D35 has bit, else 7F62). */
static void layer_upload(u16 base, u8 bit, u32 site)
{
    ldx(abs16(base)); dp16_set(0x48, g_cpu.X);
    if (bit) {
        lda8(abs8(0x1D35)); g_cpu._flag_Z = !(g_cpu.A & bit);
        if (!g_cpu._flag_Z) { dc_jsr(0xC07F77, site); return; }
    }
    dc_jsr(0xC07F62, site + 5);
}

DC_ENTRY(C00A50, RTS)
{
    d_1d00(); dc_jsr(0xC0A712, 0xC00A59); d_restore();
    dc_jsl(0xFDC0D7, 0xC00A5D);
    dc_jsr(0xC0A509, 0xC00A61);
    d_1d00();
    dc_jsr(0xC074A6, 0xC00A6D);
    dc_jsr(0xC0A508, 0xC00A70);
    static const u16 vram[4] = { 0x6000, 0x6800, 0x7000, 0x7800 };
    for (int k = 0; k < 4; k++) { ldx(vram[k]); abs16_set(0x1D7C + 2 * k, vram[k]); }
    lda8(0x01); abs8_set(0x1D84, 1); abs8_set(0x1D85, 1); abs8_set(0x1D86, 1);
    dc_jsr(0xC075E9, 0xC00A96);
    d_restore();
    ldx(abs16(0x1D7C)); dp16_set(0x48, g_cpu.X);
    dc_jsr(0xC07F62, 0xC00A9F);
    d_1d00(); dc_jsr(0xC078EC, 0xC00AAB); d_restore();
    layer_upload(0x1D7E, 0x02, 0xC00ABB);
    d_1d00();
    lda8((u8)(abs8(0x0BCA) & 0x01));
    bool layer3 = !g_cpu._flag_Z;
    if (layer3) { lda8(abs8(0x01BB)); layer3 = g_cpu._flag_Z; }
    if (layer3) {
        dc_jsr(0xC07CB5, 0xC00AD8); d_restore();
        layer_upload(0x1D80, 0x04, 0xC00AE8);
    } else d_restore();
    dc_jsr(0xC087F1, 0xC00AF3);
    lda8(abs8(0x0BE9));
    if (!g_cpu._flag_Z) abs8_set(0x1D86, (u8)g_cpu.A);
}

/* C0:E534  (16-bit) Send sprite tile A & 7FF (32 bytes at [dp CD] + 32 *
 * tile) through the WRAM port, each byte translated by the table at DB:FD00
 * (indexed with D's low byte as the high byte, as the TDC/XBA leaves it).
 * Y is kept in dp C5 and restored. */
DC_ENTRY(C0E534, RTS)
{
    u16 off = (u16)((g_cpu.A & 0x07FF) << 5);
    g_cpu._flag_C = 0;
    dp16_set(0xC5, g_cpu.Y);
    u32 src = (u32)dp16(0xCD) | (u32)dp8(0xCF) << 16;
    u16 hi = (u16)(g_cpu.D << 8);
    u8 v = 0;
    dc_set_mx(1, 0);
    for (int k = 0; k < 32; k++) {
        u16 x = (u16)(hi | bus8((src + (u16)(off + k)) & 0xFFFFFF));
        g_cpu.X = x;
        v = abs8(0xFD00 + x);
        abs8_set(0x2180, v);
    }
    g_cpu.A = (u16)(hi | v);
    dc_set_mx(0, 0);
    g_cpu.Y = dp16(0xC5);
    dc_set_flags_nz16(g_cpu.Y);
}
/* C0:E687  (16-bit) Copy sprite tile A & 7FF (32 bytes at bank dp CF,
 * offset dp CD + 32 * tile; bank D2-D5 or 7F) to 7F:[dp D0], then point the
 * WRAM port 32 bytes further. Y is kept in dp C5 and restored. */
DC_ENTRY(C0E687, RTS)
{
    u16 off = (u16)((g_cpu.A & 0x07FF) << 5);
    dp16_set(0xC5, g_cpu.Y);
    g_cpu._flag_C = 0;
    u16 x = dc_adc16(off, dp16(0xCD));
    g_cpu.X = x;
    u16 y = dp16(0xD0);
    g_cpu.Y = y;
    dc_set_mx(1, 0);
    u8 bank = dp8(0xCF);
    dc_set_a8(bank);
    dc_cmp8(bank, 0x7F);
    if (bank != 0x7F) {
        g_cpu._flag_C = 1;
        u8 d = dc_sbc8(bank, 0xD2);
        if (d) { d--; if (d) { d--; } }
        dc_set_a8(d);
        dc_set_flags_nz8(d);
        if (bank < 0xD2 || bank > 0xD4) bank = 0xD5;   /* any other bank reads D5 */
    }
    dc_push8(g_cpu.DB);
    dc_push8(0x7F);
    g_cpu.DB = dc_pull8();
    dc_set_mx(0, 0);
    u16 w = 0;
    for (int k = 0; k < 32; k += 2) {
        w = bus16(((u32)bank << 16) + x + k);
        bus16_set((0x7F0000 + y + k) & 0xFFFFFF, w);
    }
    g_cpu.A = w;
    g_cpu.DB = dc_pull8();
    g_cpu._flag_C = 0;
    u16 next = dc_adc16(y, 0x20);
    g_cpu.A = next;
    abs16_set(0x2181, next);
    g_cpu.Y = dp16(0xC5);
    dc_set_flags_nz16(g_cpu.Y);
}

/* C0:2C41 helper: one drifting layer axis. The 4.4 fixed-point accumulator
 * at 7F:acc moves the 16-bit scroll 7F:pos (and its copy 7E:mirror) by its
 * whole part, then keeps only the fraction (sign-extended when negative).
 * A zero accumulator is skipped when `skip0`. */
static void drift_axis(u16 acc, u16 pos, u16 mirror, bool skip0)
{
    u8 v = abs8(acc);
    if (skip0 && !v) { dc_set_a8(v); return; }
    u16 p;
    if (!(v & 0x80)) {
        g_cpu._flag_C = 0;
        p = dc_adc16(v >> 4, abs16(pos));
    } else {
        u8 d = (u8)((u8)-v >> 4);
        dp8_set(0xD9, d);
        dp8_set(0xDA, 0);
        g_cpu._flag_C = 1;
        p = dc_sbc16(abs16(pos), d);
    }
    abs16_set(pos, p);
    ram16_set(mirror, p);
    u8 f = !(v & 0x0F) ? 0 : (v & 0x80) ? (u8)(v | 0xF0) : (u8)(v & 0x0F);
    abs8_set(acc, f);
    g_cpu.A = (u16)(p & 0xFF00 | f);
}
/* C0:2C41 helper: add the two speeds of a layer to its accumulators. */
static void drift_add(u16 speed, u16 acc)
{
    g_cpu._flag_C = 0;
    u8 r = dc_adc8(abs8(speed), abs8(acc));
    dc_set_a8(r);
    abs8_set(acc, r);
}
/* C0:2C41  Auto-scroll of the field layers (7F:3415 bit 1: BG1 with speeds
 * 3417/3418, bit 2: BG2 with speeds 3419/341A): scroll 7F:346B/346D and
 * 346F/3471, copied to 7E:2C74..2C7A. */
DC_ENTRY(C02C41, RTL)
{
    dc_push16(g_cpu.D);
    dc_push8(g_cpu.DB);
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0x0100;
    g_cpu.D = 0x0100;
    dc_set_mx(1, g_cpu.x_flag);
    dc_set_a8(0x7F);
    dc_push8(0x7F);
    g_cpu.DB = dc_pull8();
    u8 on = abs8(0x3415);
    dc_set_a8(on);
    if (on & 2) {
        drift_add(0x3417, 0x341D);
        drift_add(0x3418, 0x341E);
        drift_axis(0x341D, 0x346B, 0x2C74, true);
        drift_axis(0x341E, 0x346D, 0x2C76, false);
    }
    on = abs8(0x3415);
    dc_set_a8(on);
    if (on & 4) {
        drift_add(0x3419, 0x341F);
        drift_add(0x341A, 0x3420);
        drift_axis(0x341F, 0x346F, 0x2C78, false);
        drift_axis(0x3420, 0x3471, 0x2C7A, false);
    }
    g_cpu.DB = dc_pull8();
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* C0:ED15 helper: VRAM words from groups of four at 7E:src[g] to the VRAM
 * addresses listed at 0000:dst.. (one address word per tile word), with
 * absolute (DB-relative) PPU register writes as the original. */
static void tile_words(u16 dst, const u16 *src, int groups)
{
    abs8_set(0x2115, 0x80);
    dc_set_mx(0, g_cpu.x_flag);
    for (int g = 0; g < groups; g++)
        for (int i = 0; i < 4; i++, dst += 2) {
            abs16_set(0x2116, abs16(dst));
            u16 w = ram16(src[g] + 2 * i);
            g_cpu.A = w;
            abs16_set(0x2118, w);
        }
    dc_set_mx(1, g_cpu.x_flag);
}
/* C0:ED15  NMI-time upload of the dialog window's animated tiles: the
 * cursor (C0:F05E, when dp 63 >= 0), then with dp 5F bit 4 the window
 * frame for size dp 60 (0: 2 rows, 1: 4, else 6 groups of four tiles from
 * the 7E:BF00 tile buffer to the VRAM addresses 7E:09CA..) or with bit 1
 * the 4-tile marker for dp 61 (7E:BFF8 / BF08 to 09B2..); the handled bit
 * is cleared. */
DC_ENTRY(C0ED15, RTS)
{
    static const u16 frame0[] = { 0xBF38, 0xBF28 };
    static const u16 frame1[] = { 0xBF68, 0xBF48, 0xBF78, 0xBF58 };
    static const u16 frame2[] = { 0xBFD8, 0xBFB8, 0xBF98, 0xBFE8, 0xBFC8, 0xBFA8 };
    static const u16 mark0[] = { 0xBFF8 }, mark1[] = { 0xBF08 };
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0x0100;
    g_cpu.D = 0x0100;
    dc_set_mx(1, g_cpu.x_flag);
    u8 c = dp8(0x63);
    dc_set_a8(c);
    if (!(c & 0x80)) dc_jsr(0xC0F05E, 0xC0ED21);
    u8 f = dp8(0x5F);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    u8 bit;
    if (f & 0x10) {
        u8 n = dp8(0x60);
        if (n == 0) tile_words(0x09CA, frame0, 2);
        else if (n == 1) tile_words(0x09CA, frame1, 4);
        else tile_words(0x09CA, frame2, 6);
        bit = 0x10;
    } else if (f & 0x02) {
        tile_words(0x09B2, dp8(0x61) ? mark1 : mark0, 1);
        bit = 0x02;
    } else {
        g_cpu._flag_Z = 1;
        return;
    }
    dc_set_a8(bit);
    g_cpu._flag_N = 0;
    u8 m = dp8(0x5F);
    g_cpu._flag_Z = !(m & bit);
    dp8_set(0x5F, m & ~bit);
}

/* C0:AF4E helper (bits 6/7): copy one bit (mask) of the 7E:30C0 tile flags
 * over the rectangle of C0:B096 (dp 3E..40 x 3F..41 to dp 42). */
static void rect_flag_copy(u8 mask)
{
    dp8_set(0xDC, 0);
    dp8_set(0xDE, 0);
    dp8_set(0xE4, 0);
    g_cpu._flag_C = 1;
    dp8_set(0xDB, dc_sbc8(dp8(0x40), dp8(0x3E)));
    g_cpu._flag_C = 1;
    dp8_set(0xDD, dc_sbc8(dp8(0x41), dp8(0x3F)));
    dc_set_mx(0, 0);
    u16 x = dp16(0x3E), y = dp16(0x42);
    dp16_set(0xDF, x & 0xFF);
    dp16_set(0xE1, y & 0xFF);
    g_cpu.A = y & 0xFF;
    dc_set_mx(1, 0);
    dc_push8(g_cpu.DB);
    dc_set_a8(0x7E);
    dc_push8(0x7E);
    g_cpu.DB = dc_pull8();
    for (;;) {
        u8 n = dp8(0xDB);
        dp8_set(0xE3, n);
        for (;;) {
            u8 b = abs8(0x30C0 + x) & mask;
            dp8_set(0xD9, b);
            abs8_set(0x30C0 + y, (u8)((abs8(0x30C0 + y) & ~mask) | b));
            n = dp8(0xE3);
            if (!n) break;
            dp8_set(0xE3, (u8)(n - 1));
            x++; y++;
        }
        u8 r = dp8(0xDD);
        if (!r) break;
        dp8_set(0xDD, (u8)(r - 1));
        g_cpu._flag_C = 0;
        x = (u16)((dc_adc16(x, 0x0100) & 0xFF00) | dp16(0xDF));
        g_cpu._flag_C = 0;
        y = (u16)((dc_adc16(y, 0x0100) & 0xFF00) | dp16(0xE1));
        g_cpu.A = y;
    }
    g_cpu.X = x;
    g_cpu.Y = y;
    dc_set_a8(0);
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}
/* C0:AF4E  Apply the pending map-rectangle copies of dp 44: bits 0..5 copy
 * the rectangle (C0:B096) in the BG1/BG2/BG3 tile maps (7E:3000 / 3040 /
 * 3080 / 30C0, 7E:7000 / 7040 as dp F0), bits 6 and 7 copy only collision
 * flag bit 0 / bit 1 of 7E:30C0. */
DC_ENTRY(C0AF4E, RTS)
{
    static const u16 base[6] = { 0x3000, 0x3040, 0x3080, 0x30C0, 0x7000, 0x7040 };
    static const u16 site[6] = { 0xAF5C, 0xAF6A, 0xAF78, 0xAF86, 0xAF94, 0xAFA2 };
    u8 f = dp8(0x44);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (!f) return;
    for (int k = 0; k < 6; k++) {
        if (!(f & (1 << k))) continue;
        g_cpu.X = base[k];
        dp16_set(0xF0, base[k]);
        dc_jsr(0xC0B096, 0xC00000 | site[k]);
        f = dp8(0x44);
        dc_set_a8(f);
        dc_set_flags_nz8(f);
    }
    if (f & 0x40) rect_flag_copy(0x01);
    f = dp8(0x44);
    dc_set_a8(f);
    dc_set_flags_nz8(f);
    if (f & 0x80) rect_flag_copy(0x02);
    else g_cpu._flag_Z = 1;
}

/* C0:91AC helpers. Sum of direct-page bytes, each added with CLC (flags of
 * the last ADC). */
static u8 dp_sum(const u8 *offs, int n)
{
    u8 a = dp8(offs[0]);
    for (int i = 1; i < n; i++) { g_cpu._flag_C = 0; a = dc_adc8(a, dp8(offs[i])); }
    return a;
}
/* Split the 4.4 accumulator dp acc into its whole part (dp neg = pixels
 * left/up, dp pos = right/down, the other one 0) and keep the fraction. */
static u8 scroll_split(u8 acc, u8 neg, u8 pos)
{
    u8 v = dp8(acc);
    if (!(v & 0x80)) {
        g_cpu._flag_C = (v >> 3) & 1;
        dp8_set(pos, v >> 4);
        dp8_set(neg, 0);
    } else {
        u8 m = (u8)-v;
        g_cpu._flag_C = (m >> 3) & 1;
        dp8_set(neg, m >> 4);
        dp8_set(pos, 0);
    }
    u8 f = !(v & 0x0F) ? 0 : (v & 0x80) ? (u8)(v | 0xF0) : (u8)(v & 0x0F);
    dp8_set(acc, f);
    return f;
}
/* C0:91AC  Per-frame camera scroll of the three layers.
 * - With an automatic move (dp 00 != 0) the camera heads for tile
 *   (dp 02, dp 03): speed dp 2E / 30 = +-10 per axis until it is on the
 *   target tile and aligned (dp 87 / 89 low nibble 0), marking the axis
 *   done in dp 01; with both done dp 00 = 2, dp 01 = 0.
 * - With 01BC set the scroll jitters by 0400 bits 2 / 1 (dp 4E / 4F,
 *   negated when 0400 bit 0 is set), not on a locked axis (dp 0A / 0E = 0).
 * - The 4.4 accumulators: BG1 dp 3B/3C += speed + dp 24/25 + jitter; BG2
 *   dp 3D/3E and BG3 dp 3F/40 add their own speed (dp 37..3A) and drift
 *   dp 26..29, plus the camera speed and jitter unless dp 36 bit 1 / 2
 *   detaches them. Each is split into whole pixels (dp 41..4C, a pair per
 *   accumulator: left/up, right/down) and its fraction.
 * - dp 4D counts the drift down; at 0 the drift dp 24..29 stops. */
DC_ENTRY(C091AC, RTS)
{
    if (dp8(0x00)) {
        u8 a = dp8(0x02), t = dp8(0x0A);
        dc_cmp8(a, t);
        if (a != t) dp8_set(0x2E, a > t ? 0x10 : 0xF0);
        else if (dp8(0x87) & 0x0F) dp8_set(0x2E, 0xF0);
        else dp8_set(0x01, dp8(0x01) | 1);
        a = dp8(0x03); t = dp8(0x0E);
        dc_cmp8(a, t);
        if (a != t) dp8_set(0x30, a > t ? 0x10 : 0xF0);
        else if (dp8(0x89) & 0x0F) dp8_set(0x30, 0xF0);
        else {
            u8 d = dp8(0x01) | 2;
            dp8_set(0x01, d);
            dc_cmp8(d, 3);
            if (d == 3) { dp8_set(0x00, 2); dp8_set(0x01, 0); }
        }
    }
    if (abs8(0x01BC)) {
        u8 j = abs8(0x0400);
        u8 jx = (u8)((j & 4) << 2), jy = (u8)((j & 2) << 3);
        if (j & 1) { jx = (u8)-jx; jy = (u8)-jy; }
        dp8_set(0x4E, jx);
        dp8_set(0x4F, jy);
        if (!dp8(0x0A)) dp8_set(0x4E, 0);
        if (!dp8(0x0E)) dp8_set(0x4F, 0);
    }
    static const u8 bg1x[] = { 0x2E, 0x3B, 0x24, 0x4E }, bg1y[] = { 0x30, 0x3C, 0x25, 0x4F };
    static const u8 bg2x[] = { 0x2E, 0x37, 0x3D, 0x26, 0x4E }, bg2y[] = { 0x30, 0x38, 0x3E, 0x27, 0x4F };
    static const u8 bg3x[] = { 0x2E, 0x39, 0x3F, 0x28, 0x4E }, bg3y[] = { 0x30, 0x3A, 0x40, 0x29, 0x4F };
    dp8_set(0x3B, dp_sum(bg1x, 4));
    dp8_set(0x3C, dp_sum(bg1y, 4));
    bool own = dp8(0x36) & 2;   /* own speed only: drop the camera speed and jitter */
    dp8_set(0x3D, own ? dp_sum(bg2x + 1, 3) : dp_sum(bg2x, 5));
    dp8_set(0x3E, own ? dp_sum(bg2y + 1, 3) : dp_sum(bg2y, 5));
    own = dp8(0x36) & 4;
    dp8_set(0x3F, own ? dp_sum(bg3x + 1, 3) : dp_sum(bg3x, 5));
    dp8_set(0x40, own ? dp_sum(bg3y + 1, 3) : dp_sum(bg3y, 5));
    scroll_split(0x3B, 0x41, 0x42);
    scroll_split(0x3C, 0x43, 0x44);
    scroll_split(0x3D, 0x45, 0x46);
    scroll_split(0x3E, 0x47, 0x48);
    scroll_split(0x3F, 0x49, 0x4A);
    u8 a = scroll_split(0x40, 0x4B, 0x4C);
    dc_set_a8(a);
    u8 n = dp8(0x4D);
    if (!n) { dc_set_a8(0); dc_set_flags_nz8(0); return; }
    n--;
    dp8_set(0x4D, n);
    dc_set_flags_nz8(n);
    if (!n) for (u8 o = 0x24; o <= 0x29; o++) dp8_set(o, 0);
}

/* C0:E12A  Build a large (24-sprite) object's graphics and sprite layout:
 * the tile source is bank 1200[X] : 1280[X] (X = the entry X, 8-bit), the
 * frame data [1300:1380] of object dp 6D. The frame's first 60 words are tile numbers
 * (bit 14: translated through DB:FD00, C0:E534; else copied, C0:E687)
 * assembled at 7F:3800 (0D80 of object dp 6D = 3800, 0F01 = 0) and sent to
 * VRAM 0400 by DMA channel 7 (0C00 bytes); then the 24 sprites' X/Y
 * offsets (frame bytes C0..EF, X sign-extended) go to the object's sprite
 * list at 7F:4BC2 + 1700[obj] with tiles 40..4E / 60..6E / 80..8E and
 * attributes 22. */
DC_ENTRY(C0E12A, RTS)
{
    u8 e = (u8)g_cpu.X;
    dp8_set(0xCF, abs8(0x1200 + e));
    dp8_set(0xD2, 0x7F);
    dp16_set(0xCD, abs16(0x1280 + e));
    u8 o = dp8(0x6D);
    g_cpu.X = o;
    abs16_set(0x0D80 + o, 0x3800);
    dp16_set(0xD0, 0x3800);
    abs8_set(0x0F01 + o, 0);
    dp8_set(0xD5, abs8(0x1300 + o));
    dp16_set(0xD3, abs16(0x1380 + o));
    dc_set_mx(1, 1);
    abs8_set(0x2183, 1);
    dc_set_mx(0, 0);
    abs16_set(0x2181, dp16(0xD0));
    dp16_set(0xC9, 0x60);
    g_cpu.Y = 0;
    for (;;) {
        u32 p = (u32)dp16(0xD3) | (u32)dp8(0xD5) << 16;
        u16 w = bus16((p + g_cpu.Y) & 0xFFFFFF);
        g_cpu.A = w;
        g_cpu._flag_Z = !(w & 0x4000);
        if (w & 0x4000) dc_jsr(0xC0E534, 0xC0E18D);
        else dc_jsr(0xC0E687, 0xC0E182);
        g_cpu.Y += 2;
        u16 n = (u16)(dp16(0xC9) - 1);
            dp16_set(0xC9, n);
        if (!n) break;
        g_cpu._flag_C = 0;
        dp16_set(0xD0, dc_adc16(dp16(0xD0), 0x0020));
    }
    dc_set_mx(1, 0);
    abs8_set(0x2115, 0x80);
    abs8_set(0x4371, 0x18);
    abs8_set(0x4370, 0x01);
    abs8_set(0x4374, 0x7F);
    abs16_set(0x2116, 0x0400);
    abs16_set(0x4372, 0x3800);
    abs16_set(0x4375, 0x0C00);
    abs8_set(0x420B, 0x80);
    u16 x = abs16(0x1700 + dp16(0x6D));
    g_cpu.A = x;
    g_cpu.X = x;
    u32 p = (u32)dp16(0xD3) | (u32)dp8(0xD5) << 16;
    u16 y = 0xC0;
    for (int k = 0; k < 24; k++) {
        u32 s = 0x7F4BC2 + x + 8 * k;
        u8 dx = bus8((p + y++) & 0xFFFFFF);
        bus8_set(s, dx);
        bus8_set(s + 1, dx & 0x80 ? 0xFF : 0x00);
        bus8_set(s + 2, bus8((p + y) & 0xFFFFFF));
        if (k < 23) y++;
    }
    g_cpu.Y = y;
    for (int k = 0; k < 24; k++)
        bus8_set(0x7F4BC6 + x + 8 * k, (u8)(0x40 + (k / 8) * 0x20 + (k % 8) * 2));
    for (int k = 0; k < 24; k++)
        bus8_set(0x7F4BC7 + x + 8 * k, 0x22);
    dc_set_a8(0x22);
    dc_set_flags_nz8(0x22);
}

/* C0:75E9  Redraw all of layer 1's tile map: 20 rows of 10 tiles from
 * (dp 0A, dp 0E) through C0:7612 into the buffer at 7E:C800. */
DC_ENTRY(C075E9, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0xC800;
    abs16_set(0x2181, 0xC800);
    dc_set_mx(1, g_cpu.x_flag);
    dc_set_a8(0);
    abs8_set(0x2183, 0);
    dp8_set(0x5A, dp8(0x0E));
    dp8_set(0x5E, 0x20);
    u8 n;
    do {
        dp8_set(0x58, dp8(0x0A));
        dp8_set(0x5C, 0x10);
        dc_jsr(0xC07612, 0xC07608);
        dp8_set(0x5A, (u8)(dp8(0x5A) + 1));
        n = (u8)(dp8(0x5E) - 1);
        dp8_set(0x5E, n);
    } while (n);
    dc_set_flags_nz8(0);
}

/* C0:78EC helper: layer 2 rows y.. while y <= last (C0:79CF, 10 tiles
 * from column x each). */
static void l2_rows(u8 x, u8 y, u8 last, u16 site)
{
    dp8_set(0x58, x);
    for (;;) {
        dp8_set(0x5A, y);
        dp8_set(0x5C, 0x10);
        dc_jsr(0xC079CF, 0xC00000 | site);
        dp8_set(0x58, x);
        y = (u8)(dp8(0x5A) + 1);
        dc_set_a8(y);
        dc_cmp8(y, last);
        if (y > last) return;
    }
}
/* C0:78EC  Redraw layer 2's tile map into the buffer at 7E:C800: normally
 * 20 rows of 10 tiles from (dp 0A, dp 0E); with 0BC9 bit 7 rows dp 16..18
 * from column dp 12; with 1D35 bit 1 (a 64x64 layer) the whole map in
 * quarters: rows 0..1F of columns 0, then of columns 20 when 0BCF != 10,
 * else (0BD1 != 10) rows 20..3F of columns 0 and dp 85 = 2. */
DC_ENTRY(C078EC, RTS)
{
    dc_set_mx(0, g_cpu.x_flag);
    g_cpu.A = 0xC800;
    abs16_set(0x2181, 0xC800);
    dc_set_mx(1, g_cpu.x_flag);
    dc_set_a8(0);
    abs8_set(0x2183, 0);
    if (!(abs8(0x1D35) & 2)) {
        if (abs8(0x0BC9) & 0x80) { l2_rows(dp8(0x12), dp8(0x16), dp8(0x18), 0x793D); return; }
        dp8_set(0x5A, dp8(0x0E));
        dp8_set(0x5E, 0x20);
        u8 n;
        do {
            dp8_set(0x58, dp8(0x0A));
            dp8_set(0x5C, 0x10);
            dc_jsr(0xC079CF, 0xC07925);
            dp8_set(0x5A, (u8)(dp8(0x5A) + 1));
            n = (u8)(dp8(0x5E) - 1);
            dp8_set(0x5E, n);
        } while (n);
        dc_set_flags_nz8(0);
        return;
    }
    l2_rows(0x00, 0x00, 0x1F, 0x795A);
    u8 a = abs8(0x0BCF);
    dc_set_a8(a);
    dc_cmp8(a, 0x10);
    if (a != 0x10) { l2_rows(0x20, 0x00, 0x1F, 0x799D); return; }
    a = abs8(0x0BD1);
    dc_set_a8(a);
    dc_cmp8(a, 0x10);
    if (a == 0x10) { l2_rows(0x00, 0x00, 0x1F, 0x7982); return; }
    l2_rows(0x00, 0x20, 0x3F, 0x79BA);
    dc_set_a8(2);
    dc_set_flags_nz8(2);
    dp8_set(0x85, 2);
}

/* C0:93E1  Per-frame scroll update of the three layers: clear the job
 * flags (dp 74..77), step the scroll counters (C0:944B / 947D / 94DF /
 * 94AD, then the BG2 counters C0:950F.. or, with 0BC9 bit 7, C0:95E3..,
 * then the BG3 ones), clear the upload flags dp 78..7B, run the queued
 * split-row and row/column jobs and recompute the layers' buffer
 * addresses (C0:7F9A, entered by a tail branch). */
DC_ENTRY(C093E1, RTS)
{
    static const u16 bg2a[4] = { 0x950F, 0x9545, 0x95AF, 0x9579 };
    static const u16 bg2b[4] = { 0x95E3, 0x9619, 0x9683, 0x964D };
    static const u16 jobs_a[4] = { 0x98C1, 0x985F, 0x97FD, 0x979B };
    static const u16 jobs_b[4] = { 0x98F2, 0x9890, 0x982E, 0x97CC };
    for (u8 o = 0x74; o <= 0x77; o++) dp8_set(o, 0);
    dc_jsr(0xC0944B, 0xC093E9);
    dc_jsr(0xC0947D, 0xC093EC);
    dc_jsr(0xC094DF, 0xC093EF);
    dc_jsr(0xC094AD, 0xC093F2);
    bool alt = abs8(0x0BC9) & 0x80;
    for (int k = 0; k < 4; k++)
        dc_jsr(0xC00000 | (alt ? bg2b : bg2a)[k], (u32)((alt ? 0xC09408 : 0xC093FA) + 3 * k));
    dc_jsr(0xC096B7, 0xC09414);
    dc_jsr(0xC096ED, 0xC09417);
    dc_jsr(0xC0975F, 0xC0941A);
    dc_jsr(0xC09721, 0xC0941D);
    for (u8 o = 0x78; o <= 0x7B; o++) dp8_set(o, 0);
    alt = abs8(0x0BC9) & 0x80;
    for (int k = 0; k < 4; k++)
        dc_jsr(0xC00000 | (alt ? jobs_b : jobs_a)[k], (u32)((alt ? 0xC0943C : 0xC0942D) + 3 * k));
    layer_addrs();
}

/* C0:885A  Camera pan driven by 0138 (1: start, 2: keep going) with the
 * scroll state at 1D00 as direct page: the camera heads toward the
 * leader (object 0197) on each axis enabled by 1D93 (x) / 1D96 (y) at
 * speed 10 or F0, through C0:91AC and C0:93E1. Ends (0138 = 0) when
 * neither axis moves. */
DC_ENTRY(C0885A, RTS)
{
    dc_push16(g_cpu.D);
    g_cpu.A = 0x1D00;
    g_cpu.D = 0x1D00;
    u8 s = abs8(0x0138);
    dc_set_a8(s);
    bool x_on = dp8(0x93), y_on = dp8(0x96);
    if (s == 1) {
        dp8_set(0x2E, 0);
        dp8_set(0x30, 0);
        dp8_set(0x32, 0);
        dp8_set(0x33, 0);
        dc_set_mx(1, 1);
        u16 x = abs8(0x0197);
        g_cpu.X = x;
        if (x_on) {
            dc_cmp8(abs8(0x0A00 + x), 0x80);
            dp8_set(0x2E, g_cpu._flag_C ? 0x10 : 0xF0);
        }
        if (y_on) {
            dc_cmp8(abs8(0x0A80 + x), 0x88);
            dp8_set(0x30, g_cpu._flag_C ? 0x10 : 0xF0);
        }
        dc_set_mx(1, 0);
        if (x_on || y_on) {
            dc_jsr(0xC091AC, 0xC088B9);
            dc_jsr(0xC093E1, 0xC088BC);
            abs8_set(0x0138, (u8)(abs8(0x0138) + 1));
        } else abs8_set(0x0138, 0);
    } else if (s == 2) {
        dp8_set(0x32, 0);
        dp8_set(0x33, 0);
        if (!x_on) dp8_set(0x2E, 0);
        if (!y_on) dp8_set(0x30, 0);
        if (x_on || y_on) {
            dc_jsr(0xC091AC, 0xC088DD);
            dc_jsr(0xC093E1, 0xC088E0);
        } else abs8_set(0x0138, 0);
    } else if (s) abs8_set(0x0138, 0);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* Unpack entry i (FF = none) of a 3-byte pointer table in bank F6 to
 * bank:dst; false for none. keep_ofs: dp D9 ends up as the table offset
 * rather than the index. */
static bool unpack_entry_to(u8 i, u32 table, u16 dst, u8 bank, bool keep_ofs, u32 site)
{
    dc_set_a8(i);
    dc_cmp8(i, 0xFF);
    if (i == 0xFF) return false;
    dc_set_mx(0, 0);
    dp16_set(0xD9, i);
    g_cpu._flag_C = 0;
    u16 t = dc_adc16(i, i);
    t = dc_adc16(t, i);
    if (keep_ofs) dp16_set(0xD9, t);
    g_cpu.X = t;
    abs16_set(0x0300, bus16(table + t));
    abs16_set(0x0303, dst);
    dc_set_mx(1, 0);
    g_cpu.A = dst;
    abs8_set(0x0302, bus8(table + 2 + t));
    dc_set_a8(bank);
    abs8_set(0x0305, bank);
    dc_jsl(0xC30002, site);
    return true;
}
static bool unpack_entry(u8 i, u32 table, u16 dst, u32 site) { return unpack_entry_to(i, table, dst, 0x7E, false, site); }

/* C0:09DD / 0A14  Unpack the room's packed blocks (indices F6:0001 /
 * F6:0002 + dp FE) to 7E:B000 / 7E:C000 (0A14: unless dp BB). */
DC_ENTRY(C009DD, RTS)
{
    g_cpu.X = dp16(0xFE);
    unpack_entry(bus8(0xF60001 + g_cpu.X), 0xF62100, 0xB000, 0xC00A0F);
}

DC_ENTRY(C00A14, RTS)
{
    u8 b = dp8(0xBB);
    dc_set_a8(b);
    dc_set_flags_nz8(b);
    if (b) return;
    g_cpu.X = dp16(0xFE);
    unpack_entry(bus8(0xF60002 + g_cpu.X), 0xF621C0, 0xC000, 0xC00A4A);
}

/* C0:6D2F  Unpack entry [dp D6] of F6:2220 to 7E:3000 and send it with
 * C0:2DC8 (transfer set up by the caller). */
DC_ENTRY(C06D2F, RTS)
{
    u32 p = (u32)dp8(0xD8) << 16 | dp16(0xD6);
    if (unpack_entry(bus8(p), 0xF62220, 0x3000, 0xC06D5D)) dc_jsr(0xC02DC8, 0xC06D61);
}

/* Send 7F:5080 to VRAM 5000 (n bytes) with C0:2DC8. */
static void send_5080(u16 n, u32 site)
{
    dp16_set(DMA_VRAM_DEST, 0x5000);
    dp16_set(DMA_SRC, 0x5080);
    dp8_set(DMA_SRC_BANK, 0x7F);
    dp8_set(DMA_MODE, 0x01);
    dp16_set(DMA_SIZE, n);
    dc_set_a8(0x01);
    g_cpu.X = n;
    dc_jsr(0xC02DC8, site);
}

/* C0:6DCF  Unpack the room's entry F6:0002 + dp FE of F6:2220 to 7F:5080
 * and send 0x1000 bytes to VRAM 5000; none: dp BB = 1. */
DC_ENTRY(C06DCF, RTS)
{
    g_cpu.X = dp16(0xFE);
    if (unpack_entry_to(bus8(0xF60002 + g_cpu.X), 0xF62220, 0x5080, 0x7F, true, 0xC06E03)) send_5080(0x1000, 0xC06E1E);
    else { dc_set_a8(1); dc_set_flags_nz8(1); dp8_set(0xBB, 1); }
}

/* C0:6E27  Unpack FD:0000 to 7F:5080 and send 0x400 bytes to VRAM 5000. */
DC_ENTRY(C06E27, RTS)
{
    abs16_set(0x0300, 0x0000);
    abs16_set(0x0303, 0x5080);
    abs8_set(0x0302, 0xFD);
    abs8_set(0x0305, 0x7F);
    dc_set_a8(0x7F);
    dc_jsl(0xC30002, 0xC06E3D);
    send_5080(0x0400, 0xC06E58);
}

/* C0:56D4  Unpack the room's entry (word F6:0008 + dp FE) of FC:F9F0 to
 * 7F:2000, then C0:5CC7 and the fixed tables of C0:5D37.. copied to
 * 7F:3700..3785. */
DC_ENTRY(C056D4, RTS)
{
    dc_set_mx(0, 0);
    u16 x = dp16(0xFE);
    g_cpu.X = x;
    u16 w = bus16(0xF60008 + x);
    dp16_set(0xD9, w);
    g_cpu._flag_C = 0;
    u16 t = dc_adc16(w, w);
    t = dc_adc16(t, w);
    g_cpu.X = t;
    abs16_set(0x0300, bus16(0xFCF9F0 + t));
    abs16_set(0x0303, 0x2000);
    dc_set_mx(1, 0);
    g_cpu.A = 0x2000;
    abs8_set(0x0302, bus8(0xFCF9F2 + t));
    dc_set_a8(0x7F);
    abs8_set(0x0305, 0x7F);
    dc_jsl(0xC30002, 0xC056FF);
    dc_jsr(0xC05CC7, 0xC05703);
    static const struct { u16 src, dst, n; } k[5] = {
        { 0x5D37, 0x3700, 0x1F }, { 0x5D50, 0x3720, 0x1F }, { 0x5D50, 0x3740, 0x1F },
        { 0x5D50, 0x3760, 0x1F }, { 0x5D68, 0x3780, 0x05 },
    };
    dc_set_mx(0, 0);
    for (int i = 0; i < 5; i++) {
        g_cpu.X = k[i].src;
        g_cpu.Y = k[i].dst;
        g_cpu.A = k[i].n;
        u8 db = g_cpu.DB;
        dc_mvn(0x7F, 0xC0);
        g_cpu.DB = db;
        dc_set_flags_nz8(db);
    }
    dc_set_mx(1, 0);
}

/* Scroll speed from a nibble: bits 0-2 = n (0 = unchanged) gives
 * 1 << (n-1), bit 3 negates it. */
static void drift_speed(u8 nib, u8 o)
{
    if (!(nib & 7)) return;
    u8 a = 1;
    g_cpu.Y = nib & 7;
    while (--g_cpu.Y) a = (u8)(a << 1);
    if (nib & 8) a = (u8)-a;
    dc_set_a8(a);
    dp8_set(o, a);
}

/* C0:A33B  Load the room's layer properties: unpack entry F6:0004 + dp FE
 * of F6:1E00 to 7E:B500, then (direct page 1D00) decode it: B500/B501
 * layer sizes and flags (0BC9-0BD5), B504 main/sub screen layers
 * (0BD7/0BD8, only layers 1, 2 and sprites while 01BB), B505 (0BDF/0BE0),
 * B502/B503 automatic scroll speeds of layers 2 and 3 (dp 37/38, 39/3A;
 * dp 35 bits 1/2). Then C0:7399, A521 and A66B. */
DC_ENTRY(C0A33B, RTS)
{
    dc_set_mx(0, 0);
    u16 x = dp16(0xFE);
    g_cpu.X = x;
    u16 w = bus16(0xF60004 + x);
    dp16_set(0xD9, w);
    g_cpu._flag_C = 0;
    u16 t = dc_adc16(w, w);
    t = dc_adc16(t, w);
    g_cpu.X = t;
    abs16_set(0x0300, bus16(0xF61E00 + t));
    abs16_set(0x0303, 0xB500);
    dc_set_mx(1, 0);
    g_cpu.A = 0xB500;
    abs8_set(0x0302, bus8(0xF61E02 + t));
    dc_set_a8(0x7E);
    abs8_set(0x0305, 0x7E);
    dc_jsl(0xC30002, 0xC0A366);
    dc_push16(g_cpu.D);
    g_cpu.A = 0x1D00;
    g_cpu.D = 0x1D00;
    for (u16 a = 0x0BCC; a <= 0x0BD6; a += 2) abs8_set(a, 0);

    u8 v = ram8(0xB501);
    dp8_set(0x50, v);
    abs8_set(0x0BCA, (v & 0x80) ? 1 : 0);
    abs8_set(0x0BC9, (v & 0x40) ? 0x80 : 0);
    u8 l = (v & 0x20) ? 4 : 0;
    if (v & 0x10) l |= 2;
    dp8_set(0x36, l);
    abs8_set(0x0BD3, (u8)(((v & 3) + 1) << 4));
    g_cpu._flag_C = 0;
    abs8_set(0x0BD5, (u8)(dc_adc8(v & 0x0C, 4) << 2));
    v = ram8(0xB500);
    dp8_set(0x52, v);
    abs8_set(0x0BCB, (u8)(((v & 3) + 1) << 4));
    g_cpu._flag_C = 0;
    abs8_set(0x0BCD, (u8)(dc_adc8(v & 0x0C, 4) << 2));
    g_cpu._flag_C = 0;
    abs8_set(0x0BCF, dc_adc8(v & 0x30, 0x10));
    g_cpu._flag_C = 0;
    abs8_set(0x0BD1, dc_adc8((u8)((v & 0xC0) >> 2), 0x10));
    v = ram8(0xB505);
    abs8_set(0x0BDF, v);
    abs8_set(0x0BE0, v);

    v = ram8(0xB504);
    dp8_set(0x50, v);
    dp8_set(0x52, v & 7);
    u8 m = (u8)(((v & 8) << 1) | (v & 7));
    if (abs8(0x01BB)) m &= 0x13;
    abs8_set(0x0BD7, m);
    dp8_set(0x52, (u8)((v & 0x70) >> 4));
    m = (u8)(((v & 0x80) >> 3) | ((v & 0x70) >> 4));
    if (abs8(0x01BB)) m &= 0x13;
    abs8_set(0x0BD8, m);
    dp8_set(0x35, dp8(0x36));

    static const u8 bit[2] = { 0x02, 0x04 };
    for (int k = 0; k < 2; k++) {
        u8 s = ram8(0xB502 + k);
        dc_set_a8(s);
        if (!s) continue;
        dp8_set(0x52, s);
        dc_set_mx(1, 1);
        drift_speed(s, (u8)(0x37 + 2 * k));
        drift_speed(s >> 4, (u8)(0x38 + 2 * k));
        u8 f = (u8)(dp8(0x35) | bit[k]);
        dc_set_a8(f);
        dp8_set(0x35, f);
        dc_set_mx(1, 0);
    }
    dc_set_flags_nz8((u8)g_cpu.A);
    dc_jsr(0xC07399, 0xC0A4FD);
    dc_jsr(0xC0A521, 0xC0A500);
    dc_jsr(0xC0A66B, 0xC0A503);
    g_cpu.D = dc_pull16();
    dc_set_flags_nz16(g_cpu.D);
}

/* C0:0960  Load the room's tile graphics: its record F6:1C00 + 8 * (F6:0001
 * + dp FE) lists packed blocks of F6:2220 (FF = none): six go to VRAM 2000,
 * 2800, .. 4800 (C0:6D2F, the last 0xF80 bytes long), two more are unpacked
 * to 7F:6000 and 7F:7000. */
DC_ENTRY(C00960, RTS)
{
    u16 x = dp16(0xFE);
    g_cpu.X = x;
    u8 r = bus8(0xF60001 + x);
    dc_set_a8(r);
    dc_cmp8(r, 0xFF);
    if (r == 0xFF) return;
    abs8_set(0x4202, r);
    abs8_set(0x4203, 0x08);
    dc_set_mx(0, 0);
    g_cpu._flag_C = 0;
    u16 p = dc_adc16(abs16(0x4216), 0x1C00);
    dp16_set(0xD6, p);
    dc_set_mx(1, 0);
    dp8_set(0xD8, 0xF6);
    dp16_set(DMA_SRC, 0x3000);
    dp8_set(DMA_SRC_BANK, 0x7E);
    dp8_set(DMA_MODE, 0x01);
    dp16_set(DMA_SIZE, 0x1000);
    static const u16 site[6] = { 0x099E, 0x09A8, 0x09B2, 0x09BC, 0x09C6, 0x09D5 };
    for (int k = 0; k < 6; k++) {
        u16 vram = (u16)(0x2000 + 0x800 * k);
        dp16_set(DMA_VRAM_DEST, vram);
        g_cpu.X = vram;
        if (k == 0) { dc_set_a8(0x01); g_cpu.X = 0x1000; }
        if (k == 5) { dp16_set(DMA_SIZE, 0x0F80); g_cpu.X = 0x0F80; }
        if (k) dp8_set(0xD6, (u8)(dp8(0xD6) + 1));      /* 8-bit INC */
        dc_jsr(0xC06D2F, 0xC00000 | site[k]);
    }
    dp8_set(0xD6, (u8)(dp8(0xD6) + 1));
    for (int k = 0; k < 2; k++) {
        if (k) { u16 d = (u16)(dp16(0xD6) + 1); g_cpu.X = d; dp16_set(0xD6, d); }
        u32 q = (u32)dp8(0xD8) << 16 | dp16(0xD6);
        unpack_entry_to(bus8(q), 0xF62220, k ? 0x7000 : 0x6000, 0x7F, false, k ? 0xC06DCA : 0xC06D93);
    }
}
