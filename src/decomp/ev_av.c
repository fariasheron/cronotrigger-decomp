/* Field event script: sound and screen commands (E8 EA-EE F0-F2). Sound
 * goes through the driver call C7:0004 with a command block at $1E00
 * (command, then up to three parameters). */
#include "ram.h"

#define SND_CMD         0x1E00
#define SND_ARG(i)      (0x1E01 + (i))
#define SND_MUSIC       0x29AE   /* 7E:29AE: map music to keep */
#define FIELD_SIGNALS   0x0118   /* bit 1 fade done ... */
#define FADE_TARGET     0x011B
#define FADE_SPEED_A    0x011C
#define FADE_SPEED_B    0x011D
#define FADE_STEP       0x0119
#define TINT_REG        0x0121   /* colour math bits; high 3 = colour channels */
#define TINT_TARGET     0x0122
#define TINT_STEP_A     0x0123
#define TINT_STEP_B     0x0124
#define TINT_MODE       0x0BE0
#define TINT_MODE_SAVED 0x0BDF

/* The driver call: C7:0004 always finishes in the interpreter under the
 * scheduler, so a 1 here means the original takes over after the call
 * (its code there only reloads X from $C7). */
static int sound(u32 site) { return dc_jsl_y(0xC70004, site); }

/* E8 s  Sound effect s (command 18, parameter 2 = 80). */
DC_ENTRY(C03C11, RTS)
{
    DC_ASSUME_D(C03C11, 0x0100);
    g_cpu.Y++;
    g_cpu.X = g_cpu.Y;
    ram16_set(EV_TMP_C7, g_cpu.Y);
    ram8_set(SND_ARG(0), ev_byte(g_cpu.Y));
    ram8_set(SND_ARG(1), 0x80);
    ram8_set(SND_CMD, 0x18);
    dc_set_a8(0x18);
    if (sound(0xC03C26)) return;
    g_cpu.X = (u16)(ram16(EV_TMP_C7) + 1);
    g_cpu._flag_C = 0;
}
/* EA m  Play music m (command 10) and make it the map's music. */
DC_ENTRY(C03C2F, RTS)
{
    DC_ASSUME_D(C03C2F, 0x0100);
    g_cpu.Y++;
    g_cpu.X = g_cpu.Y;
    u8 m = ev_byte(g_cpu.Y);
    ram8_set(SND_MUSIC, m);
    ram8_set(SND_ARG(0), m);
    ram8_set(SND_CMD, 0x10);
    dc_set_a8(0x10);
    ram16_set(EV_TMP_C7, g_cpu.X);
    if (sound(0xC03C43)) return;
    g_cpu.X = (u16)(ram16(EV_TMP_C7) + 1);
    g_cpu._flag_C = 0;
}
/* EB a b  Sound command 81 (a, b, FF), then 82 (0, FF) and 83 (0, FF). */
DC_ENTRY(C03C4C, RTS)
{
    DC_ASSUME_D(C03C4C, 0x0100);
    g_cpu.Y++;
    u16 p = g_cpu.Y;
    ram8_set(SND_ARG(0), ev_byte(p));
    ram8_set(SND_ARG(1), ev_byte((u16)(p + 1)));
    ram8_set(SND_ARG(2), 0xFF);
    ram8_set(SND_CMD, 0x81);
    g_cpu.X = (u16)(p + 1);
    ram16_set(EV_TMP_C7, g_cpu.X);
    dc_set_a8(0x81);
    if (sound(0xC03C69)) return;
    static const u8 cmd[2] = { 0x82, 0x83 };
    static const u32 site[2] = { 0xC03C7C, 0xC03C8F };
    for (int i = 0; i < 2; i++) {
        ram8_set(SND_ARG(0), 0);
        ram8_set(SND_ARG(1), 0xFF);
        ram8_set(SND_CMD, cmd[i]);
        dc_set_a8(cmd[i]);
        if (sound(site[i])) return;
    }
    g_cpu.X = (u16)(ram16(EV_TMP_C7) + 1);
    g_cpu._flag_C = 0;
}
/* EC c a b  Any sound command c with parameters a, b. */
DC_ENTRY(C03C98, RTS)
{
    DC_ASSUME_D(C03C98, 0x0100);
    g_cpu.Y++;
    u16 p = g_cpu.Y;
    ram8_set(SND_CMD, ev_byte(p));
    ram8_set(SND_ARG(0), ev_byte((u16)(p + 1)));
    u8 b = ev_byte((u16)(p + 2));
    ram8_set(SND_ARG(1), b);
    dc_set_a8(b);
    g_cpu.X = (u16)(p + 3);
    ram16_set(EV_TMP_C7, g_cpu.X);
    if (sound(0xC03CB4)) return;
    g_cpu.X = ram16(EV_TMP_C7);
    g_cpu._flag_C = 0;
}
/* ED / EE  Wait until the sound CPU reports idle (port 1 = 0 / port 3 low
 * nibble = 0). */
