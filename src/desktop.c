#define UNICODE
#define _UNICODE
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <stdio.h>
#include <stdlib.h>
#include <wchar.h>
#include "desktop.h"

#define APPCLASS L"PortmanDesktop02"
#define TRAYMSG (WM_APP+1)
enum { ID_LIST=100, ID_ADD, ID_EDIT, ID_REMOVE, ID_IMPORT, ID_START, ID_STOP,
 ID_RESTART, ID_STARTALL, ID_STOPALL, ID_BROWSER, ID_FOLDER, ID_PORTS, ID_LOGS,
 ID_CLEAR, ID_TRAY, ID_LOGIN, ID_OUTPUT, ID_STATUS, ID_OPENLOG, ID_EXPORT,
 ID_NAV_SERVICES, ID_NAV_PORTS, ID_NAV_SETTINGS, ID_NAV_ABOUT };
static HWND mainwin, list, output, status, subtitle, title, logtitle, loginbox;
static HWND settings_title,settings_copy,about_title,about_copy,brand;
static HINSTANCE instance;
static HFONT font, boldfont, titlefont, monofont;
static HBRUSH bgbrush, fieldbrush;
static COLORREF bg=RGB(23,27,32), field=RGB(31,37,44), fg=RGB(224,231,238), muted=RGB(158,172,187), accent=RGB(90,218,170);
static int scale=96, selected=-1, previous_count=-1;
static int page=0;
static wchar_t data_dir[2048];
static NOTIFYICONDATAW tray;
static HANDLE single;
static int px(int v) { return MulDiv(v,scale,96); }
static void to_w(const char *s,wchar_t *w,int cap) { if(!MultiByteToWideChar(CP_UTF8,0,s,-1,w,cap)) w[0]=0; }
static int to_u(const wchar_t *w,char *s,int cap) { return WideCharToMultiByte(CP_UTF8,0,w,-1,s,cap,NULL,NULL)!=0; }
static void errorbox(HWND h,const char *s) { wchar_t w[1024]; to_w(s,w,1024); MessageBoxW(h,w,L"Portman",MB_OK|MB_ICONWARNING); }
static void backend_error(HWND h) { errorbox(h,pmd_error()); }
static void move(HWND h,int x,int y,int w,int ht) { MoveWindow(h,px(x),px(y),px(w),px(ht),TRUE); }
static HWND control(HWND parent,const wchar_t *cls,const wchar_t *text,DWORD style,int id,int x,int y,int w,int h) {
 HWND c=CreateWindowExW(0,cls,text,WS_CHILD|WS_VISIBLE|style,px(x),px(y),px(w),px(h),parent,(HMENU)(INT_PTR)id,instance,NULL);
 SendMessageW(c,WM_SETFONT,(WPARAM)font,TRUE); return c;
}
static LRESULT CALLBACK button_proc(HWND h,UINT msg,WPARAM wp,LPARAM lp,UINT_PTR id,DWORD_PTR data) {
 (void)id; (void)data;
 if(msg==WM_MOUSEMOVE && !GetPropW(h,L"PortmanHot")) { TRACKMOUSEEVENT t={sizeof(t),TME_LEAVE,h,0}; SetPropW(h,L"PortmanHot",(HANDLE)1); TrackMouseEvent(&t); InvalidateRect(h,NULL,TRUE); }
 if(msg==WM_MOUSELEAVE) { RemovePropW(h,L"PortmanHot"); InvalidateRect(h,NULL,TRUE); }
 if(msg==WM_NCDESTROY) RemoveWindowSubclass(h,button_proc,1);
 return DefSubclassProc(h,msg,wp,lp);
}
static HWND button(HWND h,const wchar_t *name,int id,int x,int y,int w) { HWND b=control(h,L"BUTTON",name,WS_TABSTOP|BS_OWNERDRAW,id,x,y,w,32); SetWindowSubclass(b,button_proc,1,0); return b; }
static HWND label(HWND h,const wchar_t *text,int x,int y,int w,int height) { return control(h,L"STATIC",text,0,0,x,y,w,height); }
static HWND edit(HWND h,int id,int x,int y,int w,int lim) {
 HWND e=control(h,L"EDIT",L"",WS_BORDER|WS_TABSTOP|ES_AUTOHSCROLL,id,x,y,w,29);
 SendMessageW(e,EM_SETLIMITTEXT,lim,0); return e;
}
static LRESULT colors(UINT msg,WPARAM wp) {
 HDC dc=(HDC)wp; SetTextColor(dc,fg); SetBkColor(dc,msg==WM_CTLCOLOREDIT?field:bg);
 return (LRESULT)(msg==WM_CTLCOLOREDIT?fieldbrush:bgbrush);
}
static void drawbutton(DRAWITEMSTRUCT *d) {
 wchar_t text[100]; GetWindowTextW(d->hwndItem,text,100);
 int nav=d->CtlID>=ID_NAV_SERVICES && d->CtlID<=ID_NAV_ABOUT;
 int primary=d->CtlID==ID_START || d->CtlID==ID_ADD || d->CtlID==IDOK || (nav && d->CtlID==ID_NAV_SERVICES+page);
 int disabled=d->itemState & ODS_DISABLED;
 COLORREF color=disabled?RGB(37,42,48):primary?accent:nav?RGB(27,33,39):RGB(44,53,62);
 if(!disabled && GetPropW(d->hwndItem,L"PortmanHot")) color=primary?RGB(112,235,188):RGB(57,69,80);
 if(d->itemState & ODS_SELECTED) color=RGB(62,93,83);
 HBRUSH b=CreateSolidBrush(color); FillRect(d->hDC,&d->rcItem,b); DeleteObject(b);
 SetBkMode(d->hDC,TRANSPARENT); SetTextColor(d->hDC,disabled?muted:primary?bg:fg);
 SelectObject(d->hDC,font); DrawTextW(d->hDC,text,-1,&d->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 if(d->itemState & ODS_FOCUS) { RECT r=d->rcItem; InflateRect(&r,-3,-3); DrawFocusRect(d->hDC,&r); }
}
static void column(HWND lv,int n,const wchar_t *s,int width) {
 LVCOLUMNW col={0}; col.mask=LVCF_TEXT|LVCF_WIDTH|LVCF_SUBITEM; col.pszText=(LPWSTR)s; col.cx=px(width); col.iSubItem=n;
 ListView_InsertColumn(lv,n,&col);
}
static void cell(HWND lv,int row,int col,const wchar_t *s) { ListView_SetItemText(lv,row,col,(LPWSTR)s); }
static HWND table(HWND h,int id) {
 HWND lv=control(h,WC_LISTVIEWW,L"",WS_TABSTOP|WS_BORDER|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,id,0,0,0,0);
 ListView_SetExtendedListViewStyle(lv,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_LABELTIP);
 ListView_SetBkColor(lv,field); ListView_SetTextBkColor(lv,field); ListView_SetTextColor(lv,fg);
 return lv;
}
static int current(void) { return ListView_GetNextItem(list,-1,LVNI_SELECTED); }
static void selectrow(int i) {
 if(i>=0 && i<pmd_count()) { ListView_SetItemState(list,i,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED); ListView_EnsureVisible(list,i,FALSE); }
}
static void openpath(HWND h,const wchar_t *path) {
 HINSTANCE rc=ShellExecuteW(h,L"open",path,NULL,NULL,SW_SHOWNORMAL);
 if((INT_PTR)rc<=32) errorbox(h,"Windows could not open this location or application.");
}
static void refresh(void) {
 int n=pmd_count(), i;
 SendMessageW(list,WM_SETREDRAW,FALSE,0);
 if(previous_count!=n) {
   int was=current(); ListView_DeleteAllItems(list);
   for(i=0;i<n;i++) { LVITEMW item={0}; item.mask=LVIF_TEXT; item.iItem=i; item.pszText=L""; ListView_InsertItem(list,&item); }
   previous_count=n; selectrow(was<n?was:n-1);
 }
 for(i=0;i<n;i++) {
  pmd_service s; wchar_t w[2048]; pmd_get(i,&s);
  to_w(s.name,w,2048); cell(list,i,0,w);
  const wchar_t *states[]={L"Stopped",L"Starting...",L"Listening",L"Running",L"Failed",L"Check port"};
  cell(list,i,1,states[s.state>=0 && s.state<6?s.state:4]);
  if(s.port) swprintf(w,2048,L"%u",s.port); else wcscpy(w,L"--"); cell(list,i,2,w);
  if(s.pid) swprintf(w,2048,L"%u",s.pid); else wcscpy(w,L"--"); cell(list,i,3,w);
  if(s.pid) swprintf(w,2048,L"%02llu:%02llu:%02llu",s.elapsed/3600,(s.elapsed/60)%60,s.elapsed%60); else swprintf(w,2048,L"exit %d",s.exit_code); cell(list,i,4,w);
  to_w(s.command,w,2048); cell(list,i,5,w);
 }
 SendMessageW(list,WM_SETREDRAW,TRUE,0); InvalidateRect(list,NULL,FALSE);
 selected=current(); pmd_service s; int exists=pmd_get(selected,&s), active=exists && s.pid;
 EnableWindow(GetDlgItem(mainwin,ID_START),exists && !active);
 EnableWindow(GetDlgItem(mainwin,ID_STOP),active); EnableWindow(GetDlgItem(mainwin,ID_RESTART),exists);
 EnableWindow(GetDlgItem(mainwin,ID_EDIT),exists && !active); EnableWindow(GetDlgItem(mainwin,ID_REMOVE),exists && !active);
 EnableWindow(GetDlgItem(mainwin,ID_BROWSER),exists && s.port && s.state==2);
 EnableWindow(GetDlgItem(mainwin,ID_FOLDER),exists); EnableWindow(GetDlgItem(mainwin,ID_CLEAR),exists && !active);
 EnableWindow(GetDlgItem(mainwin,ID_OPENLOG),exists); EnableWindow(GetDlgItem(mainwin,ID_EXPORT),n>0);
 wchar_t w[240]; swprintf(w,240,L"%d services   /   %d active     |     Windows local development",n,pmd_running()); SetWindowTextW(subtitle,w);
 if(exists) {
  wchar_t cwd[2048]; to_w(s.cwd,cwd,2048);
  const wchar_t *tip=s.state==5?L"Process alive; expected TCP port missing, unknown or owned by another process.":s.state==1?L"Waiting for the configured TCP port. Check the command if this takes too long.":cwd;
  SetWindowTextW(status,tip);
 } else SetWindowTextW(status,L"Start here: Add service, choose your project folder, then enter its start command.");
}
/* Display a bounded log tail; strip common ANSI CSI sequences and normalize LF for native edit. */
static void refreshlog(void) {
 static wchar_t previous[100000]; char buf[32000]; wchar_t decoded[34000], shown[68000];
 int i=current(); pmd_service s; wchar_t titletext[160];
 if(pmd_get(i,&s)) { wchar_t name[80]; to_w(s.name,name,80); swprintf(titletext,160,L"OUTPUT / %s    (latest 32 KB)",name); }
 else wcscpy(titletext,L"OUTPUT / Select a service");
 SetWindowTextW(logtitle,titletext);
 int n=pmd_log(i,buf,sizeof(buf));
 if(!n) strcpy(buf,i<0?"Add or select a service to view its output.":"No output yet. Start the service to see its stdout and stderr here.");
 to_w(buf,decoded,34000); int j=0;
 for(int k=0;decoded[k] && j<67990;k++) {
  if(decoded[k]==27 && decoded[k+1]==L'[') { k+=2; while(decoded[k] && !(decoded[k]>=0x40 && decoded[k]<=0x7e)) k++; if(!decoded[k]) break; continue; }
  if(decoded[k]==L'\n' && (k==0 || decoded[k-1]!=L'\r')) shown[j++]=L'\r';
  shown[j++]=decoded[k];
 }
 shown[j]=0;
 if(wcscmp(previous,shown)) { wcscpy(previous,shown); SetWindowTextW(output,shown); SendMessageW(output,EM_SETSEL,j,j); SendMessageW(output,EM_SCROLLCARET,0,0); }
}
static void layout(void) {
 RECT r; GetClientRect(mainwin,&r); int w=MulDiv(r.right,96,scale),h=MulDiv(r.bottom,96,scale);
 int left=204,cw=w-left-24;
 move(title,left,19,cw-140,33); move(subtitle,left,58,cw-120,22);
 move(GetDlgItem(mainwin,ID_TRAY),w-138,26,112,32);
 move(GetDlgItem(mainwin,ID_NAV_SERVICES),18,112,162,38); move(GetDlgItem(mainwin,ID_NAV_PORTS),18,158,162,38);
 move(GetDlgItem(mainwin,ID_NAV_SETTINGS),18,204,162,38); move(GetDlgItem(mainwin,ID_NAV_ABOUT),18,250,162,38);
 move(brand,18,h-72,162,48);
 move(GetDlgItem(mainwin,ID_ADD),left,102,124,32); move(GetDlgItem(mainwin,ID_IMPORT),left+134,102,134,32);
 move(GetDlgItem(mainwin,ID_EDIT),left+278,102,78,32); move(GetDlgItem(mainwin,ID_REMOVE),left+366,102,88,32);
 move(GetDlgItem(mainwin,ID_EXPORT),left+464,102,110,32);
 move(GetDlgItem(mainwin,ID_STARTALL),w-242,102,104,32); move(GetDlgItem(mainwin,ID_STOPALL),w-128,102,104,32);
 int lh=(h-340)/2; if(lh<130) lh=130;
 move(list,left,149,cw,lh);
 int y=160+lh;
 int ids[]={ID_START,ID_STOP,ID_RESTART,ID_BROWSER,ID_FOLDER,ID_OPENLOG}; int widths[]={92,92,100,122,116,100};
 int x=left; for(int i=0;i<6;i++) { move(GetDlgItem(mainwin,ids[i]),x,y,widths[i],32); x+=widths[i]+10; }
 move(status,left+1,y+42,cw-2,32); move(logtitle,left+1,y+78,cw-176,24);
 move(GetDlgItem(mainwin,ID_CLEAR),w-124,y+71,100,28);
 move(output,left,y+105,cw,h-y-129);
 move(settings_title,left,116,cw,38); move(settings_copy,left,169,cw,70);
 move(loginbox,left,255,310,30); move(GetDlgItem(mainwin,ID_LOGS),left,303,140,34);
 move(about_title,left,116,cw,38); move(about_copy,left,174,cw,150);
 ListView_SetColumnWidth(list,5,px(cw-514));
}

static void show_page(int next) {
 page=next;
 int service_ids[]={ID_ADD,ID_IMPORT,ID_EDIT,ID_REMOVE,ID_EXPORT,ID_STARTALL,ID_STOPALL,ID_LIST,ID_START,ID_STOP,ID_RESTART,ID_BROWSER,ID_FOLDER,ID_OPENLOG,ID_STATUS,ID_CLEAR,ID_OUTPUT};
 for(unsigned i=0;i<sizeof(service_ids)/sizeof(service_ids[0]);i++) ShowWindow(GetDlgItem(mainwin,service_ids[i]),page==0?SW_SHOW:SW_HIDE);
 ShowWindow(logtitle,page==0?SW_SHOW:SW_HIDE);
 ShowWindow(settings_title,page==2?SW_SHOW:SW_HIDE); ShowWindow(settings_copy,page==2?SW_SHOW:SW_HIDE);
 ShowWindow(loginbox,page==2?SW_SHOW:SW_HIDE); ShowWindow(GetDlgItem(mainwin,ID_LOGS),page==2?SW_SHOW:SW_HIDE);
 ShowWindow(about_title,page==3?SW_SHOW:SW_HIDE); ShowWindow(about_copy,page==3?SW_SHOW:SW_HIDE);
 SetWindowTextW(title,page==0?L"Services":page==1?L"Ports":page==2?L"Settings":L"About Portman");
 SetWindowTextW(subtitle,page==0?L"Start, stop, and understand every local service.":page==1?L"Inspect TCP listeners without terminating foreign processes.":page==2?L"Simple preferences for your Windows workflow.":L"A lightweight local development control panel.");
 for(int id=ID_NAV_SERVICES;id<=ID_NAV_ABOUT;id++) InvalidateRect(GetDlgItem(mainwin,id),NULL,TRUE);
 layout();
}
static void hide_tray(void) {
 Shell_NotifyIconW(NIM_DELETE,&tray);
 if(!Shell_NotifyIconW(NIM_ADD,&tray)) { errorbox(mainwin,"System tray is unavailable. The panel will stay open."); return; }
 ShowWindow(mainwin,SW_HIDE);
}
static int login_enabled(void) { HKEY key; DWORD t=0,sz=0; LONG r=RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_READ,&key); if(r) return 0; r=RegQueryValueExW(key,L"Portman",NULL,&t,NULL,&sz); RegCloseKey(key); return r==ERROR_SUCCESS; }
static void set_login(void) {
 int enabled=SendMessageW(loginbox,BM_GETCHECK,0,0)==BST_CHECKED; HKEY key;
 LONG r=RegCreateKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL);
 if(r==ERROR_SUCCESS) {
  if(enabled) { wchar_t exe[2048],cmd[2100]; GetModuleFileNameW(NULL,exe,2048); swprintf(cmd,2100,L"\"%s\"",exe); r=RegSetValueExW(key,L"Portman",0,REG_SZ,(BYTE*)cmd,(DWORD)((wcslen(cmd)+1)*sizeof(wchar_t))); }
  else { r=RegDeleteValueW(key,L"Portman"); if(r==ERROR_FILE_NOT_FOUND) r=ERROR_SUCCESS; } RegCloseKey(key);
 }
 if(r!=ERROR_SUCCESS) { errorbox(mainwin,"Could not update startup setting."); SendMessageW(loginbox,BM_SETCHECK,login_enabled()?BST_CHECKED:BST_UNCHECKED,0); }
}

