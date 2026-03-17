#!/usr/bin/env bash
# build.sh
#
# Build Apache Guacamole guacd 1.6.0 on Windows using MSYS2/MINGW64.
#
# Run this script INSIDE the MSYS2 MINGW64 shell:
#   cd /path/to/guacd-windows-build
#   bash build.sh
#
# Output:
#   guacamole-server-1.6.0/src/guacd/.libs/guacd.exe
#   guacd-bundle/   (portable bundle with DLLs; requires running collect-dlls.sh)

set -euo pipefail

# ---------------------------------------------------------------------------
# Configuration
# ---------------------------------------------------------------------------

GUAC_VERSION="1.6.0"
GUAC_TARBALL="guacamole-server-${GUAC_VERSION}.tar.gz"
GUAC_URL="https://archive.apache.org/dist/guacamole/${GUAC_VERSION}/source/${GUAC_TARBALL}"
GUAC_SRC="guacamole-server-${GUAC_VERSION}"

# Absolute path to THIS script's directory (works even when called from elsewhere)
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
COMPAT_DIR="$SCRIPT_DIR/compat"
OVERRIDES_DIR="$SCRIPT_DIR/src-overrides"

# ---------------------------------------------------------------------------
# 0. Sanity check: must be running inside MINGW64
# ---------------------------------------------------------------------------

echo "=== Step 0: Checking environment ==="
if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
    echo "ERROR: This script must be run inside the MSYS2 MINGW64 shell."
    echo "  Open 'MSYS2 MinGW 64-bit' from the Start menu and try again."
    exit 1
fi
echo "OK - MSYSTEM=$MSYSTEM"

# ---------------------------------------------------------------------------
# 1. Install MSYS2 packages
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 1: Installing MSYS2/MINGW64 packages ==="

PACKAGES=(
    mingw-w64-x86_64-toolchain
    mingw-w64-x86_64-freerdp
    mingw-w64-x86_64-cairo
    mingw-w64-x86_64-libpng
    mingw-w64-x86_64-libjpeg-turbo
    mingw-w64-x86_64-openssl
    mingw-w64-x86_64-dlfcn       # provides dlopen/dlsym/dlclose for MINGW64
    mingw-w64-x86_64-pkgconf
    autoconf
    automake
    libtool
    make
    wget
    python3
)

pacman -S --needed --noconfirm "${PACKAGES[@]}"

echo "Packages installed."

# ---------------------------------------------------------------------------
# 2. Download and extract source
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 2: Downloading guacamole-server ${GUAC_VERSION} ==="

if [[ ! -f "$GUAC_TARBALL" ]]; then
    wget -c "$GUAC_URL" -O "$GUAC_TARBALL"
else
    echo "  (tarball already present, skipping download)"
fi

# Always re-extract to get a clean, unpatched source tree.
# This ensures patch-source.py is never applied twice to the same files.
# The tarball is cached above so this is fast.
if [[ -d "$GUAC_SRC" ]]; then
    echo "Removing existing source directory for clean extraction..."
    rm -rf "$GUAC_SRC"
fi
echo "Extracting..."
tar -xzf "$GUAC_TARBALL"

# ---------------------------------------------------------------------------
# 3. Install compatibility headers into the source tree
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 3: Installing compatibility headers ==="

# Copy the compat/ tree so configure and make can find the stubs via -I
COMPAT_DEST="$GUAC_SRC/compat-win32"
mkdir -p "$COMPAT_DEST/uuid" "$COMPAT_DEST/sys" "$COMPAT_DEST/arpa" "$COMPAT_DEST/netinet"


cp -v "$COMPAT_DIR/threads.h"           "$COMPAT_DEST/threads.h"
cp -v "$COMPAT_DIR/fcntl.h"             "$COMPAT_DEST/fcntl.h"
cp -v "$COMPAT_DIR/syslog.h"            "$COMPAT_DEST/syslog.h"
cp -v "$COMPAT_DIR/libgen.h"            "$COMPAT_DEST/libgen.h"
cp -v "$COMPAT_DIR/windows-posix.h"     "$COMPAT_DEST/windows-posix.h"
cp -v "$COMPAT_DIR/netdb.h"             "$COMPAT_DEST/netdb.h"
cp -v "$COMPAT_DIR/uuid/uuid.h"         "$COMPAT_DEST/uuid/uuid.h"
cp -v "$COMPAT_DIR/sys/wait.h"          "$COMPAT_DEST/sys/wait.h"
cp -v "$COMPAT_DIR/sys/socket.h"        "$COMPAT_DEST/sys/socket.h"
cp -v "$COMPAT_DIR/sys/select.h"        "$COMPAT_DEST/sys/select.h"
cp -v "$COMPAT_DIR/arpa/inet.h"         "$COMPAT_DEST/arpa/inet.h"
cp -v "$COMPAT_DIR/netinet/in.h"        "$COMPAT_DEST/netinet/in.h"
cp -v "$COMPAT_DIR/netinet/tcp.h"       "$COMPAT_DEST/netinet/tcp.h"
cp -v "$COMPAT_DIR/pwd.h"              "$COMPAT_DEST/pwd.h"
cp -v "$COMPAT_DIR/fnmatch.h"          "$COMPAT_DEST/fnmatch.h"
cp -v "$COMPAT_DIR/sys/statvfs.h"      "$COMPAT_DEST/sys/statvfs.h"

# ---------------------------------------------------------------------------
# 4. Replace proc.c / proc.h / move-fd.c with Windows-compatible versions
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 4: Installing Windows source overrides ==="

