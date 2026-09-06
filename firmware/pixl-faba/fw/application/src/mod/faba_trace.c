#include "faba_trace.h"
#include <string.h>

_Static_assert(sizeof(faba_trace_header_t) == 72, "trace header ABI");
_Static_assert(sizeof(faba_trace_event_t) == 24, "trace event ABI");

static faba_trace_t *trace;
static faba_trace_header_t last_header;
static volatile bool recording;
static volatile bool playing;
void faba_compat_set_playing(bool enabled) { playing = enabled; }

bool faba_trace_start(const faba_trace_header_t *metadata) {
    if (trace) return false; /* Never discard a pending capture. */
    trace = faba_trace_alloc(sizeof(*trace));
    if (!trace) return false;
    recording = false;
    memset(trace, 0, sizeof(*trace));
    trace->header = *metadata;
    memcpy(trace->header.magic, "FABATRC1", 8);
    trace->header.version = 1;
    trace->header.event_size = sizeof(faba_trace_event_t);
    trace->header.count = 0;
    trace->header.dropped = 0;
    if (trace->header.uid_size > sizeof(trace->header.uid))
        trace->header.uid_size = sizeof(trace->header.uid);
    trace->header.firmware[sizeof(trace->header.firmware) - 1] = 0;
    recording = true;
    return true;
}

static void reverse_events(unsigned first, unsigned end) {
    while (first < end && first < --end) {
        faba_trace_event_t tmp = trace->events[first];
        trace->events[first++] = trace->events[end];
        trace->events[end] = tmp;
    }
}

void faba_trace_stop(void) {
    bool was_recording = recording;
    recording = false; /* ISR must stop writing before rotating the saved tail. */
    if (!was_recording || !trace || trace->header.reserved[1] != 1 || !trace->header.dropped) return;
    unsigned rotate = trace->header.dropped % (FABA_TRACE_CAPACITY - 32);
    reverse_events(32, 32 + rotate);
    reverse_events(32 + rotate, FABA_TRACE_CAPACITY);
    reverse_events(32, FABA_TRACE_CAPACITY);
}
bool faba_trace_active(void) { return recording; }
const faba_trace_t *faba_trace_get(void) { return trace; }
const faba_trace_header_t *faba_trace_header(void) { return trace ? &trace->header : &last_header; }
void faba_trace_release(void) {
    recording = false;
    if (!trace) return;
    last_header = trace->header;
    faba_trace_free(trace);
    trace = NULL;
}
size_t faba_trace_size(void) {
    return trace ? sizeof(trace->header) + trace->header.count * sizeof(faba_trace_event_t) : 0;
}

void faba_trace_record(uint8_t kind, uint8_t state, const uint8_t *data, uint16_t bits) {
    if (!recording) return;
    unsigned index = trace->header.count;
    if (index == FABA_TRACE_CAPACITY) {
        if (trace->header.dropped == UINT32_MAX) return;
        if (trace->header.reserved[1] != 1) {
            trace->header.dropped++;
            return;
        }
        index = 32 + trace->header.dropped % (FABA_TRACE_CAPACITY - 32);
        trace->header.dropped++;
    } else {
        trace->header.count++;
    }
    faba_trace_event_t *event = &trace->events[index];
    memset(event, 0, sizeof(*event));
    event->cycles = faba_trace_cycles();
    event->rtc = faba_trace_rtc();
    event->kind = kind;
    event->state = state;
    event->bits = bits;
    size_t bytes = ((size_t)bits + 7) / 8;
    if (bytes > sizeof(event->data)) bytes = sizeof(event->data);
    if (data && bytes) memcpy(event->data, data, bytes);
}

/* Opt-in compatibility for the captured original-Faba sequence. Its READ07
 * arrives in READY after SELECT CL1. Ordinary Card Emulator and Phone control
 * keep the original selection rules. CRC validation remains at the caller. */
bool faba_compat_early_read(const uint8_t *data, uint16_t bits) {
    return (playing || (recording && trace && trace->header.reader == 0 &&
        trace->header.tag_type == 1100 /* NTAG213 */)) &&
        bits == 32 && data && data[0] == 0x30 && data[1] == 0x07;
}
