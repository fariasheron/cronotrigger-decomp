/* Translation pack loader (ptbr.bin next to the executable).
 *
 * The pack carries no game text of its own; whoever builds it supplies the
 * translated strings (tools/tr/build_pack.py). Without the file the game runs
 * unchanged.
 *
 * Pack layout (little-endian):
 *   "CTTR" u32 version(=1)
 *   u32 n_patches;  n_patches x { u32 snes_addr24, u16 len, len bytes }
 *       written over the in-memory ROM at load (font glyphs, glyph widths,
 *       fixed-size name records, short menu strings)
 *   u32 n_strings;  n_strings x { u32 entry_addr24, u16 len, len bytes }
 *       entry_addr24 = table + 2*index of a dialogue string table. When the
 *       text system (C2:57DF) reads that pointer, the bytes the dialogue
 *       decoder (C2:58B2) then fetches from the original string address are
 *       served from the pack instead, up to and including its final 00.
 *   version 2 adds, per string, u8 n_esc + n_esc x u16 offsets of "FF k"
 *   escapes, then u32 n_acc; n_acc x 63-byte accent records. An escape follows
 *   a capital letter: the decoder is served only the FF (blank, width 0) and,
 *   at that read, accent k is ORed into the text buffer over the letter just
 *   drawn, in the free rows above it (or below it, for the cedilla). Record:
 *   band row (0-15, glyph row 0 is band row 4), then row0 plane0, row0 plane1,
 *   row1 plane0, row1 plane1 as u16 (bit 15 = leftmost pixel of the letter);
 *   then, for the first line of a box ($7E:0131 = 0), whose upper rows the
 *   frame hides, glyph rows 0-8 as {u16 old-letter mask, u16 plane0, u16
 *   plane1} of a shorter letter with the accent on it.
 *   The ROM patch does the same in 65816 code (build_rom.py).
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "cpu_state.h"
#include "common_rtl.h"

extern int snesrecomp_exe_dir_path(const char *leaf, char *out, size_t max_len);
extern int (*g_rom_read8_hook)(uint8 bank, uint16 addr, uint8 *out);
extern void (*g_rom_read16_hook)(uint8 bank, uint16 addr);

typedef struct { uint32 key; uint16 len; const uint8 *data; const uint8 *ext; } CtrStr;

static CtrStr *s_str; static uint32 s_nstr; static uint32 s_cap;
static uint8 *s_blob;
/* active redirect: reads of bank:[base, base+len) come from data */
static uint8 s_bank; static uint16 s_base; static uint16 s_len; static const uint8 *s_data, *s_ext;
static const uint8 *s_acc; static uint32 s_nacc;

static uint8 *dp8(uint16 o) { return &g_ram[(uint16)(g_cpu.D + o) & 0x1FFF]; }

/* mirrors C2:5DC4's pixel placement: column tiles via C2:5FE6, 2 bytes per row */
#define ACC_REC 63
static void or_row(uint8 *mem, uint16 at, const uint16 col[3], unsigned sh, uint16 keep, uint16 set)
{
    uint32 m = ((uint32)keep << 8) >> sh, v = ((uint32)set << 8) >> sh;
    for (int i = 0; i < 3; i++) {
        uint8 *q = &mem[(uint16)(at + col[i])];
        unsigned bit = 16 - 8 * i;
        *q = (uint8)((*q & ~(m >> bit)) | (v >> bit));
    }
}

static void draw_accent(uint8 k)
{
    if (k >= s_nacc) return;
    const uint8 *e = s_acc + ACC_REC * k;
    uint8 code = *dp8(0x35);
    uint8 w = *RomPtr(0xC260E6 + (uint8)(code - 0xA0));
    uint8 x = (uint8)(*dp8(0x34) - w);
    unsigned sh = x & 7, t2 = (x & 0xF8) >> 2;
    uint16 stride = (*dp8(0x14) & 0x0F) ? 0x200 : 0x100;
    uint16 buf = (uint16)(*dp8(0x10) | *dp8(0x11) << 8);
    uint8 *mem = g_ram + ((*dp8(0x12) & 1) << 16);
    uint16 col[3];
    for (int i = 0; i < 3; i++) { const uint8 *c = RomPtr(0xC25FE6 + t2 + 2 * i); col[i] = (uint16)(c[0] | c[1] << 8); }
#define BANDROW(b) (uint16)(buf + ((b) & 7) * 2 + (((b) >> 3) ? stride : 0))
    if (e[0] < 4 && g_ram[0x131] == 0) {
        /* first line of the box: the rows above it are hidden by the frame,
           so the letter is redrawn shorter with the accent on top of it */
        for (int r = 0; r < 9; r++) {
            const uint8 *q = e + 9 + 6 * r;
            uint16 m = (uint16)(q[0] | q[1] << 8);
            for (int pl = 0; pl < 2; pl++)
                or_row(mem, (uint16)(BANDROW(4 + r) + pl), col, sh, m, (uint16)(q[2 + 2 * pl] | q[3 + 2 * pl] << 8));
        }
        return;
    }
    for (int j = 0; j < 4; j++)
        or_row(mem, (uint16)(BANDROW(e[0] + (j >> 1)) + (j & 1)), col, sh, 0, (uint16)(e[1 + 2 * j] | e[2 + 2 * j] << 8));
#undef BANDROW
}

