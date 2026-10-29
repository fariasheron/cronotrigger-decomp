/* Field event script: commands 00-11 (bank C0).
 *
 * The interpreter (C0:5709, still original code) keeps the current object in
 * EV_CUR_OBJ and calls the handler for the command byte at script position
 * Y (X = Y). A handler returns the next position in X (and usually Y), and
 * the carry: set = carry on with the next command, clear = stop this object
 * for now (the same command runs again on a later frame when X = Y). */
#include "ram.h"

static inline void ev_return(u16 next, bool go_on) { g_cpu.X = next; g_cpu._flag_C = go_on; }
static u16 adc_v(u16 a, u16 b) { u16 r = (u16)(a + b); g_cpu._flag_V = ((~(a ^ b) & (a ^ r)) >> 15) & 1; return r; }   /* TXA; CLC; ADC $D9 (V of the 16-bit add) */
static u16 sbc_v(u16 a, u16 b) { u16 r = (u16)(a - b); g_cpu._flag_V = (((a ^ b) & (a ^ r)) >> 15) & 1; return r; }   /* TXA; SEC; SBC $D9 */
static inline u8 ev_arg(u16 pos) { return ev_byte(pos); }
/* prio * 0x80 through the hardware multiplier, as the original does. */
static inline u16 prio_offset(u8 prio) { io8_set(0x4202, prio); io8_set(0x4203, 0x80); return (u16)(prio * 0x80); }

/* 00  Return: the current function ends. The object resumes the function it
 * interrupted: the next lower priority level (higher number) with a saved
 * position. At priority 7 there is nothing below, so the object stays. */
DC_ENTRY(C05F74, RTS)
{
    DC_ASSUME_D(C05F74, 0x0100);
    u16 obj = ram16(EV_CUR_OBJ);
    u8 prio = ram8(OBJ_PRIORITY(obj));
    if (prio == 7) { dc_set_a8(prio); ev_return(g_cpu.Y, false); return; }
    u16 slot = (u16)(prio_offset(prio) + obj);
    ram16_set(EV_SAVED_POS(0, slot), 0);
    u16 resume;
    do {
        slot += 0x80;
        ram8_set(OBJ_PRIORITY(obj), (u8)(ram8(OBJ_PRIORITY(obj)) + 1));
        resume = ram16(EV_SAVED_POS(0, slot));
    } while (resume == 0);
    g_cpu.A = resume;
    ev_return(resume, true);
}

/* Common part of the "call object function" commands: ask object `target`
 * to run function `arg` (high nibble = priority, low nibble = function).
 * A call with a higher priority (lower number) than what the object runs now
 * interrupts it right away (its position is saved for 00 Return); a lower
 * one is queued in the saved-position table if that level is free; the same
 * priority is ignored. Returns how it went. */
typedef enum { CALL_STARTED, CALL_QUEUED, CALL_SAME_PRIORITY } CallResult;
static CallResult ev_call_function(u16 target, u8 arg, u8 cur_prio)
{
    u8 prio = arg >> 4;
    ram8_set(EV_TMP_ARG, arg);
    ram8_set(EV_TMP_PRIO, prio);
    if (prio == cur_prio) return CALL_SAME_PRIORITY;
    if (prio > cur_prio) {
        u16 slot = (u16)(prio_offset(prio) + target);
        ram16_set(EV_TMP_SLOT, slot);
        if (ram16(EV_SAVED_POS(0, slot)) == 0) {
            ram16_set(EV_TMP_FNTAB, (u16)((target & 0xFF) << 4));
            ram16_set(EV_SAVED_POS(0, slot), ev_function_entry(target, arg));
        }
        return CALL_QUEUED;
    }
    u16 slot = (u16)(prio_offset(cur_prio) + target);
    ram16_set(EV_TMP_SLOT, slot);
    ram16_set(EV_SAVED_POS(0, slot), ram16(OBJ_SCRIPT_POS(target)));
    ram16_set(EV_TMP_FNTAB, (u16)((target & 0xFF) << 4));
    ram16_set(OBJ_SCRIPT_POS(target), ev_function_entry(target, arg));
    ram8_set(OBJ_PRIORITY(target), prio);
    ram8_set(OBJ_1A80(target), 0);
    ram8_set(OBJ_1A01(target), 0);
    return CALL_STARTED;
}
static void ev_note_target(u16 target)
{
    ram16_set(EV_TMP_OBJ, target);
}
static bool ev_obj_gone(u16 o) { return (ram8(OBJ_REMOVED(o)) & 0x80) || (ram8(OBJ_EV_DISABLED(o)) & 0x80); }

