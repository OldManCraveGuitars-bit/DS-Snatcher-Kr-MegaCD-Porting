"""Collect original data/driver banks for the native DS resource VM.

Only the user's input supplies these bytes. This module contains no game data.
"""
import struct, hashlib
from native_resources import FILE_NAMES

def build(source):
    names=FILE_NAMES+['SUBCODE.BIN','FMWR_1.BIN','PCMDRMDT.BIN','PCMLT_01.BIN']
    inputs=[(name,source.read_file(name)) for name in names]
    payload=bytearray(8+56*len(inputs));rows=[]
    for i,(name,data) in enumerate(inputs):
        while len(payload)%4:payload.append(0)
        at=len(payload);digest=hashlib.sha256(data).digest()
        struct.pack_into('<16sII32s',payload,8+56*i,name.encode(),at,len(data),digest)
        payload+=data;rows.append(dict(name=name,offset=at,bytes=len(data),sha256=digest.hex()))
    struct.pack_into('<4sHH',payload,0,b'NRS1',1,len(inputs))
    return bytes(payload),rows

def bootstrap(source):
    if source.directory:return {},[]
    header=source.read_sectors(0,1)
    if not header.startswith(b'SEGADISCSYSTEM'):raise ValueError('Expected Mega CD bootstrap header')
    outputs={};records=[]
    ip_at=struct.unpack_from('>I',header,0x30)[0]
    if not 0x200<ip_at<=0x800:raise ValueError('Unsupported original security/IP boundary')
    security=source.read_sectors(0,1)[0x200:ip_at]
    if len(security)!=ip_at-0x200:raise ValueError('Truncated original security program')
    outputs['native-boot-security.bin']=security
    records.append(dict(name='native-boot-security.bin',original_disc_byte_offset=0x200,
        bytes=len(security),sha256=hashlib.sha256(security).hexdigest(),
        main_ram_load_offset=0,method='Original SEGA security/license program, no recording'))
    for name,field in [('native-boot-ip.bin',0x30),('native-boot-sp.bin',0x40)]:
        at,size=struct.unpack_from('>II',header,field)
        if at<0x200 or not 0<size<=0x20000 or at+size>0x20000:
            raise ValueError('Unsupported Mega CD bootstrap extent')
        first=at//2048;offset=at%2048
        data=source.read_sectors(first,(offset+size+2047)//2048)[offset:offset+size]
        outputs[name]=data
        records.append(dict(name=name,original_disc_byte_offset=at,bytes=size,
            sha256=hashlib.sha256(data).hexdigest(),method='Original bootstrap extent, no recorded frames'))
    return outputs,records
