/* wine-windows.c: prints the Win32 window tree of all top-level windows in the wineserver session
 * (class, text, rect, style, owning pid/tid), to see how a plugin editor is built from HWNDs.
 * Build: winegcc -O2 -o wine-windows.exe wine-windows.c
 * Run:   WINEPREFIX=<prefix> ./wine-windows.exe [class filter] */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static void dump(HWND hwnd, int depth)
{
    char cls[128] = "", text[128] = "";
    RECT rc;
    DWORD pid, tid = GetWindowThreadProcessId(hwnd, &pid);
    HWND child;
    GetClassNameA(hwnd, cls, sizeof(cls));
    GetWindowTextA(hwnd, text, sizeof(text));
    GetWindowRect(hwnd, &rc);
    printf("%*s%p [%s] \"%s\" %ldx%ld+%ld+%ld style=%08lx ex=%08lx %s pid=%lu tid=%lu\n", depth * 2, "",
           hwnd, cls, text, rc.right - rc.left, rc.bottom - rc.top, rc.left, rc.top,
           GetWindowLongA(hwnd, GWL_STYLE), GetWindowLongA(hwnd, GWL_EXSTYLE),
           IsWindowVisible(hwnd) ? "visible" : "hidden", pid, tid);
    for (child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        dump(child, depth + 1);
}

int main(int argc, char **argv)
{
    HWND top;
    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT))
    {
        char cls[128] = "";
        GetClassNameA(top, cls, sizeof(cls));
        if (argc > 1 && !strstr(cls, argv[1])) continue;
        dump(top, 0);
    }
    return 0;
}
