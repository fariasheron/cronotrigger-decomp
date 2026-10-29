/* Field event script: data commands 48-56 (bank C0). They move bytes and words
 * between event variables (7F:0200 + n*2), bank 7F memory and any 24-bit address. */
#include "ram.h"

static inline void ev_return(u16 next, bool go_on) { g_cpu.X = next; g_cpu._flag_C = go_on; }

#define EV_PTR_SRC   0x01CD   /* 24-bit, direct page $CD */
#define EV_PTR_DST   0x01D0   /* 24-bit, direct page $D0 */
#define EV_TMP_C1    0x01C1   /* 16-bit */

static inline u32 ev_var(u8 n)   { return EV_LOCAL + n * 2; }
static inline u32 ev_mem7f(u16 a) { return 0x10000 + a; }
static inline u32 next24(u32 a)   { return (a + 1) & 0xFFFFFF; }
static inline u16 far16(u32 a)    { return (u16)(bus8(a) | bus8(next24(a)) << 8); }
static inline void far16_set(u32 a, u16 v) { bus8_set(a, (u8)v); bus8_set(next24(a), (u8)(v >> 8)); }

/* Operands: 24-bit address at pos+1 (saved in $CD or $D0). */
static u32 ev_take_ptr(u32 dp, u16 pos)
{
    for (int i = 0; i < 3; i++) ram8_set(dp + i, ev_byte((u16)(pos + 1 + i)));
    return ram24(dp);
}

/* 48 a24 n  var[n] (byte) = [a24] */
DC_ENTRY(C06792, RTS)
{
    DC_ASSUME_D(C06792, 0x0100);
    u16 pos = g_cpu.Y;
    u32 p = ev_take_ptr(EV_PTR_SRC, pos);
    ram16_set(EV_TMP_D9, (u16)(pos + 4));
    ram8_set(ev_var(ev_byte((u16)(pos + 4))), bus8(p));
    ev_return((u16)(pos + 5), true);
}
/* 49 a24 n  var[n] (word) = [a24] */
DC_ENTRY(C067C7, RTS)
{
    DC_ASSUME_D(C067C7, 0x0100);
    u16 pos = g_cpu.Y;
    u32 p = ev_take_ptr(EV_PTR_SRC, pos);
    ram16_set(EV_TMP_D9, (u16)(pos + 4));
    ram16_set(ev_var(ev_byte((u16)(pos + 4))), far16(p));
    ev_return((u16)(pos + 5), true);
}
/* 4A a24 b  [a24] = b */
DC_ENTRY(C067D7, RTS)
{
    DC_ASSUME_D(C067D7, 0x0100);
    u16 pos = g_cpu.Y;
    u32 p = ev_take_ptr(EV_PTR_DST, pos);
    bus8_set(p, ev_byte((u16)(pos + 4)));
    ev_return((u16)(pos + 5), true);
}
/* 4B a24 w  [a24] = w */
DC_ENTRY(C067FB, RTS)
{
    DC_ASSUME_D(C067FB, 0x0100);
    u16 pos = g_cpu.Y;
    u32 p = ev_take_ptr(EV_PTR_DST, pos);
    far16_set(p, ev_word((u16)(pos + 4)));
    ev_return((u16)(pos + 6), true);
}
/* 4C a24 n  [a24] = var[n] (byte) */
DC_ENTRY(C0680C, RTS)
{
    DC_ASSUME_D(C0680C, 0x0100);
    u16 pos = g_cpu.Y;
    u32 p = ev_take_ptr(EV_PTR_DST, pos);
    ram16_set(EV_TMP_D9, (u16)(pos + 4));
    bus8_set(p, ram8(ev_var(ev_byte((u16)(pos + 4)))));
    ev_return((u16)(pos + 5), true);
}
/* 4D a24 n  [a24] = var[n] (word) */
DC_ENTRY(C06829, RTS)
{
    DC_ASSUME_D(C06829, 0x0100);
    u16 pos = g_cpu.Y;
    u32 p = ev_take_ptr(EV_PTR_DST, pos);
    ram16_set(EV_TMP_D9, (u16)(pos + 4));
    far16_set(p, ram16(ev_var(ev_byte((u16)(pos + 4)))));
    ev_return((u16)(pos + 5), true);
}
/* 4E a16 bank len data...  Copy the inline data (len counts itself plus the
 * data) to bank:a16; bank is 7E when the byte is 7E, else 7F. */
