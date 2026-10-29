/*
 * Chrono Trigger — per-game runtime glue.
 *
 * THIS FILE IS THE PORT. Everything else the scaffold produced is layout,
 * build wiring, and packaging; this is where the actual work happens.
 *
 * The framework does not, and cannot, drive an arbitrary SNES game on its
 * own. Two responsibilities always land on the game host:
 *
 *   1. Deciding what "one frame" means for THIS title, and returning from
 *      run_frame() at that boundary. A real ROM's reset vector never
 *      returns — it enters a main loop that waits on vblank — so somebody
 *      has to choose the yield point.
 *   2. Delivering NMI/IRQ at the hardware edge (see
 *      snesrecomp/docs/FRAME_MODEL_HOSTS.md).
 *
 * What follows is the shape every working port in this ecosystem converged
 * on, built from documented bridge entry points:
 *
 *      boot from the reset vector
 *      -> deliver NMI at the vblank edge, gated on NMITIMEN
 *      -> run the guest in slices until it parks or the frame's clock is out
 *      -> service a raster IRQ whenever the comparator asserts
 *      -> rasterize the field with HDMA per line
 *
 * It is a starting point, not a finished port: a title with unusual timing
 * will need the slice loop tightened (MetalWarriorsSNESRecomp's src/mw_rtl.c
 * and GundamWingEndlessDuelSNESRecomp's src/game_rtl.c are the two most
 * developed examples, and the second walks the beam explicitly while the CPU
 * is parked). What it will NOT do is sit at a black screen doing nothing,
 * which is what a driver that delivers no interrupt always does.
 *
 * Why NMI is not gated on WAI: an earlier version of this template delivered
 * an interrupt only when interp_bridge_lle_took_wai() was true. Real titles
 * overwhelmingly wait on a WRAM flag set by their own NMI handler, not on
 * WAI — Super Metroid spins `LDA $05B4 / BNE`, Zelda 3 on $0012, Super Mario
 * World on $0010 — so that branch never fired and the guest parked forever
 * on its first vblank wait. Quiescence on a read-only cycle IS the signal
 * that only an interrupt can make progress; that is the edge to act on.
 */

#include "game_rtl.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "common_cpu_infra.h"
#include "common_rtl.h"          /* SimpleHdma_*, g_snesrecomp_last_hdmaen */
#include "cpu_state.h"
#include "snes/dma.h"
#include "snes/interp_bridge.h"
#include "snes/saveload.h"

void interp_bridge_set_lle_resume_pc(uint32_t pc);
#include "snes/ppu.h"
#include "snes/snes.h"
#include "fiber_compat.h"

extern CpuState g_cpu;
extern Ppu *g_ppu;

/* One NTSC frame: 262 scanlines x 1364 master clocks. Bounds a productive
 * MMIO loop so it cannot run across several vblanks atomically. */
#define GAME_MASTER_CYCLES_PER_FRAME 357368ull

/* How many times a frame may re-enter the guest after it parks on a poll.
 * Each slice ends at a deterministic read-only cycle or at an IRQ; the bound
 * only stops a pathological loop from spinning the host. */
#define GAME_MAX_SLICES_PER_FRAME 64

/* 0 until the first frame has booted from the reset vector. */
static uint32_t g_resume_pc;

static uint32_t read_vector(uint32_t addr)
{
    /* Read through the guest bus so a mapper or coprocessor window resolves
     * the same way the CPU sees it. */
    uint32_t lo = snes_read(g_snes, addr);
    uint32_t hi = snes_read(g_snes, addr + 1u);
    return (hi << 8) | lo;
}

static uint32_t reset_vector(void) { return read_vector(0x00FFFCu); }
static uint32_t nmi_vector(void)   { return read_vector(0x00FFEAu); }

/* Is the guest parked in an `LDA $4210 / BPL` poll? On hardware the poll's
 * own read races the NMI: when the flag rises during the LDA, the main loop
 * sees bit 7 before the handler's acknowledge clears it. Chrono Trigger's
 * field loader ($C0:3B71) relies on winning that race with NMI enabled and
 * a handler that reads $4210 first thing; the frame model would always let
 * the handler win, so the room never fades in. */
static bool resume_is_rdnmi_poll(uint32_t pc)
{
    uint32_t at = pc;
    if (snes_read(g_snes, pc) == 0x10)            /* parked on the BPL */
        at = (pc & 0xFF0000u) | ((pc - 3u) & 0xFFFFu);
    return snes_read(g_snes, at) == 0xAD && snes_read(g_snes, at + 1u) == 0x10 &&
           snes_read(g_snes, at + 2u) == 0x42;
}
static uint32_t irq_vector(void)   { return read_vector(0x00FFEEu); }

/* Run one interrupt handler to its RTI, entered as hardware enters it: the
 * frame is pushed at the PC the guest was interrupted AT, so the handler's
 * terminal RTI returns into that instruction stream. */
static int g_fiber_mode = -1;
static int g_fiber_suspended;   /* game fiber parked inside native code */
extern int g_recomp_stack_top;
static uint32_t g_parked_pc;    /* guest PC the parked chain would resume at */
static unsigned g_frames_run;