cp -v "$OVERRIDES_DIR/proc-win32.c"   "$GUAC_SRC/src/guacd/proc.c"
cp -v "$OVERRIDES_DIR/proc-win32.h"   "$GUAC_SRC/src/guacd/proc.h"
cp -v "$OVERRIDES_DIR/move-fd-win32.c" "$GUAC_SRC/src/guacd/move-fd.c"

# ---------------------------------------------------------------------------
# 5. Apply Python patches to connection.c and daemon.c
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 5: Patching connection.c / daemon.c ==="

python3 "$SCRIPT_DIR/patch-source.py" "$GUAC_SRC"

# ---------------------------------------------------------------------------
# 6. Configure
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 6: Running autoreconf + configure ==="

cd "$GUAC_SRC"

# Run autoreconf if configure doesn't exist yet
if [[ ! -f configure ]]; then
    autoreconf -fi
fi

# Override autoconf cache variables for features that don't exist on Windows
# but whose absence would abort the build.  Our source overrides provide
# compatible replacements, so we tell configure the functions "exist".
export ac_cv_func_fork=yes
export ac_cv_func_vfork=yes
export ac_cv_func_socketpair=yes
export ac_cv_func_timer_create=yes      # disabled anyway; just avoids abort
export ac_cv_func_timer_settime=yes
export ac_cv_func_uuid_generate=yes
export ac_cv_lib_uuid_uuid_generate=yes
export ac_cv_func_setpgid=yes
export ac_cv_func_setsid=yes
export ac_cv_func_setuid=yes
export ac_cv_func_setgid=yes
export ac_cv_func_getpwnam_r=yes
export ac_cv_func_kill=yes
export ac_cv_func_waitpid=yes
export ac_cv_func_sigaction=yes
export ac_cv_lib_dl_dlopen=yes   # dlfcn provided by mingw-w64-x86_64-dlfcn
export ac_cv_func_dlopen=yes

# Extra compiler flags
#   -I compat-win32   → our stub headers shadow system paths
#   -DWINVER etc.     → ensure WinSock2 targets modern Windows
# NOTE: do NOT add -include windows.h here — it breaks the autoconf
# undeclared-builtin check. display.c is patched individually instead.
CPPFLAGS="-I$(pwd)/compat-win32 \
          -DWINVER=0x0601 \
          -D_WIN32_WINNT=0x0601 \
          -D_WIN32"

# Link against winsock2, ole32 (for CoCreateGuid), and dl (for dlopen/dlsym)
LIBS="-lws2_32 -lole32 -ldl"

# CFLAGS for configure: keep minimal — -include breaks autoconf's undeclared-
# builtin detection test ("cannot make gcc report undeclared builtins").
# -Wno-pedantic: needed because compat/fcntl.h uses #include_next (GCC ext).
CFLAGS="-Wno-pedantic"

export CPPFLAGS CFLAGS LIBS

./configure \
    --prefix=/mingw64 \
    --host=x86_64-w64-mingw32 \
    --without-vnc \
    --without-ssh \
    --without-telnet \
    --without-kubernetes \
    --without-pulse \
    --without-vorbis \
    --without-webp \
    --disable-guacenc \
    --disable-guaclog \
    2>&1 | tee ../configure.log
# NOTE: --disable-posix-timers is not a valid configure flag in 1.6.0;
# POSIX timer absence is handled via ac_cv_func_timer_create=yes (exported above).

echo ""
echo "Configure complete. Check configure.log for details."
echo "Verifying RDP support was detected..."
if grep -q "guacd-rdp.*yes" ../configure.log 2>/dev/null || \
   grep -q "RDP.*enabled" ../configure.log 2>/dev/null || \
   grep -q "libfreerdp.*yes" ../configure.log 2>/dev/null; then
    echo "  RDP: ENABLED"
else
    echo "  WARNING: RDP may not be enabled. Check configure.log."
    echo "  Ensure mingw-w64-x86_64-freerdp is installed and pkg-config can find it."
fi

# ---------------------------------------------------------------------------
# 7. Build
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 7: Building (make -j$(nproc)) ==="

# Add -include windows-posix.h here (not at configure time) so autoconf
# builtin detection is not disturbed.  This injects our POSIX stubs into
# every translation unit compiled by make without touching source files.
make -j"$(nproc)" \
    CFLAGS="-Wno-pedantic -Wno-deprecated-declarations -include $(pwd)/compat-win32/windows-posix.h" \
    2>&1 | tee ../build.log

# ---------------------------------------------------------------------------
# 8. Verify output
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 8: Verifying build output ==="

GUACD_EXE="src/guacd/.libs/guacd.exe"

if [[ -f "$GUACD_EXE" ]]; then
    echo "SUCCESS: $GUACD_EXE"
    ls -lh "$GUACD_EXE"
    echo ""
    echo "Dynamic dependencies:"
    ldd "$GUACD_EXE" | head -40
else
    echo "FAILED: $GUACD_EXE not found."
    echo "Check build.log for errors."
    exit 1
fi

# ---------------------------------------------------------------------------
# 9. Done
# ---------------------------------------------------------------------------

cd ..

echo ""
echo "==================================================================="
echo " Build complete!"
echo "==================================================================="
echo ""
echo " Next step - collect the portable bundle:"
echo "   bash collect-dlls.sh"
echo ""
echo " Then run guacd:"
echo "   cd guacd-bundle"
echo "   ./guacd.exe -f -b 127.0.0.1 -l 4822"
echo ""
