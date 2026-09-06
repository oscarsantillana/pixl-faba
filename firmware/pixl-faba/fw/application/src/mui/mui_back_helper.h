#ifndef MUI_BACK_HELPER_H
#define MUI_BACK_HELPER_H

#include "mui_back.h"
#include "mui_view.h"
#include "mui_view_dispatcher.h"

struct mui_msg_box_s;
typedef struct mui_msg_box_s mui_msg_box_t;
struct mui_text_input_s;
typedef struct mui_text_input_s mui_text_input_t;

bool mui_back_try_overlay_views(mui_view_dispatcher_t *p_view_dispatcher, mui_text_input_t *p_text_input,
                                mui_msg_box_t *p_msg_box, bool text_input_cancel_to_view,
                                uint32_t text_input_cancel_view_id);

bool mui_back_if_active_view(mui_view_dispatcher_t *p_view_dispatcher, mui_view_t *p_view,
                             mui_scene_dispatcher_t *p_scene_dispatcher);

void mui_back_dismiss_msg_box(mui_msg_box_t *p_msg_box);

typedef struct {
    mui_view_dispatcher_t *p_view_dispatcher;
    mui_text_input_t *p_text_input;
    mui_msg_box_t *p_msg_box;
    mui_scene_dispatcher_t *p_scene_dispatcher;
    mui_back_cb_t extra_cb;
    void *extra_ctx;
    /** When true, BACK on text input switches to text_input_cancel_view_id instead of previous_scene. */
    bool text_input_cancel_to_view;
    uint32_t text_input_cancel_view_id;
} mui_back_app_ctx_t;

bool mui_back_app_common_cb(void *ctx, mui_input_event_t *event);

void mui_back_register_app(mui_view_dispatcher_t *p_view_dispatcher, mui_back_app_ctx_t *p_app_ctx,
                           uint32_t mini_app_id);

#endif
