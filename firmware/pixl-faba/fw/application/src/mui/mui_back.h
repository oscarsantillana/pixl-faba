#ifndef MUI_BACK_H
#define MUI_BACK_H

#include "mui_input.h"
#include "mui_scene_dispatcher.h"

typedef bool (*mui_back_cb_t)(void *ctx, mui_input_event_t *event);

typedef struct {
    mui_scene_dispatcher_t *p_scene_dispatcher;
    uint32_t mini_app_id;
    mui_back_cb_t custom_cb;
    void *custom_ctx;
} mui_back_config_t;

bool mui_back_handle(mui_back_config_t *p_config, mui_input_event_t *event);

#endif
