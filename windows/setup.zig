const c = @cImport({
    @cInclude("setup.h");
});
const gui = @embedFile("payload/Portman.exe");
const cli = @embedFile("payload/portman-cli.exe");
const guide = @embedFile("QUICKSTART.txt");
pub fn main() void {
    _ = c.pmi_main(gui.ptr, gui.len, cli.ptr, cli.len, guide.ptr, guide.len);
}
