const std = @import("std");
const builtin = @import("builtin");
const config = @import("config.zig");
const c = @cImport({
    @cInclude("platform.h");
});
const windows = builtin.os.tag == .windows;
const out = std.io.getStdOut;
const err = std.io.getStdErr;
const help =
    \\Portman 0.2.2 - local development service manager
    \\
    \\  portman [list] [--json] [--port PORT]
    \\  portman inspect PORT [--json]
    \\  portman watch [--port PORT] [--interval SECONDS]
    \\  portman kill PORT [--pid PID] [--yes] [--force]
    \\  portman free PORT [--pid PID] [--yes] [--force]
    \\  portman reserve PORT
    \\  portman init
    \\  portman check [dev.toml]
    \\  portman up [dev.toml]
    \\  portman run [dev.toml]
    \\  portman logs SERVICE [dev.toml] [--tail LINES]
    \\  portman --version
    \\
    \\TCP listeners (IPv4/IPv6). kill/free requires confirmation; --yes also
    \\requires --pid. Windows termination additionally requires --force.
    \\up runs in the foreground; Ctrl+C stops its service trees. Logs append
    \\under .portman/<config filename>/logs/. Commands use sh/cmd, no stdin.
    \\Use only trusted configs. See README.md for platform limitations.
    \\
;
const template =
    \\[project]
    \\name = "my-project"
    \\
    \\[[services]]
    \\name = "web"
    \\command = "npm run dev"
    \\cwd = "."
    \\port = 3000
    \\
    \\# [[services]]
    \\# name = "api"
    \\# command = "php artisan serve --port=8000"
    \\# cwd = "./api"
    \\# port = 8000
    \\
