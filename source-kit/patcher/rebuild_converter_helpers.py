"""Optionally rebuild all five Windows converters from the included source."""
from pathlib import Path
import argparse
import hashlib
import json
import os
import shutil
import subprocess
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))

from iso_source import SourceDisc


def digest(data):
    return hashlib.sha256(data).hexdigest()


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    for key in ('source', 'bios', 'output', 'gcc'):
        parser.add_argument('--' + key, required=True)
    args = parser.parse_args()
    root = Path(__file__).resolve().parents[1]
    from patch_release import check_package
    package = check_package(root)
    source = Path(args.source).resolve(strict=True)
    bios = Path(args.bios).resolve(strict=True)
    compiler = Path(args.gcc).resolve(strict=True)
    out = Path(args.output).resolve()
    if out.exists():
        raise ValueError('Choose a new rebuild folder; existing output is preserved')
    if any(out == parent or out.is_relative_to(parent) for parent in (root, source.parent, bios.parent)):
        raise ValueError('Keep rebuild output outside package and input directories')
    if source.suffix.lower() != '.cue':
        raise ValueError('Rebuild requires the original USA CUE and all BIN tracks')
    if digest(bios.read_bytes()) != 'b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4':
        raise ValueError('Unsupported USA BIOS')
    dependencies = root / 'converter'
    original_helpers = json.loads((root / 'host/windows-x64/manifest.json').read_text(encoding='utf8'))
    gain_bindings = [row for row in original_helpers['forecourt']['source_tables'] if row['name'] == 'sub_music_gain']
    if len(gain_bindings) != 1 or gain_bindings[0]['size'] != 256:
        raise ValueError('Missing source gain-table binding')
    binding = gain_bindings[0]
    expected = {row['name']: row for row in json.loads((root / 'engine/resources.json').read_text(encoding='utf8'))['files']}
    report = dict(passed=False, package_label=package['label'], original_writes=0,
                  compiler=str(compiler), compiler_sha256=digest(compiler.read_bytes()),
                  source_helpers=True, checked_resources=[], host_manifest=None)
    with SourceDisc(source) as disc:
        original = disc.read_file(binding['source'])
        gain = original[binding['source_offset']:binding['source_offset'] + binding['size']]
        if len(gain) != 256 or digest(gain) != binding['sha256']:
            raise ValueError('Original source gain-table identity differs')
        out.mkdir(parents=True)
        try:
            version = subprocess.run([str(compiler), '--version'], capture_output=True, text=True, check=True)
            report['compiler_version'] = version.stdout.splitlines()[0]
            engine = out / 'player'
            shutil.copytree(dependencies / 'player', engine)
            (engine / 'generated/original_sound_gain.h').write_text(
                'static const uint8_t sub_music_gain[256]={' + ','.join(map(str, gain)) + '};\n', encoding='ascii')
            from convert_forecourt_driver import build as forecourt
            from convert_score_driver import build as score
            from convert_boot_audio import build as boot
            from convert_room_image import build as room
            core = dependencies / 'third_party/GenesisPlusGX/core'
            old_force = os.environ.get('SNATCHER_FORCE_SOURCE')
            os.environ['SNATCHER_FORCE_SOURCE'] = '1'

            def check(outputs):
                for name, data in outputs.items():
                    if name not in expected:
                        raise ValueError('Unexpected rebuilt resource: ' + name)
                    row = expected[name]
                    if len(data) != row['bytes'] or digest(data) != row['sha256']:
                        raise ValueError('Rebuilt helper resource differs: ' + name)
                    report['checked_resources'].append(dict(name=name, bytes=len(data), sha256=digest(data)))

            try:
                check(forecourt(disc, engine, out / 'forecourt', compiler)[0])
                check(score(disc, engine, core, out / 'score', compiler, 0x40)[0])
                check(boot(disc, bios, dependencies, out / 'boot', compiler, core)[0])
                room_expected = {name: row['sha256'] for name, row in expected.items() if name.startswith('hq-')}
                if not room_expected:
                    raise ValueError('Missing room-helper resource inventory')
                room_outputs, room_records = room(disc, engine, out / 'room', compiler, room_expected)
                if len(room_outputs) != len(room_expected) or not all(row['matched'] for row in room_records):
                    raise ValueError('Rebuilt room helper did not reproduce all source pictures')
                check(room_outputs)
            finally:
                if old_force is None:
                    os.environ.pop('SNATCHER_FORCE_SOURCE', None)
                else:
                    os.environ['SNATCHER_FORCE_SOURCE'] = old_force
            # ctypes may still map the work DLLs. Modify new, unloaded copies.
            host = out / 'host/windows-x64'
            host.mkdir(parents=True)
            from binary_notices import attach
            libraries = dict(boot=out / 'boot/boot.dll', room=out / 'room/room.dll',
                             forecourt=out / 'forecourt/converter.dll', score=out / 'score/score.dll',
                             startup=out / 'score/startup.dll')
            manifest = {}
            for name, built in libraries.items():
                target = host / (name + '.dll')
                shutil.copyfile(built, target)
                notice = attach(target, (root / 'licenses/BINARY-NOTICES.txt').read_bytes(), 'pe')
                restored = target.read_bytes()
                data = bytearray(restored)
                records = []
                if name == 'forecourt':
                    at = restored.find(gain)
                    if at < 0 or restored.find(gain, at + 1) >= 0:
                        raise ValueError('Original gain table is not a unique rebuild reservation')
                    records.append(dict(binding, offset=at))
                    data[at:at + len(gain)] = bytes(len(gain))
                target.write_bytes(data)
                manifest[name] = dict(file=target.name, sha256=digest(data),
                                      restored_sha256=digest(restored), source_tables=records)
                report.setdefault('libraries', {})[name] = dict(raw_sha256=digest(built.read_bytes()),
                    masked_sha256=digest(data), restored_sha256=digest(restored), notice=notice)
            metadata = host / 'manifest.json'
            metadata.write_text(json.dumps(manifest, indent=2) + '\n', encoding='utf8')
            report.update(passed=True, host_manifest=str(metadata), host_manifest_sha256=digest(metadata.read_bytes()),
                          original_files_read=disc.used.copy(), complete_game_conversion=False)
        finally:
            (out / 'rebuild-result.json').write_text(json.dumps(report, indent=2) + '\n', encoding='utf8')
    print('Rebuilt and checked all five source helpers:', out / 'host/windows-x64')
    print('Checked resource identities:', len(report['checked_resources']))


if __name__ == '__main__':
    run()
