/* egl-swap-test.c — how Mesa behaves with eglSwapBuffers.
 *
 * Written while tracking down the black VPS Avenger editor: the plugin draws a frame,
 * calls wglMakeCurrent(NULL, NULL) and only then SwapBuffers. Wine swaps the
 * surface through EGL, and Mesa returns EGL_BAD_SURFACE (0x300D) if the
 * surface is not current for any context, so the frame is silently
 * lost. Patch 0010 binds the thread's internal context to the drawable for
 * the duration of the swap.
 *
 * Build: gcc -O1 -o egl-swap-test egl-swap-test.c -lEGL -lGL -lX11
 * Run:   ./egl-swap-test            - the context stays current
 *         ./egl-swap-test nocontext  - the context is released before the swap
 *         ./egl-swap-test helper     - swap through another context (as patch 0010 does)
 *
 * Prints the pixel colour of the window at the end: ff0000 - the frame arrived, 000000 - it was lost.
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <GL/gl.h>

static void dump_pixel(Display *dpy, Window win, const char *tag)
{
    XImage *img = XGetImage(dpy, win, 10, 10, 1, 1, AllPlanes, ZPixmap);
    if (!img) { printf("%s: XGetImage failed\n", tag); return; }
    printf("%s: pixel 0x%06lx\n", tag, XGetPixel(img, 0, 0));
    XDestroyImage(img);
}

int main(int argc, char **argv)
{
    const char *mode = argc > 1 ? argv[1] : "current";
    Display *dpy = XOpenDisplay(NULL);
    if (!dpy) { fprintf(stderr, "no display\n"); return 1; }
    int scr = DefaultScreen(dpy);
    Window win = XCreateSimpleWindow(dpy, RootWindow(dpy, scr), 0, 0, 200, 200, 0, 0, 0);
    XMapWindow(dpy, win);
    XSync(dpy, False);

    EGLDisplay edpy = eglGetDisplay((EGLNativeDisplayType)dpy);
    EGLint major, minor;
    if (!eglInitialize(edpy, &major, &minor)) { fprintf(stderr, "eglInitialize failed\n"); return 1; }
    eglBindAPI(EGL_OPENGL_API);

    EGLint attr[] = { EGL_SURFACE_TYPE, EGL_WINDOW_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
                      EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_NONE };
    EGLConfig cfg; EGLint n = 0;
    eglChooseConfig(edpy, attr, &cfg, 1, &n);
    EGLSurface surf = eglCreateWindowSurface(edpy, cfg, (EGLNativeWindowType)win, NULL);

    EGLContext client = eglCreateContext(edpy, cfg, EGL_NO_CONTEXT, NULL);
    /* Wine creates its internal contexts without a config (EGL_NO_CONFIG_KHR):
     * such a context works with any window surface. */
    EGLContext helper = eglCreateContext(edpy, EGL_NO_CONFIG_KHR, client, NULL);
    if (helper == EGL_NO_CONTEXT) { fprintf(stderr, "configless context failed 0x%x\n", eglGetError()); return 1; }

    eglMakeCurrent(edpy, surf, surf, client);
    glClearColor(1.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
    while (eglGetError() != EGL_SUCCESS);

    if (!strcmp(mode, "nocontext"))
    {
        eglMakeCurrent(edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    }
    else if (!strcmp(mode, "helper"))
    {
        eglMakeCurrent(edpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if (!eglMakeCurrent(edpy, surf, surf, helper)) printf("makeCurrent(helper) failed 0x%x\n", eglGetError());
    }

    EGLBoolean ok = eglSwapBuffers(edpy, surf);
    printf("%s: eglSwapBuffers -> %d, error 0x%x\n", mode, ok, ok ? 0 : eglGetError());
    XSync(dpy, False);
    usleep(150000);
    dump_pixel(dpy, win, "after swap");
    return 0;
}
