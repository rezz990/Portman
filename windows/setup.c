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
#include "upgrade_transaction.h"
#include "../src/ui/theme.h"
#include "../src/ui/maintenance.h"
static const wchar_t *keypath=L"Software\\Microsoft\\Windows\\CurrentVersion\\Uninstall\\Portman";
static wchar_t install_dir[2048], self[2048], programs[MAX_PATH], desktop[MAX_PATH];
static const unsigned char *gui_data,*cli_data,*guide_data;
static size_t gui_size,cli_size,guide_size;
static HWND window, state, launchbox, deskbox, installbtn, cancelbtn, detailsbtn, progressbar;
static int setup_dpi=96;
static HFONT controlfont,controlsmall;
static int spx(int v) {return MulDiv(v,setup_dpi,96);}
static HFONT font, smallfont, boldfont, titlefont, mono_font;
static HBRUSH background, panel, accent_panel;
#define SETUP_PROGRESS (WM_APP+10)
#define SETUP_NOTICE (WM_APP+11)
#define SETUP_DONE (WM_APP+12)
#define SETUP_RETRY (WM_APP+13)
static int installing=0, installed_ok=0, want_desktop=0, existing_install=0;
static wchar_t installed_version[80],setup_log_path[2200];
static DWORD ui_thread;
static HANDLE worker;