static void game_run_interrupt(uint32_t vector, uint64_t frame_end)
{
    const uint32_t parked_pc = g_resume_pc;
    if (getenv("SNESRECOMP_UWNH_INTRCHECK") && (g_cpu.S < 0x0500 || g_cpu.S > 0x0A00)) {
        extern int uwnh_ring_dump_request;
        static int once;
        if (!once++) {
            fprintf(stderr, "[intrcheck] f=%u vec=%06X entered with S=%04X parked=%d pc=%06X\n",
                    g_frames_run, vector, g_cpu.S, g_fiber_suspended, g_fiber_suspended ? g_parked_pc : g_resume_pc);
            uwnh_ring_dump_request = 1;
        }
    }
    cpu_push_interrupt_frame_at(&g_cpu, g_resume_pc);
    interp_bridge_set_master_deadline(frame_end);
    const uint16_t s_before = g_cpu.S;
    extern int g_recomp_ancestor_floor;
    const int floor_before = g_recomp_ancestor_floor;
    if (g_fiber_suspended)
        g_recomp_ancestor_floor = g_recomp_stack_top;
    const int top_before = g_recomp_stack_top;
    (void)interp_bridge_run_interrupt(&g_cpu, vector);
    g_recomp_ancestor_floor = floor_before;
    if (g_recomp_stack_top != top_before) {
        if (getenv("SNESRECOMP_UWNH_INTRCHECK"))
            fprintf(stderr, "[intrcheck] f=%u vec=%06X recomp stack %d -> %d parked=%d\n",
                    g_frames_run, vector, top_before, g_recomp_stack_top, g_fiber_suspended);
    }
    /* SNESRECOMP_UWNH_INTRCHECK: report a handler that does not return to the
     * stack level it was entered at (its RTI must pop exactly its frame). */
    if (getenv("SNESRECOMP_UWNH_INTRCHECK") && g_cpu.S != (uint16_t)(s_before + 4))
        fprintf(stderr, "[intrcheck] f=%u vec=%06X S %04X -> %04X parked=%d pc=%06X\n", g_frames_run, vector,
                s_before, g_cpu.S, g_fiber_suspended, parked_pc);
    /* Clearing matters: a deadline left armed stays true for every AOT block
     * prologue afterwards, which turns every compiled body into an immediate
     * yield-unwind. */
    interp_bridge_set_master_deadline(0);
    {
        uint32_t resume = interp_bridge_lle_resume_pc();
        if (resume)
            g_resume_pc = resume;
    }
    /* In the fiber model the guest resumes where it was interrupted: a parked
     * chain continues in place, and an unparked guest at the PC it yielded at.
     * The bridge's last-yield PC read above belongs to the host's own bridge
     * context there (the game fiber keeps its own), so it is stale. */
    if (g_fiber_mode > 0)
        g_resume_pc = parked_pc;
}

/* ── Fiber frame model ────────────────────────────────────────────────────
 * The guest's main loop runs on its own C stack. When native code reaches the
 * frame deadline, instead of unwinding every compiled frame back to the
 * interpreter (which then runs the rest of each unwound function interpreted),
 * the whole call chain is suspended and resumed on the next frame after the
 * host has delivered NMI/IRQ, exactly like the CPU would continue after an
 * interrupt.
 *
 * On by default; SNESRECOMP_UWNH_FIBER=0 falls back to the unwind model. At
 * sea it moves about a third of the remaining interpreted work back to
 * native code. */
extern int (*interp_bridge_deadline_fiber_hook)(void);
size_t interp_bridge_fiber_ctx_size(void);
void interp_bridge_fiber_ctx_save(void *out);
void interp_bridge_fiber_ctx_load(const void *in);

static void *g_host_fiber, *g_game_fiber;
static int g_on_game_fiber;
static int g_fiber_took_wai;
static CpuState g_parked_regs;  /* CPU state at the park */
static uint64_t g_fiber_frame_end;
static int g_fiber_base_stack_top;
static _Alignas(16) unsigned char g_ctx_idle[256], g_ctx_game[256], g_ctx_host[256];

static int uwnh_deadline_hook(void)
{
    /* A poll that cannot name its resume PC unwinds as before: a parked
     * chain must be describable as a plain resume point for save states. */
    if (!g_on_game_fiber || !interp_bridge_deadline_poll_pc())
        return 0;
    { /* SNESRECOMP_UWNH_FIBER_RANGE=lo-hi (hex pc24): park only polls in the
       * range (the rest unwind); for bisecting a park that resumes wrongly. */
        static int init; static unsigned long lo = 0, hi = 0xFFFFFF; static int logp;
        if (!init) { const char *e = getenv("SNESRECOMP_UWNH_FIBER_RANGE"); init = 1;
                     if (e) sscanf(e, "%lx-%lx", &lo, &hi);
                     logp = getenv("SNESRECOMP_UWNH_FIBER_LOG") != NULL; }
        const unsigned long pc = interp_bridge_deadline_poll_pc();
        if (pc < lo || pc > hi) return 0;
        if (logp) fprintf(stderr, "[park] f=%u pc=%06lX S=%04X\n", g_frames_run, pc, g_cpu.S);
    }
    /* The CPU state the parked block continues from. A hardware interrupt
     * is transparent to the code it interrupts; the handlers run while this
     * chain is parked must be too, including the paired-return and tailcall
     * bookkeeping a prologue reads right after this poll. */
    CpuState regs = g_cpu;
    CpuTailcallContextSave tc;
    cpu_tailcall_context_save(&tc);
    /* The chain may park with a tail-call context still armed (a nested
     * interpreter run entered through a tail transfer): the interrupt code
     * run while parked must not inherit it as its own entry frame. */
    { const CpuTailcallContextSave none = {0}; cpu_tailcall_context_restore(&none); }
    g_parked_pc = interp_bridge_deadline_poll_pc();
    g_parked_regs = regs;
    g_fiber_suspended = 1;
    SwitchToFiber(g_host_fiber);
    g_fiber_suspended = 0;
    cpu_tailcall_context_restore(&tc);
    regs.ram = g_cpu.ram;
    regs.open_bus = g_cpu.open_bus;
    regs.cycles = g_cpu.cycles;
    regs.master_cycles = g_cpu.master_cycles;
    regs.coprocessor_master_cycles = g_cpu.coprocessor_master_cycles;
    g_cpu = regs;
    interp_bridge_set_master_deadline(g_fiber_frame_end);
    return 1;
}

