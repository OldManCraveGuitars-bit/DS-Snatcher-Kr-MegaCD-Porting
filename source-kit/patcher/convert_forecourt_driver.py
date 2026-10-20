"""Build the bounded4C driver resources from the user's original disc.

No reference recording, emulator state, event trace or generated music bank
is read. The licensed engine source is compiled on the host and executes the
same fixed3F-to4C initialization used by the current player. Full original
Main/Sub IRQ and preceding-score continuity are still separate port work.
"""
from pathlib import Path
import ctypes as c
import hashlib
import re
import subprocess
import zlib
from convert_resource_archive import bootstrap


def build(disc, engine, workspace, compiler):
    engine=Path(engine).resolve(strict=True)
    workspace=Path(workspace).resolve()
    workspace.mkdir(exist_ok=False)
    source=engine/'source'
    ip=bootstrap(disc)[0]['native-boot-ip.bin']
    fm=disc.read_file('FMWR_1.BIN')
    identity=hashlib.sha256(ip+fm).digest()
    declared=re.search(r'\{([^}]+)\}',(source/'forecourt_source_identity.h').read_text())[1]
    if bytes(int(x) for x in declared.split(',') if x.strip())!=identity:
        raise ValueError('The supplied disc does not match the supported driver identity')
    wrapper=workspace/'converter.c'
    wrapper.write_text('''volatile unsigned fm_error,fm_ready,fm_sample_hash=2166136261u;
void fm_observe_stack(void){}
void memset32(void *dest,int value,int count){int *p=dest;while(count--)*p++=value;}
''')
    inputs=('adapter.c','m68kcpu.c','forecourt_fm.c','ym2612.c','audio_hot_state.c')
    library=workspace/'converter.dll'
    command=[str(compiler),'-O3','-std=gnu99','-shared','-DLSB_FIRST',
             '-I'+str(source),'-I'+str(engine/'include'),
             *(str(source/name) for name in inputs),str(wrapper),'-o',str(library),'-lm']
    from host_helpers import prepared
    if not prepared('forecourt',library,disc):
        result=subprocess.run(command,capture_output=True,text=True)
        (workspace/'build.log').write_text(result.stdout+result.stderr,encoding='utf8')
        result.check_returncode()
    lib=c.CDLL(str(library))
    lib.forecourt_fm_begin_native.argtypes=[c.c_void_p,c.c_uint,c.c_void_p,c.c_uint]
    lib.forecourt_fm_begin_native.restype=c.c_bool
    lib.forecourt_fm_render.argtypes=[c.c_void_p,c.c_uint];lib.forecourt_fm_render.restype=c.c_bool
    lib.forecourt_fm_export_state.argtypes=[c.c_void_p,c.c_uint];lib.forecourt_fm_export_state.restype=c.c_uint
    if not lib.forecourt_fm_begin_native(ip,len(ip),fm,len(fm)):
        raise ValueError('Original FM initialization failed')
    # Boundary immediately before the original4C request at source frame961.
    # All earlier clocks execute; samples are discarded, never packaged.
    preroll=961*896040*32000//53693175
    samples=(c.c_int16*256)();left=preroll
    while left:
        count=min(left,128)
        if not lib.forecourt_fm_render(samples,count):
            raise ValueError('Unsupported original FM initialization command')
        left-=count
    buffer=c.create_string_buffer(1048576)
    size=lib.forecourt_fm_export_state(buffer,len(buffer))
    if not 128<size<len(buffer):raise ValueError('Invalid portable driver state')
    state=buffer.raw[:size]
    if int.from_bytes(state[12:16],'little')!=zlib.crc32(state[16:]):
        raise ValueError('Portable driver CRC mismatch')
    outputs={'native-driver-4c-ip.bin':ip,'native-driver-4c-fm.bin':fm,'native-driver-4c-start.bin':state}
    metadata=dict(original_writes=0,captured_inputs=False,precomputed_audio_inputs=False,
                  initialization_samples=preroll,source_identity=identity.hex(),
                  source_files={p.name:hashlib.sha256(p.read_bytes()).hexdigest()
                                for p in source.iterdir() if p.suffix in ('.c','.h','.inc','.s')},
                  limits=['Fixed3F-to4C source initialization; complete original shared IRQ/music context remains incomplete.'])
    return outputs,metadata
