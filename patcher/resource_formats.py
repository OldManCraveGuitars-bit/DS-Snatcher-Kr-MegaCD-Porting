"""Port-owned readers for the original resource formats; no game bytes."""
def decode_lzkn1(data, offset=0):
    if offset < 0 or offset + 3 > len(data):
        raise ValueError('Truncated LZKN1 header')
    expected=int.from_bytes(data[offset:offset+2],'big')
    at=offset+2;bits=0;flags=0;out=bytearray()
    def byte():
        nonlocal at
        if at>=len(data):raise ValueError('Truncated LZKN1 command')
        value=data[at];at+=1;return value
    while True:
        if not bits:flags=byte();bits=8
        token=flags&1;flags>>=1;bits-=1
        if not token:out.append(byte())
        else:
            code=byte()
            if code==31:break
            if code<128:
                length=(code&31)+3;distance=((code<<3)&0x300)|byte()
            elif code<192:
                length=(code>>4)-6;distance=code&15
            else:
                length=code-0xb8;out.extend(byte() for _ in range(length))
                if len(out)>expected:raise ValueError('LZKN1 output exceeds header')
                continue
            if not 0<distance<=len(out):raise ValueError('Invalid LZKN1 distance')
            for _ in range(length):out.append(out[-distance])
        if len(out)>expected:raise ValueError('LZKN1 output exceeds header')
    if len(out)!=expected:raise ValueError('LZKN1 size mismatch')
    return bytes(out)