static void CALLBACK uwnh_game_fiber_main(void *param)
{
    (void)param;
    for (;;) {
        interp_bridge_set_master_deadline(g_fiber_frame_end);
        interp_bridge_run_until_quiescent(&g_cpu, g_resume_pc);
        interp_bridge_set_master_deadline(0);
        {
            uint32_t resume = interp_bridge_lle_resume_pc();
            if (resume)
                g_resume_pc = resume;
        }
        g_fiber_took_wai = interp_bridge_lle_took_wai();
        SwitchToFiber(g_host_fiber);
    }
}

static void fiber_new_game(void)
{
    g_game_fiber = CreateFiber(1u << 20, uwnh_game_fiber_main, NULL);
    if (!g_game_fiber) {
        fprintf(stderr, "[uwnh] fiber create failed; using the unwind frame model\n");
        g_fiber_mode = 0;
        interp_bridge_deadline_fiber_hook = NULL;
        return;
    }
    memcpy(g_ctx_game, g_ctx_idle, sizeof g_ctx_game);
    g_fiber_suspended = 0;
}

static int fiber_enabled(void)
{
    if (g_fiber_mode < 0) {
        const char *e = getenv("SNESRECOMP_UWNH_FIBER");
        g_fiber_mode = !(e && e[0] == '0');
        if (g_fiber_mode && interp_bridge_fiber_ctx_size() > sizeof g_ctx_idle) {
            fprintf(stderr, "[uwnh] fiber ctx too large; using the unwind frame model\n");
            g_fiber_mode = 0;
        }
        if (g_fiber_mode) {
            g_host_fiber = ConvertThreadToFiber(NULL);
            if (!g_host_fiber) {
                g_fiber_mode = 0;
            } else {
                interp_bridge_fiber_ctx_save(g_ctx_idle);
                g_fiber_base_stack_top = g_recomp_stack_top;
                interp_bridge_deadline_fiber_hook = uwnh_deadline_hook;
                fiber_new_game();
            }
        }
    }
    return g_fiber_mode;
}

/* Run the game fiber until it parks (deadline, quiescent, WAI) and give
 * the host back an idle bridge. */
static void fiber_switch_to_game(void)
{
    interp_bridge_fiber_ctx_save(g_ctx_host);
    interp_bridge_fiber_ctx_load(g_ctx_game);
    g_on_game_fiber = 1;
    SwitchToFiber(g_game_fiber);
    g_on_game_fiber = 0;
    interp_bridge_fiber_ctx_save(g_ctx_game);
    interp_bridge_fiber_ctx_load(g_ctx_host);
}

/* Throw a parked chain away (the machine underneath it was replaced). */
static void fiber_discard(void)
{
    if (!g_fiber_mode || !g_fiber_suspended)
        return;
    DeleteFiber(g_game_fiber);
    g_recomp_stack_top = g_fiber_base_stack_top;
    {
        CpuTailcallContextSave none;
        memset(&none, 0, sizeof none);
        cpu_tailcall_context_restore(&none);
    }
    fiber_new_game();
}

void uwnh_selftest(CpuState *cpu);
static int g_framelog;   /* frames left to log after a state save/load (SNESRECOMP_PCLOG) */

/* A guest parked on WAI sleeps until the next interrupt. When a raster IRQ
 * is armed for a point before the frame deadline, walk the beam there so the
 * handler runs on its own line. Chrono Trigger's field IRQ at line 211 forces
 * blank and reuses DMA channel 7 for VRAM uploads; left pending until the
 * next frame's walk, it ran after the NMI had rebuilt the HDMA set and wiped
 * channel 7's window table (screen wipes went black every other frame). */
static int wai_reaches_irq(uint64_t frame_end)
{
    static int off = -1;
    if (off < 0) off = getenv("SNESRECOMP_CT_WAIIRQ_OFF") != NULL;
    if (off || g_cpu._flag_I) return 0;
    snes_sync_master_clock(g_snes, g_cpu.master_cycles);
    if (!g_snes->inIrq) {
        const uint32_t d = snes_master_clocks_until_irq(g_snes);
        if (!d || g_cpu.master_cycles + d + 1 >= frame_end) return 0;
        g_cpu.master_cycles += d + 1;
        snes_sync_master_clock(g_snes, g_cpu.master_cycles);
    }
    return g_snes->inIrq;
}