/* 02 obj, pf  Call object function, don't wait. Skipped when the target
 * refuses calls or is gone. */
static void ev_cmd02(u16 pos, u16 target)
{
    ev_note_target(target);
    if (ram8(OBJ_CALLS_BLOCKED(target)) || ev_obj_gone(target)) {
        g_cpu.Y = (u16)(pos + 3); ev_return(g_cpu.Y, true); return;
    }
    u8 cur = ram8(OBJ_PRIORITY(target));
    ram8_set(EV_TMP_D9, cur);
    ev_call_function(target, ev_arg(pos + 2), cur);
    g_cpu.Y = (u16)(pos + 3);
    ev_return(g_cpu.Y, true);
}
DC_ENTRY(C05FB6, RTS)
{
    DC_ASSUME_D(C05FB6, 0x0100);
    u16 pos = g_cpu.Y;
    ev_cmd02(pos, ev_arg(pos + 1));
}

/* 03 obj, pf  Call object function, wait until the target accepts it: when
 * the target is busy with the same or a higher priority the command repeats. */
static void ev_cmd03(u16 pos, u16 target)
{
    ev_note_target(target);
    if (ram8(OBJ_CALLS_BLOCKED(target))) { ev_return(pos, false); return; }
    if (ev_obj_gone(target)) { ev_return((u16)(pos + 3), true); return; }
    u8 cur = ram8(OBJ_PRIORITY(target));
    ram8_set(EV_TMP_D9, cur);
    u8 arg = ev_arg(pos + 2);
    if ((arg >> 4) >= cur) {
        ram8_set(EV_TMP_ARG, arg); ram8_set(EV_TMP_PRIO, arg >> 4);
        ev_return(pos, false); return;
    }
    ev_call_function(target, arg, cur);
    ev_return((u16)(pos + 3), true);
}
DC_ENTRY(C0607E, RTS)
{
    DC_ASSUME_D(C0607E, 0x0100);
    u16 pos = g_cpu.Y;
    ev_cmd03(pos, ev_arg(pos + 1));
}

/* 04 obj, pf  Call object function and wait until it has finished. First
 * pass: start it like 03 and mark the caller waiting; later passes: wait
 * until the target is back below the called priority (a higher number). */
static void ev_cmd04(u16 pos, u16 target)
{
    u16 self = ram16(EV_CUR_OBJ);
    ev_note_target(target);
    if (ram8(EV_WAITING_CALL(self)) == 0) {
        if (ram8(OBJ_CALLS_BLOCKED(target))) { ev_return(pos, false); return; }
        if (ev_obj_gone(target)) { ev_return((u16)(pos + 3), true); return; }
        u8 cur = ram8(OBJ_PRIORITY(target));
        ram8_set(EV_TMP_D9, cur);
        u8 arg = ev_arg(pos + 2);
        if ((arg >> 4) >= cur) {
            ram8_set(EV_TMP_ARG, arg); ram8_set(EV_TMP_PRIO, arg >> 4);
            ev_return(pos, false); return;
        }
        ev_call_function(target, arg, cur);
        ram8_set(EV_WAITING_CALL(self), 1);
        ev_return(pos, false);
        return;
    }
    if (!ev_obj_gone(target)) {
        u8 cur = ram8(OBJ_PRIORITY(target));
        ram8_set(EV_TMP_D9, cur);
        u8 arg = ev_arg(pos + 2);
        ram8_set(EV_TMP_ARG, arg); ram8_set(EV_TMP_PRIO, arg >> 4);
        if ((arg >> 4) >= cur) { ev_return(pos, false); return; }
    }
    ram8_set(EV_WAITING_CALL(self), 0);
    ev_return((u16)(pos + 3), true);
}
DC_ENTRY(C06109, RTS)
{
    DC_ASSUME_D(C06109, 0x0100);
    u16 pos = g_cpu.Y;
    ev_cmd04(pos, ev_arg(pos + 1));
}

/* 05/06/07 pc, pf  The same three calls aimed at a party member (pc = party
 * slot * 2); skipped when that member is not present. */
