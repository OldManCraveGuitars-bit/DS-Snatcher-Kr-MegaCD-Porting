"""Original resource reader. Gameplay captures are never drawing inputs.

SUBCODE F68E/F708: scene packets and 12-byte actors.
SUBCODE EDА0: map count-1, then (dy,size,pattern,dx) sprite records.
SUBCODE E41A/E576: palette commands; F326: LZKN1 VRAM loads.
"""
from pathlib import Path
import sys, struct, hashlib, json
sys.dont_write_bytecode = True
ROOT = Path(__file__).resolve().parents[1]
ORIGINAL = None
from resource_formats import decode_lzkn1

BASE = 0x28000
FILE_NAMES = ['DATA_A0.BIN', 'DATA_B0.BIN', 'DATA_D0.BIN', 'DATA_D1.BIN',
 'DATA_D2.BIN', 'DATA_F0.BIN', 'DATA_F4.BIN', 'DATA_G0.BIN',
 'DATA_H00.BIN', 'DATA_H01.BIN', 'DATA_H02.BIN', 'DATA_H03.BIN',
 'DATA_H04.BIN', 'DATA_H05.BIN', 'DATA_H06.BIN', 'DATA_H07.BIN',
 'DATA_H08.BIN', 'DATA_H09.BIN', 'DATA_H11.BIN', 'DATA_H12.BIN',
 'DATA_H13.BIN', 'DATA_H14.BIN', 'DATA_H15.BIN', 'DATA_I0.BIN',
 'DATA_J0.BIN', 'DATA_K1.BIN', 'DATA_M0.BIN', 'DATA_O0.BIN',
 'DATA_P0.BIN', 'DATA_Q3.BIN', 'DATA_Q5.BIN', 'DATA_Q8.BIN',
 'DATA_S0.BIN', 'DATA_S1.BIN', 'DATA_S2.BIN', 'DATA_T0.BIN',
 'DATA_U0.BIN', 'DATA_Y00.BIN', 'DATA_Y01.BIN', 'DATA_Y03.BIN',
 'DATA_Y04.BIN', 'DATA_Y05.BIN', 'DATA_Y06.BIN', 'DATA_Y07.BIN',
 'DATA_Y08.BIN', 'DATA_Y09.BIN', 'DATA_Y10.BIN', 'DATA_Y11.BIN',
 'DATA_Y12.BIN', 'DATA_Y13.BIN', 'DATA_Y14.BIN', 'DATA_Y15.BIN',
 'DATA_Y16.BIN']

def u16(b, p): return struct.unpack_from('>H', b, p)[0]
def s16(b, p): return struct.unpack_from('>h', b, p)[0]
def u32(b, p): return struct.unpack_from('>I', b, p)[0]
def sha(b): return hashlib.sha256(b).hexdigest()
def color(word):
    # Same finite DAC levels as the source's RGB565 display, then DS RGB555.
    r, g, b = word & 15, word >> 4 & 15, word >> 8 & 15
    r = r * 2 | r >> 3
    g = g * 4 | g >> 2
    b = b * 2 | b >> 3
    rr, gg, bb = r * 255 // 31, g * 255 // 63, b * 255 // 31
    return 0x8000 | rr >> 3 | (gg >> 3) << 5 | (bb >> 3) << 10

class Resource:
    def __init__(self, file_id, source_dir=ORIGINAL):
        self.file_id = file_id
        self.name = FILE_NAMES[file_id]
        self.data = source_dir.read_file(self.name) if hasattr(source_dir, 'read_file') else (Path(source_dir) / self.name).read_bytes()
        self.table = u32(self.data, 4) - BASE
        assert 0 <= self.table < len(self.data)
        # DATA_D0 has 16 scene pointers and an 8-byte shared relation packet
        # between the table and first referenced scene. Its first offset is
        # therefore not the table byte count.
        if file_id == 2:
            self.count = 16
            assert u16(self.data, self.table) == 0x28
            self.banks = {}
            for scene in range(self.count):
                list(self.packets(scene))
            return
        # Other current files place the first packet directly after the table.
        stop, count = len(self.data) - self.table, 0
        while count * 2 < stop:
            offset = u16(self.data, self.table + count * 2)
            assert offset >= (count + 1) * 2, (self.name, count, offset)
            stop = min(stop, offset)
            count += 1
        assert stop == count * 2
        self.count = count
        self.banks = {}

    def packets(self, scene):
        assert 0 <= scene < self.count, (self.name, scene, self.count)
        p = self.table + u16(self.data, self.table + scene * 2)
        while self.data[p] < 128:
            op, length = self.data[p:p+2]
            end = p + 2 + length
            assert end <= len(self.data)
            yield p, op, self.data[p+2:end]
            p = end

    def bank(self, address):
        p = address - BASE
        assert 0 <= p < len(self.data), (self.name, hex(address))
        if p not in self.banks:
            self.banks[p] = decode_lzkn1(self.data, p)
        return self.banks[p]

    def map(self, address, pose):
        t = address - BASE
        p = t + u16(self.data, t + pose * 2)
        count = u16(self.data, p) + 1
        assert 0 < count <= 64, (self.name, hex(t), pose, count)
        return p, [struct.unpack_from('>hHHh', self.data, p + 2 + i*8)
                   for i in range(count)]

