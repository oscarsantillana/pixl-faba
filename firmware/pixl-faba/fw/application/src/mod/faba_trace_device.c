#include "faba_trace.h"
#include "nrf.h"
#include "tag_helper.h"
#include "tag_emulation.h"
#include "vfs.h"
#include "version2.h"
#include "mui_mem.h"
#include <stdio.h>
#include <string.h>


void *faba_trace_alloc(size_t size) {
    mui_mem_monitor_t memory;
    mui_mem_monitor(&memory);
    /* Leave working space for the UI and filesystem while a capture is held. */
    if (memory.free_biggest_size < size || memory.free_size < size + 4096) return NULL;
    return mui_mem_malloc(size);
}
void faba_trace_free(void *ptr) { mui_mem_free(ptr); }

uint32_t faba_trace_cycles(void) { return DWT->CYCCNT; }
uint32_t faba_trace_rtc(void) { return NRF_RTC1->COUNTER; }

bool faba_trace_begin(uint8_t reader) {
    const nfc_tag_14a_coll_res_reference_t *identity = tag_helper_get_active_coll_res_ref();
    uint8_t slot = tag_emulation_get_slot();
    /* This diagnostic deliberately uses the user's existing NTAG213 slot. */
    if (reader > 1 || !identity || !identity->size || !identity->uid ||
        *identity->size > 10 || tag_helper_get_active_tag_type() != TAG_TYPE_NTAG_213 ||
        !tag_emulation_slot_is_enabled(slot, TAG_SENSE_HF) || faba_trace_get()) return false;
    faba_trace_header_t meta = {0};
    meta.reader = reader;
    meta.slot = slot;
    meta.tag_type = tag_helper_get_active_tag_type();
    meta.uid_size = *identity->size;
    meta.sak = *identity->sak;
    memcpy(meta.uid, identity->uid, meta.uid_size);
    memcpy(meta.atqa, identity->atqa, 2);
    meta.cycle_hz = 64000000;
    meta.rtc_hz = 32768 / (NRF_RTC1->PRESCALER + 1);
    snprintf(meta.firmware, sizeof(meta.firmware), "%s", version_get_version(version_get()));
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
    if (!faba_timing_start()) return false;
    meta.reserved[1] = 1; /* First32 + rolling last96 events. */
    meta.reserved[0] = 1; /* PPI TIMER2 16 MHz hardware timing payloads. */
    if (!faba_trace_start(&meta)) {
        faba_timing_stop();
        return false;
    }
    faba_trace_path_reset();
    return true;
}
