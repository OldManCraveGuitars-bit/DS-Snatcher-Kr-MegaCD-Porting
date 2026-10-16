"""Original SEGA/Sonic license graphics from disc security and supplied BIOS.

No BIOS, font, image or game payload is included in this module. The original
license palettes/maps/two Sonic poses are decoded locally from the user's input.
"""
from pathlib import Path
import struct,hashlib
from nemesis import decode as tiles
from enigma import decode as nametable
from convert_resource_archive import bootstrap

SUPPORTED_US_BIOS='b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4'
def build(source,bios_path):
    original,_=bootstrap(source)
    if 'native-boot-security.bin' not in original:raise ValueError('SEGA startup requires the original CUE/BIN or ISO bootstrap')
    security=original['native-boot-security.bin'];bios=Path(bios_path).read_bytes()
    if hashlib.sha256(bios).hexdigest()!=SUPPORTED_US_BIOS:raise ValueError('This prototype supports the verified original USA Mega CD BIOS only')
    vram=bytearray(65536);palette=[0]*64
    for offset,data in ((0x6a38,bios),(0xc,security)):
        first,count=data[offset:offset+2];first//=2
        for i in range(count+1):palette[first+i]=struct.unpack_from('>H',data,offset+2+i*2)[0]
    # BIOS menu font is1bpp; the native BIOS expands it into4bpp index1.
    # BIOS1940 loads only ASCII32..127 at VRAM0400, from BIOS15000.
    for glyph in range(32,128):
        rows=bios[0x14f00+glyph*8:0x14f00+(glyph+1)*8]
        for y,bits in enumerate(rows):
            for pair in range(4):
                vram[glyph*32+y*4+pair]=((bits>>(7-pair*2)&1)<<4)|(bits>>(6-pair*2)&1)
    star=tiles(bios,0x6a9a);vram[0x20:0x20+len(star)]=star
    sega=tiles(security,0x2e);vram[0x2000:0x2000+len(sega)]=sega
    sonic=tiles(security,0x1fe);vram[0x2000+len(sega):0x2000+len(sega)+len(sonic)]=sonic
    logo,offset=nametable(security,0x404,0x6100)
    poses,offset=nametable(security,offset,0x613f)
    assert len(logo)==18*6 and len(poses)==2*3*5
    def rectangle(x,y,w,h,words):
        assert len(words)==w*h
        for row in range(h):struct.pack_into('>'+str(w)+'H',vram,0xc000+((y+row)*64+x)*2,*words[row*w:(row+1)*w])
    rectangle(13,10,18,6,logo);rectangle(8,11,3,5,poses[:15])
    # BIOS1AA0 aligns the stream; the original caller subtracts one byte to
    # reach the ASCII license string. Zero bytes advance one text row.
    text_offset=offset-1;end=security.index(255,text_offset);text=security[text_offset:end]
    x=6;y=22
    for char in text:
        if char==0:x=6;y+=1;continue
        struct.pack_into('>H',vram,0xc000+(y*64+x)*2,char);x+=1
    payload=bytearray(44+128)
    struct.pack_into('<4sHHIIIHHHHHHhhhhHH',payload,0,b'NSBG',2,0,172,44,172,
                     64,64,0,0,320,224,0,0,0,0,0xc000,0xe000)
    struct.pack_into('>64H',payload,44,*palette)
    payload+=vram
    pose_offset=len(payload);payload+=struct.pack('>30H',*poses)
    # Raw source descriptors drive the translated BIOS6B14/6D68 star actors.
    # These are original parameters/maps, never recorded animation frames.
    animation_offset=len(payload)
    payload+=struct.pack('<4sHHIII',b'NSGA',1,80,20,500,32)
    payload+=bios[0x6b88:0x6d68]
    payload+=b''.join(bios[p:p+8] for p in struct.unpack_from('>4I',bios,0x6ac0))
    report=dict(kind='original_sega_license_resources',bytes=len(payload),
        sha256=hashlib.sha256(payload).hexdigest(),bios_sha256=hashlib.sha256(bios).hexdigest(),
        security_sha256=hashlib.sha256(security).hexdigest(),
        sega_tiles=len(sega)//32,sonic_tiles=len(sonic)//32,star_tiles=len(star)//32,
        native_font_glyphs=96,sonic_poses=2,sonic_pose_map_offset=pose_offset,
        star_animation_offset=animation_offset,star_actors=80,
        star_update_passes=245,source_star_parameter_offset=0x6b88,
        screen_pixel_inputs=False,source_writes=0,bundled_bios=False,
        limits=['Native Sonic/star interpreter is separate from the active legacy boot player; boot sound scheduling pending',
                'BIOS is supplied locally; never include it or this generated bank in the asset-free patcher'])
    return bytes(payload),report
