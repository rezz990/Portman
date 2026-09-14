# Releasing

## Version checklist

1. Update the user-facing version in `src/main.zig`, `src/desktop.c`, `windows/setup.c`, `windows/QUICKSTART.txt`, `windows/app.manifest`, `windows/app.rc`, and `build.zig`.
2. Update `CHANGELOG.md` and the version in `README.md`.
3. Run formatting, unit/engine tests, CLI integration tests, and the Windows cross-build.
4. Run the native Windows acceptance checklist in `TESTING.md`.
5. Review the archive: no caches, `.pdb` files, runtime data, private paths, or stale payload.
6. Generate SHA-256 checksums and attach the installer plus source archive to a GitHub Release.

## Local commands

```bash
python -m ziglang fmt --check build.zig src/main.zig src/config.zig src/desktop.zig windows/setup.zig
python -m ziglang build test
python -m ziglang build test-desktop
python tests/integration.py zig-out/bin/portman
python scripts/build_windows.py --zig /path/to/zig
```

The Windows build script requires Zig 0.14.1 and produces `dist/windows-release/bin/Portman.exe`, `portman-cli.exe`, and the per-user installer. Keep generated binaries out of normal source commits; publish them as release assets.

## GitHub release outline

Create a tag such as `v0.2.2`, wait for the Windows CI job, and attach the tested installer, checksums, and source archive. Mark previews as pre-release until native Windows acceptance is complete. The release notes should list added behavior, known limitations, test evidence, and upgrade/uninstall notes.
