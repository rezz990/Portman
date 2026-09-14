const std = @import("std");
const cfg = @import("config.zig");
const c = @cImport({
    @cInclude("desktop.h");
});
const a = std.heap.page_allocator;
const Item = struct {
    data: c.pmd_service = std.mem.zeroes(c.pmd_service),
    child: ?*c.pm_child = null,
    started: i64 = 0,
};
var items: [32]Item = [_]Item{.{}} ** 32;
var count: usize = 0;
var root: [4096]u8 = @splat(0);
var last_error: [512:0]u8 = @splat(0);
fn str(s: anytype) []const u8 {
    return std.mem.sliceTo(s, 0);
}
fn copy(dst: []u8, src: []const u8) !void {
    if (src.len >= dst.len) return error.ValueTooLong;
    @memset(dst, 0);
    @memcpy(dst[0..src.len], src);
}
fn fail(msg: []const u8) c_int {
    copy(&last_error, msg) catch {};
    return 0;
}
fn err(e: anyerror) c_int {
    return fail(@errorName(e));
}
fn valid(i: c_int) bool {
    return i >= 0 and i < count;
}
fn path(name: []const u8) ![]u8 {
    return std.fs.path.join(a, &.{ str(&root), name });
}
fn logfile(i: usize) ![]u8 {
    const name = try std.fmt.allocPrint(a, "logs/{s}.log", .{str(&items[i].data.name)});
    defer a.free(name);
    return path(name);
}
fn serialize(data: []const Item) ![]u8 {
    var out = std.ArrayList(u8).init(a);
    errdefer out.deinit();
    const w = out.writer();
    try w.writeAll("[project]\nname = \"desktop\"\n");
    for (data) |it| {
        try w.writeAll("\n[[services]]\nname = ");
        try std.json.stringify(str(&it.data.name), .{}, w);
        try w.writeAll("\ncommand = ");
        try std.json.stringify(str(&it.data.command), .{}, w);
        try w.writeAll("\ncwd = ");
        try std.json.stringify(str(&it.data.cwd), .{}, w);
        if (it.data.port != 0) try w.print("\nport = {d}", .{it.data.port});
        try w.writeByte('\n');
    }
    return out.toOwnedSlice();
}
fn writeSnapshot(dest: []const u8, data: []const Item) !void {
    const temp = try std.fmt.allocPrint(a, "{s}.{x}.tmp", .{ dest, std.crypto.random.int(u64) });
    defer a.free(temp);
    const bytes = try serialize(data);
    defer a.free(bytes);
    var f = try std.fs.cwd().createFile(temp, .{ .exclusive = true, .mode = 0o600 });
    defer std.fs.cwd().deleteFile(temp) catch {};
    {
        defer f.close();
        try f.writeAll(bytes);
        try f.sync();
    }
    try std.fs.cwd().rename(temp, dest);
}
fn save(data: []const Item) !void {
    const dest = try path("services.toml");
    defer a.free(dest);
    try writeSnapshot(dest, data);
}
export fn pmd_export(filename: [*:0]const u8) c_int {
    if (count == 0) return fail("Add at least one service before exporting.");
    if (!std.fs.path.isAbsolute(str(filename))) return fail("Choose an absolute export filename.");
    writeSnapshot(str(filename), items[0..count]) catch |e| return err(e);
    return 1;
}
export fn pmd_row_matches(row: *const c.pm_row, query: [*:0]const u8) c_int {
    const q = std.mem.trim(u8, str(query), " \t");
    if (q.len == 0) return 1;
    var digits: [64]u8 = undefined;
    const ids = std.fmt.bufPrint(&digits, "{d} {d}", .{ row.port, row.pid }) catch return 0;
    for ([_][]const u8{ ids, str(&row.name), str(&row.address), str(&row.executable) }) |value| {
        if (std.ascii.indexOfIgnoreCase(value, q) != null) return 1;
    }
    return 0;
}
const Loaded = struct { list: [32]Item, len: usize };
fn parseFile(filename: []const u8, allow_empty: bool) !Loaded {
    const bytes = try std.fs.cwd().readFileAlloc(a, filename, 1024 * 1024);
    defer a.free(bytes);
    if (allow_empty and std.mem.eql(u8, bytes, "[project]\nname = \"desktop\"\n")) return .{ .list = [_]Item{.{}} ** 32, .len = 0 };
    var arena = std.heap.ArenaAllocator.init(a);
    defer arena.deinit();
    var diag = cfg.Diagnostic{};
    const parsed = cfg.parse(arena.allocator(), bytes, &diag) catch |e| {
        _ = std.fmt.bufPrintZ(&last_error, "Line {d}: {s}", .{ diag.line, diag.message }) catch {};
        return e;
    };
    var result = Loaded{ .list = [_]Item{.{}} ** 32, .len = parsed.services.len };
    const base = std.fs.path.dirname(filename) orelse ".";
    for (parsed.services, 0..) |s, i| {
        try copy(&result.list[i].data.name, s.name);
        try copy(&result.list[i].data.command, s.command);
        const cwd = try std.fs.path.resolve(a, &.{ base, s.cwd });
        defer a.free(cwd);
        try copy(&result.list[i].data.cwd, cwd);
        result.list[i].data.port = s.port orelse 0;
    }
    return result;
}
export fn pmd_init(directory: [*:0]const u8) c_int {
    copy(&root, str(directory)) catch |e| return err(e);
    std.fs.cwd().makePath(str(&root)) catch |e| return err(e);
    const logs = path("logs") catch |e| return err(e);
    defer a.free(logs);
    std.fs.cwd().makePath(logs) catch |e| return err(e);
    const filename = path("services.toml") catch |e| return err(e);
    defer a.free(filename);
    const loaded = parseFile(filename, true) catch |e| {
        if (e == error.FileNotFound) {
            count = 0;
            return 1;
        }
        if (e != error.InvalidConfig) _ = err(e);
        return 0;
    };
    items = loaded.list;
    count = loaded.len;
    return 1;
}
export fn pmd_error() [*:0]const u8 {
    return &last_error;
}
export fn pmd_count() c_int {
    return @intCast(count);
}
export fn pmd_get(index: c_int, out: *c.pmd_service) c_int {
    if (!valid(index)) return fail("Select a service first.");
    out.* = items[@intCast(index)].data;
    return 1;
}
export fn pmd_put(index: c_int, input: *const c.pmd_service) c_int {
    if (index != -1 and !valid(index)) return fail("Invalid service.");
    if (index == -1 and count == 32) return fail("Maximum 32 services.");
    if (index >= 0 and items[@intCast(index)].child != null) return fail("Stop the service before editing it.");
    // Reject unterminated ABI buffers before reading any string.
    if (input.name[64] != 0 or input.command[2047] != 0 or input.cwd[2047] != 0) return fail("A field is too long.");
    if (!cfg.validName(str(&input.name))) return fail("Name: 1-64 letters, digits, hyphens or underscores.");
    if (std.mem.trim(u8, str(&input.command), " \t\r\n").len == 0) return fail("Enter a command.");
    for ([_][]const u8{ str(&input.name), str(&input.command), str(&input.cwd) }) |value| {
        if (!std.unicode.utf8ValidateSlice(value) or std.mem.indexOfAny(u8, value, "\r\n") != null) return fail("Fields must be single-line UTF-8 text.");
    }
    if (!std.fs.path.isAbsolute(str(&input.cwd))) return fail("Choose an absolute project folder.");
    var dir = std.fs.openDirAbsolute(str(&input.cwd), .{}) catch return fail("Project folder does not exist or cannot be opened.");
    dir.close();
    for (items[0..count], 0..) |it, i| {
        if (index == i) continue;
        if (std.ascii.eqlIgnoreCase(str(&it.data.name), str(&input.name))) return fail("A service with this name already exists.");
        if (input.port != 0 and input.port == it.data.port) return fail("Another service is configured with that port.");
    }
    var next = items;
    const i: usize = if (index == -1) count else @intCast(index);
    next[i] = .{};
    next[i].data = input.*;
    next[i].data.state = 0;
    next[i].data.pid = 0;
    next[i].data.elapsed = 0;
    next[i].data.exit_code = 0;
    const n = count + @as(usize, if (index == -1) 1 else 0);
    save(next[0..n]) catch |e| return err(e);
    items = next;
    count = n;
    return 1;
}
export fn pmd_remove(index: c_int) c_int {
    if (!valid(index)) return fail("Select a service first.");
    const i: usize = @intCast(index);
    if (items[i].child != null) return fail("Stop the service before removing it.");
    var next = items;
    for (i..count - 1) |j| next[j] = next[j + 1];
    next[count - 1] = .{};
    save(next[0 .. count - 1]) catch |e| return err(e);
    items = next;
    count -= 1;
    return 1;
}
export fn pmd_import(filename: [*:0]const u8) c_int {
    const loaded = parseFile(str(filename), false) catch |e| {
        if (e != error.InvalidConfig) _ = err(e);
        return 0;
    };
    if (count + loaded.len > 32) return fail("Import would exceed 32 services.");
    var next = items;
    for (loaded.list[0..loaded.len], 0..) |it, n| {
        for (next[0 .. count + n]) |other| {
            if (std.ascii.eqlIgnoreCase(str(&it.data.name), str(&other.data.name))) return fail("Import has a duplicate service name. No changes saved.");
            if (it.data.port != 0 and it.data.port == other.data.port) return fail("Import has a duplicate port. No changes saved.");
        }
        next[count + n] = it;
    }
    save(next[0 .. count + loaded.len]) catch |e| return err(e);
    items = next;
    count += loaded.len;
    return 1;
}
export fn pmd_start(index: c_int) c_int {
    if (!valid(index)) return fail("Select a service first.");
    const i: usize = @intCast(index);
    const it = &items[i];
    if (it.child != null) return 1;
    if (it.data.port != 0) {
        const rows = a.alloc(c.pm_row, 8192) catch |e| return err(e);
        defer a.free(rows);
        const n = c.pm_scan(rows.ptr, rows.len);
        if (n < 0) return fail("Cannot check ports. Service was not started.");
        for (rows[0..@intCast(n)]) |r| if (r.port == it.data.port) {
            _ = std.fmt.bufPrintZ(&last_error, "Port {d} is already used by PID {d} ({s}). Choose another port or stop that app.", .{ r.port, r.pid, str(&r.name) }) catch {};
            return 0;
        };
    }
    const log = logfile(i) catch |e| return err(e);
    defer a.free(log);
    // Rotate at next start; never truncate a file that a running process writes.
    if (std.fs.cwd().statFile(log)) |stat| {
        if (stat.size > 5 * 1024 * 1024) {
            const backup = std.fmt.allocPrint(a, "{s}.previous", .{log}) catch |e| return err(e);
            defer a.free(backup);
            std.fs.cwd().rename(log, backup) catch |e| return err(e);
        }
    } else |_| {}
    const logz = a.dupeZ(u8, log) catch |e| return err(e);
    defer a.free(logz);
    it.child = c.pm_start(@ptrCast(&it.data.command), @ptrCast(&it.data.cwd), logz.ptr);
    if (it.child == null) {
        it.data.state = 4;
        return fail("Could not start service. Check folder, command, permissions and log location.");
    }
    it.started = std.time.milliTimestamp();
    it.data.pid = c.pm_child_pid(it.child.?);
    it.data.state = if (it.data.port == 0) 3 else 1;
    it.data.elapsed = 0;
    it.data.exit_code = 0;
    return 1;
}
export fn pmd_stop(index: c_int) void {
    if (!valid(index)) return;
    const it = &items[@intCast(index)];
    if (it.child) |child| c.pm_destroy(child);
    it.child = null;
    it.data.state = 0;
    it.data.pid = 0;
}
export fn pmd_running() c_int {
    var n: c_int = 0;
    for (items[0..count]) |it| {
        if (it.child != null) n += 1;
    }
    return n;
}
export fn pmd_shutdown() void {
    for (0..count) |i| pmd_stop(@intCast(i));
}
export fn pmd_tick() void {
    if (pmd_running() == 0) return;
    const rows = a.alloc(c.pm_row, 8192) catch return;
    defer a.free(rows);
    const n = c.pm_scan(rows.ptr, rows.len);
    for (items[0..count]) |*it| {
        const child = it.child orelse continue;
        var code: c_int = 0;
        const poll = c.pm_poll(child, &code);
        if (poll != 0) {
            c.pm_destroy(child);
            it.child = null;
            it.data.pid = 0;
            it.data.exit_code = if (poll < 0) -1 else code;
            it.data.state = if (poll > 0 and code == 0) 0 else 4;
            continue;
        }
        it.data.elapsed = @intCast(@max(0, @divTrunc(std.time.milliTimestamp() - it.started, 1000)));
        if (it.data.port == 0) {
            it.data.state = 3;
            continue;
        }
        if (n < 0) {
            it.data.state = 5;
            continue;
        }
        var owned = false;
        var foreign = false;
        for (rows[0..@intCast(n)]) |r| if (r.port == it.data.port) {
            if (c.pm_child_owns(child, r.pid) != 0) owned = true else foreign = true;
        };
        it.data.state = if (foreign) 5 else if (owned) 2 else if (it.data.elapsed >= 30) 5 else 1;
    }
}
export fn pmd_log_path(index: c_int, out: [*]u8, capacity: c_int) c_int {
    if (!valid(index) or capacity <= 0) return 0;
    const p = logfile(@intCast(index)) catch return 0;
    defer a.free(p);
    copy(out[0..@intCast(capacity)], p) catch return 0;
    return 1;
}
export fn pmd_log(index: c_int, out: [*]u8, capacity: c_int) c_int {
    if (capacity < 2) return 0;
    out[0] = 0;
    if (!valid(index)) return 0;
    const p = logfile(@intCast(index)) catch return 0;
    defer a.free(p);
    var f = std.fs.cwd().openFile(p, .{}) catch return 0;
    defer f.close();
    const size = f.getEndPos() catch return 0;
    const max: usize = @intCast(capacity - 1);
    const skip = size > max;
    f.seekTo(if (skip) size - max else 0) catch return 0;
    const n = f.readAll(out[0..max]) catch return 0;
    var start: usize = 0;
    if (skip) {
        while (start < n and (out[start] & 0xc0) == 0x80) : (start += 1) {}
    }
    if (start > 0) std.mem.copyForwards(u8, out[0 .. n - start], out[start..n]);
    out[n - start] = 0;
    return @intCast(n - start);
}
export fn pmd_clear_log(index: c_int) c_int {
    if (!valid(index)) return fail("Select a service first.");
    if (items[@intCast(index)].child != null) return fail("Stop service before clearing its log.");
    const p = logfile(@intCast(index)) catch |e| return err(e);
    defer a.free(p);
    var f = std.fs.cwd().createFile(p, .{}) catch |e| return err(e);
    f.close();
    return 1;
}
pub fn main() void {
    _ = c.pm_desktop_main();
}

