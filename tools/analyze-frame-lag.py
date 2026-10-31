#!/usr/bin/env python3
"""Per-transition lag of the Serum OSC A wave display (frames from win-capture.py, OSC A region).

Usage: analyze-frame-lag.py <frames dir>
The frame-number field under the display changes the moment WT Pos crosses a wavetable frame;
for each such change prints when the wave picture itself changed next, and the summary.
"""
import glob, sys
from PIL import Image, ImageChops

NUM = (0, 226, 80, 252)    # frame number field, in the capture region
WAVE = (0, 0, 400, 224)    # wave display
ev = []
prev = None
for f in sorted(glob.glob(f"{sys.argv[1]}/*.png")):
    t = float(f.rsplit("-", 1)[1][:-4])
    im = Image.open(f)
    n, w = im.crop(NUM), im.crop(WAVE)
    if prev is not None:
        ev.append((t, ImageChops.difference(n, prev[0]).getbbox() is not None,
                   ImageChops.difference(w, prev[1]).getbbox() is not None))
    prev = (n, w)
nums = [t for t, n, w in ev if n]
waves = [t for t, n, w in ev if w]
lags, missed = [], 0
for i, t in enumerate(nums):
    nxt = nums[i + 1] if i + 1 < len(nums) else float("inf")
    w = next((x for x in waves if x >= t), None)
    if w is None or w >= nxt:
        missed += 1          # the wave never showed this frame before the number moved on
    else:
        lags.append(w - t)
lags.sort()
print(f"frame-number changes {len(nums)}, wave changes {len(waves)}, "
      f"frames shown {len(lags)}, frames skipped {missed}")
if lags:
    print(f"lag after number change: median {lags[len(lags)//2]*1000:.0f} ms, "
          f"max {lags[-1]*1000:.0f} ms")
