"""Every routine entry the recompiler's analysis knows (AOT and interpreted),
from src/gen/program_manifest.json: list of (pc24, m, x, disposition)."""
import json, os
def load(manifest=None):
    root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
    d = json.load(open(manifest or f'{root}/src/gen/program_manifest.json'))
    out = []
    for k, v in d['nodes'].items():
        pc, mx = k.split(':')
        out.append((int(pc, 16), int(mx[1]), int(mx[3]), v['disposition']))
    return sorted(out)
