# Contributing to Portman

Thanks for helping improve Portman. Keep changes focused, explain the user workflow they improve, and preserve the separation between the Zig engine and the Win32 frontend.

## Before opening an issue

- Search existing issues and read `README.md`, `docs/TROUBLESHOOTING.md`, and `TESTING.md`.
- Remove passwords, tokens, customer information, usernames, private paths, and full unredacted logs.
- For a bug, include the exact Portman version, Windows/Linux version, architecture, display scaling, command, expected port, and reproduction steps.

## Local setup

Use Zig 0.14.1:

```bash
python -m pip install ziglang==0.14.1
python -m ziglang fmt --check build.zig src/main.zig src/config.zig src/desktop.zig windows/setup.zig
python -m ziglang build test
python -m ziglang build test-desktop
python tests/integration.py zig-out/bin/portman
```

For Windows output:

```powershell
python scripts/build_windows.py
```

## Change guidelines

- Keep configuration parsing strict. Do not silently ignore unknown fields or recover by overwriting user data.
- Keep service commands foreground and preserve process identity checks.
- Treat Windows process termination as a destructive action: keep confirmations and document force behavior.
- Avoid adding a runtime dependency to the GUI unless it materially improves the product.
- Do not commit generated binaries, caches, local `services.toml`, `.portman`, `.pdb` files, or private screenshots.
- Update `CHANGELOG.md`, `ROADMAP.md`, or documentation when behavior changes.
- Add a meaningful test when changing persistence, import/export, lifecycle, port ownership, or parser behavior.

## Pull requests

Use the PR template. Include the test commands and output, Windows manual-test notes for GUI/installer changes, and screenshots for visible changes. A PR should explain known limitations instead of implying native Windows behavior was tested when it was only cross-compiled.
