#!/usr/bin/env python3
"""CPU timeline of a process's threads: samples /proc/<pid>/task/*/stat every STEP seconds.

Usage: thread-cpu-timeline.py <pid> [seconds=10] [step=0.1]
Prints one row per step: time since start and CPU% of each busy thread (by comm),
so you can see how long a GUI thread keeps working after the input stops.
"""
import os, sys, time

pid = int(sys.argv[1])
secs = float(sys.argv[2]) if len(sys.argv) > 2 else 10
step = float(sys.argv[3]) if len(sys.argv) > 3 else 0.1
hz = os.sysconf("SC_CLK_TCK")


def snap():
    d = {}
    for t in os.listdir(f"/proc/{pid}/task"):
        try:
            s = open(f"/proc/{pid}/task/{t}/stat").read()
        except OSError:
            continue
        comm = s[s.index("(") + 1:s.rindex(")")]
        f = s[s.rindex(")") + 2:].split()
        d[t] = (comm, int(f[11]) + int(f[12]))
    return d


t0 = time.monotonic()
prev = snap()
names = {}
rows = []
while time.monotonic() - t0 < secs:
    time.sleep(step)
    cur = snap()
    row = {}
    for t, (comm, ticks) in cur.items():
        dt = ticks - prev.get(t, (comm, ticks))[1]
        if dt:
            key = "GUI" if t == str(pid) else f"{comm}:{t}"
            row[key] = 100.0 * dt / hz / step
    rows.append((time.monotonic() - t0, row))
    prev = cur

cols = sorted({k for _, r in rows for k in r}, key=lambda k: (k != "GUI", k))
print("   t  " + " ".join(f"{c[:18]:>18}" for c in cols))
for t, r in rows:
    print(f"{t:5.1f} " + " ".join(f"{r.get(c, 0):18.0f}" for c in cols))
