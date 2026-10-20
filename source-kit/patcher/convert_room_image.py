"""Render static compatibility pictures through the original room resource VM.

All pixels come from the input disc. Expected hashes may identify a static
frame in the original sequence; they contain no image data.
"""
from pathlib import Path
import ctypes as c
import hashlib
import subprocess
import numpy as np
from PIL import Image

MODULES = ('room','tasks','opening','opening_presentation','callback','scene','actor',
           'palette','graphics','scroll','cards','main_scroll','sprites','background',
           'lzkn1','contact','picture_row')
WRAPPER = '''#include "resource_room.h"
static resource_room r;
static bool sound(void *c,uint32_t e,unsigned a,unsigned ch,resource_scene_devices*d){return true;}
bool begin(const void*p,size_t n,const void*s,size_t sn,unsigned scene){
 resource_background_bind(NULL,0);
 if(!resource_room_begin(&r,p,n,s,sn,scene,0))return false;
 r.devices.sound=sound;return true;
}
bool tick(void){return resource_room_tick(&r);}
bool draw(void*p){return resource_room_draw_native(&r,p);}
unsigned error(void){return r.error;}
'''


def build(source, engine, workspace, compiler, expected):
    src, out = Path(engine)/'source', Path(workspace)
    out.mkdir(exist_ok=False)
    (out/'room.c').write_text(WRAPPER,encoding='utf8')
    command = [str(compiler),'-O2','-shared','-Wno-attributes','-I'+str(src),
        '-I'+str(Path(engine)/'include'),*[str(src/('resource_'+n+'.c')) for n in MODULES],
        str(out/'room.c'),'-o',str(out/'room.dll')]
    from host_helpers import prepared
    if not prepared('room',out/'room.dll',source):
        result = subprocess.run(command,capture_output=True,text=True)
        (out/'build.log').write_text(result.stdout+result.stderr,encoding='utf8')
        result.check_returncode()
    lib = c.CDLL(str((out/'room.dll').resolve()))
    lib.begin.argtypes = [c.c_void_p,c.c_size_t,c.c_void_p,c.c_size_t,c.c_uint]
    lib.begin.restype = lib.tick.restype = lib.draw.restype = c.c_bool
    lib.draw.argtypes = [c.c_void_p]
    sub = source.read_file('SUBCODE.BIN')
    outputs, records = {}, []
    for name,digest in expected.items():
        code = int(name[3:7],16)&0x3fff
        file,scene = code>>8,code&255
        from native_resources import FILE_NAMES
        bank = source.read_file(FILE_NAMES[file])
        if not lib.begin(bank,len(bank),sub,len(sub),scene):
            raise ValueError(('Original room initialization failed',name))
        im = np.zeros((192,256),dtype='<u2')
        found = False
        for frame in range(600):
            if not lib.tick() or not lib.draw(im.ctypes.data):
                break
            # RGB nearest-neighbour reproduces the historical renderer's
            # pixel-center rule. Pillow's I;16 sampler has a different phase.
            rgb = np.stack([im&31,im>>5&31,im>>10&31],axis=-1).astype('uint8')
            rgb = np.asarray(Image.fromarray(rgb[:128,32:224]).resize((256,192),Image.Resampling.NEAREST)).astype('uint16')
            packed = (32768|rgb[:,:,0]|rgb[:,:,1]<<5|rgb[:,:,2]<<10).astype('<u2').tobytes()
            if hashlib.sha256(packed).hexdigest() == digest:
                outputs[name]=packed;found=True;break
        records.append(dict(name=name,matched=found,source_frame=frame,error=lib.error()))
    return outputs, records
