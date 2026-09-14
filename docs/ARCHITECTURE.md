# Architecture

Portman is split into a small platform layer, a Zig service engine, and a native Windows frontend.

```mermaid
flowchart TD
    GUI[Win32 GUI] --> ABI[desktop.h ABI]
    ABI --> ENGINE[desktop.zig service engine]
    ENGINE --> CONFIG[Strict dev.toml parser]
    ENGINE --> PLATFORM[platform.c]
    PLATFORM --> TCP[TCP listener scan]
    PLATFORM --> TREE[Job Object / process group]
    ENGINE --> LOGS[Per-service logs]
    CLI[CLI main.zig] --> PLATFORM
    CLI --> CONFIG
```

## Responsibilities

- `src/desktop.c` owns Win32 windows, controls, drawing, tray behavior, folder/file dialogs, startup preference, and user confirmations.
- `src/desktop.zig` owns UTF-8 service data, atomic persistence, import/export, port conflict checks, process lifecycle state, log tailing, and testable C ABI functions.
- `src/config.zig` parses a deliberately strict TOML subset. Unknown keys, duplicate fields, duplicate names/ports, unsafe names, invalid ports, and malformed strings are rejected.
- `src/platform.c` is the operating-system adapter. Windows uses TCP tables, process identity checks, and Job Objects. Linux uses `/proc`, pidfds, process groups, and signals.
- `windows/setup.c` and `windows/setup.zig` build a self-contained per-user installer. It writes binaries through temporary files, registers an uninstaller, creates shortcuts, and keeps runtime data separate.

## Runtime data

The installed GUI stores configuration and logs under `%LOCALAPPDATA%\Portman`. The repository never reads this directory during build. A service command is started in its configured working directory using `cmd.exe /d /s /c` on Windows or `/bin/sh -c` on Linux.

## State model

```mermaid
stateDiagram-v2
    [*] --> Stopped
    Stopped --> Starting: Start
    Starting --> Listening: expected port owned
    Starting --> Running: no port configured
    Starting --> CheckPort: timeout / foreign owner
    Listening --> Stopped: Stop / exit 0
    Running --> Stopped: Stop / exit 0
    Listening --> Failed: nonzero exit
    Running --> Failed: nonzero exit
    CheckPort --> Failed: nonzero exit
```

`Listening` verifies a TCP listener and process-tree ownership. It does not perform an HTTP health check. The Windows Stop action closes the Job Object, which terminates the managed process tree.
