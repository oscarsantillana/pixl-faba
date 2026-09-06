#include <stdlib.h>
#include "faba_diag.h"
#include "app_error.h"

#include "nrf_log.h"
#include "nrf_log_ctrl.h"
#include "app_util_platform.h"
#include "nrf_strerror.h"

#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
#include "nrf_sdm.h"
#endif

#include "mui_core.h"
#include "nrf_delay.h"
#include "cache.h"


/*lint -save -e14 */
/**
 * Function is implemented as weak so that it can be overwritten by custom application error handler
 * when needed.
 */
void app_error_fault_handler(uint32_t id, uint32_t pc, uint32_t info)
{
    if (id == NRF_FAULT_ID_SDK_ERROR && info) {
        error_info_t *e = (error_info_t *)info;
        faba_diag_fault(id, (uint32_t)e->p_file_name, e->err_code);
        faba_diag_location(e->line_num);
    } else {
        faba_diag_fault(id, pc, info);
    }
    __disable_irq();
    NRF_LOG_FINAL_FLUSH();

#ifndef DEBUG
    NRF_LOG_ERROR("Fatal error");
#else
    switch (id)
    {
#if defined(SOFTDEVICE_PRESENT) && SOFTDEVICE_PRESENT
        case NRF_FAULT_ID_SD_ASSERT:
            NRF_LOG_ERROR("SOFTDEVICE: ASSERTION FAILED");
            break;
        case NRF_FAULT_ID_APP_MEMACC:
            NRF_LOG_ERROR("SOFTDEVICE: INVALID MEMORY ACCESS");
            break;
#endif
        case NRF_FAULT_ID_SDK_ASSERT:
        {
            assert_info_t * p_info = (assert_info_t *)info;
            NRF_LOG_ERROR("ASSERTION FAILED at %s:%u",
                          p_info->p_file_name,
                          p_info->line_num);
            break;
        }
        case NRF_FAULT_ID_SDK_ERROR:
        {
            error_info_t * p_info = (error_info_t *)info;
            NRF_LOG_ERROR("ERROR %u [%s] at %s:%u\r\nPC at: 0x%08x",
                          p_info->err_code,
                          nrf_strerror_get(p_info->err_code),
                          p_info->p_file_name,
                          p_info->line_num,
                          pc);
            NRF_LOG_ERROR("End of error report");

            char error[256];
            sprintf(error, "ERROR %u [%s] at %s:%u\r\nPC at: 0x%08x",
                          p_info->err_code,
                          nrf_strerror_get(p_info->err_code),
                          p_info->p_file_name,
                          p_info->line_num,
                          pc);
            
            mui_panic(mui(), error);
            cache_clean();
            nrf_delay_ms(5000);
            NRF_LOG_WARNING("System reset");
            NVIC_SystemReset();
            break;
        }
        default:
            NRF_LOG_ERROR("UNKNOWN FAULT at 0x%08X", pc);
            break;
    }
#endif

    NRF_BREAKPOINT_COND;
    // On assert, the system can only recover with a reset.

#ifndef DEBUG
    NRF_LOG_WARNING("System reset");
    NVIC_SystemReset();
#else
    app_error_save_and_stop(id, pc, info);
#endif // DEBUG
}



void _exit(int status){
    faba_diag_fault(0xF0000002u, (uint32_t)__builtin_return_address(0), status);
    NRF_LOG_WARNING("System reset: %d", status);
#ifndef DEBUG
    NVIC_SystemReset();
#else
    // endless loop here to wait debugger attach
    NRF_BREAKPOINT_COND;
    while(1)
        ;
#endif
}
/* Record C-library assertions before abort reaches _exit. Preserve the first
 * failure so later reset/abort wrappers cannot hide its source location. */
void __assert_func(const char *file, int line, const char *func, const char *expr) {
    (void)func;
    faba_diag_fault(0xF0000001u, (uint32_t)file, (uint32_t)expr);
    faba_diag_location((uint32_t)line);
    abort();
}
