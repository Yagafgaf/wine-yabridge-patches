/* wine-redraw.c: forces a full repaint of every window of a given class (and its children)
 * in the wineserver session, e.g. to check whether a plugin editor already has new content
 * that just was not drawn/presented.
 * Build: winegcc -O2 -o wine-redraw.exe wine-redraw.c
 * Run:   WINEPREFIX=<prefix> ./wine-redraw.exe VSTGUI */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *filter;
static int count;

static void visit(HWND hwnd)
{
    char cls[128] = "";
    HWND child;
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strstr(cls, filter))
    {
        RedrawWindow(hwnd, NULL, NULL, RDW_INVALIDATE | RDW_ERASE | RDW_ALLCHILDREN | RDW_FRAME);
        printf("redraw %p [%s]\n", hwnd, cls);
        count++;
    }
    for (child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        visit(child);
}

int main(int argc, char **argv)
{
    HWND top;
    LARGE_INTEGER c, f;
    filter = argc > 1 ? argv[1] : "VSTGUI";
    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT))
        visit(top);
    QueryPerformanceCounter(&c);
    QueryPerformanceFrequency(&f);
    printf("%.3f done, %d windows\n", (double)c.QuadPart / f.QuadPart, count);
    return 0;
}
