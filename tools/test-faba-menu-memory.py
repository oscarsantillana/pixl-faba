#!/usr/bin/env python3
"""Run real catalog scenes, scene stack and list allocation through repeated plays."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];src=root/'firmware/pixl-faba/fw/application/src';mod=src/'mod'
def strip(s):return re.sub(r'^#include.*\n','',s,flags=re.M)
def function(source,name):
 m=re.search(r'^(?:static )?(?:void|bool|uint32_t|uint16_t) '+name+r'\(',source,re.M);assert m,name
 a=m.start();i=source.index('{',m.end())+1;n=1
 while n:n+=(source[i]=='{')-(source[i]=='}');i+=1
 return source[a:i]+'\n'
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static size_t live,allocations;
void *mui_mem_malloc(size_t n){size_t *p=malloc(n+sizeof(size_t));assert(p);*p=n;live+=n;allocations++;return p+1;}
void mui_mem_free(void *p){if(p){size_t *b=(size_t*)p-1;live-=*b;allocations--;free(b);}}
void *mui_mem_realloc(void *p,size_t n){if(!p)return mui_mem_malloc(n);size_t *b=(size_t*)p-1;live-=*b;b=realloc(b,n+sizeof(size_t));assert(b);*b=n;live+=n;return b+1;}
#include "mui_mem.h"
#define LIST_ITEM_HEIGHT 13
void mui_mem_monitor(mui_mem_monitor_t *m){m->free_size=m->free_biggest_size=18000-live;}
#include "mlib_common.h"
#include "vfs.h"
#include "faba_catalog_data.h"
#include "faba_favorites.h"
static faba_favorites_t saved_favorites;
int32_t faba_favorites_load(faba_favorites_t*s){*s=saved_favorites;s->available=true;return 0;}
bool faba_favorites_has(const faba_favorites_t*s,uint16_t id){for(unsigned i=0;i<s->count;i++)if(s->ids[i]==id)return true;return false;}
int32_t faba_favorites_toggle(faba_favorites_t*s,uint16_t id){
 for(unsigned i=0;i<s->count;i++)if(s->ids[i]==id){memmove(s->ids+i,s->ids+i+1,(s->count-i-1)*2);s->count--;saved_favorites=*s;return 0;}
 s->ids[s->count++]=id;saved_favorites=*s;return 0;
}

'''
listheader=(src/'mui/view/mui_list_view.h').read_text()
a=listheader.index('struct mui_list_item_s;');b=listheader.index('mui_list_view_t *mui_list_view_create();')
# The actual list and scene types need only placeholders for hardware view/animations.
listtypes='typedef struct {unsigned placeholder;} mui_view_t; typedef struct {unsigned placeholder;} mui_anim_t;\n'+listheader[a:b]
sceneheader=(src/'mui/mui_scene_dispatcher.h').read_text();scenetypes=sceneheader[sceneheader.index('typedef void'):sceneheader.index('void inline')]
listcode=(src/'mui/view/mui_list_view.c').read_text()
names=['mui_list_view_add_item','mui_list_view_add_item_ext','mui_list_view_item_size','mui_list_view_clear_items','mui_list_view_set_focus','mui_list_view_get_focus','mui_list_view_set_selected_cb','mui_list_view_free']
anim_stubs='static void *active_anims[3];\nvoid mui_anim_stop(mui_anim_t *a){for(unsigned i=0;i<3;i++)if(active_anims[i]==a)active_anims[i]=NULL;}\n'
protos='\n'.join(function(listcode,n).split('{',1)[0]+';' for n in names)
listfuncs='\n'.join(function(listcode,n) for n in names)
scene=strip((src/'mui/mui_scene_dispatcher.c').read_text())
catalog=strip((src/'app/chameleon/scene/chameleon_scene_faba_catalog.c').read_text())
boundary=r'''
#define ICON_FOLDER 1
#define ICON_SETTINGS 2
#define ICON_HOME 3
#define ICON_FILE 4
#define ICON_BACK 5
#define ICON_VIEW 6
#define ICON_FAVORITE 7
#define CHAMELEON_VIEW_ID_LIST 0
#define CHAMELEON_SCENE_FABA_CATALOG 0
#define CHAMELEON_SCENE_FABA_TITLES 1
#define CHAMELEON_SCENE_FABA_PLAY 2
#define CHAMELEON_SCENE_FABA 3
typedef struct {mui_list_view_t *p_list_view;mui_scene_dispatcher_t *p_scene_dispatcher;void *p_toast_view,*p_view_dispatcher;} app_chameleon_t;
static unsigned errors,starts;
static void *player;
bool faba_player_start(uint16_t id){assert(id<10000&&!player);player=mui_mem_malloc(497);starts++;return true;}
void faba_player_stop(void){mui_mem_free(player);player=NULL;}
void tag_emulation_sense_end(void){}
void hal_nfc_set_nrfx_irq_enable(bool b){(void)b;}
void *mini_app_launcher(void){return NULL;}
void mini_app_launcher_exit(void *p){(void)p;assert(false);}
void mui_toast_view_show(void *p,const char *s){(void)p;if(strstr(s,"preferidos")&&(strstr(s,"Añadido")||strstr(s,"Quitado")))return;fprintf(stderr,"TOAST: %s\n",s);errors++;}
void mui_view_dispatcher_switch_to_view(void *p,unsigned v){(void)p;(void)v;}
static char asset_root[1024];
static bool mounted(void){return true;}
static void path_name(char *out,const char *path){snprintf(out,1200,"%s/%s",asset_root,strrchr(path,'/')+1);}
static int32_t stat_file(const char *p,vfs_obj_t *o){char path[1200];path_name(path,p);FILE *f=fopen(path,"rb");if(!f)return -90;fseek(f,0,SEEK_END);o->size=ftell(f);fclose(f);return 0;}
static int32_t open_file(const char *p,vfs_file_t *f,uint32_t m){(void)m;char path[1200];path_name(path,p);f->handle=fopen(path,"rb");return f->handle?0:-90;}
static int32_t close_file(vfs_file_t *f){return fclose(f->handle);}
static int32_t read_file(vfs_file_t *f,void *p,size_t n){return fread(p,1,n,f->handle);}
static vfs_driver_t fs={.mounted=mounted,.stat_file=stat_file,.open_file=open_file,.close_file=close_file,.read_file=read_file};
vfs_driver_t *vfs_get_driver(vfs_drive_t d){(void)d;return &fs;}
'''
main=r'''
static void select_action(mui_list_view_t *v,uintptr_t action){
 for(unsigned i=0;i<mui_list_view_item_size(v);i++){
  mui_list_item_t *item=mui_list_item_array_get(v->items,i);
  if((uintptr_t)item->user_data==action){mui_list_view_set_focus(v,i);v->selected_cb(MUI_LIST_VIEW_EVENT_SELECTED,v,item);return;}
 }
 assert(false);
}
static void long_action(mui_list_view_t*v,uintptr_t action){
 for(unsigned i=0;i<mui_list_view_item_size(v);i++){
  mui_list_item_t*item=mui_list_item_array_get(v->items,i);
  if((uintptr_t)item->user_data==action){mui_list_view_set_focus(v,i);v->selected_cb(MUI_LIST_VIEW_EVENT_LONG_SELECTED,v,item);return;}
 }
 assert(false);
}
int main(int argc,char **argv){assert(argc==2);snprintf(asset_root,sizeof(asset_root),"%s",argv[1]);
 mui_list_view_t list={0};mui_list_item_array_init(list.items);
 mui_scene_dispatcher_t *scenes=mui_scene_dispatcher_create();
 const mui_scene_t defines[]={
 {0,chameleon_scene_faba_catalog_on_enter,chameleon_scene_faba_catalog_on_exit},
 {1,chameleon_scene_faba_titles_on_enter,chameleon_scene_faba_titles_on_exit},
 {2,chameleon_scene_faba_play_on_enter,chameleon_scene_faba_play_on_exit}};
 app_chameleon_t app={.p_list_view=&list,.p_scene_dispatcher=scenes};list.user_data=&app;
 mui_scene_dispatcher_set_user_data(scenes,&app);mui_scene_dispatcher_set_scene_defines(scenes,defines,3);
 mui_scene_dispatcher_next_scene(scenes,0);
 assert(!strcmp(string_get_cstr(mui_list_item_array_get(list.items,0)->text),"Preferidos"));
 select_action(&list,0);unsigned spanish=(uintptr_t)mui_list_item_array_get(list.items,0)->user_data;
 long_action(&list,spanish);assert(!player&&catalog->favorites.count==1);
 assert(mui_list_item_array_get(list.items,0)->icon==ICON_FAVORITE);
 select_action(&list,ACTION_BACK);select_action(&list,2);
 unsigned french=(uintptr_t)mui_list_item_array_get(list.items,0)->user_data;long_action(&list,french);
 select_action(&list,ACTION_BACK);select_action(&list,FAVORITES_GROUP);
 assert(catalog->total==2);select_action(&list,spanish);assert(player);select_action(&list,ACTION_BACK);
 long_action(&list,spanish);assert(catalog->total==1);long_action(&list,french);assert(catalog->total==0&&!catalog->favorites.count);
 assert(!strcmp(string_get_cstr(mui_list_item_array_get(list.items,0)->text),"Sin preferidos"));
 select_action(&list,ACTION_BACK);
 select_action(&list,0);unsigned favorites_ids[12];
 for(unsigned i=0;i<10;i++)favorites_ids[i]=(uintptr_t)mui_list_item_array_get(list.items,i)->user_data;
 for(unsigned i=0;i<10;i++)long_action(&list,favorites_ids[i]);
 select_action(&list,ACTION_BACK);select_action(&list,2);
 favorites_ids[10]=(uintptr_t)mui_list_item_array_get(list.items,0)->user_data;
 favorites_ids[11]=(uintptr_t)mui_list_item_array_get(list.items,1)->user_data;
 long_action(&list,favorites_ids[10]);long_action(&list,favorites_ids[11]);
 select_action(&list,ACTION_BACK);select_action(&list,FAVORITES_GROUP);
 assert(catalog->total==12);select_action(&list,ACTION_NEXT);assert(catalog->page==1);
 long_action(&list,favorites_ids[10]);assert(catalog->page==1&&catalog->total==11);
 long_action(&list,favorites_ids[11]);assert(catalog->page==0&&catalog->total==10);
 select_action(&list,ACTION_BACK);
 chameleon_scene_faba_catalog_on_exit(&app);faba_catalog_close();chameleon_scene_faba_catalog_on_enter(&app);
 assert(catalog->favorites.count==10);select_action(&list,FAVORITES_GROUP);assert(catalog->total==10);
 for(unsigned i=0;i<10;i++)long_action(&list,favorites_ids[i]);
 assert(catalog->total==0);select_action(&list,ACTION_BACK);
 unsigned baseline=0;
 for(unsigned cycle=0;cycle<100;cycle++){
  for(unsigned group=0;group<7;group++){
   select_action(&list,group);
   for(unsigned page=0;page*10<faba_groups[group].count;page++){
    unsigned id=(uintptr_t)mui_list_item_array_get(list.items,0)->user_data;
    for(unsigned play=0;play<3;play++){
     size_t before=live;
     select_action(&list,id);assert(player);select_action(&list,ACTION_BACK);assert(!player);
     if(play>0)assert(live==before);
    }
    if((page+1)*10<faba_groups[group].count)select_action(&list,ACTION_NEXT);
   }
   select_action(&list,ACTION_BACK);
  }
  printf("Cycle %u: live=%zu allocations=%zu starts=%u\n",cycle,live,allocations,starts);
  if(cycle==0)baseline=live;else assert(live==baseline);
 }
 mui_scene_dispatcher_exit(scenes);mui_scene_dispatcher_free(scenes);faba_catalog_close();mui_list_item_array_clear(list.items);
 assert(live==0&&allocations==0&&errors==0);
 mui_list_view_t *last=mui_mem_malloc(sizeof(*last));memset(last,0,sizeof(*last));
 last->p_view=mui_mem_malloc(sizeof(mui_view_t));mui_list_item_array_init(last->items);
 mui_list_view_add_item_ext(last,0,"A sufficiently long title that owns storage", "A long detail string that also owns storage",NULL);
 active_anims[0]=&last->anim;active_anims[1]=&last->text_anim;active_anims[2]=&last->gap_anim;
 mui_list_view_free(last);
 fprintf(stderr,"List destruction: live=%zu, remaining animation=%d\n",live,active_anims[2]!=NULL);
 assert(live==0&&allocations==0);assert(!active_anims[0]&&!active_anims[1]&&!active_anims[2]);puts("PASS: repeated real menu, list and scene allocations return to baseline");
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'menu.c').write_text(prefix+listtypes+scenetypes+anim_stubs+protos+listfuncs+scene+boundary+catalog+main)
 subprocess.run(['cc','-std=c11','-Wno-tautological-compare','-fsanitize=address,undefined',f'-I{src}/mui',f'-I{mod}',f'-I{mod}/vfs',f'-I{root}/tools/faba-test-support',f'-I{root}/firmware/pixl-faba/fw/components/mlib',str(p/'menu.c'),'-o',str(p/'menu')],check=True)
 subprocess.run([str(p/'menu'),str(root/'out/faba-catalog')],check=True)
