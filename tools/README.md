# Wine/yabridge debugging tools

Sources only, no binaries. Build: `winegcc -O2 -o <name>.exe <name>.c` (add
`-ldwmapi` etc. where needed).

## Windows and pixels

| file | what it does |
|---|---|
| `win-capture.py` | list windows by name (`list`), screenshot (`shot`), series of frames with diffs (`watch`). Works with XWayland |
| `grabwin.py` | raw screenshot of an X11 window by id |
| `wine-windows.c` | list Win32 windows with classes, styles, pid |
| `wine-grab.c` | reads a Win32 window's surface via `GetDC`+`GetDIBits`, tells "not drawn" from "drawn but not shown" |
| `wine-inval.c` | prints pending `GetUpdateRect` regions, shows whether the app invalidates |
| `wine-fginfo.c` | window foreground/visible/iconic/style |

## Poking a window

| file | what it does |
|---|---|
| `wine-redraw.c` | forced `RedrawWindow(RDW_INVALIDATE|ERASE|FRAME|ALLCHILDREN)` |
| `wine-flush.c` | `SetWindowPos`/`ShowWindow`, forces a surface flush without a repaint |
| `wine-activate.c` | `SetForegroundWindow`/`SetActiveWindow`/`WM_ACTIVATE`, tests "doesn't draw until focused" |
| `wine-click.c` | posts `WM_LBUTTONDOWN/UP` to a window (client coordinates) |
| `wine-hover.c` | a single `WM_MOUSEMOVE` |
| `wine-drag.c` | drag via `SendInput` (unreliable on KDE Wayland) |
| `xtest-drag.py` | the same via XTest |

## Benchmarks and profiling

| file | what it does |
|---|---|
| `d2dtest.c` | window + d2d1 render target + a frame of text; main benchmark for the d2d1/dwrite patches |
| `fonttest.c` | font/text creation |
| `gltest.c` | OpenGL path for comparison |
| `thread-cpu-timeline.py` | per-thread CPU of a process over time |
| `thread-state-sampler.py` | which syscall/primitive each thread sleeps in |
| `analyze-frame-lag.py`, `analyze-wave-lag.py` | frame/waveform lag from a series of screenshots |
| `callers.py`, `symbolize-profile.py` | `perf` profile analysis (`D2D1_DLL` points at an unstripped d2d1.dll) |
| `dual-capture.py` | two simultaneous screenshots (X11 + Win32) |
| `msgcount.c` | window message counter |
| `clean-font-registry.py` | removes external font entries from the prefix registry (`$WINEPREFIX`) |
