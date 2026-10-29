#!/usr/bin/env python3
"""liveness.py <rom> <inventory.json> <out.h>
For every routine, which register/flag outputs any caller can still read after
it returns (A X Y N V Z C). Callers are found statically (JSR/JSL/JMP/JML
operands in every disassembled routine); the scan from each return address
follows the caller's control flow until each output is overwritten or read,
and a caller's own RTS/RTL hands over the caller's live-out (fixpoint).
A routine whose address also appears as a data word in its bank (a pointer
table entry, an indirect call) or with no static caller gets everything live.
Writes a C table (pc24, mask) used by the decomp check: outputs outside the
mask are not compared, so a decompiled routine only has to reproduce the
values its callers use."""
import sys, os, json, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fdis import walk
A, X, Y, N, V, Z, C = 1, 2, 4, 8, 16, 32, 64
ALL = 127
F_NZ = N | Z
rom = open(sys.argv[1], 'rb').read()
inv = json.load(open(sys.argv[2]))
def regs_of_mode(mode):
    u = 0
    if mode in ('dpx', 'absx', 'longx', 'dpxi', 'absxi'): u |= X
    if mode in ('dpy', 'absy', 'dpiy', 'dpily', 'sriy'): u |= Y
    return u
def usedef(mn, mode, v):
    """(use, def, kind) kind: None normal, 'call', 'ret', 'stop' (all live), 'jmp'"""
    ix = regs_of_mode(mode)
    acc = mode == 'acc'
    if mn == 'LDA': return ix, A | F_NZ, None
    if mn == 'LDX': return ix, X | F_NZ, None
    if mn == 'LDY': return ix, Y | F_NZ, None
    if mn == 'STA': return ix | A, 0, None
    if mn == 'STX': return ix | X, 0, None
    if mn == 'STY': return ix | Y, 0, None
    if mn == 'STZ': return ix, 0, None
    if mn in ('ADC', 'SBC'): return ix | A | C, A | F_NZ | C | V, None
    if mn in ('AND', 'ORA', 'EOR'): return ix | A, A | F_NZ, None
    if mn == 'CMP': return ix | A, F_NZ | C, None
    if mn == 'CPX': return X, F_NZ | C, None
    if mn == 'CPY': return Y, F_NZ | C, None
    if mn == 'BIT': return ix | A, (Z if mode == 'immm' else N | V | Z), None
    if mn in ('INC', 'DEC'): return (A if acc else ix), (A if acc else 0) | F_NZ, None
    if mn in ('INX', 'DEX'): return X, X | F_NZ, None
    if mn in ('INY', 'DEY'): return Y, Y | F_NZ, None
    if mn in ('ASL', 'LSR'): return (A if acc else ix), (A if acc else 0) | F_NZ | C, None
    if mn in ('ROL', 'ROR'): return (A if acc else ix) | C, (A if acc else 0) | F_NZ | C, None
    if mn == 'TAX': return A, X | F_NZ, None
    if mn == 'TAY': return A, Y | F_NZ, None
    if mn == 'TXA': return X, A | F_NZ, None
    if mn == 'TYA': return Y, A | F_NZ, None
    if mn == 'TXY': return X, Y | F_NZ, None
    if mn == 'TYX': return Y, X | F_NZ, None
    if mn == 'TSX': return 0, X | F_NZ, None
    if mn == 'TXS': return X, 0, None
    if mn in ('TCD', 'TCS'): return A, (F_NZ if mn == 'TCD' else 0), None
    if mn in ('TDC', 'TSC'): return 0, A | F_NZ, None
    if mn == 'XBA': return A, A | F_NZ, None
    if mn == 'PHA': return A, 0, None
    if mn == 'PHX': return X, 0, None
    if mn == 'PHY': return Y, 0, None
    if mn == 'PLA': return 0, A | F_NZ, None
    if mn == 'PLX': return 0, X | F_NZ, None
    if mn == 'PLY': return 0, Y | F_NZ, None
    if mn in ('PLB', 'PLD'): return 0, F_NZ, None
    if mn == 'PHP': return N | V | Z | C, 0, None
    if mn == 'PLP': return 0, N | V | Z | C, None
    if mn == 'CLC' or mn == 'SEC': return 0, C, None
    if mn == 'CLV': return 0, V, None
    if mn in ('REP', 'SEP'):
        d = (C if v & 1 else 0) | (Z if v & 2 else 0) | (V if v & 0x40 else 0) | (N if v & 0x80 else 0)
        return 0, d, None
    if mn in ('TRB', 'TSB'): return ix | A, Z, None
    if mn in ('MVN', 'MVP'): return A | X | Y, A | X | Y, None
    if mn in ('BCC', 'BCS'): return C, 0, None
    if mn in ('BEQ', 'BNE'): return Z, 0, None
    if mn in ('BMI', 'BPL'): return N, 0, None
    if mn in ('BVC', 'BVS'): return V, 0, None
    if mn in ('BRA', 'BRL', 'NOP', 'WAI', 'PHB', 'PHD', 'PHK', 'PEA', 'PEI', 'PER', 'CLI', 'SEI', 'CLD', 'SED', 'WDM'):
        return (0 if mn != 'PEI' else 0), 0, None
    if mn in ('JSR', 'JSL'): return (ALL if mode in ('absxi',) else 0), 0, 'call'
    if mn in ('RTS', 'RTL'): return 0, 0, 'ret'
    if mn in ('JMP', 'JML'): return (X if mode == 'absxi' else 0), 0, 'jmp'
    return ALL, 0, 'stop'
