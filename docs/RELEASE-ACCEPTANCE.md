# Release acceptance

The release is ready when the following evidence is attached to the exact build.
A blank result means pending, not passed. Use Windows 10/11 x64 and record the
OS build, screen resolution, scaling, asset SHA-256, version, and tester/date.

## Blocking gates

| Area | Required evidence | Current status |
|---|---|---|
| Installation | Normal-user fresh install, 0.2.5→0.2.6 upgrade, same-version repair, Retry/Cancel with a locked file, shortcuts, launch option, uninstall, preserved data | Pending native Windows test |
| Installer responsiveness | Repaint/move/close behavior under slow disk or antivirus; no unexplained hang | Worker implementation added; native Windows test pending |
| Process ownership | Start, stop, restart, child cleanup, foreign-port protection | Engine coverage exists; Windows acceptance pending |
| Navigation | All four sidebar pages, keyboard focus, Ports filtering and return to Services | Pending native Windows test |
| Editor | Tab/Space/Enter/Escape, dropdown, Browse, Cancel, template replacement, invalid paths | Pending native Windows test |
| Layout | Minimum/maximized size, 100/125/150/200% scaling, visible actions | Pending; current minimum 960 × 640 logical pixels may exceed small work areas |
| Diagnostics | `exit 7`, invalid runtime, occupied port, mixed-success Start all | Pending native Windows test |
| Packaging | Fresh build, archive checks, version match, clean-commit provenance | Local verification plus clean CI build required |
| Documentation | Current screenshots, working download links, accurate known limits | Screenshots and public release links pending |

## Focused Windows scenarios

1. Add a foreground worker using `echo ready & ping -t 127.0.0.1 >nul` and no port.
2. Add a service using `exit 7`. Start it; expect Failed and exit code 7.
3. Add an HTTP server with its actual port. Confirm Listening and browser behavior.
4. Occupy one configured port with a separate process, then Start all. Other
   stopped services should still launch; the result lists the conflict. Nothing
   should terminate the foreign owner. Launched is not the same as ready.
5. Edit a stopped service. Select Custom: fields stay intact. Apply another
   template: Cancel preserves the command; OK replaces command and port only.
   Test Enter while Cancel/Browse is focused and while the dropdown is open.
6. Switch to Ports, filter by the HTTP port, Refresh, visit Settings and return.
   Confirm exactly one active sidebar item and no detached inspector window.
7. Pause output before selecting/copying text, resume, then stop the service.
8. While installation is busy, try opening Portman: it must ask you to wait. After Finish, confirm normal launch. Also try updating with Portman already open: setup must refuse before replacing files.
9. Move a stopped service’s folder, then Start: expect Failed and a persistent folder explanation. Restore the folder and retry.
10. Upgrade and uninstall with disposable demo data; verify persistence promises.

## Scope for the first stable release

Foreground development services, read-only TCP inspection, explicit commands,
local configuration and logs. Automatic restarts, dependency orchestration,
service brokers, production hosting and database graceful shutdown are not
implemented. Do not market these as supported features.

The installer is unsigned. System-DPI scaling is used; per-monitor DPI changes
are not handled. Native scrollbars retain OS styling. Those limitations must be
visible in the release notes even when accepted for the chosen release scope.

## Decision record

- Candidate filename and SHA-256:
- Commit / working tree state:
- Windows edition/build and display configuration:
- Tester and date:
- Passed scenarios:
- Failed scenarios and issue links:
- Accepted limitations:
- Decision: HOLD / PRE-RELEASE / STABLE

Current decision: **HOLD for stable publication**. Complete blocking work and
Windows acceptance before introducing this build as a stable community release.
