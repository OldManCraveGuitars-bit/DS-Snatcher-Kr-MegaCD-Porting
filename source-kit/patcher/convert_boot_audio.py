"""Generate startup PCM by executing the user's BIOS and original sound driver.

No recordings, saved states, preconverted sounds or reference outputs are read.
The original player's declared isolated driver timing is retained exactly.
"""
from pathlib import Path
import ctypes as c
import hashlib
import re
import struct
import subprocess
from convert_resource_archive import bootstrap


class Event(c.Structure):
    _fields_ = [('clock', c.c_uint64), ('port', c.c_uint8), ('data', c.c_uint8)]


def build(source, bios_path, dependencies, workspace, compiler, cpu_core=None):
    deps, work = Path(dependencies), Path(workspace)
    work.mkdir(exist_ok=False)
    bios = Path(bios_path).read_bytes()
    if hashlib.sha256(bios).hexdigest() != 'b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4':
        raise ValueError('Unsupported Sega CD USA BIOS; see supported input hashes')
    boot, _ = bootstrap(source)
    security, ip = boot['native-boot-security.bin'], boot['native-boot-ip.bin']
    fm = source.read_file('FMWR_1.BIN')
    library = work / 'boot.dll'
    command = [str(compiler), '-O2', '-std=gnu99', '-shared',
               str(deps/'tools/native_sega_z80_r22.c'),
               str(deps/'tools/native_fm_synth_r22.c'),
               str(deps/'third_party/Nuked-OPN2/ym3438.c'), '-o', str(library)]
    from host_helpers import prepared
    if not prepared('boot',library,source):
        result = subprocess.run(command, capture_output=True, text=True)
        (work/'build.log').write_text(result.stdout+result.stderr, encoding='utf8')
        result.check_returncode()
    lib = c.CDLL(str(library.resolve()))
    lib.native_sega_trace.argtypes = [c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t,
                                      c.c_uint,c.POINTER(Event),c.c_size_t,c.c_void_p]
    lib.native_fm_render.argtypes = [c.POINTER(Event),c.c_size_t,c.c_void_p,c.c_size_t]
    events = (Event*400000)()
    count = lib.native_sega_trace(bios,len(bios),security,len(security),363,events,len(events),None)
    if count <= 0:
        raise ValueError(('Original BIOS execution failed',count))
    fps = 53693175/896040

    def render(rows, frames):
        samples = round(frames*32000/fps)
        output = c.create_string_buffer(samples*4)
        consumed = lib.native_fm_render(rows,len(rows),output,samples)
        if consumed != len(rows):
            raise ValueError(('Unconsumed startup sound commands',consumed,len(rows)))
        return output.raw

    sega = render((Event*count)(*events[:count]),363)
    from unicorn import Uc, UC_ARCH_M68K, UC_MODE_BIG_ENDIAN, UC_HOOK_CODE, UC_HOOK_MEM_READ, UC_HOOK_MEM_WRITE
    from unicorn.m68k_const import UC_CPU_M68K_M68000, UC_M68K_REG_SR, UC_M68K_REG_A7, UC_M68K_REG_D0, UC_M68K_REG_PC
    cpu = Uc(UC_ARCH_M68K,UC_MODE_BIG_ENDIAN)
    cpu.ctl_set_cpu_model(UC_CPU_M68K_M68000)
    cpu.mem_map(0,0x20000); cpu.mem_map(0x200000,0x10000); cpu.mem_write(0x200000,fm)
    cpu.mem_map(0xa00000,0x20000); cpu.mem_map(0xc00000,0x10000)
    shared = c.create_string_buffer(65536)
    pointer = c.cast(shared,c.c_void_p).value
    cpu.mem_map_ptr(0xff0000,65536,7,pointer); cpu.mem_map_ptr(0xffff0000,65536,7,pointer)
    cpu.mem_write(0xff0600,ip)
    core = Path(cpu_core) if cpu_core else deps/'core'
    costs = [int(n) for n in re.findall(r'(\d+)\*7',(core/'m68k/m68ki_cycles.h').read_text())]
    if len(costs) != 65536:
        raise ValueError('Unsupported CPU timing table')
    counter, frame, writes = 0, 0, []

    def code(emu,pc,size,user):
        nonlocal counter
        counter += costs[int.from_bytes(emu.mem_read(pc,2),'big')]

    def read(emu,kind,address,size,value,user):
        if address in (0xa04000,0xa04002,0xa11100):
            emu.mem_write(address,bytes(size))

    def write(emu,kind,address,size,value,user):
        if 0xa04000 <= address <= 0xa04003 or address == 0xc00011:
            if size != 1:
                raise ValueError('Unexpected startup sound write size')
            writes.append((frame*896040//42+counter//6,4 if address==0xc00011 else address-0xa04000,value&255))

    cpu.hook_add(UC_HOOK_CODE,code); cpu.hook_add(UC_HOOK_MEM_READ,read); cpu.hook_add(UC_HOOK_MEM_WRITE,write)

    def execute(pc):
        cpu.reg_write(UC_M68K_REG_SR,0x2700); cpu.reg_write(UC_M68K_REG_A7,0xff7000)
        cpu.mem_write(0xff7000,struct.pack('>I',0x1fffc))
        cpu.emu_start(pc,0x1fffc,count=200000)
        if cpu.reg_read(UC_M68K_REG_PC) != 0x1fffc:
            raise ValueError('Original sound routine failed to return')

    execute(0xff56ae)
    counter=0; execute(0xff52ac)
    cpu.mem_write(0xff7f5e,b'\xff\xff')
    frame=1; counter=0
    cpu.reg_write(UC_M68K_REG_D0,0x3f); execute(0xff5058); execute(0xff5106)
    for frame in range(1,721):
        counter=0; execute(0xff5106); execute(0xff408a)
    konami = render((Event*len(writes))(*(Event(*row) for row in writes)),721)
    total = round(1334*32000/fps)
    pcm = bytearray(total*4)
    for start,data in [(0,sega),(round(722*32000/fps)*4,konami)]:
        available = len(pcm)-start
        if any(data[available:]):
            raise ValueError('Startup schedule would cut audible sound')
        pcm[start:start+min(len(data),available)] = data[:available]
    payload = struct.pack('<4sHHIIII',b'NST1',1,2,32000,total,24,16)+pcm
    return {'native-boot-audio-r22.nsta':payload}, dict(sega_events=count,konami_events=len(writes),
        original_writes=0,recorded_inputs=False,sha256=hashlib.sha256(payload).hexdigest())
