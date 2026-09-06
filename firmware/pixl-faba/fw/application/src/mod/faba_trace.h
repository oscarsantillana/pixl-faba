#ifndef FABA_TRACE_H
#define FABA_TRACE_H

#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>

#define FABA_TRACE_CAPACITY 128
#define FABA_TRACE_DATA_SIZE 12

enum {
    FABA_FIELD_ON = 1, FABA_FIELD_OFF, FABA_RX,
    FABA_TX_CRC, FABA_TX_BYTES, FABA_TX_BITS,
    FABA_TX_DONE, FABA_ERROR, FABA_RX_RESULT, FABA_RX_ERROR,
    FABA_TX_START, FABA_TIMING_CONFIG
};

typedef struct {
    uint32_t cycles;
    uint32_t rtc;
    uint16_t bits;
    uint8_t kind;
    uint8_t state;
    uint8_t data[FABA_TRACE_DATA_SIZE];
} faba_trace_event_t;

typedef struct {
    char magic[8];
    uint16_t version;
    uint16_t event_size;
    uint32_t count;
    uint32_t dropped;
    uint32_t cycle_hz;
    uint32_t rtc_hz;
    uint8_t reader; /* 0 = Faba, 1 = phone control. */
    uint8_t slot;   /* zero based */
    uint8_t uid_size;
    uint8_t sak;
    uint16_t tag_type;
    uint8_t atqa[2];
    uint8_t uid[10];
    uint8_t reserved[2];
    char firmware[24];
} faba_trace_header_t;

typedef struct {
    faba_trace_header_t header;
    faba_trace_event_t events[FABA_TRACE_CAPACITY];
} faba_trace_t;

/* Start with NFC sensing stopped; stop recording before deinitializing NFC.
 * Serialize/release only after deinitialization. Single ISR producer; no
 * allocation, file I/O or text formatting in ISR. */
bool faba_trace_start(const faba_trace_header_t *metadata);
void faba_trace_stop(void);
void faba_trace_release(void);
bool faba_trace_active(void);
void faba_compat_set_playing(bool enabled);
bool faba_compat_early_read(const uint8_t *data, uint16_t bits);
void faba_trace_record(uint8_t kind, uint8_t state, const uint8_t *data, uint16_t bits);
const faba_trace_t *faba_trace_get(void);
const faba_trace_header_t *faba_trace_header(void);
size_t faba_trace_size(void);
uint32_t faba_trace_cycles(void);
uint32_t faba_trace_rtc(void);
void *faba_trace_alloc(size_t size);
void faba_trace_free(void *ptr);

/* Device adapter. Saved traces have unique names; failed saves retain RAM. */
bool faba_trace_begin(uint8_t reader);
int32_t faba_trace_save(void);
const char *faba_trace_last_path(void);
void faba_record_finish(void);
void faba_trace_path_reset(void);
bool faba_timing_start(void);
void faba_timing_stop(void);
void faba_timing_tx_start(uint8_t state);
void faba_timing_tx_end(uint8_t state);

#endif
