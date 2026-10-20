"""Asset-free patcher prototype: convert user-owned Mega CD resources locally."""
from pathlib import Path
import sys, json, argparse, hashlib
sys.dont_write_bytecode=True
from iso_source import SourceDisc
from convert_portraits import build
from convert_pcm import convert
from convert_pcm_effects import build_entry as build_entry_effects
from convert_entry_program import build as build_entry_program
from convert_resource_archive import build as build_archive, bootstrap
from convert_office_motion import build as build_office
from convert_background import build as build_background,build_reception
from convert_opening_timeline import build as build_opening_timeline
from convert_cdda import build as build_cdda
from convert_cdda_stream import build as build_cdda_stream
from convert_sound_tables import build as build_sound_tables
from convert_sega import build as build_sega
from convert_konami import build as build_konami
from convert_startup_cards import build as build_startup_cards

def run():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source',required=True,help='USA CUE/BIN, ISO, or extracted ISO directory')
    parser.add_argument('--output',required=True,help='A new local output directory')
    scope=parser.add_mutually_exclusive_group()
    scope.add_argument('--portraits-only',action='store_true')
    scope.add_argument('--entry-effects-only',action='store_true',help='Extract only the original entry PCM wave bank and its provenance (no large CDDA copies)')
    scope.add_argument('--entry-script-only',action='store_true',help='Extract and inspect only the original SP06 entry script (no large CDDA copies)')
    scope.add_argument('--entry-player-only',action='store_true',help='Extract only entry script, six native banks and original effect/ACT1 speech sectors')
    scope.add_argument('--flight-player-only',action='store_true',help='Extract original vehicle/script/speech129 and convert CD20 without recorded media')
    scope.add_argument('--forecourt-driver-only',action='store_true',help='Compute original4C driver startup without captured audio or state')
    scope.add_argument('--vehicle-driver-only',action='store_true',help='Execute original62 score, verify its loop and compute native FM/PCM inputs')
    scope.add_argument('--gameover-driver-only',action='store_true',help='Execute original5E score, verify its loop and compute native FM inputs')
    scope.add_argument('--headquarters-driver-only',action='store_true',help='Execute original5A score, verify its loop and compute native FM/PCM inputs')
    scope.add_argument('--headquarters-speech-only',action='store_true',help='Decode original SP08 speech181..201 for the DS stream, without captured audio')
    scope.add_argument('--contact-speech-only',action='store_true',help='Decode original SP18 telephone speech1185..1191/1193 from the user disc')
    scope.add_argument('--story-pcm-only',action='store_true',help='Extract native vehicle-exit effect bank16 and forecourt speech37/130..136')
    scope.add_argument('--battle-feedback-only',action='store_true',help='Build the four user-selected battle portraits from original maps and tiles')
    parser.add_argument('--engine',help='Native player source directory for driver conversion')
    parser.add_argument('--cpu-core',help='Licensed Genesis Plus GX core source directory for vehicle score conversion')
    parser.add_argument('--compiler',help='Host GCC executable for driver conversion')
    parser.add_argument('--bios',help='Optional user-supplied USA Mega CD BIOS for original startup font/star resources')
    parser.add_argument('--opening-callbacks',action='store_true',help='Convert clean USA D1/D0 executable callbacks, retaining dynamic CD waits (optional Unicorn CPU library required)')
    args=parser.parse_args()
    dest=Path(args.output).resolve();src=Path(args.source).resolve(strict=True)
    if dest==src or (src.is_dir() and dest.is_relative_to(src)):
        raise ValueError('The output must be outside the original source directory')
    if dest.is_relative_to(Path('C:/CODEX/snatcher-segacd-kr').resolve()):
        raise ValueError('The reference project is read-only')
    if dest.exists():raise ValueError('Choose a new output directory; existing files are preserved')
    if args.story_pcm_only or args.battle_feedback_only or args.headquarters_speech_only or args.contact_speech_only:
        with SourceDisc(src) as source:
            if args.contact_speech_only:
                from convert_contact_speech import build as build_current
                outputs,report=build_current(source)
            elif args.headquarters_speech_only:
                from convert_headquarters_speech import build as build_current
                outputs,report=build_current(source)
            elif args.story_pcm_only:
                from convert_native_speech import build as build_current
                outputs,report=build_current(source,[16,37,*range(130,137)],allow_effect_banks=True)
            else:
                from convert_battle_feedback import build as build_current
                bindings=json.loads((Path(__file__).parent/'portrait-bindings-v1.json').read_text())
                outputs,report=build_current(source,bindings)
            inputs=source.used.copy()
        dest.mkdir(parents=True)
        for name,data in outputs.items():(dest/name).write_bytes(data)
        manifest=dict(version=1,status='Original resource conversion; full game assembly pending',
            source=str(src),source_writes=0,screen_pixel_inputs=False,bundled_original_assets=False,
            original_inputs=inputs,resources=report,
            outputs=[dict(name=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()) for name,data in outputs.items()])
        (dest/'conversion.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
        print('Converted original story resources:',sum(map(len,outputs.values())),'bytes; source unchanged.')
        return
    if args.forecourt_driver_only or args.vehicle_driver_only or args.gameover_driver_only or args.headquarters_driver_only:
        score_driver=args.vehicle_driver_only or args.gameover_driver_only or args.headquarters_driver_only
        if not args.engine or not args.compiler or (score_driver and not args.cpu_core):
            parser.error('Driver conversion requires --engine and --compiler; score conversion also requires --cpu-core')
        engine=Path(args.engine).resolve(strict=True)
        compiler=Path(args.compiler).resolve(strict=True)
        dest.mkdir(parents=True)
        if score_driver:
            from convert_score_driver import build as build_driver
            extra=[Path(args.cpu_core).resolve(strict=True)]
        else:
            from convert_forecourt_driver import build as build_driver
            extra=[]
        with SourceDisc(src) as source:
            options={'command':0x5a if args.headquarters_driver_only else 0x5e if args.gameover_driver_only else 0x62} if score_driver else {}
            outputs,report=build_driver(source,engine,*extra,dest/'host',compiler,**options)
            inputs=source.used.copy()
        for name,data in outputs.items():(dest/name).write_bytes(data)
        manifest=dict(version=1,status='Source-derived music inputs; full shared IRQ and complete game assembly pending',
            source=str(src),source_writes=0,screen_pixel_inputs=False,bundled_original_assets=False,
            original_inputs=inputs,driver=report,
            outputs=[dict(name=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()) for name,data in outputs.items()])
        (dest/'conversion.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
        print('Converted original music inputs:',sum(len(data) for data in outputs.values()),'bytes; source unchanged.')
        return
    if args.entry_player_only or args.flight_player_only:
        if args.flight_player_only:
            from convert_flight_player import build as build_entry_player
        else:
            from convert_entry_player import build as build_entry_player
        with SourceDisc(src) as source:
            outputs,report=build_entry_player(source);inputs=source.used.copy()
        manifest=dict(version=1,status='Original flight, arrival and forecourt inputs; full game assembly pending' if args.flight_player_only else 'Native entry inputs; main entry and FM4F connected, full game assembly pending',
            source=str(src),source_writes=0,screen_pixel_inputs=False,bundled_original_assets=False,
            original_inputs=inputs,entry=report,
            outputs=[dict(name=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest()) for name,data in outputs.items()])
        dest.mkdir(parents=True)
        for name,data in outputs.items():(dest/name).write_bytes(data)
        (dest/'conversion.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
        print('Extracted native flight inputs:' if args.flight_player_only else 'Extracted native entry inputs:',sum(len(data) for data in outputs.values()),'bytes; source unchanged.')
        return
    if args.entry_script_only:
        with SourceDisc(src) as source:
            data,report=build_entry_program(source)
            inputs=source.used.copy()
        name='native-entry-script-r22.bin'
        manifest=dict(version=1,status='Original entry script; continuous scene/audio integration pending',
            source=str(src),source_writes=0,screen_pixel_inputs=False,bundled_original_assets=False,
            original_inputs=inputs,original_entry_program=report,
            outputs=[dict(name=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())])
        dest.mkdir(parents=True)
        (dest/name).write_bytes(data)
        (dest/'conversion.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
        print('Extracted original entry script:',len(data),'bytes; source unchanged.')
        return
    if args.entry_effects_only:
        with SourceDisc(src) as source:
            data,report=build_entry_effects(source)
            inputs=source.used.copy()
        name='native-entry-pcm-bank-r22.bin'
        manifest=dict(version=1,status='Entry effect resources; native playback integration pending',
            source=str(src),source_writes=0,screen_pixel_inputs=False,bundled_original_assets=False,
            original_inputs=inputs,original_entry_pcm_effects=report,
            outputs=[dict(name=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())])
        dest.mkdir(parents=True)
        (dest/name).write_bytes(data)
        (dest/'conversion.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf8')
        print('Extracted original entry wave bank:',len(data),'bytes; source unchanged.')
        return
    bindings=json.loads(Path(__file__).with_name('portrait-bindings-v1.json').read_text(encoding='utf-8'))
    # Finish validation and conversion before creating output files.
    with SourceDisc(src) as source:
        portrait,report=build(bindings,source)
        outputs={'native-portraits-r22.nspr':portrait}
        office,office_report=build_office(bindings,source)
        outputs['native-office-actors-r22.nsar']=office
        background,background_report=build_background(source)
        outputs['native-office-background-r22.nsbg']=background
        reception,reception_report=build_reception(source)
        outputs['native-reception-background-r22.nsbg']=reception
        opening_timeline,opening_report=build_opening_timeline(source)
        outputs['native-opening-timeline-r22.nstm']=opening_timeline
        opening_callbacks_report=None;travel_callbacks_report=None
        if args.opening_callbacks:
            from convert_opening_callbacks import build as build_callbacks
            original_opening=source.read_file('DATA_D1.BIN')
            if hashlib.sha256(original_opening).hexdigest()!='f60026501fd348935b74e6cf2936235bdd2d87bce01c8a3d21b1cb67eb7b8654':
                raise ValueError('Native upper opening requires clean USA DATA_D1; Korean captions are imported separately')
            opening_callbacks,opening_callbacks_report=build_callbacks(source)
            outputs['native-opening-03-r22.nsop']=opening_callbacks
            original_travel=source.read_file('DATA_D0.BIN')
            if hashlib.sha256(original_travel).hexdigest()!='2887e2690cc2e9608ddcbed1c3323b9ff590a0635050c5f6ab78efbfee3873af':
                raise ValueError('Native upper travel requires clean USA DATA_D0; Korean captions are imported separately')
            travel_callbacks,travel_callbacks_report=build_callbacks(source,'DATA_D0.BIN',4,2)
            outputs['native-opening-04-r22.nsop']=travel_callbacks
        sound_tables,sound_report=build_sound_tables(source)
        outputs['native-fm-tables-r22.nsfm']=sound_tables
        sega_report=None
        konami_report=None
        if not source.directory:
            konami,konami_report=build_konami(source)
            outputs['native-konami-startup-r22.nsbg']=konami
            cards,cards_report=build_startup_cards(source);outputs.update(cards)
        else:cards_report=None
        if args.bios:
            sega,sega_report=build_sega(source,Path(args.bios).resolve(strict=True))
            outputs['native-sega-license-r22.nsbg']=sega
        voices=[];entry_effects_report=None;entry_program_report=None
        native_files=[];boot_files=[];cdda_records=[];stream_records=[]
        if not args.portraits_only:
            pcm,voices=convert(source);outputs.update(pcm)
            entry_effects,entry_effects_report=build_entry_effects(source)
            outputs['native-entry-pcm-bank-r22.bin']=entry_effects
            entry_program,entry_program_report=build_entry_program(source)
            outputs['native-entry-script-r22.bin']=entry_program
            archive,native_files=build_archive(source)
            outputs['native-resource-banks.nrs']=archive
            boot,boot_files=bootstrap(source);outputs.update(boot)
            cdda,cdda_records=build_cdda(source);outputs.update(cdda)
            streams,stream_records=build_cdda_stream(cdda);outputs.update(streams)
        inputs=source.used.copy()
    dest.mkdir(parents=True)
    for name,data in outputs.items():(dest/name).write_bytes(data)
    manifest=dict(version=1,status='resource conversion prototype; final ROM assembly pending',
        source=str(src),source_writes=0,screen_pixel_inputs=False,
        bundled_original_assets=False,original_inputs=inputs,portraits=report,voices=voices,
        native_resource_banks=native_files,original_bootstrap=boot_files,
        office_actor_commands=office_report,
        office_background_planes=background_report,
        reception_background_planes=reception_report,
        original_opening_timeline=opening_report,
        original_opening_callbacks=opening_callbacks_report,
        original_travel_callbacks=travel_callbacks_report,
        original_cdda=cdda_records,
        original_cdda_ds_streams=stream_records,
        original_fm_tables=sound_report,
        original_entry_pcm_effects=entry_effects_report,
        original_entry_program=entry_program_report,
        original_sega_license=sega_report,
        original_konami_startup=konami_report,
        original_startup_cards=cards_report,
        outputs=[dict(name=name,bytes=len(data),sha256=hashlib.sha256(data).hexdigest())
                 for name,data in outputs.items()],
        pending=['Native large-scene and opening animation VM',
                 'Original FM/CDDA music and sound effects',
                 'Game asset extraction and complete asset-free ROM assembly'])
    (dest/'conversion.json').write_text(json.dumps(manifest,ensure_ascii=False,indent=2),encoding='utf-8')
    print(f'Converted {len(report["pattern_bindings"])} original tiles / {report["frames"]} portrait poses; {len(voices)} PCM resources.')
    print('Source unchanged. Final NDS assembly is still in development.')

if __name__=='__main__':
    try:run()
    except (OSError,ValueError,KeyError,AssertionError) as e:
        print(f'Conversion failed: {e}',file=sys.stderr);sys.exit(1)
