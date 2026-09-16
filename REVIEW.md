# Portman 0.2.4 review

This update prepares consistent, verifiable release packages while retaining the 0.2.3 UI and service behavior.

## Evidence already available

- The maintainer installed 0.2.3 and reported satisfaction with its UI.
- 0.2.3: 9 engine/parser tests passed locally; Windows binaries cross-compiled.
- 0.2.4: seven release-tooling regression tests cover stale versions, invalid Windows version values, preserved addresses/dependency versions, tampered assets, missing checksums, generated source data and stale assets.
- The 0.2.4 build and fresh engine/CLI results are recorded in the delivery note.
- New GitHub Actions definitions have not been run remotely as part of this change.

## Your Windows review

Record Windows version, display scaling and the exact asset filename. Leave items unchecked until tested.

- [ ] Install over the stopped 0.2.3 installation; service configuration and logs remain.
- [ ] Window title, About, CLI `--version` and Windows Apps show 0.2.4.
- [ ] Start a service with its correct expected port; confirm Listening and browser action.
- [ ] Stop and restart it; confirm its children and listener are cleaned up.
- [ ] Occupy the port with another process; confirm Start refuses without terminating it.
- [ ] Run `exit 7`; confirm Failed and useful output/exit information.
- [ ] Pause/resume output and copy text while paused.
- [ ] Test installer shortcut/launch options with mouse and Space.
- [ ] Test tray restore/exit and startup preference.
- [ ] Import/export and reopen the application; verify persistence.
- [ ] Test default, minimum and maximized sizes and the scaling you use.
- [ ] Run the portable archive; verify it uses the same per-user configuration.
- [ ] Uninstall; app entries disappear while project/config/log files remain.

## Reporting a problem

Send the unchecked/failed item, expected behavior, actual behavior and a screenshot or log excerpt. Confirm the new executable was launched: an already-running 0.2.3 tray instance can make testing the wrong version confusing.

## Remaining work before stable

Windows acceptance, responsive installer execution, graceful-stop policy for database use, richer per-monitor DPI handling, and consistent OS-controlled scrollbars remain outside this release-preparation patch. No claim of full Windows acceptance is made by automated packaging tests.
