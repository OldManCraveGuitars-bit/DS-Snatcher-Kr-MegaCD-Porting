"""Build NitroFS from current resources, excluding explicitly retired media.
Frozen release inputs stay in place. Staging uses same-volume hard links so
another full asset copy does not consume disk space.
"""
from pathlib import Path
import hashlib,json,os,sys
sys.dont_write_bytecode=True
from resource_manager import RETIRED
ROOT=Path(__file__).resolve().parents[1]
SOURCE=ROOT/'nitrofiles';DEST=ROOT/'build/runtime-nitrofs-r22'
REPORT=ROOT/'build/runtime-resources-r22.json'
def safe(path):
    resolved=path.resolve()
    if not resolved.is_relative_to(ROOT/'build') or resolved==ROOT/'build':
        raise ValueError('Staging target escaped build directory')
    for p in [path,*path.parents]:
        if p==ROOT:break
        if p.exists() and getattr(os.lstat(p),'st_file_attributes',0)&0x400:
            raise ValueError('Staging cannot traverse a reparse point')
    return path
safe(DEST).mkdir(parents=True,exist_ok=True)
code='\n'.join(p.read_text(encoding='utf8') for p in (ROOT/'source').iterdir() if p.suffix in ('.c','.h'))
assets=[];retired=[]
for src in sorted(SOURCE.iterdir()):
    if not src.is_file() or src.is_symlink():raise ValueError('Unexpected NitroFS entry: '+str(src))
    relative=src.relative_to(ROOT).as_posix()
    if relative in RETIRED:
        if src.name in code:raise ValueError('Retired resource still referenced: '+relative)
        retired.append(dict(name=src.name,bytes=src.stat().st_size));continue
    dest=safe(DEST/src.name)
    if dest.exists() and not os.path.samefile(src,dest):dest.unlink()
    if not dest.exists():os.link(src,dest)
    assets.append(dict(name=src.name,bytes=src.stat().st_size,sha256=hashlib.sha256(src.read_bytes()).hexdigest()))
names={a['name'] for a in assets}
for dest in DEST.iterdir():
    if dest.name not in names:
        if not dest.is_file():raise ValueError('Unexpected staging directory')
        safe(dest).unlink()
report=dict(assets=assets,excluded_retired=retired,bytes=sum(a['bytes'] for a in assets),
            excluded_bytes=sum(a['bytes'] for a in retired),method='Read-only packaging via hard links; original and frozen inputs unchanged')
encoded=json.dumps(report,indent=2)
if not REPORT.exists() or REPORT.read_text()!=encoded:REPORT.write_text(encoded)
print('NitroFS:',len(assets),'files;',report['bytes'],'bytes;',report['excluded_bytes'],'retired bytes excluded')
