"""All original inputs for the SP06 entry player, without media captures."""
import struct
from native_resources import sha
from convert_pcm_effects import extract_dynamic
from convert_entry_program import build as script_build

def build(source):
    names=('DATA_H15.BIN','DATA_H01.BIN','DATA_H00.BIN','SUBCODE.BIN','PCMLT_01.BIN','PCMDRMDT.BIN')
    archive=bytearray(8+len(names)*56);rows=[]
    for index,name in enumerate(names):
        data=source.read_file(name)
        while len(archive)%4:archive.append(0)
        at=len(archive);digest=sha(data)
        struct.pack_into('<16sII32s',archive,8+index*56,name.encode(),at,len(data),bytes.fromhex(digest))
        archive+=data;rows.append(dict(name=name,bytes=len(data),sha256=digest))
    struct.pack_into('<4sHH',archive,0,b'NRS1',1,len(names))
    script,program=script_build(source)
    effects,effect_info=extract_dynamic(source,34)
    voice,voice_info=extract_dynamic(source,55)
    outputs={'native-resource-banks-r22.nrs':bytes(archive),'native-entry-script-r22.bin':script,
             'native-entry-pcm-bank-r22.bin':effects,'native-entry-voice55-r22.bin':voice}
    return outputs,dict(program=program,resources=rows,effects=effect_info,voice=voice_info,
        captured_inputs=False,source_writes=0,
        limits=['Minimal entry-only archive for development; not the full game archive.',
                'Music4F is a separate original FM-driver request; no silent substitute.'])
