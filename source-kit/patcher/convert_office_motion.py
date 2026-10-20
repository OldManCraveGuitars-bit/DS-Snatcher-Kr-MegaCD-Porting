"""Original office actor commands for the DS facial animation interpreter.

Inputs are original DATA_H02/SUBCODE and source-locator metadata only.
No observed animation timeline, PCM envelope or screenshot is read.
"""
import struct
from native_resources import Resource, Graphics, BASE, u16, sha

GROUPS = ((0, (8,9,10), (0,1,2,3,4,5,9,10,23)),
          (1, (5,6,7), tuple(range(14,23))),
          (2, (11,12,13), (6,7,8,11,12,13,24,25,26)))

def build(bindings, source):
    resource = Resource(10, source)
    subcode = source.read_file('SUBCODE.BIN') if hasattr(source,'read_file') else (source/'SUBCODE.BIN').read_bytes()
    dispatch = subcode[0x13fec-0xd400:0x13fec-0xd400+174]
    assert len(dispatch) == 174
    voices = []
    for voice in range(86,95):
        scene = dispatch[voice]
        assert scene == voice-82, (voice,scene)
        graphics = Graphics(resource); graphics.scene(scene)
        assert not graphics.unknown
        assert len(graphics.actors) == 3
        actors = []
        for actor_id, definition in graphics.actors.items():
            state = bytearray(80)
            struct.pack_into('>H',state,0,1)
            state[0x25] = definition['flags']
            struct.pack_into('>h',state,6,definition['x'])
            struct.pack_into('>h',state,10,definition['y'])
            struct.pack_into('>H',state,0x28,65535)
            struct.pack_into('>I',state,0x2e,definition['maps'])
            struct.pack_into('>I',state,0x32,BASE+definition['sequence'])
            pose = graphics.initial_pose(definition)
            assert pose is not None
            state[0x16] = definition['attr']
            map_offset,_ = resource.map(definition['maps'],pose)
            struct.pack_into('>I',state,0x36,BASE+map_offset)
            actors.append((actor_id,bytes(state)))
        voices.append((voice,scene,actors))
    lookup = []
    for slot,actors,frames in GROUPS:
        graphics = Graphics(resource); graphics.scene((4,6,5)[slot])
        eyes,mouth,body = (graphics.actors[i] for i in actors)
        pairs = set()
        for frame in frames:
            row = bindings['frames'][frame]
            eye_pose,mouth_pose,body_pose = row['poses']
            assert body_pose == 0
            pair = (eye_pose,mouth_pose)
            assert pair not in pairs; pairs.add(pair)
            eye_map,_ = resource.map(eyes['maps'],eye_pose)
            mouth_map,_ = resource.map(mouth['maps'],mouth_pose)
            body_map,_ = resource.map(body['maps'],body_pose)
            lookup.append((slot,frame,BASE+eye_map,BASE+mouth_map,BASE+body_map))
        assert pairs == {(eye,mouth) for eye in (1,2,3) for mouth in (4,5,6)}
    payload = bytearray(32+len(voices)*12)
    for i,(voice,scene,actors) in enumerate(voices):
        offset = len(payload)
        struct.pack_into('<HBBII',payload,32+i*12,voice,scene,len(actors),offset,0)
        for actor_id,state in actors:
            payload += struct.pack('<B3x80s',actor_id,state)
    lookup_offset = len(payload)
    for slot,frame,eyes,mouth,body in lookup:
        payload += struct.pack('<BBHIII',slot,frame,0,eyes,mouth,body)
    resource_offset = len(payload)
    payload += resource.data
    struct.pack_into('<4sHHIIIIII',payload,0,b'NSAR',1,len(voices),32,
                     lookup_offset,len(lookup),resource_offset,len(resource.data),BASE)
    result = dict(kind='original_office_actor_commands',voices=list(range(86,95)),
                  bytes=len(payload),sha256=sha(payload),native_pose_pairs=len(lookup),
                  sources=[dict(file='DATA_H02.BIN',sha256=sha(resource.data)),
                           dict(file='SUBCODE.BIN',sha256=sha(subcode))],
                  animation_timeline_inputs=False,screen_pixel_inputs=False,
                  limits=['Facial sequence interpreter subset, not full graphics/audio VM',
                          'DS PCM start gate uses its actual channel clock',
                          'RNG arithmetic is original; global game RNG scheduling is still pending'])
    return bytes(payload),result
