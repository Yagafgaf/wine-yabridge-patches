#!/usr/bin/env python3
"""Capture an X11 window (XWayland too) repeatedly and report when its picture settles.

Usage:
  win-capture.py list [pattern]           list windows whose WM_NAME matches pattern
  win-capture.py shot <pattern> <out.png>  one screenshot of the first matching window
  win-capture.py watch <pattern> <secs> [step=0.02] [outdir] [x0,y0,x1,y1]
      grab every step seconds for secs; print time and difference from the previous frame,
      so you can see when the picture stops changing. Frames go to outdir if given.
      With a region only that part of the window is grabbed (faster, smaller files).
Times are CLOCK_MONOTONIC seconds (same clock as REAPER's time_precise on Linux).
"""
import os, sys, time
from Xlib import display, X
from PIL import Image, ImageChops

d = display.Display(os.environ.get("DISPLAY", ":0"))


def walk(w, depth=0):
    try:
        name = w.get_wm_name()
    except Exception:
        name = None
    yield w, name, depth
    try:
        for c in w.query_tree().children:
            yield from walk(c, depth + 1)
    except Exception:
        pass


def find(pattern):
    for w, name, _ in walk(d.screen().root):
        if name and pattern.lower() in str(name).lower():
            g = w.get_geometry()
            if g.width > 50 and g.height > 50:
                return w
    sys.exit(f"no window matching {pattern!r}")


def grab(w, region=None):
    if region:
        x0, y0, x1, y1 = region
    else:
        g = w.get_geometry()
        x0, y0, x1, y1 = 0, 0, g.width, g.height
    raw = w.get_image(x0, y0, x1 - x0, y1 - y0, X.ZPixmap, 0xFFFFFFFF)
    return Image.frombytes("RGB", (x1 - x0, y1 - y0), raw.data, "raw", "BGRX")


cmd = sys.argv[1]
if cmd == "list":
    pat = (sys.argv[2] if len(sys.argv) > 2 else "").lower()
    for w, name, depth in walk(d.screen().root):
        if name and pat in str(name).lower():
            g = w.get_geometry()
            print(f"{'  ' * depth}{w.id:#x} {g.width}x{g.height} {name!r}")
elif cmd == "shot":
    grab(find(sys.argv[2])).save(sys.argv[3])
elif cmd == "watch":
    w = find(sys.argv[2])
    secs = float(sys.argv[3])
    step = float(sys.argv[4]) if len(sys.argv) > 4 else 0.02
    outdir = sys.argv[5] if len(sys.argv) > 5 and sys.argv[5] != "-" else None
    region = tuple(int(v) for v in sys.argv[6].split(",")) if len(sys.argv) > 6 else None
    if outdir:
        os.makedirs(outdir, exist_ok=True)
    prev = None
    t_end = time.monotonic() + secs
    i = 0
    while time.monotonic() < t_end:
        t = time.monotonic()
        im = grab(w, region)
        diff = 0
        if prev is not None:
            bbox = ImageChops.difference(im, prev).getbbox()
            diff = 0 if bbox is None else (bbox[2] - bbox[0]) * (bbox[3] - bbox[1])
        print(f"{t:.3f} {diff}" + (f" {bbox}" if prev is not None and diff else ""), flush=True)
        if outdir and (diff or prev is None):
            im.save(f"{outdir}/{i:04d}-{t:.3f}.png")
        prev = im
        i += 1
        time.sleep(max(0, step - (time.monotonic() - t)))
