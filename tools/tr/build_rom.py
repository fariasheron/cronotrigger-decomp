"""build_rom.py <rom.sfc> <traducao.json> <out.sfc> : 8 MB ExHiROM with the translation baked in."""
import sys, json, struct, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import build_pack as bp, glyph

EXP_BANK0 = 0x50          # strings/tables/hook in lower halves of banks $50-$57
NBANKS = 8
HOOK_SITE = 0x0257EE      # C2:57EE  LDA [$0D],Y ... STA $33 (13 bytes)
ACC_BANK = 0x58           # accented-capital routine + tables, lower half of $58
ACC_SITE = 0x0258BE       # C2:58BE  REP #$20 / STA $35 / SEP #$20 (glyph byte -> $35)

def asm_acc():
    """C2:58BE -> JSL here. Byte FF after a capital: read accent number k, OR the
    accent (or, on the first line of the box, a shorter letter with the accent)
    into the text buffer, then draw FF (blank, width 0). Same as src/ct_tr.c."""
    L = lambda a: [a & 0xFF, (a >> 8) & 0xFF, ACC_BANK]
    code = []; labels = {}; fix = []
    def e(*b): code.extend(b)
    def br(op, lab): e(op, 0); fix.append((len(code) - 1, lab))
    def lab(n): labels[n] = len(code)
    e(0xC9,0xFF); br(0xF0,'esc'); e(0xC2,0x20, 0x85,0x35, 0xE2,0x20, 0x6B)
    lab('esc')
    e(0x08, 0xC2,0x30, 0xDA, 0x5A)                       # PHP REP#30 PHX PHY
    e(0xA7,0x31, 0x29,0xFF,0x00, 0xE6,0x31, 0x0A, 0x85,0x6A)   # k*2 -> $6A, consume k
    e(0xE2,0x20, 0xAF,0x31,0x01,0x7E, 0xC2,0x20); br(0xD0,'nm'); e(0xE6,0x6A)  # first line: +1
    lab('nm')
    e(0xA5,0x6A, 0x0A,0x0A,0x0A, 0x85,0x6A)               # (k*2+mode)*8
    e(0xA5,0x35, 0x29,0xFF,0x00, 0x38, 0xE9,0xA0,0x00, 0x29,0xFF,0x00, 0xAA)
    e(0xE2,0x20, 0xBF,0xE6,0x60,0xC2, 0x49,0xFF, 0x38, 0x65,0x34, 0xC2,0x20, 0x29,0xFF,0x00)  # x = $34 - width
    e(0x48, 0x29,0xF8,0x00, 0x4A, 0x4A, 0xAA)
    e(0xBF,0xE6,0x5F,0xC2, 0x85,0x6C, 0xBF,0xE8,0x5F,0xC2, 0x85,0x6E, 0xBF,0xEA,0x5F,0xC2, 0x85,0x76)
    e(0x68, 0x29,0x07,0x00, 0x18, 0x65,0x6A, 0x0A, 0xAA, 0xBF,*L(0x0200), 0xAA)   # X = unit list
    e(0xE2,0x20, 0xA5,0x14, 0x29,0x0F, 0xC2,0x20); br(0xF0,'s1'); e(0xA9,0x00,0x02); br(0x80,'s2')
    lab('s1'); e(0xA9,0x00,0x01)
    lab('s2'); e(0x85,0x79, 0xE2,0x20, 0xA5,0x12, 0x85,0x75, 0xC2,0x20)
    lab('loop')
    e(0xBF,*L(0), 0x29,0xFF,0x00, 0xC9,0xFF,0x00); br(0xF0,'done')
    e(0x48, 0x29,0x07,0x00, 0x0A, 0x18, 0x65,0x10, 0x85,0x73, 0x68, 0x29,0x08,0x00); br(0xF0,'nost')
    e(0xA5,0x73, 0x18, 0x65,0x79, 0x85,0x73)
    lab('nost')
    e(0xBF,*L(1), 0x29,0xFF,0x00, 0x18, 0x65,0x73, 0x85,0x73, 0xE2,0x20)
    for col, i in ((0x6C, 0), (0x6E, 1), (0x76, 2)):
        e(0xA4,col, 0xB7,0x73, 0x3F,*L(2 + i), 0x1F,*L(5 + i), 0x97,0x73)
    e(0xC2,0x20, 0x8A, 0x18, 0x69,0x08,0x00, 0xAA); br(0x80,'loop')
    lab('done')
    e(0x7A, 0xFA, 0x28, 0xC2,0x20, 0xA9,0xFF,0x00, 0x85,0x35, 0xE2,0x20, 0x6B)
    for pos, l in fix:
        d = labels[l] - (pos + 1); assert -128 <= d <= 127, l; code[pos] = d & 0xFF
    return bytes(code)

