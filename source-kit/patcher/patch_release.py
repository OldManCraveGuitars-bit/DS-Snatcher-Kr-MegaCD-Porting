"""v1.0 full-resource patcher: user originals -> Korean NDS.

Paths are relative to this package, never to a developer checkout. The frozen
public v0.1Demo entry point is separate and is not changed by this program.
"""
from pathlib import Path
from types import SimpleNamespace
import argparse
import hashlib
import importlib
import json
import os
import struct
import sys

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parent))


def file_hash(path):
    h = hashlib.sha256()
    with Path(path).open('rb') as stream:
        for block in iter(lambda: stream.read(4 * 1024 * 1024), b''):
            h.update(block)
    return h.hexdigest()


def check_package(root):
    manifest = json.loads((root / 'package-files.json').read_text(encoding='utf8'))
    for name, expected in manifest['files'].items():
        path = (root / name).resolve(strict=True)
        if not path.is_relative_to(root) or not path.is_file():
            raise ValueError('Unsafe package entry: ' + name)
        if file_hash(path) != expected:
            raise ValueError('Package file changed or damaged: ' + name)
    if os.name != 'nt' or struct.calcsize('P') != 8:
        raise ValueError('This helper package requires Windows and 64-bit Python.')
    for name in ('numpy', 'PIL', 'unicorn'):
        importlib.import_module(name)
    return manifest


def select_inputs():
    import tkinter as tk
    from tkinter import filedialog, messagebox
    ui = tk.Tk()
    ui.withdraw()
    try:
        messagebox.showinfo('SNATCHER DS V1.0 비공식 한글화·포팅 패처',
            'V1.0 비공식 한글화·포팅 패처입니다. 버그와 미검증 구간이 남아 있습니다.\n'
            'Mega-CD판 최종 한글 번역을 자동 적용합니다.\n\n'
            '깨끗한 미국판 원본 CUE와 USA BIOS를 선택해 주세요.')
        source = filedialog.askopenfilename(title='미국판 SNATCHER 원본 CUE', filetypes=[('CUE', '*.cue')])
        if not source:
            return None
        bios = filedialog.askopenfilename(title='Sega CD USA BIOS', filetypes=[('BIOS', '*.bin'), ('All', '*.*')])
        if not bios:
            return None
        parent = filedialog.askdirectory(title='새 결과 폴더를 만들 위치')
        if not parent:
            return None
        return source, bios, str(Path(parent) / 'Snatcher-DS-v1.0-output')
    finally:
        ui.destroy()


def run(argv=None):
    parser = argparse.ArgumentParser(description=__doc__)
    for name in ('source', 'bios', 'output'):
        parser.add_argument('--' + name)
    parser.add_argument('--check', action='store_true', help='Check package and Python dependencies without generating an NDS')
    args = parser.parse_args(argv)
    root = Path(__file__).resolve().parents[1]
    manifest = check_package(root)
    if args.check:
        print('Package and dependencies OK:', manifest['label'], flush=True)
        return
    if not any((args.source, args.bios, args.output)):
        inputs = select_inputs()
        if inputs is None:
            return
        args.source, args.bios, args.output = inputs
    elif not all((args.source, args.bios, args.output)):
        parser.error('--source, --bios and --output must be supplied together')
    source = Path(args.source).resolve(strict=True)
    bios = Path(args.bios).resolve(strict=True)
    out = Path(args.output).resolve()
    if out.exists() or out.is_relative_to(root):
        raise ValueError('Choose a new output folder outside the patcher folder.')
    if source.suffix.lower() != '.cue':
        raise ValueError('The complete game requires the original CUE and all its BIN tracks.')
    if file_hash(bios) != 'b1c2036b79467514eaaa69c76e5ba83801621e821059caa53cb921a0b21e3af4':
        raise ValueError('Unsupported USA BIOS. See README_KO.txt for the required SHA-256.')
    from build_release_resources import generate
    print('원본 리소스를 변환하여 NDS를 만듭니다. 수 분 걸릴 수 있습니다.', flush=True)
    report = generate(SimpleNamespace(
        source=str(source), bios=str(bios), output=str(out),
        manifest=str(root / 'engine/resources.json'),
        engine=str(root / 'converter/player'),
        cpu_core=str(root / 'converter/third_party/GenesisPlusGX/core'),
        compiler='bundled', dependencies=str(root / 'converter'),
        theme=str(root / 'art/junker-upper-r1.png'),
        translation_header=str(root / 'player/generated/text.h'),
        helpers=str(root / 'host/windows-x64'),
        engine_bundle=str(root / 'engine'), verify_only=True))
    if not report['all_resource_identities_match'] or not report['complete_nds_assembly']:
        raise ValueError('Conversion incomplete. See conversion.json; no final NDS was published.')
    final = dict(report['assembly'], label=manifest['label'], translation='final-review-r2/r61', release='v1.0',
                 known_bugs=True, full_game_verified=False, public_release=True)
    (out / 'patch-result.json').write_text(json.dumps(final, ensure_ascii=False, indent=2), encoding='utf8')
    print('SUCCESS:', out / 'snatcher_ds.nds', flush=True)
    print('SHA256:', final['nds_sha256'], flush=True)


if __name__ == '__main__':
    try:
        run()
    except Exception as error:
        print('PATCH FAILED:', error, file=sys.stderr, flush=True)
        sys.exit(1)
