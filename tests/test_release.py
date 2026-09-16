"""Release regression tests: version isolation and integrity failures."""
import contextlib
import hashlib
import io
import json
import pathlib
import sys
import tempfile
import unittest
import zipfile

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parents[1] / 'scripts'))
from version import FILES, synchronize, read_version
from verify_release import verify


class VersionTests(unittest.TestCase):
    def test_version_sync_preserves_addresses_and_windows_dependency(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            (root / 'VERSION').write_text('0.2.4\n')
            for name in FILES:
                path = root / name
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_text('Portman 0.2.3 http://127.0.0.1\n')
            manifest = root / 'windows/app.manifest'
            manifest.write_text('<assemblyIdentity version="0.2.3.0" name="Portman.Desktop"/>\n<assemblyIdentity name="Microsoft.Windows.Common-Controls" version="6.0.0.0"/>')
            self.assertTrue(synchronize(root, check=True))
            self.assertIn('0.2.3', (root / 'src/main.zig').read_text())
            synchronize(root)
            self.assertEqual(synchronize(root, check=True), [])
            self.assertIn('127.0.0.1', (root / 'src/main.zig').read_text())
            self.assertIn('6.0.0.0', manifest.read_text())
            self.assertIn('0.2.4.0', manifest.read_text())

    def test_invalid_windows_version(self):
        with tempfile.TemporaryDirectory() as d:
            root = pathlib.Path(d)
            for value in ['01.2.3', '1.2', '65536.0.0', '../1.2.3']:
                (root / 'VERSION').write_text(value)
                with self.assertRaises(ValueError):
                    read_version(root)


class ReleaseTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.addCleanup(self.temp.cleanup)
        self.root = pathlib.Path(self.temp.name)
        (self.root / 'BUILD-INFO.json').write_text(json.dumps({'version': '0.2.4'}))
        (self.root / 'Portman-Setup-0.2.4.exe').write_bytes(b'fixture installer')
        with zipfile.ZipFile(self.root / 'Portman-0.2.4-windows-x64.zip', 'w') as z:
            for name in ['Portman.exe', 'portman-cli.exe', 'QUICKSTART.txt']:
                z.writestr('Portman-0.2.4/' + name, b'fixture')
        with zipfile.ZipFile(self.root / 'Portman-0.2.4-source.zip', 'w') as z:
            z.writestr('source/VERSION', '0.2.4')
        self.checksums()

    def checksums(self):
        (self.root / 'SHA256SUMS.txt').write_text(''.join(
            hashlib.sha256(p.read_bytes()).hexdigest() + '  ' + p.name + '\n'
            for p in sorted(self.root.iterdir()) if p.name != 'SHA256SUMS.txt'))

    def test_valid_bundle(self):
        with contextlib.redirect_stdout(io.StringIO()):
            verify(self.root)

    def test_tampered_installer(self):
        (self.root / 'Portman-Setup-0.2.4.exe').write_bytes(b'changed')
        with self.assertRaisesRegex(ValueError, 'Checksum mismatch'):
            verify(self.root)

    def test_missing_checksum(self):
        (self.root / 'SHA256SUMS.txt').write_text('')
        with self.assertRaisesRegex(ValueError, 'Missing'):
            verify(self.root)

    def test_source_rejects_build_output_even_with_valid_hash(self):
        with zipfile.ZipFile(self.root / 'Portman-0.2.4-source.zip', 'a') as z:
            z.writestr('source/windows/payload/Portman.exe', b'generated')
        self.checksums()
        with self.assertRaisesRegex(ValueError, 'Generated data'):
            verify(self.root)

    def test_extra_stale_asset(self):
        (self.root / 'old-installer.exe').write_bytes(b'old')
        with self.assertRaisesRegex(ValueError, 'unexpected files'):
            verify(self.root)


if __name__ == '__main__':
    unittest.main()