# disassemble everything
code = {}            # pc -> (m,x,op,mn,mode,n,b,v)
owner = collections.defaultdict(set)
routines = set()
import routines as _rt
for pc, m_, x_, disp in _rt.load():
    if (pc >> 16) < 0xC0: continue
    routines.add(pc)
    s, labels, calls = walk(rom, pc, m_, x_)
    for a, e in s.items():
        code.setdefault(a, e); owner[a].add(pc)
def nxt(a, e): return (a & 0xFF0000) | ((a + 1 + e[5]) & 0xFFFF)
def target(a, e):
    m, x, op, mn, mode, n, b, v = e
    if mode == 'rel': return (a & 0xFF0000) | ((a + 2 + (v - 256 if v > 127 else v)) & 0xFFFF)
    if mode == 'rell': return (a & 0xFF0000) | ((a + 3 + (v - 65536 if v > 32767 else v)) & 0xFFFF)
    if mode == 'abs': return (a & 0xFF0000) | v
    if mode == 'long': return v
    return None
callsites = collections.defaultdict(list)   # callee -> [return address]
jumps_in = collections.defaultdict(list)    # callee reached by JMP/JML from routine(s)
operand_refs = collections.Counter()
for a, e in code.items():
    mn, mode = e[3], e[4]
    if mn in ('JSR', 'JSL') and mode in ('abs', 'long'):
        callsites[target(a, e)].append(nxt(a, e))
    if mn in ('JMP', 'JML') and mode in ('abs', 'long'):
        jumps_in[target(a, e)].append(a)
# data references: 16-bit occurrences of the routine's low address in its own bank, outside code bytes
codebytes = set()
for a, e in code.items():
    for k in range(1 + e[5]): codebytes.add((a & 0xFF0000) | ((a + k) & 0xFFFF))
datarefs = set()
runs = []
# pointer tables: runs of >= 2 consecutive little-endian words, outside code, each the
# address of a known routine (or code byte) in the same bank
entries_by_bank = collections.defaultdict(set)
for pc in routines: entries_by_bank[pc >> 16].add(pc & 0xFFFF)
codeaddr_by_bank = collections.defaultdict(set)
for a in code: codeaddr_by_bank[a >> 16].add(a & 0xFFFF)
for bank in entries_by_bank:
    base = (bank & 0x3F) << 16; seg = rom[base:base + 0x10000]
    ents = entries_by_bank[bank]; cad = codeaddr_by_bank[bank]
    for par in (0, 1):
        run = []
        for i in range(par, 0xFFFE, 2):
            w = seg[i] | seg[i + 1] << 8
            ok = ((bank << 16) | i) not in codebytes and (w in cad)
            if ok:
                if not run: run_start = i
                run.append(w)
            else:
                if len(run) >= 2:
                    runs.append((bank, run_start, list(run)))
                    for w2 in run:
                        if w2 in ents: datarefs.add((bank << 16) | w2)
                run = []
