"""Build guard for the local FatFs compatibility declarations, not an updater."""
from pathlib import Path
import hashlib,json,os,sys
HERE=Path(__file__).resolve().parent
def sha(p):
    with p.open('rb') as f:return hashlib.file_digest(f,'sha256').hexdigest()
def check():
    meta=json.loads((HERE/'sdk.json').read_text(encoding='utf8'))
    path=Path(sys.argv[1])
    if os.name=='nt' and sys.argv[1].startswith('/opt/'):
        roots=[p/'tools/msys64'/sys.argv[1].lstrip('/') for p in HERE.parents]
        path=next((p for p in roots if p.is_file()),path)
    assert path.is_file(),'Cannot locate the linked BlocksDS libnds9.a: '+str(path)
    assert sha(path)==meta['sdk_library_sha256'],'BlocksDS changed: re-audit FatFs descriptor/layout/cache semantics before building.'
    for name,digest in meta['declarations'].items():assert sha(HERE/name)==digest,'Storage ABI declaration changed: '+name
    print('Validated pinned storage SDK and FatFs declarations')
if __name__=='__main__':check()
