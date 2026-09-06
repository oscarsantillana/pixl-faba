#!/usr/bin/env python3
"""Compile the real player lifecycle with mocked hardware and slot boundaries."""
from pathlib import Path
import re, subprocess,tempfile
root=Path(__file__).resolve().parents[1];mod=root/'firmware/pixl-faba/fw/application/src/mod'
source=(mod/'faba_player.c').read_text(); source=re.sub(r'^#include.*\n','',source,flags=re.M)
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "faba_player_template.h"
void faba_diag_begin(uint16_t id){(void)id;}
void faba_diag_phase(uint32_t phase){(void)phase;}
typedef struct {uint8_t prefix[273];uint8_t memory[];} nfc_tag_mf0_ntag_information_t;
typedef struct {unsigned free_biggest_size,free_size;} mui_mem_monitor_t;
typedef struct {uint16_t length;uint8_t *buffer;uint16_t *crc;} tag_data_buffer_t;
typedef int tag_specific_type_t;
typedef struct {void *cb;} nfc_tag_14a_handler_t;
#define TAG_TYPE_NTAG_213 1100
static bool recording,playing,sensing,irq,timer,fail_timer,fail_alloc,low_memory,undefined_slot;
static uint16_t timeout=300;
static unsigned outstanding;
static uint8_t saved_slot[4500];
static uint8_t *active_buffer=saved_slot;
static tag_data_buffer_t slot={sizeof(saved_slot),saved_slot,NULL};
bool faba_trace_active(void){return recording;}
void mui_mem_monitor(mui_mem_monitor_t *m){m->free_biggest_size=m->free_size=low_memory?0:12000;}
void *mui_mem_malloc(size_t n){if(fail_alloc)return NULL;outstanding++;return malloc(n);}
void mui_mem_free(void *p){assert(!sensing);assert(p!=active_buffer);outstanding--;free(p);}
void tag_emulation_sense_end(void){sensing=false;}
void hal_nfc_set_nrfx_irq_enable(bool b){irq=b;}
bool faba_timing_start(void){assert(!sensing);if(fail_timer)return false;timer=true;return true;}
void faba_timing_stop(void){assert(!sensing);timer=false;}
void nfc_tag_mf0_ntag_data_loadcb(int t,tag_data_buffer_t *b){assert(t==1100&&b->length==497);active_buffer=b->buffer;}
uint16_t nrf_pwr_mgmt_get_timeout(void){return timeout;}
void nrf_pwr_mgmt_set_timeout(uint16_t t){timeout=t;}
void nrf_pwr_mgmt_feed(void){}
void faba_compat_set_playing(bool b){playing=b;}
void nfc_tag_14a_sense_switch(bool b){if(b)assert(playing&&irq&&timer);sensing=b;}
void nfc_tag_14a_set_handler(nfc_tag_14a_handler_t *h){(void)h;assert(!sensing);active_buffer=NULL;}
int tag_helper_get_active_tag_type(void){return undefined_slot?0:1100;}
tag_data_buffer_t *get_buffer_by_tag_type(int t){return t?&slot:NULL;}
bool tag_emulation_load_by_buffer(int t,bool update_crc){assert(t==1100&&!update_crc);active_buffer=saved_slot;return false;}
'''
main=r'''
int main(void){
 memset(saved_slot,0xab,sizeof(saved_slot));
 unsigned ids[]={IDS};
 for(unsigned n=0;n<sizeof(ids)/sizeof(*ids);n++){
  assert(faba_player_start(ids[n]));assert(timeout==0&&sensing&&playing&&outstanding==1);
  assert(!faba_player_start(ids[n]));
  char text[15];snprintf(text,sizeof(text),"02190530%04u00",ids[n]);
  assert(!memcmp(active_buffer+303,text,14));
  for(unsigned i=0;i<497;i++)if(i<311||i>314)assert(active_buffer[i]==faba_player_template[i]);
  active_buffer[460]^=0xff; /* reader writes or counters belong to private RAM */
  faba_player_stop();assert(timeout==300&&!sensing&&!playing&&!irq&&!timer&&!outstanding);
  assert(active_buffer==saved_slot);for(unsigned i=0;i<sizeof(saved_slot);i++)assert(saved_slot[i]==0xab);
  faba_player_stop();assert(timeout==300);
 }
 assert(!faba_player_start(10000));
 recording=true;assert(!faba_player_start(443));recording=false;
 low_memory=true;assert(!faba_player_start(443));low_memory=false;
 fail_alloc=true;assert(!faba_player_start(443));fail_alloc=false;
 fail_timer=true;assert(!faba_player_start(443));fail_timer=false;
 assert(!outstanding&&timeout==300&&!playing&&!sensing&&active_buffer==saved_slot);
 undefined_slot=true;assert(faba_player_start(443));faba_player_stop();assert(active_buffer==NULL&&!outstanding);
 puts("PASS: all 257 NDEF IDs, private data isolation, saved-slot restoration, power/IRQ/timer cleanup, repeated stop and start, invalid IDs and failure paths");
}
'''
import json
ids=[int(e['id']) for e in json.loads((root/'catalog/entries.json').read_text())]
with tempfile.TemporaryDirectory() as temp:
 p=Path(temp);(p/'player.c').write_text(prefix+source+main.replace('IDS',','.join(map(str,ids))))
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-fsanitize=address,undefined',f'-I{mod}',str(p/'player.c'),'-o',str(p/'player')],check=True)
 subprocess.run([str(p/'player')],check=True)
