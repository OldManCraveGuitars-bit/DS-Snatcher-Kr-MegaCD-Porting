"""Original resident effect table and dynamically loaded PCM wave banks.

Effect commands index PCMLT_01+4000 in 16-byte records (SUB RAM54000).
They do not index the eight-byte speech/load table at SUB RAM50000.
No captured audio, resampled waveform or recorded gameplay is an input.
"""
import struct
from native_resources import sha
from pcm_descriptor import descriptor

def wave_at(data, base, address):
    offset = address-base
    if offset < 0 or offset+16 > len(data):
        raise ValueError(f'Wave header outside loaded bank: {address:06X}')
    size, loop, rate = struct.unpack_from('>IIH', data, offset)
    if not size or offset+16+size > len(data):
        raise ValueError(f'Wave payload outside loaded bank: {address:06X}')
    samples = data[offset+16:offset+16+size]
    # FF is the RF5C164 sentinel. Native headers retain their terminator
    # padding; never treat it as signed-8-bit audio or strip internal data.
    try:stop = samples.index(255)
    except ValueError:raise ValueError(f'Wave has no native terminator: {address:06X}')
    if any(value != 255 for value in samples[stop:]):
        raise ValueError(f'Unsupported internal sentinel: {address:06X}')
    return dict(address=address,bank_offset=offset,encoded_bytes=size,
                samples=stop,loop=loop,nominal_rate=rate,
                header_hex=data[offset:offset+16].hex(),
                encoded_sha256=sha(samples))

def extract_dynamic(source, load_id):
    table = source.read_file('PCMLT_01.BIN')
    data = source.read_file('PCMLD_01.BIN')
    entry = descriptor(table, load_id)
    sector, count = struct.unpack_from('>HH',entry,4)
    if not count or (sector+count)*2048 > len(data):raise ValueError('Invalid PCM sector extent')
    payload = data[sector*2048:(sector+count)*2048]
    return payload,dict(load_id=load_id,sector=sector,sectors=count,
                        bytes=len(payload),sha256=sha(payload))

def build_entry(source):
    table = source.read_file('PCMLT_01.BIN')
    drum = source.read_file('PCMDRMDT.BIN')
    dynamic, extent = extract_dynamic(source, 0x22)
    count = table[0x4000]
    if 0x4000+count*16 != len(table):raise ValueError('Unrecognized effect table extent')
    effects = []
    for effect in (0x45,0x46):
        offset = 0x4000+effect*16
        priority,pair,volume,pan,step,address = struct.unpack_from('>BBBBHI',table,offset)
        wave = wave_at(dynamic,0x65000,address)
        effects.append(dict(effect_id=effect,priority=priority,channel_pair=pair,
                            volume=volume,pan=pan,step=step,wave=wave,
                            original_descriptor=table[offset:offset+16].hex()))
    # Also validate every resident (non-streaming) waveform referenced by
    # the original table. Dynamic addresses depend on the currently loaded
    # bank and must not be bound to arbitrary same-numbered speech clips.
    resident={}
    for effect in range(1,count):
        address=struct.unpack_from('>I',table,0x4000+effect*16+6)[0]
        if 0x55000<=address<0x65000:
            resident.setdefault(address,wave_at(drum,0x55000,address))
    report=dict(kind='original_dynamic_pcm_bank',source_writes=0,recorded_audio_inputs=False,
                table_sha256=sha(table),resident_sha256=sha(drum),dynamic=extent,
                effect_table_offset=0x4000,effect_record_bytes=16,effect_count=count,
                entry_effects=effects,resident_waves=list(resident.values()),
                limits=['Native encoded waveforms and descriptors; driver playback/mixing still requires the original device semantics.'])
    return dynamic,report
