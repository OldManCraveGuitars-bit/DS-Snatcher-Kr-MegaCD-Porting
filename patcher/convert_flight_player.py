"""Original vehicle scene, speech129 and CD20; no recorded media input."""
import struct
from native_resources import sha
from convert_pcm_effects import extract_dynamic
from convert_cdda import build as cdda
from convert_cdda_stream import build as stream

def inspect(script):
    cockpit=bytes.fromhex('30 25 3d 20 03 25 4a 00 3c 25 43')
    departure=bytes.fromhex('25 0a 00 ff 25 4a 00 56 25 04 00 81 25 46')
    tail=bytes.fromhex('25 0d 00 14 25 45 00 20 25 45 00 0f 25 3d 20 02 25 48')
    handoff=bytes.fromhex('2c 21 00 40 00 04')
    for pattern in (cockpit,departure,tail):
        if script.count(pattern)!=1:raise ValueError('Unsupported or ambiguous original vehicle script')
    entry=script.index(cockpit);start=script.index(departure);cd=script.index(tail)
    end=script.find(handoff,cd)
    if not entry<start<cd<end or end-start>256:raise ValueError('Unsupported original flight continuation')
    return dict(source_file='SP06.BIN',sha256=sha(script),cockpit_pc=entry,
        cockpit_dialogue_pc=entry+9,departure_pc=start,cd_command_pc=cd,
        handoff_pc=end,handoff_module=64,handoff_target=4,track=20,voice=129,
        scene_file='DATA_S0.BIN',captured_timeline_inputs=False)

def build_archive(source):
    """Pack the original vehicle banks without decoding CD20 a second time."""
    names=('DATA_S0.BIN','DATA_K1.BIN','SUBCODE.BIN','PCMLT_01.BIN','PCMDRMDT.BIN')
    archive=bytearray(8+len(names)*56);rows=[]
    for index,name in enumerate(names):
        data=source.read_file(name)
        while len(archive)%4:archive.append(0)
        at=len(archive);digest=sha(data)
        struct.pack_into('<16sII32s',archive,8+index*56,name.encode(),at,len(data),bytes.fromhex(digest))
        archive+=data;rows.append(dict(name=name,bytes=len(data),sha256=digest))
    struct.pack_into('<4sHH',archive,0,b'NRS1',1,len(names))
    return bytes(archive),rows


def build(source):
    script=source.read_file('SP06.BIN');program=inspect(script)
    arrival=source.read_file('SP07.BIN')
    # The actual module continuation is another S0 animation, not an
    # immediate static cockpit. Keep the unmodified original script.
    prefix=bytes.fromhex('30 25 3d 20 04 25 00 23 00 11 00 00 25 0a 00 45 25 45 00 11 25 43 20 00 02 02 13')
    if arrival[4:4+len(prefix)]!=prefix:raise ValueError('Unsupported original flight arrival')
    arrival_info=dict(source_file='SP07.BIN',sha256=sha(arrival),entry_pc=4,dialogue_pc=0x18,actor=17,music=0x45,text_offset=0x213)
    forecourt=bytes.fromhex('30 25 3d 19 05 25 4a 00 14 25 00 23 00 15 00 00 25 0a 00 4c 25 4a 00 28 25 04 00 02 25 46 25 00 23 00 14 00 00 25 43')
    if arrival[0x72:0x72+len(forecourt)]!=forecourt:raise ValueError('Unsupported original vehicle exit')
    archive,rows=build_archive(source)
    voice,voice_info=extract_dynamic(source,129)
    effects,effect_info=extract_dynamic(source,2)
    wavs,cd_info=cdda(source,(20,))
    if not wavs:raise ValueError('The flight requires original AUDIO track20 from CUE/BIN')
    outputs,stream_info=stream(wavs,(20,))
    outputs.update({'native-flight-banks-r22.nrs':bytes(archive),
        'native-flight-script-r22.bin':script,'native-flight-voice129-r22.bin':voice,'native-arrival-script-r22.bin':arrival,
        'native-forecourt-pcm2-r22.bin':effects})
    return outputs,dict(program=program,arrival=arrival_info,forecourt=dict(entry_pc=0x72,dialogue_pc=0x97,music=0x4c,pcm=effect_info),resources=rows,voice=voice_info,cdda=cd_info,stream=stream_info,
        captured_inputs=False,source_writes=0,
        limits=['Minimal vehicle inputs; do not overwrite the full game resource archive.',
                'Runtime integration and complete original/DS playback comparison must be verified separately.'])