DC_ENTRY(C03CBC, RTS) { u8 v = io8(0x2141); dc_set_a8(v); g_cpu.X = (u16)(g_cpu.Y + (v ? 0 : 1)); g_cpu._flag_C = 0; }
DC_ENTRY(C03CC5, RTS) { u8 v = io8(0x2143) & 0x0F; dc_set_a8(v); g_cpu.X = (u16)(g_cpu.Y + (v ? 0 : 1)); g_cpu._flag_C = 0; }

/* F0 b  Fade: low nibble != 0 -> fade to brightness (b >> 4) at that speed
 * (signals $0118 bit 1 when done); low nibble 0 -> set brightness step b >> 4. */
DC_ENTRY(C03CD0, RTS)
{
    DC_ASSUME_D(C03CD0, 0x0100);
    u16 pos = g_cpu.Y;
    u8 b = ev_byte((u16)(pos + 1));
    ram8_set(EV_TMP_D9, b);
    g_cpu.X = (u16)(pos + 2);
    g_cpu._flag_C = 0;
    if (b & 0x0F) {
        ram8_set(FADE_SPEED_A, b & 0x0F);
        ram8_set(FADE_SPEED_B, b & 0x0F);
        ram8_set(FADE_TARGET, b >> 4);
        ram8_set(FIELD_SIGNALS, ram8(FIELD_SIGNALS) | 0x02);
        dc_set_a8(0x02);
    } else {
        ram8_set(FADE_STEP, b >> 4);
        dc_set_a8(b >> 4);
    }
}
/* F2  Wait for the fade ($0118 bit 1). */
DC_ENTRY(C03CFD, RTS)
{
    DC_ASSUME_D(C03CFD, 0x0100);
    u8 v = ram8(FIELD_SIGNALS);
    dc_set_a8(v);
    g_cpu.X = (u16)(g_cpu.Y + ((v & 0x02) ? 0 : 1));
    g_cpu._flag_C = 0;
}
/* F1 c s  Screen tint: c = colour channels (bits 5-7) and intensity;
 * s = steps (bit 7: other mode). c = 0 restores; s = 0 sets it at once. */
