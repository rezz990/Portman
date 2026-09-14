# Roadmap

## 0.2.x

- Native Windows GUI and per-user installer
- Service persistence, import/export, port inspector, logs, tray, and startup panel
- Windows Job Object process cleanup

## Next

- Graceful stop hooks per service
- Restart policy with bounded backoff
- Dependency readiness and ordered Start all
- Live log rotation and log size policy
- Environment file/editor with redaction rules
- Better service health checks (HTTP/TCP/custom command)
- Code-signed installer and reproducible release metadata

## Later

- Windows service integration for users who explicitly want background startup
- Optional PATH integration for `portman-cli.exe`
- Profile/workspace switching
- Linux desktop frontend only after the Windows workflow remains stable
