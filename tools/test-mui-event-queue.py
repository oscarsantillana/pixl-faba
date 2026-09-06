#!/usr/bin/env python3
"""Stress the actual event queue; mock only platform locks/logging and heap."""
from pathlib import Path
import re,subprocess,tempfile
root=Path(__file__).resolve().parents[1];src=root/'firmware/pixl-faba/fw/application/src'
def strip(s):return re.sub(r'^#include.*\n','',s,flags=re.M)
header=strip((src/'mui/mui_event.h').read_text());code=strip((src/'mui/mui_event.c').read_text())
prefix=r'''
#include <assert.h>
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
static unsigned allocations,locked;
void faba_diag_queue_overflow(void){}
void *mui_mem_malloc(size_t n){allocations++;return malloc(n);}
void *mui_mem_realloc(void*p,size_t n){allocations++;return realloc(p,n);}
void mui_mem_free(void*p){free(p);}
#include "mlib_common.h"
#define NRF_LOG_WARNING(...) ((void)0)
#define CRITICAL_REGION_ENTER() { assert(!locked);locked=1;
#define CRITICAL_REGION_EXIT() locked=0; }
#define MUI_EVENT_ID_INPUT 0
#define MUI_EVENT_ID_REDRAW 1
#define MUI_EVENT_ID_ANIM 2
'''
main=r'''
static unsigned received,last;
static void dispatch(void *ctx,mui_event_t *e){
 (void)ctx;assert(!locked);assert(e->arg_int==received);last=e->arg_int;received++;
}
static void count_event(void*ctx,mui_event_t*e){(void)ctx;(void)e;assert(!locked);received++;}
static void repost(void*ctx,mui_event_t*e){
 assert(!locked);received++;
 if(e->arg_int==0){mui_event_t next={.id=MUI_EVENT_ID_INPUT,.arg_int=1};mui_event_post(ctx,&next);}
}
int main(void){
 mui_event_queue_t q={0};mui_event_queue_init(&q);unsigned initial=allocations;
 mui_event_set_callback(&q,dispatch,NULL);
 for(unsigned i=0;i<1000;i++){mui_event_t e={.id=MUI_EVENT_ID_INPUT,.arg_int=i};mui_event_post(&q,&e);}
 mui_event_dispatch(&q);
 printf("Overflow: delivered=%u, allocations initial=%u final=%u\n",received,initial,allocations);fflush(stdout);
 assert(received==MAX_EVENT_MSG);assert(allocations==initial);
 for(unsigned cycle=0;cycle<10000;cycle++){
  received=0;for(unsigned i=0;i<20;i++){mui_event_t e={.id=0,.arg_int=i};mui_event_post(&q,&e);}mui_event_dispatch(&q);assert(received==20);
 }
 mui_event_set_callback(&q,count_event,NULL);received=0;
 for(unsigned i=0;i<1000;i++){mui_event_t e={.id=MUI_EVENT_ID_REDRAW};mui_event_post(&q,&e);e.id=MUI_EVENT_ID_ANIM;mui_event_post(&q,&e);}
 mui_event_dispatch(&q);assert(received==2);
 mui_event_set_callback(&q,repost,&q);received=0;mui_event_t e={.id=0};mui_event_post(&q,&e);mui_event_dispatch(&q);assert(received==2);
 assert(allocations==initial);puts("PASS: bounded FIFO, overflow, wraparound, coalescing, reentrant callbacks and no runtime allocations");
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d);(p/'test.c').write_text(prefix+header+code+main)
 subprocess.run(['cc','-std=c11','-Wno-tautological-constant-out-of-range-compare','-I'+str(src/'mui'),'-I'+str(root/'firmware/pixl-faba/fw/components/mlib'),str(p/'test.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
