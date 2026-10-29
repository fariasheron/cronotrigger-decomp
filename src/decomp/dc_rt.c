/* Chrono Trigger decomp: runtime glue and the differential check.
 *
 * CT_DECOMP_CHECK=1 (Linux developer builds): on a sampled call of a
 * decompiled routine, fork; the child runs the ORIGINAL routine in the
 * interpreter (pure 65816, no generated or decompiled code) from the same
 * machine state and pipes back WRAM, SRAM and registers. The parent runs
 * the decompiled C, then compares. Differences go to CT_DECOMP_LOG
 * (default dc_check.log) with the differing addresses; a per-routine
 * summary is written at exit. The game always continues with the
 * decompiled result, so a run also shows the C works in place. */
#include "dc.h"
#include <stdio.h>
#include <stdlib.h>
#include "snes/snes.h"
#include "common_cpu_infra.h"

extern Snes *g_snes;
void interp_bridge_set_master_deadline(uint64_t master_clock);
extern int (*interp_bridge_deadline_fiber_hook)(void);

void dc_assume_d_failed(u32 pc, u16 want)
{
    static u32 seen[256]; static unsigned n;
    for (unsigned i = 0; i < n; i++) if (seen[i] == pc) return;
    if (n < 256) seen[n++] = pc;
    fprintf(stderr, "[decomp] %06X entered with D=%04X, written for D=%04X\n", pc, g_cpu.D, want);
}

void dc_assume_m_failed(u32 pc, int want)
{
    static u32 seen[256]; static unsigned n;
    for (unsigned i = 0; i < n; i++) if (seen[i] == pc) return;
    if (n < 256) seen[n++] = pc;
    fprintf(stderr, "[decomp] %06X entered with m=%d, written for m=%d\n", pc, g_cpu.m_flag, want);
}

void dc_assume_x_failed(u32 pc, int want)
{
    static u32 seen[256]; static unsigned n;
    for (unsigned i = 0; i < n; i++) if (seen[i] == pc) return;
    if (n < 256) seen[n++] = pc;
    fprintf(stderr, "[decomp] %06X entered with x=%d, written for x=%d\n", pc, g_cpu.x_flag, want);
}

/* Every DC_ENTRY registers itself (constructor) for the fuzz harness. */
static struct { u32 pc; RecompReturn (*fn)(CpuState *); } s_reg[4096];
static unsigned s_nreg;
void dc_register(u32 pc, RecompReturn (*fn)(CpuState *))
{
    if (s_nreg < 4096) { s_reg[s_nreg].pc = pc; s_reg[s_nreg].fn = fn; s_nreg++; }
}
static RecompReturn (*dc_lookup(u32 pc))(CpuState *)
{
    for (unsigned i = 0; i < s_nreg; i++) if (s_reg[i].pc == pc) return s_reg[i].fn;
    return NULL;
}

void dc_sync_p(void) { cpu_mirrors_to_p(&g_cpu); }
void dc_set_mx(int m, int x)
{
    g_cpu.m_flag = (u8)m; g_cpu.x_flag = (u8)x;
    if (x) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
    cpu_mirrors_to_p(&g_cpu);
}

void dc_push8(u8 v)   { cpu_write8(&g_cpu, 0, g_cpu.S, v); g_cpu.S--; }
void dc_push16(u16 v) { dc_push8((u8)(v >> 8)); dc_push8((u8)v); }
u8   dc_pull8(void)   { g_cpu.S++; return cpu_read8(&g_cpu, 0, g_cpu.S); }
u16  dc_pull16(void)  { u8 lo = dc_pull8(); return (u16)(lo | dc_pull8() << 8); }

static void dc_call(u32 target, u32 site, u8 size)
{
    u16 s0 = g_cpu.S;
    u8 pb = g_cpu.PB;
    /* JSR abs (3 bytes) pushes site+2; JSL long (4 bytes) pushes PB, site+3. */
    if (size == 3) dc_push8(pb);
    dc_push16((u16)(site + (size == 3 ? 3 : 2)));
    if (size == 3) g_cpu.PB = (u8)(target >> 16);
    RecompReturn r = cpu_dispatch_call_pc_pushed(&g_cpu, target, site, size, NULL);
    if (r != RECOMP_RETURN_NORMAL || g_cpu.S != s0) {
        fprintf(stderr, "[decomp] call %06X from %06X returned %d S=%04X (expected %04X)\n",
                target, site, (int)r, g_cpu.S, s0);
        abort();
    }
    g_cpu.PB = pb;
}
void dc_jsr(u32 target, u32 site) { dc_call(((u32)g_cpu.PB << 16) | (target & 0xFFFF), site, 2); }
void dc_jsl(u32 target, u32 site) { dc_call(target, site, 3); }

/* A pending LLE yield unwind from a dc_jsr_y / dc_jsl_y callee, handed on by
 * the next dc_leave (that of the routine making the call). */
static RecompReturn s_unwind;
static int s_unwind_depth;
static int dc_depth(void);
static int dc_call_y(u32 target, u32 site, u8 size)
{
    u16 s0 = g_cpu.S;
    u8 pb = g_cpu.PB;
    if (size == 3) dc_push8(pb);
    dc_push16((u16)(site + (size == 3 ? 3 : 2)));
    if (size == 3) g_cpu.PB = (u8)(target >> 16);
    RecompReturn r = cpu_dispatch_call_pc_pushed(&g_cpu, target, site, size, NULL);
    /* Every level on the way up returns r - 1 (compiled bodies and dc_leave
     * alike), so a sentinel from below arrives decayed under BASE; genuine
     * SKIP_N values are tiny. */
    if ((unsigned)r >= RECOMP_RETURN_LLE_UNWIND_BASE / 2 && interp_bridge_in_lle_scheduler()) {
        /* The guest is mid-callee with our return address on its stack; the
         * scheduler resumes it there and returns into the original at site. */
        s_unwind = r; s_unwind_depth = dc_depth();
        return 1;
    }
    if (r != RECOMP_RETURN_NORMAL || g_cpu.S != s0) {
        fprintf(stderr, "[decomp] call %06X from %06X returned %d S=%04X (expected %04X)\n",
                target, site, (int)r, g_cpu.S, s0);
        abort();
    }
    g_cpu.PB = pb;
    return 0;
}
int dc_jsr_y(u32 target, u32 site) { return dc_call_y(((u32)g_cpu.PB << 16) | (target & 0xFFFF), site, 2); }
int dc_jsl_y(u32 target, u32 site) { return dc_call_y(target, site, 3); }

extern void (*g_func_entry_hook)(uint32_t pc24);
/* Fuzz: S after the harness pushed its return frame (0 outside the fuzz), and
 * where the routine's own RTS/RTL returned to (0: not seen). */
static u16 s_fuzz_ret_s;
static u32 s_fuzz_ret_pc;
/* A routine reached by a generated tail jump inherits its caller's frame. */
static void dc_take_context(DcFrame *f)
{
    u16 es = g_cpu.S; u8 hrv = g_cpu.host_return_valid;
    if (cpu_take_tailcall_return_context(&es, &hrv)) {
        g_cpu.host_return_valid = hrv; f->entry_s = es; f->hrv = hrv;
    }
}
int dc_check_mode(void);
/* Entries whose hex pc starts with one of these prefixes run the original
 * code in the interpreter instead of the C. Default "C7": the sound driver
 * handshakes, whose sound CPU side only keeps up with the original's
 * instruction-by-instruction pace (the C sends packets faster and the sound CPU
 * loses sample bytes: noise and a high whine instead of music). Their C is
 * still what the fuzz (CT_DECOMP_FUZZ) and the in-game check compare.
 * CT_DECOMP_ORIGINAL_PFX=C7,C03C overrides the list (bisecting); "-" = none. */
static int dc_orig_listed(u32 pc)
{
    static const char *pfx = (const char *)1;
    if (pfx == (const char *)1) {
        pfx = getenv("CT_DECOMP_ORIGINAL_PFX");
        if (!pfx) pfx = (getenv("CT_DECOMP_FUZZ") || dc_check_mode()) ? "" : "C7";
    }
    if (!pfx || !*pfx) return 0;
    char h[8]; snprintf(h, sizeof h, "%06X", (unsigned)pc);
    for (const char *q = pfx; *q; ) {
        size_t n = strcspn(q, ",");
        if (n && n <= 6 && !strncmp(h, q, n)) return 1;
        q += n; if (*q) q++;
    }
    return 0;
}
#ifdef _WIN32
static int dc_depth(void) { return 0; }
static void dc_cancel_check(DcFrame *f) { (void)f; }
void dc_frames_passed(void) {}
int dc_check_mode(void) { return 0; }
void dc_enter(DcFrame *f)
{
    f->check_fd = -1; dc_take_context(f);
    if (g_func_entry_hook) g_func_entry_hook(f->pc);
    if (dc_orig_listed(f->pc)) f->check_fd = -2;
}
void dc_frame_hook(unsigned frame) { (void)frame; }
RecompReturn dc_run_original(DcFrame *f)
{
    g_cpu.S = f->entry_s; g_cpu.host_return_valid = f->hrv;
    return interp_tier_run_call_frame(&g_cpu, f->pc, f->pc, f->size, NULL);
}
int g_dc_vscript = -1;
u8 dc_vscript_read(void) { return 0; }   /* never: g_dc_vscript stays -1 */
u8 dc_beam_latch(void) { return abs8(0x2137); }
#else
#include <unistd.h>
#include <sys/wait.h>
#include <signal.h>
#include <errno.h>

