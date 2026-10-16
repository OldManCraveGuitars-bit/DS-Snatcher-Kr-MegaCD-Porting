"""Compile original executable opening callbacks into native resource actions.

Only the original resource and SUBCODE instruction bytes are executed. Export
calls become explicit engine jobs. No recording, framebuffer, saved gameplay
RAM or observed event schedule is an input. Requires the Unicorn CPU library.
"""
import struct,hashlib
from convert_opening_timeline import callbacks
from unicorn import Uc,UC_ARCH_M68K,UC_MODE_BIG_ENDIAN,UC_HOOK_CODE,UC_HOOK_MEM_WRITE
from unicorn.m68k_const import UC_CPU_M68K_M68000,UC_M68K_REG_SR,UC_M68K_REG_A7,UC_M68K_REG_PC,UC_M68K_REG_D0,UC_M68K_REG_D7,UC_M68K_REG_A1

def build(source,name='DATA_D1.BIN',track=3,file_id=3):
    read=lambda n:source.read_file(n) if hasattr(source,'read_file') else (source/n).read_bytes()
    data=read(name);sub=read('SUBCODE.BIN');function,events=callbacks(data)
    cpu=Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN);cpu.ctl_set_cpu_model(UC_CPU_M68K_M68000)
    cpu.mem_map(0,0x100000);cpu.mem_map(0xff8000,4096);cpu.mem_write(0xd400,sub);cpu.mem_write(0x28000,data)
    version=3 if track==4 and file_id==2 else 1
    cd_status=0
    actions=[];all_actions=[];counts={};pcs=set()
    def returned(emu):
        sp=emu.reg_read(UC_M68K_REG_A7);pc=struct.unpack('>I',emu.mem_read(sp,4))[0]
        emu.reg_write(UC_M68K_REG_A7,sp+4);emu.reg_write(UC_M68K_REG_PC,pc)
    def code(emu,pc,size,user):
        pcs.add(pc)
        if version>=2 and pc==0xe30a:
            actions.append((7,0,0,emu.reg_read(UC_M68K_REG_D0)&65535));counts[7]=counts.get(7,0)+1
            returned(emu);return
        if version>=2 and pc==0xe2fe:
            emu.reg_write(UC_M68K_REG_D0,(emu.reg_read(UC_M68K_REG_D0)&0xffff0000)|cd_status)
            returned(emu);return
        exports={0xf5e8:1,0xf68e:2,0xe41a:3,0xf3f8:4}
        if pc in exports:
            kind=exports[pc]
            # Clear has no D0 argument; its incoming register is dead state.
            value=0 if kind==1 else emu.reg_read(UC_M68K_REG_A1) if kind==3 else emu.reg_read(UC_M68K_REG_D0)&65535
            actions.append((kind,0,0,value));counts[kind]=counts.get(kind,0)+1
            # Enqueue's successful MOVEQ#0,D7 updates native condition codes.
            if kind==3:emu.reg_write(UC_M68K_REG_D7,0);emu.reg_write(UC_M68K_REG_SR,(emu.reg_read(UC_M68K_REG_SR)&~15)|4)
            returned(emu)
    def write(emu,kind,at,size,value,user):
        if 0x7000<=at<0x7800:return
        if version>=2 and at==0xff800f:
            pc=emu.reg_read(UC_M68K_REG_PC)
            if bytes(emu.mem_read(pc,8))!=bytes.fromhex('08f9000400ff800f'):
                raise ValueError(('Unsupported original MMIO operation',hex(pc)))
            actions.append((6,1,at,16));counts[6]=counts.get(6,0)+1
        elif 0x7800<=at<0x10000 or version>=2 and 0xb6000<=at<0xb6500:
            assert size in (1,2,4);actions.append((5,size,at,value));counts[5]=counts.get(5,0)+1
        else:raise ValueError(('Unexpected original callback write',hex(at),size,hex(value)))
    cpu.hook_add(UC_HOOK_CODE,code);cpu.hook_add(UC_HOOK_MEM_WRITE,write)
    initializers=[]
    if version==3:
        handler=0x28000+function+6+struct.unpack_from('>h',data,function+6)[0]
        initializers=[dict(index=0,clock_bcd=0,handler=handler,phase=phase) for phase in (0,1)]
    compiled=[]
    for event in [*events,*initializers]:
        variants=[]
        for cd_status in ((0,0x100) if version>=2 else (0,)):
            actions=[];cpu.reg_write(UC_M68K_REG_SR,0x2700);cpu.reg_write(UC_M68K_REG_A7,0x7400)
            cpu.mem_write(0x7896,struct.pack('>H',65535 if 'phase' in event else event['index']+1))
            cpu.mem_write(0x7898,struct.pack('>H',event.get('phase',0)))
            cpu.mem_write(0x7400,struct.pack('>I',0x1000));cpu.emu_start(event['handler'],0x1000,count=200000)
            if cpu.reg_read(UC_M68K_REG_PC)!=0x1000:raise ValueError(('Original callback did not return',name,event))
            for kind,_,_,value in actions:
                if kind==2:assert value<32
                if kind==3:assert 0x28000<=value<=0x28000+len(data)-4
            variants.append(actions)
        if len(variants)==2 and variants[0]!=variants[1]:
            waiting,ready=variants
            # Explicit original CD-status branch, retaining both execution
            # paths and their callback-index/clock writes for runtime.
            compiled.append([(8,0,0x100,len(ready)|(len(waiting)<<16)),*ready,*waiting])
        else:compiled.append(variants[0])
    all_actions=compiled[:len(events)];initial_actions=compiled[len(events):]
    header_size=40 if version==3 else 32
    payload=bytearray(header_size+(len(events)+len(initializers))*16)
    for i,(event,jobs) in enumerate(zip([*events,*initializers],compiled)):
        offset=len(payload);struct.pack_into('<IIII',payload,header_size+i*16,event['clock_bcd'],len(jobs),offset,event['handler'])
        for job in jobs:payload+=struct.pack('<HHII',*job)
    resource_offset=len(payload);payload+=data
    struct.pack_into('<4sHHIIIIII',payload,0,b'NSOP',version,track,file_id,len(events),header_size,resource_offset,len(data),0x28000)
    if version==3:struct.pack_into('<II',payload,32,header_size+len(events)*16,len(initializers))
    report=dict(kind='original_executable_opening_callback_actions',file=name,track=track,
        source_sha256=hashlib.sha256(data).hexdigest(),subcode_sha256=hashlib.sha256(sub).hexdigest(),
        bytes=len(payload),sha256=hashlib.sha256(payload).hexdigest(),callbacks=len(events),
        jobs=sum(map(len,all_actions)),job_counts=counts,executed_original_pcs=len(pcs),version=version,
        callback_actions=[dict(index=i,cd_frame=e['cd_frame'],handler=e['handler'],actions=a) for i,(e,a) in enumerate(zip(events,all_actions))],
        initializers=[dict(phase=e['phase'],handler=e['handler'],actions=a) for e,a in zip(initializers,initial_actions)],
        source_writes=0,recording_inputs=False,game_state_inputs=False,
        limits=['Original callback export calls compiled as explicit jobs; native palette/scene/actor engines execute these jobs',
                'Full animation/native GPU scheduling must be verified separately'])
    return bytes(payload),report
