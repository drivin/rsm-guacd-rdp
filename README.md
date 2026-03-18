# rsm-guacd-rdp

Minimal build of **Apache Guacamole guacd 1.6.0** with RDP support for platforms that have no official binary:

| Platform | Architecture | Build machine | Toolchain |
|---|---|---|---|
| Windows | x86-64 | x64 | MSYS2 MINGW64 / GCC |
| Windows | ARM64 | x64 (cross-compile) | MSYS2 MINGW64 / Clang |
| Linux | x86-64 | x64 | GCC |
| Linux | ARM64 | x64 (cross-compile) | GCC / aarch64-linux-gnu |

The build produces a standalone `guacd.exe` (Windows) or `guacd` (Linux) plus a portable folder containing all required DLLs / shared libraries.

---

## What is guacd?

`guacd` is the Guacamole proxy daemon. It accepts connections from the Guacamole web application and forwards them to remote desktops via RDP, VNC, SSH, etc.
This repo focuses on an **RDP-only** build (VNC, SSH, Telnet, Kubernetes disabled) to keep dependencies minimal.

---

## Windows Build (x86-64)

### Prerequisites

- **Windows 10/11 64-bit**
- **MSYS2** — download from <https://www.msys2.org/>
  Install to the default path (`C:\msys64`).

Everything else (compiler, libraries, Python) is installed automatically by the build script.

### Quick start

1. Clone or download this repository.
2. Double-click **`run-build.bat`**.

The script opens an MSYS2 MinGW64 shell and runs the full build pipeline automatically.
On first run it downloads the guacamole-server source tarball (~1.2 MB) and installs MSYS2 packages — this takes a few minutes. Subsequent runs are faster because the tarball is cached.

### What the build script does

```
run-build.bat
  └─ build.sh (inside MSYS2 MINGW64)
       1. Install MSYS2/MINGW64 packages (compiler, FreeRDP, Cairo, …)
       2. Download guacamole-server-1.6.0.tar.gz (once, cached)
       3. Extract source (always fresh to avoid double-patching)
       4. Copy compat/ headers into the source tree
       5. Copy src-overrides/ (Windows thread-based process model)
       6. Apply patch-source.py (source-level Windows compatibility patches)
       7. Run autoreconf + ./configure
       8. make -j$(nproc)
       9. Verify guacd.exe was produced
```

After a successful build, collect the portable DLL bundle:

```bat
run-collect-dlls.bat
```

This creates `guacd-bundle\` containing `guacd.exe` and all required DLLs.

### Running guacd on Windows

```powershell
cd guacd-bundle
.\guacd.exe -f -b 127.0.0.1 -l 4822
```

| Flag | Meaning |
|---|---|
| `-f` | Run in foreground (required — `fork()` is not available on Windows) |
| `-b <addr>` | Bind address |
| `-l <port>` | Listen port (default: 4822) |

---

## Windows Build (ARM64)

Cross-compilation from an **x64 Windows machine** — kein ARM64-Gerät erforderlich.

### Prerequisites

- **Windows 10/11 64-bit** (AMD64 Build-Maschine)
- **MSYS2** — download from <https://www.msys2.org/>
  Install to the default path (`C:\msys64`).

Everything else (Clang cross-compiler, ARM64 target libraries, Python) is installed automatically by the build script.

### Quick start

1. Clone or download this repository.
2. Double-click **`run-build-arm64.bat`**.

The script opens an MSYS2 **MINGW64** shell (x64 host tools) and sets `GUACD_TARGET_ARCH=arm64` to activate cross-compile mode in `build.sh`.

### How cross-compilation works

| Aspect | x64 (native) | ARM64 (cross-compile) |
|---|---|---|
| MSYS2 environment | MINGW64 | MINGW64 (x64 host) |
| Compiler | GCC | Clang `--target=aarch64-w64-mingw32` |
| Host tools | `mingw-w64-x86_64-*` | `mingw-w64-x86_64-clang` |
| Target libraries | `/mingw64/` | `/clangarm64/` (`mingw-w64-clang-aarch64-*`) |
| Host triplet | `x86_64-w64-mingw32` | `aarch64-w64-mingw32` |
| Output bundle | `guacd-bundle\` | `guacd-bundle-arm64\` |

`build.sh` erkennt `GUACD_TARGET_ARCH=arm64` und stellt Compiler, `pkg-config` und Linker-Pfade automatisch um. `patch-source.py` ist für beide Ziele identisch.

### Collect the DLL bundle

```bat
run-collect-dlls-arm64.bat
```

This creates `guacd-bundle-arm64\` containing `guacd.exe` and all required ARM64 DLLs (from `/clangarm64/bin`).

### Running guacd on Windows ARM64

Copy the `guacd-bundle-arm64\` folder to the target ARM64 device, then:

```powershell
cd guacd-bundle-arm64
.\guacd.exe -f -b 127.0.0.1 -l 4822
```

### How the Windows port works

Windows lacks several POSIX APIs that guacd depends on. The port addresses them as follows:

| POSIX feature | Windows solution |
|---|---|
| `fork()` | Replaced with `pthread_t` threads (`src-overrides/proc-win32.c`) |
| `socketpair(AF_UNIX)` | TCP loopback pair via `WSADuplicateSocket` (`src-overrides/move-fd-win32.c`) |
| `syslog`, `fcntl`, `fnmatch`, … | Stub headers in `compat/` |
| `uuid_generate` | `CoCreateGuid` from `ole32.dll` |
| `WSAStartup` | Auto-called via `__attribute__((constructor))` in `windows-posix.h` |
| FreeRDP 3.x API | `patch-source.py` patches `rdp.h`, `rdp.c`, `input-queue.c`, `keyboard.c` |

**Note:** Because `fork()` is replaced with threads, guacd **must** be run with `-f` (foreground mode). The `-d` (daemon/background) mode is not supported.

---

## Linux Build (x86-64 and ARM64)

### Prerequisites

```bash
# Debian / Ubuntu
sudo apt-get install -y \
    build-essential autoconf automake libtool pkg-config \
    libcairo2-dev libpng-dev libjpeg-turbo8-dev \
    libssl-dev libfreerdp2-dev uuid-dev

