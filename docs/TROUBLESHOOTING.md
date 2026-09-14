# Troubleshooting

## The app opens but the service stays in `Starting...`

Check that the command is a foreground command, the working folder exists, the runtime is installed, and the command actually binds the expected port. Read **Open log**. A process that daemonizes itself or uses `start` can leave the process tree Portman cannot observe.

## Status is `Check port`

The process is alive but the expected TCP listener has not been confirmed. Common causes are a wrong port, a server binding only after a long build, binding to another address, a different process owning the port, or a failed port scan. Use the **Ports** inspector and verify the command's own configuration.

## Portman refuses to start

Portman checks the expected port before launch. Stop the other local service or choose a different expected port. This check is not a reservation; another program can still win a race immediately after the check.

## `Open browser` does nothing useful

The action opens `http://127.0.0.1:<port>`. It is intended for HTTP services. It does not infer HTTPS, custom hostnames, databases, or non-HTTP protocols.

## Runtime command is not found

Install Node/Bun/PHP/Python/etc. separately and ensure it works from a normal PowerShell window. Restart Portman after changing PATH. The Portman installer does not install runtimes.

## Stop does not gracefully close a database

Windows Stop terminates the managed Job Object process tree. Use the database's own shutdown command before Stop when data integrity matters. Graceful-stop hooks are planned for a future version.

## Installer is blocked by Windows

The preview installer is unsigned. Verify that the file came from the project release, compare the published checksum, and do not disable Windows security features. Native code-signing is planned before a production release.

## Configuration load fails

Portman reports the line and parser error and does not replace the file with an empty configuration. Open `%LOCALAPPDATA%\Portman\services.toml`, fix the reported field, and restart. Make a backup before manual edits.

## Collecting a useful bug report

Include Portman version, Windows version and architecture, display scaling, exact service command with secrets removed, expected port, project type, reproduction steps, screenshot, and the last relevant log lines. Do not attach full logs containing credentials or private paths.
