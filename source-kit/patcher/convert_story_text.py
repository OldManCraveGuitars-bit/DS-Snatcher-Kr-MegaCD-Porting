"""Pack the port's authored translation glyph pages; no game media input."""
import re
import struct
import zlib


def build(header):
    tables = re.findall(r'static const uint16_t \* const story_pages\[\]\s*=\s*\{([^}]+)\};', header)
    if len(tables) != 1:
        raise ValueError('Expected one translation page directory')
    names = [name.strip() for name in tables[0].split(',') if name.strip()]
    if not names or len(names) != len(set(names)):
        raise ValueError('Empty or duplicate translation page names')
    declarations = re.findall(r'static const uint16_t (\w+)\[\]\s*=\s*\{([^}]+)\};', header)
    arrays = {}
    selected = set(names)
    for name, body in declarations:
        if name not in selected:
            continue
        if name in arrays:
            raise ValueError('Duplicate translation page declaration: ' + name)
        if not re.fullmatch(r'[\s\d,]+', body):
            raise ValueError('Non-numeric translation glyph page: ' + name)
        values = [int(value) for value in body.split(',') if value.strip()]
        if not values or values[-1] != 65535 or 65535 in values[:-1]:
            raise ValueError('Invalid translation page terminator: ' + name)
        if any(value < 0 or value > 65535 for value in values):
            raise ValueError('Translation glyph outside uint16 range')
        arrays[name] = values
    if set(arrays) != selected:
        raise ValueError('Missing translation pages: ' + ', '.join(sorted(selected - set(arrays))))
    capacity = (max(len(arrays[name]) for name in names) + 15) & ~15
    if capacity > 65535:
        raise ValueError('Translation page exceeds archive capacity')
    prefix = struct.pack('<4sHHI', b'STP1', 1, capacity, len(names))
    directory, payload = bytearray(), bytearray()
    for name in names:
        values = arrays[name]
        data = struct.pack('<' + 'H' * len(values), *values)
        directory += struct.pack('<IHHI', len(prefix) + len(names) * 12 + len(payload),
                                 len(values), 0, zlib.crc32(data))
        payload += data
    return bytes(prefix + directory + payload)
