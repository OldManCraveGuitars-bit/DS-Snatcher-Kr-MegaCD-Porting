"""Assemble an NDS from the masked port engine and validated local resources."""
from pathlib import Path
import hashlib
import json
import math
import struct
import zlib
from host_helpers import restore


def crc16(data):
    crc=65535
    for byte in data:
        crc ^= byte
        for _ in range(8):
            crc=(crc>>1)^(0xa001 if crc&1 else 0)
    return crc


def assemble(source, engine, resources, output, manifest):
    engine, resources, output = Path(engine),Path(resources),Path(output)
    if output.exists():raise ValueError('NDS output already exists')
    meta=json.loads((engine/'runner.json').read_text(encoding='utf8'))
    packed=(engine/'runner.zlib').read_bytes()
    if hashlib.sha256(packed).hexdigest()!=meta['sha256']:raise ValueError('Damaged port engine')
    data=bytearray(restore(zlib.decompress(packed),meta['source_tables'],source))
    if hashlib.sha256(data).hexdigest()!=meta['restored_sha256']:raise ValueError('Engine restoration mismatch')
    names=sorted(row['name'] for row in manifest['files'])
    rows={row['name']:row for row in manifest['files']}
    if len(names)!=len(rows):raise ValueError('Duplicate resource names')
    fnt=bytearray(struct.pack('<IHH',8,0,1))
    for name in names:
        if not name.isascii() or not 0<len(name)<128 or Path(name).name!=name:raise ValueError('Invalid NitroFS name')
        fnt+=bytes([len(name)])+name.encode('ascii')
    fnt+=b'\0'

    def append(payload):
        data.extend(b'\xff'*((-len(data))%512))
        at=len(data);data.extend(payload);return at

    fnt_at=append(fnt)
    fat_at=append(bytes(len(names)*8))
    banner=zlib.decompress((engine/'banner.zlib').read_bytes())
    if hashlib.sha256(banner).hexdigest()!=meta['banner_sha256']:raise ValueError('Damaged banner')
    banner_at=append(banner)
    for i,name in enumerate(names):
        payload=(resources/name).read_bytes()
        if hashlib.sha256(payload).hexdigest()!=rows[name]['sha256']:raise ValueError('Resource mismatch: '+name)
        at=append(payload);struct.pack_into('<II',data,fat_at+i*8,at,at+len(payload))
    data.extend(b'\xff'*((-len(data))%512))
    struct.pack_into('<4I',data,0x40,fnt_at,len(fnt),fat_at,len(names)*8)
    struct.pack_into('<I',data,0x68,banner_at)
    struct.pack_into('<I',data,0x80,len(data))
    data[0x14]=max(0,(len(data)-1).bit_length()-17)
    struct.pack_into('<H',data,0x15e,crc16(data[:0x15e]))
    output.write_bytes(data)
    return dict(nds_sha256=hashlib.sha256(data).hexdigest(),bytes=len(data),resource_files=len(names),
        original_writes=0,engine_restored=True,all_resource_hashes_match=True)
