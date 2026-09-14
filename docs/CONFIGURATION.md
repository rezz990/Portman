# Configuration

Portman saves the services created in the GUI as `%LOCALAPPDATA%\Portman\services.toml`. You can also import a compatible file. The parser intentionally supports a small, predictable subset of TOML.

```toml
[project]
name = "my-workspace"

[[services]]
name = "web"
command = "npm run dev"
cwd = "C:/Projects/my-web"
port = 3000

[[services]]
name = "api"
command = "php artisan serve --host=127.0.0.1 --port=8000"
cwd = "C:/Projects/my-api"
port = 8000
```

## Supported fields

| Field | Required | Rules |
|---|---:|---|
| `[project].name` | no | 1–64 letters, digits, `-`, `_`; default `project` |
| `[[services]].name` | yes | unique case-insensitively; same name becomes the log filename |
| `command` | yes | non-empty one-line string executed by the platform shell |
| `cwd` | no | non-empty path; relative paths are resolved against the imported config file |
| `port` | no | integer 1–65535; omit for worker/background commands |

Unknown fields and duplicate fields are errors. Service names `CON`, `PRN`, `AUX`, `NUL`, `COM1`–`COM9`, and `LPT1`–`LPT9` are rejected because they are Windows device names.

## Sharing a configuration

Use **Export config** to create a copy. The export contains service names, commands, folders, and ports. It does not contain logs, PIDs, runtime state, environment variables, secrets, or project files. Review absolute paths and commands before sharing.

## Safe examples

Use environment variables for credentials. Avoid putting passwords, tokens, database URLs, or customer data in `command`, because commands and stdout/stderr are written to the log.
