/* wine-flush.exe: triggers a window-surface flush without asking the app to repaint, to tell
 * "the Win32 surface already has the new content but was never copied to X" apart from
 * "the app never painted".
 * Build: winegcc -O2 -o wine-flush.exe wine-flush.c
 * Run:   WINEPREFIX=... WINELOADER=... ./wine-flush.exe JUCE [pos|show]
 *   pos  - SetWindowPos(SWP_SHOWWINDOW|SWP_NOMOVE|SWP_NOSIZE|SWP_NOZORDER|SWP_NOACTIVATE)
 *   show - ShowWindow(SW_SHOWNA), also a no-op state-wise */
#include <windows.h>
#include <stdio.h>
#include <string.h>

static const char *filter;
static HWND found;

static void visit(HWND hwnd)
{
    char cls[128] = "";
    HWND child;
    if (found) return;
    GetClassNameA(hwnd, cls, sizeof(cls));
    if (strstr(cls, filter) && IsWindowVisible(hwnd)) { found = hwnd; return; }
    for (child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        visit(child);
}

int main(int argc, char **argv)
{
    HWND top;
    const char *mode = argc > 2 ? argv[2] : "pos";

    filter = argc > 1 ? argv[1] : "JUCE";
    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT)) visit(top);
    if (!found) { fprintf(stderr, "no window\n"); return 1; }

    if (!strcmp(mode, "show"))
        ShowWindow(found, SW_SHOWNA);
    else
        SetWindowPos(found, 0, 0, 0, 0, 0,
                     SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE | SWP_SHOWWINDOW);
    printf("%s on %p\n", mode, found);
    return 0;
}