#ifndef _WIN32
#include <unistd.h>
#include <signal.h>
#include <sys/wait.h>
/* Differential test of the generated bodies (developer tool, Linux only).
 * SNESRECOMP_CT_FUNCTEST=<list from tools/dev/funclist.py>,
 * SNESRECOMP_CT_FUNCTEST_FRAME=<n>, SNESRECOMP_CT_FUNCTEST_OUT=<file>,
 * SNESRECOMP_CT_FUNCTEST_SEED=<n>. At frame n, from the live machine state,
 * each body runs once natively and once in the interpreter with an empty
 * program (nested calls interpreted too), each in a forked child with the
 * beam held; WRAM, SRAM and registers afterwards must agree. */
typedef struct { uint64_t ram, sram; uint16_t A, X, Y, S, D; uint8_t DB, flags, ok; } FtResult;
static uint64_t ft_hash(const uint8_t *p, size_t n)
{
    uint64_t h = 1469598103934665603ull;
    for (size_t i = 0; i < n; i++) { h ^= p[i]; h *= 1099511628211ull; }
    return h;
}
static const DispatchEntry *ft_find(uint32_t pc)
{
    unsigned lo = 0, hi = g_dispatch_table_count;
    while (lo < hi) {
        unsigned mid = (lo + hi) / 2;
        if (g_dispatch_table[mid].pc24 < pc) lo = mid + 1; else hi = mid;
    }
    return (lo < g_dispatch_table_count && g_dispatch_table[lo].pc24 == pc) ? &g_dispatch_table[lo] : NULL;
}
static void ft_child(int side, uint32_t pc, unsigned var, unsigned fs, const CpuState *base, unsigned seed, int fd)
{
    FtResult r; memset(&r, 0, sizeof r);
    CpuState *c = &g_cpu;
    *c = *base;
    alarm(2);
    interp_bridge_set_master_deadline(0);
    interp_bridge_deadline_fiber_hook = NULL;
    if (!getenv("SNESRECOMP_CT_FUNCTEST_NOHOLD")) snes_beam_hold(1);
    uint32_t h = (pc * 2654435761u) ^ (seed * 40503u);
    c->A = seed ? (uint16_t)h : 0; c->X = seed ? (uint16_t)(h >> 8) : 0; c->Y = seed ? (uint16_t)(h >> 16) : 0;
    c->m_flag = (var >> 1) & 1; c->x_flag = var & 1; c->emulation = 0;
    if (c->x_flag) { c->X &= 0xFF; c->Y &= 0xFF; }
    c->PB = (uint8_t)(pc >> 16);
    c->S = 0x1F80;
    if (fs == 3) { cpu_write8(c, 0x00, c->S, 0x00); c->S--; }
    cpu_write8(c, 0x00, c->S, 0xFF); c->S--;
    cpu_write8(c, 0x00, c->S, 0xEF); c->S--;
    c->host_return_valid = (uint8_t)fs;
    if (side == 0) {
        const DispatchEntry *e = ft_find(pc);
        if (!e || !e->variant[var]) _exit(3);
        e->variant[var](c);
    } else {
        cpu_select_interpreted_program();
        interp_tier_run_call_frame(c, pc, 0, (uint8_t)fs, NULL);
    }
    r.ram = ft_hash(c->ram, 0x20000);
    if (g_snes->cart && g_snes->cart->ram) r.sram = ft_hash(g_snes->cart->ram, g_snes->cart->ramSize);
    r.A = c->m_flag ? (c->A & 0xFF) | (c->A & 0xFF00) : c->A;
    r.X = c->X; r.Y = c->Y; r.S = c->S; r.D = c->D; r.DB = c->DB;
    r.flags = (uint8_t)(c->_flag_N << 7 | c->_flag_V << 6 | c->m_flag << 5 | c->x_flag << 4 | c->_flag_D << 3 | c->_flag_I << 2 | c->_flag_Z << 1 | c->_flag_C);
    r.ok = 1;
    if (write(fd, &r, sizeof r) != (ssize_t)sizeof r) _exit(4);
    if (side == 0 && getenv("SNESRECOMP_CT_FUNCTEST_DUMP")) {
        char path[256]; snprintf(path, sizeof path, "%s/%06X_%u_%c.bin", getenv("SNESRECOMP_CT_FUNCTEST_DUMP"), pc, var, 'n');
        FILE *f = fopen(path, "wb"); if (f) { fwrite(c->ram, 1, 0x20000, f); fclose(f); }
    }
    if (side == 1 && getenv("SNESRECOMP_CT_FUNCTEST_DUMP")) {
        char path[256]; snprintf(path, sizeof path, "%s/%06X_%u_%c.bin", getenv("SNESRECOMP_CT_FUNCTEST_DUMP"), pc, var, 'i');
        FILE *f = fopen(path, "wb"); if (f) { fwrite(c->ram, 1, 0x20000, f); fclose(f); }
    }
    _exit(0);
}
static int ft_side(int side, uint32_t pc, unsigned var, unsigned fs, const CpuState *base, unsigned seed, FtResult *out)
{
    int fd[2];
    if (pipe(fd)) return -1;
    fflush(NULL);
    pid_t p = fork();
    if (p == 0) { close(fd[0]); ft_child(side, pc, var, fs, base, seed, fd[1]); }
    close(fd[1]);
    memset(out, 0, sizeof *out);
    ssize_t n = read(fd[0], out, sizeof *out);
    close(fd[0]);
    int st = 0; waitpid(p, &st, 0);
    if (n != (ssize_t)sizeof *out) return WIFSIGNALED(st) ? -WTERMSIG(st) : 100 + WEXITSTATUS(st);
    return 0;
}
static void functest_run(void)
{
    FILE *in = fopen(getenv("SNESRECOMP_CT_FUNCTEST"), "r");
    const char *op = getenv("SNESRECOMP_CT_FUNCTEST_OUT");
    FILE *out = op ? fopen(op, "w") : stdout;
    const unsigned seed = getenv("SNESRECOMP_CT_FUNCTEST_SEED") ? (unsigned)atoi(getenv("SNESRECOMP_CT_FUNCTEST_SEED")) : 0;
    const CpuState base = g_cpu;
    unsigned pc, var, fs; char name[128];
    unsigned n = 0, same = 0, diff = 0, inc = 0;
    while (in && fscanf(in, "%x %u %u %127s", &pc, &var, &fs, name) == 4) {
        FtResult a, b;
        const int ra = ft_side(0, pc, var, fs, &base, seed, &a);
        const int rb = ft_side(1, pc, var, fs, &base, seed, &b);
        const char *v;
        if (ra || rb) { v = "INCONCLUSIVE"; inc++; }
        else if (memcmp(&a, &b, sizeof a)) { v = "DIFF"; diff++; }
        else { v = "same"; same++; }
        fprintf(out, "%06X %u %s %s na=%d ni=%d", pc, var, name, v, ra, rb);
        if (!ra && !rb && memcmp(&a, &b, sizeof a))
            fprintf(out, " ram=%d sram=%d A=%04X/%04X X=%04X/%04X Y=%04X/%04X S=%04X/%04X D=%04X/%04X DB=%02X/%02X P=%02X/%02X",
                    a.ram != b.ram, a.sram != b.sram, a.A, b.A, a.X, b.X, a.Y, b.Y, a.S, b.S, a.D, b.D, a.DB, b.DB, a.flags, b.flags);
        fputc('\n', out);
        n++;
    }
    fprintf(out, "# total %u same %u diff %u inconclusive %u\n", n, same, diff, inc);
    if (out != stdout) fclose(out);
    exit(0);
}
#endif

