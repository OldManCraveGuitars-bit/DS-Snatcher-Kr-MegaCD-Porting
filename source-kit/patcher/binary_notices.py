"""Attach preserved third-party notices without changing executable/resource bytes.

The NDS size/capacity/checksum fields are adjusted for an aligned trailer.
PE and zlib templates retain every original byte before the trailer. Repeated
attachment is idempotent; damaged or different existing trailers are rejected.
"""
from pathlib import Path
import argparse
import hashlib
import os
import struct
import zlib

MAGIC=b'SNDNOTE1'
FOOTER=struct.Struct('<8sQQ32s8x')
MAX_NOTICES=2*1024*1024


def crc16(data):
    value=65535
    for byte in data:
        value^=byte
        for _ in range(8):value=(value>>1)^(0xa001 if value&1 else 0)
    return value


def _footer(stream,size):
    if size<FOOTER.size:return None
    stream.seek(size-FOOTER.size);raw=stream.read(FOOTER.size)
    magic,original,length,digest=FOOTER.unpack(raw)
    if magic!=MAGIC:return None
    if not 0<length<=MAX_NOTICES or original<64 or original+length>size-FOOTER.size:
        raise ValueError('Invalid binary notice trailer bounds')
    stream.seek(original);body=stream.read(length)
    padding=stream.read(size-FOOTER.size-original-length)
    if len(padding)>=512 or any(byte!=255 for byte in padding) or hashlib.sha256(body).digest()!=digest:
        raise ValueError('Damaged binary notice trailer')
    return dict(original_bytes=original,notice_bytes=length,notice_sha256=digest.hex(),bytes=size)


def inspect(path):
    """Read and verify a trailer. No writes and no executable loading."""
    path=Path(path)
    with path.open('rb') as stream:return _footer(stream,path.stat().st_size)


def _validate(stream,original,kind):
    stream.seek(0);header=bytearray(stream.read(min(original,576)))
    if kind=='nds':
        if original<576 or crc16(header[:0x15e])!=struct.unpack_from('<H',header,0x15e)[0]:
            raise ValueError('Not a valid NDS header')
        for at in (0x20,0x30):
            offset,entry,destination,size=struct.unpack_from('<4I',header,at)
            if offset<512 or not size or offset+size>original:
                raise ValueError('Invalid NDS executable bounds')
        return header
    if kind=='pe':
        if header[:2]!=b'MZ' or len(header)<64:raise ValueError('Not a PE executable')
        at=struct.unpack_from('<I',header,0x3c)[0]
        if at<64 or at+24>original:raise ValueError('Invalid PE header bounds')
        stream.seek(at)
        if stream.read(4)!=b'PE\0\0':raise ValueError('Invalid PE signature')
        return None
    if kind=='zlib':
        if original>16*1024*1024:raise ValueError('Unexpected compressed template size')
        stream.seek(0);packed=stream.read(original);decoder=zlib.decompressobj()
        data=decoder.decompress(packed,16*1024*1024+1)
        if len(data)>16*1024*1024 or not decoder.eof or decoder.unused_data or decoder.unconsumed_tail:
            raise ValueError('Invalid compressed engine template')
        return None
    raise ValueError('Unknown binary notice format')


def attach(path,notices,kind):
    """Modify only a fresh, singly-linked output owned by the caller."""
    path=Path(path);notices=bytes(notices)
    if not 0<len(notices)<=MAX_NOTICES:raise ValueError('Invalid notice text size')
    notices.decode('utf8')
    digest=hashlib.sha256(notices).digest();before=path.stat()
    with path.open('rb') as stream:
        prior=_footer(stream,before.st_size)
        original=prior['original_bytes'] if prior else before.st_size
        header=_validate(stream,original,kind)
    if prior:
        if prior['notice_sha256']!=digest.hex() or prior['notice_bytes']!=len(notices):
            raise ValueError('Binary already contains different notices')
        if kind=='nds':
            field=0x210 if header[0x12]&2 else 0x80
            if struct.unpack_from('<I',header,field)[0]!=before.st_size:
                raise ValueError('NDS notice trailer size does not match header')
        return dict(prior,format=kind,already_attached=True,added_bytes=0)
    if before.st_nlink!=1 or path.is_symlink():
        raise ValueError('Refuse to alter linked or shared binary outputs')
    pad=(-original-len(notices)-FOOTER.size)%512
    total=original+len(notices)+pad+FOOTER.size
    if kind=='nds':
        if total>512*1024*1024:raise ValueError('NDS exceeds 512 MiB after notices')
        field=0x210 if header[0x12]&2 else 0x80
        struct.pack_into('<I',header,field,total)
        header[0x14]=max(header[0x14],max(0,(total-1).bit_length()-17))
        struct.pack_into('<H',header,0x15e,crc16(header[:0x15e]))
    current=path.stat()
    if (current.st_ino,current.st_size,current.st_mtime_ns,current.st_nlink)!=(before.st_ino,before.st_size,before.st_mtime_ns,1):
        raise ValueError('Binary changed before notice attachment')
    with path.open('r+b') as stream:
        stream.seek(original);stream.write(notices);stream.write(b'\xff'*pad)
        stream.write(FOOTER.pack(MAGIC,original,len(notices),digest))
        if header is not None:stream.seek(0);stream.write(header)
        stream.flush();os.fsync(stream.fileno())
    proof=inspect(path)
    if not proof or proof['bytes']!=total or proof['notice_sha256']!=digest.hex():
        raise ValueError('Binary notice verification failed')
    return dict(proof,format=kind,already_attached=False,added_bytes=total-original)


def run(argv=None):
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--kind',choices=('nds','pe','zlib'),required=True)
    parser.add_argument('--notices',required=True)
    parser.add_argument('binary')
    args=parser.parse_args(argv)
    result=attach(args.binary,Path(args.notices).read_bytes(),args.kind)
    print('Preserved binary notices:',result['notice_sha256'],'bytes:',result['bytes'])


if __name__=='__main__':run()
