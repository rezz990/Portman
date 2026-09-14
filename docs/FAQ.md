# FAQ

## Is Portman another web server, package manager, or container runtime?

No. Portman is a local process control panel. It launches the command you choose and shows its process, output, and expected TCP port. Your framework and runtime still provide the actual web server or worker.

## Why use Zig for this project?

The core needs a small native executable with direct access to files, processes, TCP tables, and platform APIs. Zig keeps the runtime footprint small and makes the cross-platform engine easy to ship as one binary. The GUI uses C/Win32 where native Windows integration is the clearest fit.

## Does Portman install Node.js, PHP, Bun, Python, MySQL, or Android SDKs?

No. These are independent developer tools with their own installers and versions. Keeping them separate prevents Portman from changing an existing workstation unexpectedly.

## Can I use it for production services?

The preview is aimed at local development. It has no service supervisor policy, graceful database hooks, authentication, remote management, or signed installer. Keep production workloads under their normal service manager.

## Does Stop send Ctrl+C or a graceful signal?

The current Windows implementation closes the managed Job Object and force-terminates the process tree. A per-service graceful-stop hook is planned. Use the database's own shutdown procedure before stopping a stateful process.

## Can multiple Portman windows run at once?

The GUI uses a per-user single-instance mutex. A second launch focuses the existing window. The CLI remains an independent process, but it should not modify the same runtime configuration while the GUI is active.

## Can I share exported configuration files?

Yes, after reviewing them. An export contains commands, absolute folder paths, service names, and ports. It does not contain logs or project files, but commands can still contain secrets if you put them there. Prefer environment variables and redact paths before sharing.

## Why does a service show `Check port` even though the command is running?

Portman may be waiting for a slow build, the command may bind a different port/interface, another process may own the expected port, or the listener scan may not have enough permission to identify the owner. Open Ports and the service log to distinguish these cases.

## Does Portman reserve ports?

No. It checks the port before starting and verifies ownership after launch. Another process can claim the port between those checks.

## Does the portable build keep data beside the executable?

No. The portable executable uses `%LOCALAPPDATA%\\Portman`, the same data directory as the installed GUI. This keeps configuration consistent when moving from portable to installed mode.

## Why is the installer unsigned?

Code signing requires a certificate and release process. The current preview is unsigned. Verify checksums and use the official release source; code signing is on the roadmap.
