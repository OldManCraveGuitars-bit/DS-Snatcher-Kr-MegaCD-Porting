"""Original IP copyright and DATA_D2 title planes, no image/capture input."""
import struct,hashlib
from convert_resource_archive import bootstrap
from resource_formats import decode_lzkn1
from native_resources import Resource,Graphics

def pack(vram,palette,width=64):
    header=bytearray(172)
    struct.pack_into('<4sHHIIIHHHHHHhhhhHH',header,0,b'NSBG',2,0,172,44,172,
                     width,64,0,0,256,224,0,0,0,0,0xe000,0xc000)
    struct.pack_into('>64H',header,44,*palette)
    return bytes(header)+bytes(vram)

def build(source):
    original,_=bootstrap(source)
    if 'native-boot-ip.bin' not in original:raise ValueError('Native copyright requires original disc bootstrap')
    ip=original['native-boot-ip.bin'];vram=bytearray(65536)
    font=decode_lzkn1(ip,0x226e);vram[0x2000:0x2000+len(font)]=font
    text=ip[0x2a94:0x2a94+107]
    # Original FF18E4 jobs use byte counts, converted to words by FF1864.
    # The original glyph codes are pre-encoded in IP; do not redraw strings.
    for index,dest,n in ((0,0xe294,22),(12,0xe39c,4),(14,0xe482,60),
                         (44,0xe704,58),(74,0xea94,22),(86,0xeb8c,42)):
        struct.pack_into('>'+str(n//2)+'H',vram,dest,*[0x100+x for x in text[index:index+n//2]])
    palette=list(struct.unpack_from('>16H',ip,0x14be+6*32))*4
    copyright=pack(vram,palette)
    r=Resource(4,source);g=Graphics(r);g.scene(0)
    # Original title backdrop uses plane A atE000. Source NEW GAME and menu
    # actors are replaced by the already-approved lower Touch/Start/Load UI.
    title=pack(g.vram,g.palette[:64],32)
    outputs={'native-copyright-r22.nsbg':copyright,'native-title-r22.nsbg':title}
    report=dict(source_writes=0,screen_pixel_inputs=False,observed_frame_inputs=False,
        copyright=dict(ip_sha256=hashlib.sha256(ip).hexdigest(),font_source_offset=0x226e,
                       encoded_text_source_offset=0x2a94,minimum_hold_vblanks=180),
        title=dict(source_file=r.name,source_sha256=hashlib.sha256(r.data).hexdigest(),scene=0,
                   loads=[dict(source=p-0x28000,destination=d,bytes=n) for p,d,n in g.loads],
                   limits=['Original idle title remained static for960 observed source ticks; other title transitions/controller behavior require separate verification.']),
        outputs=[dict(name=n,bytes=len(b),sha256=hashlib.sha256(b).hexdigest()) for n,b in outputs.items()])
    return outputs,report
