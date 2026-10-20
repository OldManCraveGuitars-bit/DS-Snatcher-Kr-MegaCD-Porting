"""Derive original SP text indexes from a user-owned disc at conversion time.

No original strings or converted script data are distributed in this module.
The SP format's 0x3800 code/text boundary and 0xFF-terminated byte strings are
format rules. All entry offsets, contents and identifiers come from input.
"""
import hashlib
import re


def decode_original(raw):
    return ''.join(chr(value) if 0x20<=value<0x7f else '{raw:%02X}'%value for value in raw)


def build(source,names):
    files=[]
    for name in names:
        if not re.fullmatch(r'SP\d{2}\.BIN',name):
            raise ValueError('Unsupported script name: '+name)
        data=source.read_file(name)
        if len(data)<4:
            raise ValueError('Truncated script: '+name)
        payload=data[0x3800:];offset=0;entries=[]
        while offset<len(payload):
            # File-length padding is not an authored empty entry. Preserve
            # interior empty strings whenever nonzero bytes still follow.
            if not payload[offset:].strip(b'\0'):break
            end=payload.find(b'\xff',offset)
            if end<0:raise ValueError('Unterminated original text: '+name)
            raw=payload[offset:end]
            entries.append(dict(entry_id=name+'/'+str(len(entries)).zfill(4),index=len(entries),
                text_offset='0x%04X'%offset,raw_sha256=hashlib.sha256(raw).hexdigest(),
                raw_size=len(raw),text=decode_original(raw)))
            offset=end+1
        files.append(dict(file_id=name,source_size=len(data),source_sha256=hashlib.sha256(data).hexdigest(),
            prefix_size=min(len(data),0x3800),entry_count=len(entries),entries=entries))
    return dict(schema_version=1,scope='Original SP text indexes derived only from user input',files=files)
