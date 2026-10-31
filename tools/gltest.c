#include <windows.h>
#define GL_COLOR_BUFFER_BIT 0x4000
void WINAPI glClearColor(float,float,float,float); void WINAPI glClear(unsigned); void WINAPI glFinish(void);
#include <stdio.h>
static double now(void){LARGE_INTEGER f,c;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&c);return c.QuadPart*1000.0/f.QuadPart;}
int main(void){
  WNDCLASSA wc={0}; wc.style=CS_OWNDC; wc.lpfnWndProc=DefWindowProcA; wc.hInstance=GetModuleHandleA(0); wc.lpszClassName="t"; RegisterClassA(&wc);
  for(int i=0;i<5;i++){
    double t0=now();
    HWND w=CreateWindowA("t","t",WS_POPUP|WS_VISIBLE,0,0,400,300,0,0,wc.hInstance,0);
    HDC dc=GetDC(w); double t1=now();
    PIXELFORMATDESCRIPTOR pfd={sizeof(pfd),1,PFD_DRAW_TO_WINDOW|PFD_SUPPORT_OPENGL|PFD_DOUBLEBUFFER,PFD_TYPE_RGBA,32};
    int pf=ChoosePixelFormat(dc,&pfd); SetPixelFormat(dc,pf,&pfd); double t2=now();
    HGLRC rc=wglCreateContext(dc); double t3=now();
    wglMakeCurrent(dc,rc); glClearColor(0,0,1,1); glClear(GL_COLOR_BUFFER_BIT); SwapBuffers(dc); glFinish(); double t4=now();
    wglMakeCurrent(0,0); wglDeleteContext(rc); ReleaseDC(w,dc); DestroyWindow(w); double t5=now();
    printf("run %d: window %.0f  pixfmt %.0f  createctx %.0f  first-frame %.0f  teardown %.0f  total %.0f ms\n",i,t1-t0,t2-t1,t3-t2,t4-t3,t5-t4,t5-t0);
    fflush(stdout);
  }
  return 0;
}