/* Service editor, using actual commands instead of silently rewriting framework options. */
enum { E_NAME=500,E_COMMAND,E_CWD,E_PORT,E_BROWSE,E_TEMPLATE };
static HWND editor, e_name,e_cmd,e_cwd,e_port,e_template;
static int edit_index, modal_done;
static const wchar_t *templates[]={L"Custom command",L"Node / npm dev",L"Bun dev",L"Laravel / PHP",L"PHP built-in server",L"Python HTTP server"};
static void pickfolder(HWND h) {
 BROWSEINFOW bi={0}; bi.hwndOwner=h; bi.lpszTitle=L"Choose project folder"; bi.ulFlags=BIF_RETURNONLYFSDIRS|BIF_NEWDIALOGSTYLE;
 PIDLIST_ABSOLUTE id=SHBrowseForFolderW(&bi); if(id) { wchar_t p[MAX_PATH]; if(SHGetPathFromIDListW(id,p)) SetWindowTextW(e_cwd,p); CoTaskMemFree(id); }
}
static void save_editor(HWND h) {
 pmd_service s={0}; wchar_t w[4096],pw[20];
 GetWindowTextW(e_name,w,4096); if(!to_u(w,s.name,sizeof(s.name))) { errorbox(h,"Service name is too long."); return; }
 GetWindowTextW(e_cmd,w,4096); if(!to_u(w,s.command,sizeof(s.command))) { errorbox(h,"Command is too long in UTF-8."); return; }
 GetWindowTextW(e_cwd,w,4096); if(!to_u(w,s.cwd,sizeof(s.cwd))) { errorbox(h,"Folder path is too long in UTF-8."); return; }
 GetWindowTextW(e_port,pw,20); if(pw[0]) { wchar_t *end; unsigned long p=wcstoul(pw,&end,10); if(*end || !p || p>65535) { errorbox(h,"Port must be between 1 and 65535, or left blank."); return; } s.port=(unsigned short)p; }
 if(!pmd_put(edit_index,&s)) { backend_error(h); return; } modal_done=1; DestroyWindow(h);
}
static LRESULT CALLBACK EditorProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
 switch(msg) {
 case WM_CREATE: {
  label(h,L"SERVICE DETAILS",24,20,540,24);
  label(h,L"Template",24,59,100,22);
  e_template=control(h,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,E_TEMPLATE,145,53,418,200);
  for(int i=0;i<6;i++) SendMessageW(e_template,CB_ADDSTRING,0,(LPARAM)templates[i]); SendMessageW(e_template,CB_SETCURSEL,0,0);
  label(h,L"Name",24,104,108,22); e_name=edit(h,E_NAME,145,98,418,64);
  label(h,L"Project folder",24,149,120,22); e_cwd=edit(h,E_CWD,145,143,326,1800); button(h,L"Browse",E_BROWSE,479,142,84);
  label(h,L"Command",24,194,108,22); e_cmd=edit(h,E_COMMAND,145,188,418,1800);
  label(h,L"Expected port",24,239,115,22); e_port=edit(h,E_PORT,145,233,100,5);
  label(h,L"Optional. Must match the port used by your command.",24,279,539,25);
  label(h,L"Examples: npm run dev  /  php artisan serve --port=8000\nCommands run inside the project folder using cmd.exe.\nRuntime tools (Node, PHP, etc.) must already be installed.",24,311,539,68);
  button(h,L"Save service",IDOK,318,398,132); button(h,L"Cancel",IDCANCEL,461,398,102);
  if(edit_index>=0) { pmd_service s; wchar_t w[4096]; pmd_get(edit_index,&s); to_w(s.name,w,4096); SetWindowTextW(e_name,w); to_w(s.command,w,4096); SetWindowTextW(e_cmd,w); to_w(s.cwd,w,4096); SetWindowTextW(e_cwd,w); if(s.port) { swprintf(w,4096,L"%u",s.port); SetWindowTextW(e_port,w); } }
  SetFocus(e_name); return 0;
 }
 case WM_COMMAND:
  switch(LOWORD(wp)) {
  case IDOK: save_editor(h); break;
  case IDCANCEL: DestroyWindow(h); break;
  case E_BROWSE: pickfolder(h); break;
  case E_TEMPLATE: if(HIWORD(wp)==CBN_SELCHANGE) { int t=(int)SendMessageW(e_template,CB_GETCURSEL,0,0); const wchar_t *cmds[]={L"",L"npm run dev",L"bun run dev",L"php artisan serve --host=127.0.0.1 --port=8000",L"php -S 127.0.0.1:8000",L"python -m http.server 8080 --bind 127.0.0.1"}; const wchar_t *ports[]={L"",L"3000",L"3000",L"8000",L"8000",L"8080"}; if(t>=0 && t<6) { SetWindowTextW(e_cmd,cmds[t]); SetWindowTextW(e_port,ports[t]); } } break;
  } return 0;
 case WM_CLOSE: DestroyWindow(h); return 0;
 case WM_DESTROY: modal_done=1; return 0;
 case WM_DRAWITEM: drawbutton((DRAWITEMSTRUCT*)lp); return TRUE;
 case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORLISTBOX: case WM_CTLCOLORBTN: return colors(msg,wp);
 } return DefWindowProcW(h,msg,wp,lp);
}
static void edit_service(int i) {
 edit_index=i; modal_done=0;
 EnableWindow(mainwin,FALSE);
 RECT r; GetWindowRect(mainwin,&r);
 editor=CreateWindowExW(WS_EX_DLGMODALFRAME,L"PortmanServiceEditor",i<0?L"Add service":L"Edit service",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU,r.left+px(60),r.top+px(40),px(604),px(487),mainwin,NULL,instance,NULL);
 if(!editor) { EnableWindow(mainwin,TRUE); return; }
 ShowWindow(editor,SW_SHOW);
 MSG m; while(!modal_done && GetMessageW(&m,NULL,0,0)>0) { if(m.message==WM_KEYDOWN && m.wParam==VK_ESCAPE) DestroyWindow(editor); else if(m.message==WM_KEYDOWN && m.wParam==VK_RETURN) save_editor(editor); else if(!IsDialogMessageW(editor,&m)) { TranslateMessage(&m); DispatchMessageW(&m); } }
 EnableWindow(mainwin,TRUE); SetForegroundWindow(mainwin); refresh(); if(i<0) selectrow(pmd_count()-1); refreshlog();
}
static void import_config(void) {
 wchar_t file[2048]=L""; OPENFILENAMEW of={0}; of.lStructSize=sizeof(of); of.hwndOwner=mainwin; of.lpstrFilter=L"Portman config (*.toml)\0*.toml\0All files\0*.*\0"; of.lpstrFile=file; of.nMaxFile=2048; of.Flags=OFN_FILEMUSTEXIST|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetOpenFileNameW(&of)) return;
 if(MessageBoxW(mainwin,L"Import services from this configuration?\n\nOnly import configurations you trust. Their commands will execute when you press Start.\nExisting services are kept; duplicate names or ports cancel the import.",L"Import dev.toml",MB_OKCANCEL|MB_ICONINFORMATION)!=IDOK) return;
 char u[8192]; if(!to_u(file,u,sizeof(u))) return;
 if(!pmd_import(u)) backend_error(mainwin); refresh(); refreshlog();
}
static void export_config(void) {
 wchar_t file[2048]=L"portman-services.toml";
 OPENFILENAMEW of={0}; of.lStructSize=sizeof(of); of.hwndOwner=mainwin;
 of.lpstrFilter=L"Portman config (*.toml)\0*.toml\0"; of.lpstrFile=file;
 of.nMaxFile=2048; of.lpstrDefExt=L"toml";
 of.Flags=OFN_OVERWRITEPROMPT|OFN_PATHMUSTEXIST|OFN_NOCHANGEDIR;
 if(!GetSaveFileNameW(&of)) return;
 char u[8192]; if(!to_u(file,u,sizeof(u))) { errorbox(mainwin,"Export path is too long."); return; }
 if(!pmd_export(u)) { backend_error(mainwin); return; }
 MessageBoxW(mainwin,L"Configuration exported.\n\nProject files and logs are not included. Folder paths remain absolute; review paths and commands before sharing or importing on another computer.",L"Export complete",MB_OK|MB_ICONINFORMATION);
}
/* Read-only TCP inspector: never offers to terminate arbitrary Windows processes. */
static HWND portswin,portlist,portstatus,portsearch;
static pm_row *portrows;
static int portcount=-1;
static void render_ports(void) {
 wchar_t query[256]; char utf[1024]; GetWindowTextW(portsearch,query,256);
 if(!to_u(query,utf,sizeof(utf))) utf[0]=0;
 SendMessageW(portlist,WM_SETREDRAW,FALSE,0); ListView_DeleteAllItems(portlist);
 int visible=0;
 if(portcount<0) SetWindowTextW(portstatus,L"Port scan failed. Try refreshing.");
 else {
  for(int i=0;i<portcount;i++) {
   pm_row *row=&portrows[i]; if(!pmd_row_matches(row,utf)) continue;
   wchar_t w[2048]; LVITEMW item={0}; swprintf(w,2048,L"%u",row->port);
   item.mask=LVIF_TEXT; item.iItem=visible; item.pszText=w; ListView_InsertItem(portlist,&item);
   swprintf(w,2048,L"%u",row->pid); cell(portlist,visible,1,w);
   to_w(row->name,w,2048); cell(portlist,visible,2,w);
   to_w(row->address,w,2048); cell(portlist,visible,3,w);
   to_w(row->executable,w,2048); cell(portlist,visible,4,w); visible++;
  }
  wchar_t w[200]; swprintf(w,200,L"%d of %d TCP listeners (IPv4 + IPv6). Refresh to rescan.",visible,portcount); SetWindowTextW(portstatus,w);
 }
 SendMessageW(portlist,WM_SETREDRAW,TRUE,0); InvalidateRect(portlist,NULL,FALSE);
}
static void refresh_ports(void) {
 if(!portrows) portrows=calloc(8192,sizeof(pm_row));
 portcount=portrows?pm_scan(portrows,8192):-1; render_ports();
}
static LRESULT CALLBACK PortsProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
 switch(msg) {
 case WM_CREATE: portlist=table(h,700); column(portlist,0,L"Port",66); column(portlist,1,L"PID",74); column(portlist,2,L"Process",175); column(portlist,3,L"Address",170); column(portlist,4,L"Executable",380); portstatus=label(h,L"",20,12,760,26); button(h,L"Refresh",701,20,422,100); portsearch=edit(h,702,20,49,800,255); SendMessageW(portsearch,EM_SETCUEBANNER,TRUE,(LPARAM)L"Search port, PID, process, address or path..."); refresh_ports(); return 0;
 case WM_SIZE: { RECT r; GetClientRect(h,&r); int w=MulDiv(r.right,96,scale),ht=MulDiv(r.bottom,96,scale); move(portlist,20,91,w-40,ht-155); move(portsearch,20,49,w-40,29); move(GetDlgItem(h,701),20,ht-49,100,32); move(portstatus,20,16,w-40,24); return 0; }
 case WM_COMMAND: if(LOWORD(wp)==701) refresh_ports(); if(LOWORD(wp)==702 && HIWORD(wp)==EN_CHANGE) render_ports(); return 0;
 case WM_DESTROY: free(portrows); portrows=NULL; portcount=-1; portswin=NULL; return 0;
 case WM_DRAWITEM: drawbutton((DRAWITEMSTRUCT*)lp); return TRUE;
 case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN: return colors(msg,wp);
 } return DefWindowProcW(h,msg,wp,lp);
}
static void show_ports(void) {
 if(portswin) { SetForegroundWindow(portswin); return; }
 portswin=CreateWindowExW(0,L"PortmanPorts",L"Port inspector - TCP listeners",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,px(940),px(520),mainwin,NULL,instance,NULL); ShowWindow(portswin,SW_SHOW);
}
static void do_action(int id) {
 int i=current(); pmd_service s; wchar_t w[4096]; char path[8192];
 switch(id) {
 case ID_ADD: edit_service(-1); break;
 case ID_EDIT: if(i>=0) edit_service(i); break;
 case ID_REMOVE: if(i>=0 && MessageBoxW(mainwin,L"Remove this service from Portman?\nYour project files and logs will be kept.",L"Remove service",MB_YESNO|MB_ICONQUESTION)==IDYES && !pmd_remove(i)) backend_error(mainwin); break;
 case ID_IMPORT: import_config(); break;
 case ID_EXPORT: export_config(); break;
 case ID_START: if(!pmd_start(i)) backend_error(mainwin); break;
 case ID_STOP: pmd_stop(i); break;
 case ID_RESTART: pmd_stop(i); if(!pmd_start(i)) backend_error(mainwin); break;
 case ID_STARTALL: for(int j=0;j<pmd_count();j++) if(!pmd_start(j)) { backend_error(mainwin); break; } break;
 case ID_STOPALL: if(pmd_running() && MessageBoxW(mainwin,L"Stop all services launched by Portman?\nWindows will terminate their process trees.",L"Stop all",MB_YESNO|MB_ICONQUESTION)==IDYES) pmd_shutdown(); break;
 case ID_FOLDER: if(pmd_get(i,&s)) { to_w(s.cwd,w,4096); openpath(mainwin,w); } break;
 case ID_BROWSER: if(pmd_get(i,&s) && s.port && s.state==2) { swprintf(w,4096,L"http://127.0.0.1:%u",s.port); openpath(mainwin,w); } break;
 case ID_LOGS: swprintf(w,4096,L"%s\\logs",data_dir); openpath(mainwin,w); break;
 case ID_OPENLOG: if(pmd_log_path(i,path,sizeof(path))) { to_w(path,w,4096); openpath(mainwin,w); } break;
 case ID_CLEAR: if(MessageBoxW(mainwin,L"Clear the selected service log?",L"Clear log",MB_YESNO|MB_ICONQUESTION)==IDYES && !pmd_clear_log(i)) backend_error(mainwin); break;
 case ID_PORTS: show_ports(); break;
 case ID_NAV_SERVICES: show_page(0); break;
 case ID_NAV_PORTS: show_ports(); break;
 case ID_NAV_SETTINGS: show_page(2); break;
 case ID_NAV_ABOUT: show_page(3); break;
 case ID_TRAY: hide_tray(); break;
 case ID_LOGIN: set_login(); break;
 }
 refresh(); refreshlog();
}
static LRESULT CALLBACK MainProc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
 if(msg==RegisterWindowMessageW(L"TaskbarCreated")) {
  if(!Shell_NotifyIconW(NIM_ADD,&tray)) ShowWindow(h,SW_RESTORE);
  return 0;
 }
 switch(msg) {
 case WM_CREATE: {
  mainwin=h;
  label(h,L"PORTMAN",18,24,162,32); title=label(h,L"Services",204,19,650,33); SendMessageW(title,WM_SETFONT,(WPARAM)titlefont,TRUE);
  subtitle=label(h,L"Start, stop, and understand every local service.",204,60,660,24);
  button(h,L"Services",ID_NAV_SERVICES,0,0,162); button(h,L"Ports",ID_NAV_PORTS,0,0,162); button(h,L"Settings",ID_NAV_SETTINGS,0,0,162); button(h,L"About",ID_NAV_ABOUT,0,0,162);
  brand=label(h,L"Built with \u2665\nby rakarmp (rezz990)",18,0,162,48);
  button(h,L"To tray",ID_TRAY,0,0,112);
  button(h,L"+ Add service",ID_ADD,0,0,124); button(h,L"Import dev.toml",ID_IMPORT,0,0,134);
  button(h,L"Edit",ID_EDIT,0,0,78); button(h,L"Remove",ID_REMOVE,0,0,88); button(h,L"Export config",ID_EXPORT,0,0,110);
  button(h,L"Start all",ID_STARTALL,0,0,104); button(h,L"Stop all",ID_STOPALL,0,0,104);
  list=table(h,ID_LIST); column(list,0,L"Service",156); column(list,1,L"Status",118); column(list,2,L"Port",65); column(list,3,L"PID",75); column(list,4,L"Uptime",100); column(list,5,L"Command",340);
  button(h,L"Start",ID_START,0,0,92); button(h,L"Stop",ID_STOP,0,0,92); button(h,L"Restart",ID_RESTART,0,0,100); button(h,L"Open browser",ID_BROWSER,0,0,122); button(h,L"Project folder",ID_FOLDER,0,0,116); button(h,L"Open log",ID_OPENLOG,0,0,100);
  status=label(h,L"",24,0,860,32); logtitle=label(h,L"OUTPUT",24,0,680,24); SendMessageW(logtitle,WM_SETFONT,(WPARAM)boldfont,TRUE);
  button(h,L"Clear log",ID_CLEAR,0,0,100);
  output=control(h,L"EDIT",L"",WS_TABSTOP|WS_BORDER|WS_VSCROLL|WS_HSCROLL|ES_MULTILINE|ES_READONLY|ES_AUTOVSCROLL|ES_AUTOHSCROLL,ID_OUTPUT,0,0,0,0); SendMessageW(output,WM_SETFONT,(WPARAM)monofont,TRUE); SendMessageW(output,EM_SETLIMITTEXT,100000,0);
  loginbox=control(h,L"BUTTON",L"Open Portman at sign-in",WS_TABSTOP|BS_AUTOCHECKBOX,ID_LOGIN,0,0,245,25); SendMessageW(loginbox,BM_SETCHECK,login_enabled()?BST_CHECKED:BST_UNCHECKED,0);
  button(h,L"Logs folder",ID_LOGS,0,0,122);
  settings_title=label(h,L"Windows preferences",0,0,500,38); SendMessageW(settings_title,WM_SETFONT,(WPARAM)titlefont,TRUE);
  settings_copy=label(h,L"STARTUP\nOpen the control panel when you sign in. Services never start automatically.",0,0,700,70);
  about_title=label(h,L"Local development, under control.",0,0,700,38); SendMessageW(about_title,WM_SETFONT,(WPARAM)titlefont,TRUE);
  about_copy=label(h,L"Portman 0.2.2\n\nNative Win32 interface. Zig service engine. No browser, Electron, .NET, Node.js, or administrator access required.\n\nBuilt with \u2665 by rakarmp (rezz990)",0,0,700,150);
  tray.cbSize=sizeof(tray); tray.hWnd=h; tray.uID=1; tray.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP; tray.uCallbackMessage=TRAYMSG; tray.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); wcscpy(tray.szTip,L"Portman - double-click to open");
  if(!Shell_NotifyIconW(NIM_ADD,&tray)) EnableWindow(GetDlgItem(h,ID_TRAY),FALSE);
  SetTimer(h,1,1500,NULL); refresh(); refreshlog(); show_page(0); return 0;
 }
 case WM_SIZE: layout(); return 0;
 case WM_GETMINMAXINFO: ((MINMAXINFO*)lp)->ptMinTrackSize.x=px(900); ((MINMAXINFO*)lp)->ptMinTrackSize.y=px(690); return 0;
 case WM_COMMAND: do_action(LOWORD(wp)); return 0;
 case WM_TIMER: pmd_tick(); refresh(); refreshlog(); return 0;
 case WM_NOTIFY: { NMHDR *n=(NMHDR*)lp; if(n->hwndFrom==list && n->code==LVN_ITEMCHANGED && current()!=selected) { selected=current(); refreshlog(); } if(n->hwndFrom==list && n->code==NM_DBLCLK && current()>=0) do_action(ID_EDIT); return 0; }
 case WM_DRAWITEM: drawbutton((DRAWITEMSTRUCT*)lp); return TRUE;
 case WM_CTLCOLORSTATIC: case WM_CTLCOLOREDIT: case WM_CTLCOLORBTN: return colors(msg,wp);
 case TRAYMSG:
  if(lp==WM_LBUTTONDBLCLK) { ShowWindow(h,SW_RESTORE); SetForegroundWindow(h); }
  if(lp==WM_RBUTTONUP) { POINT p; GetCursorPos(&p); HMENU m=CreatePopupMenu(); AppendMenuW(m,MF_STRING,800,L"Open Portman"); AppendMenuW(m,MF_STRING,801,L"Exit"); SetForegroundWindow(h); int cmd=TrackPopupMenu(m,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,h,NULL); DestroyMenu(m); if(cmd==800) { ShowWindow(h,SW_RESTORE); SetForegroundWindow(h); } if(cmd==801) SendMessageW(h,WM_CLOSE,0,0); } return 0;
 case WM_CLOSE:
  if(pmd_running()) { int r=MessageBoxW(h,L"Some services are still running.\n\nYes: stop them and exit Portman.\nNo: keep running in the system tray.\nCancel: return to the panel.",L"Exit Portman?",MB_YESNOCANCEL|MB_ICONQUESTION); if(r==IDCANCEL) return 0; if(r==IDNO) { hide_tray(); return 0; } }
  DestroyWindow(h); return 0;
 case WM_QUERYENDSESSION: return TRUE;
 case WM_ENDSESSION: if(wp) pmd_shutdown(); return 0;
 case WM_DESTROY: KillTimer(h,1); Shell_NotifyIconW(NIM_DELETE,&tray); pmd_shutdown(); PostQuitMessage(0); return 0;
 }
 return DefWindowProcW(h,msg,wp,lp);
}
int pm_desktop_main(void) {
 instance=GetModuleHandleW(NULL);
 single=CreateMutexW(NULL,FALSE,L"Local\\PortmanDesktop02");
 if(!single || GetLastError()==ERROR_ALREADY_EXISTS) { HWND old=FindWindowW(APPCLASS,NULL); if(old) { ShowWindow(old,SW_RESTORE); SetForegroundWindow(old); } if(single) CloseHandle(single); return 0; }
 SetProcessDPIAware(); HDC dc=GetDC(NULL); scale=GetDeviceCaps(dc,LOGPIXELSX); ReleaseDC(NULL,dc);
 CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
 wchar_t local[MAX_PATH]; if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,local))) { errorbox(NULL,"Cannot locate Local AppData."); return 1; }
 swprintf(data_dir,2048,L"%s\\Portman",local); char utf[8192]; to_u(data_dir,utf,sizeof(utf));
 if(!pmd_init(utf)) { backend_error(NULL); MessageBoxW(NULL,L"Configuration could not be loaded. Portman has not overwritten it.\nCheck services.toml in your Local AppData / Portman folder.",L"Portman",MB_OK); return 1; }
 bgbrush=CreateSolidBrush(bg); fieldbrush=CreateSolidBrush(field);
 font=CreateFontW(-px(14),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 boldfont=CreateFontW(-px(13),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 titlefont=CreateFontW(-px(25),0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 monofont=CreateFontW(-px(13),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
 WNDCLASSEXW cls={0}; cls.cbSize=sizeof(cls); cls.hInstance=instance; cls.hCursor=LoadCursorW(NULL,IDC_ARROW); cls.hbrBackground=bgbrush; cls.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1)); cls.lpfnWndProc=MainProc; cls.lpszClassName=APPCLASS; RegisterClassExW(&cls);
 cls.lpfnWndProc=EditorProc; cls.lpszClassName=L"PortmanServiceEditor"; RegisterClassExW(&cls);
 cls.lpfnWndProc=PortsProc; cls.lpszClassName=L"PortmanPorts"; RegisterClassExW(&cls);
 mainwin=CreateWindowExW(WS_EX_CONTROLPARENT,APPCLASS,L"Portman 0.2.2 - Windows Control Panel",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,px(1080),px(780),NULL,NULL,instance,NULL);
 if(!mainwin) { pmd_shutdown(); return 1; }
 ShowWindow(mainwin,SW_SHOW); UpdateWindow(mainwin);
 MSG m; while(GetMessageW(&m,NULL,0,0)>0) { if(!IsDialogMessageW(mainwin,&m)) { TranslateMessage(&m); DispatchMessageW(&m); } }
 DeleteObject(font); DeleteObject(boldfont); DeleteObject(titlefont); DeleteObject(monofont); DeleteObject(bgbrush); DeleteObject(fieldbrush); CloseHandle(single); CoUninitialize(); return 0;
}