typedef struct {
    u16 A, X, Y, S, D; u8 DB, PB, P, m, x, e, ok;
    u8 ram[0x20000];
    u8 sram[0x2000];
    /* video memory and DMA registers, for routines that talk to the PPU */
    u16 vram[0x8000], cgram[0x100], oam[0x100]; u8 high_oam[0x20];
    u16 vram_ptr; u8 cgram_ptr, oam_adr, vram_inc;
    DmaChannel dma[8];
    /* hardware multiply / divide inputs and results (4202..4206, 4214..4217) */
    u8 mul_a; u16 mul_r, div_a, div_r;
    /* last value the routine wrote to each register 2100-21FF / 4200-42FF (fuzz only) */
    u8 hwreg[0x200], hwset[0x200];
    u16 hw_in_wram;   /* fuzz: a PPU port / DMA enable address the decomp stored to in WRAM */
    u32 ret_pc;       /* fuzz: where the routine returned to (0: not seen; the stack may be overwritten) */
    uint64_t cycles;  /* check: master cycles the original took (reference run) */
    u16 lat_h, lat_v; /* beam counters 2137 last latched (not compared: the latch replay) */
} DcResult;

typedef struct { u32 pc; unsigned calls, checked, bad, fail, ncyc, timing, unwound, framed; uint64_t cyc; } DcStat;
static DcStat s_stats[4096];
static unsigned s_nstats;
static int s_mode = -1, s_depth;
/* Host frames passed during the running check (dc_frames_passed). */
static int s_frames_passed;
static int dc_depth(void) { return s_depth; }
static FILE *s_log;

static DcStat *dc_stat(u32 pc)
{
    for (unsigned i = 0; i < s_nstats; i++) if (s_stats[i].pc == pc) return &s_stats[i];
    if (s_nstats == 4096) return &s_stats[4095];
    s_stats[s_nstats].pc = pc;
    return &s_stats[s_nstats++];
}
static void dc_summary(void)
{
    if (!s_log) return;
    fprintf(s_log, "# summary pc calls checked bad fail timing\n");
    for (unsigned i = 0; i < s_nstats; i++)
        fprintf(s_log, "S %06X %u %u %u %u %u\n", s_stats[i].pc, s_stats[i].calls,
                s_stats[i].checked, s_stats[i].bad, s_stats[i].fail, s_stats[i].timing);
    /* calls whose check was dropped because the routine went on in the original (LLE unwind) */
    for (unsigned i = 0; i < s_nstats; i++)
        if (s_stats[i].unwound) fprintf(s_log, "U %06X %u\n", s_stats[i].pc, s_stats[i].unwound);
    /* calls whose check was dropped because host frames passed inside them (dc_frames_passed) */
    for (unsigned i = 0; i < s_nstats; i++)
        if (s_stats[i].framed) fprintf(s_log, "N %06X %u\n", s_stats[i].pc, s_stats[i].framed);
    /* the original's master cycles per checked call: tools/decomp/mkcost.py -> dc_cost.inc */
    for (unsigned i = 0; i < s_nstats; i++)
        if (s_stats[i].ncyc)
            fprintf(s_log, "C %06X %u %llu\n", s_stats[i].pc, s_stats[i].ncyc, (unsigned long long)s_stats[i].cyc);
    fclose(s_log); s_log = NULL;
}
int dc_check_mode(void)
{
    if (s_mode < 0) {
        s_mode = getenv("CT_DECOMP_CHECK") && atoi(getenv("CT_DECOMP_CHECK")) > 0;
        if (s_mode) {
            const char *p = getenv("CT_DECOMP_LOG");
            s_log = fopen(p ? p : "dc_check.log", "w");
            setvbuf(s_log, NULL, _IOLBF, 0);
            atexit(dc_summary);
        }
    }
    return s_mode;
}

static u8 s_hwreg[0x200], s_hwset[0x200];
extern void (*g_hw_write_hook)(u16 addr, u8 v);
static void hw_write_rec(u16 addr, u8 v)
{
    int i = (addr & 0xFF00) == 0x2100 ? addr & 0xFF : (addr & 0xFF00) == 0x4200 ? 0x100 + (addr & 0xFF) : -1;
    if (i >= 0) { s_hwreg[i] = v; s_hwset[i] = 1; }
}
static void dc_snapshot(DcResult *r)
{
    r->A = g_cpu.A; r->X = g_cpu.X; r->Y = g_cpu.Y; r->S = g_cpu.S; r->D = g_cpu.D;
    r->DB = g_cpu.DB; r->PB = g_cpu.PB; r->ret_pc = s_fuzz_ret_pc;
    cpu_mirrors_to_p(&g_cpu);
    r->P = g_cpu.P; r->m = g_cpu.m_flag; r->x = g_cpu.x_flag; r->e = g_cpu.emulation;
    memcpy(r->ram, g_cpu.ram, 0x20000);
    memset(r->sram, 0, sizeof r->sram);
    if (g_snes->cart && g_snes->cart->ram)
        memcpy(r->sram, g_snes->cart->ram, g_snes->cart->ramSize < 0x2000 ? g_snes->cart->ramSize : 0x2000);
    Ppu *ppu = g_snes->ppu;
    memcpy(r->vram, ppu->vram, sizeof r->vram);
    memcpy(r->cgram, ppu->cgram, sizeof r->cgram);
    memcpy(r->oam, ppu->oam, sizeof r->oam);
    memcpy(r->high_oam, ppu->highOam, sizeof r->high_oam);
    r->vram_ptr = ppu->vramPointer; r->cgram_ptr = ppu->cgramPointer;
    r->oam_adr = ppu->oamAdr; r->vram_inc = ppu->vramIncrement;
    memcpy(r->dma, g_snes->dma->channel, sizeof r->dma);
    r->mul_a = g_snes->multiplyA; r->mul_r = g_snes->multiplyResult;
    r->div_a = g_snes->divideA; r->div_r = g_snes->divideResult;
    memcpy(r->hwreg, s_hwreg, sizeof s_hwreg); memcpy(r->hwset, s_hwset, sizeof s_hwset);
    r->hw_in_wram = g_dc_hw_in_wram;
    r->lat_h = ppu->hCount; r->lat_v = ppu->vCount;
    r->ok = 1;
}

static int write_all(int fd, const void *p, size_t n)
{
    const u8 *b = p;
    while (n) {
        ssize_t k = write(fd, b, n);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return -1;
        b += k; n -= (size_t)k;
    }
    return 0;
}
static int read_all(int fd, void *p, size_t n)
{
    u8 *b = p;
    /* A signal (SIGTERM from timeout, SIGINT) must not drop the result: the
     * reference child would then die of SIGPIPE (status 13). */
    while (n) {
        ssize_t k = read(fd, b, n);
        if (k < 0 && errno == EINTR) continue;
        if (k <= 0) return -1;
        b += k; n -= (size_t)k;
    }
    return 0;
}

static void dc_waitpid(pid_t p, int *st)
{
    while (waitpid(p, st, 0) < 0 && errno == EINTR) {}
}

/* fork() with the APU mutex held, so the child's APU/DSP state is not caught
 * half-updated by the SDL audio thread. The child can never take that mutex:
 * it may be held by the audio thread (which does not exist there), and even
 * when this thread holds it, the owner the child sees is another thread id.
 * So in a child every RtlApuLock/RtlApuUnlock is a no-op (the link wraps them,
 * see CMakeLists.txt); a child is single-threaded and needs no lock. */