test "desktop saves, reloads, validates duplicates and persists an empty list" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const dir = try tmp.dir.realpathAlloc(a, ".");
    defer a.free(dir);
    const zdir = try a.dupeZ(u8, dir);
    defer a.free(zdir);
    try std.testing.expectEqual(@as(c_int, 1), pmd_init(zdir.ptr));
    defer pmd_shutdown();
    var service = std.mem.zeroes(c.pmd_service);
    try copy(&service.name, "test-service");
    try copy(&service.command, "echo hello");
    try copy(&service.cwd, dir);
    service.port = 54321;
    try std.testing.expectEqual(@as(c_int, 1), pmd_put(-1, &service));
    try std.testing.expectEqual(@as(c_int, 0), pmd_put(-1, &service));
    try std.testing.expectEqual(@as(c_int, 1), pmd_init(zdir.ptr));
    try std.testing.expectEqual(@as(c_int, 1), pmd_count());
    var out: c.pmd_service = undefined;
    try std.testing.expectEqual(@as(c_int, 1), pmd_get(0, &out));
    try std.testing.expectEqualStrings("echo hello", str(&out.command));
    try std.testing.expectEqual(@as(c_int, 1), pmd_remove(0));
    try std.testing.expectEqual(@as(c_int, 1), pmd_init(zdir.ptr));
    try std.testing.expectEqual(@as(c_int, 0), pmd_count());
}
test "desktop import is atomic and resolves folders relative to config" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const dir = try tmp.dir.realpathAlloc(a, ".");
    defer a.free(dir);
    const zdir = try a.dupeZ(u8, dir);
    defer a.free(zdir);
    try std.testing.expectEqual(@as(c_int, 1), pmd_init(zdir.ptr));
    defer pmd_shutdown();
    try tmp.dir.writeFile(.{ .sub_path = "dev.toml", .data = "[[services]]\nname=\"web\"\ncommand=\"echo hello\"\ncwd=\".\"\nport=53129\n" });
    const file = try std.fs.path.joinZ(a, &.{ dir, "dev.toml" });
    defer a.free(file);
    try std.testing.expectEqual(@as(c_int, 1), pmd_import(file.ptr));
    try std.testing.expectEqual(@as(c_int, 0), pmd_import(file.ptr));
    try std.testing.expectEqual(@as(c_int, 1), pmd_count());
    var s: c.pmd_service = undefined;
    _ = pmd_get(0, &s);
    try std.testing.expectEqualStrings(dir, str(&s.cwd));
    try tmp.dir.writeFile(.{ .sub_path = "dev.toml", .data = "[[services]]\nname=\"valid\"\ncommand=\"echo x\"\n[[services]]\nname=\"WEB\"\ncommand=\"echo duplicate\"\n" });
    try std.testing.expectEqual(@as(c_int, 0), pmd_import(file.ptr));
    try std.testing.expectEqual(@as(c_int, 1), pmd_count());
    try tmp.dir.writeFile(.{ .sub_path = "services.toml", .data = "invalid configuration" });
    try std.testing.expectEqual(@as(c_int, 0), pmd_init(zdir.ptr));
    const still = try tmp.dir.readFileAlloc(a, "services.toml", 200);
    defer a.free(still);
    try std.testing.expectEqualStrings("invalid configuration", still);
}
test "desktop real process lifecycle, log output, edit guards and failure state" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const dir = try tmp.dir.realpathAlloc(a, ".");
    defer a.free(dir);
    const zdir = try a.dupeZ(u8, dir);
    defer a.free(zdir);
    try std.testing.expectEqual(@as(c_int, 1), pmd_init(zdir.ptr));
    defer pmd_shutdown();
    var s = std.mem.zeroes(c.pmd_service);
    try copy(&s.name, "lifecycle");
    try copy(&s.cwd, dir);
    const win = @import("builtin").os.tag == .windows;
    try copy(&s.command, if (win) "echo portman-test & ping -n 30 127.0.0.1 >nul" else "echo portman-test; sleep 30");
    try std.testing.expectEqual(@as(c_int, 1), pmd_put(-1, &s));
    try std.testing.expectEqual(@as(c_int, 1), pmd_start(0));
    try std.testing.expectEqual(@as(c_int, 1), pmd_running());
    try std.testing.expectEqual(@as(c_int, 0), pmd_put(0, &s));
    try std.testing.expectEqual(@as(c_int, 0), pmd_remove(0));
    try std.testing.expectEqual(@as(c_int, 0), pmd_clear_log(0));
    var log: [1024]u8 = @splat(0);
    var attempts: usize = 0;
    while (attempts < 50) : (attempts += 1) {
        _ = pmd_log(0, &log, log.len);
        if (std.mem.indexOf(u8, str(&log), "portman-test") != null) break;
        std.Thread.sleep(50 * std.time.ns_per_ms);
    }
    try std.testing.expect(std.mem.indexOf(u8, str(&log), "portman-test") != null);
    pmd_stop(0);
    try std.testing.expectEqual(@as(c_int, 0), pmd_running());
    try std.testing.expectEqual(@as(c_int, 1), pmd_clear_log(0));
    try copy(&s.command, "exit 7");
    try std.testing.expectEqual(@as(c_int, 1), pmd_put(0, &s));
    try std.testing.expectEqual(@as(c_int, 1), pmd_start(0));
    attempts = 0;
    while (pmd_running() != 0 and attempts < 50) : (attempts += 1) {
        std.Thread.sleep(50 * std.time.ns_per_ms);
        pmd_tick();
    }
    _ = pmd_get(0, &s);
    try std.testing.expectEqual(@as(c_int, 4), s.state);
    try std.testing.expectEqual(@as(c_int, 7), s.exit_code);
}

