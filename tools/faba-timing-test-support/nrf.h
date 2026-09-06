#include <stdint.h>
typedef struct { uint32_t TASKS_STOP, TASKS_START, TASKS_SHUTDOWN, TASKS_CLEAR;
uint32_t INTENCLR, SHORTS, MODE, BITMODE, PRESCALER, TASKS_CAPTURE[4], CC[4], EVENTS_COMPARE[4]; } fake_timer_t;
typedef struct { uint32_t EVENTS_RXFRAMEEND, EVENTS_TXFRAMESTART, EVENTS_TXFRAMEEND;
uint32_t FRAMEDELAYMIN, FRAMEDELAYMAX, FRAMEDELAYMODE; } fake_nfct_t;
extern fake_timer_t timer;
extern fake_nfct_t nfct;
#define NRF_TIMER2 (&timer)
#define NRF_NFCT (&nfct)
#define TIMER_MODE_MODE_Timer 0
#define TIMER_BITMODE_BITMODE_32Bit 3
