#!/usr/bin/env bash
# build.sh
#
# Build Apache Guacamole guacd 1.6.0 on Windows using MSYS2/MINGW64.
#
# Usage (inside MSYS2 MINGW64 shell):
#   bash build.sh
#
# Output:
#   guacamole-server-1.6.0/src/guacd/.libs/guacd.exe
#   guacd-bundle/   (after running collect-dlls.sh)

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
# 0. Environment detection: native build vs. cross-compile
# ---------------------------------------------------------------------------

echo "=== Step 0: Checking environment ==="

if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
    echo "ERROR: This script must be run inside the MSYS2 MINGW64 shell."
    echo "  Open 'MSYS2 MinGW 64-bit' from the Start menu and try again."
    exit 1
fi

MINGW_PREFIX="/mingw64"
PKG_PREFIX="mingw-w64-x86_64"
HOST_TRIPLET="x86_64-w64-mingw32"

echo "OK - Native build: MSYSTEM=$MSYSTEM  HOST=$HOST_TRIPLET  PREFIX=$MINGW_PREFIX"

# ---------------------------------------------------------------------------
# 1. Install MSYS2 packages
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 1: Installing MSYS2/MINGW64 packages ==="

PACKAGES=(
    "${PKG_PREFIX}-toolchain"
    "${PKG_PREFIX}-freerdp"
    "${PKG_PREFIX}-cairo"
    "${PKG_PREFIX}-libpng"
    "${PKG_PREFIX}-libjpeg-turbo"
    "${PKG_PREFIX}-openssl"
    "${PKG_PREFIX}-dlfcn"        # provides dlopen/dlsym/dlclose
    "${PKG_PREFIX}-pkgconf"
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

# Patch Makefile.am files to add -no-undefined so libtool builds shared DLLs
# on Windows (MinGW64 requires all symbols resolved at link time).
echo "Patching Makefile.am files with -no-undefined..."
for makefile_am in \
    "$GUAC_SRC/src/libguac/Makefile.am" \
    "$GUAC_SRC/src/common/Makefile.am" \
    "$GUAC_SRC/src/terminal/Makefile.am" \
    "$GUAC_SRC/src/protocols/rdp/Makefile.am"; do
    if [[ -f "$makefile_am" ]]; then
        sed -i 's/_la_LDFLAGS\s*=/_la_LDFLAGS = -no-undefined /g' "$makefile_am"
        echo "  patched: $makefile_am"
    fi
done

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

autoreconf -fi

# Override autoconf cache variables for features that don't exist on Windows
# but whose absence would abort the build.
export ac_cv_func_fork=yes
export ac_cv_func_vfork=yes
export ac_cv_func_socketpair=yes
export ac_cv_func_timer_create=yes
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
export ac_cv_lib_dl_dlopen=yes
export ac_cv_func_dlopen=yes

# Compiler flags
CPPFLAGS="-I$(pwd)/compat-win32 \
          -I${MINGW_PREFIX}/include \
          -DWINVER=0x0601 \
          -D_WIN32_WINNT=0x0601 \
          -D_WIN32"

LDFLAGS="-L${MINGW_PREFIX}/lib"

LIBS="-lws2_32 -lole32 -ldl"

CFLAGS="-Wno-pedantic"

export CPPFLAGS CFLAGS LDFLAGS LIBS

# Build the configure argument list
CONFIGURE_ARGS=(
    "--prefix=${MINGW_PREFIX}"
    "--host=${HOST_TRIPLET}"
    "--without-vnc"
    "--without-ssh"
    "--without-telnet"
    "--disable-kubernetes"
    "--without-pulse"
    "--without-vorbis"
    "--without-webp"
    "--disable-guacenc"
    "--disable-guaclog"
)

./configure "${CONFIGURE_ARGS[@]}" 2>&1 | tee ../configure.log

# Remove -luuid from libguac's generated Makefile.
# libuuid is static-only on MSYS2; our compat header provides uuid_generate
# inline via CoCreateGuid so the library is not needed at link time.
echo "Removing -luuid from src/libguac/Makefile..."
sed -i 's/ -luuid//g' src/libguac/Makefile

echo ""
echo "Configure complete. Check configure.log for details."
echo "Verifying RDP support was detected..."
if grep -qi "rdp.*yes" ../configure.log 2>/dev/null || \
   grep -qi "freerdp.*yes" ../configure.log 2>/dev/null; then
    echo "  RDP: ENABLED"
else
    echo "  ERROR: RDP support was NOT detected by configure."
    echo "  Ensure ${PKG_PREFIX}-freerdp is installed and pkg-config can find it."
    echo "  Run: pkg-config --modversion freerdp3"
    exit 1
fi

# ---------------------------------------------------------------------------
# 7. Build
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 7: Building (make -j$(nproc)) ==="

make -j"$(nproc)" \
    CFLAGS="-Wno-pedantic -Wno-deprecated-declarations -include $(pwd)/compat-win32/windows-posix.h" \
    2>&1 | tee ../build.log

# ---------------------------------------------------------------------------
# 8. Verify output
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 8: Verifying build output ==="

GUACD_EXE="src/guacd/.libs/guacd.exe"
RDP_PLUGIN=$(ls src/protocols/rdp/.libs/libguac-client-rdp*.dll 2>/dev/null | grep -v '\.dll\.a' | head -1)

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

echo ""
if [[ -n "$RDP_PLUGIN" && -f "$RDP_PLUGIN" ]]; then
    echo "SUCCESS: $RDP_PLUGIN"
    ls -lh "$RDP_PLUGIN"
else
    echo "ERROR: RDP plugin not built (no libguac-client-rdp*.dll found)."
    echo "RDP protocol support is missing. Check build.log for compile errors."
    exit 1
fi

# ---------------------------------------------------------------------------
# 9. Done
# ---------------------------------------------------------------------------

cd ..

BUNDLE_DIR="guacd-bundle"

echo ""
echo "==================================================================="
echo " Build complete!"
echo "==================================================================="
echo ""
echo " Next step - collect the portable bundle:"
echo "   bash collect-dlls.sh"
echo ""
echo " Then run guacd:"
echo "   cd ${BUNDLE_DIR}"
echo "   ./guacd.exe -f -b 127.0.0.1 -l 4822"
echo ""
