#include "faba_favorites.h"
#include "vfs.h"
#include <string.h>
#define PACKET_SIZE (14 + 2 * FABA_FAVORITES_MAX)
#define CHECKSUM_OFFSET (PACKET_SIZE - 4)
static const char *const paths[] = {"/faba/pref0.bin", "/faba/pref1.bin"};
static uint32_t get32(const uint8_t *p) { return p[0] | (uint32_t)p[1]<<8 | (uint32_t)p[2]<<16 | (uint32_t)p[3]<<24; }
static void put32(uint8_t *p,uint32_t v) { for(unsigned i=0;i<4;i++)p[i]=(uint8_t)(v>>(8*i)); }
static uint32_t checksum(const uint8_t *p) {
    uint32_t hash=2166136261u;
    for(unsigned i=0;i<CHECKSUM_OFFSET;i++)hash=(hash^p[i])*16777619u;
    return hash;
}
static int32_t read_bank(vfs_driver_t *fs,unsigned bank,uint8_t *bytes) {
    vfs_obj_t info;
    int32_t result=fs->stat_file(paths[bank],&info);
    if(result!=VFS_OK)return result;
    if(info.size!=PACKET_SIZE)return VFS_ERR_CRPT;
    vfs_file_t file;
    result=fs->open_file(paths[bank],&file,VFS_MODE_READONLY);
    if(result!=VFS_OK)return result;
    result=fs->read_file(&file,bytes,PACKET_SIZE);
    int32_t closed=fs->close_file(&file);
    if(result!=PACKET_SIZE||closed!=VFS_OK)return VFS_ERR_FAIL;
    unsigned count=bytes[8]|(unsigned)bytes[9]<<8;
    if(memcmp(bytes,"FBP1",4)||count>FABA_FAVORITES_MAX||get32(bytes+CHECKSUM_OFFSET)!=checksum(bytes))return VFS_ERR_CRPT;
    unsigned previous=0;
    for(unsigned i=0;i<count;i++) {
        unsigned id=bytes[10+2*i]|(unsigned)bytes[11+2*i]<<8;
        if(id<=previous||id>9999)return VFS_ERR_CRPT;
        previous=id;
    }
    return VFS_OK;
}
int32_t faba_favorites_load(faba_favorites_t *state) {
    memset(state,0,sizeof(*state));state->bank=-1;
    vfs_driver_t *fs=vfs_get_driver(VFS_DRIVE_EXT);
    if(!fs||!fs->mounted())return VFS_ERR_NODEV;
    uint8_t bytes[PACKET_SIZE];
    int32_t failure=VFS_OK;
    for(unsigned bank=0;bank<2;bank++) {
        int32_t result=read_bank(fs,bank,bytes);
        if(result==VFS_ERR_NOOBJ)continue;
        if(result!=VFS_OK){failure=result;continue;}
        uint32_t generation=get32(bytes+4);
        if(state->bank>=0&&(int32_t)(generation-state->generation)<=0)continue;
        state->bank=(int8_t)bank;state->generation=generation;
        state->count=bytes[8]|(unsigned)bytes[9]<<8;
        for(unsigned i=0;i<state->count;i++)state->ids[i]=bytes[10+2*i]|(unsigned)bytes[11+2*i]<<8;
    }
    state->available=state->bank>=0||failure==VFS_OK;
    return state->available?VFS_OK:failure;
}
bool faba_favorites_has(const faba_favorites_t *state,uint16_t id) {
    for(unsigned i=0;i<state->count;i++)if(state->ids[i]==id)return true;
    return false;
}
int32_t faba_favorites_toggle(faba_favorites_t *state,uint16_t id) {
    if(!state->available)return VFS_ERR_CRPT;
    if(!id||id>9999)return VFS_ERR_FAIL;
    faba_favorites_t next=*state;
    unsigned pos=0;
    while(pos<next.count&&next.ids[pos]<id)pos++;
    if(pos<next.count&&next.ids[pos]==id) {
        memmove(next.ids+pos,next.ids+pos+1,(next.count-pos-1)*sizeof(uint16_t));next.count--;
    } else {
        if(next.count==FABA_FAVORITES_MAX)return VFS_ERR_NOSPC;
        memmove(next.ids+pos+1,next.ids+pos,(next.count-pos)*sizeof(uint16_t));next.ids[pos]=id;next.count++;
    }
    next.generation++;next.bank=state->bank==0?1:0;
    uint8_t bytes[PACKET_SIZE]={0};memcpy(bytes,"FBP1",4);put32(bytes+4,next.generation);
    bytes[8]=next.count;bytes[9]=next.count>>8;
    for(unsigned i=0;i<next.count;i++){bytes[10+2*i]=next.ids[i];bytes[11+2*i]=next.ids[i]>>8;}
    put32(bytes+CHECKSUM_OFFSET,checksum(bytes));
    vfs_driver_t *fs=vfs_get_driver(VFS_DRIVE_EXT);
    if(!fs||!fs->mounted())return VFS_ERR_NODEV;
    /* Write the inactive copy; a partial write never destroys the active copy. */
    vfs_file_t file;
    int32_t result=fs->open_file(paths[next.bank],&file,VFS_MODE_CREATE|VFS_MODE_TRUNC|VFS_MODE_WRITEONLY);
    if(result!=VFS_OK)return result;
    result=fs->write_file(&file,bytes,sizeof(bytes));
    int32_t closed=fs->close_file(&file);
    if(result!=sizeof(bytes)||closed!=VFS_OK)return VFS_ERR_FAIL;
    uint8_t verify[PACKET_SIZE];
    result=read_bank(fs,next.bank,verify);
    if(result!=VFS_OK||memcmp(bytes,verify,sizeof(bytes)))return VFS_ERR_FAIL;
    *state=next;
    return VFS_OK;
}