static const CtrStr *find_str(uint32 key)
{
    if (!s_cap) return NULL;
    for (uint32 h = (key * 2654435761u) & (s_cap - 1);; h = (h + 1) & (s_cap - 1)) {
        if (!s_str[h].data) return NULL;
        if (s_str[h].key == key) return &s_str[h];
    }
}

static int ctr_read8(uint8 bank, uint16 addr, uint8 *out)
{
    if (s_data && bank == s_bank && (uint16)(addr - s_base) < s_len) {
        uint16 k = (uint16)(addr - s_base);
        *out = s_data[k];
        if (s_ext && s_ext[k]) draw_accent((uint8)(s_ext[k] - 1));
        if (k + 1u == s_len) s_data = NULL;   /* terminator served: string done */
        return 1;
    }
    return 0;
}

static void ctr_read16(uint8 bank, uint16 addr)
{
    if (bank < 0xC0) return;
    const CtrStr *s = find_str(((uint32)bank << 16) | addr);
    if (!s) return;
    const uint8 *p = RomPtr(((uint32)bank << 16) | addr);
    s_bank = bank; s_base = (uint16)(p[0] | p[1] << 8); s_len = s->len; s_data = s->data; s_ext = s->ext;
}

static uint32 rd32(const uint8 *p) { return p[0] | p[1] << 8 | p[2] << 16 | (uint32)p[3] << 24; }
static uint16 rd16(const uint8 *p) { return (uint16)(p[0] | p[1] << 8); }

void ct_tr_init(void)
{
    static int done; if (done) return; done = 1;
    char path[1024];
    const char *env = getenv("CT_TRADUCAO");
    if (env && !env[0]) return;                    /* CT_TRADUCAO= turns it off */
    if (env) snprintf(path, sizeof path, "%s", env);
    else if (!snesrecomp_exe_dir_path("ptbr.bin", path, sizeof path)) return;
    FILE *f = fopen(path, "rb");
    if (!f) return;
    fseek(f, 0, SEEK_END); long n = ftell(f); fseek(f, 0, SEEK_SET);
    s_blob = malloc((size_t)n);
    if (!s_blob || fread(s_blob, 1, (size_t)n, f) != (size_t)n) { fclose(f); free(s_blob); s_blob = NULL; return; }
    fclose(f);
    uint32 ver = n >= 16 ? rd32(s_blob + 4) : 0;
    if (n < 16 || memcmp(s_blob, "CTTR", 4) || (ver != 1 && ver != 2)) {
        fprintf(stderr, "[traducao] %s: formato desconhecido, ignorado\n", path);
        return;
    }
    const uint8 *p = s_blob + 8, *end = s_blob + n;
    uint32 np = rd32(p); p += 4;
    for (uint32 i = 0; i < np && p + 6 <= end; i++) {
        uint32 a = rd32(p); uint16 len = rd16(p + 4); p += 6;
        if (p + len > end) break;
        for (uint16 k = 0; k < len; k++) *(uint8 *)RomPtr(a + k) = p[k];
        p += len;
    }
    uint32 ns = (p + 4 <= end) ? rd32(p) : 0; p += 4;
    for (s_cap = 16; s_cap < ns * 2; s_cap <<= 1) {}
    s_str = calloc(s_cap, sizeof *s_str);
    for (uint32 i = 0; i < ns && p + 6 <= end; i++) {
        uint32 key = rd32(p); uint16 len = rd16(p + 4); p += 6;
        if (p + len > end) break;
        const uint8 *data = p, *ext = NULL; p += len;
        if (ver >= 2) {
            if (p + 1 > end) break;
            uint8 ne = *p++;
            if (p + 2u * ne > end) break;
            if (ne) {
                uint8 *d = malloc(len), *x = calloc(len, 1); uint16 o = 0, prev = 0;
                for (uint8 j = 0; j < ne; j++) {
                    uint16 at = rd16(p + 2 * j);
                    if (at < prev || at + 1u >= len) break;
                    memcpy(d + o, data + prev, at - prev); o += at - prev;
                    d[o] = data[at]; x[o] = (uint8)(data[at + 1] + 1); o++;
                    prev = at + 2;
                }
                memcpy(d + o, data + prev, len - prev); o += len - prev;
                data = d; ext = x; len = o;
            }
            p += 2u * ne;
        }
        uint32 h = (key * 2654435761u) & (s_cap - 1);
        while (s_str[h].data && s_str[h].key != key) h = (h + 1) & (s_cap - 1);
        s_str[h].key = key; s_str[h].len = len; s_str[h].data = data; s_str[h].ext = ext;
        s_nstr++;
    }
    if (ver >= 2 && p + 4 <= end) {
        uint32 na = rd32(p); p += 4;
        if (p + (uint32)ACC_REC * na <= end) { s_acc = p; s_nacc = na; }
    }
    g_rom_read8_hook = ctr_read8;
    g_rom_read16_hook = ctr_read16;
    fprintf(stderr, "[traducao] %s: %u trechos no ROM, %u falas, %u acentos de maiuscula\n", path, np, s_nstr, s_nacc);
}
