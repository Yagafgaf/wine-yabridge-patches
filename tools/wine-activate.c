/* wine-activate.exe: give a plugin editor window real focus/activation, to test whether the app
 * only repaints while it believes it is foreground/active.
 * Build: winegcc -O2 -o wine-activate.exe wine-activate.c
 * Run:   WINEPREFIX=... WINELOADER=... ./wine-activate.exe JUCE
 * Prints the window handle, foreground/active handles, visibility, cloaked state before and after. */
#include <windows.h>
#include <dwmapi.h>
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

static void dump(const char *tag)
{
    DWORD cloaked = 0;
    DwmGetWindowAttribute(found, DWMWA_CLOAKED, &cloaked, sizeof(cloaked));
    printf("%s: hwnd=%p fg=%p active=%p focus=%p visible=%d iconic=%d cloaked=%lu\n",
           tag, (void *)found, (void *)GetForegroundWindow(), (void *)GetActiveWindow(),
           (void *)GetFocus(), IsWindowVisible(found), IsIconic(found), (unsigned long)cloaked);
}

int main(int argc, char **argv)
{
    HWND top;
    filter = argc > 1 ? argv[1] : "JUCE";
    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT)) visit(top);
    if (!found) { fprintf(stderr, "no window\n"); return 1; }
    dump("before");

    ShowWindow(found, SW_SHOW);
    SetWindowPos(found, HWND_TOP, 0, 0, 0, 0,
                 SWP_NOMOVE | SWP_NOSIZE | SWP_SHOWWINDOW | SWP_NOACTIVATE);
    SendMessageW(found, WM_NCACTIVATE, TRUE, 0);
    SendMessageW(found, WM_ACTIVATE, WA_ACTIVE, 0);
    SendMessageW(found, WM_ACTIVATEAPP, TRUE, 0);
    SendMessageW(found, WM_SETFOCUS, 0, 0);
    SetActiveWindow(found);
    SetFocus(found);
    SetForegroundWindow(found);
    UpdateWindow(found);

    dump("after");
    return 0;
}
