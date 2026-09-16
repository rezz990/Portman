# Release workflow

Portman remains a Windows preview. A successful build is not a Windows GUI acceptance report.

## Version and build

1. Edit the root `VERSION` file (numeric major.minor.patch).
2. Run `python scripts/version.py` to synchronize checked-in application labels and Windows resources.
3. Run `python scripts/version.py --check`; CI refuses stale labels.
4. Use Zig 0.14.1, then run `python scripts/build_windows.py`.

The build checks version consistency before replacing generated output. It builds fresh GUI/CLI payloads before the installer and verifies the resulting package.

## Files to distribute

Use **only the contents of `dist/release/`** for a release:

- `Portman-Setup-<version>.exe`
- `Portman-<version>-windows-x64.zip`
- `Portman-<version>-source.zip`
- `BUILD-INFO.json`
- `SHA256SUMS.txt`

`python scripts/verify_release.py dist/release` verifies exact asset membership, SHA-256 checksums, ZIP integrity and source-package exclusions. Hashes establish consistency with this manifest; they are not a code signature or independent proof of publisher identity.

`BUILD-INFO.json` records version, compiler, target, optimization, base commit and whether local changes were present. A modified working tree is explicitly marked. A source ZIP built without Git records null commit/working-tree information. No signing or bit-for-bit reproducibility claim is made.

## Owner review

Run the Windows checklist in `REVIEW.md` on the exact installer and portable archive being considered for publication. Do not label an incomplete checklist as passed.

The maintainer reported satisfaction with the 0.2.3 UI installation. That feedback does not establish that every lifecycle, scaling, upgrade or uninstall scenario has passed.

## GitHub Actions

- Pushes and pull requests run version checks, release tooling tests, engine tests and native CLI integration tests on Linux and Windows.
- Manual **Windows release candidate** runs build a downloadable workflow artifact without publishing a release.
- A version tag must exactly match `v` plus `VERSION`; mismatches fail before compilation.
- Tag runs build/test the Windows candidate and prepare a **draft pre-release** with all five assets.
- The owner reviews the draft, adds release notes and publishes it when ready.

An existing manually created release for the same tag should be reviewed before rerunning the tag workflow; avoid maintaining two competing sets of release assets.

## Stable designation

Only remove the preview designation after recording acceptance results for the supported Windows scenarios. Unsigned distribution, forceful Job Object shutdown, system-DPI behavior and synchronous installation remain documented limitations. This workflow does not implement signing, auto-update or background installation.
