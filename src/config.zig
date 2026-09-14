const std = @import("std");
pub const Service = struct { name: []const u8 = "", command: []const u8 = "", cwd: []const u8 = ".", port: ?u16 = null };
pub const Config = struct { name: []const u8 = "project", services: []const Service };
pub const Diagnostic = struct { line: usize = 0, message: []const u8 = "" };

fn fail(d: *Diagnostic, message: []const u8) error{InvalidConfig} {
    d.message = message;
    return error.InvalidConfig;
}
fn quoted(a: std.mem.Allocator, text: []const u8, d: *Diagnostic) ![]const u8 {
    if (text.len < 2 or text[0] != '"' or text[text.len - 1] != '"') return fail(d, "Expected a double-quoted string");
    // Supported TOML basic strings: JSON-compatible escapes; reject multiline strings.
    const parsed = std.json.parseFromSlice([]const u8, a, text, .{ .allocate = .alloc_always }) catch return fail(d, "Invalid string escape (use / or \\\\ in Windows paths)");
    defer parsed.deinit();
    if (std.mem.indexOfScalar(u8, parsed.value, 0) != null or std.mem.indexOfAny(u8, parsed.value, "\r\n") != null) return fail(d, "NUL and multiline values are not supported");
    return a.dupe(u8, parsed.value);
}
pub fn validName(name: []const u8) bool {
    if (name.len == 0 or name.len > 64) return false;
    for (name) |ch| if (!std.ascii.isAlphanumeric(ch) and ch != '-' and ch != '_') return false;
    // Names become log filenames. Windows device names stay reserved even with .log.
    for ([_][]const u8{ "CON", "PRN", "AUX", "NUL" }) |reserved| {
        if (std.ascii.eqlIgnoreCase(name, reserved)) return false;
    }
    if (name.len == 4 and name[3] >= '1' and name[3] <= '9' and
        (std.ascii.eqlIgnoreCase(name[0..3], "COM") or std.ascii.eqlIgnoreCase(name[0..3], "LPT"))) return false;
    return true;
}
// Deliberately strict TOML subset. Unknown fields are errors, never ignored.
// Returned strings are allocated from a caller-owned arena.
pub fn parse(a: std.mem.Allocator, text: []const u8, d: *Diagnostic) !Config {
    var services = std.ArrayList(Service).init(a);
    var name: []const u8 = "project";
    var section: enum { none, project, service } = .none;
    var seen_project = false;
    var seen_name = false;
    var keys: u8 = 0;
    var lines = std.mem.splitScalar(u8, text, '\n');
    while (lines.next()) |raw| {
        d.line += 1;
        var end = raw.len;
        var in_string = false;
        var escaped = false;
        for (raw, 0..) |ch, i| {
            if (escaped) {
                escaped = false;
                continue;
            }
            if (in_string and ch == '\\') {
                escaped = true;
                continue;
            }
            if (ch == '"') in_string = !in_string;
            if (ch == '#' and !in_string) {
                end = i;
                break;
            }
        }
        const line = std.mem.trim(u8, raw[0..end], " \r\t");
        if (line.len == 0) continue;
        if (std.mem.eql(u8, line, "[project]")) {
            if (seen_project) return fail(d, "Duplicate [project] table");
            seen_project = true;
            section = .project;
            continue;
        }
        if (std.mem.eql(u8, line, "[[services]]")) {
            if (services.items.len >= 32) return fail(d, "Maximum 32 services");
            try services.append(.{});
            section = .service;
            keys = 0;
            continue;
        }
        const eq = std.mem.indexOfScalar(u8, line, '=') orelse return fail(d, "Expected key = value or a supported table");
        const key = std.mem.trim(u8, line[0..eq], " \t");
        const value = std.mem.trim(u8, line[eq + 1 ..], " \t");
        switch (section) {
            .none => return fail(d, "Put fields inside [project] or [[services]]"),
            .project => {
                if (!std.mem.eql(u8, key, "name")) return fail(d, "Unknown project field");
                if (seen_name) return fail(d, "Duplicate project name");
                name = try quoted(a, value, d);
                seen_name = true;
                if (!validName(name)) return fail(d, "Project name must use 1-64 letters, digits, - or _");
            },
            .service => {
                const s = &services.items[services.items.len - 1];
                const bit: u8 = if (std.mem.eql(u8, key, "name")) 1 else if (std.mem.eql(u8, key, "command")) 2 else if (std.mem.eql(u8, key, "cwd")) 4 else if (std.mem.eql(u8, key, "port")) 8 else return fail(d, "Unknown service field");
                if (keys & bit != 0) return fail(d, "Duplicate service field");
                keys |= bit;
                switch (bit) {
                    1 => s.name = try quoted(a, value, d),
                    2 => s.command = try quoted(a, value, d),
                    4 => s.cwd = try quoted(a, value, d),
                    8 => {
                        for (value) |ch| if (!std.ascii.isDigit(ch)) return fail(d, "Port must be an integer from 1 to 65535");
                        s.port = std.fmt.parseInt(u16, value, 10) catch return fail(d, "Port must be an integer from 1 to 65535");
                        if (s.port.? == 0) return fail(d, "Port cannot be zero");
                    },
                    else => unreachable,
                }
            },
        }
    }
    if (services.items.len == 0) return fail(d, "At least one [[services]] entry is required");
    for (services.items, 0..) |s, i| {
        if (!validName(s.name)) return fail(d, "Service name must use 1-64 letters, digits, - or _");
        if (std.mem.trim(u8, s.command, " \t").len == 0) return fail(d, "Every service needs a nonempty command");
        if (s.cwd.len == 0) return fail(d, "cwd cannot be empty");
        for (services.items[0..i]) |other| {
            if (std.ascii.eqlIgnoreCase(s.name, other.name)) return fail(d, "Duplicate service name (case-insensitive)");
            if (s.port != null and other.port == s.port) return fail(d, "Duplicate service port");
        }
    }
    return .{ .name = name, .services = try services.toOwnedSlice() };
}
test "quoted hashes, CRLF, paths and multiple services" {
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    var d = Diagnostic{};
    const c = try parse(arena.allocator(), "[project]\r\nname = \"demo\"\n[[services]]\nname=\"web\"\ncommand=\"echo #hi\" # comment\ncwd=\"C:\\\\dev\"\nport=3000\n[[services]]\nname=\"api\"\ncommand=\"php artisan serve\"", &d);
    try std.testing.expectEqual(@as(usize, 2), c.services.len);
    try std.testing.expectEqualStrings("echo #hi", c.services[0].command);
    try std.testing.expectEqualStrings("C:\\dev", c.services[0].cwd);
}
test "reject invalid configs instead of guessing" {
    const cases = [_][]const u8{
        "",                                                        "[[services]]\nname=\"../bad\"\ncommand=\"x\"",                                         "[[services]]\nname=\"web\"",
        "[[services]]\nname=\"web\"\ncommand=\"x\"\nport=0",       "[[services]]\nname=\"web\"\ncommand=\"x\"\nport=65536",                                "[[services]]\nname=\"web\"\ncommand=\"x\"\nrestart=true",
        "[[services]]\nname=\"web\"\nname=\"api\"\ncommand=\"x\"", "[[services]]\nname=\"web\"\ncommand=\"x\"\n[[services]]\nname=\"web\"\ncommand=\"y\"", "[[services]]\nname=\"web\"\ncommand=\"x\"\nport=3\n[[services]]\nname=\"api\"\ncommand=\"y\"\nport=3",
        "[[services]]\nname=\"web\"\ncommand=\"x\\u0000y\"",
    };
    for (cases) |input| {
        var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
        defer arena.deinit();
        var d = Diagnostic{};
        try std.testing.expectError(error.InvalidConfig, parse(arena.allocator(), input, &d));
        try std.testing.expect(d.message.len > 0);
    }
}

test "Windows log names cannot alias devices or differ only by case" {
    for ([_][]const u8{ "CON", "con", "NUL", "Aux", "COM1", "lpt9" }) |name| try std.testing.expect(!validName(name));
    try std.testing.expect(validName("web-console"));
    var arena = std.heap.ArenaAllocator.init(std.testing.allocator);
    defer arena.deinit();
    var diag = Diagnostic{};
    try std.testing.expectError(error.InvalidConfig, parse(arena.allocator(), "[[services]]\nname=\"web\"\ncommand=\"x\"\n[[services]]\nname=\"WEB\"\ncommand=\"y\"", &diag));
}
