/* C3:0B03  A small script machine run from WRAM code (7E:3749, D = 0A00,
 * DB = 7E), apparently for sound and screen sequencing. Bank C3.
 *
 * dp 20 = script pointer (bank DB), dp 22.. = loop counters, dp 30 =
 * return address. Registers r0..r15 are the words at D + 2r.
 *
 *   0x..7F  arithmetic on register r (low nibble), by the high nibble:
 *           0x00 r = imm16          0x10/40/50 r += imm16
 *           0x20 r = dp[b & 1E]     0x30/60/70 r += dp[b & 1E]
 *   8r lim, target   loop: counter r counts up to lim, jumping to target
 *   9r lim, target   the same, then stop (both ways)
 *   A0..FF by the low nibble:
 *     0 w      call 7E:37F3 with 00F0 = w     8 w  add w to the list at 0940
 *     1 a, b   00:a = b                       9 w  remove w from the list at 0920
 *     2 a, w   00:a = w (word)                A w  add w to the list at 0920
 *     3 t      call t (return address in 30)  B b, r  PPU register 21r = b
 *     4        return                         C    stop
 *     5 n      wait while APU port 2 < n      D t  jump to t and stop
 *              (stops here, retried later)    E t  jump to t
 *     6 a, b   sound command 1E00 = a,        F    halt: stays here, 00:0000 = 1
 *              1E02 = b (C7:0004 unless 0388)
 *     7 w      remove w from the list at 0940
 *
 * The flags are kept as the original leaves them: the caller sees N/Z/C/V
 * of the stopping command (V carries over from the last addition). */
#include "ram.h"

static u16 pc_get(void)   { return dp16(0x20); }
static void pc_set(u16 v) { dp16_set(0x20, v); }
static u16 arg16(void)    { return bus16(dba(pc_get())); }
static u8 arg8(void)      { return bus8(dba(pc_get())); }
/* INC $20 */
static void pc_inc(void)  { u16 v = (u16)(pc_get() + 1); pc_set(v); dc_set_flags_nz16(v); }
static void pc_skip2(void) { pc_inc(); pc_inc(); }

/* Lists of words at 0920 / 0940 (0 = free). The original searches without
 * a bound; PHP/PLP keep the flags. */
static void list_add(u16 base)
{
    u16 v = g_cpu.X;
    g_cpu.X = 0;
    while (abs16(base + g_cpu.X)) g_cpu.X += 2;
    g_cpu.A = v;
    abs16_set(base + g_cpu.X, v);
}
static void list_remove(u16 base)
{
    g_cpu.A = g_cpu.X;
    g_cpu.X = 0;
    while (abs16(base + g_cpu.X) != g_cpu.A) g_cpu.X += 2;
    abs16_set(base + g_cpu.X, 0);
}

/* C3:0C92  Loop command: counter r (dp 22 + 2r) against the limit word;
 * below it (or 0), count and jump to the target; at the limit, reset and
 * go on past the command. */
static void loop_cmd(void)
{
    g_cpu._flag_C = 0;
    g_cpu.X = (u16)((dp16(0xF0) & 0x0F) << 1);
    u16 c = dp16((u8)(0x22 + g_cpu.X));
    dc_set_flags_nz16(c);
    if (c) {
        dc_cmp16(c, arg16());
        if (!((u16)(c - arg16()) & 0x8000)) {
            dp16_set((u8)(0x22 + g_cpu.X), 0);
            g_cpu._flag_C = 0;
            g_cpu.A = dc_adc16(pc_get(), 4);
            pc_set(g_cpu.A);
            return;
        }
    }
    c++;
    g_cpu.A = c;
    dp16_set((u8)(0x22 + g_cpu.X), c);
    pc_skip2();
    g_cpu.A = arg16();
    dc_set_flags_nz16(g_cpu.A);
    pc_set(g_cpu.A);
}

