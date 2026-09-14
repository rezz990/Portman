# Portman

**Lihat port, temukan prosesnya, dan jalankan service project dari satu terminal.**

Portman v0.1.0 adalah developer utility native untuk Windows x64 dan Linux x64.
Core CLI, konfigurasi, output, dan supervisor ditulis dalam **Zig**. `platform.c`
menjadi adapter Win32/POSIX untuk tabel TCP, identitas proses, signal, dan process tree.
Tidak perlu Node/Python untuk menjalankan Portman. Runtime service seperti Node,
PHP, atau Bun tetap harus terpasang kalau command project memakainya.

## Langsung pakai

Ekstrak ZIP. Binary berada di folder `bin/windows/` dan `bin/linux/`.
Windows build sudah berhasil di-cross-compile; pengujian runtime lokal dilakukan
di Linux. Tes Windows disediakan dalam GitHub Actions, belum dijalankan di sini.

PowerShell, dari folder hasil ekstrak:

```powershell
.\bin\windows\portman.exe
.\bin\windows\portman.exe inspect 3000
.\bin\windows\portman.exe watch
```

Simpan `portman.exe` di folder milikmu seperti `C:\Tools\Portman`, lalu tambahkan
folder itu ke **user PATH** agar command `portman` bisa dipakai dari folder project.
Tanpa PATH, gunakan path lengkap executable.

Linux:

```bash
chmod +x bin/linux/portman
./bin/linux/portman list
mkdir -p ~/.local/bin
cp bin/linux/portman ~/.local/bin/portman
# Pastikan ~/.local/bin ada dalam PATH shell kamu.
```

## Command

| Command | Perilaku |
|---|---|
| `portman` / `portman list` | Snapshot TCP listener IPv4 dan IPv6, PID, proses, alamat |
| `portman list --json` | Output JSON untuk script; error tetap ke stderr |
| `portman list --port 3000` | Filter port |
| `portman inspect 3000` | Listener plus path executable jika bisa dibaca |
| `portman watch --interval 2` | Refresh setiap 2 detik; Ctrl+C keluar |
| `portman kill 3000` | Linux: minta konfirmasi PID, lalu kirim SIGTERM |
| `portman free 3000` | Alias kill; memeriksa apakah listener port masih ada |
| `portman kill 3000 --force` | Linux SIGKILL; Windows TerminateProcess; tetap konfirmasi |
| `portman kill 3000 --pid 1234 --yes --force` | Mode eksplisit tanpa prompt untuk automation |
| `portman reserve 3000` | Hold socket TCP **127.0.0.1** sampai Ctrl+C |
| `portman init` | Buat `dev.toml`; tidak menimpa file yang sudah ada |
| `portman check [dev.toml]` | Validasi konfigurasi dan folder cwd, tanpa menjalankan command |
| `portman up [dev.toml]` | Jalankan semua service, simpan log, awasi lifecycle |
| `portman run [dev.toml]` | Alias up |
| `portman logs web [dev.toml] --tail 80` | Baca akhir log, default 80 baris |

Untuk menghentikan proses di Windows, tambahkan `--force`. Tidak ada padanan
SIGTERM universal untuk sembarang proses Windows. Menjalankan Portman biasa
tidak membutuhkan admin; mengakses/menghentikan proses milik user lain bisa ditolak OS.

Kill/free menghentikan **satu PID**, bukan seluruh process tree eksternal. Semua
port milik PID itu ikut terdampak. Process tree cleanup khusus service yang
dijalankan oleh `up`. Kalau beberapa PID memakai port sama, pilih `--pid`.
PID dan waktu pembuatan diverifikasi ulang sebelum penghentian; Linux memakai
pidfd untuk menahan identitas target. Listener yang tidak bisa diidentifikasi
tidak akan dihentikan secara tebakan.

## Jalankan project

Di folder project:

```bash
portman init
# Edit dev.toml sesuai service yang sudah ada di project.
portman check
portman up
```

Contoh `dev.toml` untuk web + Laravel API:

```toml
[project]
name = "carwash"

[[services]]
name = "web"
command = "npm run dev -- --port 3000"
cwd = "./web"
port = 3000

[[services]]
name = "api"
command = "php artisan serve --host=127.0.0.1 --port=8000"
cwd = "./api"
port = 8000
```

`cwd` dihitung dari folder file konfigurasi, bukan folder tempat kamu menjalankan
Portman. Omit `cwd` untuk memakai folder konfigurasi. `port` opsional dan hanya
dipakai untuk mendeteksi konflik sebelum startup; **tidak mengubah port command,
tidak inject PORT, dan bukan health check**. Set port di command/framework juga.

Semua port diperiksa sebelum service pertama dimulai. Tetap ada kemungkinan
proses lain mengambil port sesudah pemeriksaan; kegagalan bind akan tampak di log.
Jika **satu service keluar, termasuk exit 0**, Portman menghentikan service lain
dan keluar. Exit service bukan nol membuat Portman exit 1. Ini cocok untuk
service dev yang berjalan terus, bukan pipeline build berurutan.

