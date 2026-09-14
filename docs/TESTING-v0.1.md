# Verification — Portman 0.1.0

Toolchain: Zig 0.14.1. Local runtime: Linux x86_64 container.

| Check | Result |
|---|---|
| `zig fmt --check` | Pass |
| Config unit tests | 2/2 pass, including 10 invalid-config fixtures |
| Linux debug build | Pass |
| Linux x86_64 musl ReleaseSafe build | Pass, statically linked |
| Windows x86_64 GNU ReleaseSafe cross-compile | Pass |
| CLI errors, strict config, init refuses overwrite | Pass |
| Reserve, inspect owner, bind conflict, release | Pass |
| Wrong-PID kill refused; matching PID termination and release | Pass |
| Two live services, exclusive supervisor lock, logs, Ctrl+C cleanup and relaunch | Pass |
| Conflict on later service prevents all startup | Pass |
| Failed service exit cleans other service tree | Pass |
| Custom config path and cwd with spaces | Pass |
| IPv6 loopback address and process identity | Pass |

The seven primary integration cases passed against both the Linux debug and
release binaries. The additional IPv6 case passed against the release binary.
PID mapping was also exercised with an ancestor-namespace `/proc` mount.

Windows execution was **not** tested locally. A Windows GitHub Actions job is
provided, but has not been run/published by this task. Cross-compilation does not
verify console signals, Job Object behavior, permissions, or filesystem locking
at runtime. On Windows, first run `list`, `inspect`, then the integration suite
before relying on project supervision.

Not covered: elevated/system processes, large production workloads, macOS,
fatal supervisor crashes, detached/daemonized descendants, or load benchmarks.
No speed/memory claims are inferred from these correctness tests.