test "desktop rejects occupied ports and tracks only its own listener" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const dir = try tmp.dir.realpathAlloc(a, ".");
    defer a.free(dir);
    const zdir = try a.dupeZ(u8, dir);
    defer a.free(zdir);
    try std.testing.expectEqual(@as(c_int, 1), pmd_init(zdir.ptr));
    defer pmd_shutdown();
    var listener = try (try std.net.Address.parseIp4("127.0.0.1", 0)).listen(.{});
    const port = listener.listen_address.getPort();
    listener.deinit();
    const cmd = try std.fmt.allocPrintZ(a, "{s} -u -m http.server {d} --bind 127.0.0.1", .{ if (@import("builtin").os.tag == .windows) "python" else "python3", port });
    defer a.free(cmd);
    var service = std.mem.zeroes(c.pmd_service);
    try copy(&service.name, "http-test");
    try copy(&service.cwd, dir);
    try copy(&service.command, cmd);
    service.port = port;
    try std.testing.expectEqual(@as(c_int, 1), pmd_put(-1, &service));
    const foreign_log = try std.fs.path.joinZ(a, &.{ dir, "foreign.log" });
    defer a.free(foreign_log);
    const foreign = c.pm_start(cmd.ptr, zdir.ptr, foreign_log.ptr) orelse return error.TestProcessStartFailed;
    var foreign_alive = true;
    defer {
        if (foreign_alive) c.pm_destroy(foreign);
    }
    const rows = try a.alloc(c.pm_row, 8192);
    defer a.free(rows);
    var ready = false;
    for (0..100) |_| {
        const n = c.pm_scan(rows.ptr, rows.len);
        try std.testing.expect(n >= 0);
        for (rows[0..@intCast(n)]) |r| if (r.port == port) {
            ready = true;
        };
        if (ready) break;
        std.Thread.sleep(50 * std.time.ns_per_ms);
    }
    try std.testing.expect(ready);
    try std.testing.expectEqual(@as(c_int, 0), pmd_start(0));
    try std.testing.expectEqual(@as(c_int, 0), pmd_running());
    c.pm_destroy(foreign);
    foreign_alive = false;
    try std.testing.expectEqual(@as(c_int, 1), pmd_start(0));
    for (0..100) |_| {
        pmd_tick();
        _ = pmd_get(0, &service);
        if (service.state == 2) break;
        std.Thread.sleep(50 * std.time.ns_per_ms);
    }
    try std.testing.expectEqual(@as(c_int, 2), service.state);
    pmd_stop(0);
    const n = c.pm_scan(rows.ptr, rows.len);
    try std.testing.expect(n >= 0);
    for (rows[0..@intCast(n)]) |r| try std.testing.expect(r.port != port);
}

