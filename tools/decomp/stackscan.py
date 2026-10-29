#!/usr/bin/env python3
"""stackscan.py [-l] [-t] [pc24 ...]: decompiled routines whose callees can leave non-locally.

A decompiled routine calls other code through dc_jsr / dc_jsl / glue and expects it
to come back to the next instruction with the stack as it was.  Code that pops its
caller's return address or saved registers (PLA/PLX/PLY/PLP/PLB/PLD below its entry
depth), sets S (TCS/TXS), returns with RTS/RTL/RTI at a nonzero stack depth, ends with
the other kind of return than the call used, or jumps / calls through a table the
analysis cannot resolve (like the 7E:398A / 39BE task lists) breaks that.

Every routine is walked with the stack depth and M/X tracked along each path (ROM
bytes; bank 7E from the module C3 unpacks to 7E:3000, with the ram_routine bytes of
recomp/bank00.cfg on top; after a call the callee's exit width from the manifest).
From each DC_ENTRY in src/decomp the calls of src/gen/program_manifest.json (which
also gives jump / call table targets) are followed SCAN_DEPTH (3) levels down, and
every routine on the way with an issue is listed with the call sites leading to it.
TABLES cuts tables the manifest reads past their end; REVIEWED / REVIEWED_MODES are
leaves checked by hand.  Exit status is not used; an empty listing is the goal.
  -l   also list the distinct leaves     -t   (with pc24) print the path to each issue
With pc24 arguments it prints the walk of those routines instead."""
import sys, os, re, json, glob, collections
sys.setrecursionlimit(100000)
here = os.path.dirname(os.path.abspath(__file__))
root = os.path.dirname(os.path.dirname(here))
sys.path.insert(0, os.path.join(root, 'tools', 'dev'))
from dis65816 import OPS, SIZE

rom = open(f'{root}/ct.sfc', 'rb').read()
def unlz(bank, start, dst):
    """The C3:0557 LZ format (see src/decomp/decompress.c): {address: byte}."""
    rb = lambda a: rom[((bank & 0x3F) << 16) | (a & 0xFFFF)]
    rw = lambda a: rb(a) | rb(a + 1) << 8
    out = {}; x = start + 2; end = (x + rw(start)) & 0xFFFF
    long_len = rb(end) & 0xC0
    mask, shift = (0x07FF, 3) if long_len else (0x0FFF, 4)
    y = dst; cnt = 8
    while True:
        if x == end:
            c = rb(x) & 0x3F
            if not c: return out
            cnt = c; end = (rw(x + 1) + start) & 0xFFFF; x += 3; continue
        f = rb(x)
        if not f:
            for k in range(1, 9): out[y] = rb(x + k); y += 1
            x += 9; cnt = 8; continue
        x += 1; bits = f
        while True:
            if bits & 1:
                n = (rb(x + 1) >> shift) + 3; fr = y - (rw(x) & mask)
                for k in range(n): out[y] = out.get(fr + k, 0); y += 1
                x += 2
            else: out[y] = rb(x); y += 1; x += 1
            cnt -= 1
            if not cnt: cnt = 8; break
            bits >>= 1
# Bank 7E code: the module C3:0000 unpacks to 7E:3000 (source word at FE:0003), with the
# bytes captured at run time (ram_routine lines) on top.
ram = {0x7E0000 | a: b for a, b in unlz(0xC3, rom[0x3E0003] | rom[0x3E0004] << 8, 0x3000).items()}
for m in re.finditer(r'^ram_routine (7E[0-9A-F]{4}) \S+ ([0-9A-F]+)', open(f'{root}/recomp/bank00.cfg').read(), re.M):
    a = int(m.group(1), 16)
    for k, b in enumerate(bytes.fromhex(m.group(2))): ram[a + k] = b
def rd(a):
    if (a >> 16) in (0x7E, 0x7F): return ram.get(a)
    if (a >> 16) < 0x40 and (a & 0xFFFF) < 0x8000: return None   # WRAM / I/O mirrors
    return rom[a & 0x3FFFFF]
def decode(pc, m, x):
    op = rd(pc)
    if op is None: return None
    mn, mode = OPS[op]
    n = (1 if m else 2) if mode == 'immm' else (1 if x else 2) if mode == 'immx' else SIZE.get(mode, 1)
    b = [rd((pc & 0xFF0000) | ((pc + 1 + k) & 0xFFFF)) for k in range(n)]
    if None in b: return None
    return mn, mode, n, sum(v << (8 * k) for k, v in enumerate(b))

