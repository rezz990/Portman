"""Reproducible Windows x64 build: native GUI, CLI and per-user installer."""
import argparse
import pathlib
import shutil
import subprocess
import sys
import hashlib
import zipfile

parser = argparse.ArgumentParser()
parser.add_argument('--zig', help='Path to Zig 0.14.1 executable')
args = parser.parse_args()
root = pathlib.Path(__file__).resolve().parents[1]
zig = [args.zig] if args.zig else ([shutil.which('zig')] if shutil.which('zig') else [sys.executable, '-m', 'ziglang'])
version = subprocess.check_output(zig + ['version'], text=True).strip()
if version != '0.14.1':
    raise SystemExit(f'Use Zig 0.14.1, found {version}.')
prefix = root / 'dist' / 'windows-release'
# Keep the output deterministic: stale versioned installers must not be mistaken for the current build.
shutil.rmtree(prefix, ignore_errors=True)
common = ['build', '-Dtarget=x86_64-windows-gnu', '-Doptimize=ReleaseSafe', '--prefix', str(prefix)]
subprocess.run(zig + common, cwd=root, check=True)
payload = root / 'windows' / 'payload'
payload.mkdir(exist_ok=True)
for name in ('Portman.exe', 'portman-cli.exe'):
    shutil.copy2(prefix / 'bin' / name, payload / name)
subprocess.run(zig + common + ['-Dsetup=true'], cwd=root, check=True)

dist = root / 'dist'
version_name = '0.2.2'
portable_zip = dist / f'Portman-{version_name}-windows-x64.zip'
source_zip = dist / f'Portman-{version_name}-source.zip'
for archive in (portable_zip, source_zip):
    archive.unlink(missing_ok=True)
with zipfile.ZipFile(portable_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for name in ('Portman.exe', 'portman-cli.exe'):
        archive.write(prefix / 'bin' / name, f'Portman-{version_name}/{name}')
    archive.write(root / 'windows' / 'QUICKSTART.txt', f'Portman-{version_name}/QUICKSTART.txt')
excluded = {'.git', '.zig-cache', 'zig-out', 'dist', '__pycache__', 'payload'}
with zipfile.ZipFile(source_zip, 'w', zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
    for path in sorted(root.rglob('*')):
        relative = path.relative_to(root)
        if path.is_file() and not any(part in excluded for part in relative.parts):
            archive.write(path, pathlib.PurePosixPath(f'Portman-{version_name}-source') / relative)
artifacts = [prefix / 'bin' / f'Portman-Setup-{version_name}.exe', portable_zip, source_zip]
checksum = dist / 'SHA256SUMS.txt'
checksum.write_text(''.join(f'{hashlib.sha256(path.read_bytes()).hexdigest()}  {path.name}\n' for path in artifacts), encoding='ascii')
print('Windows binaries ready:', prefix / 'bin')
print('Release archives ready:', portable_zip, source_zip)
