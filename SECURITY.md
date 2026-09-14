# Security policy

## Supported versions

| Version | Security fixes |
|---|---|
| Latest release | Yes |
| Older previews | Best effort |

## Reporting a vulnerability

Do not publish credentials, exploit details, private logs, or a working proof of concept in a public issue. Use GitHub's private security advisory for the repository when available. Include the affected version, Windows/Linux version, a minimal reproduction, impact, and a safe contact method.

Portman executes commands supplied by the user. Treat service configuration, imported `dev.toml` files, and log output as potentially sensitive. Never put passwords or tokens in commands when environment variables or the runtime's secret store can be used.