void ct_tr_init(void);
void GameRunOneFrame(void)
{
    ct_tr_init();
    { void dc_frame_hook(unsigned); dc_frame_hook(g_frames_run + 1); }
    if (++g_frames_run == 600 && getenv("SNESRECOMP_SELFTEST"))
        uwnh_selftest(&g_cpu);
    if (getenv("SNESRECOMP_PCLOG") && g_frames_run % 60 == 0)
        fprintf(stderr, "[pclog] f=%u resume=%06X S=%04X D=%04X DB=%02X\n",
                g_frames_run, g_resume_pc, g_cpu.S, g_cpu.D, g_cpu.DB);
    {
        extern int uwnh_ring_dump_request;
        const char *ra = getenv("SNESRECOMP_RINGAT");
        if (ra && (unsigned)atoi(ra) == g_frames_run) { uwnh_ring_dump_request = 1; g_framelog = 3; }
    }
    if (g_framelog > 0) {
        g_framelog--;
        fprintf(stderr, "[framelog] f=%u resume=%06X A=%04X X=%04X Y=%04X S=%04X D=%04X DB=%02X P=%02X nmi=%d\n",
                g_frames_run, g_resume_pc, g_cpu.A, g_cpu.X, g_cpu.Y, g_cpu.S, g_cpu.D, g_cpu.DB, g_cpu.P, g_snes->nmiEnabled);
        fprintf(stderr, "[framelog] stack");
        for (unsigned k = 0; k < 16; k++) fprintf(stderr, " %02X", g_snes->ram[(g_cpu.S + 1 + k) & 0x1FFFF]);
        fprintf(stderr, " inNmi=%d inIrq=%d I=%d\n", g_snes->inNmi, g_snes->inIrq, g_cpu._flag_I);
    }
    { /* SNESRECOMP_UWNH_RAMDUMP=<dir>[,<every>]: WRAM snapshots for RAM hunting. */
        const char *rd = getenv("SNESRECOMP_UWNH_RAMDUMP");
        if (rd) {
            char dir[512]; int every = 60; const char *c = strchr(rd, ',');
            snprintf(dir, sizeof dir, "%.*s", c ? (int)(c - rd) : (int)strlen(rd), rd);
            if (c) every = atoi(c + 1);
            if (every > 0 && g_frames_run % (unsigned)every == 0) {
                char path[600]; snprintf(path, sizeof path, "%s/f%06u.bin", dir, g_frames_run);
                FILE *f = fopen(path, "wb");
                if (f) { fwrite(g_snes->ram, 1, 0x20000, f); fclose(f); }
            }
        }
    }
#ifndef _WIN32
    { static int ftf = -2;
      if (ftf == -2) ftf = getenv("SNESRECOMP_CT_FUNCTEST") ? atoi(getenv("SNESRECOMP_CT_FUNCTEST_FRAME") ? getenv("SNESRECOMP_CT_FUNCTEST_FRAME") : "600") : -1;
      if (ftf >= 0 && g_frames_run == (unsigned)ftf) functest_run(); }
#endif
    const int booting = (g_resume_pc == 0);
    /* Phase-lock the host frame to the PPU field: the vblank edge (and NMI)
     * lands on scanline 225, as on hardware. Chrono Trigger's main loop
     * reads the beam ($C0:5A46 latches OPVCT) and only runs its job queue
     * while it is still in vblank or above line $69; with NMI at an arbitrary
     * line the queue never ran and the intro stayed black. Time the guest
     * left idle before the edge (a WAI or a parked poll) is walked here. */
    if (!booting) {
        snes_sync_master_clock(g_snes, g_cpu.master_cycles);
        const uint32_t idle = snes_master_clocks_until_line(g_snes, 225);
        /* A guest that ran to the deadline overshoots line 225 by a few
         * clocks; walking a whole frame to the next edge would make every
         * iteration two hardware frames long, and the host paces by that
         * length (30 fps). It takes this edge late instead. */
        if (idle && idle < GAME_MASTER_CYCLES_PER_FRAME / 2) {
            g_cpu.master_cycles += idle;
            snes_sync_master_clock(g_snes, g_cpu.master_cycles);
        }
    }
    /* The guest's slice ends at the next line-225 edge, so the following
     * NMI stays on the vblank line instead of drifting with overshoot. */
    uint64_t frame_end = g_cpu.master_cycles + GAME_MASTER_CYCLES_PER_FRAME;
    if (!booting) {
        const uint32_t to_edge = snes_master_clocks_until_line(g_snes, 225);
        if (to_edge > GAME_MASTER_CYCLES_PER_FRAME / 2)
            frame_end = g_cpu.master_cycles + to_edge;
    }
    int slice;

    if (booting)
        g_resume_pc = reset_vector();

    /* Vblank edge. NMITIMEN gates it: delivering before the guest has enabled
     * NMI would land an interrupt frame in the middle of its SEI boot
     * sequence. Nothing is delivered on the very first frame either — reset
     * has not run yet, so there is no instruction stream to interrupt. */
    if (!booting && g_snes->nmiEnabled) {
        extern unsigned g_rdnmi_reads;
        const unsigned reads = g_rdnmi_reads;
        g_snes->inNmi = true;
        game_run_interrupt(nmi_vector(), frame_end);
        /* RDNMI stays latched until read: a handler that never touches
         * $4210 leaves it set for a main-loop poll later in vblank. */
        g_snes->inNmi = (g_rdnmi_reads == reads) || resume_is_rdnmi_poll(g_resume_pc);
        if (getenv("SNESRECOMP_CT_NMIDBG")) fprintf(stderr, "[nmidbg] f=%u resume=%06X reads=%u poll=%d inNmi=%d v=%u\n", g_frames_run, g_resume_pc, g_rdnmi_reads - reads, resume_is_rdnmi_poll(g_resume_pc), g_snes->inNmi, g_snes->vPos);
    } else if (!booting) {
        /* RDNMI ($4210 bit 7) latches at every vblank edge whether or not
         * NMI is enabled, and stays set until read. Chrono Trigger's boot
         * ($C0:0B38) polls it with NMI still off. */
        g_snes->inNmi = true;
    }
    /* Everything the vblank-edge work left is this frame's line-0 state;
     * register writes the guest makes while the beam is on lines 1-224 are
     * journaled and replayed per line by GameDrawPpuFrame. Chrono Trigger's
     * raster IRQ forces blank at the bottom of the picture and the field
     * code switches HDMA off mid-frame; drawn from the end-of-frame state,
     * those frames came out black. */
    ppu_rasterBegin(g_ppu);
    dma_line0Arm();

    /* Run the guest until it parks on a read-only poll (its vblank wait) or
     * the frame's clock runs out. A single call is not enough: the guest
     * typically parks several times per frame — on HVBJOY, on a DMA-complete
     * flag, on its own state machine — and each park needs either an
     * interrupt or simply more time. */
    if (fiber_enabled()) {
        g_fiber_frame_end = frame_end;
        for (slice = 0; slice < GAME_MAX_SLICES_PER_FRAME; slice++) {
            if (g_cpu.master_cycles >= frame_end)
                break;
            fiber_switch_to_game();
            if (g_fiber_suspended)
                break;
            if (g_snes->inIrq && !g_cpu._flag_I) {
                game_run_interrupt(irq_vector(), frame_end);
                continue;
            }
            if (g_fiber_took_wai) {
                if (wai_reaches_irq(frame_end)) {
                    game_run_interrupt(irq_vector(), frame_end);
                    continue;
                }
                break;
            }
        }
        return;
    }

    for (slice = 0; slice < GAME_MAX_SLICES_PER_FRAME; slice++) {
        if (g_cpu.master_cycles >= frame_end)
            break;
        interp_bridge_set_master_deadline(frame_end);
        interp_bridge_run_until_quiescent(&g_cpu, g_resume_pc);
        interp_bridge_set_master_deadline(0);
        {
            uint32_t resume = interp_bridge_lle_resume_pc();
            if (resume)
                g_resume_pc = resume;
        }

        /* A raster IRQ asserted while the guest ran: service it before
         * continuing, exactly as the CPU samples it between instructions. */
        if (g_snes->inIrq && !g_cpu._flag_I) {
            game_run_interrupt(irq_vector(), frame_end);
            continue;
        }
        /* Parked with no interrupt pending and clock left over: the guest is
         * waiting for the next vblank. Nothing more happens this frame. */
        if (interp_bridge_lle_took_wai()) {
            if (wai_reaches_irq(frame_end)) {
                game_run_interrupt(irq_vector(), frame_end);
                continue;
            }
            break;
        }
    }
}

