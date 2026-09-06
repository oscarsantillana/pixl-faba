#include "app_chameleon.h"
#include "chameleon_scene.h"
#include "faba_catalog_data.h"
#include "faba_player.h"
#include "faba_favorites.h"
#include "tag_emulation.h"
#include "hal_nfc_t2t.h"
#include "mini_app_launcher.h"
#include "mui_icons.h"
#include "mui_mem.h"
#include "vfs.h"
#include <stdio.h>
#include <string.h>

#define PAGE_SIZE 10
#define ACTION_BACK 10000
#define ACTION_NEXT 10001
#define ACTION_PREV 10002
#define ACTION_DIAGNOSTICS 10003
#define ACTION_NONE 10004
#define FAVORITES_GROUP FABA_CATALOG_GROUP_COUNT

typedef struct {
    unsigned group, page, total;
    faba_favorites_t favorites;
    uint16_t id, focus;
    char title[FABA_CATALOG_RECORD_SIZE - 2];
} catalog_state_t;
static catalog_state_t *catalog;

void faba_catalog_close(void) {
    if (catalog) mui_mem_free(catalog);
    catalog = NULL;
}
static void show_list(app_chameleon_t *app, mui_list_view_selected_cb cb) {
    mui_list_view_set_selected_cb(app->p_list_view, cb);
    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, CHAMELEON_VIEW_ID_LIST);
}
static void back(app_chameleon_t *app) {
    if (mui_scene_dispatcher_is_at_root(app->p_scene_dispatcher)) mini_app_launcher_exit(mini_app_launcher());
    else mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
}
static void catalog_select(mui_list_view_event_t event, mui_list_view_t *view, mui_list_item_t *item) {
    app_chameleon_t *app = view->user_data;
    uintptr_t action = (uintptr_t)item->user_data;
    if (action == ACTION_BACK) back(app);
    else if (action == ACTION_DIAGNOSTICS)
        mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, CHAMELEON_SCENE_FABA);
    else if (catalog && action <= FAVORITES_GROUP) {
        catalog->group = action; catalog->page = 0; catalog->focus = 0;
        mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, CHAMELEON_SCENE_FABA_TITLES);
    }
}
void chameleon_scene_faba_catalog_on_enter(void *user_data) {
    app_chameleon_t *app = user_data;
    tag_emulation_sense_end(); hal_nfc_set_nrfx_irq_enable(false);
    if (!catalog) {
        mui_mem_monitor_t mem; mui_mem_monitor(&mem);
        if (mem.free_biggest_size >= sizeof(*catalog) && mem.free_size > 4096)
            catalog = mui_mem_malloc(sizeof(*catalog));
        if (catalog) {
            memset(catalog, 0, sizeof(*catalog));
            if (faba_favorites_load(&catalog->favorites) != VFS_OK)
                mui_toast_view_show(app->p_toast_view, "No se pudieron cargar preferidos");
        }
    }
    if (catalog) {
        char detail[24];snprintf(detail,sizeof(detail),"%u títulos",catalog->favorites.count);
        mui_list_view_add_item_ext(app->p_list_view,ICON_FAVORITE,"Preferidos",detail,(void *)FAVORITES_GROUP);
    }
    if (catalog) for (unsigned i = 0; i < FABA_CATALOG_GROUP_COUNT; i++) {
        char detail[24]; snprintf(detail, sizeof(detail), "%u titles", faba_groups[i].count);
        mui_list_view_add_item_ext(app->p_list_view, ICON_FOLDER, faba_groups[i].name, detail, (void *)(uintptr_t)i);
    }
    else mui_toast_view_show(app->p_toast_view, "Not enough memory; reopen Faba");
    mui_list_view_add_item(app->p_list_view, ICON_SETTINGS, "Diagnostics", (void *)ACTION_DIAGNOSTICS);
    mui_list_view_add_item(app->p_list_view, ICON_HOME, "Main menu", (void *)ACTION_BACK);
    show_list(app, catalog_select);
}
void chameleon_scene_faba_catalog_on_exit(void *user_data) {
    mui_list_view_clear_items(((app_chameleon_t *)user_data)->p_list_view);
}

/* Fixed-size records allow bounded RAM use. VFS has no seek, so skip complete
 * records sequentially; only ten titles are allocated in the list at a time. */
