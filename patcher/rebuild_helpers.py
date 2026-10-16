"""Optional developer rebuild of Windows helper libraries from included sources."""
from pathlib import Path
import argparse
import json
import os
import shutil
from iso_source import SourceDisc


def run():
    p=argparse.ArgumentParser(description=__doc__)
    for key in ('source','bios','output','gcc'):p.add_argument('--'+key,required=True)
    a=p.parse_args();root=Path(__file__).resolve().parents[1];out=Path(a.output).resolve()
    if out.exists():raise ValueError('Choose a new output directory')
    out.mkdir(parents=True);engine=out/'player';shutil.copytree(root/'player',engine)
    os.environ['SNATCHER_FORCE_SOURCE']='1'
    from convert_forecourt_driver import build as forecourt
    from convert_score_driver import build as score
    from convert_boot_audio import build as boot
    from convert_room_image import build as room
    manifest=json.loads((root/'engine/resources.json').read_text())
    with SourceDisc(a.source) as disc:
        sub=disc.read_file('SUBCODE.BIN')
        (engine/'generated/original_sound_gain.h').write_text('static const uint8_t sub_music_gain[256]={'+','.join(map(str,sub[0x15fba-0xd400:0x160ba-0xd400]))+'};\n')
        forecourt(disc,engine,out/'forecourt',a.gcc)
        score(disc,engine,root/'third_party/GenesisPlusGX/core',out/'score',a.gcc,0x40)
        boot(disc,a.bios,root,out/'boot',a.gcc,root/'third_party/GenesisPlusGX/core')
        room(disc,engine,out/'room',a.gcc,{r['name']:r['sha256'] for r in manifest['files'] if r['name'].startswith('hq-')})
    print('Rebuilt helpers in',out,'with source tables restored from your disc.')


if __name__=='__main__':run()
