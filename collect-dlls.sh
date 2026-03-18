#!/usr/bin/env bash
# collect-dlls.sh
#
# Copies guacd.exe and all required runtime DLLs into a self-contained
# bundle directory.
#
# Usage (inside MSYS2 MINGW64 shell):
#   bash collect-dlls.sh [bundle-dir]
#
# Default bundle directory: guacd-bundle/

set -euo pipefail

MINGW_BIN="/mingw64/bin"
DEFAULT_BUNDLE="guacd-bundle"

BUNDLE="${1:-$DEFAULT_BUNDLE}"
GUACD_EXE="guacamole-server-1.6.0/src/guacd/.libs/guacd.exe"

echo "=== Collecting guacd bundle into: $BUNDLE (DLL source: $MINGW_BIN) ==="

if [[ ! -f "$GUACD_EXE" ]]; then
    echo "ERROR: $GUACD_EXE not found."
    echo "Run build.sh first, then run this script from the same directory."
    exit 1
fi

mkdir -p "$BUNDLE"
cp -v "$GUACD_EXE" "$BUNDLE/"

# ---- Resolve DLL dependencies recursively using ldd -----------------------
echo ""
echo "Resolving DLL dependencies with ldd ..."
echo ""

collect_dlls() {
    local binary="$1"
    ldd "$binary" 2>/dev/null \
        | grep -i "$MINGW_BIN" \
        | awk '{print $3}' \
        | sort -u
}

# Seed with direct dependencies of guacd.exe
pending=()
while IFS= read -r dll; do
    pending+=("$dll")
done < <(collect_dlls "$BUNDLE/guacd.exe")

# BFS over transitive dependencies
declare -A visited
while [[ ${#pending[@]} -gt 0 ]]; do
    dll="${pending[0]}"
    pending=("${pending[@]:1}")

    [[ "${visited[$dll]+set}" ]] && continue
    visited["$dll"]=1

    if [[ -f "$dll" ]]; then
        name="$(basename "$dll")"
        if [[ ! -f "$BUNDLE/$name" ]]; then
            cp -v "$dll" "$BUNDLE/"
        fi
        # Recurse into this DLL's dependencies
        while IFS= read -r dep; do
            [[ "${visited[$dep]+set}" ]] || pending+=("$dep")
        done < <(collect_dlls "$dll")
    fi
done

# ---- License files (Apache 2.0 compliance) ---------------------------------
echo ""
echo "Copying license files ..."

GUAC_SRC="guacamole-server-1.6.0"

# Apache 2.0 license text
if [[ ! -f "$BUNDLE/LICENSE-APACHE.txt" ]]; then
    curl -fsSL "https://www.apache.org/licenses/LICENSE-2.0.txt" \
        -o "$BUNDLE/LICENSE-APACHE.txt" 2>/dev/null \
    || wget -qO "$BUNDLE/LICENSE-APACHE.txt" \
        "https://www.apache.org/licenses/LICENSE-2.0.txt" 2>/dev/null \
    || echo "  WARNING: could not download Apache 2.0 license text"
fi

# NOTICE file from the guacamole-server source tree
if [[ -f "$GUAC_SRC/NOTICE" ]]; then
    cp -v "$GUAC_SRC/NOTICE" "$BUNDLE/NOTICE"
else
    echo "  WARNING: $GUAC_SRC/NOTICE not found; run collect-dlls.sh before cleaning the source tree"
fi

# Brief description of the modifications made by patch-source.py
cat > "$BUNDLE/CHANGES.txt" << 'EOF'
This binary was built from Apache Guacamole guacd 1.6.0 source code
(https://guacamole.apache.org/) with the following modifications for
Windows/MINGW64 compatibility:

- src/guacd/proc.c         replaced with thread-based implementation (proc-win32.c)
- src/guacd/proc.h         extended with pthread_t thread field (proc-win32.h)
- src/guacd/move-fd.c      replaced with WSADuplicateSocket implementation (move-fd-win32.c)
- src/guacd/connection.c   socketpair() replaced with TCP loopback; read/write/close
                            replaced with recv/send/closesocket
- src/guacd/daemon.c       added windows-posix.h compatibility include
- src/guacd/proc-map.c     added windows-posix.h compatibility include
- src/libguac/socket-fd.c  read/write/close replaced with WinSock equivalents
- src/libguac/display.c    added windows.h include guard before winbase.h
- src/libguac/wol.c        added (const char*) casts for setsockopt/sendto
- src/protocols/rdp/rdp.h  GUAC_RDP_CONTEXT macro updated for FreeRDP 3.x
- src/protocols/rdp/rdp.c  VerifyCertificate → VerifyCertificateEx (FreeRDP 3.x)
- src/protocols/rdp/input-queue.c  input access updated for FreeRDP 3.x
- src/protocols/rdp/keyboard.c     input access updated for FreeRDP 3.x
- src/terminal/*.c         added Windows stubs for mkdir, pipe, wcwidth

All modifications are available at: https://github.com/drivin/rsm-guacd-rdp
EOF
echo "  written: $BUNDLE/CHANGES.txt"

# ---- Summary ---------------------------------------------------------------
echo ""
echo "=== Bundle contents ==="
ls -lh "$BUNDLE/"
echo ""
echo "Total files: $(ls "$BUNDLE" | wc -l)"
echo ""
echo "To run guacd in foreground mode:"
echo "  cd $BUNDLE && ./guacd.exe -f -b 127.0.0.1 -l 4822"