def acc_bank(src):
    """$58:0000 code, $58:0200 list pointers [(k*2+first)*8+shift], $58:0400 unit lists:
    {band row, plane, 3 AND bytes, 3 OR bytes} ... FF"""
    rec = bp.acc_records(src); n = len(rec) // 63
    b = bytearray(0x400); b[:len(asm_acc())] = asm_acc()
    sh3 = lambda w, s: [((w << 8) >> s >> k) & 0xFF for k in (16, 8, 0)]
    for k in range(n):
        e = rec[63 * k: 63 * (k + 1)]
        for first in (0, 1):
            for s in range(8):
                units = []
                if first and e[0] < 4:
                    for r in range(9):
                        m, p0, p1 = struct.unpack('<HHH', e[9 + 6 * r: 15 + 6 * r])
                        keep = [x ^ 0xFF for x in sh3(m, s)]
                        units += [bytes([4 + r, 0] + keep + sh3(p0, s)), bytes([4 + r, 1] + keep + sh3(p1, s))]
                else:
                    for j in range(4):
                        w = struct.unpack('<H', e[1 + 2 * j: 3 + 2 * j])[0]
                        units.append(bytes([e[0] + (j >> 1), j & 1, 0xFF, 0xFF, 0xFF] + sh3(w, s)))
                i = (k * 2 + first) * 8 + s
                b[0x200 + 2 * i: 0x202 + 2 * i] = struct.pack('<H', len(b))
                b += b''.join(units) + b'\xff' * 8
    assert len(b) <= 0x8000
    return bytes(b)

def asm_hook(tbl):
    t = lambda off: [(tbl+off) & 0xFF, ((tbl+off) >> 8) & 0xFF, (tbl+off) >> 16]
    code = []; labels = {}; fix = []
    def e(*b): code.extend(b)
    def br(op, lab): e(op, 0); fix.append((len(code)-1, lab))
    def L(n): labels[n] = len(code)
    e(0x08, 0xC2,0x30, 0xDA, 0x5A, 0xB7,0x0D, 0x85,0x31, 0xA5,0x0F, 0x29,0xFF,0x00, 0x85,0x33, 0xA2,0x00,0x00)
    L('loop'); e(0xBF,*t(0)); e(0xC9,0xFF,0xFF); br(0xF0,'done'); e(0xC5,0x0D); br(0xD0,'next')
    e(0xBF,*t(1)); e(0xC5,0x0E); br(0xF0,'found')
    L('next'); e(0x8A, 0x18, 0x69,0x08,0x00, 0xAA); br(0x80,'loop')
    L('found'); e(0xA5,0x0C, 0x29,0xFF,0x00, 0xDF,*t(6)); br(0xB0,'done')
    e(0x48, 0x0A, 0x18, 0x63,0x01, 0xA8, 0x68)
    e(0xA5,0x31, 0x48, 0xA5,0x33, 0x48)
    e(0xBF,*t(3), 0x85,0x31, 0xBF,*t(5), 0x29,0xFF,0x00, 0x85,0x33)
    e(0xB7,0x31, 0x48, 0xC8, 0xC8, 0xB7,0x31, 0x29,0xFF,0x00); br(0xF0,'notr')
    e(0x85,0x33, 0x68, 0x85,0x31, 0x68, 0x68); br(0x80,'done')
    L('notr'); e(0x68, 0x68, 0x85,0x33, 0x68, 0x85,0x31)
    L('done'); e(0x7A, 0xFA, 0x28, 0xA9,0x00,0x00, 0xE2,0x20, 0xA5,0x33, 0x6B)
    for pos, lab in fix:
        d = labels[lab] - (pos + 1); assert -128 <= d <= 127, lab; code[pos] = d & 0xFF
    return bytes(code)