DC_ENTRY(C06846, RTS)
{
    DC_ASSUME_D(C06846, 0x0100);
    u16 pos = g_cpu.Y;
    u16 dst = ev_word((u16)(pos + 1));
    u32 bank = ev_byte((u16)(pos + 3)) == 0x7E ? 0x00000 : 0x10000;
    u16 len = ev_word((u16)(pos + 4));
    ram16_set(EV_TMP_C1, (u16)(len - 3));
    u16 src = (u16)(pos + 6 + 0x2001);
    u32 n = (u16)(len - 3) + 1u;
    for (u32 i = 0; i < n; i++, src++, dst++)
        ram8_set(bank + dst, ram8(0x10000 + src));
    u16 r = (u16)(src - 0x2001);
    g_cpu._flag_V = ((src ^ 0x2001) & (src ^ r) & 0x8000) != 0;
    g_cpu.A = r;
    g_cpu.Y = dst;
    ev_return(r, true);
}
/* 4F b n  var[n] (byte) = b */
DC_ENTRY(C068A5, RTS)
{
    DC_ASSUME_D(C068A5, 0x0100);
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(pos);
    ram8_set(EV_TMP_D9, b);
    ram8_set(ev_var(ev_byte((u16)(pos + 1))), b);
    g_cpu.Y = pos;
    ev_return((u16)(pos + 2), true);
}
/* 50 w n  var[n] (word) = w */
DC_ENTRY(C068C6, RTS)
{
    DC_ASSUME_D(C068C6, 0x0100);
    u16 pos = (u16)(g_cpu.Y + 1);
    u16 w = ev_word(pos);
    ram16_set(EV_TMP_D9, w);
    ram16_set(ev_var(ev_byte((u16)(pos + 2))), w);
    g_cpu.Y = pos;
    ev_return((u16)(pos + 3), true);
}
/* 51 s d  var[d] (byte) = var[s];  52: word */
static void ev_copy_var(bool word)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u16 v = ram16(ev_var(ev_byte(pos)));
    ram16_set(EV_TMP_D9, v);
    u32 d = ev_var(ev_byte((u16)(pos + 1)));
    if (word) ram16_set(d, v); else ram8_set(d, (u8)v);
    g_cpu.Y = (u16)(pos + 1);
    ev_return((u16)(pos + 2), true);
}
DC_ENTRY(C068E9, RTS) { DC_ASSUME_D(C068E9, 0x0100); ev_copy_var(false); }
DC_ENTRY(C06913, RTS) { DC_ASSUME_D(C06913, 0x0100); ev_copy_var(true); }
/* 53 a16 n  var[n] (byte) = 7F:a16;  54: word */
static void ev_load_7f(bool word)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u16 v = ram16(ev_mem7f(ev_word(pos)));
    ram16_set(EV_TMP_D9, v);
    u32 d = ev_var(ev_byte((u16)(pos + 2)));
    if (word) ram16_set(d, v); else ram8_set(d, (u8)v);
    g_cpu.Y = (u16)(pos + 2);
    ev_return((u16)(pos + 3), true);
}
DC_ENTRY(C0693D, RTS) { DC_ASSUME_D(C0693D, 0x0100); ev_load_7f(false); }
DC_ENTRY(C06964, RTS) { DC_ASSUME_D(C06964, 0x0100); ev_load_7f(true); }
/* 55 n  var[n] (byte) = 7F:0000 */
DC_ENTRY(C0698B, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 v = ram8(ev_mem7f(0));
    dc_set_a8(v);
    ram8_set(ev_var(ev_byte(pos)), v);
    g_cpu.Y = pos;
    ev_return((u16)(pos + 1), true);
}
/* 56 b a16  7F:a16 = b */
DC_ENTRY(C069A6, RTS)
{
    DC_ASSUME_D(C069A6, 0x0100);
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(pos);
    ram8_set(EV_TMP_D9, b);
    ram8_set(ev_mem7f(ev_word((u16)(pos + 1))), b);
    g_cpu.Y = (u16)(pos + 3);
    ev_return(g_cpu.Y, true);
}

/* 58 s a16  7F:a16 = var[s] (byte);  59: word */
static void ev_store_7f(bool word)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u16 v = ram16(ev_var(ev_byte(p)));
    ram16_set(EV_TMP_D9, v);
    u32 d = ev_mem7f(ev_word((u16)(p + 1)));
    if (word) ram16_set(d, v); else ram8_set(d, (u8)v);
    g_cpu.Y = (u16)(p + 3);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C069C4, RTS) { DC_ASSUME_D(C069C4, 0x0100); ev_store_7f(false); }