/* Arithmetic commands 00-7F. */
static void arith(u16 op)
{
    g_cpu._flag_C = 0;
    u16 reg = dc_adc16(g_cpu.D, (u16)((op & 0x0F) << 1));
    dp16_set(0xF0, reg);
    u16 grp = op & 0xF0;
    u16 v;
    if (grp & 0x20) {
        dc_cmp16(grp, 0x20);
        g_cpu.X = arg16() & 0x1E;
        if (grp == 0x20) v = dp16((u8)g_cpu.X);
        else { g_cpu._flag_C = 0; v = dc_adc16(bus16(dba(reg)), dp16((u8)g_cpu.X)); }
        pc_inc();
    } else {
        dc_cmp16(grp, 0);
        if (!grp) v = arg16();
        else { g_cpu._flag_C = 0; v = dc_adc16(bus16(dba(reg)), arg16()); }
        pc_skip2();
    }
    g_cpu.A = v;
    bus16_set(dba(reg), v);
}

/* A0-FF: runs command n; returns 0 when the script stops, VM_UNWOUND when the
 * original took over (command 6's sound driver call, see dc_jsl_y). */
#define VM_UNWOUND 2
static int command(u8 n)
{
    switch (n) {
    case 0x0:   /* C3:0C51 */
        g_cpu.A = arg16();
        abs16_set(0x00F0, g_cpu.A);
        dc_jsl(0x7E37F3, 0xC30C58);
        dc_set_mx(0, g_cpu.x_flag);
        pc_skip2();
        return 1;
    case 0x1:   /* C3:0C19 */
        g_cpu.A = g_cpu.X = arg16();
        pc_skip2();
        dc_set_mx(1, 0);
        dc_set_a8(arg8());
        dc_set_flags_nz8((u8)g_cpu.A);
        bus8_set(g_cpu.X, (u8)g_cpu.A);
        dc_set_mx(0, 0);
        pc_inc();
        return 1;
    case 0x2:   /* C3:0C41 */
        g_cpu.X = arg16();
        pc_skip2();
        g_cpu.A = arg16();
        bus16_set(g_cpu.X, g_cpu.A);
        pc_skip2();
        return 1;
    case 0x3:   /* C3:0C7C */
        g_cpu.A = (u16)(pc_get() + 2);
        dp16_set(0x30, g_cpu.A);
        g_cpu.A = arg16();
        dc_set_flags_nz16(g_cpu.A);
        pc_set(g_cpu.A);
        return 1;
    case 0x4:   /* C3:0C85 */
        g_cpu.A = dp16(0x30);
        dc_set_flags_nz16(g_cpu.A);
        pc_set(g_cpu.A);
        return 1;
    case 0x5: { /* C3:0C2A */
        dc_set_mx(1, 0);
        u8 a;
        do {   /* read until two reads agree; the time lets the sound CPU run */
            a = bus8(0x002142);
            dc_cmp8(a, bus8(0x002142));
            dc_spend(104);
        } while (!g_cpu._flag_Z);
        a &= 0x0F;
        dc_set_a8(a);
        dc_cmp8(a, arg8());
        dc_set_mx(0, 0);
        if (!g_cpu._flag_N) { pc_inc(); return 1; }
        u16 v = (u16)(pc_get() - 1);
        pc_set(v);
        dc_set_flags_nz16(v);
        return 0;
    }
    case 0x6:   /* C3:0BD4 */
        abs16_set(0x1E00, arg16());
        pc_skip2();
        g_cpu.A = arg16();
        abs16_set(0x1E02, g_cpu.A);
        dc_set_mx(1, 0);
        dc_set_a8(abs8(0x0388));
        dc_set_flags_nz8((u8)g_cpu.A);
        /* The original sits in the command (reached by JMP from the loop at
         * 0B03, no frame of its own) with the VM state in $20 / $F0: the code
         * after the call (0BB1) just steps $20 and jumps back to 0B03. */
        if (!(u8)g_cpu.A && dc_jsl_y(0xC70004, 0xC30BE9)) return VM_UNWOUND;
        dc_set_mx(0, g_cpu.x_flag);
        pc_skip2();
        return 1;
    case 0x7: g_cpu.X = arg16(); list_remove(0x0940); pc_skip2(); return 1;   /* C3:0BCC */
    case 0x8: g_cpu.X = arg16(); list_add(0x0940);    pc_skip2(); return 1;   /* C3:0BC4 */
    case 0x9: g_cpu.X = arg16(); list_remove(0x0920); pc_skip2(); return 1;   /* C3:0BBC */
    case 0xA: g_cpu.X = arg16(); list_add(0x0920);    pc_skip2(); return 1;   /* C3:0BAB */
    case 0xB: { /* C3:0C5F */
        dc_set_mx(1, 0);
        u8 b = arg8();
        dc_set_mx(0, 0);
        pc_inc();
        g_cpu._flag_C = 0;
        g_cpu.A = g_cpu.X = dc_adc16(arg16() & 0xFF, 0x2100);
        dc_set_mx(1, 0);
        dc_set_a8(b);
        dc_set_flags_nz8(b);
        bus8_set(g_cpu.X, b);
        dc_set_mx(0, 0);
        pc_inc();
        return 1;
    }
    case 0xC:   /* C3:0BAA */
        return 0;
    case 0xD:   /* C3:0BA6 */
        g_cpu.A = arg16();
        dc_set_flags_nz16(g_cpu.A);
        pc_set(g_cpu.A);
        return 0;
    case 0xE:   /* C3:0B9F */
        g_cpu.A = arg16();
        dc_set_flags_nz16(g_cpu.A);
        pc_set(g_cpu.A);
        return 1;
    default: {  /* C3:0B95 */
        u16 v = (u16)(pc_get() - 1);
        pc_set(v);
        g_cpu.A = 1;
        bus16_set(0x000000, 1);
        dc_set_flags_nz16(1);
        return 0;
    }
    }
}

