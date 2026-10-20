"""Draw K1's 35 battle poses from original scene loads, maps and palette jobs.

No screenshot, observed VRAM/CRAM, saved emulator state or prebuilt sprite
bank is a conversion input. The small scene palette jobs take precedence
over the full palette installed by the original controller16.
"""
from source_validation import matches_fingerprint, find_fingerprint, unique_fingerprint
import struct
from native_resources import Resource, Graphics, BASE, color, sha


def build(source):
    resource = Resource(25, source)
    graphics = Graphics(resource)
    graphics.scene(0)
    data = resource.data
    sequence = graphics.actors[16]['sequence']
    if not matches_fingerprint(data[sequence:sequence + 10],10,'fac716f436a7b834671b843d438497c3024a040281c6221faeba57736e9a377a'):
        raise ValueError('Unsupported original battle palette controller')
    job = sequence + struct.unpack_from('>h', data, sequence+10)[0]
    if tuple(data[job:job+4]) != (1, 0, 32, 47):
        raise ValueError('Unsupported battle palette job')
    palette_at = struct.unpack_from('>I', data, job+4)[0] - BASE
    palette = [0]*64
    palette[16:64] = struct.unpack_from('>48H', data, palette_at)
    # Two original root-scene commands update individual red shades.
    for _, pointer, first, count in graphics.palette_sources:
        if first+count > 64:
            raise ValueError('Unexpected battle upper-screen palette')
        palette[first:first+count] = struct.unpack_from('>'+str(count)+'H', data, pointer-BASE)
    colors = [color(word) for word in palette]
    table = 0x8b16
    output = bytearray()
    for pose in range(35):
        _, maps = resource.map(BASE+table, pose)
        canvas = [0]*1024
        for dy, size, attr, dx in maps:
            width, height = ((size>>10&3)+1)*8, ((size>>8&3)+1)*8
            if dx < 0 or dy < 0 or dx+width > 32 or dy+height > 32:
                raise ValueError('Original battle pose exceeds its 32x32 bounds')
            for y in range(height):
                for x in range(width):
                    tx = width-1-x if attr&0x800 else x
                    ty = height-1-y if attr&0x1000 else y
                    tile = (attr&2047)+tx//8*(height//8)+ty//8
                    packed = graphics.vram[tile*32+ty%8*4+tx%8//2]
                    index = packed>>4 if tx%2 == 0 else packed&15
                    if index:
                        canvas[(dy+y)*32+dx+x] = colors[(attr>>13&3)*16+index]
        output += struct.pack('<1024H', *canvas)
    return bytes(output), dict(source_file=resource.name, source_sha256=sha(data),
        source_sprite_table=table, palette_job=job, source_frames=35,
        source_writes=0, captured_inputs=False, preconverted_asset_inputs=False,
        sha256=sha(output))
