# Windows interface update — 0.2.3 preview

Based on GitHub main commit a7fef0aed3f4c6ccd1317f6a682ce7f55d48252e.
The existing sidebar, packaging script, screenshots and workflow updates are retained.

## What changed

- Dark custom-painted table headings and rows in Services and Port Inspector.
- Taller rows, alternating backgrounds, visible selected rows and status colors.
- Rounded buttons retain native keyboard handling and focus indication.
- Service actions wrap onto another row at smaller window widths.
- Configuration actions and process actions are separated; the list has a bounded height so a maximized window does not become mostly empty table space.
- First service is selected automatically; actions update immediately on selection.
- Pausable, word-wrapped output; Pause freezes the displayed tail, not log capture.
- Dark rendering for template choices and native checkboxes.
- Dark title bars requested through DWM (depends on Windows support).
- Installer checkbox semantics corrected: automatic checkboxes no longer combine incompatible owner-draw button styles.
- Installer progress control initialized explicitly, progression remains monotonic, artificial delays removed.
- Installer details button shortened and moved so its label fits.
- Installer fonts and layout scale using the system DPI.

## Validation and boundaries

9 engine/parser tests passed locally on Linux. Windows compilation is recorded separately in the delivery message.
No interactive Windows desktop or Wine was available during this change. Screenshots in docs/images show the earlier app and are not evidence for the revised UI.
Native scrollbar and combo arrow styling remains controlled by Windows; this patch does not claim a fully custom Fluent frontend.
The app declares System DPI awareness deliberately: it scales at launch. Moving between monitors with different scaling may cause Windows bitmap scaling. Per-monitor live relayout is not implemented.
Installer file operations remain synchronous; the UI may briefly stop accepting input during installation. No cancellable background-install worker is claimed.

## Windows acceptance

1. Open the new binary and verify the title identifies 0.2.3.
2. Compare Services at default size, minimum size and maximized. Confirm no overlapping toolbar actions.
3. Check table headings, selected rows, Running/Listening/Failed colors and keyboard focus.
4. Select a stopped service: Start must enable immediately. Start/Stop it and confirm lifecycle is unchanged.
5. Pause output, select and copy text, wait for new output, then Resume. Captured output should remain in the full log.
6. Open the service editor and template dropdown; navigate with Tab and arrow keys.
7. Test both installer options with mouse and Space, independently enabled and disabled. Verify shortcut creation and automatic launch follow the choices.
8. Repeat at 100%, 125%, 150% and 200% system scaling after restarting the app. The minimum window is 960 x 780 logical pixels; use an appropriate desktop work area.
9. Inspect Ports with enough listeners to scroll; Windows-native scrollbar appearance is a known remaining limitation.
10. Verify Settings, About, tray, import/export, reinstall and uninstall preserve their existing behavior.

## Screenshots to replace after acceptance

Capture the main panel with stopped/running services, the selected service log, template dropdown, Port Inspector, Settings and installer with one option disabled. Preserve the existing screenshots until real captures of this version are available.

## Applying the source update

Use the full source archive or copy the files listed in the delivery message into your current repository. Add src/ui/theme.h as a new file. Build with Zig 0.14.1 using python scripts/build_windows.py. Nothing here publishes to GitHub.
