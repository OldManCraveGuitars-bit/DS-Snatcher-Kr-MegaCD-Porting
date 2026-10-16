"""Original office background: tiles, native plane tables, palette and sprites.

This emits drawing resources, not bitmap screenshots or recorded frames.
The current binding is the bounded office root; full scene commands follow.
"""
import struct
from native_resources import Resource,Graphics,BASE,sha

def build_reception(source):
    """H00 counter root from original packets/maps; no observed state inputs.

    This static root is a migration building block. The H15/H01/H00 entry
    sequence and H00 actor VM must drive their own transitions separately.
    """
    resource=Resource(8,source);graphics=Graphics(resource);graphics.scene(0)
    sub=source.read_file('SUBCODE.BIN') if hasattr(source,'read_file') else (source/'SUBCODE.BIN').read_bytes()
    for pc in range(0xf600-0xd400,0xf640-0xd400,8):
        assert sub[pc:pc+2]==b'\x33\xfc'
        value,destination=struct.unpack_from('>HI',sub,pc+2)
        assert 0xb6100<=destination<0xb6110
        graphics.palette[(destination-0xb6100)//2]=value
    sprites=[];poses={}
    for actor_id,actor in sorted(graphics.actors.items()):
        # Original lower-screen UI controller9 lies outside the scene crop.
        # The artwork actors16..19 are defined by the resource packet.
        if actor['y']>=144:continue
        # F7F0 continues after93 until the first pose/wait. A97 wait hides
        # the actor and does not publish a future frame (actor16's big face).
        sequence=actor['sequence'];pc=sequence;pose=None
        for budget in range(256):
            op,arg=resource.data[pc:pc+2]
            if op<0x80:pose=op;break
            if op in (0x80,0x97):break
            if op==0x81:pc=sequence+arg*4
            else:pc+=4
        else:raise ValueError('Counter initial pose exceeds source command budget')
        if pose is None:continue
        poses[actor_id]=pose
        _,maps=resource.map(actor['maps'],pose)
        for dy,size,attr,dx in maps:
            if actor['attr']&128:attr=(attr&0x9fff)|(actor['attr']&3)<<13
            sprites.append((actor['x']+dx,actor['y']+dy,size,attr))
    palette_offset=40;sprite_offset=168;vram_offset=sprite_offset+len(sprites)*8
    payload=bytearray(vram_offset)
    struct.pack_into('<4sHHIIIHHHHHHhhhh',payload,0,b'NSBG',1,len(sprites),vram_offset,palette_offset,sprite_offset,32,64,32,8,192,128,0,0,0,0)
    struct.pack_into('>64H',payload,palette_offset,*graphics.palette[:64])
    for i,sprite in enumerate(sprites):struct.pack_into('<hhHH',payload,sprite_offset+i*8,*sprite)
    payload+=graphics.vram
    report=dict(kind='original_reception_tile_planes',file=resource.name,scene=0,source_sha256=sha(resource.data),bytes=len(payload),sha256=sha(payload),
                source_initial_poses=poses,native_crop=[32,8,192,128],screen_pixel_inputs=False,observed_frame_inputs=False,
                limits=['Static original counter root; not the entry scene/actor VM','Original initial visible poses17/18/19; later actor animation is separate'])
    return bytes(payload),report

def build(source):
    resource=Resource(10,source);graphics=Graphics(resource);graphics.scene(0)
    sub=source.read_file('SUBCODE.BIN') if hasattr(source,'read_file') else (source/'SUBCODE.BIN').read_bytes()
    for pc in range(0xf600-0xd400,0xf640-0xd400,8):
        assert sub[pc:pc+2]==b'\x33\xfc'
        value,destination=struct.unpack_from('>HI',sub,pc+2)
        assert 0xb6100<=destination<0xb6110
        graphics.palette[(destination-0xb6100)//2]=value
    # Actor15's original palette job (88 + sequence-relative pointer).
    actor=graphics.actors[15];sequence=actor['sequence']
    assert resource.data[sequence:sequence+8:4]==b'\x97\x88'
    palette_job=sequence+struct.unpack_from('>h',resource.data,sequence+6)[0]
    kind,delay,start,count=resource.data[palette_job:palette_job+4]
    assert (kind,delay,start,count)==(1,0,32,47)
    pointer=struct.unpack_from('>I',resource.data,palette_job+4)[0]-BASE
    for i in range(count+1):graphics.palette[start//2+i]=struct.unpack_from('>H',resource.data,pointer+i*2)[0]
    # Original controller25's 8D/01 sets plane B vertical origin to -88.
    controller=graphics.actors[25]['sequence']
    op,axis,vertical=struct.unpack_from('>BBh',resource.data,controller)
    assert (op,axis)==(0x8d,1)
    # Root sprite actor18 / pose1 is present in the original office root.
    actor=graphics.actors[18];_,maps=resource.map(actor['maps'],1)
    sprites=[]
    for dy,size,attr,dx in maps:
        sprites.append((actor['x']+dx,actor['y']+dy,size,attr))
    palette_offset=40;sprite_offset=palette_offset+128
    vram_offset=sprite_offset+len(sprites)*8
    payload=bytearray(vram_offset)
    struct.pack_into('<4sHHIIIHHHHHHhhhh',payload,0,b'NSBG',1,len(sprites),
                     vram_offset,palette_offset,sprite_offset,32,64,32,8,192,128,0,0,0,vertical)
    struct.pack_into('>64H',payload,palette_offset,*graphics.palette[:64])
    for i,(x,y,size,attr) in enumerate(sprites):struct.pack_into('<hhHH',payload,sprite_offset+i*8,x,y,size,attr)
    payload+=graphics.vram
    report=dict(kind='original_background_tile_planes',file=resource.name,scene=0,
        source_sha256=sha(resource.data),bytes=len(payload),sha256=sha(payload),
        palette_job_offset=palette_job,scroll_command_offset=controller,
        native_sprite_actor=18,native_sprite_pose=1,
        vram_loads=[dict(source_offset=ptr-BASE,destination=dst,decoded_bytes=n) for ptr,dst,n in graphics.loads],
        screen_pixel_inputs=False,observed_frame_inputs=False,
        limits=['Bounded office root binding; not the complete scene animation VM',
                'Renderer supports changing scroll/palette; native commands must drive those changes'])
    return bytes(payload),report
