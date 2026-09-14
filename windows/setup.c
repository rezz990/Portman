#define UNICODE
#define _UNICODE
#define COBJMACROS
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <shlobj.h>
#include <shobjidl.h>
#include <shellapi.h>
#include <commctrl.h>
#include <stdio.h>
#include <wchar.h>
#include "setup.h"
static const wchar_t *keypath=L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Portman";
static wchar_t install_dir[2048], self[2048], programs[MAX_PATH], desktop[MAX_PATH];
static const unsigned char *gui_data,*cli_data,*guide_data;
static size_t gui_size,cli_size,guide_size;
static HWND window, state, launchbox, deskbox, installbtn, cancelbtn, detailsbtn, progressbar;
static HWND pathlabel, detailslabel, versionlabel;
static HFONT font, smallfont, boldfont, titlefont, mono_font;
static HBRUSH background, panel, accent_panel;
static int details_open=0;
static HANDLE installer_lock;
static void notice(const wchar_t *msg) { MessageBoxW(window,msg,L"Portman Setup",MB_OK|MB_ICONWARNING); }
static void paths(wchar_t *out,size_t cap,const wchar_t *name) { swprintf(out,cap,L"%s\\%s",install_dir,name); }
static void installer_progress(int value,const wchar_t *message) {
 if(progressbar) SendMessageW(progressbar,PBM_SETPOS,value,0);
 if(state) SetWindowTextW(state,message);
 if(window) { UpdateWindow(window); Sleep(value<100?90:40); }
}
static int write_file(const wchar_t *path,const unsigned char *bytes,size_t length) {
 HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE) return 0;
 DWORD done=0; int ok=length<0xffffffff && WriteFile(h,bytes,(DWORD)length,&done,NULL) && done==length && FlushFileBuffers(h);
 CloseHandle(h); return ok;
}
static int shortcut(const wchar_t *destination,const wchar_t *target) {
 IShellLinkW *link=NULL; IPersistFile *file=NULL;
 HRESULT hr=CoCreateInstance(&CLSID_ShellLink,NULL,CLSCTX_INPROC_SERVER,&IID_IShellLinkW,(void**)&link);
 if(FAILED(hr)) return 0;
 hr=IShellLinkW_SetPath(link,target);
 if(SUCCEEDED(hr)) hr=IShellLinkW_SetWorkingDirectory(link,install_dir);
 if(SUCCEEDED(hr)) hr=IShellLinkW_SetDescription(link,L"Portman local development control panel");
 if(SUCCEEDED(hr)) hr=IShellLinkW_QueryInterface(link,&IID_IPersistFile,(void**)&file);
 if(SUCCEEDED(hr)) { hr=IPersistFile_Save(file,destination,TRUE); IPersistFile_Release(file); }
 IShellLinkW_Release(link); return SUCCEEDED(hr);
}
static int reg_string(HKEY k,const wchar_t *name,const wchar_t *value) { return RegSetValueExW(k,name,0,REG_SZ,(const BYTE*)value,(DWORD)((wcslen(value)+1)*2))==ERROR_SUCCESS; }
static int active_app(void) {
 HANDLE h=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\PortmanDesktop02");
 if(h) { CloseHandle(h); return 1; } return 0;
}
static int install(void) {
 if(active_app()) { notice(L"Close Portman before installing or updating. Use Exit from its tray menu."); return 0; }
 installer_progress(6,L"Preparing a clean per-user installation...");
 int result=SHCreateDirectoryExW(NULL,install_dir,NULL);
 if(result!=ERROR_SUCCESS && result!=ERROR_ALREADY_EXISTS && result!=ERROR_FILE_EXISTS) { notice(L"Cannot create the installation directory."); return 0; }
 const wchar_t *names[]={L"Portman.exe",L"portman-cli.exe",L"Uninstall.exe",L"QUICKSTART.txt"};
 wchar_t temp[2200],dest[2200],backup[2200]; int backed[4]={0}, installed[4]={0};
 for(int i=0;i<4;i++) {
  swprintf(temp,2200,L"%s\\%s.new",install_dir,names[i]);
  int ok=i==0?write_file(temp,gui_data,gui_size):i==1?write_file(temp,cli_data,cli_size):i==2?CopyFileW(self,temp,FALSE):write_file(temp,guide_data,guide_size);
  if(!ok) { notice(L"Could not prepare application files. Check free space and folder permissions."); goto rollback; }
  installer_progress(15+(i+1)*14,L"Copying Portman components...");
 }
 installer_progress(70,L"Registering Portman with Windows...");
 for(int i=0;i<4;i++) {
  paths(dest,2200,names[i]); swprintf(backup,2200,L"%s.bak",dest); swprintf(temp,2200,L"%s.new",dest);
  if(GetFileAttributesW(dest)!=INVALID_FILE_ATTRIBUTES) {
   if(!MoveFileExW(dest,backup,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { notice(L"An existing file is in use. Close Portman and any CLI processes, then retry."); goto rollback; }
   backed[i]=1;
  }
  if(!MoveFileExW(temp,dest,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) { notice(L"Could not install application files."); goto rollback; }
  installed[i]=1;
 }
 HKEY key;
 if(RegCreateKeyExW(HKEY_CURRENT_USER,keypath,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL)!=ERROR_SUCCESS) { notice(L"Could not register the uninstaller."); goto rollback; }
 wchar_t uninstall[2200], icon[2200]; paths(dest,2200,L"Uninstall.exe"); swprintf(uninstall,2200,L"\"%s\" --uninstall",dest); paths(icon,2200,L"Portman.exe");
 int ok=reg_string(key,L"DisplayName",L"Portman") && reg_string(key,L"DisplayVersion",L"0.2.2") && reg_string(key,L"Publisher",L"Portman Project") && reg_string(key,L"InstallLocation",install_dir) && reg_string(key,L"DisplayIcon",icon) && reg_string(key,L"UninstallString",uninstall);
 DWORD one=1,size=(DWORD)((gui_size+cli_size+guide_size)*2/1024);
 RegSetValueExW(key,L"NoModify",0,REG_DWORD,(BYTE*)&one,4); RegSetValueExW(key,L"NoRepair",0,REG_DWORD,(BYTE*)&one,4); RegSetValueExW(key,L"EstimatedSize",0,REG_DWORD,(BYTE*)&size,4); RegCloseKey(key);
 if(!ok) { notice(L"Registration was incomplete. The application files are installed; run setup again to repair registration."); return 0; }
 paths(dest,2200,L"Portman.exe"); swprintf(temp,2200,L"%s\\Portman.lnk",programs);
 installer_progress(84,L"Creating your shortcuts...");
 if(!shortcut(temp,dest)) notice(L"Installed, but the Start menu shortcut could not be created. Open Portman.exe in the install folder.");
 if(SendMessageW(deskbox,BM_GETCHECK,0,0)==BST_CHECKED) { swprintf(temp,2200,L"%s\\Portman.lnk",desktop); if(!shortcut(temp,dest)) notice(L"The desktop shortcut could not be created."); }
 for(int i=0;i<4;i++) { swprintf(backup,2200,L"%s\\%s.bak",install_dir,names[i]); DeleteFileW(backup); }
 installer_progress(100,L"Portman is ready. Launching your control panel...");
 return 1;
rollback:
 for(int i=3;i>=0;i--) {
  paths(dest,2200,names[i]); swprintf(backup,2200,L"%s.bak",dest); swprintf(temp,2200,L"%s.new",dest);
  if(installed[i]) DeleteFileW(dest);
  if(backed[i] && !MoveFileExW(backup,dest,MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) notice(L"A previous file could not be restored. A .bak copy remains in the install folder.");
  DeleteFileW(temp);
 } return 0;
}
static int uninstall_cleanup(void) {
 if(active_app()) { notice(L"Portman is running. Close it, then run the uninstaller again."); return 1; }
 const wchar_t *names[]={L"Portman.exe",L"portman-cli.exe",L"QUICKSTART.txt",L"Uninstall.exe"};
 wchar_t p[2200]; int failed=0;
 for(int i=0;i<4;i++) { paths(p,2200,names[i]); if(!DeleteFileW(p) && GetLastError()!=ERROR_FILE_NOT_FOUND) failed=1; }
 if(failed) { notice(L"Some application files are in use and could not be removed. Close their processes and retry. Your settings and project files were kept."); return 1; }
 swprintf(p,2200,L"%s\\Portman.lnk",programs); DeleteFileW(p);
 swprintf(p,2200,L"%s\\Portman.lnk",desktop); DeleteFileW(p);
 RegDeleteTreeW(HKEY_CURRENT_USER,keypath);
 HKEY k; if(RegOpenKeyExW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Run",0,KEY_SET_VALUE,&k)==ERROR_SUCCESS) { RegDeleteValueW(k,L"Portman"); RegCloseKey(k); }
 RemoveDirectoryW(install_dir);
 MessageBoxW(NULL,L"Portman was removed.\n\nYour service configuration and logs are kept in Local AppData / Portman. Project folders were not removed.",L"Portman",MB_OK|MB_ICONINFORMATION);
 return 0;
}
static int uninstall(void) {
 if(active_app()) { notice(L"Close Portman before uninstalling. Use Exit from its tray menu."); return 1; }
 if(MessageBoxW(NULL,L"Uninstall Portman for this user?\n\nProject files, service configuration and logs will be kept.",L"Uninstall Portman",MB_YESNO|MB_ICONQUESTION)!=IDYES) return 0;
 wchar_t dir[MAX_PATH],temp[MAX_PATH],command[2400];
 if(!GetTempPathW(MAX_PATH,dir) || !GetTempFileNameW(dir,L"pmun",0,temp) || !CopyFileW(self,temp,FALSE)) { notice(L"Cannot prepare the uninstaller in the temporary folder."); return 1; }
 // A temporary helper waits for the installed uninstaller to exit before deleting it.
 swprintf(command,2400,L"\"%s\" --cleanup %lu",temp,GetCurrentProcessId());
 STARTUPINFOW si={0}; si.cb=sizeof(si); PROCESS_INFORMATION pi={0};
 if(!CreateProcessW(temp,command,NULL,NULL,FALSE,0,NULL,NULL,&si,&pi)) { DeleteFileW(temp); notice(L"Cannot launch the uninstaller."); return 1; }
 CloseHandle(pi.hThread); CloseHandle(pi.hProcess); return 0;
}
static void draw_text(HDC dc,const wchar_t *text,RECT r,HFONT f,COLORREF color,UINT flags) {
 HFONT old=(HFONT)SelectObject(dc,f); SetBkMode(dc,TRANSPARENT); SetTextColor(dc,color); DrawTextW(dc,text,-1,&r,flags); SelectObject(dc,old);
}
static void fill_card(HDC dc,int x,int y,int w,int h,const wchar_t *eyebrow,const wchar_t *heading,const wchar_t *body) {
 RECT r={x,y,x+w,y+h}; HBRUSH b=CreateSolidBrush(RGB(35,43,50)); FillRect(dc,&r,b); DeleteObject(b);
 RECT a={x+18,y+14,x+w-18,y+32}; draw_text(dc,eyebrow,a,smallfont,RGB(90,218,170),DT_SINGLELINE);
 RECT t={x+18,y+37,x+w-18,y+60}; draw_text(dc,heading,t,boldfont,RGB(238,245,250),DT_SINGLELINE);
 RECT q={x+18,y+62,x+w-18,y+h-10}; draw_text(dc,body,q,smallfont,RGB(165,181,193),DT_WORDBREAK);
}
static void paint_install_window(HWND h,HDC dc) {
 RECT client; GetClientRect(h,&client); int w=client.right, height=client.bottom;
 HBRUSH b=CreateSolidBrush(RGB(19,24,29)); FillRect(dc,&client,b); DeleteObject(b);
 HBRUSH hero=CreateSolidBrush(RGB(28,38,43)); RECT hr={0,0,w,128}; FillRect(dc,&hr,hero); DeleteObject(hero);
 RECT line={0,124,w,128}; HBRUSH green=CreateSolidBrush(RGB(90,218,170)); FillRect(dc,&line,green); DeleteObject(green);
 RECT r={36,25,w-220,64}; draw_text(dc,L"PORTMAN",r,titlefont,RGB(238,245,250),DT_SINGLELINE);
 RECT s={38,67,w-220,91}; draw_text(dc,L"LOCAL DEVELOPMENT, UNDER CONTROL.",s,smallfont,RGB(157,181,190),DT_SINGLELINE);
 RECT by={w-208,32,w-35,58}; draw_text(dc,L"BUILT WITH \u2665",by,boldfont,RGB(90,218,170),DT_RIGHT|DT_SINGLELINE);
 RECT who={w-208,59,w-35,81}; draw_text(dc,L"by rakarmp (rezz990)",who,smallfont,RGB(188,204,211),DT_RIGHT|DT_SINGLELINE);
 RECT badge={w-130,91,w-35,113}; HBRUSH badgebrush=CreateSolidBrush(RGB(49,67,70)); FillRect(dc,&badge,badgebrush); DeleteObject(badgebrush); draw_text(dc,L"WINDOWS x64",badge,smallfont,RGB(210,240,232),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 RECT intro={36,150,w-36,181}; draw_text(dc,L"Install your local development control panel",intro,boldfont,RGB(238,245,250),DT_SINGLELINE);
 RECT copy={36,184,w-36,218}; draw_text(dc,L"A focused native workspace for your projects, ports, processes, and logs.",copy,font,RGB(165,181,193),DT_SINGLELINE);
 int cw=(w-96)/3; fill_card(dc,36,238,cw,92,L"01  ORGANIZE",L"One clean panel",L"Keep project services together"); fill_card(dc,48+cw,238,cw,92,L"02  OBSERVE",L"Know what runs",L"Ports, PIDs, uptime, and logs"); fill_card(dc,60+cw*2,238,cw,92,L"03  CONTROL",L"Stop with confidence",L"Managed process-tree cleanup");
 RECT info={36,350,w-36,374}; draw_text(dc,L"INSTALL LOCATION",info,smallfont,RGB(90,218,170),DT_SINGLELINE);
 RECT path={36,378,w-36,409}; HBRUSH p=CreateSolidBrush(RGB(35,43,50)); FillRect(dc,&path,p); DeleteObject(p); draw_text(dc,install_dir,path,mono_font,RGB(219,232,238),DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
 RECT note={36,420,w-36,444}; draw_text(dc,L"Per-user install • local-first • no runtime bundled • Node/PHP/Bun/Python stay yours to manage",note,smallfont,RGB(145,164,176),DT_SINGLELINE);
 RECT footer={36,height-31,w-36,height-8}; draw_text(dc,L"Portman 0.2.2  •  Built with \u2665 by rakarmp (rezz990)",footer,smallfont,RGB(123,144,154),DT_SINGLELINE);
}
static void draw_button(DRAWITEMSTRUCT *d) {
 wchar_t text[160]; GetWindowTextW(d->hwndItem,text,160);
 int checkbox=(d->CtlID==102 || d->CtlID==103);
 if(checkbox) {
  RECT box=d->rcItem; box.right=box.left+18; box.bottom=box.top+18; HBRUSH qb=CreateSolidBrush(RGB(35,43,50)); FillRect(d->hDC,&box,qb); DeleteObject(qb);
  HBRUSH frame=CreateSolidBrush(RGB(104,126,135)); FrameRect(d->hDC,&box,frame); DeleteObject(frame);
  if(SendMessageW(d->hwndItem,BM_GETCHECK,0,0)==BST_CHECKED) { HBRUSH cb=CreateSolidBrush(RGB(90,218,170)); FillRect(d->hDC,&box,cb); DeleteObject(cb); draw_text(d->hDC,L"\u2713",box,boldfont,RGB(19,24,29),DT_CENTER|DT_VCENTER|DT_SINGLELINE); }
  RECT label=d->rcItem; label.left+=27; draw_text(d->hDC,text,label,font,RGB(214,228,232),DT_VCENTER|DT_SINGLELINE); return;
 }
 COLORREF color=(d->CtlID==IDOK)?RGB(90,218,170):RGB(47,59,66); if(d->itemState&ODS_SELECTED) color=(d->CtlID==IDOK)?RGB(117,236,190):RGB(68,84,92);
 HBRUSH b=CreateSolidBrush(color); FillRect(d->hDC,&d->rcItem,b); DeleteObject(b);
 draw_text(d->hDC,text,d->rcItem,(d->CtlID==IDOK)?boldfont:font,(d->CtlID==IDOK)?RGB(19,24,29):RGB(228,237,240),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
 switch(msg) {
 case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); paint_install_window(h,dc); EndPaint(h,&ps); return 0; }
 case WM_ERASEBKGND: return 1;
 case WM_CREATE: {
  window=h;
  deskbox=CreateWindowW(L"BUTTON",L"Create a desktop shortcut",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX|BS_OWNERDRAW,36,452,275,26,h,(HMENU)102,NULL,NULL); SendMessageW(deskbox,WM_SETFONT,(WPARAM)font,TRUE); SendMessageW(deskbox,BM_SETCHECK,BST_CHECKED,0);
  launchbox=CreateWindowW(L"BUTTON",L"Open Portman after installation",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX|BS_OWNERDRAW,322,452,310,26,h,(HMENU)103,NULL,NULL); SendMessageW(launchbox,WM_SETFONT,(WPARAM)font,TRUE); SendMessageW(launchbox,BM_SETCHECK,BST_CHECKED,0);
  detailsbtn=CreateWindowW(L"BUTTON",L"Show installation details",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,650,451,150,27,h,(HMENU)104,NULL,NULL); SendMessageW(detailsbtn,WM_SETFONT,(WPARAM)smallfont,TRUE);
  state=CreateWindowW(L"STATIC",L"Ready to install Portman.",WS_CHILD|WS_VISIBLE|SS_PATHELLIPSIS,36,484,520,25,h,NULL,NULL,NULL); SendMessageW(state,WM_SETFONT,(WPARAM)smallfont,TRUE);
  progressbar=CreateWindowExW(0,PROGRESS_CLASSW,L"",WS_CHILD|WS_VISIBLE|PBS_SMOOTH,36,512,764,8,h,(HMENU)105,NULL, NULL); SendMessageW(progressbar,PBM_SETRANGE,0,MAKELPARAM(0,100)); SendMessageW(progressbar,PBM_SETBARCOLOR,0,RGB(90,218,170)); SendMessageW(progressbar,PBM_SETBKCOLOR,0,RGB(35,43,50));
  installbtn=CreateWindowW(L"BUTTON",L"Install Portman",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON|BS_OWNERDRAW,520,538,145,38,h,(HMENU)IDOK,NULL,NULL); SendMessageW(installbtn,WM_SETFONT,(WPARAM)boldfont,TRUE);
  cancelbtn=CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,676,538,124,38,h,(HMENU)IDCANCEL,NULL,NULL); SendMessageW(cancelbtn,WM_SETFONT,(WPARAM)font,TRUE); return 0;
  }
  case WM_COMMAND:
  if(LOWORD(wp)==IDCANCEL) DestroyWindow(h);
  if(LOWORD(wp)==104 && HIWORD(wp)==BN_CLICKED) { details_open=!details_open; if(details_open) { SetWindowTextW(detailsbtn,L"Hide installation details"); SetWindowTextW(state,L"Includes GUI, CLI, uninstaller, Start menu shortcut, and QUICKSTART guide."); } else { SetWindowTextW(detailsbtn,L"Show installation details"); SetWindowTextW(state,L"Ready to install Portman."); } InvalidateRect(h,NULL,FALSE); }
  if(LOWORD(wp)==IDOK) {
   EnableWindow(installbtn,FALSE); SetWindowTextW(state,L"Installing..."); UpdateWindow(h);
   if(install()) {
    if(SendMessageW(launchbox,BM_GETCHECK,0,0)==BST_CHECKED) { wchar_t app[2200]; paths(app,2200,L"Portman.exe"); if((INT_PTR)ShellExecuteW(h,L"open",app,NULL,install_dir,SW_SHOWNORMAL)<=32) notice(L"Installed. Windows could not launch the app automatically; open Portman from the Start menu."); }
    else MessageBoxW(h,L"Installed. Open Portman from the Start menu.",L"Portman",MB_OK|MB_ICONINFORMATION);
    DestroyWindow(h);
   } else { EnableWindow(installbtn,TRUE); SetWindowTextW(state,L"Installation did not finish. Resolve the error and retry."); }
  } return 0;
 case WM_CTLCOLORSTATIC: { HDC dc=(HDC)wp; SetTextColor(dc,RGB(165,181,193)); SetBkColor(dc,RGB(19,24,29)); return (LRESULT)background; }
 case WM_DRAWITEM: draw_button((DRAWITEMSTRUCT*)lp); return TRUE;
 case WM_DESTROY: PostQuitMessage(0); return 0;
 } return DefWindowProcW(h,msg,wp,lp);
}
int pmi_main(const unsigned char *g,size_t gs,const unsigned char *c,size_t cs,const unsigned char *guide,size_t guides) {
 gui_data=g; gui_size=gs; cli_data=c; cli_size=cs; guide_data=guide; guide_size=guides;
 CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 wchar_t local[MAX_PATH]; if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,local)) || FAILED(SHGetFolderPathW(NULL,CSIDL_PROGRAMS|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,programs)) || FAILED(SHGetFolderPathW(NULL,CSIDL_DESKTOPDIRECTORY,NULL,SHGFP_TYPE_CURRENT,desktop))) { notice(L"Cannot locate your Windows profile folders."); return 1; }
 swprintf(install_dir,2048,L"%s\\Programs\\Portman",local); GetModuleFileNameW(NULL,self,2048);
 int argc=0; wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
 if(argc>1 && !wcscmp(argv[1],L"--cleanup")) {
  if(argc!=3) return 1;
  DWORD pid=wcstoul(argv[2],NULL,10); HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);
  if(parent) { DWORD wait=WaitForSingleObject(parent,15000); CloseHandle(parent); if(wait!=WAIT_OBJECT_0) return 1; }
  int result=uninstall_cleanup(); LocalFree(argv); CoUninitialize(); return result;
 }
 const wchar_t *base=wcsrchr(self,L'\\'); base=base?base+1:self;
 if((argc>1 && !wcscmp(argv[1],L"--uninstall")) || (argc==1 && !_wcsicmp(base,L"Uninstall.exe"))) { LocalFree(argv); int result=uninstall(); CoUninitialize(); return result; }
 LocalFree(argv);
 installer_lock=CreateMutexW(NULL,FALSE,L"Local\\PortmanSetup02"); if(!installer_lock || GetLastError()==ERROR_ALREADY_EXISTS) { notice(L"Another Portman installer is already open."); return 1; }
 background=CreateSolidBrush(RGB(19,24,29)); panel=CreateSolidBrush(RGB(35,43,50)); accent_panel=CreateSolidBrush(RGB(28,38,43));
 font=CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 smallfont=CreateFontW(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 boldfont=CreateFontW(-15,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 titlefont=CreateFontW(-31,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 mono_font=CreateFontW(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
 WNDCLASSW cls={0}; cls.hInstance=GetModuleHandleW(NULL); cls.lpfnWndProc=proc; cls.lpszClassName=L"PortmanSetup02"; cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(1)); cls.hCursor=LoadCursorW(NULL,IDC_ARROW); cls.hbrBackground=background; RegisterClassW(&cls);
 window=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_APPWINDOW,cls.lpszClassName,L"Install Portman 0.2.2",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,860,660,NULL,NULL,cls.hInstance,NULL); if(!window) return 1;
 ShowWindow(window,SW_SHOW); MSG m;
 while(GetMessageW(&m,NULL,0,0)>0) { if(!IsDialogMessageW(window,&m)) { TranslateMessage(&m); DispatchMessageW(&m); } }
 DeleteObject(font); DeleteObject(smallfont); DeleteObject(boldfont); DeleteObject(titlefont); DeleteObject(mono_font); DeleteObject(background); DeleteObject(panel); DeleteObject(accent_panel); CloseHandle(installer_lock); CoUninitialize(); return 0;
}
