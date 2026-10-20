"""Publish local EXE artifacts only after the frozen conversion succeeds."""
from pathlib import Path
from datetime import datetime
from zoneinfo import ZoneInfo
import hashlib
import json
import shutil
import zipfile

from patcher_app import MANIFEST_SHA, NDS_SHA, payload_root, sha

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT/'work/exe-patcher-v1-r4'
OUT = ROOT/'work/releases/Snatcher-DS-EXE-Patcher-v1'


def package():
    proof = json.loads((WORK/'검증 한글 공백 경로/verification.json').read_text(encoding='utf8'))
    build = json.loads((WORK/'build-result.json').read_text(encoding='utf8'))
    source = Path(__file__).resolve().parent
    exe = WORK/'dist/Snatcher-DS-Patcher.exe'
    assert proof['passed'] and proof['conversion']['nds_sha256'] == NDS_SHA
    assert sha(exe) == proof['exe_sha256'] == build['sha256']
    for name, digest in build['source'].items():
        assert sha(source/name) == digest, 'Changed tested source: '+name
    # The complete embedded converter must equal the sealed original-free kit.
    from PyInstaller.archive.readers import CArchiveReader
    archive = CArchiveReader(str(exe))
    entries = {name.replace('\\', '/'): name for name in archive.toc}
    root = payload_root()
    assert sha(root/'package-files.json') == MANIFEST_SHA
    payload = {p.relative_to(root).as_posix(): p for p in root.rglob('*') if p.is_file()}
    embedded = {name.removeprefix('payload/'): original for name, original in entries.items()
                if name.startswith('payload/')}
    assert set(payload) == set(embedded)
    for name, path in payload.items():
        assert hashlib.sha256(archive.extract(embedded[name])).hexdigest() == sha(path), name
    OUT.mkdir(exist_ok=False)
    shutil.copyfile(exe, OUT/exe.name)
    shutil.copyfile(source/'사용 방법.txt', OUT/'사용 방법.txt')
    shutil.copytree(WORK/'runtime-licenses', OUT/'runtime-licenses')
    gui_source = OUT/'source/apps/windows_patcher'
    gui_source.mkdir(parents=True)
    for path in source.iterdir():
        if path.is_file():
            shutil.copyfile(path, gui_source/path.name)
    shutil.copytree(source/'licenses', gui_source/'licenses')
    for path in (ROOT/'work/releases/Snatcher-DS-source-kit-v26-ko081.zip',
                 ROOT/'work/exe-patcher-bootloader/pyinstaller-6.22.3.tar.gz'):
        shutil.copyfile(path, OUT/'source'/path.name)
    report = dict(passed=True, timestamp=datetime.now(ZoneInfo('Asia/Seoul')).isoformat(),
                  exe=exe.name, bytes=exe.stat().st_size, exe_sha256=sha(exe),
                  expected_nds_sha256=NDS_SHA, generated_resource_files=1230,
                  frozen_conversion_seconds=proof['conversion']['elapsed_seconds'],
                  verified_checks=proof['checks'], bundled_payload_files=len(payload),
                  payload_manifest_sha256=MANIFEST_SHA, original_game_included=False,
                  bios_included=False, public_demo_updated=False, desktop_click_test=False,
                  frozen_gui_construction_verified=True,
                  verification=str(WORK/'검증 한글 공백 경로/verification.json'),
                  bootloader_source_sha256=sha(ROOT/'work/exe-patcher-bootloader/pyinstaller-6.22.3.tar.gz'),
                  bootloader_sha256=build['bootloader_sha256'],
                  compatibility_note='Source-built MinGW PyInstaller launcher for Unicorn longjmp; no system security settings modified.')
    (OUT/'검증 결과.json').write_text(json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    files = sorted(p for p in OUT.rglob('*') if p.is_file())
    (OUT/'SHA256.txt').write_text(''.join(sha(p)+'  '+p.relative_to(OUT).as_posix()+'\n' for p in files), encoding='utf8')
    zipped = OUT.with_suffix('.zip')
    with zipfile.ZipFile(zipped, 'x', compression=zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for path in sorted(p for p in OUT.rglob('*') if p.is_file()):
            z.write(path, OUT.name+'/'+path.relative_to(OUT).as_posix())
    with zipfile.ZipFile(zipped) as z:
        assert z.testzip() is None
        assert hashlib.sha256(z.read(OUT.name+'/'+exe.name)).hexdigest() == sha(exe)
    report.update(zip=str(zipped), zip_bytes=zipped.stat().st_size, zip_sha256=sha(zipped),
                  delivered_exe=str(OUT/exe.name), guide=str(OUT/'사용 방법.txt'))
    (ROOT/'work/r22-resources/milestone-exe-patcher-v1.json').write_text(
        json.dumps(report, ensure_ascii=False, indent=2), encoding='utf8')
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == '__main__':
    package()