DC_ENTRY(C069EB, RTS) { DC_ASSUME_D(C069EB, 0x0100); ev_store_7f(true); }
/* 5A b  7F:0000 = b */
DC_ENTRY(C06A12, RTS)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    dc_set_a8(b);
    ram8_set(ev_mem7f(0), b);
    g_cpu.Y = p;
    ev_return((u16)(p + 1), true);
}

/* Arithmetic with the 65816 overflow flag (binary mode). */
static u8 add8(u8 a, u8 b)   { u8 r = (u8)(a + b);   g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 7) & 1; return r; }
static u16 add16(u16 a, u16 b) { u16 r = (u16)(a + b); g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1; return r; }
static u8 sub8(u8 a, u8 b)   { u8 r = (u8)(a - b);   g_cpu._flag_V = (((a ^ b) & (a ^ r)) >> 7) & 1; return r; }
static u16 sub16(u16 a, u16 b) { u16 r = (u16)(a - b); g_cpu._flag_V = (((a ^ b) & (a ^ r)) >> 15) & 1; return r; }

/* 5B b n  var[n] += b (byte) */
DC_ENTRY(C06A1F, RTS)
{
    DC_ASSUME_D(C06A1F, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    ram8_set(EV_TMP_D9, b);
    u32 v = ev_var(ev_byte((u16)(p + 1)));
    ram8_set(v, add8(ram8(v), b));
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}
/* 5D s d  var[d] += var[s] (byte);  5E: word */
static void ev_add_var(bool word)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u16 s = ram16(ev_var(ev_byte(p)));
    ram16_set(EV_TMP_D9, s);
    u32 v = ev_var(ev_byte((u16)(p + 1)));
    if (word) ram16_set(v, add16(ram16(v), s)); else ram8_set(v, add8(ram8(v), (u8)s));
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C06A45, RTS) { DC_ASSUME_D(C06A45, 0x0100); ev_add_var(false); }
DC_ENTRY(C06A74, RTS) { DC_ASSUME_D(C06A74, 0x0100); ev_add_var(true); }
/* 5F b n  var[n] -= b (byte) */
DC_ENTRY(C06AA3, RTS)
{
    DC_ASSUME_D(C06AA3, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    ram8_set(EV_TMP_D9, b);
    u32 v = ev_var(ev_byte((u16)(p + 1)));
    ram8_set(v, sub8(ram8(v), b));
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}
/* 60 w n  var[n] -= w (word) */
DC_ENTRY(C06AC9, RTS)
{
    DC_ASSUME_D(C06AC9, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u16 w = ev_word(p);
    ram16_set(EV_TMP_D9, w);
    u32 v = ev_var(ev_byte((u16)(p + 2)));
    ram16_set(v, sub16(ram16(v), w));
    g_cpu.Y = (u16)(p + 3);
    ev_return(g_cpu.Y, true);
}
/* 61 s d  var[d] -= var[s] (byte) */
DC_ENTRY(C06AF1, RTS)
{
    DC_ASSUME_D(C06AF1, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u16 s = ram16(ev_var(ev_byte(p)));
    ram16_set(EV_TMP_D9, s);
    u32 v = ev_var(ev_byte((u16)(p + 1)));
    ram8_set(v, sub8(ram8(v), (u8)s));
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}

/* Bit masks: DB:FF20 = 01 02 04 .. 80, DB:FF28 = their complements (in ROM). */
static u8 bit_mask(u16 base, u16 n) { return bus8(((u32)g_cpu.DB << 16) + base + n); }
enum { OP_OR, OP_AND, OP_EOR };
static u8 apply(int op, u8 v, u8 m) { return op == OP_OR ? (u8)(v | m) : op == OP_AND ? (u8)(v & m) : (u8)(v ^ m); }

/* 63 b n  set bit b of var[n];  64: clear it */
static void ev_var_bit(u16 table, int op)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u8 m = bit_mask(table, ev_byte(p));
    ram8_set(EV_TMP_D9, m);
    u32 v = ev_var(ev_byte((u16)(p + 1)));
    ram8_set(v, apply(op, ram8(v), m));
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C06B20, RTS) { DC_ASSUME_D(C06B20, 0x0100); ev_var_bit(0xFF20, OP_OR); }
DC_ENTRY(C06B4B, RTS) { DC_ASSUME_D(C06B4B, 0x0100); ev_var_bit(0xFF28, OP_AND); }
/* 65 b a  set bit (b & 0F) of 7F:a, where a is a byte plus 0x100 when b bit 7
 * is set (bits 4-6 of b are ignored, so b & 0F above 7 reads past the table);
 * 66: clear it */
static void ev_7f_bit(u16 table, int op)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    ram8_set(EV_TMP_OBJ, b);
    u8 m = bit_mask(table, b & 0x0F);
    ram8_set(EV_TMP_D9, m);
    u32 v = ev_mem7f((u16)((b >> 7) << 8 | ev_byte((u16)(p + 1))));
    ram8_set(v, apply(op, ram8(v), m));
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C06B76, RTS) { DC_ASSUME_D(C06B76, 0x0100); ev_7f_bit(0xFF20, OP_OR); }
DC_ENTRY(C06BA4, RTS) { DC_ASSUME_D(C06BA4, 0x0100); ev_7f_bit(0xFF28, OP_AND); }
/* 67 b n  var[n] &= b;  69: |= b;  6B: ^= b */
static void ev_var_logic(int op)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    ram8_set(EV_TMP_D9, b);
    u32 v = ev_var(ev_byte((u16)(p + 1)));
    ram8_set(v, apply(op, ram8(v), b));
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C06BD2, RTS) { DC_ASSUME_D(C06BD2, 0x0100); ev_var_logic(OP_AND); }
DC_ENTRY(C06BF7, RTS) { DC_ASSUME_D(C06BF7, 0x0100); ev_var_logic(OP_OR); }
DC_ENTRY(C06C1C, RTS) { DC_ASSUME_D(C06C1C, 0x0100); ev_var_logic(OP_EOR); }
/* 6F b n  var[n] >>= b (b = 0 shifts 256 times) */
DC_ENTRY(C06C41, RTS)
{
    DC_ASSUME_D(C06C41, 0x0100);
    u16 p = (u16)(g_cpu.Y + 1);
    u8 b = ev_byte(p);
    u32 v = ev_var(ev_byte((u16)(p + 1)));
    ram8_set(v, (b == 0 || b >= 8) ? 0 : (u8)(ram8(v) >> b));
    ram8_set(EV_TMP_D9, 0);
    g_cpu.Y = (u16)(p + 2);
    ev_return(g_cpu.Y, true);
}
/* 71 n  var[n]++ (byte) */
DC_ENTRY(C06C69, RTS)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u32 v = ev_var(ev_byte(p));
    ram8_set(v, (u8)(ram8(v) + 1));
    g_cpu.Y = (u16)(p + 1);
    ev_return(g_cpu.Y, true);
}
/* 72 n  var[n]++ (word);  73: var[n]-- (byte) */
DC_ENTRY(C06C85, RTS)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u32 v = ev_var(ev_byte(p));
    ram16_set(v, (u16)(ram16(v) + 1));
    g_cpu.Y = (u16)(p + 1);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C06CA1, RTS)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u32 v = ev_var(ev_byte(p));
    ram8_set(v, (u8)(ram8(v) - 1));
    g_cpu.Y = (u16)(p + 1);
    ev_return(g_cpu.Y, true);
}
/* 75 n  var[n] = 1 (byte);  76: = 1 (word);  77: = 0 (byte) */
static void ev_var_set(u16 v, bool word)
{
    u16 p = (u16)(g_cpu.Y + 1);
    u32 a = ev_var(ev_byte(p));
    if (word) ram16_set(a, v); else ram8_set(a, (u8)v);
    g_cpu.Y = (u16)(p + 1);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C06CBD, RTS) { ev_var_set(1, false); }
DC_ENTRY(C06CD6, RTS) { ev_var_set(1, true); }
DC_ENTRY(C06CF0, RTS) { ev_var_set(0, false); }
/* 7F n  var[n] = next random byte (DB:FE00 table, index $01F8) */
#define RNG_INDEX 0x01F8
DC_ENTRY(C06D09, RTS)
{
    DC_ASSUME_D(C06D09, 0x0100);
    u8 i = (u8)(ram8(RNG_INDEX) + 1);
    ram8_set(RNG_INDEX, i);
    u8 r = bit_mask(0xFE00, i);
    ram8_set(EV_TMP_D9, r);
    ev_var_set(r, false);
}