static int s_forked;
#ifdef DC_WRAP_APU
void __real_RtlApuLock(void);
void __real_RtlApuUnlock(void);
void __wrap_RtlApuLock(void) { if (!s_forked) __real_RtlApuLock(); }
void __wrap_RtlApuUnlock(void) { if (!s_forked) __real_RtlApuUnlock(); }
#define DC_APU_LOCK() __real_RtlApuLock()
#define DC_APU_UNLOCK() __real_RtlApuUnlock()
#else
#define DC_APU_LOCK() ((void)0)
#define DC_APU_UNLOCK() ((void)0)
#endif
static pid_t dc_fork(void)
{
    fflush(NULL);
    if (s_forked) return fork();
    DC_APU_LOCK();
    pid_t p = fork();
    if (p == 0) s_forked = 1;
    else DC_APU_UNLOCK();
    return p;
}
/* The original routine in the interpreter, from the current (entry) state. */
static void dc_reference(const DcFrame *f, DcResult *r)
{
    interp_bridge_set_master_deadline(0);
    interp_bridge_deadline_fiber_hook = NULL;
    cpu_select_interpreted_program();
    uint64_t c0 = g_cpu.master_cycles;
    interp_tier_run_call_frame(&g_cpu, f->pc, f->pc, f->size, NULL);
    dc_snapshot(r);
    r->cycles = g_cpu.master_cycles - c0;
}

/* ---- timing-dependent routines -------------------------------------------
 * Their result depends on where the beam / the sound CPU is when they poll it,
 * and the decompiled C does not spend the original's cycles instruction by
 * instruction, so the check's reference can legitimately end differently.
 * A difference is reported as "T" (timing) instead of "D" only when it is
 * shown to come from the timing read:
 *   regs:   these registers / flags (the value read, a poll count) may differ,
 *           nothing else;
 *   replay 1 (pass count): the decomp is run again from the entry state (a
 *           spare child) with the V counter scripted to pass its target after
 *           n loop passes, n = 0..64; if one n gives exactly the original's
 *           result (memory, video, everything but regs), the difference is
 *           the pass count;
 *   replay 2 (latch): the decomp is run again from the entry state with
 *           every 2137 latch (dc_beam_latch) giving the beam position the
 *           original's run latched; if that gives exactly the original's
 *           result, the difference is where the beam was.
 * Any other difference stays a D. */
enum { TR_A = 1, TR_X = 2, TR_Y = 4, TR_P = 8 };
typedef struct { u32 pc; u8 regs, replay; } DcTimed;
static const DcTimed s_timed[] = {
    { 0x7E31B0, TR_A | TR_P, 0 },          /* waits for vblank end / start (4212) */
    { 0x7E377E, TR_A | TR_X | TR_P, 0 },   /* APU upload poll: X = polls left */
    { 0xC0CA76, TR_A | TR_P, 1 },          /* object queue work until the V counter passes $6B */
    { 0xC70000, TR_A | TR_X | TR_Y | TR_P, 0 },   /* APU handshakes (cost_self_timed.txt) */
    { 0xC70655, TR_A | TR_X | TR_Y | TR_P, 0 },
    { 0xC70922, TR_A | TR_X | TR_Y | TR_P, 0 },
    { 0xC709DA, TR_A | TR_X | TR_Y | TR_P, 0 },
    { 0xC709EA, TR_A | TR_X | TR_Y | TR_P, 0 },
    { 0xC302DD, 0, 2 },                    /* random byte from the beam counters (also 0386) */
    { 0xC3000E, 0, 2 },                    /* ripple step: C302DD picks the table start (0355) */
    { 0xC022D8, 0, 2 },                    /* calls C3000E */
};
static const DcTimed *dc_timed(u32 pc)
{
    for (unsigned i = 0; i < sizeof s_timed / sizeof s_timed[0]; i++) if (s_timed[i].pc == pc) return &s_timed[i];
    return NULL;
}
static int dc_diff(u32 pc, u16 entry_s, const DcResult *gotp, const DcResult *refp, char *buf, size_t cap);
/* got with the registers a timed routine may legitimately end with differently
 * taken from ref; then diff. */
static int dc_diff_timed(const DcTimed *t, u32 pc, u16 entry_s, const DcResult *got, const DcResult *ref, char *buf, size_t cap)
{
    static DcResult g;
    g = *got;
    if (t->regs & TR_A) g.A = ref->A;
    if (t->regs & TR_X) g.X = ref->X;
    if (t->regs & TR_Y) g.Y = ref->Y;
    if (t->regs & TR_P) g.P = (u8)((g.P & 0x3C) | (ref->P & ~0x3C));   /* N V Z C */
    return dc_diff(pc, entry_s, &g, ref, buf, cap);
}
int g_dc_vscript = -1;
static unsigned s_vread;
static u16 s_vtarget;
static int s_latch_replay;
static u16 s_lat_h, s_lat_v;
u8 dc_beam_latch(void)
{
    u8 v = abs8(0x2137);
    if (s_latch_replay) { g_snes->ppu->hCount = s_lat_h; g_snes->ppu->vCount = s_lat_v; }
    return v;
}
u8 dc_vscript_read(void)
{
    unsigned pass = s_vread / 2, hi = s_vread & 1;
    s_vread++;
    u16 v = pass < (unsigned)g_dc_vscript ? 0x00F0 : s_vtarget;   /* F0: not yet (outside 0..EF) */
    return hi ? (u8)(v >> 8) : (u8)v;
}
static RecompReturn (*dc_lookup(u32 pc))(CpuState *);
/* The spare: forked at entry, waits for the original's result, then replays. */
static void dc_spare_start(DcFrame *f)
{
    int a[2], b[2];
    if (pipe(a)) return;
    if (pipe(b)) { close(a[0]); close(a[1]); return; }
    pid_t p = dc_fork();
    if (p == 0) {
        close(a[1]); close(b[0]);
        static DcResult ref, got;
        if (read_all(a[0], &ref, sizeof ref)) _exit(0);   /* no difference: nothing to do */
        const DcTimed *t = dc_timed(f->pc);
        int found = -1;
        for (int n = 0; n <= (t->replay == 1 ? 64 : 0) && found < 0; n++) {
            int g2[2];
            if (pipe(g2)) break;
            pid_t g = dc_fork();
            if (g == 0) {
                close(g2[0]);
                alarm(5);
                s_mode = 0; s_depth = 1;
                if (t->replay == 1) {
                    g_dc_vscript = n; s_vread = 0;
                    s_vtarget = dp16(0x6B) < 0xF0 ? dp16(0x6B) : 0x00F0;
                } else {
                    s_latch_replay = 1; s_lat_h = ref.lat_h; s_lat_v = ref.lat_v;
                }
                g_cpu.S = f->entry_s; g_cpu.host_return_valid = f->size;
                dc_lookup(f->pc)(&g_cpu);
                dc_snapshot(&got);
                char buf[2048];
                int ok = dc_diff_timed(t, f->pc, f->entry_s, &got, &ref, buf, sizeof buf) == 0;
                write_all(g2[1], &ok, sizeof ok);
                _exit(0);
            }
            close(g2[1]);
            int ok = 0;
            if (g > 0 && read_all(g2[0], &ok, sizeof ok)) ok = 0;
            close(g2[0]);
            if (g > 0) dc_waitpid(g, NULL);
            if (ok) found = n;
        }
        write_all(b[1], &found, sizeof found);
        _exit(0);
    }
    close(a[0]); close(b[1]);
    if (p < 0) { close(a[1]); close(b[0]); return; }
    f->spare_pid = p; f->spare_w = a[1]; f->spare_r = b[0];
}
static void dc_spare_end(DcFrame *f)
{
    if (f->spare_pid <= 0) return;
    close(f->spare_w); close(f->spare_r);
    dc_waitpid(f->spare_pid, NULL);
    f->spare_pid = -1;
}

/* CT_DECOMP_ORIGINAL=1: every decompiled entry runs the original ROM code in
 * the interpreter instead (the game then runs as without the decomp), and the
 * master cycles of each call are counted ("C" lines at exit). This measures
 * dc_cost.inc. */
