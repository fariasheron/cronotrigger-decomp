/* Sound driver side on the main CPU (bank C7): handshakes with the sound
 * CPU through the APU ports 2140-2143. */
#include "ram.h"

/* Direct page indexed (dp,X): the address wraps in bank 0, not the page. */
static u8 dpx8(u8 o, u16 x) { return b0_8((u16)(g_cpu.D + o + x)); }
static void dpx8_set(u8 o, u16 x, u8 v) { b0_8_set((u16)(g_cpu.D + o + x), v); }

/* Waits until APU port 0 echoes v. The original loop (CMP abs; BNE) takes
 * about 7 cycles per pass; the time is spent so the sound CPU, which is
 * caught up to the main clock on each port access, can answer. */
static void apu_wait_echo(u8 v)
{
    while (abs8(0x2140) != v) dc_spend(56);
    dc_cmp8(v, v);
}

/* C7:09DA  Sends A on port 0 and waits for the echo; A = (A + 1) & 7F
 * (the next sequence number). P is kept. */
static void apu_cmd(void)
{
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_set_mx(1, g_cpu.x_flag);
    u8 a = (u8)g_cpu.A;
    abs8_set(0x2140, a);
    apu_wait_echo(a);
    dc_set_a8((u8)((a + 1) & 0x7F));
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}
DC_ENTRY(C709DA, RTS) { apu_cmd(); }

/* C7:09EA  Sends dp 84 on port 0, waits for the echo, then dp 84 = E0
 * (resynchronizes the sequence after the sound CPU refused a command). */
static void apu_resync(void)
{
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_set_mx(1, g_cpu.x_flag);
    u8 a = dp8(0x84);
    dc_set_a8(a);
    abs8_set(0x2140, a);
    apu_wait_echo(a);
    dc_set_a8(0xE0);
    dp8_set(0x84, 0xE0);
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}
DC_ENTRY(C709EA, RTS) { apu_resync(); }

/* Sends the 3 bytes at [12],Y on ports 1-3 and dp 84 as the sequence
 * number; Y advances 3 (with or without carrying into the bank byte 14). */
static void apu_send3(int carry_bank)
{
    for (int k = 0; k < 3; k++) {
        u8 b = bus8(((u32)dp8(0x14) << 16 | dp16(0x12)) + g_cpu.Y);
        dc_set_a8(b);
        abs8_set((u16)(0x2141 + k), b);
        g_cpu.Y++;
        if (carry_bank && !g_cpu.Y) dp8_set(0x14, (u8)(dp8(0x14) + 1));
    }
    dc_set_a8(dp8(0x84));
    apu_cmd();
    dp8_set(0x84, (u8)g_cpu.A);
}
/* C7:0734  Marks a sound command as "wait" (dp 00 |= 04): commands 14 and
 * up always, lower ones when the song id dp 01 is in the list at C7:0AD8
 * (ended by a negative byte). P is kept. */
DC_ENTRY(C70734, RTS)
{
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_set_mx(1, g_cpu.x_flag);
    u8 cmd = dp8(0x00);
    int mark = cmd >= 0x14;
    dc_set_a8(cmd);
    if (!mark) {
        for (g_cpu.X = 0;; g_cpu.X++) {
            u8 s = bus8(0xC70AD8 + g_cpu.X);
            dc_set_a8(s);
            if (s & 0x80) break;
            if (s == dp8(0x01)) { mark = 1; break; }
        }
    }
    if (mark) {
        dc_set_a8((u8)(0x04 | cmd));
        dp8_set(0x00, (u8)(0x04 | cmd));
    }
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}

/* C7:0A12  Finds the first entry of the word list at dp 63 (0 = end) whose
 * low byte is >= dp F2 and clears the bytes from there on in the three
 * 32-byte tables at dp 20, 40 and 62. P is kept. */
static void voices_trim(void)
{
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_set_mx(1, g_cpu.x_flag);
    for (g_cpu.X = 0; g_cpu.X != 0x20; g_cpu.X += 2) {
        u8 v = dpx8(0x63, g_cpu.X);
        dc_set_a8(v);
        if (!v) break;
        if (v >= dp8(0xF2)) {
            dc_set_a8(0);
            for (; g_cpu.X != 0x20; g_cpu.X++) {
                dpx8_set(0x20, g_cpu.X, 0);
                dpx8_set(0x40, g_cpu.X, 0);
                dpx8_set(0x62, g_cpu.X, 0);
            }
            break;
        }
    }
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}
DC_ENTRY(C70A12, RTS) { voices_trim(); }

/* F2 = F1 - 8 * n: the sound CPU memory left above n voice blocks. */
static void voices_limit(u8 n)
{
    dp8_set(0xF0, n);
    u8 a = (u8)(n << 3);
    g_cpu._flag_C = (n >> 5) & 1;
    a = (u8)(~a + 1);
    g_cpu._flag_C = 0;
    a = dc_adc8(a, 0xF1);
    dc_set_a8(a);
    dp8_set(0xF2, a);
}

