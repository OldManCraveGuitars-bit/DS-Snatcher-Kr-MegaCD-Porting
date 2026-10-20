"""Compile original 4bpp tiles/maps/palettes into a DS drawing resource.

No PNG, video, WAV, rendered frame bank or emulator state is read here.
"""
from pathlib import Path
import json, struct
from native_resources import Resource, Graphics, BASE, ROOT, sha

def build(bindings, source_dir):
    resources, tiles, tile_ids, frames, provenance = {}, [], {}, [], []
    for row in bindings['frames']:
        assert row['frame'] == len(frames)
        fid = row['file_id']
        if fid not in resources:
            resources[fid] = Resource(fid, source_dir)
        r = resources[fid]; g = Graphics(r); g.scene(row['scene'])
        assert not g.unknown, (r.name, row['scene'], g.unknown)
        x0,y0,width,height = row['rectangle']; assert (width,height)==(64,64)
        sprites=[]
        for actor_id, pose in zip(row['actors'], row['poses']):
            a=g.actors[actor_id];g.initial_pose(a)
            map_offset, records=r.map(a['maps'],pose)
            for dy,size,attr,dx in records:
                if a['attr']&128:attr=(attr&0x9fff)|(a['attr']&3)<<13
                w,h=(size>>10&3)+1,(size>>8&3)+1
                tile=attr&2047;refs=[]
                for index in range(w*h):
                    packed=bytes(g.vram[(tile+index)*32:(tile+index+1)*32])
                    if packed not in tile_ids:
                        tile_ids[packed]=len(tiles);tiles.append(packed)
                        owners=[(ptr,dst,length) for ptr,dst,length in g.loads
                                if dst<=(tile+index)*32<dst+length]
                        assert owners, ('Unbound original pattern',r.name,tile+index)
                        ptr,dst,_=owners[-1]
                        provenance.append(dict(tile=tile_ids[packed],file=r.name,
                            lzkn1_offset=ptr-BASE,source_tile=((tile+index)*32-dst)//32,
                            sha256=sha(packed)))
                    refs.append(tile_ids[packed])
                sprites.append((a['x']+dx-x0,a['y']+dy-y0,w,h,
                                attr>>13&3,(attr>>11)&3,refs))
        # Earlier actors/maps own earlier SAT entries and win pixel overlap.
        frame=bytearray(struct.pack('<HBB',len(sprites),fid,row['scene']))
        frame+=struct.pack('>64H',*g.palette[64:128])
        directory=len(frame);frame+=bytes(12*len(sprites))
        for i,(x,y,w,h,palette,flips,refs) in enumerate(sprites):
            at=len(frame)
            struct.pack_into('<hhBBBBI',frame,directory+i*12,x,y,w,h,palette,flips,at)
            frame+=struct.pack('<'+'H'*len(refs),*refs)
        frames.append(bytes(frame))
    payload=bytearray(16+len(frames)*4)
    for i,frame in enumerate(frames):
        struct.pack_into('<I',payload,16+i*4,len(payload));payload+=frame
    tile_offset=len(payload)
    payload+=b''.join(tiles)
    struct.pack_into('<4sHHII',payload,0,b'NSPR',1,len(frames),len(tiles),tile_offset)
    report=dict(kind='original_4bpp_sprite_resource',frames=len(frames),
        original_tiles=len(tiles),bytes=len(payload),sha256=sha(payload),
        sources=[dict(file=r.name,sha256=sha(r.data)) for r in resources.values()],
        pattern_bindings=provenance,source_writes=0,screen_pixel_inputs=False,
        limits=['Draws original tiles and original sprite maps at runtime',
                'Office86-94 uses a separate native actor command bank; other rooms still have observed timing during migration'])
    return bytes(payload),report
