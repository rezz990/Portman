# Publishing the existing Portman repository

The repository already exists at `rezz990/Portman`. Keep its history and remote.

1. Review the diff and run the commands in `TESTING.md`.
2. Complete `docs/RELEASE-ACCEPTANCE.md` on the exact candidate binaries.
3. Refresh the screenshots described in `docs/SCREENSHOTS.md`.
4. Commit the reviewed source and documentation, then push using your normal workflow.
5. Run the Windows release candidate workflow manually and review its artifacts.
6. Follow `RELEASING.md` to tag the matching VERSION only when ready.

Tag builds create a **draft pre-release**, not a public stable release. Inspect its
assets, checksums, build provenance and release notes before publishing. A manual
workflow run only uploads an Actions artifact. Neither a green compiler build nor
an attractive screenshot replaces native Windows acceptance.

Do not upload a stale installer from another version. Use the five verified files
from `dist/release/`. For a stable release, rebuild from the committed, reviewed
source so `BUILD-INFO.json` identifies a clean commit.
