#include "faba_diag.h"
#include "faba_player.h"
#include "faba_trace.h"
#include "faba_player_template.h"
#include "tag_helper.h"
#include "tag_emulation.h"
#include "nfc_mf0_ntag.h"
#include "hal_nfc_t2t.h"
#include "nrf_pwr_mgmt.h"
#include "mui_mem.h"
#include <stddef.h>
#include <string.h>

static uint8_t *player_data;
static uint16_t previous_timeout;
_Static_assert(offsetof(nfc_tag_mf0_ntag_information_t, memory) == 273, "NTAG template ABI");
_Static_assert(sizeof(faba_player_template) == 497, "NTAG213 profile size");

bool faba_player_start(uint16_t id) {
    if (player_data || faba_trace_active() || id > 9999) return false;
    faba_diag_begin(id);
    mui_mem_monitor_t memory;
    mui_mem_monitor(&memory);
    if (memory.free_biggest_size < sizeof(faba_player_template) || memory.free_size < 4096) return false;
    player_data = mui_mem_malloc(sizeof(faba_player_template));
    if (!player_data) return false;
    memcpy(player_data, faba_player_template, sizeof(faba_player_template));
    /* Preserve the working sticker's TLVs, language, UID and configuration.
     * This private buffer is never passed to the slot persistence layer. */
    for (int i = 3; i >= 0; i--) { player_data[273 + 38 + i] = '0' + id % 10; id /= 10; }
    tag_emulation_sense_end();
    hal_nfc_set_nrfx_irq_enable(false);
    if (!faba_timing_start()) { mui_mem_free(player_data); player_data = NULL; return false; }
    tag_data_buffer_t buffer = { .length = sizeof(faba_player_template), .buffer = player_data, .crc = NULL };
    nfc_tag_mf0_ntag_data_loadcb(TAG_TYPE_NTAG_213, &buffer);
    previous_timeout = nrf_pwr_mgmt_get_timeout();
    nrf_pwr_mgmt_set_timeout(0);
    faba_diag_phase(2);
    faba_compat_set_playing(true);
    hal_nfc_set_nrfx_irq_enable(true);
    /* The catalog owns its NTAG213 context, independent of the active slot. */
    nfc_tag_14a_sense_switch(true);
    return true;
}

void faba_player_stop(void) {
    if (!player_data) return;
    faba_compat_set_playing(false);
    nfc_tag_14a_sense_switch(false);
    hal_nfc_set_nrfx_irq_enable(false);
    faba_timing_stop();
    faba_diag_phase(3);
    nfc_tag_14a_handler_t empty = {0};
    nfc_tag_14a_set_handler(&empty);
    /* Restore the saved slot's handler before any app-exit save can run. */
    tag_specific_type_t type = tag_helper_get_active_tag_type();
    if (get_buffer_by_tag_type(type)) tag_emulation_load_by_buffer(type, false);
    mui_mem_free(player_data);
    player_data = NULL;
    nrf_pwr_mgmt_feed();
    nrf_pwr_mgmt_set_timeout(previous_timeout);
}
