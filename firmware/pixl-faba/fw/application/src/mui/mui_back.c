#include "mui_back.h"

#include "mini_app_launcher.h"
#include "mini_app_registry.h"

bool mui_back_handle(mui_back_config_t *p_config, mui_input_event_t *event) {
    if (event->type != INPUT_TYPE_SHORT || event->key != INPUT_KEY_BACK) {
        return false;
    }

    if (p_config->custom_cb && p_config->custom_cb(p_config->custom_ctx, event)) {
        return true;
    }

    if (p_config->p_scene_dispatcher && mui_scene_dispatcher_stack_size(p_config->p_scene_dispatcher) > 1) {
        mui_scene_dispatcher_previous_scene(p_config->p_scene_dispatcher);
        return true;
    }

    if (p_config->mini_app_id != MINI_APP_ID_DESKTOP) {
        mini_app_launcher_kill(mini_app_launcher(), p_config->mini_app_id);
        return true;
    }

    return true;
}
