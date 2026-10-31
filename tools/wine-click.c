/* wine-click.exe: posts a mouse click (or a small drag) directly to the client area of the first
 * visible window whose class matches a filter, bypassing cursor position / stacking / focus
 * problems of SendInput. Client coordinates are relative to the window's client area.
 * Build: winegcc -O2 -o wine-click.exe wine-click.c
 * Run:   WINEPREFIX=... WINELOADER=... ./wine-click.exe JUCE x y [dy] [steps]
 *   dy: vertical drag distance (negative = up); 0 or omitted = plain click. */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *filter;
static HWND found;

static void visit(HWND hwnd)
{
    char cls[128] = "";
    HWND child;
    RECT rc;
    if (found) return;
    GetClassNameA(hwnd, cls, sizeof(cls));
    GetClientRect(hwnd, &rc);
    if (strstr(cls, filter) && IsWindowVisible(hwnd) && rc.right > 500 && rc.bottom > 500)
    { found = hwnd; return; }
    for (child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        visit(child);
}

static void post(UINT msg, WPARAM wp, int x, int y)
{
    PostMessageA(found, msg, wp, MAKELONG(x, y));
}

int main(int argc, char **argv)
{
    HWND top;
    int x, y, dy, steps, i;
    RECT rc;

    if (argc < 4) { fprintf(stderr, "usage: wine-click.exe <class> x y [dy] [steps]\n"); return 1; }
    filter = argv[1];
    x = atoi(argv[2]); y = atoi(argv[3]);
    dy = argc > 4 ? atoi(argv[4]) : 0;
    steps = argc > 5 ? atoi(argv[5]) : 12;

    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT)) visit(top);
    if (!found) { fprintf(stderr, "no window\n"); return 1; }
    GetClientRect(found, &rc);

    post(WM_MOUSEMOVE, 0, x, y);
    Sleep(20);
    post(WM_LBUTTONDOWN, MK_LBUTTON, x, y);
    Sleep(50);
    if (dy)
    {
        for (i = 1; i <= steps; i++)
        {
            post(WM_MOUSEMOVE, MK_LBUTTON, x, y + dy * i / steps);
            Sleep(15);
        }
    }
    Sleep(30);
    post(WM_LBUTTONUP, 0, x, y + dy);
    printf("clicked %p client %d,%d dy %d (client %ldx%ld)\n", found, x, y, dy, rc.right, rc.bottom);
    return 0;
}
