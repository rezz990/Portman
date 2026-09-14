# Changelog

## 0.2.2 — Branded Windows installer

- Rebuilt the installer UI as a native charcoal onboarding panel with Portman branding.
- Added hero header, Windows x64 badge, feature cards, install location card, and clearer per-user information.
- Added “Built with ♥ by rakarmp (rezz990)” branding in the header and footer.
- Added interactive installation details toggle.
- Added staged progress bar and status messages while preparing files, registering Windows metadata, creating shortcuts, and finishing installation.
- Added owner-drawn shortcut options and branded action buttons.
- Added installer icon/resource metadata for the new presentation.
- Preserved existing atomic replacement, rollback, shortcut, uninstaller, and runtime-data behavior.

## 0.2.1 — Windows GUI preview

- Added **Export config** with atomic writes and explicit review message.
- Added live search to the TCP Port Inspector for port, PID, process, address, and executable path.
- Added engine tests for export round trips, failed export preservation, and port search matching.
- Added Windows device-name validation and case-insensitive service-name validation.
- Added rich GitHub documentation, screenshot checklist, architecture/configuration/troubleshooting/release guides, contribution templates, security policy, support policy, code of conduct, MIT license, and roadmap.
- Version metadata and installer labels updated to 0.2.1.

## 0.2.0 — Windows GUI preview

- Native Win32 control panel with charcoal theme, service table, status, PID, uptime, actions, output, tray, and startup-panel option.
- Add/edit/remove services and import strict `dev.toml` configurations.
- Node/Bun/PHP/Python service templates and project folder picker.
- Windows Job Object process-tree cleanup and TCP port ownership status.
- Per-user installer, Start menu/desktop shortcut, uninstaller, icon, and portable binaries.
- CLI renamed `portman-cli.exe` on Windows to avoid case-insensitive filesystem collision with `Portman.exe`.
- Parser, lifecycle, persistence, import, occupied-port, and CLI integration tests.

## 0.1.0 — CLI foundation

- TCP listener listing, inspect, watch, reserve, kill/free confirmation flow.
- Strict service configuration parser and foreground `up` supervisor.
- Per-service logs, cleanup, IPv4/IPv6 inspection, and Windows/Linux adapters.
