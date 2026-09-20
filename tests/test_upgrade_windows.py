"""Exercise actual Win32 sharing violations; --build-only allows cross-compilation."""
import argparse
import os
import pathlib
import subprocess
import sys
import tempfile

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('--build-only', action='store_true')
args = parser.parse_args()
if os.name != 'nt' and not args.build_only:
    raise SystemExit('Run on Windows, or use --build-only for compilation without execution.')
root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='portman-win-upgrade-') as temp:
    binary = pathlib.Path(temp) / 'upgrade-windows.exe'
    command = [sys.executable, '-m', 'ziglang', 'cc', '-target', 'x86_64-windows-gnu',
               '-std=c11', '-O0', '-g', str(root / 'tests' / 'upgrade_windows.c'), '-o', str(binary)]
    command += ['-l' + lib for lib in ['user32', 'gdi32', 'shell32', 'ole32', 'advapi32',
                                      'uuid', 'comctl32', 'uxtheme', 'dwmapi']]
    subprocess.run(command, check=True, cwd=root)
    if args.build_only:
        print('Native Windows upgrade tests compiled; NOT executed.')
    else:
        subprocess.run([str(binary)], check=True, timeout=60, cwd=root)
