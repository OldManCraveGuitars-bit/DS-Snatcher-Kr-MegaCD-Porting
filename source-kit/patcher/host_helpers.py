"""Load audited helper binaries after restoring source-only lookup tables."""
from pathlib import Path
import hashlib
import json
import os
import struct

_helper_directory = None


def use_directory(path):
    """Select a checked helper bundle for a private release conversion."""
    global _helper_directory
    root = Path(path).resolve(strict=True)
    if not root.is_dir() or not (root/'manifest.json').is_file():
        raise ValueError('Helper directory has no manifest')
    _helper_directory = root


def restore(payload, records, source):
    data = bytearray(payload)
    for row in records:
        start, size = row['offset'], row['size']
        if start < 0 or size <= 0 or start+size > len(data) or any(data[start:start+size]):
            raise ValueError('Invalid masked source table')
        source_bytes=source.read_file(row['source'])
        if 'source_stride' in row or 'element_bytes' in row:
            stride=row['source_stride'];element=row['element_bytes'];at=row['source_offset']
            if (type(stride) is not int or type(element) is not int or type(at) is not int
                    or not 0<element<=stride or size%element or at<0
                    or at+(size//element-1)*stride+element>len(source_bytes)):
                raise ValueError('Invalid strided source table')
            original=b''.join(source_bytes[i:i+element] for i in range(at,at+(size//element)*stride,stride))
        else:
            original = source_bytes[row['source_offset']:row['source_offset']+size]
        if len(original) != size or hashlib.sha256(original).hexdigest() != row['sha256']:
            raise ValueError('Unsupported original source table')
        data[start:start+size] = original
    return bytes(data)


def prepared(name, destination, source):
    if os.environ.get('SNATCHER_FORCE_SOURCE') == '1':
        return False
    root = _helper_directory or Path(__file__).resolve().parents[1]/'host/windows-x64'
    if not (root/'manifest.json').exists():
        return False
    if os.name != 'nt' or struct.calcsize('P') != 8:
        raise ValueError('This patcher requires 64-bit Python on Windows')
    row = json.loads((root/'manifest.json').read_text(encoding='utf8'))[name]
    data = (root/row['file']).read_bytes()
    if hashlib.sha256(data).hexdigest() != row['sha256']:
        raise ValueError('Damaged helper: '+name)
    data = restore(data,row['source_tables'],source)
    if hashlib.sha256(data).hexdigest() != row['restored_sha256']:
        raise ValueError('Helper reconstruction failed: '+name)
    Path(destination).write_bytes(data)
    return True
