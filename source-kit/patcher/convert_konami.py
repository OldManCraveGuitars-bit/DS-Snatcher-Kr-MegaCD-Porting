"""Original IP logo tiles/maps/palettes, including native beam-plane writes."""
import struct,hashlib
from convert_resource_archive import bootstrap
from resource_formats import decode_lzkn1
def build(source):
    original,_=bootstrap(source)
    if 'native-boot-ip.bin' not in original:raise ValueError('KONAMI startup requires original disc bootstrap')
    ip=original['native-boot-ip.bin'];vram=bytearray(65536)
    loads=[]
    for start,dest in ((0x159e,0),(0x1fc8,0xc000),(0x1eb4,0xe000)):
        data=decode_lzkn1(ip,start);vram[dest:dest+len(data)]=data
        loads.append(dict(ip_offset=start,vram=dest,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()))
    # Native IP FF2DDA..FF2E26. These beam/background writes are part of the
    # original startup; no observed framebuffer or tile-map dump is consulted.
    for address,count,value in ((0xe140,1536,0x6001),(0xe0c0,32,0x6002),
        (0xe140,32,0x6003),(0xe080,1,0x6002),(0xe100,1,0x6003)):
        struct.pack_into('>'+str(count)+'H',vram,address,*([value]*count))
    palettes=ip[0x14be:0x159e];assert len(palettes)==224
    palette=b''.join(palettes[n*32:n*32+32] for n in (4,4,4,5))
    # Original boot VDP reg16=11:64x64 cells even in H32 display mode.
    payload=bytearray(172)
    struct.pack_into('<4sHHIIIHHHHHHhhhhHH',payload,0,b'NSBG',2,0,172,44,172,
        64,64,0,0,256,224,0,0,0,0,0xe000,0xc000)
    payload[44:172]=palette;payload+=vram
    payload+=struct.pack('<4sHHI',b'NKON',1,7,12)+palettes+ip[0x2892:0x2896]
    report=dict(kind='original_konami_startup_resources',bytes=len(payload),
        sha256=hashlib.sha256(payload).hexdigest(),ip_sha256=hashlib.sha256(ip).hexdigest(),
        native_loads=loads,palette_banks=7,source_palette_offset=0x14be,
        source_writes=0,screen_pixel_inputs=False,observed_frame_inputs=False,
        limits=['Source animation interpreter is separate from active legacy boot; original FM3F playback pending'])
    return bytes(payload),report