DC_ENTRY(C03D07, RTS)
{
    DC_ASSUME_D(C03D07, 0x0100);
    u16 pos = g_cpu.Y;
    u8 c = ev_byte((u16)(pos + 1));
    if (!c) {
        ram8_set(TINT_TARGET, 0xE0);
        ram8_set(TINT_REG, 0xE0);
        u8 m = ram8(TINT_MODE_SAVED);
        ram8_set(TINT_MODE, m);
        dc_set_a8(m);
        g_cpu.X = (u16)(pos + 2);
        g_cpu._flag_C = 1;
        return;
    }
    ram8_set(TINT_TARGET, c);
    ram8_set(EV_TMP_D9, c & 0xE0);
    ram8_set(TINT_REG, (u8)((ram8(TINT_REG) & 0x1F) | (c & 0xE0)));
    u8 s = ev_byte((u16)(pos + 2));
    u8 mode = (s & 0x80) ? 0x97 : 0x17;
    u8 n = s & 0x7F;
    g_cpu.X = (u16)(pos + 3);
    if (!n) {
        ram8_set(TINT_REG, c);
        ram8_set(TINT_MODE, mode);
        dc_set_a8(mode);
        g_cpu._flag_C = 1;
        return;
    }
    ram8_set(TINT_STEP_A, n);
    ram8_set(TINT_STEP_B, n);
    ram8_set(TINT_MODE, mode);
    u8 t = ram8(TINT_TARGET) & 0xE0;
    ram8_set(EV_TMP_D9, t);
    ram8_set(TINT_REG, (u8)((ram8(TINT_REG) & 0x1F) | t));
    ram8_set(FIELD_SIGNALS, ram8(FIELD_SIGNALS) | 0x18);
    dc_set_a8(0x18);
    g_cpu._flag_C = 0;
}

/* F3  Wait for the tint ($0118 bit 3). */
DC_ENTRY(C03D7A, RTS)
{
    DC_ASSUME_D(C03D7A, 0x0100);
    u8 v = ram8(FIELD_SIGNALS);
    dc_set_a8(v);
    g_cpu.X = (u16)(g_cpu.Y + ((v & 0x08) ? 0 : 1));
    g_cpu._flag_C = 0;
}
/* F4 b  $01BC = b; 0 also clears $1D4E/$1D4F (screen shake?). */
DC_ENTRY(C03D84, RTS)
{
    DC_ASSUME_D(C03D84, 0x0100);
    u16 pos = g_cpu.Y;
    u8 b = ev_byte((u16)(pos + 1));
    dc_set_a8(b);
    ram8_set(0x01BC, b);
    if (!b) { ram8_set(0x1D4E, 0); ram8_set(0x1D4F, 0); }
    g_cpu.X = (u16)(pos + 2);
    g_cpu._flag_C = 1;
}
/* E4 a b c d  Copy a map tile block now (C0:AF4E) with parameters
 * $013E/$0140/$0142 (16-bit) and $0144;  E5: the same queued for the
 * field loop ($0117 bit 5, $0145 = d & 7). */
static void tile_block_args(u16 pos)
{
    ram16_set(0x013E, ev_word((u16)(pos + 1)));
    ram16_set(0x0140, ev_word((u16)(pos + 3)));
    ram16_set(0x0142, ev_word((u16)(pos + 5)));
    u8 d = ev_byte((u16)(pos + 7));
    ram8_set(0x0144, d);
    dc_set_a8(d);
    g_cpu.X = (u16)(pos + 7);
}
DC_ENTRY(C03D97, RTS)
{
    DC_ASSUME_D(C03D97, 0x0100);
    tile_block_args(g_cpu.Y);
    ram16_set(EV_TMP_C7, g_cpu.X);
    dc_jsr(0xC0AF4E, 0xC03DBD);
    g_cpu.X = (u16)(ram16(EV_TMP_C7) + 1);
    g_cpu._flag_C = 0;
}
DC_ENTRY(C03DC5, RTS)
{
    DC_ASSUME_D(C03DC5, 0x0100);
    tile_block_args(g_cpu.Y);
    u8 d = ram8(0x0144) & 7;
    ram8_set(0x0145, d);
    ram8_set(0x0117, ram8(0x0117) | 0x20);
    dc_set_a8(0x20);
    g_cpu.X++;
    g_cpu._flag_C = 0;
}
/* E6 w m l  Layer scroll words: $1D4D = l; w goes to $1D24 / $1D26 / $1D28
 * for mask bits 0 / 1 / 2 of m. */
