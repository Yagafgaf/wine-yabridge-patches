/* msgcount.exe: count WM_MOUSEMOVE / WM_TIMER delivery timing in a Wine window.
 * Usage: msgcount.exe <x> <y> <seconds> <logpath>
 * Logs "M <t> <x> <y>" for mouse moves and "T <t>" for the 10 ms timer.
 */
#include <windows.h>
#include <windowsx.h>
#include <stdio.h>

static FILE *logf;
static LARGE_INTEGER freq, t0;
static int moves, timers;

static double now(void)
{
    LARGE_INTEGER c;
    QueryPerformanceCounter(&c);
    return (double)(c.QuadPart - t0.QuadPart) / (double)freq.QuadPart;
}

static LRESULT CALLBACK wndproc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
    switch (msg) {
    case WM_MOUSEMOVE:
        if (logf) fprintf(logf, "M %.6f %d %d\n", now(), GET_X_LPARAM(lp), GET_Y_LPARAM(lp));
        moves++;
        return 0;
    case WM_TIMER:
        if (logf) fprintf(logf, "T %.6f\n", now());
        timers++;
        return 0;
    case WM_DESTROY:
        PostQuitMessage(0);
        return 0;
    }
    return DefWindowProcW(hwnd, msg, wp, lp);
}

int main(int argc, char **argv)
{
    int x = argc > 1 ? atoi(argv[1]) : 600;
    int y = argc > 2 ? atoi(argv[2]) : 600;
    int secs = argc > 3 ? atoi(argv[3]) : 5;
    const char *path = argc > 4 ? argv[4] : "C:\\msgcount.txt";
    WNDCLASSW wc = {0};
    HWND hwnd;
    MSG msg;

    logf = fopen(path, "w");
    printf("msgcount: window at %d,%d for %ds log=%s\n", x, y, secs, path);
    fflush(stdout);
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&t0);

    wc.lpfnWndProc = wndproc;
    wc.hInstance = GetModuleHandleW(NULL);
    wc.hCursor = LoadCursorW(NULL, (LPCWSTR)IDC_ARROW);
    wc.lpszClassName = L"MsgCountWnd";
    RegisterClassW(&wc);
    hwnd = CreateWindowExW(WS_EX_TOPMOST, wc.lpszClassName, L"msgcount", WS_OVERLAPPEDWINDOW | WS_VISIBLE,
                           x, y, 400, 400, NULL, NULL, wc.hInstance, NULL);
    if (!hwnd) {
        printf("CreateWindow failed: %lu\n", GetLastError());
        return 1;
    }
    SetTimer(hwnd, 1, 10, NULL);

    while (now() < secs) {
        while (PeekMessageW(&msg, NULL, 0, 0, PM_REMOVE)) {
            TranslateMessage(&msg);
            DispatchMessageW(&msg);
        }
        MsgWaitForMultipleObjects(0, NULL, FALSE, 5, QS_ALLINPUT);
    }
    printf("moves %d timers %d\n", moves, timers);
    if (logf) {
        fprintf(logf, "# moves %d timers %d seconds %d\n", moves, timers, secs);
        fclose(logf);
    }
    DestroyWindow(hwnd);
    return 0;
}