_mf = json.load(open(f'{root}/src/gen/program_manifest.json'))
man = _mf['nodes']
exit_mode = {k: (v['m'], v['x']) for k, v in _mf['exit_modes'].items() if v}
nodes = collections.defaultdict(list)          # pc -> [(m, x)]
calls = collections.defaultdict(set)           # (pc, m, x) -> {(site, target pc, tm, tx, kind)}
dispatch = collections.defaultdict(set)        # site -> {(target, m, x)}
dynamic = collections.defaultdict(set)         # (pc, m, x) -> {(site, kind, detail)}
for k, v in man.items():
    pc, mx = k.split(':'); pc = int(pc, 16); m, x = int(mx[1]), int(mx[3])
    nodes[pc].append((m, x))
    for d in v['demands']:
        t = d.get('target')
        if d['kind'] in ('dynamic_dispatch', 'unresolved_indirect', 'suppressed_indirect_call') or not t:
            dynamic[(pc, m, x)].add((d['site_pc24'], d['kind'], d['detail'][:40]))
            continue
        calls[(pc, m, x)].add((d['site_pc24'], t['pc24'], t['m'], t['x'], d['kind']))
        if d['kind'] == 'static_dispatch': dispatch[d['site_pc24']].add((t['pc24'], t['m'], t['x']))

PUSH = {'PHP': 1, 'PHB': 1, 'PHK': 1, 'PHD': 2, 'PEA': 2, 'PEI': 2, 'PER': 2}
PULL = {'PLP': 1, 'PLB': 1, 'PLD': 2}

def walk(start, m, x, maxn=20000, trace=None):
    """Issues of one routine body: [(kind, pc, detail)], its exit kinds at depth 0
    and the calls it makes [(site, target, m, x, 'JSR'|'JSL')]."""
    issues, exits, out = [], set(), []
    seen = set(); work = [(start, m, x, 0, ())]; parent = {}; prev = None
    def issue(kind, pc, det=''):
        if not any(i[0] == kind and i[1] == pc for i in issues):
            issues.append((kind, pc, det))
            if trace is not None:   # the path of (pc, m, x, depth) that led here
                t, st = [], prev
                while st: t.append(st); st = parent.get(st)
                trace.append((kind, pc, t[::-1]))
    while work and len(seen) < maxn:
        pc, m, x, dep, php, *src = work.pop() + (None,)
        prev = src[0] if src else None
        while True:
            st = (pc, m, x, dep, php)
            if st in seen: break
            seen.add(st)
            if prev is not None and st not in parent: parent[st] = prev
            prev = st
            d = decode(pc, m, x)
            if d is None: issue('unreadable', pc); break
            mn, mode, n, v = d
            nxt = (pc & 0xFF0000) | ((pc + 1 + n) & 0xFFFF)
            if mn == 'REP': m = 0 if v & 0x20 else m; x = 0 if v & 0x10 else x
            if mn == 'SEP': m = 1 if v & 0x20 else m; x = 1 if v & 0x10 else x
            if mn in PUSH or mn in ('PHA', 'PHX', 'PHY'):
                dep += PUSH.get(mn) or (2 - (m if mn == 'PHA' else x))
                if mn == 'PHP': php = php + ((m, x),)
            if mn in PULL or mn in ('PLA', 'PLX', 'PLY'):
                dep -= PULL.get(mn) or (2 - (m if mn == 'PLA' else x))
                if mn == 'PLP':
                    if php: m, x = php[-1]; php = php[:-1]
                    else: issue('plp-unknown-mx', pc)
                if dep < 0: issue('pops-caller-stack', pc, f'{mn} depth {dep}')
            if mn in ('TCS', 'TXS'): issue('sets-S', pc, mn); break
            if abs(dep) > 48: issue('stack-runaway', pc); break
            if mode in ('rel', 'rell'):
                dd = v - 256 if mode == 'rel' and v > 127 else (v - 65536 if mode == 'rell' and v > 32767 else v)
                t = (pc & 0xFF0000) | ((nxt + dd) & 0xFFFF)
                work.append((t, m, x, dep, php, st))
                if mn in ('BRA', 'BRL'): break
            if mn == 'JMP' and mode == 'abs': pc = (pc & 0xFF0000) | v; continue
            if mn == 'JML' and mode == 'long': pc = v; continue
            if mn in ('JSR', 'JSL') and mode in ('abs', 'long'):
                t = (pc & 0xFF0000) | v if mode == 'abs' else v
                out.append((pc, t, m, x, mn))
                m, x = exit_mode.get(f'{t:06X}:M{m}X{x}', (m, x))   # the width the callee returns with
            if mn in ('JSR', 'JMP') and mode == 'absxi' or mn in ('JMP', 'JML') and mode in ('absi', 'absil'):
                tg = dispatch.get(pc)
                if pc in TABLES:
                    tb, n = TABLES[pc]
                    tg = {((pc & 0xFF0000) | (rd(tb + 2 * i) | rd(tb + 2 * i + 1) << 8), m, x) for i in range(n)}
                if not tg: issue('unknown-indirect', pc, f'{mn} {mode} depth {dep}')
                for t, tm, tx in sorted(tg or ()):
                    if mn == 'JSR': out.append((pc, t, tm, tx, 'JSR'))
                    else: work.append((t, tm, tx, dep, php, st))
                if mn != 'JSR': break
            if mn in ('RTS', 'RTL'):
                if dep: issue('return-at-depth', pc, f'{mn} depth {dep}')
                else: exits.add(mn)
                break
            if mn == 'RTI': issue('rti', pc); break
            if mn in ('BRK', 'COP', 'STP', 'WAI'): break
            pc = nxt
    return issues, exits, out

