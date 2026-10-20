"""Convert original indexed sign/magnitude speech to DS signed 8-bit PCM."""
import re, struct, hashlib
from pcm_descriptor import descriptor

SUPPORTED_IDS=(1,2,5,6,7,8,9,16,20,21,28,32,37,40,*range(77,170))

def convert(source, ids=SUPPORTED_IDS):
    table=source.read_file('PCMLT_01.BIN')
    data=source.read_file('PCMLD_01.BIN')
    result={};records=[]
    for voice in ids:
        entry=descriptor(table,voice)
        first,count=struct.unpack_from('>HH',entry,4)
        if not count or (first+count)*2048>len(data):
            raise ValueError(f'Voice {voice} has an invalid sector extent')
        encoded=data[first*2048:(first+count)*2048]
        ends=[m.start() for m in re.finditer(b'\xff{4,}',encoded)
              if set(encoded[m.end():])<={128}]
        if len(ends)!=1:
            raise ValueError(f'Voice {voice}: unsupported encoding or terminator')
        unsigned=bytes(128-x if x<128 else x for x in encoded[:ends[0]])
        pcm=bytes(x^128 for x in unsigned)
        pcm+=bytes((-len(pcm))%4)
        result[f'pcm{voice:04d}.bin']=pcm
        records.append(dict(voice_id=voice,samples=len(unsigned),padded_bytes=len(pcm),
            source_sector=first,source_sectors=count,sha256=hashlib.sha256(pcm).hexdigest()))
    return result,records
