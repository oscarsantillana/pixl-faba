#include "app_chameleon.h"
#include "chameleon_scene.h"
#include "faba_trace.h"
#include "tag_helper.h"
#include "tag_emulation.h"
#include "hal_nfc_t2t.h"
#include "nrf_pwr_mgmt.h"
#include "mini_app_launcher.h"
#include "mui_icons.h"
#include <stdio.h>

enum { RECORD_FABA, RECORD_PHONE, CHOOSE_SLOT, RETRY_SAVE, HOME, BACK };
static uint8_t reader;
static uint16_t previous_timeout;
static int32_t save_result;

static void faba_select(mui_list_view_event_t event, mui_list_view_t *view, mui_list_item_t *item) {
    app_chameleon_t *app = view->user_data;
    switch ((uintptr_t)item->user_data) {
    case RECORD_FABA:
    case RECORD_PHONE:
        reader = (uintptr_t)item->user_data;
        if (!faba_trace_begin(reader)) {
            mui_toast_view_show(app->p_toast_view, "Check NTAG213 slot, save trace or restart");
            break;
        }
        mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, CHAMELEON_SCENE_FABA_RECORD);
        break;
    case CHOOSE_SLOT:
        mui_scene_dispatcher_next_scene(app->p_scene_dispatcher, CHAMELEON_SCENE_MENU_CARD_SLOT_SELECT);
        break;
    case RETRY_SAVE:
        save_result = faba_trace_save();
        mui_toast_view_show(app->p_toast_view, save_result == 0 ? "Trace saved" : "Save failed; trace stays in RAM");
        break;
    case HOME: mini_app_launcher_exit(mini_app_launcher()); break;
    case BACK:
        if (mui_scene_dispatcher_is_at_root(app->p_scene_dispatcher))
            mini_app_launcher_exit(mini_app_launcher());
        else mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
        break;
    }
}

void chameleon_scene_faba_on_enter(void *user_data) {
    app_chameleon_t *app = user_data;
    tag_emulation_sense_end();
    hal_nfc_set_nrfx_irq_enable(false);
    char text[64];
    const faba_trace_header_t *h = faba_trace_header();
    const tag_specific_type_name_t *type = tag_helper_get_tag_type_name(tag_helper_get_active_tag_type());
    snprintf(text, sizeof(text), "Slot %02u / %s", tag_emulation_get_slot() + 1, type->short_name);
    mui_list_view_add_item_ext(app->p_list_view, ICON_SLOT, "Faba diagnostics", text, (void *)CHOOSE_SLOT);
    mui_list_view_add_item(app->p_list_view, ICON_VIEW, "Record Faba", (void *)RECORD_FABA);
    mui_list_view_add_item(app->p_list_view, ICON_VIEW, "Phone control", (void *)RECORD_PHONE);
    if (h->version) {
        snprintf(text, sizeof(text), "%lu events / %lu dropped", (unsigned long)h->count, (unsigned long)h->dropped);
        mui_list_view_add_item_ext(app->p_list_view, ICON_FILE,
            *faba_trace_last_path() ? faba_trace_last_path() : "Trace not saved", text, (void *)RETRY_SAVE);
        if (save_result) mui_list_view_add_item(app->p_list_view, ICON_FILE, "Retry save", (void *)RETRY_SAVE);
    }
    mui_list_view_add_item(app->p_list_view, ICON_BACK, "Back", (void *)BACK);
    mui_list_view_add_item(app->p_list_view, ICON_HOME, "Main menu", (void *)HOME);
    mui_list_view_set_selected_cb(app->p_list_view, faba_select);
    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, CHAMELEON_VIEW_ID_LIST);
}

void chameleon_scene_faba_on_exit(void *user_data) {
    app_chameleon_t *app = user_data;
    mui_list_view_clear_items(app->p_list_view);
}

static void record_select(mui_list_view_event_t event, mui_list_view_t *view, mui_list_item_t *item) {
    app_chameleon_t *app = view->user_data;
    mui_scene_dispatcher_previous_scene(app->p_scene_dispatcher);
}

void chameleon_scene_faba_record_on_enter(void *user_data) {
    app_chameleon_t *app = user_data;
    mui_list_view_add_item(app->p_list_view, ICON_FILE, "Stop and save", 0);
    mui_list_view_add_item(app->p_list_view, ICON_VIEW,
        reader ? "Scan once with iPhone" : "Place on Faba briefly", 0);
    mui_list_view_add_item(app->p_list_view, ICON_VIEW, "Then press Stop", 0);
    mui_list_view_set_selected_cb(app->p_list_view, record_select);
    mui_view_dispatcher_switch_to_view(app->p_view_dispatcher, CHAMELEON_VIEW_ID_LIST);
    previous_timeout = nrf_pwr_mgmt_get_timeout();
    nrf_pwr_mgmt_set_timeout(0);
    hal_nfc_set_nrfx_irq_enable(true);
    tag_emulation_sense_run();
}

void faba_record_finish(void) {
    if (!faba_trace_active()) return;
    /* Do not report events caused by stopping the emulator as reader events. */
    faba_trace_stop();
    tag_emulation_sense_end();
    hal_nfc_set_nrfx_irq_enable(false);
    faba_timing_stop();
    save_result = faba_trace_save();
    nrf_pwr_mgmt_feed();
    nrf_pwr_mgmt_set_timeout(previous_timeout);
}

void chameleon_scene_faba_record_on_exit(void *user_data) {
    app_chameleon_t *app = user_data;
    faba_record_finish();
    mui_list_view_clear_items(app->p_list_view);
    if (save_result) mui_toast_view_show(app->p_toast_view, "Save failed; retry in Faba menu");
}
