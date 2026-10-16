"""Recompile all affected DS objects when profiling flags change."""
from pathlib import Path
import json, sys

ROOT = Path(__file__).resolve().parents[1]
target = (ROOT / sys.argv[1]).resolve()
if not target.is_relative_to(ROOT / 'build') or target.name != 'profile-mode.json':
    raise ValueError('Profile stamp must stay inside this project build directory')
if any(value not in ('0', '1') for value in sys.argv[2:]) or len(sys.argv) != 4:
    raise ValueError('Expected opening and flight profiling flags')
flags = json.dumps(dict(opening=sys.argv[2] == '1', flight=sys.argv[3] == '1'), sort_keys=True) + '\n'
if not target.exists() or target.read_text(encoding='utf8') != flags:
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_text(flags, encoding='utf8')
