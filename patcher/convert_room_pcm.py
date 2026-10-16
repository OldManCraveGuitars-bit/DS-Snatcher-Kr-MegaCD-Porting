"""Original mode1 PCM loads for native room actors; no recorded audio."""
import struct
from native_resources import sha
from convert_pcm_effects import extract_dynamic

def build(source):
    table = source.read_file('PCMLT_01.BIN')
    rows = [('PCMLT_01.BIN',table),('PCMDRMDT.BIN',source.read_file('PCMDRMDT.BIN'))]
    extents = []
    for ident in range(1,min(len(table),0x4000)//8):
        if table[ident*8+1] != 1:continue
        payload, extent = extract_dynamic(source,ident)
        rows.append((f'PCM{ident:04d}.BIN',payload));extents.append(extent)
    data = bytearray(8+56*len(rows));records=[]
    struct.pack_into('<4sHH',data,0,b'NRS1',1,len(rows))
    for i,(name,payload) in enumerate(rows):
        while len(data)%4:data.append(0)
        digest=sha(payload);at=len(data)
        struct.pack_into('<16sII32s',data,8+56*i,name.encode(),at,len(payload),bytes.fromhex(digest))
        data+=payload;records.append(dict(name=name,bytes=len(payload),sha256=digest))
    return bytes(data),dict(records=records,extents=extents,source_writes=0,recorded_audio_inputs=False)