def main():
    src = open(sys.argv[1], 'rb').read(); assert len(src) == 0x400000
    tr = json.load(open(sys.argv[2], encoding='utf-8'))
    rom = bytearray(src)
    # 1) same fixed-size patches as the port pack: font glyphs/widths, names, menus
    glyph.load(src); font = bytearray(src)
    for ch, code in bp.ACC.items():
        glyph.put(code, bp.make_glyph(src, ch), font)
        font[0x0260E6 + code - 0xA0] = src[0x0260E6 + bp.enc(bp.BASE[ch])[0] - 0xA0]
    font[0x0260E6 + bp.ESC - 0xA0] = 0          # FF: blank, width 0 (accent escape)
    rom[:] = font
    for n in tr.get('nomes', []):
        base = bp.TABLES[n['tipo']] + 11 * n['indice'] - 0xC00000
        b = bp.enc(n['texto'], acentos=False)
        if n['tipo'] == 'item': b = bytes([src[base]]) + b
        assert len(b) <= 11, n
        rom[base:base+11] = b + bytes([0xEF]) * (11 - len(b))
    for m in tr.get('menus', []):
        a = int(m['endereco'].replace(':', ''), 16) - 0xC00000; b = bp.enc(m['texto'], acentos=False)
        assert len(b) <= m['tamanho'], m
        rom[a:a+m['tamanho']] = b + bytes([0xEF]) * (m['tamanho'] - len(b))
    # 2) strings into expansion, per-table 24-bit pointer tables
    tabs = {}
    for f in tr['falas']:
        t = int(f['tabela'].replace(':', ''), 16); b = bp.enc(f['texto'])
        if not b.endswith(b'\x00'): b += b'\x00'
        tabs.setdefault(t, {})[f['indice']] = b
    exp = {k: bytearray() for k in range(NBANKS)}   # lower 32 KB of each bank
    cur = 0
    def alloc(data):
        nonlocal cur
        while len(exp[cur]) + len(data) > 0x8000: cur += 1; assert cur < NBANKS, 'sem espaço'
        a = ((EXP_BANK0 + cur) << 16) | len(exp[cur]); exp[cur] += data; return a
    # bank 0 starts with hook + table-of-tables + pointer tables
    tbl_addr = (EXP_BANK0 << 16) | 0x0100
    hook = asm_hook(tbl_addr); assert len(hook) < 0x100
    exp[0] += hook + bytes(0x100 - len(hook))
    ttab_size = 8 * len(tabs) + 2
    exp[0] += bytes(ttab_size)
    ptrtabs = {}
    for t in sorted(tabs):
        cnt = max(tabs[t]) + 1
        ptrtabs[t] = (alloc(bytes(3 * cnt)), cnt)
    for t in sorted(tabs):
        pa, cnt = ptrtabs[t]
        for i, b in sorted(tabs[t].items()):
            a = alloc(b)
            o = (pa & 0xFFFF) + 3 * i; bk = (pa >> 16) - EXP_BANK0
            exp[bk][o:o+3] = struct.pack('<I', a)[:3]
    tt = bytearray()
    for t in sorted(tabs):
        pa, cnt = ptrtabs[t]
        tt += struct.pack('<I', t)[:3] + struct.pack('<I', pa)[:3] + struct.pack('<H', cnt)
    tt += b'\xff\xff'
    exp[0][0x100:0x100+len(tt)] = tt
    # 3) hook call at C2:57EE
    assert src[HOOK_SITE:HOOK_SITE+13] == bytes.fromhex('b70d8531a90000e220a50f8533')
    rom[HOOK_SITE:HOOK_SITE+13] = bytes([0x22, 0x00, 0x00, EXP_BANK0]) + b'\xea' * 9
    # 3b) accented capitals: escape FF handled at C2:58BE
    assert src[ACC_SITE:ACC_SITE+6] == bytes.fromhex('c2208535e220')
    rom[ACC_SITE:ACC_SITE+6] = bytes([0x22, 0x00, 0x00, ACC_BANK, 0xEA, 0xEA])
    # 4) 8 MB ExHiROM: upper 4 MB = copy (keeps every HiROM mirror), then our banks
    big = bytearray(rom + rom)
    for k in range(NBANKS):
        o = 0x400000 + ((EXP_BANK0 + k) & 0x3F) * 0x10000
        big[o:o+len(exp[k])] = exp[k]
    for h in (0x00FFC0, 0x40FFC0):
        big[h+0x15] = 0x35; big[h+0x17] = 0x0D
        big[h+0x1C:h+0x20] = b'\xff\xff\x00\x00'
    accb = acc_bank(src); o = 0x400000 + (ACC_BANK & 0x3F) * 0x10000
    big[o:o+len(accb)] = accb
    s = sum(big) & 0xFFFF
    for h in (0x00FFC0, 0x40FFC0):
        big[h+0x1C:h+0x20] = struct.pack('<HH', s ^ 0xFFFF, s)
    open(sys.argv[3], 'wb').write(big)
    used = sum(len(v) for v in exp.values())
    print(f'{sys.argv[3]}: {len(tabs)} tabelas, {sum(len(v) for v in tabs.values())} falas, expansão {used} bytes em {cur+1} bancos, gancho {len(hook)} bytes, checksum {s:04X}')

if __name__ == '__main__':
    main()
