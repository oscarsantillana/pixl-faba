#!/usr/bin/env python3
"""Verify shipped assets and Nordic metadata without building or using Bluetooth."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess
import sys
import tempfile

root = Path(__file__).resolve().parents[1]
for line in (root / 'release/SHA256SUMS').read_text().splitlines():
    expected, name = line.split('  ', 1)
    actual = hashlib.sha256((root / name).read_bytes()).hexdigest()
    if actual != expected:
        raise SystemExit('Checksum mismatch: ' + name)
manifest = json.loads((root / 'out/faba-catalog/manifest.json').read_text())
assert manifest['entries'] == 257 and len(manifest['files']) == 7
for i, entry in enumerate(manifest['files']):
    assert entry['file'] == f'cat{i}.bin'
    assert entry['device_path'] == f'E:/faba/cat{i}.bin'
    data = (root / 'out/faba-catalog' / entry['file']).read_bytes()
    assert len(data) == entry['bytes']
    assert hashlib.sha256(data).hexdigest() == entry['sha256']
with tempfile.TemporaryDirectory(prefix='pixl-faba-validation-') as tmp:
    source = Path(tmp) / 'source'
    output = Path(tmp) / 'report'
    (source / 'fw/_build_faba').mkdir(parents=True)
    (source / 'fw/bootloader/src').mkdir(parents=True)
    shutil.copy2(root / 'evidence/faba10/pixljs.bin', source / 'fw/_build_faba/pixljs.bin')
    shutil.copy2(root / 'firmware/pixl-faba/fw/bootloader/src/dfu_public_key.c', source / 'fw/bootloader/src/dfu_public_key.c')
    subprocess.run([sys.executable, str(root / 'tools/verify-faba-package.py'),
                    str(root / 'release/pixl-faba-2.16.1-faba10-preferidos.zip'),
                    str(source), str(output), '2.16.1-faba10'], check=True)
    for name in ['public-key.bin', 'signed-message.bin', 'signature.bin']:
        assert (output / name).read_bytes() == (root / 'evidence/faba10' / name).read_bytes(), name
print('PASS: release checksums, all catalogue files, Nordic metadata, binary and signature-evidence equality.')
print('For cryptographic verification, run .build/bin/verify-faba-signature evidence/faba10 after Mac setup.')
