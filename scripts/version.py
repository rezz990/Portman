"""Synchronize application labels from VERSION; --check never changes files."""
import argparse
import pathlib
import re

ROOT = pathlib.Path(__file__).resolve().parents[1]
FILES = ('build.zig', 'src/main.zig', 'src/desktop.c', 'windows/setup.c',
         'windows/app.rc', 'windows/app.manifest', 'windows/QUICKSTART.txt')


def read_version(root=ROOT):
    version = (root / 'VERSION').read_text(encoding='utf-8').strip()
    if not re.fullmatch(r'(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)', version):
        raise ValueError('VERSION must contain a numeric major.minor.patch version.')
    if any(int(part) > 65535 for part in version.split('.')):
        raise ValueError('Windows version components must fit unsigned 16-bit integers.')
    return version


def synchronize(root=ROOT, check=False):
    version = read_version(root)
    stale = []
    for name in FILES:
        path = root / name
        old = path.read_text(encoding='utf-8')
        pattern = r'((?:Portman-Setup-|Portman |portman |PORTMAN |L"DisplayVersion",L"))\d+\.\d+\.\d+(?![\d.])'
        new = re.sub(pattern, lambda m: m[1] + version, old)
        if name == 'windows/app.rc':
            new = re.sub(r'(VALUE "(?:FileVersion|ProductVersion)", ")\d+\.\d+\.\d+',
                         lambda m: m[1] + version, old)
        if name == 'windows/app.manifest':
            new = re.sub(r'(<assemblyIdentity version=")\d+\.\d+\.\d+\.0',
                         lambda m: m[1] + version + '.0', old)
        if name == 'windows/app.rc':
            new = re.sub(r'(?m)^( (?:FILEVERSION|PRODUCTVERSION) )\d+,\d+,\d+,\d+',
                         lambda m: m[1] + version.replace('.', ',') + ',0', new)
        if new != old:
            stale.append(name)
            if not check:
                path.write_text(new, encoding='utf-8')
    return stale


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--check', action='store_true')
    args = parser.parse_args()
    stale = synchronize(check=args.check)
    if stale:
        print(('Outdated version labels: ' if args.check else 'Updated: ') + ', '.join(stale))
    else:
        print('Version labels match ' + read_version())
    if args.check and stale:
        raise SystemExit(1)
