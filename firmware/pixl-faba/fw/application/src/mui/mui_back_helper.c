#include "mui_back_helper.h"

#include "m-string.h"
#include "mui_msg_box.h"
#include "mui_text_input.h"

static bool mui_msg_box_has_buttons(mui_msg_box_t *p_msg_box) {
    if (!p_msg_box) {
        return false;
    }
    return string_size(p_msg_box->btn_left_text) > 0 || string_size(p_msg_box->btn_center_text) > 0 ||
           string_size(p_msg_box->btn_right_text) > 0;
}

void mui_back_dismiss_msg_box(mui_msg_box_t *p_msg_box) {
    if (!p_msg_box || !p_msg_box->event_cb || !mui_msg_box_has_buttons(p_msg_box)) {
        return;
    }
    if (string_size(p_msg_box->btn_right_text) > 0) {
        p_msg_box->event_cb(MUI_MSG_BOX_EVENT_SELECT_RIGHT, p_msg_box);
    } else if (string_size(p_msg_box->btn_center_text) > 0) {
        p_msg_box->event_cb(MUI_MSG_BOX_EVENT_SELECT_CENTER, p_msg_box);
    } else if (string_size(p_msg_box->btn_left_text) > 0) {
        p_msg_box->event_cb(MUI_MSG_BOX_EVENT_SELECT_LEFT, p_msg_box);
    }
}

bool mui_back_try_overlay_views(mui_view_dispatcher_t *p_view_dispatcher, mui_text_input_t *p_text_input,
                                mui_msg_box_t *p_msg_box, bool text_input_cancel_to_view,
                                uint32_t text_input_cancel_view_id) {
    mui_view_t *p_active_view = mui_view_dispatcher_get_active_view(p_view_dispatcher);
    if (!p_active_view) {
        return false;
    }

    if (p_msg_box && p_active_view == mui_msg_box_get_view(p_msg_box)) {
        if (!mui_msg_box_has_buttons(p_msg_box)) {
            return false;
        }
        mui_back_dismiss_msg_box(p_msg_box);
        return true;
    }

    if (p_text_input && p_active_view == mui_text_input_get_view(p_text_input)) {
        if (p_text_input->event_cb) {
            p_text_input->event_cb(MUI_TEXT_INPUT_EVENT_CANCELLED, p_text_input);
        }
        if (text_input_cancel_to_view &&
            mui_view_dispatcher_get_active_view(p_view_dispatcher) == mui_text_input_get_view(p_text_input)) {
            mui_view_dispatcher_switch_to_view(p_view_dispatcher, text_input_cancel_view_id);
        }
        if (mui_view_dispatcher_get_active_view(p_view_dispatcher) == mui_text_input_get_view(p_text_input)) {
            return false;
        }
        return true;
    }

    return false;
}

bool mui_back_if_active_view(mui_view_dispatcher_t *p_view_dispatcher, mui_view_t *p_view,
                             mui_scene_dispatcher_t *p_scene_dispatcher) {
    if (!p_view_dispatcher || !p_view || !p_scene_dispatcher) {
        return false;
    }
    if (mui_view_dispatcher_get_active_view(p_view_dispatcher) == p_view) {
        mui_scene_dispatcher_previous_scene(p_scene_dispatcher);
        return true;
    }
    return false;
}

bool mui_back_app_common_cb(void *ctx, mui_input_event_t *event) {
    mui_back_app_ctx_t *p_app_ctx = ctx;
    if (mui_back_try_overlay_views(p_app_ctx->p_view_dispatcher, p_app_ctx->p_text_input, p_app_ctx->p_msg_box,
                                   p_app_ctx->text_input_cancel_to_view, p_app_ctx->text_input_cancel_view_id)) {
        return true;
    }
    if (p_app_ctx->extra_cb && p_app_ctx->extra_cb(p_app_ctx->extra_ctx, event)) {
        return true;
    }
    return false;
}

void mui_back_register_app(mui_view_dispatcher_t *p_view_dispatcher, mui_back_app_ctx_t *p_app_ctx,
                           uint32_t mini_app_id) {
    mui_back_config_t back_config = {
        .p_scene_dispatcher = p_app_ctx->p_scene_dispatcher,
        .mini_app_id = mini_app_id,
        .custom_cb = mui_back_app_common_cb,
        .custom_ctx = p_app_ctx,
    };
    mui_view_dispatcher_set_back_handler(p_view_dispatcher, &back_config);
}
