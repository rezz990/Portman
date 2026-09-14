# Screenshot checklist for the GitHub README

The README has optional image slots. Screenshots make the project easy to understand, but they should show a clean demo environment. Use Windows Snipping Tool or `Win + Shift + S`, crop to the application window, and save PNG files under `docs/images/`. Do not show your username, private project paths, IP addresses, tokens, customer data, or unrelated browser tabs.

## Required screenshots

### 1. Main control panel — `01-main-panel.png`

Open Portman with at least three services configured. Show the charcoal panel, service table, status values, ports, PIDs, uptime, Start/Stop controls, and output panel. Use harmless demo commands and names such as `demo-web`, `api-preview`, and `worker`.

### 2. Add/edit service — `02-service-editor.png`

Open the service editor. Show the template selector, project folder, command, expected port, Save service, and Cancel controls. Use a fake or generic folder path.

### 3. Listening state and output — `03-listening-log.png`

Run a local HTTP service. Show `Listening`, the PID, uptime, expected port, a visible output log, and the `Open browser` action if the panel fits.

### 4. Port inspector — `04-port-inspector.png`

Open Ports. Show the search box, TCP listener table, port, PID, process, address, and executable columns. Search for a harmless process or port so the filter behavior is visible.

### 5. Import/export — `05-import-export.png`

Show either the Import dev.toml file picker or the Export config confirmation. The selected path must be generic. A second cropped image of `examples/windows-dev.toml` is useful.

### 6. System tray — `06-system-tray.png`

Show Portman minimized to the notification area and its right-click menu with Open Portman and Exit. Windows tray icons can be captured with the overflow panel open.

### 7. Installer — `07-installer.png`

Show the redesigned setup window with the charcoal hero header, `BUILT WITH ♥` / `by rakarmp (rezz990)` branding, Windows x64 badge, feature cards, per-user install path, desktop shortcut option, launch-after-install option, and the **Show installation details** control. A second capture while installing can show the green progress bar and staged status text. Do not show a real username if the path contains one.

### 8. Start menu / uninstall — `08-start-menu.png`

Show Portman in the Start menu or Windows Settings > Apps. This establishes the install flow and uninstaller entry.

## Optional screenshots

- `09-conflict-warning.png`: occupied-port message naming the detected PID/process.
- `10-failed-service.png`: Failed status and a redacted nonzero exit log.
- `11-scaled-ui.png`: the panel at 125% or 150% scaling.
- `12-portable.png`: the portable folder with `Portman.exe` and `portman-cli.exe`.

## Add images to the README

After placing files in `docs/images/`, uncomment the image lines in the README. Keep the first three images near the top; move the rest below the feature explanation if the page becomes too long.
