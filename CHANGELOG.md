# Changelog

## 0.2.6 — upgrade reliability (unreleased)

- Detect existing installations and offer Update / repair with installed/package versions.
- Show the exact operation, file paths and Windows error instead of labeling every failure as a file-in-use issue.
- Add Retry / Cancel to blocked file operations and registration failures.
- Probe existing files before replacement and stage each attempt in a unique folder.
- Recover earlier replacements on handled file failures; retain backups when recovery is blocked.
- Add Setup log, preserving the Windows error before handles are closed.
- Add 260 portable transaction scenarios and six native Windows file-lock scenarios to CI.
- In-app update discovery/download remains planned; this release improves manual upgrades.


## 0.2.5 — review candidate (unreleased)

- Launch failures retain a per-service explanation even after other API errors. Missing project folders are detected before launching.
- App startup and install/uninstall file replacement share a maintenance lock.
- Template dropdown confirmation happens after selection is committed; Escape leaves the service unchanged.

- Installer uses a worker thread, keeps progress responsive and waits for Finish after success.
- Main window initial size respects the desktop work area; minimum height is 640 logical pixels.
- Ports now opens inside the main workspace with consistent active navigation.
- Start all continues past individual launch failures and reports each failure.
- Start all / Stop all reflect the current service availability.
- Failed services display a nonzero exit code or a launch/inspection explanation.
- Applying a template asks before replacing existing commands; Custom preserves fields.
- Editor Enter respects Cancel and Browse; open template dropdowns keep their keyboard handling.
- Corrected portable paths, export privacy wording, outdated publishing instructions and screenshot guidance.
- Added explicit release acceptance gates. Native Windows acceptance remains pending.


## 0.2.4 — Release preparation (preview)

- Added VERSION with a synchronization command and stale-label CI gate.
- Unified installer, portable/source ZIPs, build provenance and checksums in dist/release.
- Added package integrity verification and seven tooling regression tests.
- Windows release workflow now tests the CLI and prepares a draft pre-release with all assets.
- Added exact-tag validation and a maintainer Windows review checklist.
- Retains the 0.2.3 UI and service behavior; full Windows acceptance remains pending.


## 0.2.3 — Windows interface polish (preview)

- Consistent dark table headers and rows, colored statuses, taller rows, and rounded actions.
- Wrapped action toolbar, bounded service-list height, immediate selection updates, and automatic initial selection.
- Pausable, word-wrapped output and dark template/checkbox rendering.
- Correct native installer checkbox behavior, initialized progress control, monotonic progress, and readable details action.
- System-DPI-scaled installer and supported dark title bars.
- Native Windows visual acceptance remains pending; see docs/UI-0.2.3.md.


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
