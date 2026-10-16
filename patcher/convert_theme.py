"""Pack the port's user-authored decoration without changing its colors."""
import numpy as np
from PIL import Image


def build(path):
    rgb=np.asarray(Image.open(path).convert('RGB').resize((256,192),Image.Resampling.BOX),dtype=np.uint16)
    pixels=(32768|(rgb[:,:,0]>>3)|(rgb[:,:,1]>>3)<<5|(rgb[:,:,2]>>3)<<10).astype('<u2')
    colors,inverse=np.unique(pixels&32767,return_inverse=True)
    if len(colors)>127:raise ValueError('Decorative palette exceeds reserved colors')
    palette=np.zeros(128,dtype='<u2');palette[1:len(colors)+1]=colors
    codes=(inverse.reshape(192,256)+1).astype('uint8')
    tiles=b''.join(codes[y:y+8,x:x+8].tobytes() for y in range(0,192,8) for x in range(0,256,8))
    return {'junker-upper-r1.rgb555':pixels.tobytes(),'junker-upper-tv-r22.bin':palette.tobytes()+tiles}