static int s_orig = -1;
static int dc_original_mode(void)
{
    if (s_orig < 0) {
        s_orig = getenv("CT_DECOMP_ORIGINAL") && atoi(getenv("CT_DECOMP_ORIGINAL")) > 0;
        if (s_orig && !s_log) {
            const char *p = getenv("CT_DECOMP_LOG");
            s_log = fopen(p ? p : "dc_check.log", "w");
            atexit(dc_summary);
        }
    }
    return s_orig;
}
RecompReturn dc_run_original(DcFrame *f)
{
    DcStat *st = dc_stat(f->pc);
    st->calls++;
    uint64_t c0 = g_cpu.master_cycles;
    g_cpu.S = f->entry_s; g_cpu.host_return_valid = f->hrv;
    RecompReturn r = interp_tier_run_call_frame(&g_cpu, f->pc, f->pc, f->size, NULL);
    st->ncyc++; st->cyc += g_cpu.master_cycles - c0;
    return r;
}
void dc_enter(DcFrame *f)
{
    f->check_fd = -1;
    dc_take_context(f);
    if (g_func_entry_hook) g_func_entry_hook(f->pc);
    if (dc_original_mode() || dc_orig_listed(f->pc)) { f->check_fd = -2; return; }
    if (!dc_check_mode() || s_depth > 0) { s_depth++; return; }
    s_depth++;
    s_frames_passed = 0;
    DcStat *st = dc_stat(f->pc);
    st->calls++;
    if (st->calls > 64 && (st->calls & 31)) return;
    int fd[2];
    if (pipe(fd)) return;
    pid_t p = dc_fork();
    if (p == 0) {
        /* The reference runs in a grandchild; one that times out (alarm, status 14:
         * a hang, or a slow host) is run once more from the same entry state. */
        close(fd[0]);
        static DcResult r;
        int st2 = 0;
        for (int t = 0; t < 2; t++) {
            int g2[2];
            if (pipe(g2)) _exit(2);
            pid_t g = dc_fork();
            if (g == 0) {
                close(g2[0]);
                alarm(5);
                dc_reference(f, &r);
                write_all(g2[1], &r, sizeof r);
                _exit(0);
            }
            close(g2[1]);
            int rc = g < 0 ? -1 : read_all(g2[0], &r, sizeof r);
            close(g2[0]);
            if (g > 0) dc_waitpid(g, &st2);
            if (!rc) { write_all(fd[1], &r, sizeof r); _exit(0); }
            if (!(WIFSIGNALED(st2) && WTERMSIG(st2) == SIGALRM)) break;
        }
        _exit(WIFSIGNALED(st2) ? 100 + WTERMSIG(st2) : 99);
    }
    close(fd[1]);
    if (p < 0) { close(fd[0]); return; }
    f->check_fd = fd[0]; f->pid = p;
    if (dc_timed(f->pc) && dc_timed(f->pc)->replay) dc_spare_start(f);
    st->checked++;
}

/* Outputs callers can read after each routine returns (tools/decomp/liveness.py). */
static const struct { u32 pc; u8 mask; } s_live[] = {
#include "live.inc"
};
enum { L_A = 1, L_X = 2, L_Y = 4, L_N = 8, L_V = 16, L_Z = 32, L_C = 64 };
static u8 dc_live_mask(u32 pc)
{
    unsigned lo = 0, hi = sizeof s_live / sizeof s_live[0];
    while (lo < hi) { unsigned mid = (lo + hi) / 2; if (s_live[mid].pc < pc) lo = mid + 1; else hi = mid; }
    return (lo < sizeof s_live / sizeof s_live[0] && s_live[lo].pc == pc) ? s_live[lo].mask : 0x7F;
}

/* Differences between the decompiled (got) and original (ref) results, as
 * text; returns how many. Stack bytes up to 0x100 below entry_s are scratch. */
static int dc_diff(u32 pc, u16 entry_s, const DcResult *gotp, const DcResult *refp, char *buf, size_t cap)
{
    const DcResult got = *gotp, ref = *refp;
    int n = 0, nd = 0;
    buf[0] = 0;
    const u32 slo = (u32)(entry_s - 0x100) & 0xFFFF, shi = entry_s;
#define DIFF(name, a, b) if ((a) != (b)) { n += snprintf(buf + n, cap - n, " %s=%X/%X", name, (unsigned)(a), (unsigned)(b)); nd++; }
    const u8 live = dc_live_mask(pc);
    u8 pmask = 0x3C;  /* M X D I always; N V Z C when a caller reads them */
    if (live & L_N) pmask |= 0x80;
    if (live & L_V) pmask |= 0x40;
    if (live & L_Z) pmask |= 0x02;
    if (live & L_C) pmask |= 0x01;
    if (live & L_A) DIFF("A", got.A, ref.A);
    if (live & L_X) DIFF("X", got.X, ref.X);
    if (live & L_Y) DIFF("Y", got.Y, ref.Y);
    if (got.ret_pc && ref.ret_pc) DIFF("ret", got.ret_pc, ref.ret_pc);
    DIFF("S", got.S, ref.S); DIFF("D", got.D, ref.D); DIFF("DB", got.DB, ref.DB);
    DIFF("P", got.P & pmask, ref.P & pmask); DIFF("PB", got.PB, ref.PB);
    int nram = 0;
    if (memcmp(got.ram, ref.ram, sizeof got.ram)) for (u32 a = 0; a < 0x20000; a++) {
        if (got.ram[a] == ref.ram[a]) continue;
        if (a >= slo && a <= shi && slo < shi) continue;
        if (nram < 12) n += snprintf(buf + n, cap - n, " %05X=%02X/%02X", a, got.ram[a], ref.ram[a]);
        nram++; nd++;
    }
    if (memcmp(got.sram, ref.sram, sizeof got.sram)) for (u32 a = 0; a < 0x2000; a++) {
        if (got.sram[a] == ref.sram[a]) continue;
        if (nram < 12) n += snprintf(buf + n, cap - n, " sram%04X=%02X/%02X", a, got.sram[a], ref.sram[a]);
        nram++; nd++;
    }
    if (nram) n += snprintf(buf + n, cap - n, " (ram bytes %d)", nram);
    int nv = 0;
#define VDIFF(arr, cnt, tag) if (memcmp(got.arr, ref.arr, sizeof got.arr)) for (u32 a = 0; a < (cnt); a++) if (got.arr[a] != ref.arr[a]) { \
        if (nv < 6) n += snprintf(buf + n, cap - n, " " tag "%04X=%X/%X", a, (unsigned)got.arr[a], (unsigned)ref.arr[a]); nv++; nd++; }
    VDIFF(vram, 0x8000, "vram") VDIFF(cgram, 0x100, "cgram") VDIFF(oam, 0x100, "oam") VDIFF(high_oam, 0x20, "hoam")
#undef VDIFF
    DIFF("vramptr", got.vram_ptr, ref.vram_ptr); DIFF("cgramptr", got.cgram_ptr, ref.cgram_ptr);
    DIFF("oamadr", got.oam_adr, ref.oam_adr); DIFF("vraminc", got.vram_inc, ref.vram_inc);
    for (int i = 0; i < 0x200; i++)
        if (got.hwset[i] != ref.hwset[i] || (got.hwset[i] && got.hwreg[i] != ref.hwreg[i])) {
            n += snprintf(buf + n, cap - n, " reg%04X=%02X/%02X", i < 0x100 ? 0x2100 + i : 0x4100 + i, got.hwreg[i], ref.hwreg[i]); nd++;
        }
    if (memcmp(got.dma, ref.dma, sizeof got.dma)) {   /* as the registers 43x0..43xA read back */
        int nr = 0;
        for (int c = 0; c < 8; c++) {
            u8 rg[2][11];
            for (int w = 0; w < 2; w++) {
                const DmaChannel *d = w ? &ref.dma[c] : &got.dma[c];
                rg[w][0] = (u8)(d->fromB << 7 | d->indirect << 6 | d->unusedBit << 5 | d->decrement << 4 | d->fixed << 3 | (d->mode & 7));
                rg[w][1] = d->bAdr; rg[w][2] = (u8)d->aAdr; rg[w][3] = (u8)(d->aAdr >> 8); rg[w][4] = d->aBank;
                rg[w][5] = (u8)d->size; rg[w][6] = (u8)(d->size >> 8); rg[w][7] = d->indBank;
                rg[w][8] = (u8)d->tableAdr; rg[w][9] = (u8)(d->tableAdr >> 8); rg[w][10] = d->repCount;
            }
            for (int k = 0; k < 11; k++) if (rg[0][k] != rg[1][k]) {
                if (nr < 6) n += snprintf(buf + n, cap - n, " dma%04X=%02X/%02X", 0x4300 + 0x10 * c + k, rg[0][k], rg[1][k]);
                nr++;
            }
        }
        if (!nr) n += snprintf(buf + n, cap - n, " dma-state");   /* internal flags only (active, terminated, ...) */
        nd++;
    }
    DIFF("mula", got.mul_a, ref.mul_a); DIFF("mulr", got.mul_r, ref.mul_r);
    DIFF("diva", got.div_a, ref.div_a); DIFF("divr", got.div_r, ref.div_r);
    if (nv) n += snprintf(buf + n, cap - n, " (video words %d)", nv);
    return nd;
}

/* No end state to compare (the routine is left to the original mid-way by an
 * LLE unwind, or dc_frames_passed): drop the reference run. */
static void dc_cancel_check(DcFrame *f)
{
    if (f->check_fd < 0) return;
    kill(f->pid, SIGKILL);
    close(f->check_fd);
    dc_waitpid(f->pid, NULL);
    f->check_fd = -1;
    dc_spare_end(f);
}
/* A callee let host frames pass (C7:0004 waiting on the sound CPU, run in a
 * nested interpreter frame that parks at the frame deadline): the live run took
 * those frames' NMIs, which the reference (no deadline) does not, so their end
 * states cannot be compared. Only that callee calls this. */