static void vm_run(void)
{
    for (;;) {
        u16 op = arg16();
        pc_inc();
        if (!(op & 0x80)) { arith(op); continue; }
        dp16_set(0xF0, op);
        u16 grp = op & 0xF0;
        dc_cmp16(grp, 0x80);
        if (grp == 0x80) { loop_cmd(); continue; }
        dc_cmp16(grp, 0x90);
        if (grp == 0x90) { loop_cmd(); return; }
        g_cpu.A = g_cpu.X = (u16)((op & 0x0F) << 1);
        g_cpu._flag_C = 0;
        dc_set_flags_nz16(g_cpu.A);
        if (command((u8)(op & 0x0F)) != 1) return;
    }
}

/* C3:0AFF  Runs the script until it stops. */
/* Through the JSR, so that the loop runs with the original's stack (command 6
 * may hand the rest of the script to the original). */
DC_ENTRY(C30AFF, RTL) { dc_jsr_y(0xC30B03, 0xC30AFF); }
DC_ENTRY(C30B03, RTS) { vm_run(); }

/* The commands are jump-table targets of 0B03; each runs once and then the
 * machine goes on (or stops) as in the original. */
#define VM_CMD(pc, n) DC_ENTRY(pc, RTS) { if (command(n) == 1) vm_run(); }
VM_CMD(C30C51, 0x0)
VM_CMD(C30C19, 0x1)
VM_CMD(C30C41, 0x2)
VM_CMD(C30C7C, 0x3)
VM_CMD(C30C85, 0x4)
VM_CMD(C30C2A, 0x5)
VM_CMD(C30BD4, 0x6)
VM_CMD(C30BCC, 0x7)
VM_CMD(C30BC4, 0x8)
VM_CMD(C30BBC, 0x9)
VM_CMD(C30BAB, 0xA)
VM_CMD(C30C5F, 0xB)
VM_CMD(C30BAA, 0xC)
VM_CMD(C30BA6, 0xD)
VM_CMD(C30B9F, 0xE)
VM_CMD(C30B95, 0xF)

/* The list subroutines and the loop command on their own (JSR targets). */
DC_ENTRY(C30CB8, RTS) { list_remove(0x0920); }
DC_ENTRY(C30CCD, RTS) { list_remove(0x0940); }
DC_ENTRY(C30CE2, RTS) { list_add(0x0920); }
DC_ENTRY(C30CF8, RTS) { list_add(0x0940); }
DC_ENTRY(C30C92, RTS) { loop_cmd(); }