static bool load_page(app_chameleon_t *app) {
    char path[32]; uint8_t record[FABA_CATALOG_RECORD_SIZE];
    vfs_driver_t *fs = vfs_get_driver(VFS_DRIVE_EXT);
    if (!fs || !fs->mounted()) return false;
    bool favorites=catalog->group==FAVORITES_GROUP;
    if(favorites&&!catalog->favorites.available)return false;
    unsigned first=catalog->page*PAGE_SIZE, seen=0;
    unsigned begin=favorites?0:catalog->group, end=favorites?FABA_CATALOG_GROUP_COUNT:begin+1;
    for(unsigned group=begin;group<end;group++) {
        snprintf(path,sizeof(path),"/faba/cat%u.bin",group);
        vfs_obj_t info;unsigned count=faba_groups[group].count;
        if(fs->stat_file(path,&info)!=VFS_OK||info.size!=16+count*sizeof(record))return false;
        vfs_file_t file;
        if(fs->open_file(path,&file,VFS_MODE_READONLY)!=VFS_OK)return false;
        bool ok=fs->read_file(&file,record,16)==16&&!memcmp(record,"FABACAT1",8)&&
            record[8]==group&&record[9]==0&&(unsigned)(record[10]|record[11]<<8)==count&&
            record[12]==sizeof(record)&&record[13]==0&&record[14]==0&&record[15]==0;
        for(unsigned i=0;ok&&i<count;i++) {
            ok=fs->read_file(&file,record,sizeof(record))==sizeof(record);
            if(!ok)break;
            unsigned id=record[0]|record[1]<<8;
            ok=id>0&&id<=9999&&record[2]&&memchr(record+2,0,sizeof(record)-2);
            if(!ok)break;
            bool starred=faba_favorites_has(&catalog->favorites,id);
            if(favorites&&!starred)continue;
            if(seen>=first&&seen<first+PAGE_SIZE) {
                char detail[32];snprintf(detail,sizeof(detail),"%s %04u",favorites?faba_groups[group].name:"Faba",id);
                mui_list_view_add_item_ext(app->p_list_view,starred?ICON_FAVORITE:ICON_FILE,(char *)record+2,detail,(void *)(uintptr_t)id);
            }
            seen++;
        }
        int32_t closed=fs->close_file(&file);
        if(!ok||closed!=VFS_OK)return false;
    }
    catalog->total=seen;
    return true;
}
void chameleon_scene_faba_titles_on_enter(void *user_data);
static void titles_select(mui_list_view_event_t event, mui_list_view_t *view, mui_list_item_t *item) {
    app_chameleon_t *app = view->user_data;
    uintptr_t action = (uintptr_t)item->user_data;
    if (event == MUI_LIST_VIEW_EVENT_LONG_SELECTED && action > 0 && action < ACTION_BACK) {
        bool was=faba_favorites_has(&catalog->favorites,action);
        catalog->focus=mui_list_view_get_focus(view);
        if(faba_favorites_toggle(&catalog->favorites,action)!=VFS_OK) {
            mui_toast_view_show(app->p_toast_view,"No se pudo guardar preferidos");return;
        }
        if(catalog->group==FAVORITES_GROUP) {
            unsigned last=catalog->favorites.count?(catalog->favorites.count-1)/PAGE_SIZE:0;
            if(catalog->page>last)catalog->page=last;
        }
        mui_list_view_clear_items(view);chameleon_scene_faba_titles_on_enter(app);
        mui_toast_view_show(app->p_toast_view,was?"Quitado de preferidos":"Añadido a preferidos");
        return;
    }
    if(event != MUI_LIST_VIEW_EVENT_SELECTED)return;
    if (action == ACTION_BACK) back(app);
    else if (action == ACTION_NEXT || action == ACTION_PREV) {
        if (action == ACTION_NEXT) catalog->page++; else if (catalog->page) catalog->page--;
        catalog->focus = 0;
        mui_list_view_clear_items(view);
        chameleon_scene_faba_titles_on_enter(app);
    } else if (action > 0 && action < ACTION_BACK) {
        catalog->id = action;
        catalog->focus = mui_list_view_get_focus(view);
        snprintf(catalog->title, sizeof(catalog->title), "%s", string_get_cstr(item->text));
        mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, CHAMELEON_SCENE_FABA_PLAY);
    }
}
void chameleon_scene_faba_titles_on_enter(void *user_data) {
    app_chameleon_t *app = user_data;
    if (!load_page(app)) {
        mui_list_view_clear_items(app->p_list_view);
        mui_toast_view_show(app->p_toast_view, catalog->group==FAVORITES_GROUP&&!catalog->favorites.available ? "Preferidos no disponibles" : "Install Faba catalog files via BLE");
    } else {
        char label[32];
        if(catalog->group==FAVORITES_GROUP&&!catalog->total)
            mui_list_view_add_item(app->p_list_view,ICON_FAVORITE,"Sin preferidos",(void *)ACTION_NONE);
        if ((catalog->page + 1) * PAGE_SIZE < catalog->total) {
            snprintf(label, sizeof(label), "Next page (%u/%u)", catalog->page + 1,
                (catalog->total + PAGE_SIZE - 1) / PAGE_SIZE);
            mui_list_view_add_item(app->p_list_view, ICON_FOLDER, label, (void *)ACTION_NEXT);
        }
        if (catalog->page) mui_list_view_add_item(app->p_list_view, ICON_BACK, "Previous page", (void *)ACTION_PREV);
    }
    mui_list_view_add_item(app->p_list_view,ICON_FAVORITE,"Mantén OK: preferidos",(void *)ACTION_NONE);
    mui_list_view_add_item(app->p_list_view, ICON_BACK, "Languages", (void *)ACTION_BACK);
    show_list(app, titles_select);
    mui_list_view_set_focus(app->p_list_view, catalog->focus < mui_list_view_item_size(app->p_list_view) ? catalog->focus : 0);
}
void chameleon_scene_faba_titles_on_exit(void *user_data) {
    mui_list_view_clear_items(((app_chameleon_t *)user_data)->p_list_view);
}
static void play_select(mui_list_view_event_t event, mui_list_view_t *view, mui_list_item_t *item) {
    if ((uintptr_t)item->user_data == ACTION_BACK) back(view->user_data);
}
void chameleon_scene_faba_play_on_enter(void *user_data) {
    app_chameleon_t *app = user_data;
    char detail[24]; snprintf(detail, sizeof(detail), "Faba %04u / Ready", catalog->id);
    mui_list_view_add_item_ext(app->p_list_view, ICON_VIEW, catalog->title, detail, NULL);
    mui_list_view_add_item(app->p_list_view, ICON_VIEW, "Keep close to Faba", NULL);
    mui_list_view_add_item(app->p_list_view, ICON_BACK, "Stop and return", (void *)ACTION_BACK);
    show_list(app, play_select);
    if (!faba_player_start(catalog->id)) {
        mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
        mui_toast_view_show(app->p_toast_view, "Cannot start; save trace or reopen Faba");
    }
}
void chameleon_scene_faba_play_on_exit(void *user_data) {
    faba_player_stop();
    mui_list_view_clear_items(((app_chameleon_t *)user_data)->p_list_view);
}