void dc_frames_passed(void) { if (s_depth > 0) s_frames_passed = 1; }
static void dc_compare(DcFrame *f);
/* At the routine's return: compare, unless the check had to be dropped. */
static void dc_finish_check(DcFrame *f)
{
    if (f->check_fd < 0) return;
    if (s_frames_passed) { dc_cancel_check(f); dc_stat(f->pc)->framed++; }
    else dc_compare(f);
}
static void dc_compare(DcFrame *f)
{
    static DcResult ref, got;
    DcStat *st = dc_stat(f->pc);
    int rc = read_all(f->check_fd, &ref, sizeof ref);
    close(f->check_fd);
    int status = 0; dc_waitpid(f->pid, &status);
    if (rc) {
        st->fail++;
        fprintf(s_log, "F %06X reference run failed (status %d%s)\n", f->pc,
                WIFEXITED(status) ? WEXITSTATUS(status) : status,
                WIFEXITED(status) && WEXITSTATUS(status) == 114 ? ": timed out twice" : "");
        dc_spare_end(f);
        return;
    }
    st->ncyc++; st->cyc += ref.cycles;
    dc_snapshot(&got);
    char buf[2048];
    int nd = dc_diff(f->pc, f->entry_s, &got, &ref, buf, sizeof buf);
    const DcTimed *t = nd ? dc_timed(f->pc) : NULL;
    if (t) {
        char b2[2048];
        int replayed = -1;
        int nt = dc_diff_timed(t, f->pc, f->entry_s, &got, &ref, b2, sizeof b2);
        if (nt && t->replay && f->spare_pid > 0 && !write_all(f->spare_w, &ref, sizeof ref)) {
            close(f->spare_w); f->spare_w = -1;
            int n = -1;
            if (!read_all(f->spare_r, &n, sizeof n) && n >= 0) { nt = 0; replayed = n; }
        }
        if (!nt) {
            st->timing++;
            if (st->timing <= 20) {
                fprintf(s_log, "T %06X call %u timing only", f->pc, st->calls);
                if (replayed >= 0 && t->replay == 1)
                    fprintf(s_log, " (the decomp gives the original's result with the V counter passing after %d passes)", replayed);
                if (replayed >= 0 && t->replay == 2)
                    fprintf(s_log, " (the decomp gives the original's result with its latched beam H=%u V=%u)", ref.lat_h, ref.lat_v);
                fprintf(s_log, ":%s\n", buf);
            }
            nd = 0;
        }
    }
    dc_spare_end(f);
    if (nd) {
        st->bad++;
        if (st->bad <= 20)
            fprintf(s_log, "D %06X call %u (decomp/original)%s\n", f->pc, st->calls, buf);
    }
}

/* ---- fuzz harness ------------------------------------------------------
 * CT_DECOMP_FUZZ=<spec file>, CT_DECOMP_FUZZ_FRAME=<n> (default 2000),
 * CT_DECOMP_FUZZ_N=<seeds per routine> (default 200). At frame n, from the
 * live machine state, every routine in the spec runs N times with randomized
 * inputs: once decompiled and once in the interpreter, each in a forked
 * child; results compared like the call check. Spec lines:
 *   <pc24> <m> <x> <ret RTS|RTL> <profile>
 * profiles: regs = random A/X/Y; ev = event command (X = Y = random script
 * position, 24 random script bytes there, random current object $016D);
 * obj = random object index (X = Y = 0..0x7E even) plus regs; dp = random
 * direct-page scratch D+C0..FF and current object. Constraints dOO&XX etc.
 * apply to the direct-page byte D+OO (hex); dOO~MM sets it to a random value
 * masked with MM. A~MM / A=XXXX set A. rAAAAA:N&MM / |MM / +MM: N random
 * WRAM bytes (masked / or-ed half the time / always or-ed). */
static uint64_t s_rng;
static u32 rnd(void) { s_rng ^= s_rng << 13; s_rng ^= s_rng >> 7; s_rng ^= s_rng << 17; return (u32)(s_rng >> 11); }
/* Optional per-routine constraints after the profile, e.g. "b3&07 b4|80":
 * script byte at Y+3 masked with 07, byte at Y+4 or'ed with 80. */
static char s_fuzz_extra[1024];
/* Profile "vm" / "vmloop": a random C3:0B03 script at 7E:2000 (D = 0A00,
 * DB = 7E), a subroutine at 7E:2400 ending in "return", and two preset
 * lists at 0920 / 0940. "vmloop" adds loop commands (8r/9r) back to the
 * start and leaves out list removals (a value removed twice is searched
 * forever). */
static u8 *vm_p;
static void vm_b(u8 b) { *vm_p++ = b; }
static void vm_w(u16 w) { vm_b((u8)w); vm_b((u8)(w >> 8)); }
static u8 vm_hi(u8 n) { return (u8)(0xA0 + 0x10 * (rnd() % 6) + n); }
static u16 vm_list[2][16];
static void vm_arith(void)
{
    u8 op = (u8)(rnd() & 0x7F);
    vm_b(op);
    if (op & 0x20) vm_b((u8)rnd()); else vm_w((u16)rnd());
}
static void vm_emit(int loops, int sub)
{
    switch (rnd() % 18) {
    case 0: case 1: case 2: case 3: case 4: case 5: vm_arith(); break;
    case 6: vm_b(vm_hi(1)); vm_w((u16)(0x0B00 + rnd() % 0x1300)); vm_b((u8)rnd()); break;
    case 7: vm_b(vm_hi(2)); vm_w((u16)(0x0B00 + rnd() % 0x1300)); vm_w((u16)rnd()); break;
    case 8: vm_b(vm_hi(0xB)); vm_b((u8)rnd()); vm_b((u8)(rnd() & 0x3F)); break;
    case 9: vm_b(vm_hi(6)); vm_w((u16)rnd()); vm_w((u16)rnd()); break;
    case 10: vm_b(vm_hi((rnd() & 1) ? 8 : 0xA)); vm_w((u16)(rnd() | 1)); break;
    case 11:
        if (loops) { vm_arith(); break; }
        { int l = rnd() & 1, k = rnd() % 15; vm_b(vm_hi(l ? 7 : 9)); vm_w(vm_list[l][k]); vm_list[l][k] = 0; }
        break;
    case 12: vm_arith(); break;   /* 5 (wait on the sound CPU) depends on its timing */
    case 13: if (sub) { vm_b(vm_hi(3)); vm_w(0x2400); } else vm_arith(); break;
    case 14: vm_b(vm_hi(0)); vm_w((u16)rnd()); break;
    case 15: case 16:
        if (!loops) { vm_arith(); break; }
        vm_b((u8)(0x80 | (rnd() & 0xF))); vm_w((u16)(1 + rnd() % 3)); vm_w(0x2000); break;
    default: { u16 t = (u16)(0x7E0000 + (vm_p - g_cpu.ram) + 3); vm_b(vm_hi(0xE)); vm_w(t); } break;
    }
}
static void vm_script(int loops)
{
    g_cpu.D = 0x0A00; g_cpu.DB = 0x7E;
    for (int k = 0; k < 0x40; k++) g_cpu.ram[0x0A00 + k] = (rnd() & 1) ? (u8)rnd() : 0;
    if (loops || (rnd() & 1)) for (int k = 0x22; k < 0x40; k++) g_cpu.ram[0x0A00 + k] = 0;   /* loop counters idle */
    g_cpu.ram[0x0A20] = 0x00;
    g_cpu.ram[0x0A20] = 0x00; g_cpu.ram[0x0A21] = 0x20;
    g_cpu.ram[0x37F3] = 0x6B;   /* the WRAM routine command 0 calls (only loaded in its mode): RTL */
    g_cpu.ram[0x0388] = (u8)(rnd() | 1);   /* sound command queued: no call to the sound driver */
    for (int l = 0; l < 2; l++)
        for (int k = 0; k < 16; k++) {
            u16 v = (rnd() & 3) ? (u16)(rnd() | 1) : 0;
            vm_list[l][k] = v;
            g_cpu.ram[0x0920 + l * 0x20 + k * 2] = (u8)v; g_cpu.ram[0x0921 + l * 0x20 + k * 2] = (u8)(v >> 8);
        }
    g_cpu.ram[0x095E] = g_cpu.ram[0x095F] = 0;   /* a free slot in each list */
    g_cpu.ram[0x093E] = g_cpu.ram[0x093F] = 0;
    vm_p = &g_cpu.ram[0x2400];
    for (int n = rnd() % 4; n--; ) vm_arith();
    vm_b(vm_hi(4));
    vm_p = &g_cpu.ram[0x2000];
    for (int n = 1 + rnd() % 12; n--; ) vm_emit(loops, 1);
    switch (rnd() % 5) {
    case 0: vm_b(vm_hi(0xC)); break;
    case 1: vm_b(vm_hi(0xD)); vm_w(0x2000); break;
    case 2: vm_b(vm_hi(0xF)); break;
    case 3: vm_b(vm_hi(0xC)); break;
    default: vm_b((u8)(0x90 | (rnd() & 0xF))); vm_w((u16)(1 + rnd() % 3)); vm_w(0x2000); vm_b(vm_hi(0xC)); break;
    }
}

