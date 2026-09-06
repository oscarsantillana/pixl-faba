#include "nrf_nvic.h"
#include "faba_diag.h"
#include <stdint.h>

/* Receive the exception stack before a C prologue changes MSP. Never inspect
 * an invalid frame or walk the UI heap while handling a processor exception. */
__attribute__((used, noinline, noreturn))
void faba_exception_reset(uint32_t *stack, uint32_t exc_return, uint32_t exception) {
    uint32_t pc = 0, saved_lr = 0;
    uint32_t cfsr = SCB->CFSR;
    uintptr_t frame = (uintptr_t)stack;
    if (!(exc_return & 16)) frame += 18 * sizeof(uint32_t);
    if (!(cfsr & 0x3838) && !(frame & 3) && frame >= 0x20000000u && frame <= 0x20010000u - 32) {
        uint32_t *core = (uint32_t *)frame;
        pc = core[6]; saved_lr = core[5];
    }
    faba_diag_fault(0xF1000000u | exception, pc, saved_lr);
    faba_diag_cpu_status(cfsr);
    NVIC_SystemReset();
}
#define DEF_INTERRUPT_HANDLER(name) \
    __attribute__((naked)) void name(void) { \
        __asm volatile("tst lr, #4\n" \
                       "ite eq\n" \
                       "mrseq r0, msp\n" \
                       "mrsne r0, psp\n" \
                       "mov r1, lr\n" \
                       "mrs r2, ipsr\n" \
                       "b faba_exception_reset\n"); \
    }
DEF_INTERRUPT_HANDLER(HardFault_Handler)
DEF_INTERRUPT_HANDLER(MemoryManagement_Handler)
DEF_INTERRUPT_HANDLER(BusFault_Handler)
DEF_INTERRUPT_HANDLER(UsageFault_Handler)
DEF_INTERRUPT_HANDLER(DebugMon_Handler)
