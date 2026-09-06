#include "faba_trace.h"
#include "nrf.h"
#include "nrfx_ppi.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
fake_timer_t timer;
fake_nfct_t nfct;
static unsigned step, fail_step, owned, enabled, records;
static bool active;
static uint8_t kinds[4];
static uint32_t payloads[4][3];
static int next(void) { return ++step == fail_step ? 1 : 0; }
int nrfx_ppi_channel_alloc(nrf_ppi_channel_t *p) {
 if (next()) return 1;
 *p=owned; owned++; return 0;
}
int nrfx_ppi_channel_assign(nrf_ppi_channel_t c, uint32_t e, uint32_t t) {
 assert(c<owned); assert(e && t); return next();
}
int nrfx_ppi_channel_enable(nrf_ppi_channel_t c) {
 assert(c<owned); if(next()) return 1; enabled |= 1u<<c; return 0;
}
int nrfx_ppi_channel_free(nrf_ppi_channel_t c) {
 assert(owned && c==owned-1); owned--; enabled &= ~(1u<<c); return 0;
}
bool faba_trace_active(void) { return active; }
void faba_trace_record(uint8_t kind, uint8_t state, const uint8_t *data, uint16_t bits) {
 assert(state==2 && bits==96 && records<4);
 kinds[records]=kind; memcpy(payloads[records++],data,12);
}
int main(void) {
 for(unsigned fail=1;fail<=9;fail++) {
  step=0; fail_step=fail; assert(!faba_timing_start());
  assert(!owned && !enabled); faba_timing_stop();
 }
 step=0; fail_step=0; assert(faba_timing_start());
 assert(owned==3 && enabled==7 && timer.BITMODE==3 && timer.PRESCALER==0);
 assert(!faba_timing_start());
 timer.CC[0]=0xfffffff0; timer.CC[1]=0x550; timer.CC[2]=0x1110; timer.CC[3]=0x590;
 nfct.FRAMEDELAYMIN=1232; nfct.FRAMEDELAYMAX=4096; nfct.FRAMEDELAYMODE=3;
 faba_timing_tx_start(2); assert(records==0);
 active=true; faba_timing_tx_start(2); faba_timing_tx_end(2); faba_timing_tx_start(2);
 assert(records==4 && kinds[0]==FABA_TX_START && kinds[1]==FABA_TIMING_CONFIG && kinds[2]==FABA_TX_DONE);
 assert(payloads[0][0]==0xfffffff0 && payloads[0][1]==0x550 && payloads[0][2]==0x590);
 assert(payloads[1][0]==1232 && payloads[1][1]==4096 && payloads[1][2]==3);
 assert(payloads[2][2]==0x1110);
 faba_timing_stop(); assert(!owned && !enabled && timer.TASKS_STOP && timer.TASKS_SHUTDOWN);
 faba_timing_stop(); faba_timing_tx_start(2); assert(records==4);
 puts("PASS: PPI allocation/assignment/enable failures release resources; hardware capture payloads, config once, inactive guards, stop cleanup");
}
