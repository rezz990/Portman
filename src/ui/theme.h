#ifndef PORTMAN_THEME_H
#define PORTMAN_THEME_H
/* Shared native controls: retain BUTTON keyboard/accessibility/check semantics. */
#include <uxtheme.h>
#include <dwmapi.h>
static void pm_frame(HWND h) {
 BOOL dark=TRUE;
 DwmSetWindowAttribute(h,20,&dark,sizeof(dark));
}
static void pm_round(HDC dc,RECT r,COLORREF color,int radius) {
 HBRUSH b=CreateSolidBrush(color); HGDIOBJ oldb=SelectObject(dc,b),oldp=SelectObject(dc,GetStockObject(NULL_PEN));
 RoundRect(dc,r.left,r.top,r.right,r.bottom,radius,radius);
 SelectObject(dc,oldp); SelectObject(dc,oldb); DeleteObject(b);
}
static LRESULT CALLBACK pm_check(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
 (void)id; (void)data;
 if(msg==WM_PAINT) {
  PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); RECT r; GetClientRect(h,&r);
  HBRUSH b=CreateSolidBrush(RGB(23,27,32)); FillRect(dc,&r,b); DeleteObject(b);
  HFONT f=(HFONT)SendMessageW(h,WM_GETFONT,0,0); HGDIOBJ old=SelectObject(dc,f);
  TEXTMETRICW tm; GetTextMetricsW(dc,&tm); int side=tm.tmHeight,top=(r.bottom-side)/2;
  RECT box={1,top,side+1,top+side}; int checked=SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;
  pm_round(dc,box,checked?RGB(90,218,170):RGB(104,120,132),4);
  if(!checked) { InflateRect(&box,-1,-1); pm_round(dc,box,RGB(31,37,44),3); }
  else { SetTextColor(dc,RGB(23,27,32)); SetBkMode(dc,TRANSPARENT); DrawTextW(dc,L"\u2713",1,&box,DT_CENTER|DT_VCENTER|DT_SINGLELINE); }
  wchar_t text[200]; GetWindowTextW(h,text,200); r.left=side+12;
  SetTextColor(dc,IsWindowEnabled(h)?RGB(224,231,238):RGB(158,172,187)); SetBkMode(dc,TRANSPARENT);
  DrawTextW(dc,text,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
  if(GetFocus()==h) { InflateRect(&r,-2,-2); DrawFocusRect(dc,&r); }
  SelectObject(dc,old); EndPaint(h,&ps); return 0;
 }
 if(msg==WM_ERASEBKGND) return 1;
 LRESULT result=DefSubclassProc(h,msg,wp,lp);
 if(msg==BM_SETCHECK||msg==WM_SETFOCUS||msg==WM_KILLFOCUS||msg==WM_LBUTTONUP||msg==WM_KEYUP||msg==WM_ENABLE) InvalidateRect(h,NULL,FALSE);
 if(msg==WM_NCDESTROY) RemoveWindowSubclass(h,pm_check,1);
 return result;
}
#endif
