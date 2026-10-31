#!/usr/bin/env python3
"""Callchains for samples whose leaf function matches a pattern (Wine PE frames resolved).
Usage: callers.py <perf.data> <tid> <leaf regex> [depth]"""
import collections, os, re, subprocess, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
data, tid, pat = sys.argv[1], sys.argv[2], re.compile(sys.argv[3])
depth = int(sys.argv[4]) if len(sys.argv) > 4 else 8
out = subprocess.run(["perf", "script", "-i", data, "--tid", tid, "-F", "ip,sym,dso,dsoff"],
                     capture_output=True, text=True).stdout
fr = re.compile(r"^\s*([0-9a-f]+)\s+(.*?)\s*\((.*?)(?:\+0x([0-9a-f]+))?\)\s*$")
samples, cur = [], []
for line in out.splitlines() + [""]:
    m = fr.match(line)
    if m:
        cur.append((m.group(3), m.group(2), int(m.group(4), 16) if m.group(4) else 0))
    elif cur:
        samples.append(cur); cur = []
bases = {}
def base(path):
    if path not in bases:
        t = subprocess.run(["llvm-readobj", "--file-headers", path], capture_output=True, text=True).stdout
        m = re.search(r"ImageBase: (0x[0-9a-f]+)", t); bases[path] = int(m.group(1), 16) if m else 0
    return bases[path]
D2D = os.environ.get("D2D1_DLL", "build/dlls/d2d1/x86_64-windows/d2d1.dll")
def obj(dso):
    if "/wine-mao/" in dso and dso.endswith(".dll"): return dso
    if dso.endswith("system32/d2d1.dll"): return D2D
    return None
q = sorted({(obj(d), o) for s in samples for d, _, o in s if obj(d)})
inp = "".join(f"{p} 0x{base(p) + o:x}\n" for p, o in q)
res = subprocess.run(["llvm-symbolizer", "--no-inlines"], input=inp, capture_output=True, text=True).stdout.strip().split("\n\n")
names = {k: r.strip().splitlines()[0] for k, r in zip(q, res)}
def name(f):
    d, s, o = f
    if obj(d): return f"{os.path.basename(d)}:{names[(obj(d), o)]}"
    if "perf-" in d: return "plugin"
    return f"{os.path.basename(d)}:{s if s and s != '[unknown]' else hex(o)}"
c = collections.Counter()
hits = 0
for s in samples:
    chain = [name(f) for f in s]
    if pat.search(chain[0]):
        hits += 1
        c[" <- ".join(chain[:depth])] += 1
print(f"{hits} of {len(samples)} samples")
for k, v in c.most_common(15):
    print(f"{v:5d}  {k}")