Ctrl+C menghentikan seluruh project. Linux memberi tiap process group kesempatan
SIGTERM sekitar 1 detik, lalu SIGKILL. Windows menggunakan Job Object dan
penghentian paksa saat shutdown; simpan pekerjaan service sebelum menghentikannya.
Log stdout/stderr tiap service digabung ke:

```text
.portman/dev.toml/logs/web.log
.portman/dev.toml/logs/api.log
```

Log ditambahkan setiap run, belum ada rotasi otomatis. `logs` membaca paling
banyak 1 MiB terakhir. Untuk live log, gunakan `Get-Content -Wait` di PowerShell
atau `tail -f` di Linux. Setiap config punya file lock agar dua `up` untuk file
yang sama tidak berjalan bersamaan. File lock boleh tetap ada setelah keluar;
lock OS otomatis dilepas, jadi tidak perlu menghapus filenya.

Command dieksekusi dengan `/bin/sh -c` di Linux dan `cmd.exe /d /s /c` di Windows,
mewarisi environment Portman. Stdin service diarahkan ke null: command yang
meminta input interaktif tidak cocok. Gunakan config dari sumber yang kamu percaya
karena isinya menjalankan command lokal. Jangan menjalankan daemon yang detach.

## Format konfigurasi

Parser memakai **subset TOML yang ketat**, bukan implementasi TOML penuh:

- Satu `[project]`, dengan field `name` opsional; default `project`.
- 1–32 `[[services]]`: `name`, `command` wajib; `cwd`, `port` opsional.
- Nama 1–64 karakter, huruf ASCII, angka, `-`, `_`; nama/port tidak boleh duplikat.
- String double quote satu baris dengan escape kompatibel JSON. Path Windows
  bisa memakai `C:/dev/app` atau `C:\\dev\\app` di dalam string TOML.
- Komentar `#` di luar string didukung. Port integer 1–65535.
- Field tidak dikenal, duplicate key, single quote, multiline string, environment
  table, array dependency dan format TOML lain ditolak.

## Build dari source

Toolchain dikunci ke **Zig 0.14.1**, sesuai API yang dipakai dan diuji. Jangan
menganggap build ini kompatibel dengan Zig versi lain; `.zigversion` mencatat
pin. Compiler tidak disertakan dalam ZIP. Download dari
[arsip resmi Zig](https://ziglang.org/download/) dan lihat
[dokumentasi Zig 0.14.1](https://ziglang.org/documentation/0.14.1/).

```bash
zig version
zig build -Doptimize=ReleaseSafe
zig build test
zig build run -- inspect 3000
```

Cross-compile:

```bash
zig build -Dtarget=x86_64-windows-gnu -Doptimize=ReleaseSafe --prefix dist/windows
zig build -Dtarget=x86_64-linux-musl -Doptimize=ReleaseSafe --prefix dist/linux
```

Binary muncul di `zig-out/bin` atau `<prefix>/bin`. Linux musl build bersifat
static. Untuk integration test saja, install Python 3 lalu:

```bash
python tests/integration.py zig-out/bin/portman
# Windows:
python tests/integration.py zig-out/bin/portman.exe
```

## Batasan v0.1.0

- CLI + watch yang refresh layar; belum TUI navigasi keyboard.
- TCP listener saja. Belum UDP, koneksi keluar, CPU/RAM/uptime atau mapping project
  untuk proses yang dijalankan di luar Portman.
- Linux membutuhkan `/proc`; penghentian external PID membutuhkan kernel 5.3+
  dan izin pidfd. Container/proc restrictions dapat membatasi identitas proses.
  Socket yang diwariskan bersama ditampilkan dengan satu pemilik yang terlihat;
  `free` melaporkan gagal jika listener lain masih tersisa.
- Hanya IPv4 loopback yang di-hold oleh `reserve`; tidak mereservasi IPv6,
  interface lain, UDP atau port secara permanen. Socket itu bukan HTTP server.
- Belum background daemon, `down`, restart, dependency ordering, readiness check,
  auto-restart, environment per service, maupun GUI.
- Linux Ctrl+C/SIGTERM cleanup didukung. SIGKILL terhadap supervisor, crash fatal,
  atau service yang membuat session/process group baru bisa meninggalkan proses;
  proses tersebut berada di luar jaminan cleanup normal.
- Windows build perlu diuji langsung di laptopmu; cross-compile bukan bukti
  seluruh perilaku Win32 sudah tervalidasi runtime.
- macOS belum didukung.

## Struktur

| File | Tanggung jawab |
|---|---|
| `src/main.zig` | CLI, output JSON, port commands, supervisor, logs |
| `src/config.zig` | Parser subset TOML dan validasi + unit test |
| `src/platform.c` / `.h` | API native OS, identitas PID, process group/Job |
| `tests/integration.py` | Tes socket nyata, guard kill, konflik, lock dan cleanup |
| `.github/workflows/ci.yml` | Build + test Linux/Windows saat di-push ke GitHub |
| `TESTING.md` | Hasil verifikasi paket ini |

Pengembangan berikutnya yang paling berguna: validasi Windows di perangkat asli,
lalu TUI, pengelolaan restart service, dan health check.