DC_ENTRY(C03DF4, RTS)
{
    DC_ASSUME_D(C03DF4, 0x0100);
    u16 pos = g_cpu.Y;
    u16 w = ev_word((u16)(pos + 1));
    ram16_set(EV_TMP_OBJ, w);
    u8 m = ev_byte((u16)(pos + 3));
    ram8_set(EV_TMP_D9, m);
    ram8_set(0x1D4D, ev_byte((u16)(pos + 4)));
    dc_set_a8(m);
    for (int i = 0; i < 3; i++)
        if (m & (1 << i)) { g_cpu.Y = w; ram16_set(0x1D24 + i * 2, w); }
    g_cpu.X = (u16)(pos + 5);
    g_cpu._flag_C = 1;
}
/* E7 x y  Scroll the screen to tile (x, y) and wait ($1D00: 0 idle,
 * 1 scrolling, 2 done). */
DC_ENTRY(C03E30, RTS)
{
    DC_ASSUME_D(C03E30, 0x0100);
    u16 pos = g_cpu.Y;
    u8 s = ram8(0x1D00);
    g_cpu.X = pos;
    g_cpu._flag_C = 0;
    if (s == 0) {
        ram8_set(0x1D02, (u8)(ev_byte((u16)(pos + 1)) << 1));
        u8 y2 = (u8)(ev_byte((u16)(pos + 2)) << 1);
        ram8_set(0x1D03, y2);
        dc_set_a8(y2);
        ram8_set(0x1D00, 1);
        ram8_set(0x011F, 0);
    } else if (s == 1) {
        dc_set_a8(0);
    } else {
        dc_set_a8((u8)(s - 1));
        ram8_set(0x1D00, 0);
        g_cpu.X = (u16)(pos + 3);
        g_cpu._flag_C = 1;
    }
}
/* F8 / F9 / FA  Restore HP and MP / HP / MP (C2:8004 ops 6 and 7); carry
 * as the service returns it. */
static void restore(bool hp, bool mp)
{
    g_cpu.Y++;
    ram16_set(EV_TMP_C7, g_cpu.Y);
    if (hp) { g_cpu.A = (u16)((g_cpu.A & 0xFF00) | 6); dc_jsl(0xC28004, mp ? 0xC03E61 : 0xC03E67); }
    if (mp) { g_cpu.A = (u16)((g_cpu.A & 0xFF00) | 7); dc_jsl(0xC28004, 0xC03E67); }
    g_cpu.X = ram16(EV_TMP_C7);
}
DC_ENTRY(C03E5C, RTS) { DC_ASSUME_D(C03E5C, 0x0100); restore(true, true); }
DC_ENTRY(C03E6E, RTS) { DC_ASSUME_D(C03E6E, 0x0100); restore(true, false); }
DC_ENTRY(C03E75, RTS) { DC_ASSUME_D(C03E75, 0x0100); restore(false, true); }
/* FE n (a0 b0)*8  Prepare a fade of 8 colour components over n steps: for
 * each pair stores the start a<<8 at 7F:1CE9+2i and the per-step delta
 * ((b - a) << 8) / n (hardware divider, sign handled by hand) at 7F:1CF9+2i;
 * $0139 = 6 starts it. */