void GameDrawPpuFrame(void)
{
    SimpleHdma hdma_chans[8];
    Dma *dma = g_snes->dma;
    uint8_t hdmaen, v;
    int line, ch;

    /* Back to the line-0 register state, then replay the journal per line.
     * The raster IRQ is not run here: the beam already delivered it while
     * the guest ran, and its register writes are in the journal. */
    static int noraster = -1;
    if (noraster < 0) noraster = getenv("SNESRECOMP_CT_NORASTER") != NULL;
    if (!noraster) ppu_rasterRenderBegin(g_ppu);
    hdmaen = noraster ? g_snesrecomp_last_hdmaen : ppu_rasterBaseHdmaen();
    DmaChannel line0[8], endch[8];
    static int nol0 = -1;
    if (nol0 < 0) nol0 = getenv("SNESRECOMP_CT_NOLINE0DMA") != NULL;
    const int swapped = !nol0 && dma_line0Get(line0);
    if (swapped) {
        memcpy(endch, dma->channel, sizeof endch);
        memcpy(dma->channel, line0, sizeof line0);
    }
    dma_startDma(dma, hdmaen, true);
    for (ch = 0; ch < 8; ch++)
        SimpleHdma_Init(&hdma_chans[ch], &dma->channel[ch]);

    /* From line 0, not line 1: starting at 1 leaves the top scanline holding
     * the previous frame's state, which shows up as a stripe of stale
     * tilemap above a HUD. */
    for (line = 0; line <= 224; line++) {
        if (!noraster) ppu_rasterApplyLine(g_ppu, line);
        if (ppu_rasterTakeHdmaen(&v)) {
            /* HDMAEN written mid-frame: a channel switched on starts its table
             * from the top, one switched off stops. */
            const uint8_t on = (uint8_t)(v & ~hdmaen);
            if (on) {
                /* Set up mid-frame: the CPU's registers are the ones meant. */
                if (swapped)
                    for (ch = 0; ch < 8; ch++)
                        if (on & (1 << ch))
                            dma->channel[ch] = endch[ch];
                dma_startDma(dma, on, true);
                for (ch = 0; ch < 8; ch++)
                    if (on & (1 << ch))
                        SimpleHdma_Init(&hdma_chans[ch], &dma->channel[ch]);
            }
            hdmaen = v;
        }
        /* HDMA runs in the H-blank that ends each line, so a table entry
         * reaches the next one: the first transfer (line 0's H-blank) is what
         * line 1, the top visible row, shows. Running it before the line drew
         * every split one line early: Chrono Trigger's dialogue box puts a
         * one-line black entry above and below the box, which came out as a
         * stripe of box gradient instead. */
        static int hdma_early = -1;
        if (hdma_early < 0) hdma_early = getenv("SNESRECOMP_CT_HDMA_EARLY") != NULL;
        if (hdma_early)
            for (ch = 0; ch < 8; ch++)
                if (hdmaen & (1 << ch))
                    SimpleHdma_DoLine(&hdma_chans[ch]);
        { static int oldirq = -1; static int trig = -2;
          if (oldirq < 0) oldirq = getenv("SNESRECOMP_CT_RENDERIRQ") != NULL;
          if (oldirq) {
            if (line == 0) trig = g_snes->vIrqEnabled ? (int)g_snes->vTimer : -1;
            if (line == trig) {
              g_snes->inIrq = true;
              game_run_interrupt(irq_vector(), g_cpu.master_cycles + GAME_MASTER_CYCLES_PER_FRAME);
              g_snes->inIrq = false;
              trig = g_snes->vIrqEnabled ? (int)g_snes->vTimer : -1;
            } } }
        ppu_runLine(g_ppu, line);
        if (!hdma_early)
            for (ch = 0; ch < 8; ch++)
                if (hdmaen & (1 << ch))
                    SimpleHdma_DoLine(&hdma_chans[ch]);
    }
    /* Channels the CPU rewrote mid-frame keep the CPU's values. */
    if (swapped)
        for (ch = 0; ch < 8; ch++)
            if (memcmp(&line0[ch], &endch[ch], sizeof endch[ch]) != 0)
                dma->channel[ch] = endch[ch];
}

