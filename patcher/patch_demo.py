"""v0.1Demo one-time Windows patcher: original USA CUE/BIN + BIOS -> NDS."""
from pathlib import Path
import argparse
import json
import subprocess
import sys

sys.dont_write_bytecode=True


def run():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--source');parser.add_argument('--bios');parser.add_argument('--output')
    args=parser.parse_args()
    if not any((args.source,args.bios,args.output)):
        import tkinter as tk
        from tkinter import filedialog,messagebox
        ui=tk.Tk();ui.withdraw()
        messagebox.showinfo('SNATCHER DS v0.1Demo','버그가 남아 있으며 일부 구간만 진행할 수 있는 데모입니다.\n1회성 패처로 다음 배포 버전까지 업데이트하지 않습니다.\n\n미국판 원본 CUE와 USA BIOS를 선택해 주세요.')
        args.source=filedialog.askopenfilename(title='미국판 SNATCHER 원본 CUE',filetypes=[('CUE','*.cue')])
        if not args.source:return
        args.bios=filedialog.askopenfilename(title='Sega CD USA BIOS',filetypes=[('BIOS','*.bin'),('All','*.*')])
        if not args.bios:return
        parent=filedialog.askdirectory(title='새 결과 폴더를 만들 위치')
        if not parent:return
        args.output=str(Path(parent)/'Snatcher-DS-v0.1Demo-output')
        ui.destroy()
    elif not all((args.source,args.bios,args.output)):
        parser.error('--source, --bios and --output are required together')
    root=Path(__file__).resolve().parents[1]
    out=Path(args.output).resolve()
    if out.exists():raise ValueError('Output already exists. Choose a new empty output path.')
    source=Path(args.source).resolve(strict=True);bios=Path(args.bios).resolve(strict=True)
    if out.is_relative_to(source.parent) or out.is_relative_to(bios):
        raise ValueError('Keep output outside the original disc folder.')
    out.mkdir(parents=True)
    manifest_path=root/'engine/resources.json'
    manifest=json.loads(manifest_path.read_text(encoding='utf8'))
    print('SNATCHER DS v0.1Demo: converting original resources. This may take several minutes.',flush=True)
    command=[sys.executable,'-B',str(root/'patcher/build_demo_resources.py'),
        '--source',str(source),'--bios',str(bios),'--output',str(out/'resources'),
        '--manifest',str(manifest_path),'--engine',str(root/'player'),
        '--cpu-core',str(root/'third_party/GenesisPlusGX/core'),'--compiler','bundled',
        '--dependencies',str(root),'--theme',str(root/'art/junker-upper-r1.png')]
    result=subprocess.run(command)
    result.check_returncode()
    report=json.loads((out/'resources/conversion.json').read_text(encoding='utf8'))
    if not report['all_resource_identities_match']:
        raise ValueError('Resource conversion failed. See resources/conversion.json; no NDS was published.')
    from iso_source import SourceDisc
    from assemble_demo import assemble
    with SourceDisc(source) as disc:
        final=assemble(disc,root/'engine',out/'resources',out/'DS-Snatcher-Kr-v0.1Demo.nds',manifest)
        sub=disc.read_file('SUBCODE.BIN')
        headers=out/'build-inputs';headers.mkdir()
        gain=sub[0x15fba-0xd400:0x160ba-0xd400]
        random=sub[0x12010-0xd400:0x12030-0xd400]
        (headers/'original_sound_gain.h').write_text('static const uint8_t sub_music_gain[256]={'+','.join(map(str,gain))+'};\n')
        (headers/'source_battle_random.h').write_text('static const unsigned char battle_random_map[32]={'+','.join(map(str,random))+'};\n')
    final.update(version='v0.1Demo',partial_game=True,known_bugs=True,one_time_patcher=True)
    (out/'patch-result.json').write_text(json.dumps(final,ensure_ascii=False,indent=2),encoding='utf8')
    print('SUCCESS:',out/'DS-Snatcher-Kr-v0.1Demo.nds',flush=True)
    print('SHA256:',final['nds_sha256'],flush=True)


if __name__=='__main__':
    try:run()
    except Exception as error:
        print('PATCH FAILED:',error,file=sys.stderr,flush=True)
        sys.exit(1)
