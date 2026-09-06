#include "faba_trace.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static uint32_t cycles = 0xfffffff0, rtc = 0x00fffffe;
static bool fail_allocation;
void *faba_trace_alloc(size_t size) { return fail_allocation ? NULL : malloc(size); }
void faba_trace_free(void *ptr) { free(ptr); }
uint32_t faba_trace_cycles(void) { cycles += 320; return cycles; }
uint32_t faba_trace_rtc(void) { return rtc++ & 0xffffff; }

int main(int argc, char **argv) {
    assert(argc == 2);
    faba_trace_header_t meta = { .cycle_hz = 64000000, .rtc_hz = 32768,
        .slot = 1, .tag_type = 1100, .uid_size = 7, .uid = {4,0xe7,0xdd,0x17,0xbc,0x2a,0x81} };
    strcpy(meta.firmware, "2.16.1-faba1");
    uint8_t req = 0x26, read[] = {0x30, 4, 0x26, 0xee}, response[16];
    memset(response, 0xa5, sizeof(response));
    fail_allocation = true;
    assert(!faba_trace_start(&meta));
    assert(!faba_trace_active());
    fail_allocation = false;
    assert(faba_trace_start(&meta));
    assert(!faba_trace_start(&meta));
    faba_trace_record(FABA_FIELD_ON, 0, NULL, 0);
    faba_trace_record(FABA_RX, 0, &req, 7);
    faba_trace_record(FABA_RX, 2, read, 32);
    faba_trace_record(FABA_TX_CRC, 2, response, 128);
    faba_trace_record(FABA_TX_DONE, 2, NULL, 0);
    faba_trace_stop();
    faba_trace_record(FABA_FIELD_OFF, 0, NULL, 0);
    const faba_trace_t *t = faba_trace_get();
    assert(t->header.count == 5);
    assert(t->events[1].data[0] == req && t->events[1].data[1] == 0);
    assert(t->events[3].bits == 128 && t->events[3].data[11] == 0xa5);
    FILE *out = fopen(argv[1], "wb"); assert(out);
    assert(fwrite(t, 1, faba_trace_size(), out) == 72 + 5*24); fclose(out);
    faba_trace_release();
    assert(faba_trace_header()->count == 5);
    assert(faba_trace_start(&meta));
    t = faba_trace_get();
    for (unsigned i = 0; i < FABA_TRACE_CAPACITY + 3; i++) faba_trace_record(FABA_RX, 0, &req, 7);
    assert(t->header.count == FABA_TRACE_CAPACITY && t->header.dropped == 3);
    assert(t->events[0].data[0] == 0x26 && t->events[127].data[0] == 0x26);
    faba_trace_release();
    assert(faba_trace_start(&meta));
    t = faba_trace_get();
    assert(t->header.count == 0 && t->header.dropped == 0);
    assert(t->events[0].data[0] == 0);
    faba_trace_stop();
    faba_trace_release();
    /* Preserve startup plus a chronological rolling tail, including multiple wraps. */
    meta.reserved[1] = 1;
    for (unsigned count = 128; count <= 421; count += 73) {
        assert(faba_trace_start(&meta));
        for (unsigned i = 0; i < count; i++) {
            uint32_t value = i;
            faba_trace_record(FABA_RX, 0, (uint8_t *)&value, 32);
        }
        faba_trace_stop();
        t = faba_trace_get();
        assert(t->header.count == 128 && t->header.dropped == count - 128);
        for (unsigned i = 0; i < 128; i++) {
            uint32_t value;
            memcpy(&value,t->events[i].data,4);
            assert(value == (i < 32 ? i : count - 96 + i - 32));
        }
        faba_trace_stop(); /* save/retry calls stop again; rotation must be idempotent. */
        uint32_t last; memcpy(&last,t->events[127].data,4); assert(last == count - 1);
        assert(!faba_compat_early_read((uint8_t[]){0x30,7,0xbd,0xdc},32));
        faba_trace_release();
    }
    puts("PASS: allocation failure, pending protection, capture, stop, bounds, overflow, reset and export");
    return 0;
}