class Graphics:
    def __init__(self, resource):
        self.r = resource
        self.vram = bytearray(65536)
        self.palette = [0] * 128
        self.actors = {}
        self.loads = []
        self.palette_sources = []
        self.unknown = []

    def scene(self, scene):
        for p, op, body in self.r.packets(scene):
            if op == 0:
                kind, delay, start, count = body[:4]
                if kind not in (1, 6):
                    self.unknown.append((p, op, kind))
                    continue
                palette_ptr = u32(body, 4)
                assert u16(body, len(body)-2) == 65535
                start //= 2
                for i in range(count+1):
                    self.palette[start+i] = u16(self.r.data, palette_ptr-BASE+2*i)
                self.palette_sources.append((p, palette_ptr, start, count+1))
            elif op == 1:
                assert len(body) % 8 == 2 and body[-2:] == b'\xff\xff'
                for at in range(0, len(body)-2, 8):
                    pointer, destination = struct.unpack_from('>II', body, at)
                    bank = self.r.bank(pointer)
                    assert destination + len(bank) <= 65536
                    self.vram[destination:destination+len(bank)] = bank
                    self.loads.append((pointer, destination, len(bank)))
            elif op == 2:
                assert len(body) % 12 == 0
                for at in range(0, len(body), 12):
                    actor, flags, x, y, maps, rel = struct.unpack_from('>BBhhIh', body, at)
                    sequence = p + 2 + at + 12 + rel
                    assert 0 <= sequence < len(self.r.data)
                    self.actors[actor] = dict(id=actor, flags=flags, x=x, y=y,
                        maps=maps, sequence=sequence, pc=-1, wait=0, pose=None,
                        attr=0, alive=True, keep=False, loop=0)
            elif op == 4:
                assert len(body) % 2 == 0
                for at in range(0, len(body), 2):
                    parent, child = body[at:at+2]
                    if child in self.actors:
                        self.actors[child]['parent'] = parent
            else:
                self.unknown.append((p, op, None))

    def initial_pose(self, actor):
        p = actor['sequence']
        for _ in range(256):
            cmd, arg, word = struct.unpack_from('>BBH', self.r.data, p)
            if cmd < 128:
                return cmd
            if cmd == 0x81:
                p = actor['sequence'] + 4 * arg
                continue
            if cmd in (0xa1, 0xa7):
                actor['attr'] = arg
            if cmd == 0x80:
                return None
            p += 4
        raise ValueError(('Initial sequence did not reach a pose', actor))

    def draw(self, actor_ids=None, poses=None, rectangle=(0, 0, 256, 224)):
        import numpy as np
        x0,y0,width,height = rectangle
        canvas = np.zeros((height,width), dtype='<u2')
        colors = [color(c) for c in self.palette]
        selected = self.actors if actor_ids is None else actor_ids
        # Earlier SAT entries have priority. Native actor IDs grow in SAT order.
        for actor_id in reversed(sorted(selected)):
            a = self.actors[actor_id]
            if not a['alive']: continue
            pose = poses.get(actor_id) if poses is not None and actor_id in poses else self.initial_pose(a)
            if pose is None: continue
            _, sprites = self.r.map(a['maps'], pose)
            for dy,size,attr,dx in reversed(sprites):
                w, h = ((size>>10&3)+1)*8, ((size>>8&3)+1)*8
                if a['attr'] & 128: attr = (attr&0x9fff) | (a['attr']&3)<<13
                sx,sy=a['x']+dx-x0,a['y']+dy-y0
                for y in range(max(0,sy),min(height,sy+h)):
                    for x in range(max(0,sx),min(width,sx+w)):
                        tx,ty=x-sx,y-sy
                        if attr&0x800:tx=w-1-tx
                        if attr&0x1000:ty=h-1-ty
                        tile=(attr&2047)+(tx//8)*(h//8)+(ty//8)
                        value=self.vram[tile*32+(ty%8)*4+(tx%8)//2]
                        index=value>>4 if tx%2==0 else value&15
                        if index:canvas[y,x]=colors[(64 if a['y']>=144 else 0)+(attr>>13&3)*16+index]
        return canvas

def preview(words):
    import numpy as np
    a=words.astype(np.uint32)
    rgb=np.stack(((a&31)*255//31,((a>>5)&31)*255//31,
                  ((a>>10)&31)*255//31),axis=-1).astype(np.uint8)
    from PIL import Image
    return Image.fromarray(rgb)
