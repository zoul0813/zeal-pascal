#!/usr/bin/env python3
"""Verify prepared asset hashes, native source constraints, and download consistency."""
from pathlib import Path
import hashlib
import json
import zipfile

book = Path(__file__).resolve().parents[1]
manifest = json.loads((book / 'static/provenance.json').read_text())
for name, asset in manifest['assets'].items():
    data = (book / 'static' / name).read_bytes()
    assert len(data) == asset['bytes'] and hashlib.sha256(data).hexdigest() == asset['sha256'], name
with zipfile.ZipFile(book / 'static/code/book-code.zip') as archive:
    for source in (book / 'static/code/lab').glob('*.PAS'):
        text = source.read_text(encoding='ascii')
        assert all(len(line) <= 127 for line in text.splitlines()), source
        assert archive.read('lab/' + source.name) == source.read_bytes(), source
print(f'Checked {len(manifest["assets"])} asset hashes and every lesson source/download.')