static void fuzz_constraints(void)
{
    char buf[1024]; snprintf(buf, sizeof buf, "%s", s_fuzz_extra);
    for (char *t = strtok(buf, " \t\n"); t; t = strtok(NULL, " \t\n")) {
        unsigned off, v; char opc;
        if (sscanf(t, "D=%x", &v) == 1) { g_cpu.D = (u16)v; continue; }
        if (sscanf(t, "C=%x", &v) == 1) { g_cpu._flag_C = (u8)(v & 1); continue; }   /* carry on entry */
        if (sscanf(t, "V=%x", &v) == 1) { g_cpu._flag_V = (u8)(v & 1); continue; }   /* V on entry */
        if (sscanf(t, "N=%x", &v) == 1) { g_cpu._flag_N = (u8)(v & 1); continue; }   /* N on entry (BPL/BMI first) */
        unsigned a0, cnt, mask = 0xFF;   /* rAAAAA:N[&MM]  N random bytes at WRAM AAAAA (a zero now and then) */
        if (sscanf(t, "r%x:%x|%x", &a0, &cnt, &mask) == 3) {   /* rAAAAA:N|MM  same, or'ed with MM half the time */
            for (unsigned k = 0; k < cnt; k++) { u8 b = (u8)rnd(); g_cpu.ram[(a0 + k) & 0x1FFFF] = (rnd() & 1) ? (u8)(b | mask) : b; }
            continue;
        }
        if (sscanf(t, "r%x:%x+%x", &a0, &cnt, &mask) == 3) {   /* rAAAAA:N+MM  random bytes, always or'ed with MM */
            for (unsigned k = 0; k < cnt; k++) g_cpu.ram[(a0 + k) & 0x1FFFF] = (u8)(rnd() | mask);
            continue;
        }
        if (sscanf(t, "r%x:%x&%x", &a0, &cnt, &mask) >= 2) {
            for (unsigned k = 0; k < cnt; k++) g_cpu.ram[(a0 + k) & 0x1FFFF] = (rnd() & 7) ? (u8)(rnd() & mask) : 0;
            continue;
        }
        if (sscanf(t, "v%x:%x", &a0, &cnt) == 2) {   /* vAAAA:N  N random VRAM words from word address AAAA */
            for (unsigned k = 0; k < cnt; k++) g_snes->ppu->vram[(a0 + k) & 0x7FFF] = (u16)rnd();
            continue;
        }
        if (sscanf(t, "c%x:%x", &a0, &cnt) == 2) {   /* cAA:N  N random CGRAM words from colour AA */
            for (unsigned k = 0; k < cnt; k++) g_snes->ppu->cgram[(a0 + k) & 0xFF] = (u16)(rnd() & 0x7FFF);
            continue;
        }
        if (sscanf(t, "o%x:%x", &a0, &cnt) == 2) {   /* oAAA:N  N random OAM words from word AAA (100.. = high table bytes) */
            for (unsigned k = 0; k < cnt; k++) {
                unsigned w = (a0 + k) % 0x110;
                if (w < 0x100) g_snes->ppu->oam[w] = (u16)rnd(); else g_snes->ppu->highOam[(w - 0x100) * 2 & 0x1F] = (u8)rnd();
            }
            continue;
        }
        if (sscanf(t, "s%x:%x=%x", &a0, &cnt, &v) == 3) {   /* sAAAA:N=VV  N SRAM bytes (offset AAAA, 30:6000 = 0) set to VV */
            for (unsigned k = 0; k < cnt; k++) if (g_snes->cart->ramSize) g_snes->cart->ram[(a0 + k) % g_snes->cart->ramSize] = (u8)v;
            continue;
        }
        if (sscanf(t, "s%x=%x", &a0, &v) == 2) {   /* sAAAA=VV  one SRAM byte */
            if (g_snes->cart->ramSize) g_snes->cart->ram[a0 % g_snes->cart->ramSize] = (u8)v;
            continue;
        }
        if (sscanf(t, "r%x=%x", &a0, &v) == 2) { g_cpu.ram[a0 & 0x1FFFF] = (u8)v; continue; }   /* rAAAAA=VV  fixed byte */
        if (sscanf(t, "DB=%x", &v) == 1) { g_cpu.DB = (u8)v; continue; }
        if (sscanf(t, "i%x%c%x", &off, &opc, &v) == 3 && (opc == '=' || opc == '~')) {   /* iRRRR=VV / iRRRR~MM  write an I/O register */
            cpu_write8(&g_cpu, 0, (u16)off, opc == '=' ? (u8)v : (u8)(rnd() & v));
            continue;
        }
        if (sscanf(t, "W=%x", &v) == 1) {   /* WRAM port address */
            cpu_write8(&g_cpu, 0, 0x2181, (u8)v); cpu_write8(&g_cpu, 0, 0x2182, (u8)(v >> 8)); cpu_write8(&g_cpu, 0, 0x2183, (u8)(v >> 16));
            continue;
        }
        if (!strcmp(t, "FB")) { cpu_write8(&g_cpu, 0, 0x2100, 0x80); continue; }   /* forced blank: VRAM writes land */
        if (sscanf(t, "A~%x", &v) == 1) { g_cpu.A = (u16)((g_cpu.A & 0xFF00) | (rnd() & v)); continue; }   /* A low byte random, masked */
        if (sscanf(t, "A=%x", &v) == 1) { g_cpu.A = (u16)v; continue; }
        if (sscanf(t, "X=%x", &v) == 1) { g_cpu.X = (u16)v; continue; }
        if (sscanf(t, "Y=%x", &v) == 1) { g_cpu.Y = (u16)v; continue; }
        if (sscanf(t, "d%x%c%x", &off, &opc, &v) == 3) {   /* direct-page byte D+off */
            u8 *b = &g_cpu.ram[(u16)(g_cpu.D + off)];
            if (opc == '&') *b &= (u8)v; else if (opc == '|') *b |= (u8)v; else if (opc == '=') *b = (u8)v;
            else if (opc == '~') *b = (rnd() & 7) ? (u8)(rnd() & v) : 0;   /* random, masked */
        } else if (sscanf(t, "b%u%c%x", &off, &opc, &v) == 3) {
            u8 *b = &g_cpu.ram[(0x12001 + g_cpu.Y + off) & 0x1FFFF];
            if (opc == '&') *b &= (u8)v; else if (opc == '|') *b |= (u8)v; else if (opc == '=') *b = (u8)v;
        }
    }
}
/* D= / DB= from the spec, taken before the profile so that "dp" randomizes the spec's direct
 * page (the constraints apply them again afterwards). */
