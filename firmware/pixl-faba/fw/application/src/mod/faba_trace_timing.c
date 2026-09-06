#include "faba_trace.h"
#include "nrf.h"
#include "nrfx_ppi.h"
#include "sdk_config.h"

/* TIMER0 belongs to S112, TIMER1 to display PWM, TIMER3 to NFCT's workaround,
 * TIMER4 to the alternate emulator. TIMER2 is unclaimed in this board build. */
#if TIMER2_ENABLED || NRFX_TIMER2_ENABLED
#error "Faba hardware capture requires exclusive TIMER2 ownership"
#endif
static nrf_ppi_channel_t channels[3];
static uint8_t allocated;
static bool running;
static bool config_saved;

void faba_timing_stop(void) {
    /* Caller has stopped NFC sensing before releasing event connections. */
    while (allocated) nrfx_ppi_channel_free(channels[--allocated]);
    if (running) {
        NRF_TIMER2->TASKS_STOP = 1;
        NRF_TIMER2->TASKS_SHUTDOWN = 1;
    }
    running = false;
}

bool faba_timing_start(void) {
    if (running || allocated) return false;
    volatile uint32_t *events[] = { &NRF_NFCT->EVENTS_RXFRAMEEND,
        &NRF_NFCT->EVENTS_TXFRAMESTART, &NRF_NFCT->EVENTS_TXFRAMEEND };
    for (unsigned i = 0; i < 3; i++) {
        if (nrfx_ppi_channel_alloc(&channels[i]) != NRFX_SUCCESS) goto fail;
        allocated++;
        if (nrfx_ppi_channel_assign(channels[i], (uint32_t)(uintptr_t)events[i],
            (uint32_t)(uintptr_t)&NRF_TIMER2->TASKS_CAPTURE[i]) != NRFX_SUCCESS) goto fail;
    }
    NRF_TIMER2->TASKS_STOP = 1;
    NRF_TIMER2->INTENCLR = 0xffffffff;
    NRF_TIMER2->SHORTS = 0;
    NRF_TIMER2->MODE = TIMER_MODE_MODE_Timer;
    NRF_TIMER2->BITMODE = TIMER_BITMODE_BITMODE_32Bit;
    NRF_TIMER2->PRESCALER = 0; /* 16 MHz; 62.5 ns per tick. */
    NRF_TIMER2->TASKS_CLEAR = 1;
    for (unsigned i = 0; i < 4; i++) {
        NRF_TIMER2->CC[i] = 0;
        NRF_TIMER2->EVENTS_COMPARE[i] = 0;
    }
    NRF_TIMER2->TASKS_START = 1;
    running = true;
    config_saved = false;
    for (unsigned i = 0; i < 3; i++) {
        if (nrfx_ppi_channel_enable(channels[i]) != NRFX_SUCCESS) goto fail;
    }
    return true;
fail:
    faba_timing_stop();
    return false;
}

void faba_timing_tx_start(uint8_t state) {
    if (!running || !faba_trace_active()) return;
    /* CC0/CC1 are latched by hardware, independent of interrupt latency. */
    NRF_TIMER2->TASKS_CAPTURE[3] = 1;
    uint32_t ticks[3] = { NRF_TIMER2->CC[0], NRF_TIMER2->CC[1], NRF_TIMER2->CC[3] };
    faba_trace_record(FABA_TX_START, state, (uint8_t *)ticks, 96);
    /* All existing TX helpers program WINDOWGRID; log the first actual config. */
    if (!config_saved) {
        uint32_t config[3] = { NRF_NFCT->FRAMEDELAYMIN, NRF_NFCT->FRAMEDELAYMAX,
                              NRF_NFCT->FRAMEDELAYMODE };
        faba_trace_record(FABA_TIMING_CONFIG, state, (uint8_t *)config, 96);
        config_saved = true;
    }
}

void faba_timing_tx_end(uint8_t state) {
    if (!running || !faba_trace_active()) return;
    uint32_t ticks[3] = { NRF_TIMER2->CC[0], NRF_TIMER2->CC[1], NRF_TIMER2->CC[2] };
    faba_trace_record(FABA_TX_DONE, state, (uint8_t *)ticks, 96);
}
