#!/usr/bin/env python3
"""build_pack.py <rom.sfc> <traducao.json> <saida ptbr.bin>

Monta o pacote de tradução que o port lê (src/ct_tr.c). O JSON é escrito por
quem traduz; este script só codifica o texto e desenha as letras acentuadas.

JSON:
{
  "falas": [ {"tabela": "FC:BA00", "indice": 31, "texto": "Olá!{0B}..."} ],
  "nomes": [ {"tipo": "item"|"tecnica"|"inimigo", "indice": 3, "texto": "Poção"} ],
  "menus": [ {"endereco": "FF:D03E", "tamanho": 12, "texto": "..."} ]
}
Texto: letras, números, pontuação comum e os acentos do português nas falas:
á à â ã é ê í ó ô õ ú ç e Á À Â Ã É Ê Í Ó Ô Õ Ú Ç. Nas maiúsculas acentuadas a
letra é a normal e o acento é desenhado no espaço livre entre as linhas (a
cedilha, embaixo); cada uma ocupa 3 bytes na fala (letra, FF, número do acento)
e precisa do port 0.1.6 ou do patch de ROM montado por build_rom.py. Nos nomes e menus
(fonte 8x8) todos os acentos caem, porque os códigos livres da fonte de diálogo
são ícones na fonte de menus.
{XX} insere o byte hexadecimal XX (códigos de controle 01-20: nomes,
pausas, nova página, escolhas); copie-os na mesma ordem do original.
"""
import sys, json, struct, re, os
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import glyph

ACC = {'á':0xF2,'à':0xF4,'â':0xF5,'ã':0xF6,'é':0xF7,'ê':0xF8,'í':0xF9,'ó':0xFA,'ô':0xFB,'õ':0xFC,'ú':0xFD,'ç':0xFE}
BASE = {'á':'a','à':'a','â':'a','ã':'a','é':'e','ê':'e','í':'i','ó':'o','ô':'o','õ':'o','ú':'u','ç':'c'}
CAPS = str.maketrans('ÁÀÂÃÉÊÍÓÔÕÚÇ','AAAAEEIOOOUC')
# accented capitals: letter, FF (blank, width 0), accent number i; the accent
# is ORed into the text buffer above the letter (src/ct_tr.c, build_rom.py)
ACC_UP = 'ÁÀÂÃÉÊÍÓÔÕÚÇ'
ESC = 0xFF
PUNCT = {'!':0xDE,'?':0xDF,'/':0xE0,'«':0xE1,'»':0xE2,':':0xE3,'&':0xE4,'(':0xE5,')':0xE6,"'":0xE7,'.':0xE8,',':0xE9,'=':0xEA,'-':0xEB,'+':0xEC,'%':0xED,' ':0xEF,'…':0xF1,'"':0xE1}
TABLES = {'item':0xCC0B5E,'tecnica':0xCC15C4,'inimigo':0xCC6500}

NOACC = str.maketrans('áàâãéêíóôõúç','aaaaeeiooouc')

def enc(s, acentos=True):
    return enc_esc(s, acentos)[0]

def enc_esc(s, acentos=True):
    """bytes plus the offsets where an FF escape starts"""
    if not acentos: s = s.translate(NOACC).translate(CAPS)
    out = bytearray(); esc = []; i = 0
    while i < len(s):
        c = s[i]
        if c in ACC_UP:
            out.append(0xA0 + ord(BASE[c.lower()].upper()) - 65)
            esc.append(len(out)); out += bytes([ESC, ACC_UP.index(c)]); i += 1; continue
        m = re.match(r'\{([0-9A-Fa-f]{2})\}', s[i:])
        if m: out.append(int(m.group(1), 16)); i += 4; continue
        if 'A' <= c <= 'Z': out.append(0xA0 + ord(c) - 65)
        elif 'a' <= c <= 'z': out.append(0xBA + ord(c) - 97)
        elif '0' <= c <= '9': out.append(0xD4 + ord(c) - 48)
        elif c in ACC: out.append(ACC[c])
        elif c in PUNCT: out.append(PUNCT[c])
        elif c == '\n': out.append(0x05)
        else: raise SystemExit(f'caractere sem glifo: {c!r} em {s!r}')
        i += 1
    return bytes(out), esc

# accent marks drawn in rows 0-1 (lowercase letters start at row 2); 2 = shadow
MARKS = {
 'agudo':      ['...@@', '..@@.'],
 'grave':      ['.@@..', '..@@.'],
 'circunflexo':['..@@.', '.@..@'],
 'til':        ['.@@.@', '@..@.'],
 'agudo_i':    ['.@@', '@@.'],
}
KIND = {'á':'agudo','é':'agudo','í':'agudo_i','ó':'agudo','ú':'agudo','à':'grave','â':'circunflexo','ê':'circunflexo','ô':'circunflexo','ã':'til','õ':'til'}

def make_glyph(rom, ch):
    b = glyph.get(enc(BASE[ch])[0], rom)
    g = [row[:] for row in b]
    if ch == 'ç':
        # cedilla under the c: rows 9-10
        g[9][2] = g[9][3] = 3; g[9][4] = 1; g[10][1] = g[10][2] = 3; g[10][3] = 1
        return g
    if ch == 'í':   # drop the dot of the i (rows 0-1) before adding the accent
        for y in range(2): g[y] = [0] * 12
    m = MARKS[KIND[ch]]
    for y, row in enumerate(m):
        for x, v in enumerate(row):
            if v == '@':
                g[y][x] = 3
                if x + 1 < 12 and (x + 1 >= len(row) or row[x + 1] != '@') and g[y][x + 1] == 0:
                    g[y][x + 1] = 1   # shadow on the right, like the font
    return g

