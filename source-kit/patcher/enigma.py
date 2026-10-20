"""Bounded original Enigma nametable decoding for the startup graphics."""
import struct

def decode(data,offset,base=0):
    if offset<0 or offset+6>len(data):raise ValueError('Invalid Enigma header')
    width,flags=data[offset:offset+2]
    if not 1<=width<=11 or flags&~31:raise ValueError('Invalid Enigma inline attributes')
    increasing,common=struct.unpack_from('>HH',data,offset+2)
    increasing=(increasing+base)&65535;common=(common+base)&65535
    position=(offset+6)*8;result=[]
    def take(count):
        nonlocal position
        if position+count>len(data)*8:raise ValueError('Truncated Enigma stream')
        value=0
        for _ in range(count):
            value=(value<<1)|((data[position//8]>>(7-position%8))&1);position+=1
        return value
    def inline():
        attributes=0
        for bit in range(4,-1,-1):
            if flags&(1<<bit):attributes|=take(1)<<(11+bit)
        return ((take(width)+base)&65535)|attributes
    while len(result)<8192:
        if not take(1):
            mode=take(1);count=take(4)+1
            for _ in range(count):
                result.append(common if mode else increasing)
                if not mode:increasing=(increasing+1)&65535
        else:
            mode=take(2);count=take(4)
            if mode==3 and count==15:return result,((position+15)//16)*2
            count+=1
            if mode==3:
                result.extend(inline() for _ in range(count))
            else:
                value=inline()
                for _ in range(count):
                    result.append(value)
                    if mode==1:value=(value+1)&65535
                    elif mode==2:value=(value-1)&65535
    raise ValueError('Enigma nametable exceeds supported geometry')