DC_ENTRY(C061E9, RTS)
{
    DC_ASSUME_D(C061E9, 0x0100);
    u16 pos = g_cpu.Y;
    u8 target = ram8(PARTY_OBJ(ev_arg(pos + 1)));
    if (target & 0x80) { g_cpu.Y = (u16)(pos + 3); ev_return(g_cpu.Y, true); return; }
    ev_cmd02(pos, target);
}
DC_ENTRY(C061FE, RTS)
{
    DC_ASSUME_D(C061FE, 0x0100);
    u16 pos = g_cpu.Y;
    u8 target = ram8(PARTY_OBJ(ev_arg(pos + 1)));
    if (target & 0x80) { g_cpu.Y = (u16)(pos + 3); ev_return(g_cpu.Y, true); return; }
    ev_cmd03(pos, target);
}
DC_ENTRY(C06214, RTS)
{
    DC_ASSUME_D(C06214, 0x0100);
    u16 pos = g_cpu.Y;
    u16 self = ram16(EV_CUR_OBJ);
    u8 target = ram8(PARTY_OBJ(ev_arg(pos + 1)));
    if (ram8(EV_WAITING_CALL(self)) == 0 && (target & 0x80)) {
        g_cpu.Y = (u16)(pos + 3); ev_return(g_cpu.Y, true); return;
    }
    ev_cmd04(pos, target);
}

/* 08 / 09  Refuse / accept calls from other objects. */
DC_ENTRY(C06240, RTS)
{
    ram8_set(OBJ_CALLS_BLOCKED(ram16(EV_CUR_OBJ)), 1);
    dc_set_a8(1);
    ev_return((u16)(g_cpu.Y + 1), true);
}
DC_ENTRY(C0624B, RTS)
{
    ram8_set(OBJ_CALLS_BLOCKED(ram16(EV_CUR_OBJ)), 0);
    dc_set_a8(0);
    ev_return((u16)(g_cpu.Y + 1), true);
}

/* 0A obj  Remove object. */
DC_ENTRY(C06254, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 obj = ev_arg(pos);
    ram8_set(OBJ_REMOVED(obj), 0x80);
    ram8_set(OBJ_1A81(obj), 0);
    g_cpu.Y = pos;
    dc_set_a8(0);
    ev_return((u16)(pos + 1), true);
}

/* 0B obj  Stop running an object's script. Stopping itself ends this pass. */
DC_ENTRY(C06269, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 obj = ev_arg(pos);
    u8 v = ram8(OBJ_EV_DISABLED(obj)) | 0x80;
    ram8_set(OBJ_EV_DISABLED(obj), v);
    g_cpu.Y = pos;
    dc_set_a8(v);
    ev_return((u16)(pos + 1), obj != ram16(EV_CUR_OBJ));
}

/* 0C obj  Resume running an object's script. */
DC_ENTRY(C06282, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 obj = ev_arg(pos);
    u8 v = ram8(OBJ_EV_DISABLED(obj)) & 0x7F;
    ram8_set(OBJ_EV_DISABLED(obj), v);
    g_cpu.Y = pos;
    dc_set_a8(v);
    ev_return((u16)(pos + 1), true);
}

/* 0D v / 0E v  Set the current object's movement / positioning properties. */
DC_ENTRY(C06295, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 v = ev_arg(pos);
    ram8_set(OBJ_MOVE_PROPS(ram16(EV_CUR_OBJ)), v);
    g_cpu.Y = pos; dc_set_a8(v);
    ev_return((u16)(pos + 1), true);
}
DC_ENTRY(C062A4, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 v = ev_arg(pos);
    ram8_set(OBJ_POS_PROPS(ram16(EV_CUR_OBJ)), v);
    g_cpu.Y = pos; dc_set_a8(v);
    ev_return((u16)(pos + 1), true);
}

/* 10 n / 11 n  Jump forward / back n bytes (from the operand). */
DC_ENTRY(C062B5, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 n = ev_arg(pos);
    ram16_set(EV_TMP_D9, n);
    g_cpu.Y = pos;
    g_cpu.A = adc_v(pos, n);
    ev_return(g_cpu.A, true);
}
DC_ENTRY(C062CB, RTS)
{
    u16 pos = (u16)(g_cpu.Y + 1);
    u8 n = ev_arg(pos);
    ram16_set(EV_TMP_D9, n);
    g_cpu.Y = pos;
    g_cpu.A = sbc_v(pos, n);
    ev_return(g_cpu.A, true);
}
