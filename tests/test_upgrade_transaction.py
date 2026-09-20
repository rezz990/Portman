"""Compile and execute the installer's portable failure-injection scenarios."""
import os
import pathlib
import subprocess
import sys
import tempfile

root = pathlib.Path(__file__).resolve().parents[1]
with tempfile.TemporaryDirectory(prefix='portman-upgrade-tests-') as temp:
    binary = pathlib.Path(temp) / ('upgrade-test.exe' if os.name == 'nt' else 'upgrade-test')
    subprocess.run([sys.executable, '-m', 'ziglang', 'cc', '-std=c11', '-Wall', '-Wextra',
                    '-Werror', '-O0', str(root / 'tests' / 'upgrade_transaction.c'),
                    '-o', str(binary)], check=True, cwd=root)
    subprocess.run([str(binary)], check=True, cwd=root)
