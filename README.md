# wine-mao patches

Patches on top of Wine 11.20 for running Windows audio plugins through
[yabridge](https://github.com/robbert-vdh/yabridge). They fix plugin crashes and
editors that don't redraw, and make plugin editors open faster.

## Patches (`patches/`)

| # | change | why |
|---|---|---|
| 0001 | d2d1: limit recursion depth in CDT triangulation | Serum 2 crashed the host when turning WT Pos (unbounded recursion, [Wine bug 60115](https://bugs.winehq.org/show_bug.cgi?id=60115)) |
| 0002 | d2d1: compile shape shaders once per process, in the background on factory creation | d2d1 recompiled 8 HLSL shaders for every window, ~25–30 ms per editor open |
| 0003 | dwrite: cache FreeType size objects (`FT_Size`) per face | a new size object per glyph call re-ran font hinting; text rendering was ~4x slower |
| 0004 | windowscodecs: vectorizable alpha premultiply loop for PBGRA/PRGBA | byte-by-byte PNG conversion; the branch- and division-free loop is 3–4x faster with identical output (all 65 536 pairs checked) |
| 0005 | debug: optional event log (`WINE_MAO_STATS`) | not a fix. When the variable is set, vkd3d (bytecode md5), d2d1 (contexts), dwrite (font objects) and WIC (conversions) log events with call stacks. No effect otherwise |
| 0006 | d2d1: share immutable D3D objects (shaders, input layouts, buffers) among a device's contexts | Serum creates ~76 bitmap render targets per editor, each recreated 11 shader objects: ~1370 DXBC parses and SPIR-V translations per open |
| 0007 | dwrite: share FreeType faces among font objects with the same data, keep up to 16 idle ones cached | Serum created a font from the same 3 files ~280 times per open; for unhinted fonts FreeType's autohinter re-parsed the font via harfbuzz each time. Key: size + sampled hash, then full memcmp; FreeType calls under one mutex |
| 0008 | win32u: copy offscreen client surfaces (Vulkan/GL in a child window) to the toplevel again 16, 50 and 150 ms after present | the copy ran right after `vkQueuePresentKHR`, but Mesa's X11 WSI sends the frame later from its own thread, so the previous frame was copied. Without another frame the window kept showing stale content (Serum 2's wavetable view lagged 1.6 s+). Now it catches up in ~150–250 ms. yabridge plugin windows are always child windows, so always offscreen |
| 0009 | dxgi: `IDXGIOutput::WaitForVBlank` waits for the next tick of a synthetic 60 Hz timeline and returns `S_OK`; `GetFrameStatistics` reports counts and `SyncQPCTime` from QPC instead of `E_NOTIMPL` | Nexus 4 (reFX) drives its UI loop from these two calls. With the stubs the loop spun ~1000/s without producing a frame: presets and knobs changed the sound, but the editor never repainted. Cost: ~6.7% of one core while the Nexus editor is open (2.5% before, spinning without drawing) |
| 0010 | win32u: on `wglSwapBuffers` without a current context, bind the thread's internal context to the drawable for the duration of the swap, then return the null surface | VPS Avenger draws a frame, calls `wglMakeCurrent(NULL, NULL)` and only then `SwapBuffers`. That is legal on Windows, but an EGL driver needs a current context: Mesa returns `EGL_BAD_SURFACE` (0x300D), the frame is silently lost and the editor stays black (clicks still work). Wine's internal contexts are created configless (`EGL_NO_CONFIG_KHR`), so they fit any window surface. Checked with a separate EGL test (`tools/egl-swap-test.c`): swap without a current context gives `EGL_BAD_SURFACE` and a black pixel, with a bound context the frame reaches the screen. With the patch the Avenger editor draws, and an idle host takes ~8% of a core instead of ~190% (the loop used to run without vsync because the swap never blocked) |
| 0011 | ole32: `RevokeDragDrop` releases the drop target even when the window is already destroyed (own per-HWND registration list under a critical section; the return code stays `DRAGDROP_E_INVALIDHWND`, as on Windows). Entries for windows whose revoke never came are cleaned up on the next registration, but only ole32's own mapping and marshal data are freed there, never the application's object: the cleanup can run on another thread and after the plugin DLL is unloaded | u-he VST3 plugins (Diva, Hive, Zebra2/ZebraHZ, ACE, Bazille) destroy their window first and call `RevokeDragDrop` afterwards when the editor closes. Wine returned on `err:RevokeDragDrop invalid hwnd` before `CloseHandle(map)`, so every editor open left a 4 KB `memfd:wine-mapping` section (holding a table-strong marshal reference to the ole32 wrapper) and an `AddRef` on the plugin's `IDropTarget` that keeps the object alive. Measured: one leaked section per open (303 piled up in a ZebraHZ host); with the patch, 40 opens of ZebraHZ VST3 and 21 opens of Diva VST3 leave 0 sections, and `RevokeDragDrop` frees the stub manager (`stub_manager_delete` is visible in `WINEDEBUG=+ole`). The rest of the plugin's own memory growth (~1.4-2.3 MB per editor open) is a separate leak inside u-he and is not fixed by this |

The patches are generic: they help any plugin that draws through
Direct2D/DirectWrite/WIC. Plugins using OpenGL (FabFilter) or their own renderer
(MeldaProduction) barely change, there the time is spent in the plugin itself.

`debug/` holds a temporary logging patch used while tracking down 0008, kept for
reference only.

Timings can be repeated with `tools/bench.py` (see below).

## Measurements

`tools/d2dtest.c` (window + d2d1 render target + one frame), mean over repeated opens:

| | stock Wine 11.20 | patched |
|---|---|---|
| render target creation | ~30 ms | ~3 ms |
| frame with 200 lines of text | ~206 ms | ~48 ms |
| whole open | ~256 ms | ~74 ms |

Other d2dtest modes: 50 intermediate render targets 10 → 0.5 ms, window teardown
39 → 12 ms (0006); 100 new unhinted fonts from memory 186 → 65 ms (0007).

Median plugin editor open time in REAPER, ms:

| plugin | stock | 0001–0004 | 0001–0007 |
|---|---|---|---|
| Serum 2 | 365 | 300 | **224** |
| FabFilter Pro-C 3 | 320 | 283 | 288 |
| FabFilter Pro-Q 4 | 329 | 323 | 325 |
| MeldaProduction MEqualizer | 61 | 58 | 36–59 (noisy) |

## Repeatable timings

`tools/bench.py` builds `d2dtest` and `fonttest` with the `winegcc` of the Wine
build under test, runs them in a separate prefix and writes JSON. `compare`
prints before/after and exits with 1 if a metric grew by more than the
threshold (8 % and 1 ms by default). Scenarios: `open` (window + hwnd render
target + frame), `text200` (200 new text formats and lines, as in an editor
open), `faces100` (100 new fonts from memory) and `fonttest`. The metric is
the median; the first run inside a process and one process-level warm-up are
dropped.

```sh
tools/bench.py run --wine /path/to/wine-mao/bin/wine --label wine-mao --out wine-mao.json
tools/bench.py compare --baseline wine-mao.json --wine /usr/bin/wine --label stock
```

Note: `winegcc` without mingw builds a winelib ELF (`d2dtest.exe.so`) plus a
wrapper that starts `wine` from `PATH`. The harness runs the `.exe.so` with
the target `wine` directly; running the wrapper would measure the system Wine
for both builds.

## Building

```sh
curl -O https://dl.winehq.org/wine/source/11.x/wine-11.20.tar.xz
tar xf wine-11.20.tar.xz
cd wine-11.20 && git init && git add -A && git commit -qm wine-11.20
git am /path/to/wine-mao-patches/patches/*.patch
mkdir ../build && cd ../build
../wine-11.20/configure --prefix=/opt/wine-mao --disable-tests \
    --enable-archs=x86_64,i386 --with-mingw=clang
make -j"$(nproc)" && make install
```

Point yabridge's Wine host at this build, e.g. by setting `WINELOADER` to its
`bin/wine` before starting the DAW. Use the same Wine version as the rest of the
prefix, otherwise the prefix gets updated back and forth.

Without mingw-gcc, the PE side builds with clang + lld (`--with-mingw=clang`).
`CROSSLDFLAGS=-Wl,-O1` doesn't work with lld-link.

## `WINE_MAO_STATS` (patch 0005)

Set `WINE_MAO_STATS` to a Windows path of a directory (e.g. `Z:\tmp\stats`). Each
process writes `<module>-<windows pid>.log` with lines
`<tick> <tid> <event> <value> <return addresses...>`. Events: `md5` (DXBC size),
`d2d_ctx`, `d2d_device`, `ft_face` (font size), `premultiply` (pixels). Count with
`awk '{print $3}' file | sort | uniq -c`. Addresses are raw, map them with
`/proc/<pid>/maps`. Stacks come from `RtlCaptureStackBackTrace` (Windows unwind
tables), so they are correct for PE code, unlike `perf`.

## Tools

Debugging and benchmark helpers, see [`tools/README.md`](tools/README.md).

## Related

yabridge-side fixes: [robbert-vdh/yabridge pull requests](https://github.com/robbert-vdh/yabridge/pulls?q=author%3AYagafgaf).
