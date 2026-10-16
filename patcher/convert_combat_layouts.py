"""Read K1 battle positions and pose families from the user's original disc."""
import hashlib,struct
from native_resources import Resource

def build(disc):
    resource=Resource(25,disc)
    rows=[]
    markers=bytes((2,4,7,15,18,22,24,29))
    common=None
    for scene in (0,3):
        actors={}
        for pc,op,body in resource.packets(scene):
            if op!=2:continue
            assert len(body)%12==0
            for at in range(0,len(body),12):
                actor,flags,x,y,maps,relative=struct.unpack_from('>BBhhIh',body,at)
                if not 32<=actor<=40:continue
                assert actor not in actors and flags==3 and maps==0x30b16
                sequence=pc+2+at+12+relative
                raw=resource.data[sequence:sequence+124]
                enabled=raw[:8]==markers
                if not enabled:assert raw[:4]==b'\x80\0\0\0'
                base=raw[8] if enabled else 0
                assert base in (0,7,14,21)
                if enabled:
                    normalized=bytearray(raw)
                    for offset in range(8,124,4):
                        op=raw[offset]
                        if op<128:
                            assert base<=op<base+7
                            normalized[offset]-=base
                        else:assert op==0x81
                    if common is None:common=bytes(normalized)
                    assert bytes(normalized)==common,'Different AI animation must be implemented explicitly'
                actors[actor]=dict(cell=actor-32,enabled=enabled,x=x,y=y,frame_base=base,
                    packet_pc=pc,record_pc=pc+2+at,sequence_pc=sequence,maps=maps)
        assert set(actors)==set(range(32,41))
        cells=[actors[i] for i in range(32,41)]
        rows.append(dict(scene=0x1900|scene,allowed_mask=sum(1<<r['cell'] for r in cells if r['enabled']),cells=cells))
    header=['#pragma once','#include <stdint.h>',
        'typedef struct {uint16_t scene,allowed_mask; int16_t x[9],y[9]; uint8_t frame_base[9];} CombatLayout;',
        'static const CombatLayout combat_layouts[] = {']
    for row in rows:
        fields=[]
        for name in ('x','y','frame_base'):
            # Disabled cells have no renderer owner; retain old neutral values.
            fields.append('{'+','.join(str(r[name] if r['enabled'] else 0) for r in row['cells'])+'}')
        header.append('{'+','.join([str(row['scene']),str(row['allowed_mask'])]+fields)+'},')
    header+=['};','#define COMBAT_LAYOUT_COUNT (sizeof(combat_layouts)/sizeof(combat_layouts[0]))']
    report=dict(source_file=resource.name,source_sha256=hashlib.sha256(resource.data).hexdigest(),
        layouts=rows,common_animation_sha256=hashlib.sha256(common).hexdigest(),
        original_writes=0,captured_production_inputs=False,
        limits=['Layout and common animation identity only; this converter does not verify gameplay, audio or full engine RNG timing.'])
    return '\n'.join(header)+'\n',report
