"""Reconstruct additional source tables needed by the current/legacy player."""
import hashlib,struct


def build(source,kind):
    if kind=='legacy_office':
        from derive_legacy_office import build as office
        arrays,header=office(source)
        return arrays,{'office_branch.h':header}
    if kind=='combat_layouts':
        from convert_combat_layouts import build as layouts
        header,report=layouts(source)
        raw=b''.join(struct.pack('<HH18h9Bx',row['scene'],row['allowed_mask'],
            *[cell[key] if cell['enabled'] else 0 for key in ('x','y','frame_base') for cell in row['cells']])
            for row in report['layouts'])
        return {'combat_layouts':raw},{'battle_layouts_r22.h':header}
    raise ValueError('Unsupported derived source table kind: '+str(kind))


def restore(payload,groups,source):
    if not isinstance(groups,list):raise ValueError('Invalid derived source table groups')
    spans=[];writes=[];seen=set()
    for group in groups:
        kind=group['kind']
        if kind in seen:raise ValueError('Duplicate derived source table kind')
        seen.add(kind);arrays,_=build(source,kind);records=group['arrays']
        if set(arrays)!=set(records):raise ValueError('Incomplete derived source table bindings')
        for name,data in arrays.items():
            row=records[name];at=row['offset'];size=row['size']
            if (type(at) is not int or type(size) is not int or at<512 or size!=len(data) or not size
                    or at+size>len(payload) or hashlib.sha256(data).hexdigest()!=row['sha256']):
                raise ValueError('Invalid derived source table: '+name)
            if any(start<at+size and at<end for start,end in spans):raise ValueError('Overlapping derived source tables')
            if any(payload[at:at+size]):raise ValueError('Derived source table region must be erased')
            spans.append((at,at+size));writes.append((at,data))
    result=bytearray(payload)
    for at,data in writes:result[at:at+len(data)]=data
    return bytes(result)
