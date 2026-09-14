"""Exercise real sockets and child lifecycles. Python is a test dependency only."""
import json
import os
from pathlib import Path
import signal
import socket
import subprocess
import sys
import tempfile
import time
import unittest

BIN = str(Path(sys.argv.pop(1) if len(sys.argv) > 1 else 'zig-out/bin/portman').resolve())
WIN = os.name == 'nt'


def free_port():
    with socket.socket() as s:
        s.bind(('127.0.0.1', 0))
        return s.getsockname()[1]


def listening(port):
    with socket.socket() as s:
        s.settimeout(.15)
        return s.connect_ex(('127.0.0.1', port)) == 0


def until(predicate, timeout=8):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        if predicate():
            return
        time.sleep(.08)
    raise AssertionError('Timed out waiting for expected state')


class Integration(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory(prefix='portman test ')
        self.root = Path(self.temp.name)
        self.children = []

    def tearDown(self):
        for p in reversed(self.children):
            if p.poll() is None:
                self.stop(p)
        self.temp.cleanup()

    def call(self, *args, ok=True):
        p = subprocess.run([BIN, *map(str, args)], cwd=self.root, capture_output=True, text=True, timeout=10)
        self.assertEqual(p.returncode == 0, ok, (args, p.stdout, p.stderr))
        return p

    def start(self, *args):
        p = subprocess.Popen([BIN, *map(str, args)], cwd=self.root, stdout=subprocess.DEVNULL,
                             stderr=subprocess.DEVNULL,
                             creationflags=subprocess.CREATE_NEW_PROCESS_GROUP if WIN else 0)
        self.children.append(p)
        return p

    def stop(self, p):
        if p.poll() is None:
            p.send_signal(signal.CTRL_BREAK_EVENT if WIN else signal.SIGINT)
            try:
                p.wait(timeout=8)
            except subprocess.TimeoutExpired:
                p.kill()
                p.wait(timeout=3)
                raise

    def config(self, ports, extra=''):
        # shell invocation deliberately covers Python executable paths with spaces.
        lines = ['[project]', 'name="test"']
        for i, port in enumerate(ports):
            command = f'"{sys.executable}" -u -m http.server {port} --bind 127.0.0.1'
            lines += ['[[services]]', f'name="web{i}"', f'command={json.dumps(command)}', f'port={port}']
        (self.root / 'dev.toml').write_text('\n'.join(lines) + '\n' + extra, encoding='utf-8')

    def test_cli_validation_and_non_overwriting_init(self):
        self.call('--version')
        self.call('init')
        self.call('init', ok=False)
        self.call('check')
        for args in [('inspect','0'), ('inspect','65536'), ('watch','--interval','0'),
                     ('unknown',), ('list','--typo'), ('kill','3000','--yes')]:
            self.call(*args, ok=False)
        (self.root / 'dev.toml').write_text('[[services]]\nname="bad"\ncommand="x"\nrestart=true')
        self.assertIn('Unknown service field', self.call('check', ok=False).stderr)

    def test_reserve_inspect_conflict_release(self):
        port = free_port()
        holder = self.start('reserve', port)
        until(lambda: listening(port))
        rows = json.loads(self.call('inspect', port, '--json').stdout)
        self.assertTrue(any(r['pid'] == holder.pid and r['port'] == port for r in rows), rows)
        self.call('reserve', port, ok=False)
        self.stop(holder)
        until(lambda: not listening(port))

    def test_ipv6_listener_identity_and_address(self):
        try:
            s = socket.socket(socket.AF_INET6, socket.SOCK_STREAM)
            s.bind(('::1', 0))
            s.listen(1)
        except OSError as exc:
            self.skipTest(f'IPv6 loopback unavailable: {exc}')
        with s:
            port = s.getsockname()[1]
            rows = json.loads(self.call('inspect', port, '--json').stdout)
            self.assertTrue(any(r['protocol'] == 'tcp6' and r['address'] == '::1'
                                and r['pid'] == os.getpid() for r in rows), rows)

    def test_kill_requires_matching_pid_and_confirms_release(self):
        port = free_port()
        holder = self.start('reserve', port)
        until(lambda: listening(port))
        flags = ['--force'] if WIN else []
        self.call('kill', port, '--yes', '--pid', os.getpid(), *flags, ok=False)
        self.assertIsNone(holder.poll())
        self.call('free', port, '--yes', '--pid', holder.pid, *flags)
        holder.wait(timeout=3)
        self.assertFalse(listening(port))

    def test_supervisor_two_services_lock_logs_and_cleanup(self):
        ports = [free_port(), free_port()]
        self.config(ports)
        self.call('check')
        manager = self.start('up')
        until(lambda: all(listening(p) for p in ports))
        self.assertIn('ProjectAlreadyRunning', self.call('up', ok=False).stderr)
        self.assertIn('Serving HTTP', self.call('logs', 'web0', '--tail', '10').stdout)
        self.stop(manager)
        until(lambda: all(not listening(p) for p in ports))
        # Persistent lock file must not block the next valid launch.
        manager = self.start('run')
        until(lambda: all(listening(p) for p in ports))
        self.stop(manager)
        until(lambda: all(not listening(p) for p in ports))

    def test_port_conflict_does_not_start_any_service(self):
        occupied, other = free_port(), free_port()
        holder = self.start('reserve', occupied)
        until(lambda: listening(occupied))
        self.config([other, occupied])
        self.assertIn('PortConflict', self.call('up', ok=False).stderr)
        self.assertFalse(listening(other))
        self.assertIsNone(holder.poll())

    def test_child_failure_cleans_other_tree(self):
        port = free_port()
        script = self.root / 'fail.py'
        script.write_text('import time\ntime.sleep(1)\nraise SystemExit(7)\n')
        command = f'"{sys.executable}" "{script}"'
        self.config([port], '[[services]]\nname="failure"\ncommand=' + json.dumps(command) + '\n')
        manager = self.start('up')
        until(lambda: listening(port))
        self.assertNotEqual(manager.wait(timeout=8), 0)
        until(lambda: not listening(port))

    def test_config_relative_cwd(self):
        directory = self.root / 'nested space'
        directory.mkdir()
        port = free_port()
        self.config([port])
        (self.root / 'dev.toml').rename(directory / 'custom.toml')
        manager = self.start('up', str(directory / 'custom.toml'))
        until(lambda: listening(port))
        self.assertTrue((directory / '.portman/custom.toml/logs/web0.log').exists())
        self.stop(manager)
        until(lambda: not listening(port))


if __name__ == '__main__':
    unittest.main(verbosity=2)
