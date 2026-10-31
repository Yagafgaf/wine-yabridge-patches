#!/usr/bin/env python3
"""Drag a knob in an X11 (XWayland) window with XTest and log the time of each mouse step.

Usage: xtest-drag.py <window name pattern> <x> <y> <dy total> <seconds> [steps]
  x, y are window-relative; the pointer presses there, moves dy pixels (negative = up)
  in `steps` steps spread over `seconds`, then releases. Prints CLOCK_MONOTONIC times.
"""
import os, sys, time
from Xlib import display, X
from Xlib.ext import xtest

d = display.Display(os.environ.get("DISPLAY", ":0"))
root = d.screen().root


def walk(w):
    try:
        name = w.get_wm_name()
    except Exception:
        name = None
    yield w, name
    try:
        for c in w.query_tree().children:
            yield from walk(c)
    except Exception:
        pass


pat, x, y, dy, secs = sys.argv[1], int(sys.argv[2]), int(sys.argv[3]), int(sys.argv[4]), float(sys.argv[5])
steps = int(sys.argv[6]) if len(sys.argv) > 6 else 60
win = next(w for w, n in walk(root) if n and pat.lower() in str(n).lower() and w.get_geometry().width > 50)
c = root.translate_coords(win, x, y)
ax, ay = c.x, c.y

xtest.fake_input(d, X.MotionNotify, x=ax, y=ay); d.sync()
time.sleep(0.15)
print(f"{time.monotonic():.3f} press {ax},{ay}", flush=True)
xtest.fake_input(d, X.ButtonPress, 1); d.sync()
time.sleep(0.05)
for i in range(1, steps + 1):
    yy = ay + round(dy * i / steps)
    xtest.fake_input(d, X.MotionNotify, x=ax, y=yy); d.sync()
    if i == steps or i % 10 == 0:
        print(f"{time.monotonic():.3f} move {i} y={yy}", flush=True)
    time.sleep(secs / steps)
xtest.fake_input(d, X.ButtonRelease, 1); d.sync()
print(f"{time.monotonic():.3f} release", flush=True)
