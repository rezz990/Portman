/* Runs the production Win32 file adapter in a temporary folder, without UI,
   registry writes, shortcuts, or touching an installed copy of Portman. */
#define PORTMAN_SETUP_TEST
#include "../windows/setup.c"
#include <assert.h>
#include <string.h>
static HANDLE locked_file;
static int release_lock_and_retry(void) {
 assert(locked_file!=INVALID_HANDLE_VALUE);
 CloseHandle(locked_file); locked_file=INVALID_HANDLE_VALUE;
 test_retry_decision=NULL; return 1;
}
static void expect_bytes(const wchar_t *file,const char *expected) {
 char bytes[80]={0}; DWORD size=0;
 HANDLE f=CreateFileW(file,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
 assert(f!=INVALID_HANDLE_VALUE && ReadFile(f,bytes,sizeof(bytes)-1,&size,NULL));
 CloseHandle(f); assert(size==strlen(expected) && !memcmp(bytes,expected,size));
}
int main(void) {
 wchar_t temp[MAX_PATH],root[MAX_PATH]; assert(GetTempPathW(MAX_PATH,temp));
 assert(GetTempFileNameW(temp,L"pmu",0,root)); assert(DeleteFileW(root)); assert(CreateDirectoryW(root,NULL));
 wcscpy(install_dir,root); swprintf(self,2048,L"%s\\source-setup.exe",root);
 assert(write_file(self,(const unsigned char*)"new-uninstall",13));
 gui_data=(const unsigned char*)"new-gui"; gui_size=7;
 cli_data=(const unsigned char*)"new-cli"; cli_size=7;
 guide_data=(const unsigned char*)"new-guide"; guide_size=9;
 const pm_upgrade_ops ops={stage_component,backup_component,publish_component,restore_component,cleanup_component};
 const char *expected[]={"new-gui","new-cli","new-uninstall","new-guide"};
 /* Cancel a real sharing violation at each component; earlier files must recover. */
 for(int scenario=0;scenario<6;scenario++) {
  upgrade_context ctx={0}; wchar_t dest[2400],staged[2400],backup[2400];
  swprintf(ctx.directory,2200,L"%s\\transaction-%d",root,scenario); assert(CreateDirectoryW(ctx.directory,NULL));
  for(int i=0;i<4;i++) { paths(dest,2400,component_names[i]); assert(write_file(dest,(const unsigned char*)"previous",8)); }
  int index=scenario<4?scenario:1; paths(dest,2400,component_names[index]);
  locked_file=CreateFileW(dest,GENERIC_READ,FILE_SHARE_READ,NULL,OPEN_EXISTING,0,NULL);
  assert(locked_file!=INVALID_HANDLE_VALUE);
  test_file_error=0; test_retry_decision=scenario==4?release_lock_and_retry:NULL;
  if(scenario==5) { assert(!preflight_files()); assert(test_file_error==ERROR_SHARING_VIOLATION); }
  else {
   pm_upgrade_result result=pm_upgrade_files(&ops,&ctx);
   assert(result.success==(scenario==4) && !result.rollback_failed);
   assert(test_file_error==ERROR_SHARING_VIOLATION);
  }
  if(locked_file!=INVALID_HANDLE_VALUE) CloseHandle(locked_file);
  for(int i=0;i<4;i++) {
   component_paths(&ctx,i,dest,staged,backup);
   expect_bytes(dest,scenario==4?expected[i]:"previous");
   assert(DeleteFileW(dest)); DeleteFileW(staged); DeleteFileW(backup);
  }
  assert(RemoveDirectoryW(ctx.directory));
 }
 assert(DeleteFileW(self)); assert(RemoveDirectoryW(root));
 puts("6 native Windows file-lock / retry / rollback scenarios passed."); return 0;
}
