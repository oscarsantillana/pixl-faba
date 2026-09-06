#!/usr/bin/env python3
"""Exercise the firmware's actual bounded reader with generated and damaged files."""
import importlib.util, json, subprocess, tempfile
from pathlib import Path
root=Path(__file__).resolve().parents[1]
mod=root/'firmware/pixl-faba/fw/application/src/mod'
src=(mod.parent/'app/chameleon/scene/chameleon_scene_faba_catalog.c').read_text()
start=src.index('static bool load_page('); end=src.index('\nvoid chameleon_scene_faba_titles_on_enter',start)
fn=src[start:end]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "vfs.h"
#include "faba_catalog_data.h"
#define PAGE_SIZE 10
#define ICON_FILE 0
#define ICON_FAVORITE 1
#define FAVORITES_GROUP FABA_CATALOG_GROUP_COUNT
#include "faba_favorites.h"
bool faba_favorites_has(const faba_favorites_t *s,uint16_t id){(void)s;(void)id;return false;}
struct {unsigned group,page,total; faba_favorites_t favorites;} state, *catalog=&state;
typedef struct {void *p_list_view;} app_chameleon_t;
static unsigned added, closed, fail_read;
static uint8_t bytes[30000]; static size_t length,pos;
static bool mounted(void){return true;}
static int32_t stat_file(const char *p,vfs_obj_t *o){(void)p;o->size=length;return 0;}
static int32_t open_file(const char *p,vfs_file_t *f,uint32_t m){(void)p;(void)f;(void)m;pos=0;return 0;}
static int32_t close_file(vfs_file_t *f){(void)f;closed++;return 0;}
static int32_t read_file(vfs_file_t *f,void *p,size_t n){(void)f;if(fail_read||pos+n>length)return -1;memcpy(p,bytes+pos,n);pos+=n;return n;}
static vfs_driver_t driver={.mounted=mounted,.stat_file=stat_file,.open_file=open_file,.close_file=close_file,.read_file=read_file};
vfs_driver_t *vfs_get_driver(vfs_drive_t d){(void)d;return &driver;}
static void mui_list_view_add_item_ext(void *v,unsigned icon,const char *title,const char *sub,void *data){
 (void)v;(void)icon;(void)sub;assert(added<10);assert(strlen(title)<158);added++;
 printf("%04u\t%s\n",(unsigned)(uintptr_t)data,title);
}
'''
main=r'''
int main(int argc,char **argv){
 assert(argc==3);state.group=atoi(argv[2]);FILE *f=fopen(argv[1],"rb");assert(f);length=fread(bytes,1,sizeof(bytes),f);fclose(f);
 app_chameleon_t app={0};
 for(state.page=0;state.page*10<faba_groups[state.group].count;state.page++) {added=0;assert(load_page(&app));assert(added>0&&added<=10);}
 state.page=0;added=0;unsigned before=closed;
 bytes[0]^=1;assert(!load_page(&app));assert(closed==before+1);bytes[0]^=1;
 length--;assert(!load_page(&app));length++;
 fail_read=1;assert(!load_page(&app));fail_read=0;
 uint8_t a=bytes[16],b=bytes[17];bytes[16]=0;bytes[17]=0;assert(!load_page(&app));bytes[16]=a;bytes[17]=b;
 memset(bytes+18,'x',158);assert(!load_page(&app));
 return 0;
}
'''
with tempfile.TemporaryDirectory() as temp:
    tmp=Path(temp); (tmp/'reader.c').write_text(prefix+fn+main)
    subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',f'-I{mod}',f'-I{mod}/vfs',f'-I{root}/tools/faba-test-support',str(tmp/'reader.c'),'-o',str(tmp/'reader')],check=True)
    manifest=json.loads((root/'out/faba-catalog/manifest.json').read_text())
    entries=json.loads((root/'catalog/entries.json').read_text())
    observed=[]
    for group,file in enumerate(manifest['files']):
        output=subprocess.check_output([str(tmp/'reader'),str(root/'out/faba-catalog'/file['file']),str(group)],text=True)
        rows=[line.split('\t',1) for line in output.splitlines()]
        expected=[[e['id'],e['title']] for e in entries if e['language']==file['language']]
        assert rows==sorted(expected), (file['language'],rows)
        observed+=rows
    assert len(observed)==257 and len({r[0] for r in observed})==257
print('PASS: all 257 titles and IDs, 7 language groups, page boundaries, corrupt headers, truncated files, failed reads, invalid IDs, unterminated titles')