test "export round trip preserves config without runtime state and failed writes keep settings" {
    var tmp = std.testing.tmpDir(.{});
    defer tmp.cleanup();
    const dir = try tmp.dir.realpathAlloc(a, ".");
    defer a.free(dir);
    const zdir = try a.dupeZ(u8, dir);
    defer a.free(zdir);
    try std.testing.expectEqual(@as(c_int, 1), pmd_init(zdir.ptr));
    defer pmd_shutdown();
    const dest = try std.fs.path.joinZ(a, &.{ dir, "export.toml" });
    defer a.free(dest);
    try std.testing.expectEqual(@as(c_int, 0), pmd_export(dest.ptr));
    var s = std.mem.zeroes(c.pmd_service);
    try copy(&s.name, "roundtrip");
    try copy(&s.command, "echo \"quoted # content\"");
    try copy(&s.cwd, dir);
    try std.testing.expectEqual(@as(c_int, 1), pmd_put(-1, &s));
    try std.testing.expectEqual(@as(c_int, 1), pmd_export(dest.ptr));
    try copy(&s.command, "echo replaced");
    try std.testing.expectEqual(@as(c_int, 1), pmd_put(0, &s));
    try std.testing.expectEqual(@as(c_int, 1), pmd_export(dest.ptr)); // overwrite succeeds
    const snapshot = try tmp.dir.readFileAlloc(a, "export.toml", 16384);
    defer a.free(snapshot);
    try std.testing.expect(std.mem.indexOf(u8, snapshot, "echo replaced") != null);
    try std.testing.expect(std.mem.indexOf(u8, snapshot, "pid") == null);
    const bad = try std.fs.path.joinZ(a, &.{ dir, "missing", "bad.toml" });
    defer a.free(bad);
    try std.testing.expectEqual(@as(c_int, 0), pmd_export(bad.ptr));
    try std.testing.expectEqual(@as(c_int, 1), pmd_count());
    try std.testing.expectEqual(@as(c_int, 1), pmd_remove(0));
    try std.testing.expectEqual(@as(c_int, 1), pmd_import(dest.ptr));
    _ = pmd_get(0, &s);
    try std.testing.expectEqualStrings("echo replaced", str(&s.command));
    try std.testing.expectEqualStrings(dir, str(&s.cwd));
    try std.testing.expectEqual(@as(c_uint, 0), s.pid);
}
test "port search matches IDs and text and handles empty queries" {
    var row = std.mem.zeroes(c.pm_row);
    row.port = 5173;
    row.pid = 9812;
    try copy(&row.name, "node.exe");
    try copy(&row.address, "127.0.0.1");
    try copy(&row.executable, "C:/Program Files/nodejs/node.exe");
    for ([_][:0]const u8{ "", "  ", "5173", "9812", "NODE", "program files", "127.0." }) |query| {
        try std.testing.expectEqual(@as(c_int, 1), pmd_row_matches(&row, query.ptr));
    }
    try std.testing.expectEqual(@as(c_int, 0), pmd_row_matches(&row, "mysql"));
}