# indirect dispatch: JSR (abs,X) / JMP (abs,X) whose table is one of the runs
indirect_ok = set()
for a, e in code.items():
    mn, mode, v = e[3], e[4], e[7]
    if mode != 'absxi' or mn not in ('JSR', 'JMP'): continue
    bank = a >> 16
    for rb, rs, words in runs:
        if rb != bank or not (rs <= v < rs + 2 * len(words)): continue
        for k in range((v - rs + 1) // 2, len(words)):
            t = (bank << 16) | words[k]
            if mn == 'JSR': callsites[t].append(nxt(a, e))
            else: jumps_in[t].append(a)
            indirect_ok.add(t)
        break
# a table entry is fully accounted for when every table holding it has a known dispatcher
unknown_tables = set()
for rb, rs, words in runs:
    has = any(e[4] == 'absxi' and e[3] in ('JSR', 'JMP') and (a >> 16) == rb and rs <= e[7] < rs + 2 * len(words)
              for a, e in []) 
datarefs -= indirect_ok
liveout = {pc: 0 for pc in routines}
forced = {pc for pc in routines if pc in datarefs or (not callsites.get(pc) and not jumps_in.get(pc))}
for pc in forced: liveout[pc] = ALL
def scan(start, caller_routines):
    """live set at `start` within the caller's code"""
    memo = {}
    def live_at(a, depth):
        if a in memo: return memo[a]
        if depth > 400 or a not in code: return ALL
        memo[a] = 0                      # loop: assume nothing new (fixpoint-ish)
        e = code[a]; mn, mode = e[3], e[4]
        use, d, kind = usedef(mn, mode, e[7])
        if kind == 'stop': r = ALL
        elif kind == 'call':
            t = target(a, e)
            if t is None or t not in inputs: r = ALL
            else: r = inputs[t] | live_at(nxt(a, e), depth + 1)
        elif kind == 'ret':
            r = 0
            for o in owner[a]: r |= liveout.get(o, ALL)
        elif kind == 'jmp':
            t = target(a, e)
            r = use | (live_at(t, depth + 1) if t is not None and mode != 'absxi' and mn == 'JMP' else ALL)
        else:
            after = 0
            if mn in ('BRA', 'BRL'): after = live_at(target(a, e), depth + 1)
            elif mode in ('rel',):
                after = live_at(target(a, e), depth + 1) | live_at(nxt(a, e), depth + 1)
            else: after = live_at(nxt(a, e), depth + 1)
            r = use | (after & ~d)
        memo[a] = r
        return r
    return live_at(start, 0)
# inputs[pc]: registers/flags a routine may read before writing (callee calls use the
# callee's inputs; a return reads nothing more)
inputs = {pc: ALL for pc in routines}
def scan_inputs(start):
    memo = {}
    def at(a, depth):
        if a in memo: return memo[a]
        if depth > 400 or a not in code: return ALL
        memo[a] = 0
        e = code[a]; mn, mode = e[3], e[4]
        use, d, kind = usedef(mn, mode, e[7])
        if kind == 'stop': r = ALL
        elif kind == 'ret': r = 0
        elif kind == 'call':
            t = target(a, e)
            r = ALL if t is None or t not in inputs else inputs[t] | at(nxt(a, e), depth + 1)
        elif kind == 'jmp':
            t = target(a, e)
            r = use | (at(t, depth + 1) if t is not None and mode != 'absxi' and mn == 'JMP' else ALL)
        else:
            if mn in ('BRA', 'BRL'): after = at(target(a, e), depth + 1)
            elif mode == 'rel': after = at(target(a, e), depth + 1) | at(nxt(a, e), depth + 1)
            else: after = at(nxt(a, e), depth + 1)
            r = use | (after & ~d)
        memo[a] = r
        return r
    return at(start, 0)
for it in range(30):
    ch = 0
    for pc in routines:
        v = scan_inputs(pc)
        if v != inputs[pc]: inputs[pc] = v; ch += 1
    if not ch: break
for it in range(30):
    changed = 0
    for callee, rets in callsites.items():
        if callee not in liveout or callee in forced: continue
        l = liveout[callee]
        for ra in rets: l |= scan(ra, owner.get(ra))
        for ja in jumps_in.get(callee, []):           # tail jump: caller's live-out
            for o in owner[ja]: l |= liveout.get(o, ALL)
        if l != liveout[callee]: liveout[callee] = l; changed += 1
    for callee, js in jumps_in.items():
        if callee in callsites or callee not in liveout or callee in forced: continue
        l = liveout[callee]
        for ja in js:
            for o in owner[ja]: l |= liveout.get(o, ALL)
        if l != liveout[callee]: liveout[callee] = l; changed += 1
    if not changed: break
with open(sys.argv[3], 'w') as f:
    f.write('/* generated by tools/decomp/liveness.py: outputs callers can read (A X Y N V Z C = 1 2 4 8 16 32 64) */\n')
    for pc in sorted(liveout): f.write(f'{{0x{pc:06X}, 0x{liveout[pc]:02X}}},\n')
cnt = collections.Counter(liveout.values())
print('iterations', it + 1, 'routines', len(liveout), 'forced-all', len(forced), 'all-live', cnt[ALL], 'none-live', cnt[0])
if os.environ.get('LIVE_DEBUG'):
    nocall = {pc for pc in routines if not callsites.get(pc) and not jumps_in.get(pc)}
    print('no static caller', len(nocall), 'dataref', len(datarefs & routines), 'both', len(nocall & datarefs))
    for q in os.environ['LIVE_DEBUG'].split():
        q = int(q, 16); print(hex(q), 'calls', [hex(x) for x in callsites.get(q, [])][:5], 'jumps', [hex(x) for x in jumps_in.get(q, [])][:5], 'dataref', q in datarefs, 'inputs', inputs.get(q))
