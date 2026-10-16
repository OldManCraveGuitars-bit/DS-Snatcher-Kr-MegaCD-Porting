"""Load audited helper binaries after restoring source-only lookup tables."""
from pathlib import Path
import hashlib
import json
import os
import struct


def restore(payload, records, source):
    data = bytearray(payload)
    for row in records:
        start, size = row['offset'], row['size']
        if start < 0 or size <= 0 or start+size > len(data) or any(data[start:start+size]):
            raise ValueError('Invalid masked source table')
        original = source.read_file(row['source'])[row['source_offset']:row['source_offset']+size]
        if len(original) != size or hashlib.sha256(original).hexdigest() != row['sha256']:
            raise ValueError('Unsupported original source table')
        data[start:start+size] = original
    return bytes(data)


def prepared(name, destination, source):
    if os.environ.get('SNATCHER_FORCE_SOURCE') == '1':
        return False
    root = Path(__file__).resolve().parents[1]/'host/windows-x64'
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
