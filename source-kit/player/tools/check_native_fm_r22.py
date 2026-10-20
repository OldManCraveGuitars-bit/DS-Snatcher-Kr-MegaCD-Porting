"""Validate all independently generated demo resources before linking."""
from pathlib import Path
import hashlib,json
root=Path(__file__).resolve().parents[1]
manifest=json.loads((root/'engine/resources.json').read_text())
for row in manifest['files']:
    assert hashlib.sha256((root/'nitrofiles'/row['name']).read_bytes()).hexdigest()==row['sha256'],row['name']
print('Validated',len(manifest['files']),'demo resources')
