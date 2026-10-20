"""Generate the complete release resource manifest from user-owned originals.

This private development entry point does not update the frozen v0.1Demo.
The manifest supplies identities only. Translation glyph pages and decorative
casing are port-authored inputs; game artwork, speech, music and credits must
be regenerated from the original disc/BIOS. Unknown files fail closed.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import sys
import time
from contextlib import ExitStack

from build_demo_resources import CachedSource
from iso_source import SourceDisc


def read_manifest(path):
    manifest = json.loads(Path(path).read_text(encoding='utf8'))
    expected = {}
    for row in manifest['files']:
        name = row['name']
        if (not re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,126}', name)
                or name in expected or not re.fullmatch(r'[a-f0-9]{64}', row['sha256'])
                or type(row.get('bytes')) is not int or row['bytes'] < 0):
            raise ValueError('Invalid resource identity: ' + str(name))
        expected[name] = row
    if not expected:
        raise ValueError('Empty resource manifest')
    return expected


def generate(args):
    expected = read_manifest(args.manifest)
    if args.helpers:
        from host_helpers import use_directory
        use_directory(args.helpers)
    src, bios = Path(args.source).resolve(strict=True), Path(args.bios).resolve(strict=True)
    engine = Path(args.engine).resolve(strict=True)
    text_source = Path(args.translation_header).resolve(strict=True)
    out = Path(args.output).resolve()
    for protected in (src if src.is_dir() else src.parent, bios.parent, engine,
                      Path(args.dependencies).resolve(strict=True), text_source.parent):
        if out == protected or out.is_relative_to(protected):
            raise ValueError('Keep output outside input directories')
    if args.engine_bundle and out.is_relative_to(Path(args.engine_bundle).resolve(strict=True)):
        raise ValueError('Keep output outside the engine bundle')
    # An existing result is never overwritten, including a failed conversion.
    out.mkdir(parents=True, exist_ok=False)
    report = dict(original_writes=0, preconverted_game_asset_inputs=0,
                  screenshot_inputs=0, complete_nds_assembly=False,
                  verify_only=args.verify_only, generated=[], mismatched=[], errors=[])
    started = time.monotonic()
    made = {}
    writer = None

    def save(outputs, method):
        for name, data in outputs.items():
            if name not in expected:
                continue
            digest = hashlib.sha256(data).hexdigest()
            if name in made:
                if made[name] != digest:
                    raise ValueError('Converters disagree: ' + name)
                continue
            matches = digest == expected[name]['sha256'] and len(data) == expected[name]['bytes']
            row = dict(name=name, bytes=len(data), sha256=digest, method=method,
                       matches_expected=matches)
            made[name] = digest
            if not args.verify_only:
                with (out/name).open('xb') as stream:
                    stream.write(data)
            report['generated'].append(row)
            if not matches:
                report['mismatched'].append(name)
            elif writer is not None:
                writer.write(name, data)

    def checkpoint():
        report.update(missing=sorted(set(expected)-set(made)),
                      matching_files=sum(row['matches_expected'] for row in report['generated']),
                      expected_files=len(expected), elapsed_seconds=round(time.monotonic()-started, 3))
        report['all_resource_identities_match'] = not (report['missing'] or report['mismatched'] or report['errors'])
        (out/'conversion.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')

    def stage(label, function):
        t = time.monotonic()
        try:
            save(function(), label)
            print('Converted:', label, f'({time.monotonic()-t:.1f}s)', flush=True)
        except Exception as error:
            report['errors'].append(dict(stage=label, error=type(error).__name__ + ': ' + str(error)))
            print('FAILED:', label, str(error), flush=True)
        checkpoint()

    with SourceDisc(src) as raw, ExitStack() as cleanup:
        source = CachedSource(raw)
        if args.engine_bundle:
            from assemble_release import ReleaseWriter
            writer = ReleaseWriter(source, args.engine_bundle, out/'snatcher_ds.nds', expected)
            cleanup.callback(writer.close)
        bindings = json.loads(Path(__file__).with_name('portrait-bindings-v1.json').read_text())
        from convert_pcm import convert
        ids = sorted(int(name[3:7]) for name in expected if re.fullmatch(r'pcm\d{4}\.bin', name))
        if ids:
            stage('original decoded speech/effects', lambda: convert(source, ids)[0])
        from convert_native_speech import build as encoded
        ids = sorted({int(re.search(r'\d{4}', name)[0]) for name in expected
                      if re.fullmatch(r'native-(?:speech|pcm-bank)-\d{4}-r22\.bin', name)})
        if ids:
            stage('original encoded PCM banks', lambda: encoded(source, ids, allow_effect_banks=True)[0])
        from convert_pcm_effects import extract_dynamic
        extents = {'native-entry-pcm-bank-r22.bin': 34, 'native-entry-voice55-r22.bin': 55,
                   'native-flight-voice129-r22.bin': 129, 'native-forecourt-pcm2-r22.bin': 2}
        stage('original sector extents', lambda: {name: extract_dynamic(source, ident)[0]
              for name, ident in extents.items() if name in expected})
        scripts = {'native-entry-script-r22.bin': 'SP06.BIN', 'native-flight-script-r22.bin': 'SP06.BIN',
                   'native-arrival-script-r22.bin': 'SP07.BIN', 'native-return-script-r22.bin': 'SP08.BIN',
                   'native-credits-r22.bin': 'DATA_Y16.BIN'}
        stage('original scripts and credits', lambda: {name: source.read_file(original)
              for name, original in scripts.items() if name in expected})
        from convert_resource_archive import build as archive
        from convert_flight_player import build_archive as flight_archive
        from convert_room_pcm import build as room_pcm
        from convert_portraits import build as portraits
        from convert_battle_feedback import build as faces
        from convert_battle_sprites import build as battle
        from convert_office_motion import build as motion
        from convert_background import build as office, build_reception
        from convert_startup_cards import build as cards
        from convert_konami import build as konami
        from convert_opening_timeline import build as timeline
        from convert_sega import build as sega
        stage('original resource archive', lambda: {'native-resource-banks-r22.nrs': archive(source)[0]})
        stage('original vehicle banks', lambda: {'native-flight-banks-r22.nrs': flight_archive(source)[0]})
        stage('original room PCM archive', lambda: {'native-room-pcm-r22.nrs': room_pcm(source)[0]})
        stage('original portrait maps/tiles', lambda: {'native-portraits-r22.nspr': portraits(bindings, source)[0]})
        stage('original battle portraits', lambda: faces(source, bindings)[0])
        stage('original battle sprites', lambda: {'battle-sprites-r21.bin': battle(source)[0]})
        stage('original office actors', lambda: {'native-office-actors-r22.nsar': motion(bindings, source)[0]})
        stage('original office background', lambda: {'native-office-background-r22.nsbg': office(source)[0]})
        stage('original reception background', lambda: {'native-reception-background-r22.nsbg': build_reception(source)[0]})
        stage('original startup cards', lambda: cards(source)[0])
        stage('original Konami startup', lambda: {'native-konami-startup-r22.nsbg': konami(source)[0]})
        stage('original opening timeline', lambda: {'native-opening-timeline-r22.nstm': timeline(source)[0]})
        stage('original Sega startup', lambda: {'native-sega-license-r22.nsbg': sega(source, bios)[0]})
        from convert_room_image import build as room_images
        stage('original battle backdrop', lambda: room_images(source, engine, out/'room-work', args.compiler,
              {name: row['sha256'] for name, row in expected.items() if name.startswith('hq-')})[0])
        from convert_forecourt_driver import build as driver
        stage('original sound driver initialization', lambda: driver(source, engine, out/'driver-work', args.compiler)[0])
        from convert_score_driver import build as score
        commands = sorted({int(name[10:12], 16) for name in expected
                           if re.fullmatch(r'native-fm-[0-9a-f]{2}\.nsf[si]', name)})
        for command in commands:
            stage(f'original FM score {command:02x}', lambda command=command: score(source, engine,
                  args.cpu_core, out/f'score-{command:02x}', args.compiler, command)[0])
        from convert_boot_audio import build as boot_audio
        stage('original startup sound drivers', lambda: boot_audio(source, bios, args.dependencies,
              out/'boot-work', args.compiler, args.cpu_core)[0])
        from convert_theme import build as theme
        stage('port-authored casing', lambda: theme(Path(args.theme)))
        from convert_opening_callbacks import build as callbacks
        stage('original opening 1 callbacks', lambda: {'native-opening-03-r22.nsop': callbacks(source)[0]})
        stage('original opening 2 callbacks', lambda: {'native-opening-04-r22.nsop': callbacks(source, 'DATA_D0.BIN', 4, 2)[0]})
        from convert_cdda import build as cdda
        from convert_cdda_stream import build as stream
        tracks = sorted(int(name[12:14]) for name in expected if re.fullmatch(r'native-cdda-\d{2}-r22\.nsta', name))
        for track in tracks:
            stage(f'original CDDA track {track:02d}', lambda track=track: stream(cdda(source, (track,))[0], (track,))[0])
        from convert_story_text import build as translation
        from source_text_pages import restore_header as restore_source_text
        stage('authored translation and user-source glyph pages', lambda: {'story-text-r22.stp': translation(
            restore_source_text(Path(__file__).resolve().parents[1], source,
                                text_source.read_bytes().decode('utf8')))})
        report['source_files_read'] = source.used
        checkpoint()
        if writer is not None:
            try:
                if report['all_resource_identities_match']:
                    # Verify read-only disc identity before publishing the NDS.
                    raw.close()
                    report['assembly'] = writer.finish()
                    report['complete_nds_assembly'] = True
            finally:
                writer.close()
    report['inputs'] = dict(manifest_sha256=hashlib.sha256(Path(args.manifest).read_bytes()).hexdigest(),
                           translation_header_sha256=hashlib.sha256(text_source.read_bytes()).hexdigest())
    if args.helpers:
        report['inputs']['helper_manifest_sha256'] = hashlib.sha256(
            (Path(args.helpers)/'manifest.json').read_bytes()).hexdigest()
    checkpoint()
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'bios', 'output', 'manifest', 'engine', 'cpu-core', 'compiler',
                 'dependencies', 'theme', 'translation-header'):
        parser.add_argument('--' + name, required=True)
    parser.add_argument('--verify-only', action='store_true', help='Hash generated bytes without retaining media duplicates')
    parser.add_argument('--helpers', help='Masked Windows helpers; source-only tables are restored from the user disc')
    parser.add_argument('--engine-bundle', help='Masked runtime to assemble directly into one NDS; implies --verify-only')
    args = parser.parse_args()
    if args.engine_bundle:
        args.verify_only = True
    report = generate(args)
    print(json.dumps({key: report[key] for key in ('matching_files', 'expected_files', 'missing', 'mismatched', 'errors')}), flush=True)
    return 0 if report['all_resource_identities_match'] else 1


if __name__ == '__main__':
    sys.exit(main())
