# Portman 0.2.6 — upgrade review

## Maintainer feedback

The maintainer reported the overall 0.2.5 app was good. Upgrading an older
installation failed with a generic “file in use” warning; a fresh install worked.
The screenshot did not contain a Windows error code, so the original cause is
not established. This update improves handling and evidence; it does not claim
the original file lock or permission issue has already been reproduced.

## Changes to verify

1. Leave 0.2.5 installed. Exit it and run `Portman-Setup-0.2.6.exe`.
2. Confirm **Update / repair**, installed version, and package version 0.2.6.
3. Update directly, click Finish, and confirm saved services and logs remain.
4. Run the installer again to repair the same version; this should also succeed.
5. If blocked, capture the exact operation, filename and Windows error. Close
   the relevant process or resolve permissions, then Retry in the same dialog.
6. Open Setup log. Confirm the error is recorded and do not uninstall merely to
   hide the failure. Redact personal paths before sharing the log.

## Automated evidence and limits

- 260 transaction policy scenarios pass locally on Linux: fresh, partial and
  existing installs, preparation/backup/publication failures, rollback failure.
- Fresh 0.2.6 validation: 10 engine/parser, 8 Linux CLI integration and 8
  release-tooling tests passed. Version consistency and whitespace checks passed.
- Six native Windows adapter scenarios are provided and cross-compiled here;
  they have NOT been executed in this Linux environment. Windows CI runs them.
- Graphical Retry, update/repair, registration failures and same-version upgrade
  still need native Windows acceptance. Public stable status is not asserted.

## Recovery boundaries

File rollback is best-effort. Backups remain if a file cannot be restored.
Registry failure after file installation retains new files and previous backups
and offers registration Retry. Sudden power loss/forced termination recovery is
not automatic. See [UPDATING.md](docs/UPDATING.md).

The package is based on GitHub `d2d6823` plus the supplied uncommitted changes.
Nothing has been pushed, tagged, or published. Build from a reviewed clean commit
before publishing. See [release acceptance](docs/RELEASE-ACCEPTANCE.md).