;
fn same(a: []const u8, b: []const u8) bool {
    return std.mem.eql(u8, a, b);
}
fn zstr(s: anytype) []const u8 {
    return std.mem.sliceTo(s, 0);
}
fn portNumber(s: []const u8) !u16 {
    for (s) |ch| if (!std.ascii.isDigit(ch)) return error.InvalidPort;
    const n = std.fmt.parseInt(u16, s, 10) catch return error.InvalidPort;
    return if (n > 0) n else error.InvalidPort;
}
fn scan(a: std.mem.Allocator) ![]c.pm_row {
    var capacity: usize = 256;
    while (capacity <= 65536) : (capacity *= 2) {
        const rows = try a.alloc(c.pm_row, capacity);
        const n = c.pm_scan(rows.ptr, rows.len);
        if (n == -2) {
            a.free(rows);
            continue;
        }
        if (n < 0) {
            a.free(rows);
            return error.PortScanFailed;
        }
        const result = try a.dupe(c.pm_row, rows[0..@intCast(n)]);
        a.free(rows);
        std.mem.sort(c.pm_row, result, {}, struct {
            fn less(_: void, l: c.pm_row, r: c.pm_row) bool {
                if (l.port != r.port) return l.port < r.port;
                if (l.pid != r.pid) return l.pid < r.pid;
                return std.mem.lessThan(u8, zstr(&l.address), zstr(&r.address));
            }
        }.less);
        return result;
    }
    return error.TooManyListeners;
}
// Treat process-derived strings as untrusted terminal text.
fn safe(a: std.mem.Allocator, s: []const u8) ![]const u8 {
    const copy = try a.dupe(u8, s);
    for (copy) |*ch| if (ch.* < 32 or ch.* == 127) {
        ch.* = '?';
    };
    return copy;
}
fn display(a: std.mem.Allocator, rows: []const c.pm_row, filter: ?u16, json: bool, detailed: bool) !void {
    const w = out().writer();
    if (json) try w.writeAll("[") else try w.writeAll("PORT    PROTO   PID       PROCESS                  ADDRESS\n");
    var count: usize = 0;
    for (rows) |r| {
        if (filter != null and r.port != filter.?) continue;
        if (json) {
            if (count > 0) try w.writeAll(",");
            try std.json.stringify(.{ .port = r.port, .protocol = if (r.ipv6 != 0) "tcp6" else "tcp4", .pid = if (r.pid == 0) @as(?u32, null) else r.pid, .process = zstr(&r.name), .address = zstr(&r.address), .executable = zstr(&r.executable) }, .{}, w);
        } else {
            const name = try safe(a, zstr(&r.name));
            defer a.free(name);
            try w.print("{d:<8}{s:<8}{d:<10}{s:<25}{s}\n", .{ r.port, if (r.ipv6 != 0) "tcp6" else "tcp4", r.pid, name, zstr(&r.address) });
            if (detailed) {
                const path = try safe(a, zstr(&r.executable));
                defer a.free(path);
                try w.print("        Executable: {s}\n", .{if (path.len > 0) path else "unavailable (process exited or permission restricted)"});
            }
        }
        count += 1;
    }
    if (json) try w.writeAll("]\n") else if (count == 0) try w.writeAll("No matching TCP listeners.\n");
}
const Options = struct { port: ?u16 = null, json: bool = false, force: bool = false, yes: bool = false, pid: ?u32 = null, interval: u64 = 2 };
fn listCommand(_: std.mem.Allocator, args: []const [:0]u8, mode: []const u8) !void {
    var opt = Options{};
    var i: usize = 0;
    const inspect = same(mode, "inspect");
    const watch = same(mode, "watch");
    if (inspect) {
        if (args.len == 0) return error.MissingPort;
        opt.port = try portNumber(args[0]);
        i = 1;
    }
    while (i < args.len) : (i += 1) {
        const arg = args[i];
        if (same(arg, "--json") and !watch) opt.json = true else if (same(arg, "--port") and !inspect) {
            i += 1;
            if (i == args.len) return error.MissingPort;
            opt.port = try portNumber(args[i]);
        } else if (same(arg, "--interval") and watch) {
            i += 1;
            if (i == args.len) return error.MissingInterval;
            opt.interval = std.fmt.parseInt(u64, args[i], 10) catch return error.InvalidInterval;
            if (opt.interval < 1 or opt.interval > 60) return error.InvalidInterval;
        } else return error.UnknownOption;
    }
    if (watch) c.pm_install_signals();
    while (true) {
        if (watch and out().isTty()) try out().writeAll("\x1b[2J\x1b[HPORTMAN | TCP listeners | Ctrl+C to exit\n\n");
        var frame = std.heap.ArenaAllocator.init(std.heap.page_allocator);
        defer frame.deinit();
        const rows = try scan(frame.allocator());
        try display(frame.allocator(), rows, opt.port, opt.json, inspect);
        if (!watch) break;
        var tick: u64 = 0;
        while (tick < opt.interval * 10 and c.pm_interrupted() == 0) : (tick += 1) std.Thread.sleep(100 * std.time.ns_per_ms);
        if (c.pm_interrupted() != 0) break;
    }
}
fn killCommand(a: std.mem.Allocator, args: []const [:0]u8) !void {
    if (args.len == 0) return error.MissingPort;
    const port = try portNumber(args[0]);
    var opt = Options{};
    var i: usize = 1;
    while (i < args.len) : (i += 1) {
        if (same(args[i], "--yes")) opt.yes = true else if (same(args[i], "--force")) opt.force = true else if (same(args[i], "--pid")) {
            i += 1;
            if (i == args.len) return error.MissingPid;
            opt.pid = std.fmt.parseInt(u32, args[i], 10) catch return error.InvalidPid;
        } else return error.UnknownOption;
    }
    if (opt.yes and opt.pid == null) return error.YesRequiresExplicitPid;
    const rows = try scan(a);
    defer a.free(rows);
    try display(a, rows, port, false, true);
    var selected: ?c.pm_row = null;
    for (rows) |r| {
        if (r.port != port) continue;
        if (opt.pid != null and r.pid != opt.pid.?) continue;
        if (selected) |prev| {
            if (prev.pid != r.pid) return error.MultipleOwnersSpecifyPid;
        }
        selected = r;
    }
    const target = selected orelse return error.NoMatchingListener;
    if (target.pid <= 4 or target.identity == 0) return error.ProcessIdentityUnavailableOrProtected;
    if (windows and !opt.force) return error.WindowsRequiresForce;
    if (!opt.yes) {
        if (!std.io.getStdIn().isTty()) return error.ConfirmationNeedsTerminalOrYesAndPid;
        try out().writer().print("\nThis stops PID {d}, including its other ports. Type that PID to confirm: ", .{target.pid});
        var buffer: [32]u8 = undefined;
        const answer = try std.io.getStdIn().reader().readUntilDelimiterOrEof(&buffer, '\n') orelse return error.Cancelled;
        const confirmed = std.fmt.parseInt(u32, std.mem.trim(u8, answer, " \r\t"), 10) catch return error.Cancelled;
        if (confirmed != target.pid) return error.Cancelled;
    }
    // Refresh after potentially long human confirmation, then pin process identity natively.
    const fresh = try scan(a);
    defer a.free(fresh);
    var found = false;
    for (fresh) |r| if (r.port == port and r.pid == target.pid and r.identity == target.identity) {
        found = true;
        break;
    };
    if (!found) return error.ListenerChanged;
    if (c.pm_terminate(target.pid, target.proc_pid, target.identity, @intFromBool(opt.force)) != 0) return error.TerminationDeniedOrProcessChanged;
    for (0..20) |_| {
        std.Thread.sleep(100 * std.time.ns_per_ms);
        const now = try scan(a);
        defer a.free(now);
        var busy = false;
        for (now) |r| if (r.port == port) {
            busy = true;
            break;
        };
        if (!busy) {
            try out().writer().print("Port {d}: no TCP listeners remain.\n", .{port});
            return;
        }
    }
    return error.PortStillListening;
}
const Loaded = struct { value: config.Config, root: []const u8, state: []const u8 };
fn load(a: std.mem.Allocator, path: []const u8) !Loaded {
    const absolute = try std.fs.cwd().realpathAlloc(a, path);
    const root = std.fs.path.dirname(absolute).?;
    const bytes = try std.fs.cwd().readFileAlloc(a, absolute, 1024 * 1024);
    var diagnostic = config.Diagnostic{};
    const value = config.parse(a, bytes, &diagnostic) catch |e| {
        try err().writer().print("Config line {d}: {s}\n", .{ diagnostic.line, diagnostic.message });
        return e;
    };
    const state = try std.fs.path.join(a, &.{ root, ".portman", std.fs.path.basename(absolute) });
    for (value.services) |s| {
        const cwd = try std.fs.path.resolve(a, &.{ root, s.cwd });
        var dir = try std.fs.cwd().openDir(cwd, .{});
        dir.close();
    }
    return .{ .value = value, .root = root, .state = state };
}
fn logs(a: std.mem.Allocator, args: []const [:0]u8) !void {
    if (args.len == 0 or !config.validName(args[0])) return error.InvalidServiceName;
    var path: []const u8 = "dev.toml";
    var tail: usize = 80;
    var i: usize = 1;
    var got_path = false;
    while (i < args.len) : (i += 1) {
        if (same(args[i], "--tail")) {
            i += 1;
            if (i == args.len) return error.MissingTail;
            tail = std.fmt.parseInt(usize, args[i], 10) catch return error.InvalidTail;
            if (tail == 0 or tail > 10000) return error.InvalidTail;
        } else if (!std.mem.startsWith(u8, args[i], "-") and !got_path) {
            path = args[i];
            got_path = true;
        } else return error.UnknownOption;
    }
    const cfg = try load(a, path);
    var exists = false;
    for (cfg.value.services) |s| if (same(s.name, args[0])) {
        exists = true;
        break;
    };
    if (!exists) return error.UnknownService;
    const file_path = try std.fs.path.join(a, &.{ cfg.state, "logs", try std.fmt.allocPrint(a, "{s}.log", .{args[0]}) });
    var f = try std.fs.cwd().openFile(file_path, .{});
    defer f.close();
    const size = (try f.stat()).size;
    const limit = @min(size, 1024 * 1024);
    try f.seekTo(size - limit);
    const data = try f.readToEndAlloc(a, 1024 * 1024);
    var start = data.len;
    var n: usize = 0;
    if (start > 0 and data[start - 1] == '\n') start -= 1;
    while (start > 0) {
        start -= 1;
        if (data[start] == '\n') {
            n += 1;
            if (n == tail) {
                start += 1;
                break;
            }
        }
    }
    const cleaned = try a.dupe(u8, data[start..]);
    for (cleaned) |*ch| if ((ch.* < 32 and ch.* != '\n' and ch.* != '\t') or ch.* == 127) {
        ch.* = '?';
    };
    try out().writeAll(cleaned);
}
fn up(a: std.mem.Allocator, cfg: Loaded) !void {
    c.pm_install_signals();
    const logdir = try std.fs.path.join(a, &.{ cfg.state, "logs" });
    try std.fs.cwd().makePath(logdir);
    const lockpath = try std.fs.path.join(a, &.{ cfg.state, "supervisor.lock" });
    // OS file lock is released on exit/crash. Keep file to avoid unlink/relock races.
    var lock = std.fs.cwd().createFile(lockpath, .{ .truncate = false, .lock = .exclusive, .lock_nonblocking = true }) catch return error.ProjectAlreadyRunningOrLockDenied;
    defer lock.close();
    const rows = try scan(a);
    for (cfg.value.services) |s| if (s.port) |p| {
        for (rows) |r| if (r.port == p) {
            try err().writer().print("Service {s}: port {d} is already used by PID {d}.\n", .{ s.name, p, r.pid });
            return error.PortConflict;
        };
    };
    var children = std.ArrayList(*c.pm_child).init(a);
    // Capacity is reserved before spawn, so allocation failure cannot orphan a child.
    try children.ensureTotalCapacity(cfg.value.services.len);
    defer {
        for (children.items) |child| c.pm_destroy(child);
    }
    try out().writer().print("PORTMAN | {s}\nCtrl+C stops all services. Logs: {s}\n", .{ cfg.value.name, logdir });
    for (cfg.value.services) |s| {
        if (c.pm_interrupted() != 0) return;
        const cwd = try std.fs.path.resolve(a, &.{ cfg.root, s.cwd });
        const log = try std.fs.path.join(a, &.{ logdir, try std.fmt.allocPrint(a, "{s}.log", .{s.name}) });
        const child = c.pm_start(try a.dupeZ(u8, s.command), try a.dupeZ(u8, cwd), try a.dupeZ(u8, log)) orelse return error.ServiceSpawnFailed;
        children.appendAssumeCapacity(child);
        try out().writer().print("  started {s} | {s}\n", .{ s.name, s.command });
    }
    while (c.pm_interrupted() == 0) {
        for (children.items, 0..) |child, i| {
            var code: c_int = 0;
            const status = c.pm_poll(child, &code);
            if (status < 0) return error.ServicePollFailed;
            if (status == 1) {
                try out().writer().print("Service {s} exited ({d}); stopping project.\n", .{ cfg.value.services[i].name, code });
                if (code != 0) return error.ServiceFailed;
                return;
            }
        }
        std.Thread.sleep(100 * std.time.ns_per_ms);
    }
    try out().writeAll("Stopping service trees...\n");
}
fn run(a: std.mem.Allocator, args: []const [:0]u8) !void {
    if (args.len == 0) return listCommand(a, args, "list");
    const cmd = args[0];
    const rest = args[1..];
    if (same(cmd, "--json") or same(cmd, "--port")) return listCommand(a, args, "list");
    if (same(cmd, "--help") or same(cmd, "help") or same(cmd, "-h")) {
        if (rest.len != 0) return error.UnknownOption;
        try out().writeAll(help);
    } else if (same(cmd, "--version")) {
        if (rest.len != 0) return error.UnknownOption;
        try out().writeAll("portman 0.2.2\n");
    } else if (same(cmd, "list") or same(cmd, "inspect") or same(cmd, "watch")) try listCommand(a, rest, cmd) else if (same(cmd, "kill") or same(cmd, "free")) try killCommand(a, rest) else if (same(cmd, "logs")) try logs(a, rest) else if (same(cmd, "init")) {
        if (rest.len != 0) return error.UnknownOption;
        var file = try std.fs.cwd().createFile("dev.toml", .{ .exclusive = true });
        defer file.close();
        try file.writeAll(template);
        try out().writeAll("Created dev.toml. Edit commands/cwd, then run portman check and portman up.\n");
    } else if (same(cmd, "up") or same(cmd, "run") or same(cmd, "check")) {
        if (rest.len > 1 or (rest.len == 1 and std.mem.startsWith(u8, rest[0], "-"))) return error.UnknownOption;
        const cfg = try load(a, if (rest.len == 1) rest[0] else "dev.toml");
        if (same(cmd, "check")) try out().writer().print("Valid: {s}, {d} service(s). Commands have not been executed.\n", .{ cfg.value.name, cfg.value.services.len }) else try up(a, cfg);
    } else if (same(cmd, "reserve")) {
        if (rest.len != 1) return error.ExpectedOnePort;
        const p = try portNumber(rest[0]);
        const addr = try std.net.Address.parseIp("127.0.0.1", p);
        var server = try addr.listen(.{ .reuse_address = false });
        defer server.deinit();
        c.pm_install_signals();
        try out().writer().print("Holding TCP 127.0.0.1:{d}. Ctrl+C releases it. IPv6/other interfaces are not reserved.\n", .{p});
        while (c.pm_interrupted() == 0) std.Thread.sleep(100 * std.time.ns_per_ms);
    } else return error.UnknownCommand;
}
pub fn main() void {
    var arena = std.heap.ArenaAllocator.init(std.heap.page_allocator);
    defer arena.deinit();
    const a = arena.allocator();
    const args = std.process.argsAlloc(a) catch {
        std.process.exit(1);
    };
    run(a, args[1..]) catch |e| {
        const hint: []const u8 = switch (@as(anyerror, e)) {
            error.WindowsRequiresForce => "Windows needs --force to terminate a process. Inspect the PID first.",
            error.YesRequiresExplicitPid => "Use --yes --pid PID after inspecting the listener.",
            error.MultipleOwnersSpecifyPid => "Several processes use this port. Select one with --pid PID.",
            error.TerminationDeniedOrProcessChanged => "The process changed, permission was denied, or pidfd is unavailable (Linux 5.3+ required).",
            error.PortStillListening => "A listener remains after 2 seconds. Inspect again; the process may ignore SIGTERM or restart automatically.",
            error.ProjectAlreadyRunningOrLockDenied => "Another supervisor holds this config's lock, or the lock file is not writable.",
            error.ServiceFailed, error.ServiceSpawnFailed => "Check portman logs SERVICE, the command, working directory and installed runtime.",
            error.PortScanFailed => "Cannot read TCP listener tables. Check OS permissions and /proc availability on Linux.",
            error.PathAlreadyExists => "The file already exists. Existing configuration was preserved.",
            error.FileNotFound => "File or working directory not found. Run portman init if dev.toml is missing.",
            else => "Run portman --help for usage.",
        };
        err().writer().print("portman: {s}\n{s}\n", .{ @errorName(e), hint }) catch {};
        std.process.exit(1);
    };
}