#ifdef PORTMAN_SETUP_TEST
static int (*test_retry_decision)(void);
static DWORD test_file_error;
#endif
static HANDLE installer_lock;
static void notice(const wchar_t *msg) { if(window && GetCurrentThreadId()!=ui_thread) SendMessageW(window,SETUP_NOTICE,0,(LPARAM)msg); else MessageBoxW(window,msg,L"Portman Setup",MB_OK|MB_ICONWARNING); }
static void paths(wchar_t *out,size_t cap,const wchar_t *name) { swprintf(out,cap,L"%s\\%s",install_dir,name); }
static void installer_progress(int value,const wchar_t *message) {
 if(window && GetCurrentThreadId()!=ui_thread) { PostMessageW(window,SETUP_PROGRESS,value,(LPARAM)message); return; }
 if(progressbar) SendMessageW(progressbar,PBM_SETPOS,value,0);
 if(state) SetWindowTextW(state,message);
 if(window) { UpdateWindow(window); UpdateWindow(progressbar); UpdateWindow(state); }
}
static void setup_log(const wchar_t *message) {
 if(!setup_log_path[0]) return;
 HANDLE f=CreateFileW(setup_log_path,FILE_APPEND_DATA,FILE_SHARE_READ|FILE_SHARE_WRITE,NULL,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(f==INVALID_HANDLE_VALUE) return;
 SYSTEMTIME t; GetLocalTime(&t); wchar_t line[9000]; char utf[36000];
 swprintf(line,9000,L"[%04u-%02u-%02u %02u:%02u:%02u] %s\r\n",t.wYear,t.wMonth,t.wDay,t.wHour,t.wMinute,t.wSecond,message);
 int n=WideCharToMultiByte(CP_UTF8,0,line,-1,utf,sizeof(utf),NULL,NULL); DWORD written;
 if(n>1) WriteFile(f,utf,(DWORD)n-1,&written,NULL);
 CloseHandle(f);
}
static int retry_message(const wchar_t *message) {
 setup_log(message);
#ifdef PORTMAN_SETUP_TEST
 return test_retry_decision?test_retry_decision():0;
#endif
 if(window && GetCurrentThreadId()!=ui_thread) return (int)SendMessageW(window,SETUP_RETRY,0,(LPARAM)message)==IDRETRY;
 return MessageBoxW(window,message,L"Portman Setup - action needed",MB_RETRYCANCEL|MB_ICONWARNING|MB_DEFBUTTON2)==IDRETRY;
}
static int file_retry(const wchar_t *operation,const wchar_t *source,const wchar_t *target,DWORD code) {
#ifdef PORTMAN_SETUP_TEST
 test_file_error=code;
#endif
 wchar_t system[1024]=L"Windows did not provide a description.",message[8000];
 FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,code,0,system,1024,NULL);
 const wchar_t *hint=(code==ERROR_SHARING_VIOLATION || code==ERROR_LOCK_VIOLATION)?L"Close the application using this file, including Portman in the tray or a Portman CLI session, then choose Retry.":code==ERROR_ACCESS_DENIED?L"Windows denied access. Check this file's permissions, read-only attribute and security-software history, then choose Retry.":L"Resolve the Windows error, then choose Retry. You do not need to uninstall first.";
 swprintf(message,8000,L"%s\n\nFile: %s\nDestination: %s\n\nWindows error %lu: %s\n%s\n\nCancel stops this attempt and rolls back replaced application files where possible. Project files, service configuration and logs are not replaced.",operation,source,target?target:L"(not applicable)",code,system,hint);
 return retry_message(message);
}
static int write_file(const wchar_t *path,const unsigned char *bytes,size_t length) {
 HANDLE h=CreateFileW(path,GENERIC_WRITE,0,NULL,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,NULL);
 if(h==INVALID_HANDLE_VALUE) return 0;
 DWORD done=0,error=ERROR_SUCCESS;
 if(length>0xffffffff) error=ERROR_FILE_TOO_LARGE;
 else if(!WriteFile(h,bytes,(DWORD)length,&done,NULL)) error=GetLastError();
 else if(done!=length) error=ERROR_WRITE_FAULT;
 else if(!FlushFileBuffers(h)) error=GetLastError();
 CloseHandle(h); SetLastError(error); return error==ERROR_SUCCESS;
}
static int move_retry(const wchar_t *operation,const wchar_t *source,const wchar_t *target,DWORD flags) {
 for(;;) {
  if(MoveFileExW(source,target,flags|MOVEFILE_WRITE_THROUGH)) return 1;
  DWORD error=GetLastError();
  if(!file_retry(operation,source,target,error)) return 0;
 }
}
static const wchar_t *component_names[]={L"Portman.exe",L"portman-cli.exe",L"Uninstall.exe",L"QUICKSTART.txt"};
typedef struct { wchar_t directory[2200]; int has_old[PM_UPGRADE_FILE_COUNT]; } upgrade_context;
static void component_paths(upgrade_context *ctx,int i,wchar_t *dest,wchar_t *staged,wchar_t *backup) {
 paths(dest,2400,component_names[i]);
 swprintf(staged,2400,L"%s\\%s.new",ctx->directory,component_names[i]);
 swprintf(backup,2400,L"%s\\%s.old",ctx->directory,component_names[i]);
}
static int stage_component(void *opaque,int i) {
 upgrade_context *ctx=opaque; wchar_t dest[2400],staged[2400],backup[2400]; component_paths(ctx,i,dest,staged,backup);
 for(;;) {
  int ok=i==0?write_file(staged,gui_data,gui_size):i==1?write_file(staged,cli_data,cli_size):i==2?CopyFileW(self,staged,FALSE):write_file(staged,guide_data,guide_size);
  if(ok) { installer_progress(15+(i+1)*10,L"Preparing new application files..."); return 1; }
  DWORD error=GetLastError(); if(!file_retry(L"Could not prepare this component",staged,NULL,error)) return 0;
 }
}
static int backup_component(void *opaque,int i) {
 upgrade_context *ctx=opaque; wchar_t dest[2400],staged[2400],backup[2400]; component_paths(ctx,i,dest,staged,backup);
 for(;;) {
  DWORD attr=GetFileAttributesW(dest);
  if(attr!=INVALID_FILE_ATTRIBUTES) {
   if(!move_retry(L"Could not back up the installed file",dest,backup,0)) return 0;
   ctx->has_old[i]=1; return 1;
  }
  DWORD error=GetLastError(); if(error==ERROR_FILE_NOT_FOUND) return 1;
  if(!file_retry(L"Could not inspect the installed file",dest,NULL,error)) return 0;
 }
}
static int publish_component(void *opaque,int i) {
 upgrade_context *ctx=opaque; wchar_t dest[2400],staged[2400],backup[2400]; component_paths(ctx,i,dest,staged,backup);
 installer_progress(58+i*6,L"Replacing application files...");
 return move_retry(L"Could not place the new application file",staged,dest,0);
}
static int restore_component(void *opaque,int i,int published) {
 upgrade_context *ctx=opaque; wchar_t dest[2400],staged[2400],backup[2400]; component_paths(ctx,i,dest,staged,backup);
 installer_progress(5,L"Restoring the previous application files...");
 if(ctx->has_old[i]) return move_retry(L"Could not restore the previous file. Its backup is kept at the source path below",backup,dest,MOVEFILE_REPLACE_EXISTING);
 if(published) for(;;) {
  if(DeleteFileW(dest)) return 1;
  DWORD error=GetLastError(); if(error==ERROR_FILE_NOT_FOUND) return 1;
  if(!file_retry(L"Could not remove a newly installed file during rollback",dest,NULL,error)) return 0;
 }
 return 1;
}
static void cleanup_component(void *opaque,int i) {
 upgrade_context *ctx=opaque; wchar_t dest[2400],staged[2400],backup[2400]; component_paths(ctx,i,dest,staged,backup);
 if(!DeleteFileW(staged) && GetLastError()!=ERROR_FILE_NOT_FOUND) setup_log(L"A staging file could not be removed; the transaction folder was retained.");
}
static int preflight_files(void) {
 for(int i=0;i<PM_UPGRADE_FILE_COUNT;i++) {
  wchar_t dest[2400]; paths(dest,2400,component_names[i]);
  for(;;) {
   HANDLE f=CreateFileW(dest,DELETE,FILE_SHARE_READ|FILE_SHARE_WRITE|FILE_SHARE_DELETE,NULL,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,NULL);
   if(f!=INVALID_HANDLE_VALUE) { CloseHandle(f); break; }
   DWORD error=GetLastError(); if(error==ERROR_FILE_NOT_FOUND) break;
   if(!file_retry(L"This installed file is not ready to be replaced",dest,NULL,error)) return 0;
  }
 }
 return 1;
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
static LONG registration_error;
static int reg_string(HKEY k,const wchar_t *name,const wchar_t *value) { registration_error=RegSetValueExW(k,name,0,REG_SZ,(const BYTE*)value,(DWORD)((wcslen(value)+1)*sizeof(wchar_t))); return registration_error==ERROR_SUCCESS; }
static int reg_dword(HKEY k,const wchar_t *name,DWORD value) { registration_error=RegSetValueExW(k,name,0,REG_DWORD,(const BYTE*)&value,sizeof(value)); return registration_error==ERROR_SUCCESS; }
static int retry_registration(LONG code) {
 wchar_t system[1024]=L"No Windows description available.",message[2400];
 FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,NULL,(DWORD)code,0,system,1024,NULL);
 swprintf(message,2400,L"The new application files are installed, but Windows registration did not finish.\n\nRegistry: HKEY_CURRENT_USER\\%s\nWindows error %ld: %s\n\nRetry attempts registration again. Cancel keeps the installed files and their previous backups; use Retry installation to finish later. See Setup log for the backup folder.",keypath,code,system);
 return retry_message(message);
}
static int active_app(void) {
 HANDLE h=OpenMutexW(SYNCHRONIZE,FALSE,L"Local\\PortmanDesktop02");
 if(h) { CloseHandle(h); return 1; } return 0;
}
static int install(void) {
 setup_log(L"Beginning installation attempt.");
 while(active_app()) {
  if(!retry_message(L"Portman is still open.\n\nExit Portman from its tray menu, stop any Portman CLI sessions, then choose Retry. Closing the panel to the tray does not exit it.\n\nNo application files have been changed. Cancel leaves this installation attempt.")) return 0;
 }
 installer_progress(6,L"Checking the existing installation...");
 for(;;) { int result=SHCreateDirectoryExW(NULL,install_dir,NULL); if(result==ERROR_SUCCESS || result==ERROR_ALREADY_EXISTS || result==ERROR_FILE_EXISTS) break; if(!file_retry(L"Cannot create the installation directory",install_dir,NULL,(DWORD)result)) return 0; }
 if(!preflight_files()) return 0;
 upgrade_context ctx={0};
 for(unsigned attempt=0;;attempt++) {
  swprintf(ctx.directory,2200,L"%s\\.portman-update-%lu-%llu-%u",install_dir,GetCurrentProcessId(),GetTickCount64(),attempt);
  if(CreateDirectoryW(ctx.directory,NULL)) break;
  DWORD error=GetLastError(); if(error==ERROR_ALREADY_EXISTS && attempt<20) continue;
  if(!file_retry(L"Could not create the update staging folder",ctx.directory,NULL,error)) return 0;
 }
 setup_log(ctx.directory);
 const pm_upgrade_ops ops={stage_component,backup_component,publish_component,restore_component,cleanup_component};
 pm_upgrade_result files=pm_upgrade_files(&ops,&ctx);
 if(!files.success) {
  if(files.rollback_failed) {
   wchar_t message[2700]; swprintf(message,2700,L"The update stopped and some files could not be restored.\n\nRetained backups: %s\n\nDo not delete this folder. Close apps using Portman files, then retry setup. See Setup log for the exact file and Windows error.",ctx.directory); setup_log(message); notice(message);
  } else { RemoveDirectoryW(ctx.directory); setup_log(L"Attempt cancelled or failed. Previous application files restored; no configuration or project files changed."); }
  return 0;
 }
 installer_progress(82,L"Registering the installed version with Windows...");
 wchar_t temp[2400],dest[2400],backup[2400];
 for(;;) {
  HKEY key; LONG code=RegCreateKeyExW(HKEY_CURRENT_USER,keypath,0,NULL,0,KEY_SET_VALUE,NULL,&key,NULL);
  if(code!=ERROR_SUCCESS) { if(retry_registration(code)) continue; return 0; }
  wchar_t uninstall[2400],icon[2400]; paths(dest,2400,L"Uninstall.exe"); swprintf(uninstall,2400,L"\"%s\" --uninstall",dest); paths(icon,2400,L"Portman.exe");
  int ok=reg_string(key,L"DisplayName",L"Portman") && reg_string(key,L"DisplayVersion",L"0.2.6") && reg_string(key,L"Publisher",L"rakarmp (rezz990)") && reg_string(key,L"InstallLocation",install_dir) && reg_string(key,L"DisplayIcon",icon) && reg_string(key,L"UninstallString",uninstall)
    && reg_dword(key,L"NoModify",1) && reg_dword(key,L"NoRepair",1) && reg_dword(key,L"EstimatedSize",(DWORD)((gui_size+cli_size+guide_size)*2/1024));
  RegCloseKey(key);
  if(ok) break;
  if(!retry_registration(registration_error)) return 0;
 }

 paths(dest,2400,L"Portman.exe"); swprintf(temp,2400,L"%s\\Portman.lnk",programs);
 installer_progress(90,L"Updating shortcuts...");
 if(!shortcut(temp,dest)) notice(L"Installed, but the Start menu shortcut could not be created. Open Portman.exe in the install folder.");
 if(want_desktop) { swprintf(temp,2400,L"%s\\Portman.lnk",desktop); if(!shortcut(temp,dest)) notice(L"The desktop shortcut could not be created."); }
 for(int i=0;i<PM_UPGRADE_FILE_COUNT;i++) {
  component_paths(&ctx,i,dest,temp,backup);
  if(ctx.has_old[i] && !DeleteFileW(backup)) setup_log(L"An old backup could not be removed. The transaction folder is retained.");
 }
 RemoveDirectoryW(ctx.directory);
 installer_progress(100,L"Portman is installed and ready."); setup_log(L"Installation finished successfully."); return 1;
}
static DWORD WINAPI install_worker(LPVOID unused) {
 (void)unused;
 HRESULT hr=CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 int ok=0;
 if(FAILED(hr)) notice(L"Could not initialize Windows integration. Close setup and retry.");
 else { HANDLE gate=pm_maintenance_enter(); if(!gate) notice(L"Portman is starting or another maintenance operation is active. Please retry."); else { ok=install(); pm_maintenance_leave(gate); } CoUninitialize(); }
 PostMessageW(window,SETUP_DONE,ok,0); return 0;
}
static void enable_options(int enabled) {
 EnableWindow(installbtn,enabled); EnableWindow(cancelbtn,enabled); EnableWindow(detailsbtn,enabled); EnableWindow(deskbox,enabled); EnableWindow(launchbox,enabled);
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
 RECT client; GetClientRect(h,&client); int w=MulDiv(client.right,96,setup_dpi), height=MulDiv(client.bottom,96,setup_dpi); client.right=w;client.bottom=height;
 HBRUSH b=CreateSolidBrush(RGB(23,27,32)); FillRect(dc,&client,b); DeleteObject(b);
 HBRUSH hero=CreateSolidBrush(RGB(28,38,43)); RECT hr={0,0,w,128}; FillRect(dc,&hr,hero); DeleteObject(hero);
 RECT line={0,124,w,128}; HBRUSH green=CreateSolidBrush(RGB(90,218,170)); FillRect(dc,&line,green); DeleteObject(green);
 RECT r={36,25,w-220,64}; draw_text(dc,L"PORTMAN",r,titlefont,RGB(238,245,250),DT_SINGLELINE);
 RECT s={38,67,w-220,91}; draw_text(dc,L"LOCAL DEVELOPMENT, UNDER CONTROL.",s,smallfont,RGB(157,181,190),DT_SINGLELINE);
 RECT by={w-208,32,w-35,58}; draw_text(dc,L"BUILT WITH \u2665",by,boldfont,RGB(90,218,170),DT_RIGHT|DT_SINGLELINE);
 RECT who={w-208,59,w-35,81}; draw_text(dc,L"by rakarmp (rezz990)",who,smallfont,RGB(188,204,211),DT_RIGHT|DT_SINGLELINE);
 RECT badge={w-130,91,w-35,113}; HBRUSH badgebrush=CreateSolidBrush(RGB(49,67,70)); FillRect(dc,&badge,badgebrush); DeleteObject(badgebrush); draw_text(dc,L"WINDOWS x64",badge,smallfont,RGB(210,240,232),DT_CENTER|DT_VCENTER|DT_SINGLELINE);
 RECT intro={36,150,w-36,181}; draw_text(dc,existing_install?L"Update your existing Portman installation":L"Install your local development control panel",intro,boldfont,RGB(238,245,250),DT_SINGLELINE);
 RECT copy={36,184,w-36,218}; wchar_t introduction[240]; if(existing_install) swprintf(introduction,240,L"Installed: %s  |  This package: Portman 0.2.6  |  Services and logs are kept.",installed_version); else wcscpy(introduction,L"A focused native workspace for your projects, ports, processes, and logs."); draw_text(dc,introduction,copy,font,RGB(165,181,193),DT_SINGLELINE);
 int cw=(w-96)/3; fill_card(dc,36,238,cw,92,L"01  ORGANIZE",L"One clean panel",L"Keep project services together"); fill_card(dc,48+cw,238,cw,92,L"02  OBSERVE",L"Know what runs",L"Ports, PIDs, uptime, and logs"); fill_card(dc,60+cw*2,238,cw,92,L"03  CONTROL",L"Stop with confidence",L"Managed process-tree cleanup");
 RECT info={36,350,w-36,374}; draw_text(dc,L"INSTALL LOCATION",info,smallfont,RGB(90,218,170),DT_SINGLELINE);
 RECT path={36,378,w-36,409}; HBRUSH p=CreateSolidBrush(RGB(35,43,50)); FillRect(dc,&path,p); DeleteObject(p); draw_text(dc,install_dir,path,mono_font,RGB(219,232,238),DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
 RECT note={36,420,w-36,444}; draw_text(dc,L"Per-user install • local-first • no runtime bundled • Node/PHP/Bun/Python stay yours to manage",note,smallfont,RGB(145,164,176),DT_SINGLELINE);
 RECT footer={36,height-31,w-36,height-8}; draw_text(dc,L"Portman 0.2.6  •  Built with \u2665 by rakarmp (rezz990)",footer,smallfont,RGB(123,144,154),DT_SINGLELINE);
}
static void draw_button(DRAWITEMSTRUCT *d) {
 FillRect(d->hDC,&d->rcItem,background);
 wchar_t text[160]; GetWindowTextW(d->hwndItem,text,160);
 COLORREF color=(d->CtlID==IDOK)?RGB(90,218,170):RGB(47,59,66); if(d->itemState&ODS_SELECTED) color=(d->CtlID==IDOK)?RGB(117,236,190):RGB(68,84,92);
 pm_round(d->hDC,d->rcItem,color,spx(8));
 draw_text(d->hDC,text,d->rcItem,controlfont,(d->CtlID==IDOK)?RGB(23,27,32):RGB(228,237,240),DT_CENTER|DT_VCENTER|DT_SINGLELINE|DT_NOPREFIX);
 if(d->itemState&ODS_FOCUS) { RECT f=d->rcItem; InflateRect(&f,-3,-3); DrawFocusRect(d->hDC,&f); }
}
static LRESULT CALLBACK proc(HWND h,UINT msg,WPARAM wp,LPARAM lp) {
 switch(msg) {
 case WM_PAINT: { PAINTSTRUCT ps; HDC dc=BeginPaint(h,&ps); int saved=SaveDC(dc); SetMapMode(dc,MM_ANISOTROPIC); SetWindowExtEx(dc,96,96,NULL); SetViewportExtEx(dc,setup_dpi,setup_dpi,NULL); paint_install_window(h,dc); RestoreDC(dc,saved); EndPaint(h,&ps); return 0; }
 case WM_ERASEBKGND: return 1;
 case WM_CREATE: {
  window=h;
  deskbox=CreateWindowW(L"BUTTON",L"Create a desktop shortcut",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,36,452,275,26,h,(HMENU)102,NULL,NULL); SendMessageW(deskbox,WM_SETFONT,(WPARAM)font,TRUE); SendMessageW(deskbox,BM_SETCHECK,BST_CHECKED,0);
  launchbox=CreateWindowW(L"BUTTON",L"Open Portman after installation",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_AUTOCHECKBOX,322,452,310,26,h,(HMENU)103,NULL,NULL); SendMessageW(launchbox,WM_SETFONT,(WPARAM)font,TRUE); SendMessageW(launchbox,BM_SETCHECK,BST_CHECKED,0);
  detailsbtn=CreateWindowW(L"BUTTON",L"Setup log",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,36,531,235,32,h,(HMENU)104,NULL,NULL); SendMessageW(detailsbtn,WM_SETFONT,(WPARAM)smallfont,TRUE);
  state=CreateWindowW(L"STATIC",L"Ready to install Portman.",WS_CHILD|WS_VISIBLE|SS_PATHELLIPSIS,36,484,764,25,h,NULL,NULL,NULL); SendMessageW(state,WM_SETFONT,(WPARAM)smallfont,TRUE);
  progressbar=CreateWindowExW(0,PROGRESS_CLASSW,L"",WS_CHILD|WS_VISIBLE|PBS_SMOOTH,36,512,764,8,h,(HMENU)105,NULL, NULL); SendMessageW(progressbar,PBM_SETRANGE,0,MAKELPARAM(0,100)); SendMessageW(progressbar,PBM_SETBARCOLOR,0,RGB(90,218,170)); SendMessageW(progressbar,PBM_SETBKCOLOR,0,RGB(35,43,50));
  installbtn=CreateWindowW(L"BUTTON",existing_install?L"Update / repair":L"Install Portman",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_DEFPUSHBUTTON|BS_OWNERDRAW,520,538,145,38,h,(HMENU)IDOK,NULL,NULL); SendMessageW(installbtn,WM_SETFONT,(WPARAM)boldfont,TRUE);
  cancelbtn=CreateWindowW(L"BUTTON",L"Cancel",WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,676,538,124,38,h,(HMENU)IDCANCEL,NULL,NULL); SendMessageW(cancelbtn,WM_SETFONT,(WPARAM)font,TRUE);
  for(HWND child=GetWindow(h,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
   RECT r;GetWindowRect(child,&r);MapWindowPoints(NULL,h,(POINT*)&r,2);MoveWindow(child,spx(r.left),spx(r.top),spx(r.right-r.left),spx(r.bottom-r.top),TRUE);
   SendMessageW(child,WM_SETFONT,(WPARAM)(child==state?controlsmall:controlfont),TRUE);
  }
  SetWindowSubclass(deskbox,pm_check,1,0); SetWindowSubclass(launchbox,pm_check,1,0);
  SetWindowTheme(progressbar,L"",L"");
  if(existing_install) SetWindowTextW(state,L"Ready to update or repair. Uninstalling first is not required.");
  return 0;
  }
  case WM_COMMAND:
  if(LOWORD(wp)==IDCANCEL && !installing) DestroyWindow(h);
  if(LOWORD(wp)==104 && HIWORD(wp)==BN_CLICKED) { if((INT_PTR)ShellExecuteW(h,L"open",setup_log_path,NULL,NULL,SW_SHOWNORMAL)<=32) notice(L"Could not open Setup log. It is stored in Local AppData / Portman / setup.log."); }
  if(LOWORD(wp)==IDOK && !installing) {
   if(installed_ok) {
    if(SendMessageW(launchbox,BM_GETCHECK,0,0)==BST_CHECKED) { wchar_t app[2200]; paths(app,2200,L"Portman.exe"); if((INT_PTR)ShellExecuteW(h,L"open",app,NULL,install_dir,SW_SHOWNORMAL)<=32) notice(L"Installed. Windows could not launch the app; open Portman from the Start menu."); }
    DestroyWindow(h); return 0;
   }
   want_desktop=SendMessageW(deskbox,BM_GETCHECK,0,0)==BST_CHECKED;
   installing=1; enable_options(FALSE); SetWindowTextW(state,L"Installing... Please keep setup open.");
   worker=CreateThread(NULL,0,install_worker,NULL,0,NULL);
   if(!worker) { installing=0; enable_options(TRUE); SetWindowTextW(state,L"Could not start installation. Please retry."); }
  } return 0;
 case SETUP_PROGRESS: installer_progress((int)wp,(const wchar_t*)lp); return 0;
 case SETUP_NOTICE: notice((const wchar_t*)lp); return 0;
 case SETUP_RETRY: return MessageBoxW(h,(const wchar_t*)lp,L"Portman Setup - action needed",MB_RETRYCANCEL|MB_ICONWARNING|MB_DEFBUTTON2);
 case SETUP_DONE:
  if(worker) { CloseHandle(worker); worker=NULL; }
  installing=0; installed_ok=(int)wp;
  if(installed_ok) {
   EnableWindow(installbtn,TRUE); EnableWindow(launchbox,TRUE);
   ShowWindow(cancelbtn,SW_HIDE); EnableWindow(detailsbtn,TRUE);
   SetWindowTextW(installbtn,L"Finish"); SetWindowTextW(launchbox,L"Open Portman when I click Finish");
   SetWindowTextW(state,L"Installed successfully. Click Finish to continue."); SetFocus(installbtn);
  } else { enable_options(TRUE); SetWindowTextW(installbtn,L"Retry installation"); SetWindowTextW(state,L"Installation did not finish. See Setup log, resolve the error, then retry."); }
  return 0;
 case WM_CLOSE:
  if(installing) { SetWindowTextW(state,L"Installation is in progress. Wait for it to finish before closing."); return 0; }
  DestroyWindow(h); return 0;
 case WM_CTLCOLORSTATIC: { HDC dc=(HDC)wp; SetTextColor(dc,RGB(165,181,193)); SetBkColor(dc,RGB(23,27,32)); return (LRESULT)background; }
 case WM_DRAWITEM: draw_button((DRAWITEMSTRUCT*)lp); return TRUE;
 case WM_DESTROY: PostQuitMessage(0); return 0;
 } return DefWindowProcW(h,msg,wp,lp);
}
int pmi_main(const unsigned char *g,size_t gs,const unsigned char *c,size_t cs,const unsigned char *guide,size_t guides) {
 ui_thread=GetCurrentThreadId();
 gui_data=g; gui_size=gs; cli_data=c; cli_size=cs; guide_data=guide; guide_size=guides;
 CoInitializeEx(NULL,COINIT_APARTMENTTHREADED);
 INITCOMMONCONTROLSEX ic={sizeof(ic),ICC_PROGRESS_CLASS|ICC_STANDARD_CLASSES}; InitCommonControlsEx(&ic);
 HDC screen=GetDC(NULL);setup_dpi=GetDeviceCaps(screen,LOGPIXELSX);ReleaseDC(NULL,screen);
 wchar_t local[MAX_PATH]; if(FAILED(SHGetFolderPathW(NULL,CSIDL_LOCAL_APPDATA,NULL,SHGFP_TYPE_CURRENT,local)) || FAILED(SHGetFolderPathW(NULL,CSIDL_PROGRAMS|CSIDL_FLAG_CREATE,NULL,SHGFP_TYPE_CURRENT,programs)) || FAILED(SHGetFolderPathW(NULL,CSIDL_DESKTOPDIRECTORY,NULL,SHGFP_TYPE_CURRENT,desktop))) { notice(L"Cannot locate your Windows profile folders."); return 1; }
 swprintf(install_dir,2048,L"%s\\Programs\\Portman",local); GetModuleFileNameW(NULL,self,2048);
 int argc=0; wchar_t **argv=CommandLineToArgvW(GetCommandLineW(),&argc);
 if(argc>1 && !wcscmp(argv[1],L"--cleanup")) {
  if(argc!=3) return 1;
  DWORD pid=wcstoul(argv[2],NULL,10); HANDLE parent=OpenProcess(SYNCHRONIZE,FALSE,pid);
  if(parent) { DWORD wait=WaitForSingleObject(parent,15000); CloseHandle(parent); if(wait!=WAIT_OBJECT_0) return 1; }
  HANDLE gate=pm_maintenance_enter();
  if(!gate) { notice(L"Another Portman operation is active. Please retry uninstalling."); LocalFree(argv); CoUninitialize(); return 1; }
  int result=uninstall_cleanup(); pm_maintenance_leave(gate); LocalFree(argv); CoUninitialize(); return result;
 }
 const wchar_t *base=wcsrchr(self,L'\\'); base=base?base+1:self;
 if((argc>1 && !wcscmp(argv[1],L"--uninstall")) || (argc==1 && !_wcsicmp(base,L"Uninstall.exe"))) { LocalFree(argv); int result=uninstall(); CoUninitialize(); return result; }
 LocalFree(argv);
 installer_lock=CreateMutexW(NULL,FALSE,L"Local\\PortmanSetup02"); if(!installer_lock || GetLastError()==ERROR_ALREADY_EXISTS) { notice(L"Another Portman installer is already open."); return 1; }
 wchar_t app[2200],data[2200]; paths(app,2200,L"Portman.exe"); existing_install=GetFileAttributesW(app)!=INVALID_FILE_ATTRIBUTES;
 wcscpy(installed_version,L"unknown version"); HKEY oldkey;
 if(RegOpenKeyExW(HKEY_CURRENT_USER,keypath,0,KEY_QUERY_VALUE,&oldkey)==ERROR_SUCCESS) {
  DWORD type=0,size=sizeof(installed_version); wchar_t value[80]={0};
  if(RegQueryValueExW(oldkey,L"DisplayVersion",NULL,&type,(BYTE*)value,&size)==ERROR_SUCCESS && type==REG_SZ) { value[79]=0; if(value[0]) wcscpy(installed_version,value); }
  RegCloseKey(oldkey);
 }
 swprintf(data,2200,L"%s\\Portman",local); SHCreateDirectoryExW(NULL,data,NULL);
 swprintf(setup_log_path,2200,L"%s\\setup.log",data);
 WIN32_FILE_ATTRIBUTE_DATA loginfo;
 if(GetFileAttributesExW(setup_log_path,GetFileExInfoStandard,&loginfo) && (loginfo.nFileSizeHigh || loginfo.nFileSizeLow>1024*1024)) {
  wchar_t previous[2400]; swprintf(previous,2400,L"%s.previous",setup_log_path); MoveFileExW(setup_log_path,previous,MOVEFILE_REPLACE_EXISTING);
 }
 setup_log(L"Portman 0.2.6 setup opened.");
 controlfont=CreateFontW(-spx(15),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 controlsmall=CreateFontW(-spx(12),0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 background=CreateSolidBrush(RGB(23,27,32)); panel=CreateSolidBrush(RGB(35,43,50)); accent_panel=CreateSolidBrush(RGB(28,38,43));
 font=CreateFontW(-15,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 smallfont=CreateFontW(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 boldfont=CreateFontW(-15,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 titlefont=CreateFontW(-31,0,0,0,FW_SEMIBOLD,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
 mono_font=CreateFontW(-12,0,0,0,FW_NORMAL,0,0,0,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Consolas");
 WNDCLASSW cls={0}; cls.hInstance=GetModuleHandleW(NULL); cls.lpfnWndProc=proc; cls.lpszClassName=L"PortmanSetup02"; cls.hIcon=LoadIconW(cls.hInstance,MAKEINTRESOURCEW(1)); cls.hCursor=LoadCursorW(NULL,IDC_ARROW); cls.hbrBackground=background; RegisterClassW(&cls);
 window=CreateWindowExW(WS_EX_CONTROLPARENT|WS_EX_APPWINDOW,cls.lpszClassName,L"Install Portman 0.2.6",WS_OVERLAPPED|WS_CAPTION|WS_SYSMENU|WS_MINIMIZEBOX,CW_USEDEFAULT,CW_USEDEFAULT,spx(860),spx(660),NULL,NULL,cls.hInstance,NULL); if(!window) return 1;
 pm_frame(window); ShowWindow(window,SW_SHOW); MSG m;
 while(GetMessageW(&m,NULL,0,0)>0) { if(!IsDialogMessageW(window,&m)) { TranslateMessage(&m); DispatchMessageW(&m); } }
 DeleteObject(controlfont); DeleteObject(controlsmall); DeleteObject(font); DeleteObject(smallfont); DeleteObject(boldfont); DeleteObject(titlefont); DeleteObject(mono_font); DeleteObject(background); DeleteObject(panel); DeleteObject(accent_panel); CloseHandle(installer_lock); CoUninitialize(); return 0;
}
