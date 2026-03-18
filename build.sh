#!/usr/bin/env bash
# build.sh
#
# Build Apache Guacamole guacd 1.6.0 on Windows using MSYS2/MINGW64.
#
# Native x64 build (inside MSYS2 MINGW64 shell):
#   bash build.sh
#
# Cross-compile ARM64 from x64 host (inside MSYS2 MINGW64 shell):
#   GUACD_TARGET_ARCH=arm64 bash build.sh
#
# Output:
#   guacamole-server-1.6.0/src/guacd/.libs/guacd.exe
#   guacd-bundle/         (x64,  after running collect-dlls.sh)
#   guacd-bundle-arm64/   (arm64, after running collect-dlls.sh)

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

TARGET_ARCH="${GUACD_TARGET_ARCH:-native}"   # "native" or "arm64"

if [[ "$TARGET_ARCH" == "arm64" ]]; then
    # ---- Cross-compile ARM64 from x64 host --------------------------------
    # Must run inside MINGW64 (x64 host tools).
    # ARM64 target libraries are taken from the CLANGARM64 prefix (/clangarm64).
    if [[ "${MSYSTEM:-}" != "MINGW64" ]]; then
        echo "ERROR: Cross-compiling ARM64 requires the MSYS2 MINGW64 shell (x64 host)."
        echo "  Open 'MSYS2 MinGW 64-bit' from the Start menu and try again."
        exit 1
    fi
    CROSS_COMPILE=1
    HOST_TRIPLET="aarch64-w64-mingw32"   # target (what we build for)
    BUILD_TRIPLET="x86_64-w64-mingw32"   # build machine (where we compile)
    MINGW_PREFIX="/clangarm64"            # target prefix (ARM64 libraries)
    HOST_PREFIX="/mingw64"               # host prefix  (x64 tools)
    PKG_PREFIX="mingw-w64-clang-aarch64" # target package prefix
    echo "OK - Cross-compiling: ${BUILD_TRIPLET} → ${HOST_TRIPLET}"
else
    # ---- Native build: detect architecture from MSYSTEM -------------------
    CROSS_COMPILE=0
    BUILD_TRIPLET=""
    case "${MSYSTEM:-}" in
        MINGW64)
            MINGW_PREFIX="/mingw64"
            HOST_PREFIX="/mingw64"
            PKG_PREFIX="mingw-w64-x86_64"
            HOST_TRIPLET="x86_64-w64-mingw32"
            ;;
        CLANGARM64)
            MINGW_PREFIX="/clangarm64"
            HOST_PREFIX="/clangarm64"
            PKG_PREFIX="mingw-w64-clang-aarch64"
            HOST_TRIPLET="aarch64-w64-mingw32"
            ;;
        *)
            echo "ERROR: This script must be run inside the MSYS2 MINGW64 or CLANGARM64 shell."
            echo "  For x64:             Open 'MSYS2 MinGW 64-bit' from the Start menu."
            echo "  For ARM64 (native):  Open 'MSYS2 CLANG ARM64' from the Start menu."
            echo "  For ARM64 (cross):   Open 'MSYS2 MinGW 64-bit' and set GUACD_TARGET_ARCH=arm64."
            exit 1
            ;;
    esac
    echo "OK - Native build: MSYSTEM=$MSYSTEM  HOST=$HOST_TRIPLET  PREFIX=$MINGW_PREFIX"
fi

# ---------------------------------------------------------------------------
# 1. Install MSYS2 packages
# ---------------------------------------------------------------------------

echo ""
echo "=== Step 1: Installing MSYS2/MINGW64 packages ==="

if [[ "$CROSS_COMPILE" == "1" ]]; then
    # Host tools: Clang cross-compiler (runs on x64, produces ARM64 output)
    HOST_PACKAGES=(
        mingw-w64-x86_64-clang          # clang/clang++ + llvm-ar/ranlib/nm/strip
        mingw-w64-x86_64-pkgconf
        autoconf
        automake
        libtool
        make
        wget
        python3
    )
    # Target libraries: ARM64 binaries/headers (installed into /clangarm64/)
    TARGET_PACKAGES=(
        "${PKG_PREFIX}-freerdp"
        "${PKG_PREFIX}-cairo"
        "${PKG_PREFIX}-libpng"
        "${PKG_PREFIX}-libjpeg-turbo"
        "${PKG_PREFIX}-openssl"
        "${PKG_PREFIX}-dlfcn"
    )
    pacman -S --needed --noconfirm "${HOST_PACKAGES[@]}" "${TARGET_PACKAGES[@]}"
else
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
fi

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

# Cross-compile toolchain setup
if [[ "$CROSS_COMPILE" == "1" ]]; then
    export CC="clang --target=${HOST_TRIPLET}"
    export CXX="clang++ --target=${HOST_TRIPLET}"
    export AR="llvm-ar"
    export RANLIB="llvm-ranlib"
    export NM="llvm-nm"
    export STRIP="llvm-strip"
    # Point pkg-config to the ARM64 target libraries
    export PKG_CONFIG="${HOST_PREFIX}/bin/pkgconf"
    export PKG_CONFIG_PATH="${MINGW_PREFIX}/lib/pkgconfig"
    export PKG_CONFIG_LIBDIR="${MINGW_PREFIX}/lib/pkgconfig"
    echo "Cross-compile toolchain: CC=$CC  AR=$AR"
fi

# Build the configure argument list
CONFIGURE_ARGS=(
    "--prefix=${MINGW_PREFIX}"
    "--host=${HOST_TRIPLET}"
    "--without-vnc"
    "--without-ssh"
    "--without-telnet"
    "--without-kubernetes"
    "--without-pulse"
    "--without-vorbis"
    "--without-webp"
    "--disable-guacenc"
    "--disable-guaclog"
)

# Tell autoconf the build machine when cross-compiling
if [[ "$CROSS_COMPILE" == "1" ]]; then
    CONFIGURE_ARGS+=("--build=${BUILD_TRIPLET}")
fi

./configure "${CONFIGURE_ARGS[@]}" 2>&1 | tee ../configure.log

echo ""
echo "Configure complete. Check configure.log for details."
echo "Verifying RDP support was detected..."
if grep -q "guacd-rdp.*yes" ../configure.log 2>/dev/null || \
   grep -q "RDP.*enabled" ../configure.log 2>/dev/null || \
   grep -q "libfreerdp.*yes" ../configure.log 2>/dev/null; then
    echo "  RDP: ENABLED"
else
    echo "  WARNING: RDP may not be enabled. Check configure.log."
    echo "  Ensure ${PKG_PREFIX}-freerdp is installed and pkg-config can find it."
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

BUNDLE_DIR="guacd-bundle"
[[ "$TARGET_ARCH" == "arm64" ]] && BUNDLE_DIR="guacd-bundle-arm64"

echo ""
echo "==================================================================="
echo " Build complete!"
echo "==================================================================="
echo ""
echo " Next step - collect the portable bundle:"
echo "   GUACD_TARGET_ARCH=${TARGET_ARCH} bash collect-dlls.sh"
echo ""
echo " Then run guacd:"
echo "   cd ${BUNDLE_DIR}"
echo "   ./guacd.exe -f -b 127.0.0.1 -l 4822"
echo ""
