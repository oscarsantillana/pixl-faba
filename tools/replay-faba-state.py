#!/usr/bin/env python3
"""Replay real DMA frames through the actual firmware state function on the host.
Only the peripheral boundary and tag-memory callback are replaced. Source function
bodies and protocol types are extracted verbatim from the current firmware tree.
"""
from pathlib import Path
import importlib.util
import re
import subprocess
import tempfile
root=Path(__file__).resolve().parents[1]
base=root/'firmware/pixl-faba/fw'
hf=base/'components/chameleon-ultra/firmware/application/src/rfid/nfctag/hf'
src=(hf/'nfc_14a.c').read_text()
def function(name):
 m=re.search(r'^(?:inline )?(?:void|bool|uint8_t) '+name+r'\(',src,re.M)
 start=m.start(); brace=src.index('{',m.end());level=1;i=brace+1
 while level:
  level += (src[i]=='{')-(src[i]=='}');i+=1
 return src[start:i].replace('inline ','')
header=(hf/'nfc_14a.h').read_text().replace('#include "tag_emulation.h"','')
arrays='\n'.join(re.search(r'(?:const|static) (?:uint8_t|uint16_t) '+n+r'\[.*?\]\s*=\s*\{.*?\};',src,re.S).group() for n in ['ByteMirror','ats_fsdi_table','m_uid_incomplete_sak'])
spec=importlib.util.spec_from_file_location('decode',root/'tools/decode-faba-trace.py');dec=importlib.util.module_from_spec(spec);spec.loader.exec_module(dec)
fixtures=[]
for kind,filename in [('faba','faba-capture-01/F01.trace'),('phone','phone-capture-01/P01.trace')]:
 capture=dec.decode((root/'diagnostics/pixl-faba-timing-build'/filename).read_bytes())
 events=[e for e in capture['events'] if e['kind']=='RX' and e['valid_rx_length']]
 fixtures.append('static frame_t '+kind+'[] = {'+','.join('{'+str(e['bits'])+',{'+','.join('0x'+b for b in e['data'].split())+'}}' for e in events)+'};')
prefix='''#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "faba_trace.h"
#define NRF_LOG_INFO(...) ((void)0)
#define NFCT_RXD_AMOUNT_RXDATABITS_Msk 7
#define NFCT_RXD_AMOUNT_RXDATABYTES_Msk 0xff8
struct { struct { uint32_t AMOUNT; } RXD; } peripheral;
#define NRF_NFCT (&peripheral)
void calc_14a_crc_lut(uint8_t *data, size_t n, uint8_t *out) {
 uint16_t crc=0x6363;
 for(size_t i=0;i<n;i++) { crc^=data[i]; for(int j=0;j<8;j++) crc=(crc>>1)^((crc&1)?0x8408:0); }
 out[0]=crc; out[1]=crc>>8;
}
void *faba_trace_alloc(size_t n) { return malloc(n); }
void faba_trace_free(void *p) { free(p); }
uint32_t faba_trace_cycles(void) { return 0; }
uint32_t faba_trace_rtc(void) { return 0; }
'''
boundary='''
nfc_tag_14a_state_t m_tag_state_14a;
nfc_tag_14a_handler_t m_tag_handler;
static unsigned reads;
static nfc_tag_14a_uid_size uid_size=7;
static uint8_t uid[]={4,0xe7,0xdd,0x17,0xbc,0x2a,0x81},atqa[]={0x44,0},sak[]={0};
static nfc_14a_ats_t ats;
static nfc_tag_14a_coll_res_reference_t identity={&uid_size,atqa,sak,uid,&ats};
static nfc_tag_14a_coll_res_reference_t *identity_get(void) { return &identity; }
void nfc_tag_14a_tx_bytes(uint8_t *data,uint32_t n,bool crc) { (void)data;(void)n;(void)crc; }
void nfc_tag_14a_tx_nbit(uint8_t data,uint32_t n) { (void)data;(void)n; }
static void memory_cb(uint8_t *data,uint16_t bits) {
 if(bits==32 && data[0]==0x30) { assert(m_tag_state_14a==NFC_TAG_STATE_14A_ACTIVE); assert(nfc_tag_14a_checks_crc(data,4)); reads++; }
}
typedef struct { uint16_t bits; uint8_t data[12]; } frame_t;
'''
main='''
static unsigned replay(frame_t *frames,size_t n,uint8_t reader,uint16_t type,bool enabled) {
 faba_trace_header_t meta={.reader=reader,.tag_type=type};
 assert(faba_trace_start(&meta)); if(!enabled) faba_trace_stop();
 m_tag_state_14a=NFC_TAG_STATE_14A_IDLE; reads=0;
 for(size_t i=0;i<n;i++) { uint8_t data[257]={0};memcpy(data,frames[i].data,12);peripheral.RXD.AMOUNT=frames[i].bits;nfc_tag_14a_data_process(data); }
 faba_trace_release();return reads;
}
int main(void) {
 m_tag_handler.get_coll_res=identity_get;m_tag_handler.cb_state=memory_cb;
 unsigned f=replay(faba,sizeof(faba)/sizeof(*faba),0,1100,true);
 printf("Captured Faba early reads delivered: %u (expected3)\\n",f);fflush(stdout);assert(f==3);
 assert(replay(phone,sizeof(phone)/sizeof(*phone),1,1100,true)==6);
 assert(replay(faba,sizeof(faba)/sizeof(*faba),1,1100,true)==0);
 assert(replay(faba,sizeof(faba)/sizeof(*faba),0,1101,true)==0);
 assert(replay(faba,sizeof(faba)/sizeof(*faba),0,1100,false)==0);
 /* Catalog owns a private NTAG213 session without trace recording. */
 faba_compat_set_playing(true);
 assert(replay(faba,sizeof(faba)/sizeof(*faba),0,1100,false)==3);
 faba_compat_set_playing(false);
 assert(replay(faba,sizeof(faba)/sizeof(*faba),0,1100,false)==0);
 /* A valid CRC does not enable a different early-read address. */
 faba_trace_header_t meta={.reader=0,.tag_type=1100};assert(faba_trace_start(&meta));
 uint8_t other[]={0x30,8,0x4a,0x24};assert(nfc_tag_14a_checks_crc(other,4));
 assert(!faba_compat_early_read(other,32));
 assert(!faba_compat_early_read(NULL,32));
 assert(!faba_compat_early_read(other,16));
 faba_trace_release();
 /* Corrupt every READ CRC in a copy of the real Faba capture. */
 for(size_t i=0;i<sizeof(faba)/sizeof(*faba);i++) if(faba[i].bits==36 && faba[i].data[0]==0x30) faba[i].data[3]^=0x10;
 assert(replay(faba,sizeof(faba)/sizeof(*faba),0,1100,true)==0);
 puts("PASS: actual state-machine replay, phone cascade path, mode/type guards and corrupt CRC rejection");
}
'''
code=prefix+header+arrays+boundary+'\n'.join(function(n) for n in ['nfc_tag_14a_create_bcc','nfc_tag_14a_append_bcc','nfc_tag_14a_checks_crc','nfc_tag_14a_unwrap_frame','nfc_tag_14a_data_process'])+'\n'+'\n'.join(fixtures)+main
with tempfile.TemporaryDirectory(prefix='faba-state-replay-') as tmp:
 p=Path(tmp);(p/'replay.c').write_text(code)
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined','-I',str(base/'application/src/mod'),str(p/'replay.c'),str(base/'application/src/mod/faba_trace.c'),'-o',str(p/'replay')],check=True)
 subprocess.run([str(p/'replay')],check=True)
