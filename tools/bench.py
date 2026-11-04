#!/usr/bin/env python3
"""bench.py - repeatable timings for the d2d1/dwrite/windowscodecs patches.

The scenarios reproduce the numbers in the README:

    open      window + hwnd render target + a frame (d2dtest, one line of text)
    text200   the same frame, 200 lines of text
    faces100  100 new fonts from memory (d2dtest, face_count)
    fonttest  EnumFontFamiliesEx / CreateFont+metrics / DirectWrite collection

Usage:

    tools/bench.py run --wine ~/.local/lib/wine-mao/bin/wine --label wine-mao \\
        --prefix ~/.cache/wine-bench/wine-mao --out bench/wine-mao.json

    tools/bench.py compare --baseline bench/wine-mao.json \\
        --wine /path/to/other/bin/wine --label other

The metric is the median over runs; the first run is dropped as warm-up.
A regression is an increase of more than --threshold percent and more than
--min-delta ms at the same time (percentages on values like 0.3 ms are noise).

The harness installs nothing and leaves your own prefixes alone: every
Wine build gets its own WINEPREFIX, created on the first run.
"""

import argparse
import json
import os
import platform
import re
import shutil
import statistics
import subprocess
import sys
import tempfile
import time

SCHEMA = 1
TOOLS_DIR = os.path.dirname(os.path.abspath(__file__))

D2D_RE = re.compile(
    r"^run (\d+): window ([\d.]+)\s+rendertarget ([\d.]+)\s+"
    r"first-frame ([\d.]+)\s+teardown ([\d.]+)\s+total ([\d.]+) ms"
    r"\s+\(compatible RTs ([\d.]+)\)"
)
FACTORIES_RE = re.compile(r"^factories ([\d.]+) ms")
FONT_RE = re.compile(
    r"^run (\d+): EnumFontFamiliesEx ([\d.]+) ms \((\d+)\)\s+"
    r"CreateFont\+metrics ([\d.]+) ms\s+DWriteFactory ([\d.]+) ms\s+"
    r"SystemFontCollection ([\d.]+) ms \((\d+) families\)"
)

D2D_FIELDS = ["window", "rendertarget", "first-frame", "teardown", "total", "compat-rt"]
FONT_FIELDS = ["enum-families", "createfont", "dwrite-factory", "font-collection"]


def log(msg):
    print(msg, file=sys.stderr, flush=True)


def wine_dir(wine):
    return os.path.dirname(os.path.abspath(wine))


def wine_env(prefix):
    env = dict(os.environ)
    env["WINEPREFIX"] = prefix
    env["WINEDEBUG"] = "-all"
    env["WINEDLLOVERRIDES"] = "mscoree,mshtml="
    return env


def wine_version(wine):
    try:
        return subprocess.run([wine, "--version"], capture_output=True, text=True,
                              timeout=60).stdout.strip()
    except (OSError, subprocess.SubprocessError):
        return "unknown"


def ensure_prefix(wine, prefix):
    if os.path.isdir(os.path.join(prefix, "drive_c")):
        return
    log(f"creating prefix {prefix} ...")
    os.makedirs(prefix, exist_ok=True)
    subprocess.run([wine, "wineboot", "-u"], env=wine_env(prefix),
                   capture_output=True, timeout=600)
    if not os.path.isdir(os.path.join(prefix, "drive_c")):
        sys.exit(f"prefix was not created: {prefix}")


def build_exes(wine, outdir):
    """Build the tests with winegcc from the same build as wine (same include).

    Without mingw, winegcc makes a winelib ELF <name>.exe.so plus a wrapper <name>.exe
    that runs wine from PATH - not the Wine we
    built for. So we return the path to the .exe.so (or to the PE .exe if the build
    cross-compiles a real PE)."""
    winegcc = os.path.join(wine_dir(wine), "winegcc")
    if not os.path.exists(winegcc):
        sys.exit(f"no {winegcc}; winegcc must sit next to wine")
    targets = {"d2dtest": ["-ld2d1", "-ldwrite", "-luuid"],
               "fonttest": ["-lgdi32", "-luser32", "-ldwrite"]}
    paths = {}
    for name, libs in targets.items():
        exe = os.path.join(outdir, name + ".exe")
        src = os.path.join(TOOLS_DIR, name + ".c")
        cmd = [winegcc, "-O2", "-o", exe, src, *libs]
        res = subprocess.run(cmd, capture_output=True, text=True, timeout=300)
        if res.returncode != 0:
            sys.exit(f"building {name} failed:\n{res.stderr[-2000:]}")
        elf = exe + ".so"
        if os.path.exists(elf):
            log(f"{name}: winelib-ELF {elf} (not using the wrapper {exe})")
            paths[name] = elf
        else:
            paths[name] = exe
    return paths


