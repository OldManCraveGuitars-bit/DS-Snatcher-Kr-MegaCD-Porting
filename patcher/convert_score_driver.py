"""Recreate supported scores, PCM gates and startup from an original disc.

The host executes the original program with a separately licensed CPU core.
It discovers a complete driver-state repeat and checks the following cycle;
no recorded trace, game state, audio or existing output asset is an input.
These declared startup sequences do not emulate the full shared game IRQ.
"""
from pathlib import Path
import ctypes as c
import hashlib
import re
import struct
import subprocess
import zlib
from convert_resource_archive import bootstrap


def _compile(command, log):
    result = subprocess.run(command, capture_output=True, text=True)
    log.write_text(result.stdout + result.stderr, encoding='utf8')
    result.check_returncode()


def build(disc, engine, cpu_core, workspace, compiler, command=0x62):
    if command not in (0x40, 0x5f, 0x62, 0x5e, 0x5a):
        raise ValueError('Unsupported source score')
    if command == 0x5a and disc.read_file('SP06.BIN')[0x54a:0x54e] != bytes.fromhex('250a005a'):
        raise ValueError('Unsupported original headquarters music script')
    if command == 0x5e and disc.read_file('SP07.BIN')[0x186b:0x1873] != bytes.fromhex('253d2905250a005e'):
        raise ValueError('Unsupported original game-over script')
    engine = Path(engine).resolve(strict=True)
    core = Path(cpu_core).resolve(strict=True)
    workspace = Path(workspace).resolve()
    workspace.mkdir(exist_ok=False)
    source = engine / 'source'
    ip = bootstrap(disc)[0]['native-boot-ip.bin']
    fm = disc.read_file('FMWR_1.BIN')
    identity = hashlib.sha256(ip + fm).digest()
    declared = re.search(r'\{([^}]+)\}', (source / 'forecourt_source_identity.h').read_text())[1]
    if bytes(int(x) for x in declared.split(',') if x.strip()) != identity:
        raise ValueError('The disc does not match the supported original driver')
    adapter = Path(__file__).with_name('source_score_driver.c')
    library = workspace / 'score.dll'
    from host_helpers import prepared
    if not prepared('score',library,disc):
        _compile([str(compiler), '-O2', '-std=gnu99', '-DLSB_FIRST', '-shared',
              '-I' + str(core), '-I' + str(core / 'm68k'), str(adapter),
              str(core / 'm68k/m68kcpu.c'), '-o', str(library)], workspace / 'score-build.log')
    lib = c.CDLL(str(library))
    lib.fm_probe_init.argtypes = [c.c_void_p, c.c_uint, c.c_void_p, c.c_uint, c.c_int]
    lib.fm_probe_execute.argtypes = [c.c_uint]
    lib.fm_probe_reg.argtypes = [c.c_uint, c.c_uint]
    lib.fm_probe_frame.argtypes = [c.c_uint]
    lib.fm_probe_read.argtypes = [c.c_uint, c.c_void_p, c.c_uint]
    lib.fm_probe_write.argtypes = [c.c_uint, c.c_void_p, c.c_uint]
    lib.fm_probe_info.argtypes = [c.c_uint]
    lib.fm_probe_info.restype = c.c_uint

    class Event(c.Structure):
        _fields_ = [('frame', c.c_uint32), ('master', c.c_uint32),
                    ('port', c.c_uint8), ('data', c.c_uint8)]

    class Drum(c.Structure):
        _fields_ = [('frame', c.c_uint32), ('address', c.c_uint32),
                    ('size', c.c_uint32), ('value', c.c_uint32)]

    lib.fm_probe_events.restype = c.POINTER(Event)
    lib.fm_probe_drums.restype = c.POINTER(Drum)
    if not lib.fm_probe_init(ip, len(ip), fm, len(fm), 1):
        raise ValueError('Original driver initialization failed')

    def execute(address):
        if not lib.fm_probe_execute(address):
            raise ValueError(('Unsupported source execution', hex(address),
                              hex(lib.fm_probe_info(6)), lib.fm_probe_info(7)))

    execute(0xff56ae)
    lib.fm_probe_frame(0)
    execute(0xff52ac)
    lib.fm_probe_write(0xff7f5e, b'\xff\xff', 2)
    lib.fm_probe_frame(1)
    lib.fm_probe_reg(0, 0x3f)
    execute(0xff5058)
    execute(0xff5106)
    # Preserve the existing demo's declared source initialization context.
    # HQ5F was generated after the original 3F/4F/5A requests; replacing that
    # history with a direct5F request changes the driver and oscillator state.
    target = 2474 if command == 0x5f else 961
    schedule = {241:0x4f, 2041:0x5a, target:command} if command == 0x5f else {target:command}
    stride = 0x574
    buffer = c.create_string_buffer(stride)
    states, seen, events, drums = bytearray(), {}, {}, {}
    event_at = drum_at = checks = compared_writes = 0
    loop = sequence_delta = None
    for frame in range(1, 240001):
        lib.fm_probe_frame(frame)
        if frame in schedule:
            lib.fm_probe_reg(0, schedule[frame])
            execute(0xff5058)
        execute(0xff5106)
        execute(0xff408a)
        lib.fm_probe_read(0xff7a00, buffer, stride)
        raw = buffer.raw
        count = lib.fm_probe_info(0)
        pointer = lib.fm_probe_events()
        # Include initialization writes stamped0 before the first tick.
        for i in range(event_at, count):
            e = pointer[i]
            events.setdefault(e.frame, []).append((e.master, e.port, e.data))
        event_at = count
        count = lib.fm_probe_info(1)
        pointer = lib.fm_probe_drums()
        for i in range(drum_at, count):
            d = pointer[i]
            drums.setdefault(d.frame, []).append((d.address, d.size, d.value))
        drum_at = count
        if loop is None:
            digest = hashlib.sha256(raw).digest()
            if frame >= target and digest in seen:
                first = seen[digest]
                if states[(first - 1) * stride:first * stride] != raw:
                    raise ValueError('Driver-state digest collision')
                loop = (first, frame, frame - first)
                checks += 1
            if frame >= target:
                seen.setdefault(digest, frame)
            states.extend(raw)
        else:
            first, repeated, period = loop
            earlier = frame - period
            if raw != states[(earlier - 1) * stride:earlier * stride]:
                raise ValueError(('Driver-state loop mismatch', earlier, frame))
            a, b = events.get(earlier, []), events.get(frame, [])
            if a != b:
                raise ValueError(('FM loop mismatch', earlier, frame))
            compared_writes += len(a)
            a, b = drums.get(earlier, []), drums.get(frame, [])
            if len(a) != len(b):
                raise ValueError(('PCM gate count mismatch', earlier, frame))
            for x, y in zip(a, b):
                if x[:2] != y[:2]:
                    raise ValueError('PCM gate address mismatch')
                if x[0] == 0xa1201d:
                    delta = (y[2] - x[2]) & 255
                    if sequence_delta is None:
                        sequence_delta = delta
                    if delta != sequence_delta:
                        raise ValueError('PCM gate sequence mismatch')
                elif x != y:
                    raise ValueError('PCM gate value mismatch')
            checks += 3
            # Following-cycle rows are proof only; do not retain duplicate data.
            events.pop(frame, None)
            drums.pop(frame, None)
            if frame == repeated + period:
                break
    else:
        raise ValueError('No complete verified driver loop within supported bound')
    if sequence_delta is None and command == 0x62:
        raise ValueError('No verified PCM sequence recurrence')
    if command == 0x5e and any(v for f, v in drums.items() if f >= target):
        raise ValueError('Unexpected PCM gates in source gameover score')
    first, end, period = loop[0] + 1, loop[1] + 1, loop[2]
    # The historical5F stream used YM-clock quantization; the other current
    # streams retain full master clocks. Keep its exact source conversion.
    rows = [(f * 896040 + (clock//42*42 if command == 0x5f else clock), port, value)
            for f in sorted(events) if f < end for clock, port, value in events[f]]
    if not rows or any(a[0] > b[0] for a, b in zip(rows, rows[1:])):
        raise ValueError('Non-monotonic source writes')
    if any(not (p < 4 or (p == 4 and v in (0x9f, 0xbf, 0xdf, 0xff))) for _, p, v in rows):
        raise ValueError('Unsupported PSG command')
    loop_at = next(i for i, row in enumerate(rows) if row[0] >= first * 896040)
    preroll = target * 32000 * 896040 // 53693175
    stream = struct.pack('<4s7I4Q', b'NSFS', 1, 16, len(rows), loop_at, command, 32000, 64,
                         first * 896040, end * 896040, preroll, 64 + 16 * len(rows))
    stream += b''.join(struct.pack('<QBB6x', *r) for r in rows)
    gate, previous, changes = bytearray(4), None, []
    for f in range(end):
        for address, size, value in drums.get(f, []):
            if size != 1 or not 0xa1201a <= address <= 0xa1201d:
                raise ValueError('Unsupported PCM command')
            gate[address - 0xa1201a] = value
        if f >= target and gate != previous:
            changes.append(struct.pack('<I', f - target) + gate)
            previous = bytes(gate)
    gates = struct.pack('<4s7I', b'NSDM', 2, end - target, first - target, end - target,
                        sequence_delta or 0, len(changes), 8) + b''.join(changes)
    event_path = workspace / 'computed.nsfs'
    event_path.write_bytes(stream)
    digest = hashlib.sha256(stream).digest()
    text = (source / 'resource_fm.c').read_text(encoding='utf8')
    marker = '#include "fm_stream_checkpoint_r22.inc"'
    if text.count(marker) != 1:
        raise ValueError('Unsupported native FM source layout')
    prefix = text[:text.index(marker) + len(marker)]
    wrapper = workspace / 'startup.c'
    # The public patcher must work from Korean-named output folders too.
    # Only this helper opens a path passed from Python through a C ABI.
    utf8_paths = '''
#ifdef _WIN32
#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
static FILE *converter_fopen(const char *path,const char *mode){
 int pn=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,NULL,0);
 int mn=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,mode,-1,NULL,0);
 if(!pn || !mn)return NULL;
 wchar_t *p=malloc((size_t)pn*sizeof(*p)),*m=malloc((size_t)mn*sizeof(*m));
 if(!p || !m){free(p);free(m);return NULL;}
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,path,-1,p,pn);
 MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,mode,-1,m,mn);
 FILE *f=_wfopen(p,m);free(p);free(m);return f;
}
#define fopen converter_fopen
#endif
'''
    wrapper.write_text(utf8_paths + prefix + '''
extern bool fm_tables_begin(void);
bool converter_begin(const char *path,const unsigned char *identity){
 if(!fm_tables_begin())return false;
 memcpy(fm_start_event_sha256,identity,32);return probe_begin_stream(path);
}
void converter_close(void){if(event_file){fclose(event_file);event_file=NULL;}}
''', encoding='utf8')
    library = workspace / 'startup.dll'
    if not prepared('startup',library,disc):
        _compile([str(compiler), '-O3', '-std=gnu99', '-shared', '-DLSB_FIRST',
              '-I' + str(source), '-I' + str(engine / 'include'), str(wrapper),
              str(source / 'ym2612.c'), str(source / 'audio_hot_state.c'),
              '-o', str(library), '-lm'], workspace / 'startup-build.log')
    synth = c.CDLL(str(library))
    synth.converter_begin.argtypes = [c.c_char_p, c.c_void_p]
    synth.converter_begin.restype = c.c_bool
    synth.probe_export_state.argtypes = [c.c_void_p, c.c_uint]
    synth.probe_export_state.restype = c.c_uint
    if not synth.converter_begin(str(event_path).encode(), digest):
        raise ValueError('Computed score startup failed')
    buffer = c.create_string_buffer(8192)
    size = synth.probe_export_state(buffer, len(buffer))
    synth.converter_close()
    state = buffer.raw[:size]
    if not 128 < size < 8192 or int.from_bytes(state[12:16], 'little') != zlib.crc32(state[16:]):
        raise ValueError('Portable startup state failed')
    outputs = {f'native-fm-{command:02x}.nsfs': stream, f'native-fm-{command:02x}.nsfi': state}
    if command in (0x40, 0x62, 0x5a):
        outputs[f'native-music-{command:02x}-drums-r22.nsdm'] = gates
    metadata = dict(original_writes=0, captured_inputs=False, precomputed_audio_inputs=False,
                    source_identity=identity.hex(), source_frames=frame, command=command,
                    source_command_schedule={1:0x3f, **schedule},
                    loop_state=dict(first_frame=loop[0], repeated_frame=loop[1], period_frames=period),
                    following_frames_compared=period, loop_checks=checks,
                    register_writes_compared=compared_writes, drum_sequence_delta=sequence_delta,
                    initialization_samples=preroll, event_count=len(rows), gate_changes=len(changes),
                    adapter_sha256=hashlib.sha256(adapter.read_bytes()).hexdigest(),
                    source_files={p.name: hashlib.sha256(p.read_bytes()).hexdigest()
                                  for p in source.iterdir() if p.suffix in ('.c', '.h', '.inc', '.s')},
                    cpu_files={p.relative_to(core).as_posix(): hashlib.sha256(p.read_bytes()).hexdigest()
                               for p in core.rglob('*') if p.is_file() and p.suffix in ('.c', '.h')},
                    limits=[f'Declared source-command startup for{command:02X}; full original shared IRQ/chip context is not implemented.',
                            'Score recurrence preserves continuous oscillator state; it is not an audio loop.'])
    return outputs, metadata