# Reviewed leaves (pc -> (why, routines that contain the leaf's exit: harmless when the
# path from the decompiled code goes through one of them; empty = everywhere)).
REVIEWED = {
    0xC70A39: ('drops its caller frame and leaves through the exit of C7:0004, which then returns '
               'normally to its own caller (kept generated, see keep_generated.txt)', {0xC70004}),
    0x7E37F3: ('JSR (00F0,X) to the routine a C3 script command A0 names; the scripts of the 7E:3000 '
               'module call 7E:3F59, 7286, 7EE8, 82F5, 87CC, 9919 and 9FC8, which all return normally', ()),
    0xC30011: ('JMP (03F0) to its own edge entries 0F35 / 0F48 / 0F5B / 0F6E: polygon.c does the '
               'quad as one C loop, no call', ()),
    0xC284EC: ('L+R+Select+Start: JML C0:FF00 (soft reset, S reset at FD:C007, never returns); the '
               'decomp calls it with dc_jsl and returns after it, which is never reached', ()),
}
# Leaves that only fail in a width the code cannot run in (the analysis also tried it):
# (pc, m, x) -> why. Their stack only balances in the other width.
REVIEWED_MODES = {
    (0xC093E1, 1, 1): 'needs 16-bit X (PHX/PLY pairs); its callers run it with X 16-bit',
    **{(pc, 0, x): 'only works with 8-bit A (PHA / PLB pair, LDA # sizes); called with M = 1'
       for pc in (0xCFF03E, 0xCFF087, 0xCFF194, 0xCFF4B2, 0xCFF503, 0xCFF619, 0xCFF61F, 0xCFFA17)
       for x in (0, 1)},
}
# Jump / call tables whose real length is known (site -> (table pc, entries)):
# the manifest reads on past their end into data.
TABLES = {
    0xC1CFE1: (0xC1D126, 6),    # C1:8003 item / gold services 0-5 (ev_items.c uses 0-5)
    0xC70173: (0xC70AD9, 8),    # C7:0004 sound commands 10-17 ((cmd & 0F) * 2)
    # Event comparison operators 0-7 (8-bit table 6467, 16-bit 6477); ev_cond.c sends other
    # ops through dc_jsr to whatever the table bytes point at, as the original does.
    0xC06307: (0xC06467, 8), 0xC06398: (0xC06467, 8), 0xC06433: (0xC06467, 8),
    0xC0633D: (0xC06477, 8), 0xC063D7: (0xC06477, 8),
    # CF:EFAF battle movers, groups 1 and 2 (handlers 0-7; battle_cf.c sends others through dc_jsr).
    0xCFEFE6: (0xCFF01E, 8), 0xCFF016: (0xCFF02E, 8),
}

memo = {}
def info(pc, m, x):
    k = (pc, m, x)
    if k not in memo: memo[k] = walk(pc, m, x)
    return memo[k]