def run_exe(wine, prefix, exe, args, timeout=900):
    """Return stdout. A non-zero exit code is not an error: some
    Wine builds exit with a garbage code after the data is already printed."""
    cmd = [wine, exe, *[str(a) for a in args]]
    res = subprocess.run(cmd, env=wine_env(prefix), capture_output=True, text=True,
                         timeout=timeout)
    return res.stdout, res.returncode, res.stderr


def parse_d2d(out, runs):
    raw = {f: [] for f in D2D_FIELDS}
    raw["factories"] = []
    for line in out.splitlines():
        line = line.strip()
        m = FACTORIES_RE.match(line)
        if m:
            raw["factories"].append(float(m.group(1)))
            continue
        m = D2D_RE.match(line)
        if not m:
            continue
        for field, value in zip(D2D_FIELDS, m.groups()[1:]):
            raw[field].append(float(value))
    return raw


def parse_font(out, runs):
    raw = {f: [] for f in FONT_FIELDS}
    for line in out.splitlines():
        m = FONT_RE.match(line.strip())
        if not m:
            continue
        raw["enum-families"].append(float(m.group(2)))
        raw["createfont"].append(float(m.group(4)))
        raw["dwrite-factory"].append(float(m.group(5)))
        raw["font-collection"].append(float(m.group(6)))
    return raw


def scenario_specs(runs):
    """Name -> (exe, argv, parser)."""
    return {
        "open": ("d2dtest", [runs, 0, 1], parse_d2d),
        "text200": ("d2dtest", [runs, 0, 200], parse_d2d),
        "faces100": ("d2dtest", [runs, 0, 1, 0, 100], parse_d2d),
        "fonttest": ("fonttest", [], parse_font),
    }


def medians(raw):
    """Median without the first run: the first one includes warm-up (factories, font cache)."""
    out = {}
    for field, values in raw.items():
        vals = values[1:] if len(values) > 1 else values
        if vals:
            out[field] = round(statistics.median(vals), 2)
    return out


def cmd_run(args):
    exes = args.exes or build_exes(args.wine, tempfile.mkdtemp(prefix="wine-bench-exes-"))
    if isinstance(exes, str):  # --exes: a directory with ready-made tests
        found = {}
        for name in ("d2dtest", "fonttest"):
            elf = os.path.join(exes, name + ".exe.so")
            found[name] = elf if os.path.exists(elf) else os.path.join(exes, name + ".exe")
        exes = found
    ensure_prefix(args.wine, args.prefix)

    try:
        load1 = round(os.getloadavg()[0], 2)
    except OSError:
        load1 = None
    ncpu = os.cpu_count() or 1
    if load1 is not None and load1 > ncpu * 0.5:
        log(f"warning: load average {load1} on {ncpu} cores - the timings will be noisy, "
            f"do not compare them with a baseline")

    result = {
        "schema": SCHEMA,
        "label": args.label,
        "wine": os.path.abspath(args.wine),
        "wine_version": wine_version(args.wine),
        "date": time.strftime("%Y-%m-%dT%H:%M:%S"),
        "runs": args.runs,
        "host": {
            "cpu": platform.processor() or platform.machine(),
            "kernel": platform.release(),
            "load1": load1,
        },
        "scenarios": {},
    }

    for name, (exe, argv, parser) in scenario_specs(args.runs).items():
        log(f"{args.label}: {name} ...")
        exe_path = exes[exe]
        if args.warmup:
            run_exe(args.wine, args.prefix, exe_path, argv)  # cache warm-up, not counted
        out, code, err = run_exe(args.wine, args.prefix, exe_path, argv)
        raw = parser(out, args.runs)
        if not any(raw.values()):
            sys.exit(f"{exe}: produced no parsable lines (exit code {code})\n"
                     f"{out[-1000:]}\n{err[-1000:]}")
        if code != 0:
            log(f"  note: {exe} exited with code {code} (data received)")
        result["scenarios"][name] = {"raw": raw, "median": medians(raw)}

    text = json.dumps(result, ensure_ascii=False, indent=2)
    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w") as f:
            f.write(text + "\n")
        log(f"written {args.out}")
    else:
        print(text)
    return 0