static void fuzz_pre_regs(void)
{
    char buf[1024]; snprintf(buf, sizeof buf, "%s", s_fuzz_extra);
    for (char *t = strtok(buf, " \t\n"); t; t = strtok(NULL, " \t\n")) {
        unsigned v;
        if (sscanf(t, "D=%x", &v) == 1) g_cpu.D = (u16)v;
        else if (sscanf(t, "DB=%x", &v) == 1) g_cpu.DB = (u8)v;
    }
}
static void fuzz_inputs(const char *prof, int m, int x)
{
    fuzz_pre_regs();
    g_cpu.A = (u16)rnd(); g_cpu.X = (u16)rnd(); g_cpu.Y = (u16)rnd();
    if (!strcmp(prof, "ev")) {
        u16 pos = (u16)(rnd() % 0x1000);
        g_cpu.X = g_cpu.Y = pos;
        for (int k = 0; k < 24; k++) {
            u8 b = (u8)rnd();
            u32 r = rnd() & 7;
            if (k > 0 && r < 2) b &= 0x7E;        /* plausible object numbers */
            else if (k > 0 && r < 4) b &= 0x07;   /* small values: operators, slots */
            g_cpu.ram[(0x12001 + pos + k) & 0x1FFFF] = b;
        }
        g_cpu.ram[0x016D] = (u8)((rnd() % 0x40) * 2); g_cpu.ram[0x016E] = 0;
        /* buttons (pressed, held, latches) and per-object word tables */
        for (int k = 0; k < 2; k++) {
            g_cpu.ram[0x00F8 + k] = (rnd() & 1) ? (u8)rnd() : 0;
            g_cpu.ram[0x00F2 + k] = (rnd() & 1) ? (u8)rnd() : 0;
            g_cpu.ram[0x0150 + k] = (rnd() & 1) ? (u8)rnd() : 0;
        }
        for (int o = 0; o < 0x80; o++) if ((rnd() & 3) == 0) g_cpu.ram[0x1400 + o] = (u8)rnd();
        /* party: members 0-6 in the three slots, leader object, follow flag */
        if (rnd() & 1) for (int k = 0; k < 3; k++) g_cpu.ram[0x2980 + k] = (rnd() & 3) ? (u8)(rnd() % 7) : 0x80;
        if (rnd() & 1) { g_cpu.ram[0x0197] = (rnd() & 3) ? (u8)((rnd() % 0x40) * 2) : 0x80; g_cpu.ram[0x0198] = 0; }
        g_cpu.ram[0x0178] = (u8)(rnd() & 1);
        g_cpu.ram[0x0129] = (u8)(rnd() & 1);
        g_cpu.ram[0x0117] = (u8)(rnd() & 0xC1); g_cpu.ram[0x0118] = (u8)(rnd() & 0xC1);
        for (int k = 1; k < 3; k++) if (rnd() & 1) { g_cpu.ram[0x0197 + k * 2] = (rnd() & 3) ? (u8)((rnd() % 0x40) * 2) : 0x80; g_cpu.ram[0x0198 + k * 2] = 0; }
        g_cpu.ram[0x103FC] = (rnd() & 1) ? 0 : (u8)(rnd() % 6);
        g_cpu.ram[0x103FE] = (u8)(rnd() % 4);
        g_cpu.ram[0x0154] ^= (u8)(rnd() & 1);
        for (int o = 0; o < 0x80; o += 2) if ((rnd() & 3) == 0) g_cpu.ram[0x10A00 + o] = (u8)(rnd() & 1);
        g_cpu.ram[0x0138] = (rnd() & 3) == 0 ? (u8)rnd() : 0;
        if (rnd() & 1) for (int o = 0; o < 0x80; o += 2) if ((rnd() & 3) == 0) g_cpu.ram[0x1100 + o] = (u8)(rnd() % 6);
        /* object tiles close together so 'arrived' / 'next to' cases happen */
        if (rnd() & 1) for (int o = 0; o < 0x80; o += 2) { g_cpu.ram[0x1801 + o] = (u8)(rnd() & 7); g_cpu.ram[0x1881 + o] = (u8)(rnd() & 7); }
        for (int k = 0; k < 8; k++) if (rnd() & 1) g_cpu.ram[0x0520 + k * 12] = (rnd() & 1) ? (u8)rnd() : 0;
        g_cpu.ram[0x0104] = (u8)rnd();
        /* object state the commands test: priority, call lock, removed /
         * disabled bits, waiting flag, saved positions */
        for (int o = 0; o < 0x80; o += 2) {
            if (rnd() & 1) g_cpu.ram[0x1C00 + o] = (u8)(rnd() & 7);
            if ((rnd() & 7) == 0) g_cpu.ram[0x1C01 + o] ^= 1;
            if ((rnd() & 7) == 0) g_cpu.ram[0x1100 + o] ^= 0x80;
            if ((rnd() & 7) == 0) g_cpu.ram[0x1000 + o] ^= 0x80;
            if ((rnd() & 3) == 0) g_cpu.ram[0x0F80 + o] ^= 0x80;
            if ((rnd() & 3) == 0) g_cpu.ram[0x1C81 + o] ^= (u8)(rnd() & 3);
            if ((rnd() & 3) == 0) {   /* animation and wait state */
                g_cpu.ram[0x10B01 + o] = (u8)(rnd() & 3); g_cpu.ram[0x1780 + o] = (u8)(rnd() & 3);
                g_cpu.ram[0x1680 + o] = (rnd() & 1) ? 0xFF : (u8)(rnd() & 7); g_cpu.ram[0x1781 + o] = (u8)(rnd() & 7);
                g_cpu.ram[0x10A01 + o] = (u8)(rnd() & 7);
            }
            if ((rnd() & 3) == 0) { g_cpu.ram[0x1A80 + o] = (u8)(rnd() & 1); g_cpu.ram[0x1A01 + o] = (rnd() & 1) ? (u8)rnd() : 0; }
            if ((rnd() & 3) == 0) { u16 v = (rnd() & 1) ? 0xFFFF : (u16)((rnd() % 8) * 12); g_cpu.ram[0x10B80 + o] = (u8)v; g_cpu.ram[0x10B81 + o] = (u8)(v >> 8); }
            if ((rnd() & 3) == 0) g_cpu.ram[0x10980 + o] = (u8)(rnd() & 1);
            for (int p = 0; p < 8; p++)
                if ((rnd() & 7) == 0) { g_cpu.ram[0x10580 + p * 0x80 + o] = (rnd() & 1) ? (u8)rnd() : 0; g_cpu.ram[0x10581 + p * 0x80 + o] = 0; }
        }
    } else if (!strcmp(prof, "vm") || !strcmp(prof, "vmloop")) {
        vm_script(!strcmp(prof, "vmloop"));
    } else if (!strcmp(prof, "cmp")) {
        for (int k = 0; k < 4; k++) g_cpu.ram[0x01D9 + k] = (rnd() & 3) ? (u8)rnd() : g_cpu.ram[0x01D9 + (k ^ 2)];
    } else if (!strcmp(prof, "obj")) {
        g_cpu.X = g_cpu.Y = (u16)((rnd() % 0x40) * 2);
    } else if (!strcmp(prof, "dp") || !strcmp(prof, "near")) {
        /* direct-page scratch D+C0..FF random (small values half the time) */
        for (int k = 0xC0; k < 0x100; k++) {
            u8 b = (u8)rnd();
            if (rnd() & 1) b &= 0x3F;
            g_cpu.ram[(u16)(g_cpu.D + k)] = b;
        }
        g_cpu.ram[0x016D] = (u8)((rnd() % 0x40) * 2); g_cpu.ram[0x016E] = 0;
        if (!strcmp(prof, "near")) {
            /* dp E9 = an object, dp DB/DD = its position +- up to 1.5 tiles */
            u8 o = (u8)((rnd() % 0x40) * 2);
            g_cpu.ram[(u16)(g_cpu.D + 0xE9)] = o;
            for (int k = 0; k < 2; k++) {
                u16 p = (u16)(g_cpu.ram[0x1800 + k * 0x80 + o] | g_cpu.ram[0x1801 + k * 0x80 + o] << 8);
                p = (u16)(p + (int)(rnd() % 0x301) - 0x180);
                g_cpu.ram[(u16)(g_cpu.D + 0xDB + k * 2)] = (u8)p;
                g_cpu.ram[(u16)(g_cpu.D + 0xDC + k * 2)] = (u8)(p >> 8);
            }
        }
    }
    fuzz_constraints();
    g_cpu.m_flag = (u8)m; g_cpu.x_flag = (u8)x; g_cpu.emulation = 0;
    if (x) { g_cpu.X &= 0xFF; g_cpu.Y &= 0xFF; }
    cpu_mirrors_to_p(&g_cpu);
}
static void fuzz_child(int side, u32 pc, int m, int x, int ret, const char *prof, uint64_t seed, int fd)
{
    s_mode = 0;
    alarm(3);
    memset(s_hwreg, 0, sizeof s_hwreg); memset(s_hwset, 0, sizeof s_hwset);
    g_hw_write_hook = hw_write_rec;
    interp_bridge_set_master_deadline(0);
    interp_bridge_deadline_fiber_hook = NULL;
    snes_beam_hold(1);
    s_rng = seed * 0x9E3779B97F4A7C15ull + pc; rnd(); rnd();
    fuzz_inputs(prof, m, x);
    g_cpu.PB = (u8)(pc >> 16);
    g_cpu.S = 0x1F80;
    if (ret == 3) dc_push8(0x00);
    dc_push16(0xFFEF);
    s_fuzz_ret_s = g_cpu.S; s_fuzz_ret_pc = 0;
    g_cpu.host_return_valid = (u8)ret;
    g_dc_hw_in_wram = 0;
    if (side == 0) {
        RecompReturn (*fn)(CpuState *) = dc_lookup(pc);
        if (!fn) _exit(3);
        fn(&g_cpu);
    } else {
        cpu_select_interpreted_program();
        /* a step-cap bail (an endless or 2M-step loop) leaves the return PC unset:
           not a result to compare, report the run as inconclusive (status 104) */
        uint32_t rpc = 0xFFFFFFFF;
        interp_tier_run_call_frame(&g_cpu, pc, 0, (u8)ret, &rpc);
        if (rpc == 0xFFFFFFFF) _exit(4);
        s_fuzz_ret_pc = rpc;
    }
    static DcResult r;
    dc_snapshot(&r);
    write_all(fd, &r, sizeof r);
    _exit(0);
}
static int fuzz_side(int side, u32 pc, int m, int x, int ret, const char *prof, uint64_t seed, DcResult *out)
{
    int fd[2];
    if (pipe(fd)) return -1;
    pid_t p = dc_fork();
    if (p == 0) { close(fd[0]); fuzz_child(side, pc, m, x, ret, prof, seed, fd[1]); }
    close(fd[1]);
    int rc = read_all(fd[0], out, sizeof *out);
    close(fd[0]);
    int st = 0; dc_waitpid(p, &st);
    return rc ? (WIFSIGNALED(st) ? -WTERMSIG(st) : 100 + WEXITSTATUS(st)) : 0;
}
static void dc_fuzz_run(void)
{
    FILE *in = fopen(getenv("CT_DECOMP_FUZZ"), "r");
    const char *op = getenv("CT_DECOMP_LOG");
    FILE *out = fopen(op ? op : "dc_fuzz.log", "w");
    int n = getenv("CT_DECOMP_FUZZ_N") ? atoi(getenv("CT_DECOMP_FUZZ_N")) : 200;
    static DcResult a, b;
    unsigned pc; int m, x; char ret[8], prof[32];
    char line[1024];
    while (in && fgets(line, sizeof line, in)) {
        int used = 0;
        if (line[0] == '#' || sscanf(line, "%x %d %d %7s %31s %n", &pc, &m, &x, ret, prof, &used) < 5) continue;
        snprintf(s_fuzz_extra, sizeof s_fuzz_extra, "%s", line + used);
        int r = !strcmp(ret, "RTL") ? 3 : 2, bad = 0, inc = 0; u32 warn = 0;
        int s0 = getenv("CT_DECOMP_FUZZ_S0") ? atoi(getenv("CT_DECOMP_FUZZ_S0")) : 0;
        for (int s = s0; s < n; s++) {
            int ra = fuzz_side(0, pc, m, x, r, prof, (uint64_t)s, &a);
            int rb = fuzz_side(1, pc, m, x, r, prof, (uint64_t)s, &b);
            if (ra || rb) { inc++; if (inc <= 3) fprintf(out, "F %06X seed %d decomp=%d original=%d\n", pc, s, ra, rb); continue; }
            if (a.hw_in_wram && !warn) warn = a.hw_in_wram;
            char buf[2048]; int nd = dc_diff(pc, 0x1F80, &a, &b, buf, sizeof buf);
            if (nd) {
                bad++;
                if (bad <= 5) {
                    /* re-derive this seed's inputs for the report */
                    static u8 save[0x20000]; CpuState c0 = g_cpu;
                    memcpy(save, g_cpu.ram, sizeof save);
                    s_rng = (uint64_t)s * 0x9E3779B97F4A7C15ull + pc; rnd(); rnd();
                    fuzz_inputs(prof, m, x);
                    u16 ix = g_cpu.X, iy = g_cpu.Y, ia = g_cpu.A;
                    char sb[64]; int k = 0;
                    for (int q = 0; q < 8; q++) k += snprintf(sb + k, sizeof sb - k, "%02X", g_cpu.ram[(0x12001 + iy + q) & 0x1FFFF]);
                    g_cpu = c0; memcpy(g_cpu.ram, save, sizeof save);
                    fprintf(out, "D %06X seed %d in A=%04X X=%04X Y=%04X bytes@Y=%s:%s\n", pc, s, ia, ix, iy, sb, buf);
                }
            }
        }
        if (warn) fprintf(out, "W %06X decomp stored to %04X with DB = 7E/7F (WRAM, not the PPU/DMA port): check the spec's DB unless the routine means the WRAM copy (7E:2000 palette buffer, 7E:42xx tables)\n", pc, warn);
        fprintf(out, "Z %06X runs=%d bad=%d inconclusive=%d\n", pc, n, bad, inc);
        fflush(out);
    }
    fclose(out);
    exit(0);
}
void dc_frame_hook(unsigned frame)
{
    static int f = -2;
    if (f == -2) f = getenv("CT_DECOMP_FUZZ") ? (getenv("CT_DECOMP_FUZZ_FRAME") ? atoi(getenv("CT_DECOMP_FUZZ_FRAME")) : 2000) : -1;
    if (f >= 0 && frame == (unsigned)f) dc_fuzz_run();
}
#endif