/* Save states: the guest snapshot cannot hold where the frame loop resumes
 * the guest, so it rides along as a game chunk. Without it a loaded state
 * would resume at whatever PC the running session had parked on. */
/* The guest snapshot covers the interpreter's Cpu, not the recompiled
 * CpuState that GameRunOneFrame actually resumes with, so the registers the
 * main loop parks on travel in this chunk alongside the resume PC. */
#define UWNH_STATE_MAGIC 0x33574E55u /* 'UNW3' */
typedef struct UwnhStateExtra {
    uint32_t magic, resume_pc;
    uint16_t A, X, Y, S, D;
    uint8_t DB, PB, P, emulation;
} UwnhStateExtra;

static void GameStateSaveExtra(struct SaveLoadInfo *sli)
{
    /* A chain parked on the game fiber is saved as what an unwind would have
     * left: the guest resuming at the parked poll's PC with the parked
     * registers. The guest stack already holds every return frame, so a load
     * continues through the interpreter exactly as the unwind model would.
     * (Unwinding here instead would run guest code after the snapshot's
     * memory was already written.) */
    const int parked = g_fiber_mode > 0 && g_fiber_suspended && g_parked_pc;
    CpuState pr = g_parked_regs;
    cpu_mirrors_to_p(&pr);   /* compiled code keeps flags in the mirrors */
    const CpuState *r = parked ? &pr : &g_cpu;
    UwnhStateExtra v = { UWNH_STATE_MAGIC, parked ? g_parked_pc : g_resume_pc,
                         r->A, r->X, r->Y, r->S, r->D, r->DB, r->PB, r->P, r->emulation };
    /* The interpreter bridge keeps its last yield PC (and bus/deadline
     * bookkeeping) between frames; GameRunOneFrame falls back to that PC when
     * a slice ends without a fresh yield, so it is part of the resume state. */
    uint32_t bsz = (uint32_t)interp_bridge_rb_state_size();
    void *bridge = calloc(1, bsz);
    interp_bridge_rb_state_save(bridge);
    sli->func(sli, &v, sizeof v);
    sli->func(sli, &bsz, sizeof bsz);
    sli->func(sli, bridge, bsz);
    free(bridge);
}

