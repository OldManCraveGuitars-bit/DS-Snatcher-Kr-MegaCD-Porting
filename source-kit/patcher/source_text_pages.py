"""Restore untranslated glyph pages exclusively from the user's original disc."""
from pathlib import Path
import hashlib
import json
import re
import struct

from source_story_text import build as source_text_index


def digest(data):
    return hashlib.sha256(data).hexdigest()


def pages(value, filename):
    value = value.replace('{raw:F2}', '\n').replace('{raw:F6}', '\f')
    controls = 'F9|FA|FB|FC|F4|FE' if filename in ('SP04.BIN', 'SP05.BIN') else 'F9|FA|FB|FC|F4'
    value = re.sub(r'\{raw:(?:' + controls + r')\}', '', value)
    if '{raw:' in value:
        raise ValueError('Unsupported control in source-only glyph page')
    result = []
    for authored in value.split('\f'):
        lines, line, width = [], '', 0
        for char in authored:
            advance = 6 if char == ' ' else 12
            if char == '\n':
                lines.append(line)
                line, width = '', 0
                continue
            if width + advance > 216:
                lines.append(line)
                line, width = '', 0
            line += char
            width += advance
        lines.append(line)
        result.extend('\n'.join(lines[i:i + 8]) for i in range(0, len(lines), 8))
    return result


def restore_description(header, keyboard, description, source):
    required = {'schema_version', 'restored_header_sha256', 'erased_header_sha256',
                'keyboard_header_sha256', 'arrays'}
    if set(description) != required or description['schema_version'] != 1:
        raise ValueError('Unsupported source-only glyph description')
    if digest(header.encode('utf-8')) != description['erased_header_sha256']:
        raise ValueError('Source-only glyph placeholder header differs')
    if digest(keyboard.encode('utf-8')) != description['keyboard_header_sha256']:
        raise ValueError('Source-only ASCII glyph mapping differs')
    match = re.search(r'\bkeyboard_ascii\[128\]\s*=\s*\{([\d,\s]+)\}', keyboard)
    if match is None:
        raise ValueError('Missing ASCII glyph mapping')
    ascii_glyphs = [int(value) for value in match[1].split(',') if value.strip()]
    if len(ascii_glyphs) != 128:
        raise ValueError('Invalid ASCII glyph mapping')
    rows = description['arrays']
    if not isinstance(rows, list) or not 1 <= len(rows) <= 16384:
        raise ValueError('Invalid source-only glyph array list')
    names, filenames = set(), set()
    for row in rows:
        if not isinstance(row, dict) or set(row) != {'name', 'entry_id', 'page_index', 'units', 'sha256'}:
            raise ValueError('Invalid source-only glyph array record')
        if (not re.fullmatch(r'[A-Za-z_]\w{0,127}', row['name']) or row['name'] in names
                or not re.fullmatch(r'SP\d{2}\.BIN/\d{4}', row['entry_id'])
                or not re.fullmatch(r'[0-9a-f]{64}', row['sha256'])
                or type(row['page_index']) is not int or not 0 <= row['page_index'] < 256
                or type(row['units']) is not int or not 1 <= row['units'] <= 4096):
            raise ValueError('Invalid or duplicate source-only glyph binding')
        names.add(row['name'])
        filenames.add(row['entry_id'].split('/')[0])
    index = source_text_index(source, sorted(filenames))
    entries = {entry['entry_id']: entry['text'] for file in index['files'] for entry in file['entries']}
    restored = header
    for row in rows:
        if row['entry_id'] not in entries:
            raise ValueError('Missing original entry for source-only glyph binding')
        source_pages = pages(entries[row['entry_id']], row['entry_id'].split('/')[0])
        if row['page_index'] >= len(source_pages):
            raise ValueError('Missing original page for source-only glyph binding')
        values = []
        for char in source_pages[row['page_index']]:
            if char == '\n':
                values.append(65534)
            elif ord(char) >= 128 or ascii_glyphs[ord(char)] == 65535:
                raise ValueError('Unsupported character in source-only glyph page')
            else:
                values.append(ascii_glyphs[ord(char)])
        values.append(65535)
        if len(values) != row['units'] or digest(struct.pack('<' + 'H' * len(values), *values)) != row['sha256']:
            raise ValueError('Original source-only glyph page identity differs')
        pattern = r'(static const uint16_t ' + re.escape(row['name']) + r'\[\]\s*=\s*\{)([\d,\s]+)(\};)'
        matches = list(re.finditer(pattern, restored))
        if len(matches) != 1:
            raise ValueError('Missing or duplicate source-only glyph placeholder')
        match = matches[0]
        erased = [int(value) for value in match[2].split(',') if value.strip()]
        if erased != [0] * row['units']:
            raise ValueError('Source-only glyph placeholder is not erased')
        numbers = iter(values)
        body = re.sub(r'\d+', lambda _: str(next(numbers)), match[2])
        restored = restored[:match.start(2)] + body + restored[match.end(2):]
    if digest(restored.encode('utf-8')) != description['restored_header_sha256']:
        raise ValueError('Restored source-only glyph header identity differs')
    return restored


def restore_header(root, source, header=None):
    root = Path(root)
    if header is None:
        header = (root / 'player/generated/text.h').read_bytes().decode('utf-8')
    runner = json.loads((root / 'engine/runner.json').read_text(encoding='utf-8'))
    binding = runner.get('source_text_pages')
    if binding is None:
        return header
    if (set(binding) != {'file', 'sha256'} or binding['file'] != 'source-text-pages.json'
            or not re.fullmatch(r'[0-9a-f]{64}', binding['sha256'])):
        raise ValueError('Invalid source-only glyph metadata binding')
    raw = (root / 'engine' / binding['file']).read_bytes()
    if digest(raw) != binding['sha256']:
        raise ValueError('Source-only glyph metadata identity differs')
    keyboard = (root / 'player/generated/people_keyboard.h').read_bytes().decode('utf-8')
    return restore_description(header, keyboard, json.loads(raw.decode('utf-8')), source)
