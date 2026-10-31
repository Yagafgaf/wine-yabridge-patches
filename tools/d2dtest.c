/* d2dtest: time what a d2d1-based plugin editor does when its window opens.
 * Per run: create window, CreateHwndRenderTarget, first frame (fill, geometry, text), teardown.
 * Build: winegcc -O2 -o d2dtest.exe d2dtest.c -ld2d1 -ldwrite -luuid
 * Run with a test d2d1: put d2d1.dll (builtin marker zeroed) next to the exe and
 * WINEDLLOVERRIDES=d2d1=n. Args: [runs] [delay ms before first run] [strings per frame] [compatible render targets per frame] [new font faces per frame] [font file, Windows path] */
#define COBJMACROS
#include <windows.h>
#include <initguid.h>
#include <d2d1.h>
#include <dwrite_3.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>

static double now(void){LARGE_INTEGER f,c;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&c);return c.QuadPart*1000.0/f.QuadPart;}

int main(int argc, char **argv)
{
    int runs = argc > 1 ? atoi(argv[1]) : 5, delay = argc > 2 ? atoi(argv[2]) : 0;
    int text_count = argc > 3 ? atoi(argv[3]) : 1;
    int compat_count = argc > 4 ? atoi(argv[4]) : 0;
    int face_count = argc > 5 ? atoi(argv[5]) : 0;
    const char *font_path = argc > 6 ? argv[6] : "Z:\\usr\\share\\fonts\\liberation\\LiberationSans-Regular.ttf";
    ID2D1Factory *factory; IDWriteFactory *dw; IDWriteTextFormat *fmt;
    double t0 = now();
    D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &IID_ID2D1Factory, NULL, (void **)&factory);
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, &IID_IDWriteFactory, (IUnknown **)&dw);
    IDWriteFactory_CreateTextFormat(dw, L"Liberation Sans", NULL, DWRITE_FONT_WEIGHT_NORMAL,
            DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 14.0f, L"en-us", &fmt);
    printf("factories %.1f ms\n", now() - t0);
    if (delay) Sleep(delay);

    WNDCLASSA wc = {0}; wc.lpfnWndProc = DefWindowProcA; wc.hInstance = GetModuleHandleA(0); wc.lpszClassName = "t";
    RegisterClassA(&wc);
    double compat_ms = 0;
    for (int i = 0; i < runs; i++)
    {
        ID2D1HwndRenderTarget *rt; ID2D1SolidColorBrush *brush; ID2D1PathGeometry *geo; ID2D1GeometrySink *sink;
        D2D1_RENDER_TARGET_PROPERTIES rp = {0};
        D2D1_HWND_RENDER_TARGET_PROPERTIES hp = {0};
        D2D1_COLOR_F c = {0.2f, 0.4f, 0.8f, 1.0f};
        D2D1_RECT_F r = {10, 10, 300, 200};
        double a = now();
        HWND w = CreateWindowA("t", "t", WS_POPUP | WS_VISIBLE, 0, 0, 800, 600, 0, 0, wc.hInstance, 0);
        double b = now();
        hp.hwnd = w; hp.pixelSize.width = 800; hp.pixelSize.height = 600;
        ID2D1Factory_CreateHwndRenderTarget(factory, &rp, &hp, &rt);
        double c1 = now();
        ID2D1RenderTarget_CreateSolidColorBrush((ID2D1RenderTarget *)rt, &c, NULL, &brush);
        ID2D1Factory_CreatePathGeometry(factory, &geo);
        ID2D1PathGeometry_Open(geo, &sink);
        D2D1_POINT_2F p0 = {50, 50}, p1 = {400, 80}, p2 = {200, 400};
        ID2D1GeometrySink_BeginFigure(sink, p0, D2D1_FIGURE_BEGIN_FILLED);
        ID2D1GeometrySink_AddLine(sink, p1); ID2D1GeometrySink_AddLine(sink, p2);
        ID2D1GeometrySink_EndFigure(sink, D2D1_FIGURE_END_CLOSED);
        ID2D1GeometrySink_Close(sink);
        ID2D1RenderTarget_BeginDraw((ID2D1RenderTarget *)rt);
        ID2D1RenderTarget_Clear((ID2D1RenderTarget *)rt, &c);
        ID2D1RenderTarget_FillRectangle((ID2D1RenderTarget *)rt, &r, (ID2D1Brush *)brush);
        ID2D1RenderTarget_FillGeometry((ID2D1RenderTarget *)rt, (ID2D1Geometry *)geo, (ID2D1Brush *)brush, NULL);
        for (int k = 0; k < text_count; k++)
        {
            /* a fresh text format per string, like editors that build their UI on open */
            IDWriteTextFormat *f;
            WCHAR str[64];
            int n = swprintf(str, 64, L"Param %d: %d.%03d dB  ABCDEFGHIJKLMNOPQRSTUVWXYZ", k, k * 7 % 100, k * 37 % 1000);
            IDWriteFactory_CreateTextFormat(dw, L"Liberation Sans", NULL, DWRITE_FONT_WEIGHT_NORMAL,
                    DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL, 9.0f + (k % 12), L"en-us", &f);
            ID2D1RenderTarget_DrawText((ID2D1RenderTarget *)rt, str, n, f, &r, (ID2D1Brush *)brush,
                    D2D1_DRAW_TEXT_OPTIONS_NONE, DWRITE_MEASURING_MODE_NATURAL);
            IDWriteTextFormat_Release(f);
        }
        for (int k = 0; k < face_count; k++)
        {
            /* a fresh font face each time, as editors with embedded fonts do */
            IDWriteFontFile *file; IDWriteFontFace *face;
            static IDWriteInMemoryFontFileLoader *memloader; static void *fontdata; static DWORD fontsize;
            UINT32 cps[20]; UINT16 glyphs[20]; DWRITE_GLYPH_RUN run = {0};
            D2D1_POINT_2F origin = {20.0f, 40.0f + k % 20 * 10};
            for (int g = 0; g < 20; g++) cps[g] = 'A' + (g + k) % 26;
            if (!memloader)
            {
                /* fonts embedded in the plugin binary are loaded from memory */
                HANDLE h = CreateFileA(font_path, GENERIC_READ, FILE_SHARE_READ, NULL, OPEN_EXISTING, 0, NULL);
                fontsize = GetFileSize(h, NULL); fontdata = malloc(fontsize); ReadFile(h, fontdata, fontsize, &fontsize, NULL); CloseHandle(h);
                IDWriteFactory5_CreateInMemoryFontFileLoader((IDWriteFactory5 *)dw, &memloader);
                IDWriteFactory_RegisterFontFileLoader(dw, (IDWriteFontFileLoader *)memloader);
            }
            if (FAILED(IDWriteInMemoryFontFileLoader_CreateInMemoryFontFileReference(memloader, (IDWriteFactory *)dw, fontdata, fontsize, NULL, &file)))
                break;
            IDWriteFactory_CreateFontFace(dw, DWRITE_FONT_FACE_TYPE_TRUETYPE, 1, &file, 0, DWRITE_FONT_SIMULATIONS_NONE, &face);
            IDWriteFontFace_GetGlyphIndices(face, cps, 20, glyphs);
            run.fontFace = face; run.fontEmSize = 12.0f + k % 6; run.glyphCount = 20; run.glyphIndices = glyphs;
            ID2D1RenderTarget_DrawGlyphRun((ID2D1RenderTarget *)rt, origin, &run, (ID2D1Brush *)brush, DWRITE_MEASURING_MODE_NATURAL);
            IDWriteFontFace_Release(face); IDWriteFontFile_Release(file);
        }
        double cc0 = now();
        for (int k = 0; k < compat_count; k++)
        {
            /* offscreen layers, as editors cache knobs/panels in bitmaps */
            ID2D1BitmapRenderTarget *brt;
            D2D1_SIZE_F sz = {64.0f, 64.0f};
            ID2D1RenderTarget_CreateCompatibleRenderTarget((ID2D1RenderTarget *)rt, &sz, NULL, NULL,
                    D2D1_COMPATIBLE_RENDER_TARGET_OPTIONS_NONE, &brt);
            ID2D1BitmapRenderTarget_BeginDraw(brt);
            ID2D1BitmapRenderTarget_Clear(brt, &c);
            ID2D1BitmapRenderTarget_EndDraw(brt, NULL, NULL);
            ID2D1BitmapRenderTarget_Release(brt);
        }
        compat_ms = now() - cc0;
        ID2D1RenderTarget_EndDraw((ID2D1RenderTarget *)rt, NULL, NULL);
        double d = now();
        ID2D1GeometrySink_Release(sink); ID2D1PathGeometry_Release(geo);
        ID2D1SolidColorBrush_Release(brush); ID2D1HwndRenderTarget_Release(rt);
        DestroyWindow(w);
        double e = now();
        printf("run %d: window %.1f  rendertarget %.1f  first-frame %.1f  teardown %.1f  total %.1f ms  (compatible RTs %.1f)\n",
                i, b - a, c1 - b, d - c1, e - d, e - a, compat_ms);
        fflush(stdout);
    }
    return 0;
}
