"""Build a standalone EXE from this checkout's hash-verified source kit."""
from pathlib import Path
import hashlib
import importlib.metadata as metadata
import json
import shutil
import subprocess
import sys
import zipfile
import argparse

HERE = Path(__file__).resolve().parent
ROOT = HERE.parents[1]
WORK = ROOT/'work/exe-patcher-v100'
PACKAGE = ROOT/'source-kit'
PACKAGER = ROOT/'work/exe-patcher-bootloader/pyinstaller-6.22.3'


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as f:
        for b in iter(lambda: f.read(4*1024*1024), b''):
            h.update(b)
    return h.hexdigest()


def build():
    from patcher_app import MANIFEST_SHA
    assert digest(PACKAGE/'package-files.json') == MANIFEST_SHA
    manifest = json.loads((PACKAGE/'package-files.json').read_text(encoding='utf8'))
    for rel, expected in manifest['files'].items():
        path = (PACKAGE/rel).resolve(strict=True)
        assert path.is_relative_to(PACKAGE.resolve()) and digest(path) == expected, rel
    WORK.mkdir(parents=True, exist_ok=True)
    notices = WORK/'runtime-licenses'
    notices.mkdir(exist_ok=True)
    versions = {}
    for name in ('numpy', 'Pillow', 'unicorn', 'pyinstaller'):
        dist = metadata.distribution(name)
        versions[name] = dist.version
        for rel in dist.files:
            if any('license' in x.lower() or 'copying' in x.lower() for x in rel.parts):
                source = Path(dist.locate_file(rel))
                if source.is_file():
                    target = notices/name/str(rel).replace('../', '_parent_/')
                    target.parent.mkdir(parents=True, exist_ok=True)
                    shutil.copyfile(source, target)
    for source in (Path(sys.base_prefix)/'LICENSE.txt',
                   HERE/'licenses/Tcl-license.terms',
                   Path(sys.base_prefix)/'tcl/tk8.6/license.terms'):
        if not source.is_file():
            raise FileNotFoundError(source)
        shutil.copyfile(source, notices/(source.parent.name+'-'+source.name))
    (notices/'RUNTIME.json').write_text(json.dumps(dict(python=sys.version, packages=versions), indent=2), encoding='utf8')
    # Unicorn's Windows longjmp is incompatible with a CFG-enabled launcher.
    # Use the official source-built MinGW launcher, without modifying OS settings.
    import pefile
    bootloader = PACKAGER/'PyInstaller/bootloader/Windows-64bit-intel/runw.exe'
    assert bootloader.is_file()
    pe = pefile.PE(str(bootloader), fast_load=True)
    assert pe.FILE_HEADER.Machine == 0x8664
    assert not pe.OPTIONAL_HEADER.DllCharacteristics & 0x4000
    pe.close()
    command = [sys.executable, '-I', '-X', 'utf8', '-B', '-c',
        "import sys,runpy; sys.path.insert(0,sys.argv.pop(1)); runpy.run_module('PyInstaller',run_name='__main__')",
        str(PACKAGER),
        '--noconfirm', '--clean', '--onefile', '--windowed', '--noupx',
        '--name', 'Snatcher-DS-Patcher', '--distpath', str(WORK/'dist'),
        '--workpath', str(WORK/'build'), '--specpath', str(WORK),
        '--add-data', str(PACKAGE)+';payload',
        '--add-data', str(notices)+';runtime-licenses',
        '--hidden-import', 'numpy', '--hidden-import', 'PIL.Image',
        '--hidden-import', 'PIL.PngImagePlugin', '--collect-all', 'unicorn',
        '--exclude-module', 'pytest', '--exclude-module', 'numpy.testing',
        '--exclude-module', 'matplotlib', '--exclude-module', 'IPython',
        str(HERE/'patcher_app.py')]
    with (WORK/'build.log').open('w', encoding='utf8') as log:
        subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=True)
    exe = WORK/'dist/Snatcher-DS-Patcher.exe'
    result = dict(passed=True, exe=str(exe), bytes=exe.stat().st_size, sha256=digest(exe),
                  payload_manifest_sha256=MANIFEST_SHA, runtime=versions, python=sys.version,
                  bootloader_sha256=digest(bootloader), bootloader_cfg=False,
                  source={p.name: digest(p) for p in HERE.glob('*.py')},
                  original_game_included=False, bios_included=False, public_demo_updated=False)
    (WORK/'build-result.json').write_text(json.dumps(result, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(result, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('--work', default='exe-patcher-v100')
    parser.add_argument('--pyinstaller-source', type=Path, default=PACKAGER)
    args = parser.parse_args()
    if not args.work.replace('-', '').isalnum():
        parser.error('Invalid work directory name')
    WORK = ROOT/'work'/args.work
    PACKAGER = args.pyinstaller_source.resolve(strict=True)
    build()
