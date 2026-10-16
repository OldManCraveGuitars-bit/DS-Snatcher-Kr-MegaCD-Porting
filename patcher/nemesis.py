"""Bounded Nemesis 4bpp tile decompression used by original SEGA startup.

Decoded from the original BIOS E84/E94 algorithm; this file has no image data.
"""
import struct

def decode(data,offset):
    def byte():
        nonlocal offset
        if offset>=len(data):raise ValueError('Truncated Nemesis source')
        value=data[offset];offset+=1;return value
    if offset<0 or offset+2>len(data):raise ValueError('Invalid Nemesis address')
    header=struct.unpack_from('>H',data,offset)[0];offset+=2
    tiles=header&0x7fff
    if not 0<tiles<=2048:raise ValueError('Unsupported Nemesis tile count')
    xor=bool(header&0x8000);table=[None]*256;nibble=0
    value=byte()
    while value!=255:
        nibble=value&15
        while True:
            descriptor=byte()
            if descriptor&128:
                value=descriptor;break
            length=descriptor&15
            if not 1<=length<=8:raise ValueError('Invalid Nemesis code width')
            run=(descriptor>>4&7)+1;code=byte()
            if code>=(1<<length):raise ValueError('Invalid Nemesis prefix')
            first=code<<(8-length)
            for index in range(first,first+(1<<(8-length))):
                table[index]=(length,run,nibble)
    bits=0;available=0
    def peek(width):
        nonlocal bits,available
        while available<width:
            bits=(bits<<8)|byte();available+=8
        return bits>>(available-width)&((1<<width)-1)
    def take(width):
        nonlocal bits,available
        value=peek(width);available-=width;bits&=(1<<available)-1;return value
    words=tiles*8;output=bytearray();packed=0;count=0;previous=0
    while len(output)<tiles*32:
        prefix=peek(8)
        if prefix>=0xfc:
            take(6);value=take(7);run=(value>>4)+1;nibble=value&15
        else:
            item=table[prefix]
            if item is None:raise ValueError('Undefined Nemesis prefix')
            length,run,nibble=item;take(length)
        for _ in range(run):
            packed=(packed<<4)|nibble;count+=1
            if count==8:
                value=packed^previous if xor else packed
                output+=struct.pack('>I',value)
                previous=value;packed=count=0
                if len(output)==words*4:break
    return bytes(output)
