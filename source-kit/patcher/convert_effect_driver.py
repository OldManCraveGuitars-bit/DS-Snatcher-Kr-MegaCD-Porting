"""Execute original foreground sound commands and retain their completion signals.

Inputs are the user's disc and a separately licensed host CPU implementation.
No recorded sound, event trace, save state or existing game asset is read.
This isolated startup context is not the full shared music/effect IRQ engine.
"""
from pathlib import Path
import ctypes as c
import hashlib
import re
import struct
import subprocess
from convert_resource_archive import bootstrap


class Event(c.Structure):
    _fields_ = [('frame', c.c_uint32), ('master', c.c_uint32),
                ('port', c.c_uint8), ('value', c.c_uint8)]


class Signal(c.Structure):
    _fields_ = [('frame', c.c_uint32), ('address', c.c_uint32),
                ('size', c.c_uint32), ('value', c.c_uint32)]


def build(disc, engine, cpu_core, workspace, compiler, commands):
    engine, core = Path(engine).resolve(strict=True), Path(cpu_core).resolve(strict=True)
    out = Path(workspace).resolve()
    out.mkdir(exist_ok=False)
    commands = list(commands)
    if not commands or len(set(commands)) != len(commands) or any(not 0 < n < 0x3f for n in commands):
        raise ValueError('Expected distinct original foreground commands')
    ip, fm = bootstrap(disc)[0]['native-boot-ip.bin'], disc.read_file('FMWR_1.BIN')
    identity = hashlib.sha256(ip + fm).digest()
    declared = re.search(r'\{([^}]+)\}', (engine/'source/forecourt_source_identity.h').read_text())[1]
    if bytes(int(x) for x in declared.split(',') if x.strip()) != identity:
        raise ValueError('Unsupported original driver identity')
    adapter = Path(__file__).with_name('source_score_driver.c')
    library = out/'driver.dll'
    proc = subprocess.run([str(compiler), '-O2', '-std=gnu99', '-DLSB_FIRST', '-shared',
        '-I'+str(core), '-I'+str(core/'m68k'), str(adapter),
        str(core/'m68k/m68kcpu.c'), '-o', str(library)], capture_output=True, text=True)
    (out/'build.log').write_text(proc.stdout+proc.stderr, encoding='utf8')
    proc.check_returncode()
    lib = c.CDLL(str(library))
    lib.fm_probe_init.argtypes = [c.c_void_p, c.c_uint, c.c_void_p, c.c_uint, c.c_int]
    lib.fm_probe_execute.argtypes = [c.c_uint]
    lib.fm_probe_frame.argtypes = [c.c_uint]
    lib.fm_probe_reg.argtypes = [c.c_uint, c.c_uint]
    lib.fm_probe_read.argtypes = [c.c_uint, c.c_void_p, c.c_uint]
    lib.fm_probe_write.argtypes = [c.c_uint, c.c_void_p, c.c_uint]
    lib.fm_probe_info.argtypes = [c.c_uint]
    lib.fm_probe_info.restype = c.c_uint
    lib.fm_probe_events.restype = c.POINTER(Event)
    lib.fm_probe_signals.restype = lib.fm_probe_drums.restype = c.POINTER(Signal)
    raw_buffer = c.create_string_buffer(0x574)

    def execute(pc):
        if not lib.fm_probe_execute(pc):
            raise ValueError(('Original CPU execution failed', hex(pc),
                              hex(lib.fm_probe_info(6)), lib.fm_probe_info(7)))

    outputs, reports = {}, []
    for command in commands:
        if not lib.fm_probe_init(ip, len(ip), fm, len(fm), 1):
            raise ValueError('Driver initialization failed')
        execute(0xff56ae)
        lib.fm_probe_frame(0)
        execute(0xff52ac)
        lib.fm_probe_write(0xff7f5e, b'\xff\xff', 2)
        lib.fm_probe_frame(1)
        lib.fm_probe_reg(0, 0x3f)
        execute(0xff5058)
        execute(0xff5106)
        target, done, repeat, signal_at, event_at, drum_at = 961, None, None, 0, 0, 0
        seen, states, signatures, statuses, events, drums = {}, {}, {}, [], [], []
        previous_sequence = 0
        proof = 0
        for frame in range(1, target+12001):
            lib.fm_probe_frame(frame)
            if frame == target:
                lib.fm_probe_reg(0, command)
                execute(0xff5058)
            execute(0xff5106)
            execute(0xff408a)
            lib.fm_probe_read(0xff7a00, raw_buffer, len(raw_buffer))
            raw = raw_buffer.raw
            ptr = lib.fm_probe_events()
            current_events = [(ptr[i].master, ptr[i].port, ptr[i].value)
                              for i in range(event_at, lib.fm_probe_info(0))]
            event_at = lib.fm_probe_info(0)
            events.extend((frame, *row) for row in current_events)
            ptr = lib.fm_probe_drums()
            current_drums = [(ptr[i].address, ptr[i].size, ptr[i].value)
                             for i in range(drum_at, lib.fm_probe_info(1))]
            drum_at = lib.fm_probe_info(1)
            drums.extend((frame, *row) for row in current_drums)
            ptr = lib.fm_probe_signals()
            current_signals = []
            for i in range(signal_at, lib.fm_probe_info(8)):
                value = ptr[i].value
                if ptr[i].address != 0xa1201e or ptr[i].size != 1:
                    raise ValueError('Unexpected source completion register')
                current_signals.append(value)
                changed = (value & 15) != previous_sequence
                if frame >= target and changed and value & 64 and done is None:
                    done = frame
                previous_sequence = value & 15
                statuses.append(dict(frame=frame, value=value, new_sequence=changed))
            signal_at = lib.fm_probe_info(8)
            if frame < target:
                continue
            state = hashlib.sha256(raw).digest()
            signatures[frame] = (current_events, current_drums, current_signals)
            states[frame] = raw
            if repeat is None and state in seen:
                first = seen[state]
                if states[first] != raw:
                    raise ValueError('State digest collision')
                repeat = (first, frame, frame-first)
            seen.setdefault(state, frame)
            if repeat and frame > repeat[1]:
                earlier = frame-repeat[2]
                if states[earlier] != raw or signatures[earlier] != signatures[frame]:
                    raise ValueError(('Source loop does not repeat', command, earlier, frame))
                proof += 1
                if proof >= max(120, repeat[2]) and (done is None or frame >= done+120):
                    break
        else:
            raise ValueError(('No verified source completion or loop', command))
        # Keep initialization writes in their original frame0. They were
        # collected on the first tick but their timestamps must not move.
        ptr = lib.fm_probe_events()
        events = [(ptr[i].frame, ptr[i].master, ptr[i].port, ptr[i].value) for i in range(event_at)]
        blob = b'NSME'+struct.pack('<HHII', 1, 10, frame+1, len(events))
        blob += b''.join(struct.pack('<IIBB', *row) for row in events)
        name = f'original-effect-{command:02x}.nsme'
        outputs[name] = blob
        active = [raw[i*128] for i in range(10)]
        report = dict(command=command, first_target_frame=target, end_frame=frame,
            completion_frame=done, completion_after_ticks=None if done is None else done-target,
            completion_signals=statuses, loop_state=dict(first=repeat[0], repeated=repeat[1], period=repeat[2]),
            following_frames_compared=proof, final_active=active,
            events=len(events), pcm_commands=drums,
            psg_values=sorted({v for f,k,p,v in events if f>=target and p==4}),
            event_file=name, sha256=hashlib.sha256(blob).hexdigest())
        reports.append(report)
    return outputs, dict(source_identity=identity.hex(), commands=reports,
        source_writes=0, recorded_audio_inputs=False, game_state_inputs=False,
        adapter_sha256=hashlib.sha256(adapter.read_bytes()).hexdigest(),
        method='Original Musashi IP sound driver and actual Main communication completion sequence',
        limits=['Isolated3F startup context, not the complete shared music/effect IRQ engine.',
                'Event and completion conversion only; native synthesis and game dispatch require integration.'])
