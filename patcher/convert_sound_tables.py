"""Bind original FM instruments/music packets and native startup song metadata."""
import struct
from native_resources import sha
from convert_resource_archive import bootstrap

def build(source):
    fm=source.read_file('FMWR_1.BIN') if hasattr(source,'read_file') else (source/'FMWR_1.BIN').read_bytes()
    instruments=[struct.unpack_from('>I',fm,i*4)[0] for i in range(256)]
    assert instruments==[0x400+i*26 for i in range(256)]
    effects_table=instruments[-1]+26
    effect_bytes=struct.unpack_from('>I',fm,effects_table)[0]
    assert effect_bytes%4==0
    effect_count=effect_bytes//4
    effects=[effects_table+struct.unpack_from('>I',fm,effects_table+i*4)[0] for i in range(effect_count)]
    assert effects==sorted(effects) and all(0<=p<len(fm) for p in effects)
    candidates=[]
    for at in range(effects[-1],len(fm)-240,2):
        if fm[at:at+4]!=b'\0\0\0\xf0':continue
        offsets=struct.unpack_from('>60I',fm,at)
        if offsets==tuple(sorted(set(offsets))) and all(240<=p<len(fm)-at for p in offsets):
            candidates.append((at,offsets))
    assert len(candidates)==1,('Ambiguous original FM music table',candidates)
    music_table,offsets=candidates[0]
    original_boot,_=bootstrap(source) if hasattr(source,'read_sectors') else ({},[])
    ip=original_boot.get('native-boot-ip.bin')
    music=[]
    for index,relative in enumerate(offsets):
        start=music_table+relative
        end=music_table+offsets[index+1] if index+1<60 else len(fm)
        row=dict(index=index,command_id=0x3f+index,source_offset=start,bytes=end-start,
                 packet_sha256=sha(fm[start:end]))
        if ip is not None:
            # Original IP is loaded at main RAM FF0600. Native LEA FFFF6142
            # references the eight-byte command descriptors at IP offset5B42.
            assert bytes.fromhex('41f9ffff6142') in ip
            at=0x6142-0x600+row['command_id']*8
            descriptor=ip[at:at+8];assert len(descriptor)==8
            kind,channels,parameter,tempo,data_index=struct.unpack('>BBBBI',descriptor)
            assert kind==1 and data_index==index
            assert struct.unpack_from('>I',fm,start)[0]==(channels+1)*4
            row.update(native_descriptor=descriptor.hex(),native_channel_count=channels+1,
                       native_tempo_byte=tempo,native_parameter=parameter)
        music.append(row)
    output=bytearray(struct.pack('<4sHHIII',b'NSFM',1,len(music),20,effects_table,effect_count))
    for row in music:
        output+=struct.pack('<BBBBIII',row['command_id'],row.get('native_channel_count',0),
                            row.get('native_tempo_byte',0),row.get('native_parameter',0),
                            row['index'],row['source_offset'],row['bytes'])
    report=dict(kind='original_fm_resource_tables',bytes=len(output),sha256=sha(output),
                original_fm_file_sha256=sha(fm),instruments=256,instrument_bytes=26,
                effect_packet_count=effect_count,effects_table_offset=effects_table,
                music_table_offset=music_table,music_packets=music,
                descriptor_source='Original IP FF0600/6142' if ip is not None else 'Disc IP required for descriptors',
                source_writes=0,recorded_audio_inputs=False,
                limits=['Native FM/YM player and loop semantics are still being ported',
                        'These are original score packets, not recorded mixed gameplay audio'])
    return bytes(output),report
