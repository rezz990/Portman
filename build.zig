const std = @import("std");
pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const optimize = b.standardOptimizeOption(.{});
    const exe = b.addExecutable(.{ .name = if (target.result.os.tag == .windows) "portman-cli" else "portman", .root_source_file = b.path("src/main.zig"), .target = target, .optimize = optimize });
    exe.addIncludePath(b.path("src"));
    exe.addCSourceFile(.{ .file = b.path("src/platform.c"), .flags = &.{ "-std=c11", "-Wall", "-Wextra" } });
    exe.linkLibC();
    if (target.result.os.tag == .windows) {
        exe.linkSystemLibrary("iphlpapi");
        exe.linkSystemLibrary("ws2_32");
        const gui = b.addExecutable(.{ .name = "Portman", .root_source_file = b.path("src/desktop.zig"), .target = target, .optimize = optimize });
        gui.subsystem = .Windows;
        gui.addIncludePath(b.path("src"));
        gui.addWin32ResourceFile(.{ .file = b.path("windows/app.rc"), .include_paths = &.{b.path("windows")} });
        gui.addCSourceFiles(.{ .files = &.{ "src/platform.c", "src/desktop.c" }, .flags = &.{ "-std=c11", "-Wall", "-Wextra" } });
        gui.linkLibC();
        for ([_][]const u8{ "iphlpapi", "ws2_32", "user32", "gdi32", "comctl32", "comdlg32", "shell32", "ole32", "advapi32", "uxtheme", "dwmapi" }) |lib| gui.linkSystemLibrary(lib);
        b.installArtifact(gui);
        if (b.option(bool, "setup", "Build installer after preparing windows/payload") orelse false) {
            const setup = b.addExecutable(.{ .name = "Portman-Setup-0.2.4", .root_source_file = b.path("windows/setup.zig"), .target = target, .optimize = optimize });
            setup.subsystem = .Windows;
            setup.addIncludePath(b.path("windows"));
            setup.addCSourceFile(.{ .file = b.path("windows/setup.c"), .flags = &.{ "-std=c11", "-Wall", "-Wextra" } });
            setup.addWin32ResourceFile(.{ .file = b.path("windows/app.rc"), .include_paths = &.{b.path("windows")} });
            setup.linkLibC();
            for ([_][]const u8{ "user32", "gdi32", "shell32", "ole32", "advapi32", "uuid", "comctl32", "uxtheme", "dwmapi" }) |lib| setup.linkSystemLibrary(lib);
            b.installArtifact(setup);
        }
    }
    b.installArtifact(exe);
    const run = b.addRunArtifact(exe);
    if (b.args) |args| run.addArgs(args);
    b.step("run", "Run Portman").dependOn(&run.step);
    const tests = b.addTest(.{ .root_source_file = b.path("src/config.zig"), .target = target, .optimize = optimize });
    b.step("test", "Test configuration validation").dependOn(&b.addRunArtifact(tests).step);
    const engine_tests = b.addTest(.{ .root_source_file = b.path("src/desktop.zig"), .target = target, .optimize = optimize });
    engine_tests.addIncludePath(b.path("src"));
    engine_tests.addCSourceFile(.{ .file = b.path("src/platform.c"), .flags = &.{ "-std=c11", "-Wall", "-Wextra" } });
    engine_tests.linkLibC();
    if (target.result.os.tag == .windows) {
        engine_tests.linkSystemLibrary("iphlpapi");
        engine_tests.linkSystemLibrary("ws2_32");
    }
    b.step("test-desktop", "Test desktop persistence and process lifecycle").dependOn(&b.addRunArtifact(engine_tests).step);
}