DC_ENTRY(C03F26, RTS)
{
    DC_ASSUME_D(C03F26, 0x0100);
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 n = ev_byte(pos++);
    ram8_set(0x01F4, n);
    ram8_set(0x7F1CE8, n);
    ram8_set(0x01F0, 0);
    ram16_set(0x01F6, 0);
    u16 a = 0, from = 0;
    for (u8 i = 8; i; i--) {
        ram8_set(0x01EE, i);
        u8 lo = ev_byte(pos), hi = ev_byte((u16)(pos + 1));
        pos += 2;
        ram8_set(0x01F1, lo);
        from = (u16)(lo << 8);
        u16 to = (u16)(hi << 8);
        a = (u16)(to - from);
        g_cpu._flag_V = (((to ^ from) & (to ^ a)) >> 15) & 1;
        if (a) {
            bool neg = to < from;
            io16_set(0x4204, neg ? (u16)-a : a);
            io8_set(0x4206, n);
            u16 q = cpu_read16(&g_cpu, 0, 0x4214);
            a = neg ? (u16)-q : q;
        }
        u16 x = ram16(0x01F6);
        ram16_set(0x7F1CF9 + x, a);
        ram16_set(0x7F1CE9 + x, from);
        ram16_set(0x01F6, (u16)(x + 2));
    }
    ram8_set(0x01EE, 0);
    g_cpu.A = (u16)((from & 0xFF00) | 6);
    ram8_set(0x0139, 6);
    g_cpu.X = pos;
    ram16_set(EV_TMP_C7, pos);
    dc_set_flags_nz8(6);
    g_cpu._flag_C = 0;
}
/* 01  Not a valid command: fatal error screen. */
DC_ENTRY(C05F6E, RTS)
{
    DC_ASSUME_D(C05F6E, 0x0100);
    fatal_error(0x1639, 0xC05F71);
}

/* ---- FF n >= 90: screen effects, dispatched through C0:400E ----------- */
/* Effect state: dp 39 = running effect (0 none), dp 3A/3B/3D = its
 * parameters; 0350-0354 = palette fade request (mode, step, source 7F:17C8),
 * 013C = fade kind. */
#define FX_KIND 0x39

/* 90 / 94: start effect 1 / 4 with three parameter bytes. */
static void fx_start3(u8 kind)
{
    u16 x = (u16)(g_cpu.Y + 2);
    dp8_set(0x3A, ev_byte(x));
    dp8_set(0x3B, ev_byte((u16)(x + 1)));
    dp8_set(0x3D, ev_byte((u16)(x + 2)));
    dp8_set(0x3C, 0);
    dp8_set(FX_KIND, kind);
    dc_set_a8(kind);
    dc_set_flags_nz8(kind);
    g_cpu.X = (u16)(x + 3);
}
DC_ENTRY(C04036, RTS) { fx_start3(1); }
DC_ENTRY(C04191, RTS) { fx_start3(4); }

/* 91 98 99 9A 9B 9C: start an effect without parameters. */
static void fx_set(u8 kind)
{
    dp8_set(FX_KIND, kind);
    dc_set_a8(kind);
    dc_set_flags_nz8(kind);
    g_cpu.X = (u16)(g_cpu.Y + 2);
}
DC_ENTRY(C04055, RTS) { fx_set(0x02); }
DC_ENTRY(C041B0, RTS) { fx_set(0x05); }
DC_ENTRY(C041B4, RTS) { fx_set(0x07); }
DC_ENTRY(C041B8, RTS) { fx_set(0x09); }
DC_ENTRY(C041C0, RTS) { fx_set(0x0B); }
DC_ENTRY(C041C4, RTS) { fx_set(0x0E); }

/* 92 93 95 96 (405D 40B3 40EF 4145): palette fade (mode 1 or 2, kind
 * 013C), waiting until it ends; then either clear all screen effects or
 * just dp 19. X = Y while waiting, Y + 2 when done. */