# Fedora / RHEL
sudo dnf install -y \
    gcc autoconf automake libtool pkgconfig \
    cairo-devel libpng-devel libjpeg-turbo-devel \
    openssl-devel freerdp-devel libuuid-devel
```

For **ARM64 cross-compilation** on an x86 host:

```bash
sudo apt-get install -y gcc-aarch64-linux-gnu
```

### Build

```bash
# Download source
wget https://archive.apache.org/dist/guacamole/1.6.0/source/guacamole-server-1.6.0.tar.gz
tar -xzf guacamole-server-1.6.0.tar.gz
cd guacamole-server-1.6.0

# Native build (x86-64 or ARM64)
autoreconf -fi
./configure --prefix=/usr/local \
    --without-vnc --without-ssh --without-telnet \
    --without-kubernetes --without-pulse \
    --without-vorbis --without-webp \
    --disable-guacenc --disable-guaclog
make -j$(nproc)
sudo make install

# Cross-compile for ARM64 (from x86 host)
./configure --host=aarch64-linux-gnu --prefix=/usr/local \
    --without-vnc --without-ssh --without-telnet \
    --without-kubernetes --without-pulse \
    --without-vorbis --without-webp \
    --disable-guacenc --disable-guaclog
make -j$(nproc)
```

---

## Repository structure

```
build.sh                     Main build script (MSYS2 MINGW64 or CLANGARM64)
patch-source.py              Python patcher — applies Windows compatibility edits
collect-dlls.sh              Collects guacd.exe + all DLL dependencies into bundle/
run-build.bat                Windows launcher — x64 build (MSYS2 MINGW64)
run-build-arm64.bat          Windows launcher — ARM64 build (MSYS2 CLANGARM64)
run-collect-dlls.bat         Windows launcher — collect x64 DLLs
run-collect-dlls-arm64.bat   Windows launcher — collect ARM64 DLLs
run-in-mingw64.bat           Generic MSYS2 MINGW64 launcher for any script
run-in-clangarm64.bat        Generic MSYS2 CLANGARM64 launcher for any script

compat/                  POSIX compatibility headers for Windows
  windows-posix.h          Central stub (fork, setsid, kill, socketpair, …)
  fcntl.h                  F_GETFL / F_SETFL / O_NONBLOCK via ioctlsocket
  pwd.h                    struct passwd, getpwnam_r, faccessat, …
  fnmatch.h                Filename pattern matching
  syslog.h                 syslog() stub
  libgen.h                 basename() / dirname()
  netdb.h                  getaddrinfo / gai_strerror (via winsock2)
  uuid/uuid.h              uuid_t + uuid_generate via CoCreateGuid
  sys/socket.h             AF_* / SOCK_* (via winsock2)
  sys/select.h             select / fd_set (via winsock2)
  sys/wait.h               waitpid stub
  sys/statvfs.h            statvfs via GetDiskFreeSpaceEx
  arpa/inet.h              inet_ntop / inet_pton (via winsock2)
  netinet/in.h             sockaddr_in / IPPROTO_* (via winsock2)
  netinet/tcp.h            TCP_NODELAY (via winsock2)

src-overrides/           Windows-specific source replacements
  proc-win32.c             Thread-based connection handler (replaces fork-based proc.c)
  proc-win32.h             Extends guac_proc with pthread_t thread field
  move-fd-win32.c          Socket hand-off via WSADuplicateSocket + _pipe
```

---

## Known limitations

- **No daemon mode** (`-d` flag): `fork()` is stubbed to fail. Always use `-f`.
- **No drive redirection** (`rdpdr`): `fdopendir` is implemented via `GetFinalPathNameByHandle` but not production-tested.
- **Certificate verification**: The Windows build accepts all TLS certificates. For production use, implement proper verification in `src/protocols/rdp/rdp.c`.
- **FreeRDP 3.x only**: The MSYS2 package repository ships FreeRDP 3.x. guacamole 1.6.0 targets FreeRDP 2.x; `patch-source.py` adapts the API differences. This applies to both x64 and ARM64 builds.

---

## License

The compatibility shims and build scripts in this repository are released under the [MIT License](LICENSE).

guacamole-server itself is licensed under the [Apache License 2.0](https://www.apache.org/licenses/LICENSE-2.0) by the Apache Software Foundation.
