#!/usr/bin/env python3
"""Watch one region in two X windows at once (e.g. Wine's offscreen Vulkan client window and the
visible window it is copied to) and print when each one changes.
Usage: dual-capture.py <visible id> <offscreen id> <secs> x0,y0,x1,y1"""
import sys, time
from Xlib import display, X
from PIL import Image, ImageChops
d = display.Display(":0")
vis, off = (d.create_resource_object("window", int(a, 16)) for a in sys.argv[1:3])
secs = float(sys.argv[3]); x0, y0, x1, y1 = map(int, sys.argv[4].split(","))
def grab(w):
    raw = w.get_image(x0, y0, x1 - x0, y1 - y0, X.ZPixmap, 0xFFFFFFFF)
    return Image.frombytes("RGB", (x1 - x0, y1 - y0), raw.data, "raw", "BGRX")
pv = po = None; end = time.monotonic() + secs
while time.monotonic() < end:
    t = time.monotonic(); v, o = grab(vis), grab(off)
    if pv is not None:
        cv = ImageChops.difference(v, pv).getbbox() is not None
        co = ImageChops.difference(o, po).getbbox() is not None
        if cv or co: print(f"{t:.3f} {'VIS' if cv else '   '} {'OFF' if co else ''}", flush=True)
    pv, po = v, o
    time.sleep(0.004)
