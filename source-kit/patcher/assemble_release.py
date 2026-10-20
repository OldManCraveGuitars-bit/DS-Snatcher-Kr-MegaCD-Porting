"""Assemble regenerated resources directly into one unpublished NDS file.

The writer retains no second media tree and publishes only after every planned
resource has the expected byte count and hash. Original lookup tables in the
engine are restored exclusively from the user's SourceDisc.
"""
from pathlib import Path
import hashlib
import os
import struct
import zlib

from assemble_demo import crc16
from host_helpers import restore
import json


def digest_file(path):
    digest = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(4*1024*1024), b''):
            digest.update(block)
    return digest.hexdigest()


class ReleaseWriter:
    def __init__(self, source, engine, output, expected):
        self.output = Path(output)
        self.partial = self.output.with_name(self.output.name+'.partial')
        self.file = None
        if self.output.exists() or self.partial.exists():
            raise ValueError('NDS output already exists')
        self.expected, self.written, self.offsets = expected, set(), {}
        engine = Path(engine)
        meta = json.loads((engine/'runner.json').read_text(encoding='utf8'))
        notice_meta = meta.get('binary_notices', {})
        self.notices = (engine.parent/'licenses/BINARY-NOTICES.txt').read_bytes()
        if (notice_meta.get('file') != 'BINARY-NOTICES.txt'
                or len(self.notices) != notice_meta.get('bytes')
                or hashlib.sha256(self.notices).hexdigest() != notice_meta.get('sha256')):
            raise ValueError('Damaged binary notice text')
        packed = (engine/'runner.zlib').read_bytes()
        if hashlib.sha256(packed).hexdigest() != meta['sha256']:
            raise ValueError('Damaged port engine')
        prefix = bytearray(restore(zlib.decompress(packed), meta['source_tables'], source))
        if 'derived_source_tables' in meta:
            from derived_source_tables import restore as restore_derived
            prefix = bytearray(restore_derived(prefix, meta['derived_source_tables'], source))
        if 'control_graph' in meta:
            from restore_story_graph import restore_from_source
            prefix = bytearray(restore_from_source(prefix, meta['control_graph'], source, engine, self.output.parent))
        if hashlib.sha256(prefix).hexdigest() != meta['restored_sha256']:
            raise ValueError('Engine restoration mismatch')
        if len(prefix) < 512:
            raise ValueError('Truncated engine header')
        extensions = []
        declared = {row['header_field']: row for row in meta.get('extensions', [])}
        if len(declared) != len(meta.get('extensions', [])) or set(declared)-{0x1c0, 0x1d0}:
            raise ValueError('Invalid DSi executable directory')
        for label, field in (('arm9i', 0x1c0), ('arm7i', 0x1d0)):
            if len(prefix) < field+16:
                raise ValueError('Truncated DSi header')
            offset, auxiliary, destination, size = struct.unpack_from('<4I', prefix, field)
            if not size:
                if field in declared:
                    raise ValueError('Unexpected DSi executable')
                continue
            if field not in declared:
                raise ValueError('Missing DSi executable: '+label)
            row = declared[field]
            if (row['name'] != label or row['file'] != label+'.zlib'
                    or (row['bytes'], row['auxiliary'], row['destination']) != (size, auxiliary, destination)):
                raise ValueError('DSi executable metadata mismatch')
            blob = (engine/row['file']).read_bytes()
            if hashlib.sha256(blob).hexdigest() != row['sha256']:
                raise ValueError('Damaged DSi executable')
            code = zlib.decompress(blob)
            if len(code) != size or hashlib.sha256(code).hexdigest() != row['restored_sha256']:
                raise ValueError('DSi executable identity mismatch')
            extensions.append((field, code))
        banner = zlib.decompress((engine/'banner.zlib').read_bytes())
        if hashlib.sha256(banner).hexdigest() != meta['banner_sha256']:
            raise ValueError('Damaged port banner')
        fnt = bytearray(struct.pack('<IHH', 8, 0, 1))
        names = sorted(expected)
        if not names or len(names) > 65535:
            raise ValueError('Invalid NitroFS file count')
        for name in names:
            if not name.isascii() or not 0 < len(name) < 128 or Path(name).name != name or ':' in name or '\\' in name:
                raise ValueError('Unsafe NitroFS name')
            fnt += bytes([len(name)]) + name.encode('ascii')
        fnt += b'\0'
        self.total = len(prefix)

        def allocate(size):
            start = (self.total + 511) & ~511
            self.total = start + size
            return start

        fnt_at = allocate(len(fnt))
        fat = bytearray(8*len(names))
        fat_at = allocate(len(fat))
        banner_at = allocate(len(banner))
        for i, name in enumerate(names):
            size = expected[name]['bytes']
            if type(size) is not int or size < 0:
                raise ValueError('Invalid resource length')
            start = allocate(size)
            self.offsets[name] = start
            struct.pack_into('<II', fat, i*8, start, start+size)
        ntr_end = self.total
        executable_writes = []
        for field, code in extensions:
            start = allocate(len(code))
            struct.pack_into('<I', prefix, field, start)
            executable_writes.append((start, code))
        self.total = (self.total+511) & ~511
        if self.total > 512*1024*1024:
            raise ValueError('NDS exceeds supported 512 MiB image size')
        struct.pack_into('<4I', prefix, 0x40, fnt_at, len(fnt), fat_at, len(fat))
        struct.pack_into('<I', prefix, 0x68, banner_at)
        struct.pack_into('<I', prefix, 0x80, ntr_end if extensions else self.total)
        if extensions:
            struct.pack_into('<I', prefix, 0x210, self.total)
        prefix[0x14] = max(0, (self.total-1).bit_length()-17)
        struct.pack_into('<H', prefix, 0x15e, crc16(prefix[:0x15e]))
        self.file = self.partial.open('xb+')
        try:
            # Deterministic erased-ROM padding, even when assets arrive out of order.
            padding = b'\xff'*(1024*1024)
            left = self.total
            while left:
                take = min(left, len(padding))
                self.file.write(padding[:take])
                left -= take
            for position, data in ((0, prefix), (fnt_at, fnt), (fat_at, fat), (banner_at, banner)):
                self.file.seek(position)
                self.file.write(data)
            for position, data in executable_writes:
                self.file.seek(position)
                self.file.write(data)
        except BaseException:
            self.close()
            raise

    def write(self, name, payload):
        if self.file is None:
            raise ValueError('NDS writer is closed')
        if name not in self.expected or name in self.written:
            raise ValueError('Unexpected or duplicate NDS resource: ' + name)
        row = self.expected[name]
        if len(payload) != row['bytes'] or hashlib.sha256(payload).hexdigest() != row['sha256']:
            raise ValueError('NDS resource mismatch: ' + name)
        self.file.seek(self.offsets[name])
        self.file.write(payload)
        self.written.add(name)

    def finish(self):
        if self.file is None or self.written != set(self.expected):
            raise ValueError('NDS resources incomplete; no final file published')
        self.file.flush()
        os.fsync(self.file.fileno())
        self.close()
        from binary_notices import attach
        notice_proof = attach(self.partial, self.notices, 'nds')
        self.total = notice_proof['bytes']
        digest = digest_file(self.partial)
        # link() fails if the final name exists, unlike replace()/POSIX rename().
        os.link(self.partial, self.output)
        self.partial.unlink()
        return dict(nds_sha256=digest, bytes=self.total, resource_files=len(self.written),
                    original_writes=0, engine_restored=True, all_resource_hashes_match=True,
                    binary_notices=notice_proof)

    def close(self):
        if self.file is not None:
            self.file.close()
            self.file = None
