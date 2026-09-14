"""Reproducible Windows x64 build: native GUI, CLI and per-user installer."""
import argparse
import pathlib
import shutil
import subprocess
import sys

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
print('Windows binaries ready:', prefix / 'bin')
