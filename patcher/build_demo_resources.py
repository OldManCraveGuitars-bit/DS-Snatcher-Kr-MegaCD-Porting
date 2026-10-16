"""Regenerate demo resource groups from the user's disc; no existing game asset inputs.

This is a resource stage, not a complete NDS patcher. A manifest is used only
to validate output identities and enumerate resource IDs, never as pixel/audio
data. Unimplemented or mismatched resources are reported, never substituted.
"""
from pathlib import Path
import argparse
import hashlib
import json
import re
import sys

sys.dont_write_bytecode = True
from iso_source import SourceDisc


class CachedSource:
    def __init__(self, source):
        self.source, self.cache = source, {}

    def __getattr__(self, name):
        return getattr(self.source, name)

    def read_file(self, name):
        if name not in self.cache:
            self.cache[name] = self.source.read_file(name)
        return self.cache[name]


def run():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source', required=True)
    parser.add_argument('--output', required=True)
    parser.add_argument('--manifest', required=True, help='Expected filenames and SHA-256, containing no asset bytes')
    parser.add_argument('--bios')
    parser.add_argument('--engine', help='Portable player sources for original sound conversion')
    parser.add_argument('--cpu-core', help='Licensed Genesis Plus GX core source directory')
    parser.add_argument('--compiler', help='Host GCC executable for original sound conversion')
    parser.add_argument('--dependencies', help='Boot synthesis sources and licensed dependencies')
    parser.add_argument('--theme', help='User-authored decorative upper-screen PNG')
    parser.add_argument('--skip-cdda', action='store_true')
    parser.add_argument('--skip-callbacks', action='store_true')
    args = parser.parse_args()
    if any((args.engine,args.cpu_core,args.compiler)) and not all((args.engine,args.cpu_core,args.compiler)):
        parser.error('--engine, --cpu-core and --compiler must be supplied together')
    src = Path(args.source).resolve(strict=True)
    dest = Path(args.output).resolve()
    if dest.exists():
        raise ValueError('Output already exists; choose a new folder')
    if dest.is_relative_to(src if src.is_dir() else src.parent):
        raise ValueError('Keep generated output outside the input disc directory')
    manifest = json.loads(Path(args.manifest).read_text(encoding='utf8'))
    expected = {row['name']: row for row in manifest['files']}
    if len(expected) != len(manifest['files']):
        raise ValueError('Duplicate resource names in manifest')
    for name, row in expected.items():
        if Path(name).name != name or not re.fullmatch('[a-f0-9]{64}', row['sha256']):
            raise ValueError('Unsafe resource manifest')
    report = dict(original_writes=0, preconverted_asset_inputs=0, screenshot_inputs=0,
                  complete_nds_assembly=False, generated=[], mismatched=[], errors=[])
    dest.mkdir(parents=True)

    def save(outputs, method):
        for name, data in outputs.items():
            if name not in expected:
                continue
            row = dict(name=name, bytes=len(data), sha256=hashlib.sha256(data).hexdigest(), method=method)
            row['matches_expected'] = row['sha256'] == expected[name]['sha256']
            if (dest/name).exists():
                if (dest/name).read_bytes() != data:
                    raise ValueError('Two converters disagree: ' + name)
                continue
            (dest/name).write_bytes(data)
            report['generated'].append(row)
            if not row['matches_expected']:
                report['mismatched'].append(name)

    with SourceDisc(src) as raw:
        source = CachedSource(raw)
        bindings = json.loads(Path(__file__).with_name('portrait-bindings-v1.json').read_text())

        def stage(label, function):
            try:
                save(function(), label)
                print('Converted:', label, flush=True)
            except Exception as error:
                report['errors'].append(dict(stage=label, error=str(error)))
                print('Incomplete:', label, type(error).__name__, str(error), flush=True)

        from convert_pcm import convert
        ids = [int(name[3:7]) for name in expected if re.fullmatch(r'pcm\d{4}\.bin', name)]
        stage('original decoded speech', lambda: convert(source, ids)[0])

        from convert_native_speech import build as encoded
        ids = [int(re.search(r'\d{4}', name)[0]) for name in expected
               if re.fullmatch(r'native-(?:speech|pcm-bank)-\d{4}-r22\.bin', name)]
        stage('original encoded speech and effect banks', lambda: encoded(source, ids, allow_effect_banks=True)[0])

        from convert_pcm_effects import extract_dynamic
        def raw_banks():
            rows = {'native-entry-pcm-bank-r22.bin':34, 'native-entry-voice55-r22.bin':55,
                    'native-flight-voice129-r22.bin':129, 'native-forecourt-pcm2-r22.bin':2}
            return {name:extract_dynamic(source, ident)[0] for name, ident in rows.items()}
        stage('original sector extents', raw_banks)
        stage('original story scripts', lambda: {name:source.read_file(source_name) for name,source_name in {
            'native-entry-script-r22.bin':'SP06.BIN', 'native-flight-script-r22.bin':'SP06.BIN',
            'native-arrival-script-r22.bin':'SP07.BIN', 'native-return-script-r22.bin':'SP08.BIN'}.items()})

        from convert_resource_archive import build as archive
        stage('original resource archive', lambda: {'native-resource-banks-r22.nrs':archive(source)[0]})
        from convert_flight_player import build_archive as flight_archive
        stage('original vehicle banks', lambda: {'native-flight-banks-r22.nrs':flight_archive(source)[0]})
        from convert_room_pcm import build as rooms
        stage('original room PCM archive', lambda: {'native-room-pcm-r22.nrs':rooms(source)[0]})
        from convert_portraits import build as portraits
        stage('original portrait maps and tiles', lambda: {'native-portraits-r22.nspr':portraits(bindings,source)[0]})
        from convert_battle_feedback import build as faces
        stage('original battle portraits', lambda: faces(source, bindings)[0])
        from convert_battle_sprites import build as battle
        stage('original battle sprites', lambda: {'battle-sprites-r21.bin':battle(source)[0]})
        from convert_office_motion import build as motion
        stage('original office actor commands', lambda: {'native-office-actors-r22.nsar':motion(bindings,source)[0]})
        from convert_background import build as office, build_reception
        stage('original office background', lambda: {'native-office-background-r22.nsbg':office(source)[0]})
        stage('original reception background', lambda: {'native-reception-background-r22.nsbg':build_reception(source)[0]})
        from convert_startup_cards import build as cards
        stage('original startup cards', lambda: cards(source)[0])
        from convert_konami import build as konami
        stage('original Konami startup', lambda: {'native-konami-startup-r22.nsbg':konami(source)[0]})
        from convert_opening_timeline import build as timeline
        stage('original opening timeline', lambda: {'native-opening-timeline-r22.nstm':timeline(source)[0]})
        if args.bios:
            from convert_sega import build as sega
            stage('original Sega startup', lambda: {'native-sega-license-r22.nsbg':sega(source,Path(args.bios).resolve(strict=True))[0]})
        if args.engine:
            from convert_room_image import build as room_images
            stage('original battle backdrop', lambda: room_images(source,args.engine,
                dest/'room-work',args.compiler,{n:r['sha256'] for n,r in expected.items() if n.startswith('hq-')})[0])
            from convert_forecourt_driver import build as driver
            stage('original sound driver initialization', lambda: driver(source,args.engine,
                dest/'driver-work',args.compiler)[0])
            from convert_score_driver import build as score
            for command in (0x40,0x5e,0x5f,0x62):
                stage(f'original FM score {command:02x}', lambda command=command: score(source,
                    args.engine,args.cpu_core,dest/f'score-{command:02x}',args.compiler,command)[0])
        if args.bios and args.dependencies and args.engine:
            from convert_boot_audio import build as boot_audio
            stage('original startup sound drivers',lambda:boot_audio(source,args.bios,args.dependencies,
                dest/'boot-work',args.compiler,args.cpu_core)[0])
        if args.theme:
            from convert_theme import build as theme
            stage('user-authored casing',lambda:theme(Path(args.theme)))
        if not args.skip_callbacks:
            def callbacks():
                from convert_opening_callbacks import build
                return {'native-opening-03-r22.nsop':build(source)[0],
                        'native-opening-04-r22.nsop':build(source,'DATA_D0.BIN',4,2)[0]}
            stage('original opening callbacks', callbacks)
        if not args.skip_cdda:
            from convert_cdda import build as cdda
            from convert_cdda_stream import build as stream
            for track in [3,4,5,6,20]:
                stage(f'original CDDA track {track:02d}', lambda track=track: stream(cdda(source,(track,))[0],(track,))[0])
        report['source_files_read'] = source.used
    made = {row['name'] for row in report['generated']}
    report['missing'] = sorted(set(expected)-made)
    report['matching_files'] = sum(row['matches_expected'] for row in report['generated'])
    report['expected_files'] = len(expected)
    report['all_resource_identities_match'] = not (report['missing'] or report['mismatched'] or report['errors'])
    (dest/'conversion.json').write_text(json.dumps(report,ensure_ascii=False,indent=2), encoding='utf8')
    print(json.dumps({key:report[key] for key in ['matching_files','expected_files','missing','mismatched','errors']},ensure_ascii=False,indent=2))


if __name__ == '__main__':
    run()