/* C7:09FD  Voice count A: sets the limit and drops what no longer fits. */
DC_ENTRY(C709FD, RTS)
{
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_set_mx(1, g_cpu.x_flag);
    voices_limit((u8)g_cpu.A);
    voices_trim();
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}

/* C7:0922  Sends one of the two fixed blocks of 3-byte commands (C7:4309,
 * or C7:5B0B when dp 10 is set) to the sound CPU at 2F00, after the
 * "upload" command (3, or 5 for command 70) is accepted. */
DC_ENTRY(C70922, RTS)
{
    for (;;) {
        dc_set_mx(1, g_cpu.x_flag);
        abs8_set(0x2143, 0x2F);
        abs8_set(0x2142, 0x00);
        u8 cmd = dp8(0x00) == 0x70 ? 0x05 : 0x03;
        abs8_set(0x2141, cmd);
        dp8_set(0x86, cmd);
        dc_set_a8(dp8(0x84));
        apu_cmd();
        dp8_set(0x84, (u8)g_cpu.A);
        dc_cmp8(dp8(0x86), abs8(0x2141));
        if (g_cpu._flag_Z) break;
        apu_resync();
    }
    u16 at = dp8(0x10) ? 0x5B0B : 0x4309;
    dp8_set(0x0C, bus8(0xC70000 + at));
    dp8_set(0x0D, bus8(0xC70000 + at + 1));
    dp8_set(0x12, (u8)(at + 2));
    dp8_set(0x13, (u8)((at + 2) >> 8));
    dp8_set(0x14, 0xC7);
    g_cpu.X = dp16(0x0C);
    g_cpu.Y = 0;
    do {
        apu_send3(0);
        g_cpu.X -= 3;
    } while (g_cpu.X);
    dp8_set(0xF3, 0);
    dc_set_a8(0xE0);
    dc_set_flags_nz8(0xE0);
    dp8_set(0x84, 0xE0);
}

/* C7:0655  Uploads sample/sequence n = (dp 20 + voice) to the sound CPU:
 * the pointer comes from C7:0AEA[n - 1] (3 bytes), the destination from
 * dp 60 + voice; dp 40 = length, 62 = end. Then 3-byte packets with a
 * handshake each. P is kept. */
DC_ENTRY(C70655, RTS)
{
    dc_sync_p();
    u8 p = g_cpu.P;
    for (;;) {
        dc_set_mx(0, g_cpu.x_flag);
        g_cpu.X = dp16(0x1C);
        u16 n = (u16)((dpx8(0x20, g_cpu.X) | dpx8(0x21, g_cpu.X) << 8) - 1);
        dp16_set(0x82, n);
        g_cpu._flag_C = 0;
        g_cpu.X = dc_adc16((u16)(n << 1), n);
        dc_set_mx(1, g_cpu.x_flag);
        dp8_set(0x12, bus8(0xC70AEA + g_cpu.X));
        dp8_set(0x13, bus8(0xC70AEB + g_cpu.X));
        dp8_set(0x14, bus8(0xC70AEC + g_cpu.X));
        g_cpu.Y = dp16(0x12);
        dp8_set(0x12, 0);
        dp8_set(0x13, 0);
        g_cpu.X = dp16(0x1C);
        abs8_set(0x2142, dpx8(0x60, g_cpu.X));
        abs8_set(0x2143, dpx8(0x61, g_cpu.X));
        abs8_set(0x2141, 0x03);
        dc_set_a8(dp8(0x84));
        apu_cmd();
        dp8_set(0x84, (u8)g_cpu.A);
        dc_cmp8(0x03, abs8(0x2141));
        if (g_cpu._flag_Z) break;
        apu_resync();
    }
    u16 v = g_cpu.X;
    for (int k = 0; k < 2; k++) {
        u8 b = bus8(((u32)dp8(0x14) << 16) + g_cpu.Y);
        dp8_set((u8)(0x0C + k), b);
        dpx8_set(0x40, (u16)(v + k), b);
        if (!k) g_cpu._flag_C = 0;
        dpx8_set(0x62, (u16)(v + k), dc_adc8(b, dpx8(0x60, (u16)(v + k))));
        g_cpu.Y++;
        if (!g_cpu.Y) dp8_set(0x14, (u8)(dp8(0x14) + 1));
    }
    g_cpu.X = dp16(0x0C);
    g_cpu._flag_C = 0;
    u16 end = dc_adc16(g_cpu.Y, dp16(0x0C));
    g_cpu.A = end;
    int cross = g_cpu._flag_C;
    do {
        apu_send3(cross);
        g_cpu.X -= 3;
    } while (g_cpu.X);
    dc_set_a8(0xE0);
    dp8_set(0x84, 0xE0);
    g_cpu.P = p;
    cpu_p_to_mirrors(&g_cpu);
}

/* C7:0000 (C7:0038)  Boots the sound CPU: waits for its IPL ready signal
 * (BBAA), uploads the six blocks listed at C7:0010 (source) / C7:001C
 * (destination) with the IPL protocol, starts the program at 0200, clears
 * the driver variables at 1E00 and sets the first ones. */
