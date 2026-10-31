#!/usr/bin/env python3
"""Self-time profile of one thread with Wine PE DLLs resolved to functions.

Usage: symbolize-profile.py <perf.data> <tid> [top N]
Wine's PE builtins are stripped; their DWARF comes from Arch's wine-debug
package (extracted to arch-wine/dbg). The code is byte-identical to the
installed CachyOS wine 11.20-1.1 (only timestamp/CRC differ), so it matches.
FileAlignment == SectionAlignment, so the perf dso offset is the RVA.
Callchains through PE code are unusable (no frame pointers), so only the
sampled leaf frame counts. DLLs from the wine-mao install (our unstripped
build) are their own debug files.
"""
import collections, os, re, subprocess, sys

HERE = os.path.dirname(os.path.abspath(__file__))
DBG = os.path.join(HERE, "arch-wine/dbg/usr/lib/debug/usr/lib/wine/x86_64-windows")
SYSDLL = "/usr/lib/wine/x86_64-windows"
# DLLs that are not the system build: dso path substring -> unstripped build of the same code
OVERRIDES = {"system32/d2d1.dll": os.environ.get("D2D1_DLL", "build/dlls/d2d1/x86_64-windows/d2d1.dll")}

data, tid = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 40

out = subprocess.run(["perf", "script", "-i", data, "--tid", tid, "-F", "ip,sym,dso,dsoff", "-G"],
                     capture_output=True, text=True).stdout
# -G hides callchains: one line per sample: "ip sym (dso+0xoff)"
line_re = re.compile(r"^\s*([0-9a-f]+)\s+(.*?)\s*\((.*?)(?:\+0x([0-9a-f]+))?\)\s*$")
samples = []
for line in out.splitlines():
    m = line_re.match(line)
    if m:
        ip, sym, dso, off = m.groups()
        name = os.path.basename(dso)
        if "/wine-mao/" in dso and dso.endswith(".dll"):
            name = "self:" + dso           # our build is unstripped: the DLL is its own debug file
        for pat in OVERRIDES:
            if pat in dso:
                name = "custom:" + pat
        samples.append((name, sym, int(off, 16) if off else int(ip, 16)))

bases = {}
def debug_obj(dll):
    if dll.startswith("self:"):
        return dll[5:]
    if dll.startswith("custom:"):
        return OVERRIDES[dll[7:]]
    return os.path.join(DBG, dll + ".debug")

def image_base(dll):
    if dll not in bases:
        img = (dll[5:] if dll.startswith("self:") else
               OVERRIDES[dll[7:]] if dll.startswith("custom:") else os.path.join(SYSDLL, dll))
        txt = subprocess.run(["llvm-readobj", "--file-headers", img],
                             capture_output=True, text=True).stdout
        m = re.search(r"ImageBase: (0x[0-9a-f]+)", txt)
        bases[dll] = int(m.group(1), 16) if m else None
    return bases[dll]

# resolve PE addresses in one llvm-symbolizer call
queries = sorted({(d, o) for d, s, o in samples
                  if d.endswith(".dll") and os.path.exists(debug_obj(d))
                  and image_base(d) is not None})
resolved = {}
if queries:
    inp = "".join(f"{debug_obj(d)} 0x{image_base(d) + o:x}\n" for d, o in queries)
    res = subprocess.run(["llvm-symbolizer", "--no-inlines", "--basenames"], input=inp,
                         capture_output=True, text=True).stdout.strip().split("\n\n")
    for (d, o), r in zip(queries, res):
        lines = r.strip().splitlines()
        fn = lines[0] if lines else "??"
        src = lines[1].split(":")[0] if len(lines) > 1 else "?"
        resolved[(d, o)] = (fn, src)

by_fn, by_src, by_dso = collections.Counter(), collections.Counter(), collections.Counter()
def short(d):
    return os.path.basename(d) if d.startswith("self:") else d

for d, s, o in samples:
    if (d, o) in resolved:
        fn, src = resolved[(d, o)]
    elif d.startswith("perf-") or d == "[JIT]":
        d, fn, src = "plugin(PE, anon)", "-", "-"
    else:
        fn, src = (s if s and s != "[unknown]" else f"0x{o:x}"), "-"
    d = short(d)
    by_fn[(d, fn)] += 1
    by_src[(d, src)] += 1
    by_dso[d] += 1

n = len(samples)
print(f"{n} samples on tid {tid}")
for title, c, k in (("by library", by_dso, top), ("by source file", by_src, top), ("by function", by_fn, top)):
    print(f"\n== {title}")
    for key, v in c.most_common(k):
        print(f"{100*v/n:6.2f}%  {key if isinstance(key, str) else '  '.join(key)}")
