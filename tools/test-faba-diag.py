#!/usr/bin/env python3
"""Exercise the actual retained report, with only timer/heap boundaries mocked."""
from pathlib import Path
import subprocess,tempfile
root=Path(__file__).resolve().parents[1]
src=root/'firmware/pixl-faba/fw/application/src/mod'
with tempfile.TemporaryDirectory() as d:
 p=Path(d)
 (p/'app_timer.h').write_text('#include <stdint.h>\nuint32_t app_timer_cnt_get(void);\n')
 (p/'mui_mem.h').write_text('typedef struct {unsigned free_size,free_biggest_size;} mui_mem_monitor_t;\nvoid mui_mem_monitor(mui_mem_monitor_t *m);\n')
 (p/'main.c').write_text(r'''
#include "faba_diag.h"
#include "mui_mem.h"
#include <assert.h>
#include <stdio.h>
#include <signal.h>
#include <unistd.h>
#include <sys/wait.h>
static unsigned ticks=100, heap=9000;
uint32_t app_timer_cnt_get(void){return ticks;}
void mui_mem_monitor(mui_mem_monitor_t*m){m->free_size=heap;m->free_biggest_size=heap-100;}
static void oom_abort(int sig){
 (void)sig;const volatile faba_diag_t *r=faba_diag_report();
 _Exit(r->fault_id==0xf0000006u && r->fault_info==4096 && (r->phase>>8)==99 ? 0:2);
}
int main(void){
 const volatile faba_diag_t *r=faba_diag_report();
 faba_diag_boot(0);assert(sizeof(*r)==64&&r->story==0);
 faba_diag_begin(443);assert(r->phase==1&&r->free_start==9000);
 faba_diag_phase(2);ticks+=163840;faba_diag_input(0x103);
 assert(r->input_count==1&&r->last_input==0x103);
 heap=6000;faba_diag_phase(3);faba_diag_kill(10);
 assert(r->flags==2&&r->phase==3&&r->free_stop==6000);
 faba_diag_input(5);assert(r->input_count==1);
 faba_diag_begin(400);faba_diag_phase(2);faba_diag_fault(1,123,456);
 faba_diag_fault(999,999,999);assert(r->fault_id==1&&r->fault_pc==123);
 faba_diag_cpu_status(0x10000);assert(r->free_stop==0x10000&&(r->flags&16));
 faba_diag_location(1234);assert((r->phase >> 8)==1234);
 faba_diag_begin(400);faba_diag_phase(2);faba_diag_fault(1,123,456);
 faba_diag_boot(4);assert(r->flags==5&&r->phase==2&&r->reset_reason==4&&r->fault_pc==123);
 faba_diag_input(8);faba_diag_phase(3);faba_diag_kill(10);
 assert(r->flags==5&&r->phase==2&&r->input_count==0);
 faba_diag_begin(123);assert(r->flags==0&&r->fault_id==0);
 faba_diag_phase(2);faba_diag_boot(8);assert(r->flags==1&&r->phase==2);
 faba_diag_begin(400);faba_diag_phase(2);faba_diag_queue_overflow();faba_diag_queue_overflow();
 assert((r->flags&32)&&r->free_stop==2);
 pid_t child=fork();assert(child>=0);
 if(child==0){signal(SIGABRT,oom_abort);faba_diag_memory_full((const char*)0x1234,99,4096);_Exit(3);}
 int status;assert(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 puts("Retained diagnostic: stop, input, kill, fault, reboot and freeze tests pass");
}
''')
 (p/'diag.c').write_text((src/'faba_diag.c').read_text().replace('__attribute__((section(".faba_diag")))',''))
 subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-I'+d,'-I'+str(src),str(p/'diag.c'),str(p/'main.c'),'-o',str(p/'test')],check=True)
 subprocess.run([str(p/'test')],check=True)