def leaves(pc, m, x, maxdepth=int(os.environ.get("SCAN_DEPTH", "3"))):
    """Every routine reachable from (pc, m, x) through calls (the walk itself follows
    jumps, up to maxdepth calls down) that has an issue, or that is called with the wrong kind of return:
    {(leaf, reason): path of call sites}."""
    found = {}; seen = {(pc, m, x): ()}; q = collections.deque([(pc, m, x)])
    while q:
        k = q.popleft(); path = seen[k]
        issues, exits, out = info(*k)
        if k in REVIEWED_MODES: continue
        for kind, at, det in issues:
            if kind == 'unreadable' and len(path) > 1: continue   # junk table targets / data
            found.setdefault((k[0], f'{kind} at {at:06X} {det}'.strip()), path)
        for site, t, tm, tx, ck in out:
            if (t >> 16) < 0xC0 and (t >> 16) not in (0x7E, 0x7F): continue   # table data read as targets
            te = info(t, tm, tx)[1]
            want = 'RTS' if ck == 'JSR' else 'RTL'
            if te and want not in te:
                found.setdefault((t, f'{ck} at {site:06X}, but it ends with {"/".join(sorted(te))}'), path + ((site, t),))
            if (t, tm, tx) not in seen and len(path) < maxdepth:
                seen[(t, tm, tx)] = path + ((site, t),); q.append((t, tm, tx))
    return found

def decompiled():
    out = {}
    for f in sorted(glob.glob(f'{root}/src/decomp/*.c')):
        for m in re.finditer(r'DC_ENTRY\(([0-9A-F]{6}), (RTS|RTL)\)', open(f).read()):
            out[int(m.group(1), 16)] = (os.path.basename(f), m.group(2))
    return out

def fmt_path(path): return ''.join(f' {p:06X} ->' for p, _ in path)

if __name__ == '__main__':
    args = [a for a in sys.argv[1:] if not a.startswith('-')]
    if args:
        for a in args:
            pc = int(a, 16)
            for m, x in nodes.get(pc, [(1, 0)]):
                iss, ex, out = info(pc, m, x)
                print(f'{pc:06X} m{m}x{x} exits={sorted(ex)} issues={[(i[0], f"{i[1]:06X}", i[2]) for i in iss]}')
                for (leaf, why), path in sorted(leaves(pc, m, x).items()):
                    print(f'   via{fmt_path(path)} {leaf:06X}: {why}')
                if '-t' in sys.argv:
                    tr = []; walk(pc, m, x, trace=tr)
                    for kind, at, t in tr:
                        print(f'   trace to {kind} at {at:06X}: ' + ' '.join(f'{a & 0xFFFF:04X}' + (f'[{d}]' if i == 0 or d != t[i - 1][3] else '') for i, (a, mm, xx, d, _) in enumerate(t)))
        sys.exit()
    dc = decompiled()
    hits = 0; allleaves = collections.defaultdict(set)
    for pc in sorted(dc):
        f, ret = dc[pc]
        lines = []
        for m, x in nodes.get(pc, []):
            if (pc, m, x) in REVIEWED_MODES: continue
            for site, kind, det in sorted(dynamic.get((pc, m, x), ())) if pc not in REVIEWED else ():
                lines.append(f'm{m}x{x} {kind} at {site:06X} {det}')
            iss, ex, out = info(pc, m, x)
            if ex and ret not in ex: lines.append(f'm{m}x{x} DC_ENTRY says {ret} but the code ends with {"/".join(sorted(ex))}')
            for (leaf, why), path in sorted(leaves(pc, m, x).items()):
                if leaf in REVIEWED and (not REVIEWED[leaf][1] or any(t in REVIEWED[leaf][1] for _, t in path)): continue
                lines.append(f'm{m}x{x} via{fmt_path(path)} {leaf:06X}: {why}')
                allleaves[(leaf, why)].add(pc)
        if lines:
            hits += 1
            print(f'{pc:06X} ({f})')
            for l in dict.fromkeys(lines): print('   ' + l)
    print(f'{hits} of {len(dc)} decompiled entries flagged, {len(allleaves)} distinct leaves', file=sys.stderr)
    if '-l' in sys.argv:
        for (leaf, why), ps in sorted(allleaves.items()): print(f'LEAF {leaf:06X} {why}: {len(ps)} entries')