void (*g_func_entry_hook)(uint32_t pc24);
u16 g_dc_hw_in_wram;

/* Master cycles the original routine takes per call (mean over check-mode
 * reference runs; tools/decomp/mkcost.py). The decompiled C spends almost none,
 * and timing-dependent code (NMI-path work, V-counter polls) needs the CPU to
 * fall behind the beam as the original does. CT_DECOMP_NOCOST=1 turns it off. */
static const struct { u32 pc; u32 mc; } s_cost[] = {
#include "dc_cost.inc"
};
static u32 dc_cost(u32 pc)
{
    unsigned lo = 0, hi = sizeof s_cost / sizeof s_cost[0];
    while (lo < hi) { unsigned mid = (lo + hi) / 2; if (s_cost[mid].pc < pc) lo = mid + 1; else hi = mid; }
    return (lo < sizeof s_cost / sizeof s_cost[0] && s_cost[lo].pc == pc) ? s_cost[lo].mc : 0;
}
static int s_cost_on = -1;
RecompReturn dc_leave(DcFrame *f)
{
    if (s_unwind) {
        /* A dc_jsr_y / dc_jsl_y callee unwound to the scheduler: pass it on as
         * a compiled body does (`return _r - 1`); the scheduler finishes the
         * callee and the original after the call site. */
        RecompReturn r = s_unwind;
        if (s_unwind_depth != dc_depth()) {
            fprintf(stderr, "[decomp] %06X: LLE unwind from a nested routine (depth %d, frame %d)\n",
                    f->pc, s_unwind_depth, dc_depth());
            abort();
        }
        s_unwind = RECOMP_RETURN_NORMAL;
#ifndef _WIN32
        if (f->check_fd >= 0) { dc_cancel_check(f); dc_stat(f->pc)->unwound++; }
        s_depth--;
#endif
        return (RecompReturn)((int)r - 1);
    }
    if (s_cost_on < 0) s_cost_on = !(getenv("CT_DECOMP_NOCOST") && atoi(getenv("CT_DECOMP_NOCOST")));
    if (s_cost_on) {
        uint64_t want = dc_cost(f->pc), spent = g_cpu.master_cycles - f->mc0;
        if (want > spent) g_cpu.master_cycles += want - spent;
    }
    cpu_mirrors_to_p(&g_cpu);
    /* RTS/RTL: pop the frame the caller pushed. */
    u16 ret = 0; u8 bank = g_cpu.PB;
    ret = dc_pull16();
    if (f->size == 3) g_cpu.PB = bank = dc_pull8();
    u32 rpc24 = ((u32)bank << 16) | (u16)(ret + 1);
    if (s_fuzz_ret_s && f->entry_s == s_fuzz_ret_s) s_fuzz_ret_pc = rpc24;
#ifndef _WIN32
    dc_finish_check(f);
    s_depth--;
#endif
    if (f->hrv && g_cpu.S == (u16)(f->entry_s + f->size)) return RECOMP_RETURN_NORMAL;
    return cpu_dispatch_pc_from(&g_cpu, rpc24, (u16)(f->entry_s + f->size), f->pc);
}

/* Hand a call that is already under way to the original: interpret from pc24
 * (the ROM instruction matching the decomp's current state, with the same
 * stack) until its RTS/RTL leaves the caller's frame. For runaways on invalid
 * data that the original only survives because the interpreter takes NMIs
 * between instructions. Replaces dc_leave. */
RecompReturn dc_resume_original(DcFrame *f, u32 pc24)
{
    cpu_mirrors_to_p(&g_cpu);
    u8 frame = (u8)(u16)(f->entry_s + f->size - g_cpu.S);
    RecompReturn r = interp_tier_run_call_frame(&g_cpu, pc24, f->pc, frame, NULL);
#ifndef _WIN32
    dc_finish_check(f);
    s_depth--;
#endif
    return r;
}

/* CT_PROF=<file>: count entries of every compiled/decompiled routine; the
 * counts (pc24 count) are written at exit. Used to pick what to decompile
 * and to know a test run actually reached a routine. */
static u32 s_prof_pc[1 << 14];
static uint64_t s_prof_n[1 << 14];
static void dc_prof_hook(uint32_t pc)
{
    unsigned h = (pc * 2654435761u) >> 18;
    while (s_prof_pc[h] && s_prof_pc[h] != pc) h = (h + 1) & ((1 << 14) - 1);
    s_prof_pc[h] = pc; s_prof_n[h]++;
}
static void dc_prof_dump(void)
{
    FILE *f = fopen(getenv("CT_PROF"), "w");
    if (!f) return;
    for (unsigned i = 0; i < (1 << 14); i++)
        if (s_prof_pc[i]) fprintf(f, "%06X %llu\n", s_prof_pc[i], (unsigned long long)s_prof_n[i]);
    fclose(f);
}
__attribute__((constructor)) static void dc_prof_init(void)
{
    if (getenv("CT_PROF")) { g_func_entry_hook = dc_prof_hook; atexit(dc_prof_dump); }
}
