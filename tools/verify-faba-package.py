#!/usr/bin/env python3
"""Check Nordic application package metadata/hash; extract signature for CryptoKit."""
import hashlib
import json
from pathlib import Path
import re
import sys
import zipfile

def varint(data, pos):
    value = shift = 0
    while pos < len(data) and shift < 70:
        byte = data[pos]; pos += 1
        value |= (byte & 127) << shift
        if byte < 128:
            return value, pos
        shift += 7
    raise ValueError('Invalid protobuf varint')

def fields(data):
    result = {}; pos = 0
    while pos < len(data):
        key, pos = varint(data, pos)
        if key & 7 == 0:
            value, pos = varint(data, pos)
        elif key & 7 == 2:
            size, pos = varint(data, pos)
            value = data[pos:pos + size]; pos += size
            assert len(value) == size
        else:
            raise ValueError('Unexpected protobuf wire type')
        assert key >> 3 not in result
        result[key >> 3] = value
    return result

package, source, output = map(Path, sys.argv[1:4])
expected_version = sys.argv[4] if len(sys.argv) == 5 else None
with zipfile.ZipFile(package) as z:
    assert z.testzip() is None
    manifest = json.loads(z.read('manifest.json'))['manifest']
    assert set(manifest) == {'application'}
    app = manifest['application']
    binary = z.read(app['bin_file']); packet = z.read(app['dat_file'])
signed = fields(fields(packet)[2]); command = fields(signed[1]); init = fields(command[2])
assert command[1] == 1
assert init[1] == 21601 and init[2] == 52 and init[4] == 0
sd = init[3]; assert sd == 0x103 or (isinstance(sd, bytes) and varint(sd, 0) == (0x103, len(sd)))
assert init[7] == len(binary) <= 0x5b000
assert fields(init[8])[1] == 3
assert fields(init[8])[2] == hashlib.sha256(binary).digest()[::-1]
assert binary == (source / 'fw/_build_faba/pixljs.bin').read_bytes()
if expected_version:
    assert expected_version.encode() + b'\0' in binary, 'Expected display version missing from image'
key_source = (source / 'fw/bootloader/src/dfu_public_key.c').read_text()
key = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})', key_source))
assert len(key) == 64
sig = signed[3]; assert len(sig) == 64
output.mkdir(parents=True, exist_ok=True)
(output / 'public-key.bin').write_bytes(b'\x04' + key[:32][::-1] + key[32:][::-1])
(output / 'signed-message.bin').write_bytes(command[2])
(output / 'signature.bin').write_bytes(sig[:32][::-1] + sig[32:][::-1])
report = dict(application_version=init[1], hardware_version=init[2], softdevice='0x0103',
              application_bytes=len(binary), application_sha256=hashlib.sha256(binary).hexdigest(),
              zip_sha256=hashlib.sha256(package.read_bytes()).hexdigest(),
              contents='application only', expected_display_version=expected_version, signature='pending separate CryptoKit verification')
(output / 'package-validation.json').write_text(json.dumps(report, indent=2) + '\n')
print(json.dumps(report, indent=2))
