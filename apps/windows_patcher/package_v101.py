"""Package v1.01 only after the actual frozen EXE conversion is verified."""
import argparse
import hashlib
import json
from pathlib import Path
import shutil
import zipfile

from patcher_app import MANIFEST_SHA, NDS_SHA, payload_root, sha

ROOT = Path(__file__).resolve().parents[2]
WORK = ROOT / "work/exe-patcher-v101"
EXE_NAME = "Snatcher-DS-v1.01-Patcher.exe"
PACKAGE_NAME = "DS-Snatcher-Kr-MegaCD-Porting-v1.01-Patcher"


def package(out, bootloader_source):
    proof = json.loads((WORK / "검증 한글 공백 경로/verification.json").read_text(encoding="utf8"))
    build = json.loads((WORK / "build-result.json").read_text(encoding="utf8"))
    runtime = json.loads((ROOT / "docs/verification-v1.01.json").read_text(encoding="utf8"))
    exe = WORK / "dist/Snatcher-DS-Patcher.exe"
    assert proof["passed"] and proof["conversion"]["nds_sha256"] == NDS_SHA
    assert runtime["actual_exe_conversion_verified"] and runtime["nds_sha256"] == NDS_SHA
    assert all(row["passed"] for row in runtime["startup_tests"])
    assert sha(exe) == proof["exe_sha256"] == build["sha256"] == runtime["exe_sha256"]
    source = Path(__file__).resolve().parent
    for name, digest in build["source"].items():
        assert sha(source / name) == digest, "Changed tested source: " + name
    from PyInstaller.archive.readers import CArchiveReader
    archive = CArchiveReader(str(exe))
    entries = {name.replace("\\", "/"): name for name in archive.toc}
    root = payload_root()
    assert sha(root / "package-files.json") == MANIFEST_SHA
    payload = {p.relative_to(root).as_posix(): p for p in root.rglob("*") if p.is_file()}
    embedded = {name.removeprefix("payload/"): original for name, original in entries.items()
                if name.startswith("payload/")}
    assert set(payload) == set(embedded)
    for name, path in payload.items():
        assert hashlib.sha256(archive.extract(embedded[name])).hexdigest() == sha(path), name
    out.mkdir(parents=True, exist_ok=False)
    folder = out / PACKAGE_NAME
    folder.mkdir()
    shutil.copyfile(exe, out / EXE_NAME)
    shutil.copyfile(exe, folder / EXE_NAME)
    for name in ("README.md", "사용 방법.txt", "RELEASE_NOTES_KO.md",
                 "THIRD_PARTY_NOTICES.md", "INPUT-SHA256-v1.01.txt"):
        shutil.copyfile(ROOT / name, folder / name)
    shutil.copyfile(ROOT / "INPUT-SHA256-v1.01.txt", out / "INPUT-SHA256-v1.01.txt")
    for name in ("docs", "screenshots", "runtime-licenses"):
        shutil.copytree(ROOT / name, folder / name)
    shutil.copytree(ROOT / "source-kit", folder / "source/source-kit")
    shutil.copytree(source, folder / "source/apps/windows_patcher",
                    ignore=shutil.ignore_patterns("__pycache__", "*.pyc"))
    shutil.copyfile(bootloader_source, folder / "source" / bootloader_source.name)
    shutil.copyfile(ROOT / "docs/BUILDING.md", folder / "source/BUILDING.md")
    all_files = sorted(p for p in folder.rglob("*") if p.is_file())
    assert not any(p.suffix.lower() in (".nds", ".cue", ".iso", ".sav", ".wav", ".pcm")
                   for p in all_files)
    (folder / "SHA256.txt").write_text(
        "".join(sha(p) + "  " + p.relative_to(folder).as_posix() + "\n" for p in all_files),
        encoding="utf8")
    zipped = out / (PACKAGE_NAME + ".zip")
    with zipfile.ZipFile(zipped, "x", zipfile.ZIP_DEFLATED, compresslevel=6) as z:
        for path in sorted(p for p in folder.rglob("*") if p.is_file()):
            z.write(path, folder.name + "/" + path.relative_to(folder).as_posix())
    with zipfile.ZipFile(zipped) as z:
        assert z.testzip() is None
        assert hashlib.sha256(z.read(folder.name + "/" + EXE_NAME)).hexdigest() == sha(exe)
    assets = [out / EXE_NAME, zipped, out / "INPUT-SHA256-v1.01.txt"]
    sums = "".join(sha(p) + "  " + p.name + "\n" for p in assets)
    (out / "SHA256SUMS-v1.01.txt").write_text(sums, encoding="utf8")
    (ROOT / "SHA256SUMS-v1.01.txt").write_text(sums, encoding="utf8")
    report = dict(passed=True, assets=[dict(name=p.name, bytes=p.stat().st_size, sha256=sha(p))
                                      for p in assets],
                  payload_files=len(payload), payload_manifest_sha256=MANIFEST_SHA,
                  nds_sha256=NDS_SHA, embedded_payload_verified=True,
                  original_game_included=False, bios_included=False,
                  bootloader_source_sha256=sha(bootloader_source))
    (out / "packaging-verification.json").write_text(json.dumps(report, indent=2), encoding="utf8")
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--output", type=Path, default=ROOT / "work/public-v101")
    parser.add_argument("--bootloader-source", type=Path, required=True)
    args = parser.parse_args()
    package(args.output.resolve(), args.bootloader_source.resolve(strict=True))
