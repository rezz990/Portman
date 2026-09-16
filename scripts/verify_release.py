"""Verify a release directory before sharing it. Does not execute its binaries."""
import hashlib
import json
import pathlib
import re
import sys
import zipfile


def verify(directory):
    directory = pathlib.Path(directory)
    metadata = json.loads((directory / 'BUILD-INFO.json').read_text(encoding='utf-8'))
    version = metadata['version']
    if not re.fullmatch(r'\d+\.\d+\.\d+', version):
        raise ValueError('Invalid release version')
    expected = {f'Portman-Setup-{version}.exe', f'Portman-{version}-windows-x64.zip',
                f'Portman-{version}-source.zip', 'BUILD-INFO.json'}
    seen = set()
    for line in (directory / 'SHA256SUMS.txt').read_text(encoding='ascii').splitlines():
        digest, name = line.split('  ', 1)
        if name not in expected or name in seen or not re.fullmatch('[0-9a-f]{64}', digest):
            raise ValueError('Unexpected or duplicate checksum entry: ' + name)
        seen.add(name)
        if hashlib.sha256((directory / name).read_bytes()).hexdigest() != digest:
            raise ValueError('Checksum mismatch: ' + name)
    if seen != expected:
        raise ValueError('Missing release checksums')
    if {p.name for p in directory.iterdir()} != expected | {'SHA256SUMS.txt'}:
        raise ValueError('Release folder contains missing or unexpected files')
    for name in expected:
        if not name.endswith('.zip'):
            continue
        with zipfile.ZipFile(directory / name) as archive:
            names = archive.namelist()
            if archive.testzip() or len(names) != len(set(n.casefold() for n in names)):
                raise ValueError('Corrupt archive or duplicate archive paths: ' + name)
            for item in names:
                path = pathlib.PurePosixPath(item)
                if path.is_absolute() or '..' in path.parts or '\\' in item:
                    raise ValueError('Unsafe archive path')
            if name.endswith('windows-x64.zip'):
                if {pathlib.PurePosixPath(n).name for n in names} != {'Portman.exe', 'portman-cli.exe', 'QUICKSTART.txt'}:
                    raise ValueError('Portable package contents are incomplete')
            else:
                for item in names:
                    if {'.git', '.zig-cache', 'zig-out', 'dist', 'payload', '__pycache__'} & set(pathlib.PurePosixPath(item).parts):
                        raise ValueError('Generated data in source package')
                if not any(n.endswith('/VERSION') for n in names):
                    raise ValueError('Source package is missing VERSION')
    print('Release files, checksums and archives verified: ' + version)


if __name__ == '__main__':
    verify(sys.argv[1] if len(sys.argv) > 1 else 'dist/release')
