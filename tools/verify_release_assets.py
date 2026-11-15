#!/usr/bin/env python3
"""Check the frozen v1.12 download hashes, CRCs and payloads against this source tree."""
import argparse
import hashlib
from pathlib import Path, PurePosixPath
import zipfile


def require(condition, message):
    if not condition:
        raise SystemExit(message)


def main():
    root = Path(__file__).resolve().parents[1]
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('assets', type=Path, help='directory containing the three ZIPs and checksums')
    args = parser.parse_args()
    assets = args.assets.resolve()
    expected = {
        'R7Intervalometer-v1.12-OLED-MIT.zip': ('R7Intervalometer', root / 'OLED/R7Intervalometer'),
        'R7Intervalometer-v1.12-TFT-MIT.zip': ('R7Intervalometer', root / 'TFT/R7Intervalometer'),
        'R7Intervalometer-v1.12-Launch-MIT.zip': ('R7Intervalometer-Launch', root),
    }
    manifest = (root / 'release/ASSETS.sha256').read_bytes()
    require((assets / 'R7Intervalometer-v1.12-MIT-SHA256SUMS.txt').read_bytes() == manifest,
            'Asset checksum file differs from release/ASSETS.sha256')
    entries = [line.split() for line in manifest.decode().splitlines() if line.strip()]
    require(len(entries) == len(expected) and all(len(row) == 2 for row in entries),
            'Invalid checksum manifest')
    require({row[1] for row in entries} == set(expected), 'Unexpected asset names')
    for digest, name in entries:
        path = assets / name
        require(hashlib.sha256(path.read_bytes()).hexdigest() == digest, 'SHA-256 mismatch: ' + name)
        prefix, destination = expected[name]
        with zipfile.ZipFile(path) as archive:
            require(archive.testzip() is None, 'ZIP CRC failed: ' + name)
            members = archive.namelist()
            require(len(members) == len(set(members)), 'Duplicate ZIP entry: ' + name)
            for member in members:
                parts = PurePosixPath(member).parts
                require(len(parts) > 1 and parts[0] == prefix and '..' not in parts,
                        'Unexpected ZIP path: ' + member)
                target = destination.joinpath(*parts[1:])
                require(target.is_file() and archive.read(member) == target.read_bytes(),
                        'Archive/source difference: ' + member)
        print('PASS: SHA-256, ZIP CRC and source bytes: ' + name)
    print('Frozen v1.12 assets match this repository. No files were extracted or changed.')


if __name__ == '__main__':
    main()
