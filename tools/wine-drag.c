/* wine-drag.c: drags the mouse inside Wine with SendInput (works where XTest does not, e.g. Wayland),
 * to test plugin GUIs running in the same Wine prefix/wineserver.
 * Build: winegcc -O2 -o wine-drag.exe wine-drag.c
 * Run:   WINEPREFIX=<prefix> ./wine-drag.exe x y dy secs [steps] [repeats] [pause]
 *   x, y: screen coordinates of the press; dy: total vertical move (negative = up);
 *   each of `repeats` drags presses at (x, y) and moves dy, alternating up and down; `pause` seconds between.
 * Prints CLOCK_MONOTONIC-like times (QueryPerformanceCounter, seconds) of press/moves/release. */
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>

static double now(void)
{
    LARGE_INTEGER c, f;
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    return (double)c.QuadPart / f.QuadPart;
}

static void send_mouse(DWORD flags, int x, int y)
{
    INPUT in = {0};
    int w = GetSystemMetrics(SM_CXVIRTUALSCREEN), h = GetSystemMetrics(SM_CYVIRTUALSCREEN);
    int ox = GetSystemMetrics(SM_XVIRTUALSCREEN), oy = GetSystemMetrics(SM_YVIRTUALSCREEN);
    in.type = INPUT_MOUSE;
    in.mi.dx = (LONG)(((x - ox) * 65535.0) / (w - 1));
    in.mi.dy = (LONG)(((y - oy) * 65535.0) / (h - 1));
    in.mi.dwFlags = flags | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_VIRTUALDESK;
    SendInput(1, &in, sizeof(in));
}

int main(int argc, char **argv)
{
    int x, y, dy, steps, repeats, i, r;
    double secs, pause;
    if (argc < 5)
    {
        fprintf(stderr, "usage: wine-drag.exe x y dy secs [steps=60] [repeats=1] [pause=1]\n");
        return 1;
    }
    x = atoi(argv[1]); y = atoi(argv[2]); dy = atoi(argv[3]); secs = atof(argv[4]);
    steps = argc > 5 ? atoi(argv[5]) : 60;
    repeats = argc > 6 ? atoi(argv[6]) : 1;
    pause = argc > 7 ? atof(argv[7]) : 1.0;
    timeBeginPeriod(1);
    for (r = 0; r < repeats; r++)
    {
        int dir = (r % 2) ? -1 : 1, base = y;
        send_mouse(MOUSEEVENTF_MOVE, x, base);
        Sleep(100);
        printf("%.3f press %d,%d\n", now(), x, base); fflush(stdout);
        send_mouse(MOUSEEVENTF_LEFTDOWN, x, base);
        Sleep(30);
        for (i = 1; i <= steps; i++)
        {
            send_mouse(MOUSEEVENTF_MOVE, x, base + dir * (dy * i / steps));
            Sleep((DWORD)(secs * 1000 / steps));
        }
        printf("%.3f release\n", now()); fflush(stdout);
        send_mouse(MOUSEEVENTF_LEFTUP, x, base + dir * dy);
        if (r + 1 < repeats) Sleep((DWORD)(pause * 1000));
    }
    return 0;
}