static void GameStateLoadExtra(struct SaveLoadInfo *sli, uint32_t version)
{
    UwnhStateExtra v;
    (void)version;
    fiber_discard();
    memset(&v, 0, sizeof v);
    sli->func(sli, &v, sizeof v);
    if (v.magic != UWNH_STATE_MAGIC && v.magic != 0x32574E55u /* 'UNW2': no bridge blob */)
        return;
    if (v.magic != UWNH_STATE_MAGIC) {
        interp_bridge_set_lle_resume_pc(v.resume_pc);
    } else {
        uint32_t bsz = 0;
        sli->func(sli, &bsz, sizeof bsz);
        if (bsz == (uint32_t)interp_bridge_rb_state_size()) {
            void *bridge = calloc(1, bsz);
            sli->func(sli, bridge, bsz);
            interp_bridge_rb_state_load(bridge);
            free(bridge);
        } else {
            interp_bridge_set_lle_resume_pc(v.resume_pc);
        }
    }
    g_resume_pc = v.resume_pc;
    g_cpu.A = v.A; g_cpu.X = v.X; g_cpu.Y = v.Y; g_cpu.S = v.S; g_cpu.D = v.D;
    g_cpu.DB = v.DB; g_cpu.PB = v.PB; g_cpu.P = v.P; g_cpu.emulation = v.emulation;
    cpu_p_to_mirrors(&g_cpu);
    if (getenv("SNESRECOMP_PCLOG"))
        g_framelog = 4;
    if (getenv("SNESRECOMP_PCLOG"))
        fprintf(stderr, "[state] load resume=%06X S=%04X P=%02X\n", g_resume_pc, g_cpu.S, g_cpu.P);
}

/* Reset hotkey: the machine restarts underneath us, so boot from the reset
 * vector again instead of resuming the old main loop. */
static void GameHardwareReset(void)
{
    fiber_discard();
    g_resume_pc = 0;
}

const RtlGameInfo kGameInfo = {
    .title = "chronotrigger",
    .initialize = NULL,
    .run_frame = &GameRunOneFrame,
    .draw_ppu_frame = &GameDrawPpuFrame,
    .save_name_prefix = "save",
    .state_save_extra = &GameStateSaveExtra,
    .state_load_extra = &GameStateLoadExtra,
    .hardware_reset = &GameHardwareReset,
};

void GameSessionReset(void)
{
    /* Rematch / soft-return: clear anything that must not survive a new
     * session. recomp-ai-rules/NETPLAY.md §3 — sticky state that "has always
     * been fine" is the usual desync culprit, because single-player never
     * re-enters the boot path twice in one process. */
    fiber_discard();
    g_resume_pc = 0;
}

#ifdef SNESRECOMP_COVERAGE
/* Coverage build (-finstrument-functions on the generated banks): records
 * every distinct function entered, written on exit as offsets from
 * GameRunOneFrame so they map back through `nm`. */
#define COV_SLOTS (1u << 17)
static void *g_cov[COV_SLOTS];
__attribute__((no_instrument_function)) static void cov_dump(void)
{
    const char *p = getenv("SNESRECOMP_COV_OUT");
    FILE *f = p ? fopen(p, "w") : NULL;
    if (!f) return;
    for (unsigned i = 0; i < COV_SLOTS; i++)
        if (g_cov[i]) fprintf(f, "%llx\n", (unsigned long long)((char *)g_cov[i] - (char *)GameRunOneFrame) & 0xffffffffffffull);
    fclose(f);
}
__attribute__((no_instrument_function)) void __cyg_profile_func_enter(void *fn, void *site)
{
    static int armed;
    (void)site;
    if (!armed) { armed = 1; atexit(cov_dump); }
    unsigned h = (unsigned)(((uintptr_t)fn >> 4) * 2654435761u) & (COV_SLOTS - 1);
    while (g_cov[h] && g_cov[h] != fn) h = (h + 1) & (COV_SLOTS - 1);
    g_cov[h] = fn;
}
__attribute__((no_instrument_function)) void __cyg_profile_func_exit(void *fn, void *site) { (void)fn; (void)site; }
#endif
