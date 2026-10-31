/* wine-grab.exe: reads the *window surface* (what the Win32 side believes the window contains) of
 * the first window whose class matches a filter, and saves it as a BMP. Compare with an X11
 * screenshot of the same window: if the surface has the new state but the screen shows the old one,
 * the content is painted but never presented/copied; if both are stale, the plugin never painted.
 * Build: winegcc -O2 -o wine-grab.exe wine-grab.c -lgdi32
 * Run:   WINEPREFIX=... WINELOADER=... ./wine-grab.exe JUCE out.bmp */
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
    HDC hdc, mem;
    HBITMAP bmp;
    BITMAPFILEHEADER fh = {0};
    BITMAPINFOHEADER ih = {0};
    RECT rc;
    int w, h, stride, size;
    void *bits;
    FILE *f;

    filter = argc > 1 ? argv[1] : "JUCE";
    for (top = GetTopWindow(NULL); top; top = GetWindow(top, GW_HWNDNEXT)) visit(top);
    if (!found) { fprintf(stderr, "no window\n"); return 1; }

    GetClientRect(found, &rc);
    w = rc.right; h = rc.bottom; stride = (w * 3 + 3) & ~3; size = stride * h;
    hdc = GetDC(found);
    mem = CreateCompatibleDC(hdc);
    bmp = CreateCompatibleBitmap(hdc, w, h);
    SelectObject(mem, bmp);
    BitBlt(mem, 0, 0, w, h, hdc, 0, 0, SRCCOPY);

    ih.biSize = sizeof(ih); ih.biWidth = w; ih.biHeight = h; ih.biPlanes = 1;
    ih.biBitCount = 24; ih.biCompression = BI_RGB; ih.biSizeImage = size;
    bits = malloc(size);
    GetDIBits(mem, bmp, 0, h, bits, (BITMAPINFO *)&ih, DIB_RGB_COLORS);

    fh.bfType = 0x4D42;
    fh.bfOffBits = sizeof(fh) + sizeof(ih);
    fh.bfSize = fh.bfOffBits + size;
    f = fopen(argv[2], "wb");
    fwrite(&fh, sizeof(fh), 1, f);
    fwrite(&ih, sizeof(ih), 1, f);
    fwrite(bits, size, 1, f);
    fclose(f);
    printf("grabbed %p %dx%d -> %s\n", found, w, h, argv[2]);
    free(bits);
    DeleteObject(bmp);
    DeleteDC(mem);
    ReleaseDC(found, hdc);
    return 0;
}