def flatten(doc):
    flat = {}
    for scenario, data in doc["scenarios"].items():
        for field, value in data["median"].items():
            flat[f"{scenario}.{field}"] = value
    return flat


def cmd_compare(args):
    with open(args.baseline) as f:
        baseline = json.load(f)
    if baseline.get("schema") != SCHEMA:
        log(f"warning: baseline schema {baseline.get('schema')}, expected {SCHEMA}")

    # the current run goes to a temporary json
    tmp = tempfile.mkdtemp(prefix="wine-bench-")
    current_path = os.path.join(tmp, "current.json")
    args.out = current_path
    cmd_run(args)
    with open(current_path) as f:
        current = json.load(f)

    base = flatten(baseline)
    now = flatten(current)

    print(f"baseline: {baseline['label']} ({baseline.get('wine_version')}, "
          f"{baseline['date']})")
    print(f"current:  {current['label']} ({current.get('wine_version')}, "
          f"{current['date']})")
    print()
    print(f"{'metric':<28}{'baseline':>10}{'now':>10}{'Δ%':>9}")

    regressions, improvements = 0, 0
    for key in sorted(set(base) | set(now)):
        if key not in base or key not in now:
            print(f"{key:<28}{'—':>10}{'—':>10}{'one side only':>9}")
            continue
        b, n = base[key], now[key]
        delta = n - b
        pct = (delta / b * 100.0) if b else 0.0
        if delta > args.min_delta and pct > args.threshold:
            mark, regressions = "REGRESSION", regressions + 1
        elif delta < -args.min_delta and pct < -args.threshold:
            mark, improvements = "better", improvements + 1
        else:
            mark = ""
        print(f"{key:<28}{b:>10.2f}{n:>10.2f}{pct:>+8.1f}%  {mark}")

    print()
    print(f"total: better {improvements}, worse {regressions}, "
          f"threshold {args.threshold:.0f}% and {args.min_delta:.1f} ms")
    if args.json:
        shutil.copy(current_path, args.json)
        log(f"current run: {args.json}")
    return 1 if regressions else 0


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = parser.add_subparsers(dest="command", required=True)

    def common(p):
        p.add_argument("--wine", required=True, help="path to the bin/wine of the build under test")
        p.add_argument("--label", required=True, help="build name for the report")
        p.add_argument("--prefix", help="WINEPREFIX for the timings (default "
                                        "~/.cache/wine-bench/<label>)")
        p.add_argument("--runs", type=int, default=7, help="runs per scenario (7)")
        p.add_argument("--exes", help="directory with ready-made d2dtest.exe/fonttest.exe")
        p.add_argument("--warmup", type=int, default=1, help="warm-ups per scenario (1)")
        p.add_argument("--out", help="where to write the JSON")

    p_run = sub.add_parser("run", help="take timings and write a baseline")
    common(p_run)
    p_run.set_defaults(func=cmd_run)

    p_cmp = sub.add_parser("compare", help="compare with a baseline")
    common(p_cmp)
    p_cmp.add_argument("--baseline", required=True)
    p_cmp.add_argument("--threshold", type=float, default=8.0,
                       help="regression threshold in percent (8)")
    p_cmp.add_argument("--min-delta", type=float, default=1.0, dest="min_delta",
                       help="minimum absolute increase in ms (1.0)")
    p_cmp.add_argument("--json", help="save the current run to a file")
    p_cmp.set_defaults(func=cmd_compare)

    args = parser.parse_args()
    if not args.prefix:
        args.prefix = os.path.expanduser(f"~/.cache/wine-bench/{args.label}")
    args.prefix = os.path.expanduser(args.prefix)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