DC_ENTRY(C70000, RTL)
{
    dc_push8(g_cpu.DB);
    dc_push16(g_cpu.D);
    dc_sync_p();
    u8 p = g_cpu.P;
    dc_push8(p);
    dc_set_mx(0, 0);
    u16 a0 = g_cpu.A, x0 = g_cpu.X, y0 = g_cpu.Y;
    dc_push16(a0); dc_push16(x0); dc_push16(y0);
    dc_set_mx(1, 0);
    g_cpu.DB = 0;
    g_cpu.D = 0x1E00;
    g_cpu.X = 0xBBAA;
    do {
        abs8_set(0x2141, 0xF0);
        abs8_set(0x2140, 0xFF);
        dc_spend(80);
    } while (abs16(0x2140) != 0xBBAA);
    g_cpu.X = 0;
    abs8_set(0x2142, bus8(0xC7001C));
    abs8_set(0x2143, bus8(0xC7001D));
    abs8_set(0x2141, 0xCC);
    abs8_set(0x2140, 0xCC);
    apu_wait_echo(0xCC);
    u8 data = 0, seq;
    for (;;) {
        seq = 0;
        dp8_set(0x12, bus8(0xC70010 + g_cpu.X));
        dp8_set(0x13, bus8(0xC70011 + g_cpu.X));
        dp8_set(0x14, 0xC7);
        u32 src = 0xC70000 | dp16(0x12);
        g_cpu._flag_C = 0;
        dp8_set(0x0C, dc_adc8(bus8(src), 0x02));
        dp8_set(0x0D, dc_adc8(bus8(src + 1), 0x00));
        for (g_cpu.Y = 2; g_cpu.Y != dp16(0x0C); g_cpu.Y++) {
            data = bus8(src + g_cpu.Y);
            abs8_set(0x2141, data);
            abs8_set(0x2140, seq);
            apu_wait_echo(seq);
            seq++;
        }
        seq += 3;                                  /* next block: skip ahead, never 0 */
        if (!seq) seq = 1;
        g_cpu.X += 2;
        if (g_cpu.X == 0x000C) break;
        abs8_set(0x2142, bus8(0xC7001C + g_cpu.X));
        abs8_set(0x2143, bus8(0xC7001D + g_cpu.X));
        abs8_set(0x2141, seq);
        abs8_set(0x2140, seq);
        apu_wait_echo(seq);
    }
    abs8_set(0x2143, 0x02);                        /* run from 0200 */
    abs8_set(0x2142, 0x00);
    abs8_set(0x2141, 0x00);
    abs8_set(0x2140, seq);
    apu_wait_echo(seq);
    abs8_set(0x2140, 0x00);
    for (g_cpu.X = 0x0100; g_cpu.X; g_cpu.X--) abs8_set(0x1DFF + g_cpu.X, 0x00);
    dc_set_mx(0, 0);
    u16 v = (u16)(bus16(0xC740B4) + 0x4700);
    dp16_set(0xF8, v);
    dp16_set(0x60, v);
    dp16_set(0xF6, v);
    dc_spend(0x800 * 40);                         /* LDX #$0800; DEX; BNE */
    g_cpu.X = 0;
    dc_set_mx(1, 0);
    dp8_set(0xFA, 0x00);
    dp8_set(0xFB, 0xC7);
    voices_limit(0x05);
    dp8_set(0xF1, 0);
    dp8_set(0x05, 0xE0);
    /* C7:0192 */
    dp8_set(0x00, 0);
    (void)data;
    dc_set_mx(0, 0);
    g_cpu.S = (u16)(g_cpu.S + 6);
    g_cpu.A = a0; g_cpu.X = x0; g_cpu.Y = y0;
    g_cpu.P = dc_pull8();
    cpu_p_to_mirrors(&g_cpu);
    g_cpu.D = dc_pull16();
    g_cpu.DB = dc_pull8();
    dc_set_flags_nz8(g_cpu.DB);
}

/* hle_func for C7:0004 (recomp/bankc7.cfg): the original, run to its RTL in a
 * nested interpreter frame. Its body waits on the sound CPU through C7:0A39,
 * and a compiled C7:0004 unwinds to the interpreted scheduler at entry for that
 * (LLE yield), which no C caller can resume; interpreted here it returns like
 * any call, as it did when C7:0004 had no compiled body. The HLE declaration
 * gives the analysis its exit (the entry width: PHP ... PLP), so its callers
 * stay compiled. Entered with the caller's JSL frame on the stack. */
RecompReturn ct_hle_c70004(CpuState *cpu)
{
    extern int snes_frame_counter;
    int frame = snes_frame_counter;
    RecompReturn r = interp_tier_run_call_frame(cpu, 0xC70004, 0xC70004, 3, NULL);
    if (snes_frame_counter != frame) dc_frames_passed();   /* a music upload spans frames */
    return r;
}
