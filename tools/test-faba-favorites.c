#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "faba_favorites.c"
static uint8_t disks[2][PACKET_SIZE];
static int lengths[2]={-1,-1}, limit=PACKET_SIZE;
static bool mounted_ok=true;
static unsigned bank_of(const char*p){return strstr(p,"pref1")?1:0;}
static bool mounted(void){return mounted_ok;}
static int32_t stat_file(const char*p,vfs_obj_t*o){unsigned b=bank_of(p);if(lengths[b]<0)return VFS_ERR_NOOBJ;o->size=lengths[b];return VFS_OK;}
static int32_t open_file(const char*p,vfs_file_t*f,uint32_t mode){unsigned b=bank_of(p);f->handle=(void*)(uintptr_t)(b+1);if(mode&VFS_MODE_TRUNC)lengths[b]=0;return VFS_OK;}
static int32_t close_file(vfs_file_t*f){(void)f;return VFS_OK;}
static int32_t read_file(vfs_file_t*f,void*p,size_t n){unsigned b=(uintptr_t)f->handle-1;if(lengths[b]!=(int)n)return VFS_ERR_FAIL;memcpy(p,disks[b],n);return n;}
static int32_t write_file(vfs_file_t*f,void*p,size_t n){unsigned b=(uintptr_t)f->handle-1;if(n>(unsigned)limit)n=limit;memcpy(disks[b],p,n);lengths[b]=n;return n;}
static vfs_driver_t fs={.mounted=mounted,.stat_file=stat_file,.open_file=open_file,.close_file=close_file,.read_file=read_file,.write_file=write_file};
vfs_driver_t *vfs_get_driver(vfs_drive_t d){(void)d;return &fs;}
int main(void){
 faba_favorites_t s,reopened;assert(faba_favorites_load(&s)==0&&s.available&&s.count==0);
 assert(faba_favorites_toggle(&s,400)==0);assert(faba_favorites_toggle(&s,443)==0);
 assert(faba_favorites_load(&reopened)==0&&reopened.count==2&&faba_favorites_has(&reopened,400));
 assert(faba_favorites_toggle(&s,400)==0);assert(!faba_favorites_has(&s,400)&&s.count==1);
 faba_favorites_t saved=s;
 for(limit=0;limit<PACKET_SIZE;limit++){
  assert(faba_favorites_toggle(&s,400)!=0);assert(!memcmp(&s,&saved,sizeof(s)));
  assert(faba_favorites_load(&reopened)==0&&reopened.count==1&&reopened.ids[0]==443);
 }
 limit=PACKET_SIZE;assert(faba_favorites_toggle(&s,443)==0&&s.count==0);
 assert(faba_favorites_load(&reopened)==0&&reopened.count==0);
 for(unsigned id=1;id<=FABA_FAVORITES_MAX;id++)assert(faba_favorites_toggle(&s,id)==0);
 saved=s;assert(faba_favorites_toggle(&s,9999)==VFS_ERR_NOSPC&&!memcmp(&s,&saved,sizeof(s)));
 assert(faba_favorites_toggle(&s,128)==0);assert(faba_favorites_toggle(&s,9999)==0);
 assert(faba_favorites_load(&reopened)==0&&reopened.count==FABA_FAVORITES_MAX&&!faba_favorites_has(&reopened,128));
 /* Newest torn/corrupt copy falls back to the complete previous copy. */
 unsigned active=s.bank;disks[active][12]^=1;
 assert(faba_favorites_load(&reopened)==0&&reopened.bank!=(int)active);
 disks[active][12]^=1;
 for(unsigned b=0;b<2;b++){put32(disks[b]+4,b==active?UINT32_MAX:UINT32_MAX-1);put32(disks[b]+CHECKSUM_OFFSET,checksum(disks[b]));}
 assert(faba_favorites_load(&s)==0&&s.generation==UINT32_MAX);
 assert(faba_favorites_toggle(&s,1)==0&&s.generation==0);
 assert(faba_favorites_load(&reopened)==0&&reopened.generation==0&&!faba_favorites_has(&reopened,1));
 disks[0][0]^=1;disks[1][0]^=1;
 assert(faba_favorites_load(&s)!=0&&!s.available);assert(faba_favorites_toggle(&s,400)!=0);
 mounted_ok=false;assert(faba_favorites_load(&s)==VFS_ERR_NODEV);
 puts("PASS: add/remove, all257favorites, reopen, empty, capacity, every partial-write length, checksum fallback, sequence wrap and unavailable storage");
}
