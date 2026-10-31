/* wine-fginfo.exe: prints the Win32 foreground/active window state of the Wine session, plus
 * visibility/iconic/rects for a window matched by class. Used to check whether a plugin window
 * that ignores repaints is considered inactive/occluded from the Win32 side.
 * Build: winegcc -O2 -o wine-fginfo.exe wine-fginfo.c
 * Run:   WINEPREFIX=... WINELOADER=... ./wine-fginfo.exe JUCE */
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
    if (strstr(cls, filter) && rc.right > 500 && rc.bottom > 500) { found = hwnd; return; }
    for (child = GetWindow(hwnd, GW_CHILD); child; child = GetWindow(child, GW_HWNDNEXT))
        visit(child);
}

int main(int argc, char **argv)
{
    HWND top, fg, desk;
    RECT wr, cr;
    char cls[128] = "";

    filter = argc > 1 ? argv[1] : "JUCE";
    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT)) visit(top);

    fg = GetForegroundWindow();
    desk = GetDesktopWindow();
    if (fg) GetClassNameA(fg, cls, sizeof(cls));
    printf("foreground %p [%s] is_desktop=%d\n", fg, fg ? cls : "-", fg == desk);
    printf("active (this thread) %p\n", GetActiveWindow());
    if (found)
    {
        GetClassNameA(found, cls, sizeof(cls));
        GetWindowRect(found, &wr);
        GetClientRect(found, &cr);
        printf("window %p [%s] visible=%d iconic=%d rect %ld,%ld-%ld,%ld client %ldx%ld style %08lx ex %08lx\n",
               found, cls, IsWindowVisible(found), IsIconic(found),
               wr.left, wr.top, wr.right, wr.bottom, cr.right, cr.bottom,
               (unsigned long)GetWindowLongA(found, GWL_STYLE),
               (unsigned long)GetWindowLongA(found, GWL_EXSTYLE));
        printf("foreground==window: %d, foreground==root: %d\n",
               fg == found, fg == GetAncestor(found, GA_ROOT));
    }
    return 0;
}
