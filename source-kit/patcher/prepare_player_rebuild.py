"""Create a fresh source rebuild tree using the user's generated NDS/disc."""
from pathlib import Path
import argparse
import hashlib
import json
import shutil
import struct
import sys
import tempfile

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from iso_source import SourceDisc
from host_helpers import restore
from derive_voice_gain import source_header as voice_gain_header
from build_release_resources import read_manifest


def run(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'nds', 'output'):
        parser.add_argument('--' + name, required=True)
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    source = Path(args.source).resolve(strict=True)
    nds = Path(args.nds).resolve(strict=True)
    output = Path(args.output).resolve()
    if output.exists() or any(output.is_relative_to(p) for p in (root, source.parent, nds.parent)):
        raise ValueError('Choose a new rebuild folder outside package and input folders')
    expected = read_manifest(root / 'engine/resources.json')
    before = nds.stat()
    with nds.open('rb') as stream:
        header = stream.read(512)
        if len(header) != 512:
            raise ValueError('Truncated NDS header')
        fnt_at, fnt_size, fat_at, fat_size = struct.unpack_from('<4I', header, 0x40)
        if not 8 <= fnt_size <= 131072 or fat_size != len(expected) * 8:
            raise ValueError('Unexpected NitroFS directory')
        stream.seek(fnt_at)
        fnt = stream.read(fnt_size)
        stream.seek(fat_at)
        fat = stream.read(fat_size)
        if len(fnt) != fnt_size or len(fat) != fat_size or struct.unpack_from('<IHH', fnt) != (8, 0, 1):
            raise ValueError('Unsupported NitroFS directory')
        names, at = [], 8
        while at < len(fnt) and fnt[at]:
            length = fnt[at]
            at += 1
            if length >= 128 or at + length >= len(fnt):
                raise ValueError('Invalid NitroFS name')
            names.append(fnt[at:at + length].decode('ascii'))
            at += length
        if at != len(fnt) - 1 or len(names) != len(expected) or set(names) != set(expected):
            raise ValueError('NDS resource inventory differs from this engine')
        extents = []
        for i, name in enumerate(names):
            start, end = struct.unpack_from('<II', fat, i * 8)
            if end < start or end > before.st_size or end - start != expected[name]['bytes']:
                raise ValueError('Invalid NDS extent: ' + name)
            extents.append((name, start, end))
            stream.seek(start)
            if hashlib.sha256(stream.read(end - start)).hexdigest() != expected[name]['sha256']:
                raise ValueError('NDS resource differs: ' + name)
        with SourceDisc(source) as disc:
            meta = json.loads((root / 'engine/runner.json').read_text())
            from source_text_pages import restore_header as restore_source_text
            regenerated_text_header=restore_source_text(root,disc)
            tables = {}
            for row in meta['source_tables']:
                local = dict(row, offset=0)
                tables[row['name']] = restore(bytes(row['size']), [local], disc)
            regenerated_voice_gain=voice_gain_header(disc)
            regenerated_story_headers={}
            for group in meta.get('derived_source_tables',[]):
                from derived_source_tables import build as build_derived
                _,headers=build_derived(disc,group['kind'])
                if regenerated_story_headers.keys()&headers.keys():raise ValueError('Duplicate regenerated source header')
                regenerated_story_headers.update(headers)
            if 'control_graph' in meta:
                from derive_story_graph import build,source_headers
                description=meta['control_graph'];filename=description['replacement_file']
                if Path(filename).name!=filename or ':' in filename or '\\' in filename:
                    raise ValueError('Invalid replacement catalog filename')
                replacement=(root/'engine'/filename).read_bytes()
                if hashlib.sha256(replacement).hexdigest()!=description['replacement_sha256']:
                    raise ValueError('Damaged replacement translation catalog')
                rows=[json.loads(line) for line in replacement.decode('utf8').splitlines()]
                # Only this newly created temporary directory is removed; the
                # input disc, package and existing output trees stay protected.
                with tempfile.TemporaryDirectory(prefix='snatcher-rebuild-',dir=output.parent) as scratch:
                    graph=build(disc,rows,Path(scratch)/'derived')
                    headers=source_headers(graph)
                    if 'lookup_index' in description:
                        from derive_story_index import binding as index_binding
                        _,_,headers['story_pc_index.h']=index_binding(description['lookup_index'],graph)
                    if regenerated_story_headers.keys()&headers.keys():raise ValueError('Duplicate regenerated story header')
                    regenerated_story_headers.update(headers)
        shutil.copytree(root / 'player', output)
        media = output / 'nitrofiles'
        media.mkdir()
        for name, start, end in extents:
            stream.seek(start)
            digest = hashlib.sha256()
            with (media / name).open('xb') as target:
                left = end - start
                while left:
                    block = stream.read(min(left, 1024 * 1024))
                    if not block:
                        raise ValueError('NDS changed during extraction')
                    target.write(block)
                    digest.update(block)
                    left -= len(block)
            if digest.hexdigest() != expected[name]['sha256']:
                raise ValueError('NDS changed during extraction: ' + name)
        after = nds.stat()
        if (before.st_size, before.st_mtime_ns) != (after.st_size, after.st_mtime_ns):
            raise ValueError('NDS changed during extraction')
    (output/'generated/voice_source_gain.h').write_text(regenerated_voice_gain,encoding='ascii')
    (output/'generated/text.h').write_bytes(regenerated_text_header.encode('utf8'))
    for filename,text in regenerated_story_headers.items():
        (output/'generated'/filename).write_text(text,encoding='utf8')
    for filename, ctype, name in (('original_sound_gain.h', 'uint8_t', 'sub_music_gain'),
                                  ('source_battle_random.h', 'unsigned char', 'battle_random_map')):
        data = tables[name]
        (output / 'generated' / filename).write_text(
            f'static const {ctype} {name}[{len(data)}]={{' + ','.join(map(str, data)) + '};\n', encoding='ascii')
    # Copyright notices are also kept in the generated executable itself.
    # This postprocessing changes only NDS size/checksum bookkeeping and a
    # trailer, preserving every executable byte and every NitroFS resource.
    shutil.copyfile(root/'patcher/binary_notices.py',output/'tools/binary_notices.py')
    shutil.copyfile(root/'licenses/BINARY-NOTICES.txt',output/'BINARY-NOTICES.txt')
    (output/'REBUILD-NOTICES_KO.txt').write_text(
        'Makefile.arm7, Makefile 순서로 빌드한 다음 아래 명령으로 실행 파일에도 외부 코드 고지를 보존합니다.\n'
        'python -B tools/binary_notices.py --kind nds --notices BINARY-NOTICES.txt snatcher_nds_resources_r22.nds\n',encoding='utf8')
    print('Prepared source and', len(extents), 'verified original-derived resources in', output)
    print('Build with the matching BlocksDS toolchain using Makefile.arm7, then Makefile.')
    print('Then preserve executable notices using the command in REBUILD-NOTICES_KO.txt.')


if __name__ == '__main__':
    run()
