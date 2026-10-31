/* wine-inval.c: watches the update region (pending WM_PAINT) of windows whose class matches a
 * filter, for N seconds, printing only changes. Used to tell "plugin never invalidates" from
 * "plugin invalidates but the paint never happens / is never shown".
 * Build: winegcc -O2 -o wine-inval.exe wine-inval.c
 * Run:   WINEPREFIX=... WINELOADER=... ./wine-inval.exe JUCE 3.0 [poll_ms] */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *filter;
static HWND targets[32];
static int ntargets;

static void visit(HWND hwnd)
{
    char cls[128] = "";
    HWND child;
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strstr(cls, filter) && ntargets < 32) targets[ntargets++] = hwnd;
    for (child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        visit(child);
}

int main(int argc, char **argv)
{
    double secs = argc > 2 ? atof(argv[2]) : 2.0;
    int poll = argc > 3 ? atoi(argv[3]) : 2;
    LARGE_INTEGER c, f;
    HWND top;
    RECT last[32];
    int i;

    filter = argc > 1 ? argv[1] : "JUCE";
    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT)) visit(top);

    QueryPerformanceFrequency(&f);
    for (i = 0; i < ntargets; i++) last[i].left = -1;
    printf("%d target(s), %.1f s\n", ntargets, secs);

    QueryPerformanceCounter(&c);
    double t0 = (double)c.QuadPart / f.QuadPart, t;
    do {
        for (i = 0; i < ntargets; i++)
        {
            RECT r = {0};
            BOOL has = GetUpdateRect(targets[i], &r, FALSE);
            if (!has) r = (RECT){0};
            if (r.left != last[i].left || r.top != last[i].top ||
                r.right != last[i].right || r.bottom != last[i].bottom)
            {
                QueryPerformanceCounter(&c);
                t = (double)c.QuadPart / f.QuadPart - t0;
                printf("%.3f %p update=%d %ld,%ld,%ld,%ld vis=%d\n", t, targets[i], has,
                       r.left, r.top, r.right, r.bottom, IsWindowVisible(targets[i]));
                fflush(stdout);
                last[i] = r;
            }
        }
        Sleep(poll);
        QueryPerformanceCounter(&c);
        t = (double)c.QuadPart / f.QuadPart - t0;
    } while (t < secs);
    return 0;
}
