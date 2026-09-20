#ifndef PORTMAN_UPGRADE_TRANSACTION_H
#define PORTMAN_UPGRADE_TRANSACTION_H
/* File transaction policy, independent of Win32 so failure paths can be tested.
   Backup/publish must fail without partially completing their operation.
   Stage may leave a partial file, which cleanup_stage must remove.
   rollback receives whether the new file was published. It must retain any
   backup it could not restore. Cleanup may remove staging, never old backups. */
#define PM_UPGRADE_FILE_COUNT 4
typedef struct {
 int (*stage)(void *,int);
 int (*backup)(void *,int);
 int (*publish)(void *,int);
 int (*restore)(void *,int,int);
 void (*cleanup_stage)(void *,int);
} pm_upgrade_ops;
typedef struct { int success; unsigned int rollback_failed; } pm_upgrade_result;
static pm_upgrade_result pm_upgrade_files(const pm_upgrade_ops *ops,void *ctx) {
 pm_upgrade_result result={0,0};
 int backed[PM_UPGRADE_FILE_COUNT]={0},published[PM_UPGRADE_FILE_COUNT]={0};
 for(int i=0;i<PM_UPGRADE_FILE_COUNT;i++) if(!ops->stage(ctx,i)) goto rollback;
 for(int i=0;i<PM_UPGRADE_FILE_COUNT;i++) {
  if(!ops->backup(ctx,i)) goto rollback;
  backed[i]=1;
  if(!ops->publish(ctx,i)) goto rollback;
  published[i]=1;
 }
 result.success=1;
 goto cleanup;
rollback:
 for(int i=PM_UPGRADE_FILE_COUNT-1;i>=0;i--)
  if(backed[i] && !ops->restore(ctx,i,published[i])) result.rollback_failed|=1u<<i;
cleanup:
 for(int i=0;i<PM_UPGRADE_FILE_COUNT;i++) ops->cleanup_stage(ctx,i);
 return result;
}
#endif
