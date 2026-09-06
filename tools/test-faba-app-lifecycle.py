#!/usr/bin/env python3
"""Exercise the actual app run/kill callbacks, tracking ownership at UI boundaries."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];src=root/'firmware/pixl-faba/fw/application/src'
s=(src/'app/chameleon/app_chameleon.c').read_text()
def function(name):
 m=re.search(r'^void '+name+r'\([^;]*?\)\s*\{',s,re.M);assert m,name
 i=m.end();depth=1
 while depth:depth+=(s[i]=='{')-(s[i]=='}');i+=1
 return s[m.start():i]
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdio.h>
static unsigned live, msgboxes;
void *mui_mem_malloc(size_t n){void *p=calloc(1,n);assert(p);live++;return p;}
void mui_mem_free(void *p){if(p){live--;free(p);}}
typedef struct {void *view;} widget;
static widget *create_widget(void){widget *w=mui_mem_malloc(sizeof(*w));w->view=mui_mem_malloc(64);return w;}
static void free_widget(widget *w){mui_mem_free(w->view);mui_mem_free(w);}
#define WIDGET(name) \
widget *name##_create(void){return create_widget();} \
void name##_free(widget *w){free_widget(w);} \
void name##_set_user_data(widget *w,void *p){(void)w;(void)p;} \
void *name##_get_view(widget *w){return w->view;}
WIDGET(chameleon_view)
WIDGET(mui_list_view)
WIDGET(mui_text_input)
WIDGET(mui_toast_view)
widget *mui_msg_box_create(void){msgboxes++;return create_widget();}
void mui_msg_box_free(widget *w){msgboxes--;free_widget(w);}
void mui_msg_box_set_user_data(widget *w,void*p){(void)w;(void)p;}
void *mui_msg_box_get_view(widget*w){return w->view;}
WIDGET(mui_scene_dispatcher)
WIDGET(mui_view_dispatcher)
typedef struct {int cycle_mode_index;} app_chameleon_retain_data_t;
typedef struct {
 widget *p_view_dispatcher,*p_chameleon_view,*p_scene_dispatcher,*p_list_view,*p_msg_box,*p_toast_view,*p_text_input,*p_view_dispatcher_toast;
} app_chameleon_t;
typedef struct {unsigned id;} mini_app_t;
typedef struct {mini_app_t *p_app;void *p_handle;void *p_retain_data;} mini_app_inst_t;
#define MINI_APP_ID_FABA 10
#define CHAMELEON_SCENE_FABA_CATALOG 1
#define CHAMELEON_SCENE_MAIN 2
#define CHAMELEON_SCENE_FACTORY 3
#define CHAMELEON_SCENE_MAX 4
#define CHAMELEON_VIEW_ID_MAIN 0
#define CHAMELEON_VIEW_ID_LIST 1
#define CHAMELEON_VIEW_ID_MSG_BOX 2
#define CHAMELEON_VIEW_ID_TEXT_INPUT 3
#define CHAMELEON_VIEW_ID_TOAST 4
#define MUI_LAYER_FULLSCREEN 0
#define MUI_LAYER_TOAST 1
#define TAG_SENSE_HF 0
#define INVALID_SLOT_INDEX 255
static void *chameleon_scene_defines;
void mui_view_dispatcher_add_view(widget*a,int b,void*c){(void)a;(void)b;(void)c;}
void mui_view_dispatcher_attach(widget*a,int b){(void)a;(void)b;}
void mui_view_dispatcher_detach(widget*a,int b){(void)a;(void)b;}
void mui_scene_dispatcher_set_scene_defines(widget*a,void*b,int c){(void)a;(void)b;(void)c;}
void mui_scene_dispatcher_next_scene(widget*a,int b){(void)a;(void)b;}
void mui_view_dispatcher_switch_to_view(widget*a,int b){(void)a;(void)b;}
void mui_scene_dispatcher_exit(widget*a){(void)a;}
void app_chameleon_register_back_handler(app_chameleon_t*a,unsigned b){(void)a;(void)b;}
void tag_emulation_init(void){}
bool fds_config_file_exists(void){return true;}
void faba_player_stop(void){}
void faba_record_finish(void){}
void faba_catalog_close(void){}
int chameleon_view_get_index(widget*w){(void)w;return 7;}
typedef struct{unsigned chameleon_default_slot_index;} settings_data_t;
static settings_data_t settings;
settings_data_t *settings_get_data(void){return &settings;}
bool tag_helper_valid_default_slot(void){return true;}
bool tag_emulation_slot_is_enabled(unsigned a,int b){(void)a;(void)b;return true;}
void tag_emulation_change_slot(unsigned a,bool b){(void)a;(void)b;}
void tag_emulation_save(void){}
void settings_save(void){}
'''
main=r'''
int main(int argc,char **argv){
 (void)argc;bool null_retain=argv[1][0]=='n';
 mini_app_t app={10};app_chameleon_retain_data_t retain={0};
 mini_app_inst_t inst={.p_app=&app,.p_retain_data=null_retain?NULL:&retain};
 for(unsigned i=0;i<100;i++){
  app_chameleon_on_run(&inst);app_chameleon_on_kill(&inst);
  printf("Close %u: live allocations=%u, message boxes=%u\n",i+1,live,msgboxes);fflush(stdout);
  assert(inst.p_handle==NULL);assert(live==0&&msgboxes==0);
 }
 puts("PASS: actual app callbacks release every UI object over 100 reopenings");
}
'''
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'app.c').write_text(prefix+function('app_chameleon_on_run')+function('app_chameleon_on_kill')+main)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-Wno-unused-variable','-fsanitize=address,undefined',str(p/'app.c'),'-o',str(p/'app')],check=True)
 for arg in ['retain','null']:subprocess.run([str(p/'app'),arg],check=True)
