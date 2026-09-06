#include <stdlib.h>
#include "faba_diag.h"
#include "app_timer.h"
#include "mui_mem.h"
#include <string.h>
#define DIAG_MAGIC 0x37444246u
/* Separate from cache_clean's region and from the normal UI heap. */
static volatile faba_diag_t report __attribute__((section(".faba_diag")));
_Static_assert(sizeof(faba_diag_t) == 64, "Diagnostic wire format and RAM budget");
static int live(void) { return report.magic == DIAG_MAGIC && report.story && !(report.flags & 1); }
void faba_diag_boot(uint32_t reset_reason) {
    if (report.magic != DIAG_MAGIC) {
        memset((void *)&report, 0, sizeof(report));
        report.magic = DIAG_MAGIC;
    } else if (report.story) report.flags |= 1; /* Report belongs to previous boot. */
    report.reset_reason = reset_reason;
}
void faba_diag_begin(uint16_t story) {
    uint32_t reset_reason = report.reset_reason;
    memset((void *)&report, 0, sizeof(report));
    report.magic = DIAG_MAGIC; report.reset_reason = reset_reason;
    report.story = story; report.phase = 1; report.start_tick = app_timer_cnt_get();
    mui_mem_monitor_t mem; mui_mem_monitor(&mem);
    report.free_start = mem.free_size; report.biggest_start = mem.free_biggest_size;
}
void faba_diag_phase(uint32_t phase) {
    if (!live()) return;
    report.phase = phase; report.end_tick = app_timer_cnt_get();
    if (phase >= 3) {
        mui_mem_monitor_t mem; mui_mem_monitor(&mem); report.free_stop = mem.free_size;
    }
}
void faba_diag_input(uint32_t input) {
    /* Keep recording navigation through the app exit, then freeze for BLE. */
    if (!live() || (report.flags & 2)) return;
    report.last_input = input; report.input_tick = app_timer_cnt_get(); report.input_count++;
}
void faba_diag_kill(uint32_t app_id) {
    if (app_id == 10 && live()) { report.flags |= 2; report.end_tick = app_timer_cnt_get(); }
}
void faba_diag_fault(uint32_t id, uint32_t pc, uint32_t info) {
    /* Fault path: no allocation, timers, flash or potentially corrupt heap walks. */
    if (!live() || (report.flags & 4)) return;
    report.fault_id = id; report.fault_pc = pc; report.fault_info = info; report.flags |= 4;
}
void faba_diag_queue_overflow(void) {
    if (live() && report.phase == 2 && !(report.flags & 4)) {
        report.flags |= 32; report.free_stop++;
    }
}
void faba_diag_memory_full(const char *file, uint32_t line, uint32_t size) {
    faba_diag_fault(0xF0000006u, (uint32_t)(uintptr_t)file, size);
    faba_diag_location(line);
    abort();
}
void faba_diag_cpu_status(uint32_t cfsr) {
    if (live()) { report.free_stop = cfsr; report.flags |= 16; }
}
void faba_diag_location(uint32_t line) {
    if (live()) { report.phase = (report.phase & 255) | (line << 8); report.flags |= 8; }
}
const volatile faba_diag_t *faba_diag_report(void) { return &report; }
