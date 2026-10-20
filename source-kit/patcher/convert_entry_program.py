"""Read the original SP06 entry program without a captured timeline.

The portable interpreter consumes the original script bytes. This inspection
records the reachable prefix and its explicit dialogue handoff, and rejects
unrecognized expressions rather than skipping them.
"""
from source_validation import matches_fingerprint, find_fingerprint, unique_fingerprint
import struct
from native_resources import sha,FILE_NAMES

def inspect(data):
    pc=0;roots=[];commands=[];visited=set();files=[]
    while True:
        if pc in visited or pc>=len(data) or data[pc]!=0x30:
            raise ValueError('Invalid or cyclic entry sequence')
        visited.add(pc);roots.append(pc);pc+=1
        while True:
            if pc>=len(data):raise ValueError('Truncated entry sequence')
            if data[pc]==0x2b:
                if pc+3>len(data):raise ValueError('Truncated entry branch')
                target=struct.unpack_from('>H',data,pc+1)[0]
                commands.append(dict(pc=pc,kind='branch',target=target));pc=target;break
            if data[pc]!=0x25 or pc+2>len(data):raise ValueError('Unsupported entry node')
            op=data[pc+1]
            if op==0x43:
                return dict(source_file='SP06.BIN',bytes=len(data),sha256=sha(data),
                    entry_pc=0,roots=roots,handoff_pc=pc,handoff_opcode=op,
                    commands=commands,resource_files=files,recorded_timeline_inputs=False,
                    limits=['Entry prefix only; dialogue continues in the story interpreter.',
                            'No physical CD seek durations or audio busy flags are fabricated.'])
            size={0x00:6,0x04:3,0x0a:3,0x3d:3,0x42:9,0x45:3,0x46:1,0x4a:3}.get(op)
            if size is None or pc+1+size>len(data):raise ValueError('Unsupported or truncated entry expression')
            p=data[pc+1:pc+1+size];row=dict(pc=pc,opcode=op,original_bytes=p.hex())
            if op==0x00 and (p[1]!=0x23 or int.from_bytes(p[2:4],'big')>=64 or int.from_bytes(p[4:6],'big')>5):
                raise ValueError('Unsupported entry actor control')
            if op==0x42 and (not matches_fingerprint(p[1:7],6,'bc1245e7aaa0de9532e05aae33e36c6bd4780c31c9ae852e2fd805999016885f')):
                raise ValueError('Unsupported entry custom command')
            if op==0x45 and int.from_bytes(p[1:3],'big')>=64:raise ValueError('Invalid actor ID')
            if op==0x3d:
                if p[1]>=len(FILE_NAMES):raise ValueError('Invalid entry resource ID')
                row.update(file_id=p[1],scene=p[2]);name=FILE_NAMES[p[1]]
                if name not in files:files.append(name)
            commands.append(row);pc+=1+size

def build(source):
    data=source.read_file('SP06.BIN')
    return data,inspect(data)