static void fx_fade(u8 mode, u8 kind, bool clear_all)
{
    u16 y = g_cpu.Y;
    g_cpu.X = y;
    u8 cur = dp8(FX_KIND);
    dc_set_a8(cur);
    if (!cur) {
        u8 busy = abs8(0x0350);
        dc_set_a8(busy);
        dc_set_flags_nz8(busy);
        if (busy) return;
        abs8_set(0x0351, 0);
        abs8_set(0x0350, mode);
        abs16_set(0x0352, 0x17C8);
        abs8_set(0x0354, 0x7F);
        dp8_set(FX_KIND, 3);
        abs8_set(0x013C, kind);
        g_cpu.X = 0x17C8;
        dc_set_a8(kind);
        dc_set_flags_nz8(kind);
        g_cpu.X = y;
        return;
    }
    dc_cmp8(cur, 3);
    if (cur != 3) return;
    u8 busy = abs8(0x0350);
    dc_set_a8(busy);
    dc_set_flags_nz8(busy);
    if (busy) return;
    dp8_set(FX_KIND, 0);
    if (clear_all) {
        ram8_set(0x11522, 0);
        ram8_set(0x11523, 0);
        abs8_set(0x0BD9, 0); abs8_set(0x0BDA, 0); abs8_set(0x0BDB, 0); abs8_set(0x0BDD, 0);
        u8 v = dp8(0x28);
        g_cpu._flag_Z = (v & 0x80) == 0;
        dp8_set(0x28, v & 0x7F);
        dc_set_a8(0x80);
    } else {
        dp8_set(0x19, 0);
    }
    g_cpu.X = (u16)(y + 2);
}
DC_ENTRY(C0405D, RTS) { fx_fade(1, 0, true); }
DC_ENTRY(C040B3, RTS) { fx_fade(1, 3, false); }
DC_ENTRY(C040EF, RTS) { fx_fade(2, 1, true); }
DC_ENTRY(C04145, RTS) { fx_fade(2, 2, false); }

/* 9E: start effect 0C and wait until it has become 0D. */
DC_ENTRY(C041C8, RTS)
{
    u16 y = g_cpu.Y;
    g_cpu.X = y;
    u8 cur = dp8(FX_KIND);
    dc_set_a8(cur);
    if (!cur) { dp8_set(FX_KIND, 0x0C); dc_set_a8(0x0C); dc_set_flags_nz8(0x0C); return; }
    dc_cmp8(cur, 0x0D);
    if (cur == 0x0D) g_cpu.X = (u16)(y + 2);
}
/* 9D: clear bit 1 of dp 54. */
DC_ENTRY(C041DC, RTS)
{
    u8 v = dp8(0x54);
    g_cpu._flag_Z = (v & 0x02) == 0;
    dp8_set(0x54, v & ~0x02);
    dc_set_a8(0x02);
    g_cpu.X = (u16)(g_cpu.Y + 2);
}
/* 9F: party/menu service 8 (C2:8004). */
DC_ENTRY(C03E7C, RTS)
{
    g_cpu.Y = (u16)(g_cpu.Y + 2);
    dp16_set(0xC7, g_cpu.Y);
    g_cpu.A = (u16)((g_cpu.A & 0xFF00) | 8);
    dc_jsl(0xC28004, 0xC03E67);
    g_cpu.X = dp16(0xC7);
}

/* C0:67E3  Read three operand bytes after the command into dp D0-D2;
 * X = position after them. */
DC_ENTRY(C067E3, RTS)
{
    u16 x = (u16)(g_cpu.Y + 1);
    for (u8 i = 0; i < 3; i++) dp8_set((u8)(0xD0 + i), ev_byte((u16)(x + i)));
    u8 last = ev_byte((u16)(x + 2));
    dc_set_a8(last);
    g_cpu.X = (u16)(x + 3);
    dc_set_flags_nz16(g_cpu.X);
}

/* C0:67A2  Operands b1-b3 -> dp CD-CF; dp D9 = position of b4; returns in
 * 16-bit A mode with A = X = b4 * 2 (a table index). */
DC_ENTRY(C067A2, RTS)
{
    u16 y = g_cpu.Y;
    dp8_set(0xCD, ev_byte((u16)(y + 1)));
    dp8_set(0xCE, ev_byte((u16)(y + 2)));
    dp8_set(0xCF, ev_byte((u16)(y + 3)));
    u16 x = (u16)(y + 4);
    dp16_set(0xD9, x);
    u16 a = (u16)(ev_byte(x) << 1);
    dc_set_mx(0, 0);
    g_cpu.A = a;
    g_cpu.X = a;
    dc_set_flags_nz16(a);
    g_cpu._flag_C = 0;
}
