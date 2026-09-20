/* Failure injection against the transaction policy used by the real installer. */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../windows/upgrade_transaction.h"
typedef struct {
 int live[4],old[4],staged[4];
 int fail_phase,fail_index,restore_fail,mutations,cleaned,restored,last_restored;
} model;
static int stage(void *v,int i) {
 model *m=v; m->staged[i]=2; /* Also exercise cleanup of a partial staged write. */
 return !(m->fail_phase==0 && m->fail_index==i);
}
static int backup(void *v,int i) {
 model *m=v;
 for(int j=i;j<4;j++) assert(m->staged[j]==2); /* All files staged before mutation. */
 if(m->fail_phase==1 && m->fail_index==i) return 0;
 m->old[i]=m->live[i]; m->live[i]=0; m->mutations++; return 1;
}
static int publish(void *v,int i) {
 model *m=v; if(m->fail_phase==2 && m->fail_index==i) return 0;
 assert(!m->live[i] && m->staged[i]==2);
 m->live[i]=m->staged[i]; m->staged[i]=0; return 1;
}
static int restore(void *v,int i,int published) {
 model *m=v; assert(i<m->last_restored); m->last_restored=i; m->restored++;
 if(i==m->restore_fail) return 0;
 if(m->old[i]) { m->live[i]=m->old[i]; m->old[i]=0; }
 else if(published) m->live[i]=0;
 return 1;
}
static void cleanup(void *v,int i) { model *m=v; m->staged[i]=0; m->cleaned++; }
int main(void) {
 const pm_upgrade_ops ops={stage,backup,publish,restore,cleanup};
 int cases=0;
 for(int mask=0;mask<16;mask++) {
  for(int phase=-1;phase<3;phase++) for(int index=0;index<4;index++) {
   model m={0}; m.fail_phase=phase; m.fail_index=index; m.restore_fail=-1; m.last_restored=4;
   for(int i=0;i<4;i++) m.live[i]=(mask>>i)&1;
   pm_upgrade_result r=pm_upgrade_files(&ops,&m); cases++;
   assert(r.success==(phase==-1) && !r.rollback_failed && m.cleaned==4);
   for(int i=0;i<4;i++) {
    assert(m.live[i]==(r.success?2:((mask>>i)&1)));
    assert(!m.staged[i]);
    if(!r.success) assert(!m.old[i]);
   }
   if(phase==0) assert(m.mutations==0 && m.restored==0);
  }
 }
 for(int broken=0;broken<4;broken++) {
  model m={0}; m.fail_phase=2; m.fail_index=3; m.restore_fail=broken; m.last_restored=4;
  for(int i=0;i<4;i++) m.live[i]=1;
  pm_upgrade_result r=pm_upgrade_files(&ops,&m); cases++;
  assert(!r.success && r.rollback_failed==(1u<<broken));
  assert(m.old[broken]==1); /* Failed recovery must retain the only original copy. */
  assert(m.restored==4); /* Continue recovery even when one file remains locked. */
  for(int i=0;i<4;i++) if(i!=broken) assert(m.live[i]==1 && !m.old[i]);
 }
 printf("%d transaction scenarios passed (fresh/partial/full installs, failures and rollback).\n",cases);
 return 0;
}
