# Updating Portman

## Update an installed copy

1. Download the new installer from the official release or the maintainer's supplied package.
2. Exit Portman using its tray menu. Stop any Portman CLI sessions you started.
3. Run the new installer. An existing `Portman.exe` selects **Update / repair**;
   the panel shows the registered version and the package version.
4. Click **Update / repair**, then **Finish** when successful.

Uninstalling first is not part of the normal update flow. The application folder
is `%LOCALAPPDATA%\Programs\Portman`. Service configuration and logs are stored
separately under `%LOCALAPPDATA%\Portman`; the file replacement transaction does
not modify them or your project folders. Running the same version repairs files.
The version label reports registry metadata, not a binary-integrity check.

## If an update is blocked

The dialog shows the operation, exact source/destination paths, numeric Windows
error, and Windows description. **Retry** repeats that operation after you resolve
the problem. **Cancel** stops the current file transaction and attempts to restore
its previous application files. It does not uninstall Portman.

- Error 32/33: another open handle prevents the operation. Exit the relevant app
  or CLI, then Retry. The installer does not forcibly terminate unrelated apps.
- Error 5: Windows denied access. Check permissions, the read-only attribute and
  security-software history. This error alone does not identify a running process.
- Other codes: use the Windows description and **Setup log** for the failing stage.

A permissions probe checks existing files before replacements begin. This reduces
avoidable partial updates; it cannot prevent another process acquiring a handle
later. Later failures still use Retry and rollback.

## Backups and recovery

Each attempt stages all four components in a unique `.portman-update-*` folder
inside the application directory. Existing files move into that folder as `.old`
backups before replacements are published. An old `.bak` file left by an earlier
installer is not overwritten or used as the new transaction's backup.

On a file-stage failure, earlier replacements are restored in reverse order. If
Windows also blocks restoration, the installer retains the backup and displays
its directory. Do not delete a retained recovery folder until the application is
working. This is best-effort rollback for handled failures, not a power-loss-safe
transaction or an automatic recovery guarantee after forcibly killing setup.

Registration in Windows Apps happens after the new files are in place. If registry
registration fails, Retry repeats registration. Cancel at this stage leaves the
new application files and old backups in place; **Retry installation** can finish
registration. The message distinguishes this state from a rolled-back file update.

`Setup log` opens `%LOCALAPPDATA%\Portman\setup.log`. At a later setup launch, a log
larger than 1 MiB is rotated to `setup.log.previous`. Logs include local paths;
share the error code, operation and filename, or redact personal paths first.

## Validation

`python tests/test_upgrade_transaction.py` runs 260 failure-injection scenarios
against the same file-transaction policy used by setup. These run on Linux and
Windows. `python tests/test_upgrade_windows.py` runs six real Windows file-lock,
Retry and rollback scenarios in a temporary folder without registry writes or
modifying an installed Portman. Both are included in the applicable CI jobs.

On Linux, `--build-only` only cross-compiles the Windows test executable. It does
not establish that those scenarios passed on Windows. Run the native checklist
against the installer too: these file tests do not exercise its graphical UI.

## In-app updates

This version supplies a manual installer upgrade path. An in-app update checker,
download progress, package verification, explicit service-stop confirmation and
installer handoff remain planned. No background network updater runs in this build.
