#include <windows.h>
#include <stdio.h>
#include <string.h>
static const char *filter; static HWND found;
static void visit(HWND h){char c[128]="";HWND ch;RECT r;if(found)return;GetClassNameA(h,c,sizeof(c));GetClientRect(h,&r);
 if(strstr(c,filter)&&IsWindowVisible(h)&&r.right>500&&r.bottom>500){found=h;return;}
 for(ch=GetWindow(h,GW_CHILD);ch;ch=GetWindow(ch,GW_HWNDNEXT))visit(ch);}
int main(int argc,char**argv){HWND t;int x,y;if(argc<4)return 1;filter=argv[1];x=atoi(argv[2]);y=atoi(argv[3]);
 for(t=GetTopWindow(NULL);t;t=GetWindow(t,GW_HWNDNEXT))visit(t);if(!found){fprintf(stderr,"no window\n");return 1;}
 PostMessageA(found,WM_MOUSEMOVE,0,MAKELONG(x,y));printf("hover %p %d,%d\n",found,x,y);return 0;}
