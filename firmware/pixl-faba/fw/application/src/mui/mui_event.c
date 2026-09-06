#include "mui_event.h"
#include "app_util_platform.h"
#include "faba_diag.h"
#include <assert.h>

void mui_event_queue_init(mui_event_queue_t *q) {
    /* One bounded allocation at UI initialization. Posting from an interrupt
     * must never allocate from the same heap the main loop is using. */
    q->items = mui_mem_malloc(MAX_EVENT_MSG * sizeof(*q->items));
    assert(q->items);
    q->head = q->count = q->pending = 0;
}
void mui_event_set_callback(mui_event_queue_t *q, mui_event_handler_t cb, void *ctx) {
    q->dispatcher = cb; q->dispatch_context = ctx;
}
static uint8_t pending_bit(uint32_t id) {
    return id == MUI_EVENT_ID_REDRAW ? 1 : id == MUI_EVENT_ID_ANIM ? 2 : 0;
}
void mui_event_post(mui_event_queue_t *q, mui_event_t *event) {
    uint8_t bit = pending_bit(event->id);
    CRITICAL_REGION_ENTER();
    if (!(bit && (q->pending & bit))) {
        if (q->count < MAX_EVENT_MSG) {
            q->items[(q->head + q->count) % MAX_EVENT_MSG] = *event;
            q->count++; q->pending |= bit;
        } else {
            faba_diag_queue_overflow();
        }
    }
    CRITICAL_REGION_EXIT();
}
void mui_event_dispatch(mui_event_queue_t *q) {
    for (;;) {
        mui_event_t event;
        bool have_event;
        CRITICAL_REGION_ENTER();
        have_event = q->count != 0;
        if (have_event) {
            event = q->items[q->head];
            q->head = (q->head + 1) % MAX_EVENT_MSG;
            q->count--; q->pending &= (uint8_t)~pending_bit(event.id);
        }
        CRITICAL_REGION_EXIT();
        if (!have_event) break;
        /* UI callbacks can post events and allocate memory; run unlocked. */
        q->dispatcher(q->dispatch_context, &event);
    }
}
void mui_event_dispatch_now(mui_event_queue_t *q, mui_event_t *event) {
    q->dispatcher(q->dispatch_context, event);
}
