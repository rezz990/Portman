## What does this change do?

<!-- Describe the user-visible result and the reason for the change. -->

## Scope

- [ ] GUI
- [ ] CLI
- [ ] service lifecycle
- [ ] configuration/import/export
- [ ] Windows installer
- [ ] documentation
- [ ] tests

## Validation

- [ ] `python -m ziglang fmt --check build.zig src/main.zig src/config.zig src/desktop.zig windows/setup.zig`
- [ ] `python -m ziglang build test`
- [ ] `python -m ziglang build test-desktop`
- [ ] CLI integration tests
- [ ] Native Windows manual test (when the change affects Windows)

Commands/output:

```text
Paste relevant output here.
```

## Screenshots or recordings

<!-- Required for visible GUI changes. Remove this section only when not applicable. -->

## Compatibility and safety

- [ ] No secrets, personal paths, generated binaries, caches, or runtime data are committed.
- [ ] Process termination behavior and user confirmations were reviewed.
- [ ] README/changelog/roadmap were updated when needed.

## Checklist

- [ ] This PR is focused and reviewable.
- [ ] I did not change `services.toml` or `.portman` runtime data in the repository.
- [ ] I have explained known limitations.
