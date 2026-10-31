#define COBJMACROS
#include <windows.h>
#include <dwrite.h>
#include <stdio.h>
static const GUID MY_IID_DWF={0xb859ee5a,0xd838,0x4b5b,{0xa2,0xe8,0x1a,0xdc,0x7d,0x93,0xdb,0x48}};
static double now(void){LARGE_INTEGER f,c;QueryPerformanceFrequency(&f);QueryPerformanceCounter(&c);return c.QuadPart*1000.0/f.QuadPart;}
static int n;
static int CALLBACK cb(const LOGFONTW*a,const TEXTMETRICW*b,DWORD c,LPARAM d){n++;return 1;}
int main(void){
  HDC dc=GetDC(0);
  for(int i=0;i<3;i++){
    LOGFONTW lf={0}; lf.lfCharSet=DEFAULT_CHARSET; n=0;
    double t0=now(); EnumFontFamiliesExW(dc,&lf,cb,0,0); double t1=now();
    HFONT f=CreateFontW(-14,0,0,0,400,0,0,0,DEFAULT_CHARSET,0,0,0,0,L"Arial"); HGDIOBJ o=SelectObject(dc,f); TEXTMETRICW tm; GetTextMetricsW(dc,&tm); SelectObject(dc,o); DeleteObject(f); double t2=now();
    IDWriteFactory*fac; IDWriteFontCollection*col; UINT32 cnt=0;
    DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,&MY_IID_DWF,(IUnknown**)&fac); double t3=now();
    IDWriteFactory_GetSystemFontCollection(fac,&col,FALSE); cnt=IDWriteFontCollection_GetFontFamilyCount(col); double t4=now();
    IDWriteFontCollection_Release(col); IDWriteFactory_Release(fac);
    printf("run %d: EnumFontFamiliesEx %.0f ms (%d)  CreateFont+metrics %.1f ms  DWriteFactory %.0f ms  SystemFontCollection %.0f ms (%u families)\n",i,t1-t0,n,t2-t1,t3-t2,t4-t3,cnt);
  }
  return 0;
}
