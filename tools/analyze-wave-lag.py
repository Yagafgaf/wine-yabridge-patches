#!/usr/bin/env python3
"""Analyze frames from win-capture.py watch (region = Serum OSC A: wave on top, knob row below).

Usage: analyze-wave-lag.py <frames dir> [split_y=231] [gap=0.6]
For every pause in knob movement (>= gap s) prints: last knob change, last wave change after it,
the lag between them, and the count of wave updates per second while the knob moved.
"""
import glob, sys
from PIL import Image, ImageChops

d = sys.argv[1]
split = int(sys.argv[2]) if len(sys.argv) > 2 else 231
GAP = float(sys.argv[3]) if len(sys.argv) > 3 else 0.6
ev = []
prev = None
for f in sorted(glob.glob(f"{d}/*.png")):
    t = float(f.rsplit("-", 1)[1][:-4])
    im = Image.open(f)
    w, k = im.crop((0, 0, im.width, split)), im.crop((0, split, im.width, im.height))
    if prev is not None:
        ev.append((t, ImageChops.difference(w, prev[0]).getbbox() is not None,
                   ImageChops.difference(k, prev[1]).getbbox() is not None))
    prev = (w, k)

knob = [t for t, w, k in ev if k]
wave = [t for t, w, k in ev if w]
# group knob changes into gestures separated by >= GAP s
gestures, cur = [], []
for t in knob:
    if cur and t - cur[-1] >= GAP:
        gestures.append(cur); cur = []
    cur.append(t)
if cur: gestures.append(cur)
for g in gestures:
    start, end = g[0], g[-1]
    nxt = next((h[0] for h in gestures if h[0] > end), float("inf"))
    during = [t for t in wave if start <= t <= end]
    after = [t for t in wave if end < t < nxt]
    lag = (after[-1] - end) if after else 0.0
    dur = max(end - start, 0.02)
    print(f"gesture {start:.2f}-{end:.2f} ({dur:.2f}s, {len(g)} knob frames): "
          f"wave updates during {len(during)} ({len(during)/dur:.1f}/s), "
          f"last wave update {lag*1000:.0f} ms after knob stopped")
