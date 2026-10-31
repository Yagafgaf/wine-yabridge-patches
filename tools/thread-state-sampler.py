#!/usr/bin/env python3
"""Sample state (R/S/D) and wchan (kernel wait function) of every thread of a process at a short interval.
Prints state changes per thread with CLOCK_MONOTONIC times.
Usage: thread-state-sampler.py <pid> <secs> [interval=0.002]"""
import os, sys, time
pid = sys.argv[1]; secs = float(sys.argv[2]); step = float(sys.argv[3]) if len(sys.argv) > 3 else 0.002
names, last = {}, {}
end = time.monotonic() + secs
while time.monotonic() < end:
    t = time.monotonic()
    for tid in os.listdir(f"/proc/{pid}/task"):
        try:
            st = open(f"/proc/{pid}/task/{tid}/stat").read()
            wchan = open(f"/proc/{pid}/task/{tid}/wchan").read().strip()
        except OSError:
            continue
        if tid not in names:
            names[tid] = st[st.index("(") + 1:st.rindex(")")]
        state = st[st.rindex(")") + 2]
        key = (state, wchan)
        if last.get(tid) != key:
            print(f"{t:.4f} {tid} {names[tid]:<16} {key[0]} {key[1]}", flush=True)
            last[tid] = key
    time.sleep(step)
