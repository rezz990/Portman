# Publishing Portman on GitHub

This guide assumes the repository will be published under `rezz990/Portman`. Replace the owner/name if you choose another repository.

## Before the first push

1. Run all commands in `README.md` and keep the output for the release notes.
2. Replace the placeholder PNG files in `docs/images/` using `docs/SCREENSHOTS.md`.
3. Review every screenshot and remove usernames, private folder paths, IP addresses, tokens, customer names, and unrelated browser tabs.
4. Replace the copyright holder in `LICENSE` if the repository will be owned by a different legal person or organization.
5. Read `SECURITY.md` and enable GitHub private vulnerability reporting if you want security advisories.

## Create and push the repository

```bash
git init
git add .
git commit -m "feat: publish Portman 0.2.2 Windows control panel"
git branch -M main
git remote add origin https://github.com/rezz990/Portman.git
git push -u origin main
```

If the repository already exists, use its existing remote and push a focused commit instead of reinitializing it. Check `git status --short` before committing; generated folders such as `.zig-cache`, `zig-out`, `dist`, `.pdb`, `windows/payload`, and local runtime data should stay ignored.

## Publish the first release

After the main branch CI is green:

```bash
git tag -a v0.2.2 -m "Portman 0.2.2 Windows GUI preview"
git push origin v0.2.2
```

The release workflow builds a fresh Windows x64 GUI, CLI, and per-user installer, computes SHA-256 checksums, and attaches release files for a tag beginning with `v`. Keep the first release marked **pre-release** until the native Windows checklist in `TESTING.md` is complete.

## GitHub repository settings

- Set the repository description to: `A lightweight Windows local development control panel built with Zig.`
- Add topics: `zig`, `windows`, `developer-tools`, `local-development`, `process-manager`, `port-inspector`, `win32`.
- Enable Issues and Discussions only if you intend to monitor them.
- Protect `main` after CI is stable; require the CI check for pull requests.
- Keep Actions permissions limited to the workflow needs. The release workflow needs contents write only when a tag creates a Release.
- Add a repository social preview image using the main control-panel screenshot or a clean project banner.

## README screenshot order

Keep `01-main-panel.png`, `03-listening-log.png`, and `04-port-inspector.png` near the top of the README. The remaining images can stay in `docs/images/` and be linked from the screenshot guide or a future documentation page.

## Release notes template

```markdown
## Portman 0.2.2

Portman is a lightweight Windows local development control panel built with Zig.

### Added
- Export service configuration.
- Searchable TCP Port Inspector.
- Native Win32 panel, tray, installer, service logs, and process-tree cleanup.

### Downloads
- `Portman-Setup-0.2.2.exe` — per-user installer.
- `Portman.exe` — portable GUI.
- `portman-cli.exe` — optional CLI.
- `SHA256SUMS.txt` — checksums.

### Validation
- 9 parser/engine tests passed.
- 8 CLI integration tests passed.
- Windows x64 cross-build passed.
- Native Windows acceptance: describe the exact machine and checklist result here.

### Known limitations
- Preview installer is unsigned.
- Windows Stop is forceful for the managed Job Object tree.
- No automatic restart, dependency readiness, or database-specific graceful-stop hook.
```
