#ifndef FABA_DIAG_H
#define FABA_DIAG_H
#include <stdint.h>
/* Read-only BLE command 03 returns these 16 little-endian words. The dedicated
 * NOLOAD region survives software reset; power removal may erase the report. */
typedef struct {
    uint32_t magic, flags, reset_reason, phase, story, start_tick, end_tick;
    uint32_t last_input, input_tick, input_count, free_start, biggest_start;
    uint32_t free_stop, fault_id, fault_pc, fault_info;
} faba_diag_t;
void faba_diag_boot(uint32_t reset_reason);
void faba_diag_begin(uint16_t story);
void faba_diag_phase(uint32_t phase);
void faba_diag_input(uint32_t input);
void faba_diag_kill(uint32_t app_id);
void faba_diag_queue_overflow(void);
void faba_diag_memory_full(const char *file, uint32_t line, uint32_t size);
void faba_diag_cpu_status(uint32_t cfsr);
void faba_diag_location(uint32_t line);
void faba_diag_fault(uint32_t id, uint32_t pc, uint32_t info);
const volatile faba_diag_t *faba_diag_report(void);
#endif
