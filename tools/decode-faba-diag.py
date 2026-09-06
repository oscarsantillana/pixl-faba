#!/usr/bin/env python3
"""Decode an immutable Pixl report against its matching application binary."""
import argparse,json
from pathlib import Path
p=argparse.ArgumentParser();p.add_argument('report',type=Path);p.add_argument('--binary',type=Path);a=p.parse_args()
r=json.loads(a.report.read_text());assert r['magic']==0x37444246
flags=r['flags'];phase=r['phase']&255
out={'story':r['story'],'phase':{0:'empty',1:'starting',2:'playing',3:'stopped'}.get(phase,phase),'previous_boot':bool(flags&1),'normal_app_kill':bool(flags&2),'fault_recorded':bool(flags&4),'reset_reason_hex':hex(r['reset_reason']),'input_count':r['input_count'],'free_bytes_at_start':r['free_start'],'largest_block_at_start':r['biggest_start'],'fault_id_hex':hex(r['fault_id'])}
if r['input_count']:
 out['last_input']={'key':{0:'left',1:'center',2:'right',3:'back'}.get(r['last_input']&255,'unknown'),'event':{0:'press',1:'release',2:'short',3:'long',4:'repeat'}.get((r['last_input']>>8)&255,'unknown')}
if phase==3:out['stop_after_seconds']=((r['end_tick']-r['start_tick'])&0xffffff)/16384
if flags&32 and phase==2 and not flags&16:out['queue_overflow_count']=r['free_stop']
if flags&16:
 out['cpu_cfsr_hex']=hex(r['free_stop']);out['instruction_pc_hex']=hex(r['fault_pc']);out['saved_lr_hex']=hex(r['fault_info'])
elif flags&8:
 out['source_line']=r['phase']>>8
 out['source_file_pointer_hex']=hex(r['fault_pc'])
 if a.binary:
  binary=a.binary.read_bytes()
  def text_at(address):
   offset=address-0x19000
   if not 0<=offset<len(binary):return '(outside application image)'
   return binary[offset:offset+512].split(b'\0',1)[0].decode('utf-8',errors='replace')
  out['source_file']=text_at(r['fault_pc'])
  if r['fault_id']==0xf0000001:out['assert_expression']=text_at(r['fault_info'])
 if r['fault_id']==0xf0000006:out['failed_allocation_bytes']=r['fault_info']
 elif r['fault_id']!=0xf0000001:out['error_code']=r['fault_info']
print(json.dumps(out,indent=2,ensure_ascii=False))
