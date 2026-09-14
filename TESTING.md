# Testing Portman 0.2.2

Portman has automated tests and a native Windows acceptance pass. Automated tests prove the parser and service engine without needing to draw a GUI. The Windows checklist covers the parts that require a real Windows desktop: installer behavior, tray integration, display scaling, file dialogs, shortcuts, and Windows process semantics.

## Automated commands

Use Zig 0.14.1.

```bash
python -m ziglang fmt --check build.zig src/main.zig src/config.zig src/desktop.zig windows/setup.zig
python -m ziglang build test
python -m ziglang build test-desktop
python tests/integration.py zig-out/bin/portman
```

The current local validation result for 0.2.2 is **9 engine/parser tests passed and 8 CLI integration tests passed**. The Windows x64 GUI, CLI, and installer also cross-compile successfully. That compilation result does not replace the native Windows checklist below.

## Native Windows checklist

1. Install as a normal user. Confirm there is no administrator prompt, the Start menu entry is created, and the optional desktop shortcut works.
2. Start a harmless worker: command `echo Portman-jalan & ping -t 127.0.0.1 >nul`, no expected port. Confirm `Running`, PID, uptime, output, and cleanup after Stop.
3. Start an HTTP service and set its real expected port. Confirm `Starting...` becomes `Listening`, the browser shortcut opens, output updates, and Stop releases the listener.
4. Occupy an expected port with another process. Confirm Start refuses the service, identifies the foreign owner, and does not terminate it.
5. Use command `exit 7`. Confirm the service becomes `Failed` and displays exit code 7.
6. While running, verify Edit, Remove, and Clear log are disabled or refused. Stop, edit, close, reopen, and verify persistence.
7. Use Unicode and spaces in a project folder. Verify commands run in that exact folder.
8. Import `examples/windows-dev.toml` after adapting its paths. Import it twice and confirm the second import is atomic and refuses duplicates.
9. Export a configuration and verify it contains service definitions only. Review paths and commands before sharing it.
10. Open Ports. Search by port, PID, process name, address, and executable path. Confirm the table is read-only.
11. Click To tray. Restore with a double-click, open the context menu, and verify Open Portman and Exit behavior.
12. Enable and disable Open Portman at sign-in. Confirm it changes only the per-user startup entry and does not auto-start services.
13. Test 100%, 125%, 150%, and 200% Windows scaling. Resize the panel to its minimum size and use Tab, Enter, Escape, and Space navigation.
14. Reinstall over a stopped existing installation. Confirm configuration/logs remain and stale versioned installers are not left in the output.
15. Uninstall from Windows Settings or `Uninstall.exe`. Confirm app files, shortcuts, and startup entry are removed while project folders and `%LOCALAPPDATA%\\Portman` data remain.
16. Test sign-out/restart with an active service and verify the documented cleanup behavior.
17. Use the Services, Ports, Settings, and About navigation with mouse and keyboard. Confirm the active item, focus rectangle, empty-service guidance, branding, and disabled actions remain clear.
18. Run `python scripts/build_windows.py`; extract both generated ZIP files and verify the portable archive launches while the source archive contains no `.git`, cache, payload, or previous `dist` directory.

## Bug report evidence

Include the Portman version, Windows edition/build, x64 architecture, display scaling, project type, exact command with secrets removed, expected port, reproduction steps, screenshot, and relevant redacted log lines. Do not attach passwords, tokens, customer data, private IPs, or a full unredacted Local AppData directory.

## Known test boundary

The preview installer is unsigned. Protected/system processes, service brokers, commands that daemonize or use `start`, database-specific graceful shutdown, and code signing require separate validation and are not claimed by the automated tests.