def squash(g):
    """capital in rows 0-8 -> rows 2-8, dropping two rows (repeated ones first)"""
    diff = lambda a, b: sum(x != y for x, y in zip(g[a], g[b]))
    dup = [r for r in range(1, 8) if g[r] == g[r - 1]]
    if len(dup) >= 2: drop = [dup[0], dup[-1]]
    else:
        drop = dup[:]
        cand = sorted((diff(r, r - 1), abs(r - 4), r) for r in range(1, 8) if r not in drop and all(abs(r - d) > 1 for d in drop))
        drop += [r for _, _, r in cand[:2 - len(drop)]]
    return [[0] * 12, [0] * 12] + [g[r][:] for r in range(9) if r not in drop] + [row[:] for row in g[9:]]

def mark(ch, rows, dx):
    """accent of ch into rows[0..1] (lists of pixel values), shadow to the right"""
    for y, m in enumerate(MARKS[KIND[ch.lower()].replace('agudo_i', 'agudo')]):
        for x, v in enumerate(m):
            if v == '@':
                rows[y][x + dx] = 3
                if (x + 1 >= len(m) or m[x + 1] != '@') and rows[y][x + dx + 1] == 0:
                    rows[y][x + dx + 1] = 1

def word(row, pl):
    return sum(((row[x] >> pl) & 1) << (15 - x) for x in range(len(row)))

def acc_records(rom):
    """63 bytes per accented capital (see src/ct_tr.c): band row, the accent's two
    rows as u16 plane words, then the first-line variant (shorter letter)."""
    out = bytearray()
    for ch in ACC_UP:
        rows = [[0] * 16, [0] * 16]; first = bytearray(54)
        if ch == 'Ç':
            band = 13
            rows[0][2] = rows[0][3] = 3; rows[0][4] = 1; rows[1][1] = rows[1][2] = 3; rows[1][3] = 1
        else:
            band = 2
            b = glyph.get(enc(BASE[ch.lower()].upper())[0], rom)
            cols = [x for row in b for x, v in enumerate(row) if v == 3]
            dx = round((min(cols) + max(cols)) / 2 - 2.5)
            mark(ch, rows, dx)
            g = squash(b); mark(ch, g, dx); first = bytearray()
            for r in range(9):
                first += struct.pack('<HHH', word([1 if v else 0 for v in b[r]], 0), word(g[r], 0), word(g[r], 1))
        out.append(band)
        for y in range(2):
            for pl in range(2): out += struct.pack('<H', word(rows[y], pl))
        out += first
    return bytes(out)

def main():
    rom = bytearray(open(sys.argv[1], 'rb').read()); tr = json.load(open(sys.argv[2], encoding='utf-8'))
    glyph.load(rom); patches = []
    # dialogue font: accented glyphs into the free codes, widths at C2:60E6
    font = bytearray(rom)
    for ch, code in ACC.items():
        glyph.put(code, make_glyph(rom, ch), font)
        font[0x0260E6 + code - 0xA0] = rom[0x0260E6 + enc(BASE[ch])[0] - 0xA0]
    font[0x0260E6 + ESC - 0xA0] = 0          # FF: blank, now width 0
    # emit only changed bytes, grouped in runs
    i = 0
    while i < len(rom):
        if font[i] != rom[i]:
            j = i
            while j < len(rom) and (font[j] != rom[j] or (j + 1 < len(rom) and font[j+1] != rom[j+1])): j += 1
            patches.append((0xC00000 + i, bytes(font[i:j]))); i = j
        else: i += 1
    for n in tr.get('nomes', []):
        base = TABLES[n['tipo']] + 11 * n['indice']
        cur = rom[base - 0xC00000: base - 0xC00000 + 11]
        b = enc(n['texto'], acentos=False)
        if n['tipo'] == 'item': b = bytes([cur[0]]) + b   # keep the item icon
        if len(b) > 11: raise SystemExit(f"nome longo demais (máx. {11 - (n['tipo']=='item')}): {n['texto']}")
        patches.append((base, b + bytes([0xEF]) * (11 - len(b))))
    for m in tr.get('menus', []):
        a = int(m['endereco'].replace(':', ''), 16); b = enc(m['texto'], acentos=False)
        if len(b) > m['tamanho']: raise SystemExit(f"menu longo demais: {m['texto']}")
        patches.append((a, b + bytes([0xEF]) * (m['tamanho'] - len(b))))
    strs = []
    for f in tr.get('falas', []):
        t = int(f['tabela'].replace(':', ''), 16)
        b, esc = enc_esc(f['texto'])
        if not b.endswith(b'\x00'): b += b'\x00'
        if len(esc) > 255: raise SystemExit(f"maiúsculas acentuadas demais numa fala: {f['tabela']} {f['indice']}")
        strs.append((t + 2 * f['indice'], b, esc))
    out = bytearray(b'CTTR' + struct.pack('<I', 2) + struct.pack('<I', len(patches)))
    for a, b in patches: out += struct.pack('<IH', a, len(b)) + b
    out += struct.pack('<I', len(strs))
    for a, b, esc in strs: out += struct.pack('<IH', a, len(b)) + b + bytes([len(esc)]) + b''.join(struct.pack('<H', e) for e in esc)
    acc = acc_records(rom)
    out += struct.pack('<I', len(acc) // 63) + acc
    open(sys.argv[3], 'wb').write(out)
    print(f'{sys.argv[3]}: {len(patches)} trechos do ROM, {len(strs)} falas, {len(out)} bytes')

if __name__ == '__main__':
    main()
