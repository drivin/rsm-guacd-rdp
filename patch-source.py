#!/usr/bin/env python3
"""
Patch guacamole-server 1.6.0 source files for Windows/MINGW64 compatibility.

Usage:
    python3 patch-source.py <guacamole-server-root>

Patches applied:
    src/guacd/connection.c      - socketpair→win32_socketpair, read/write→recv/send,
                                  close→closesocket, waitpid→pthread_join
    src/guacd/daemon.c          - #include guards for missing POSIX headers,
                                  insert windows-posix.h early
    src/guacd/log.c             - guard syslog include
    src/libguac/socket-fd.c     - read()/write()/close() → recv()/send()/closesocket()
                                  for WinSock SOCKET compatibility
"""

import sys
import re
import os


def read(path):
    with open(path, 'r', encoding='utf-8', errors='replace') as f:
        return f.read()


def write(path, content):
    with open(path, 'w', encoding='utf-8', newline='\n') as f:
        f.write(content)
    print(f"  patched: {path}")


# ---------------------------------------------------------------------------
# connection.c
# ---------------------------------------------------------------------------

WIN32_SOCKETPAIR_IMPL = r"""
/* -----------------------------------------------------------------------
 * Windows: socketpair() substitute using TCP loopback sockets.
 * On Windows/MINGW64 guacd uses threads so fds are shared; the SOCKET
 * values are cast to int for storage (WinSock uses small index values).
 * --------------------------------------------------------------------- */
#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>

static int win32_socketpair(int sv[2]) {
    struct sockaddr_in addr;
    int addrlen = (int)sizeof(addr);
    SOCKET listener, s0, s1;

    listener = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listener == INVALID_SOCKET) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family      = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    addr.sin_port        = 0;

    if (bind(listener, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR ||
        listen(listener, 1) == SOCKET_ERROR ||
        getsockname(listener, (struct sockaddr*)&addr, &addrlen) == SOCKET_ERROR)
    {
        closesocket(listener);
        return -1;
    }

    s0 = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s0 == INVALID_SOCKET) { closesocket(listener); return -1; }

    if (connect(s0, (struct sockaddr*)&addr, sizeof(addr)) == SOCKET_ERROR) {
        closesocket(s0); closesocket(listener); return -1;
    }
    s1 = accept(listener, NULL, NULL);
    closesocket(listener);
    if (s1 == INVALID_SOCKET) { closesocket(s0); return -1; }

    sv[0] = (int)(intptr_t)s0;
    sv[1] = (int)(intptr_t)s1;
    return 0;
}

/* Map the AF_UNIX socketpair call to our TCP loopback version */
#define socketpair(d,t,p,sv)  win32_socketpair(sv)

/* On Windows, close() on a WinSock SOCKET must use closesocket() */
static inline int win32_close_socket(int fd) { return closesocket((SOCKET)(intptr_t)fd); }
#define CLOSE_SOCKET(fd)  win32_close_socket(fd)

/* recv wrapper matching read() signature */
static inline int win32_recv(int fd, void* buf, size_t n) {
    return recv((SOCKET)(intptr_t)fd, (char*)buf, (int)n, 0);
}
#define READ_SOCKET(fd,buf,n)  win32_recv(fd, buf, n)

/* send wrapper matching write() signature */
static inline int win32_send(int fd, const void* buf, int n) {
    return send((SOCKET)(intptr_t)fd, (const char*)buf, n, 0);
}
#define WRITE_SOCKET(fd,buf,n)  win32_send(fd, buf, n)

#else /* !_WIN32 */
#define CLOSE_SOCKET(fd)       close(fd)
#define READ_SOCKET(fd,buf,n)  read(fd, buf, n)
#define WRITE_SOCKET(fd,buf,n) write(fd, buf, n)
#endif /* _WIN32 */
"""


def patch_connection_c(path):
    src = read(path)
    original = src

    # 1. Insert win32 socketpair block after the last #include block
    #    We look for the last #include line before any code.
    last_include_match = None
    for m in re.finditer(r'^#include\s+[<"].+[>"]\s*$', src, re.MULTILINE):
        last_include_match = m
    if last_include_match is None:
        print("  WARNING: could not find #include block in connection.c")
    else:
        insert_pos = last_include_match.end()
        src = src[:insert_pos] + "\n" + WIN32_SOCKETPAIR_IMPL + src[insert_pos:]

    # 2. Replace socketpair call  (already handled by #define above, but
    #    the AF_UNIX / SOCK_STREAM args still need to compile cleanly -
    #    they are passed to win32_socketpair which ignores them).

    # 3. Replace  close(proc_fd)  with  CLOSE_SOCKET(proc_fd)
    src = src.replace(
        '/* Close our end of the process file descriptor */\n    close(proc_fd);',
        '/* Close our end of the process file descriptor */\n    CLOSE_SOCKET(proc_fd);'
    )

    # 4. Replace  close(params->fd)  with  CLOSE_SOCKET(params->fd)
    src = src.replace(
        'guac_socket_free(params->socket);\n    close(params->fd);',
        'guac_socket_free(params->socket);\n    CLOSE_SOCKET(params->fd);'
    )

    # 5. Replace  read(params->fd, buffer, sizeof(buffer))
    src = src.replace(
        'while ((length = read(params->fd, buffer, sizeof(buffer))) > 0) {',
        'while ((length = READ_SOCKET(params->fd, buffer, sizeof(buffer))) > 0) {'
    )

    # 6. Replace  write(fd, buffer, remaining_length)  in __write_all
    src = src.replace(
        'int written = write(fd, buffer, remaining_length);',
        'int written = WRITE_SOCKET(fd, buffer, remaining_length);'
    )

    # 7. Replace  waitpid(proc->pid, NULL, 0)  with pthread_join on Windows
    src = src.replace(
        '            /* Wait for child to finish */\n            waitpid(proc->pid, NULL, 0);',
        '            /* Wait for proc thread to finish */\n'
        '#ifdef _WIN32\n'
        '            pthread_join(proc->thread, NULL);\n'
        '#else\n'
        '            waitpid(proc->pid, NULL, 0);\n'
        '#endif'
    )

    # 8. Add pthread.h include for Windows pthread_join
    src = src.replace(
        '#include "connection.h"',
        '#include "connection.h"\n'
        '#ifdef _WIN32\n'
        '#include <pthread.h>\n'
        '#endif'
    )

    if src == original:
        print("  WARNING: connection.c - no changes made; check patterns match")
    write(path, src)


# ---------------------------------------------------------------------------
# daemon.c
# ---------------------------------------------------------------------------

DAEMON_WIN32_INCLUDES = """\
#ifdef _WIN32
/* Windows POSIX compatibility layer (must come before other system headers) */
#include "windows-posix.h"
#endif
"""

PROC_MAP_WIN32_INCLUDES = """\
#ifdef _WIN32
/* Windows POSIX compatibility: kill(), waitpid() stubs */
#include "windows-posix.h"
#endif
"""


def patch_daemon_c(path):
    src = read(path)
    original = src

    # 1. Insert windows-posix.h after #include "config.h"
    src = src.replace(
        '#include "config.h"\n',
        '#include "config.h"\n' + DAEMON_WIN32_INCLUDES,
        1
    )

    # 2. Guard sys/wait.h (compat stub handles it, but still needs to compile)
    # Already handled by compat/sys/wait.h - no action needed.

    # 3. The daemonize() function uses fork() + setsid() which are stubbed
    # by windows-posix.h to always fail; daemon.c is always invoked with -f
    # (foreground) so daemonize() is never called in practice. No change needed.

    # 4. signal(SIGPIPE/SIGCHLD, SIG_IGN) - SIGPIPE/SIGCHLD are defined in
    # windows-posix.h, and signal() returns SIG_ERR for them on Windows,
    # which is handled gracefully by the existing log-and-continue code.

    # 5. sigaction(SIGINT/SIGTERM, ...) - windows-posix.h provides sigaction()
    # stub that delegates to signal().

    if src == original:
        print("  WARNING: daemon.c - no changes made; check config.h pattern")
    write(path, src)


# ---------------------------------------------------------------------------
# log.c  (syslog.h is already provided by compat/)
# ---------------------------------------------------------------------------

def patch_proc_map_c(path):
    """proc-map.c uses kill() to probe whether a pid is still alive.
    Our kill() stub returns 0 (alive) which is correct for threads.
    Ensure windows-posix.h is included so kill() is declared."""
    src = read(path)
    original = src

    src = src.replace(
        '#include "config.h"\n',
        '#include "config.h"\n' + PROC_MAP_WIN32_INCLUDES,
        1
    )

    if src == original:
        print("  WARNING: proc-map.c - no changes made; check config.h pattern")
    write(path, src)


def patch_log_c(path):
    """log.c uses syslog() which is stubbed via compat/syslog.h.
    No source changes needed - the stub header handles everything."""
    print(f"  skipped (compat header handles): {path}")


# ---------------------------------------------------------------------------
# socket-fd.c  (WinSock SOCKET vs CRT fd)
# ---------------------------------------------------------------------------

# Preamble block injected after the last #include in socket-fd.c
SOCKET_FD_WIN32_PREAMBLE = r"""
/* -----------------------------------------------------------------------
 * Windows/MINGW64: WinSock SOCKETs are not CRT file descriptors.
 * read()/write()/close() do not work on SOCKETs; use recv/send/closesocket.
 * The fd stored in guac_socket_fd_data is a WinSock SOCKET cast to int.
 * Include-guard prevents double-injection when patcher is run twice.
 * --------------------------------------------------------------------- */
#ifndef GUACD_SOCKET_FD_WIN32_PATCHED
#define GUACD_SOCKET_FD_WIN32_PATCHED
#ifdef _WIN32
#include <winsock2.h>

/* Cast the stored int back to a SOCKET safely on both 32- and 64-bit. */
static inline SOCKET _fd_to_socket(int fd) {
    return (SOCKET)(uintptr_t)(unsigned int)fd;
}

#define SOCKET_READ(fd, buf, n)   recv(_fd_to_socket(fd), (char*)(buf), (int)(n), 0)
#define SOCKET_WRITE(fd, buf, n)  send(_fd_to_socket(fd), (const char*)(buf), (int)(n), 0)
#define SOCKET_CLOSE(fd)          closesocket(_fd_to_socket(fd))

#else  /* !_WIN32 */

#define SOCKET_READ(fd, buf, n)   read((fd), (buf), (n))
#define SOCKET_WRITE(fd, buf, n)  write((fd), (buf), (n))
#define SOCKET_CLOSE(fd)          close(fd)

#endif /* _WIN32 */
#endif /* GUACD_SOCKET_FD_WIN32_PATCHED */
"""


def patch_socket_fd_c(path):
    src = read(path)
    original = src

    # Idempotency: skip preamble injection if already applied
    if 'GUACD_SOCKET_FD_WIN32_PATCHED' in src:
        print(f"  already patched (skipping preamble): {path}")
        return

    # 1. Inject the preamble after the last #include line
    last_inc = None
    for m in re.finditer(r'^#include\s+[<"].+[>"]\s*$', src, re.MULTILINE):
        last_inc = m
    if last_inc is None:
        print("  WARNING: socket-fd.c - no #include found; skipping preamble injection")
    else:
        pos = last_inc.end()
        src = src[:pos] + "\n" + SOCKET_FD_WIN32_PREAMBLE + src[pos:]

    # 2-4. Simple string replacements for the exact patterns in socket-fd.c.
    #
    # In guacamole-server 1.6.0, socket-fd.c stores the fd in a struct field
    # (typically "data->fd") and the calls are:
    #   read(data->fd, buf, count)
    #   write(data->fd, buf, count)
    #   close(data->fd)
    #
    # We replace these with our SOCKET_* macros. Using plain str.replace()
    # avoids regex character-class pitfalls with '->'.

    REPLACEMENTS = [
        # (old,                          new)
        ('read(data->fd,',               'SOCKET_READ(data->fd,'),
        ('write(data->fd,',              'SOCKET_WRITE(data->fd,'),
        ('close(data->fd)',              'SOCKET_CLOSE(data->fd)'),
        # Some versions store it directly as 'socket->data->fd'
        ('read(socket->data->fd,',       'SOCKET_READ(socket->data->fd,'),
        ('write(socket->data->fd,',      'SOCKET_WRITE(socket->data->fd,'),
        ('close(socket->data->fd)',      'SOCKET_CLOSE(socket->data->fd)'),
        # Fallback: plain 'fd' variable name (older layout)
        ('return read(fd,',              'return SOCKET_READ(fd,'),
        ('return write(fd,',             'return SOCKET_WRITE(fd,'),
        ('return close(fd)',             'return SOCKET_CLOSE(fd)'),
    ]

    changed = False
    for old, new in REPLACEMENTS:
        if old in src:
            src = src.replace(old, new)
            changed = True

    if not changed:
        print("  WARNING: socket-fd.c - none of the expected patterns found; "
              "I/O calls NOT replaced. Check the actual field name used for fd.")

    if src == original:
        print("  WARNING: socket-fd.c - no changes applied; verify file structure")
    write(path, src)


# ---------------------------------------------------------------------------
# display.c  (winbase.h included before windows.h → DWORD/HANDLE undefined)
# ---------------------------------------------------------------------------

# display.c (new in 1.6.0) includes <winbase.h> directly without first
# including <windows.h> / <windef.h>.  winbase.h expects DWORD, HANDLE etc.
# to be defined by windef.h, which normally comes in via windows.h.
# Fix: insert   #include <windows.h>   immediately before any <winbase.h>.

DISPLAY_WIN32_GUARD = """\
#ifdef _WIN32
/* windows.h must precede winbase.h so DWORD/LPVOID/HANDLE are defined */
#include <windows.h>
#endif
"""


def patch_display_c(path):
    src = read(path)
    original = src

    # Insert the guard block before the first occurrence of #include <winbase.h>
    TARGET = '#include <winbase.h>'
    if TARGET in src:
        src = src.replace(TARGET, DISPLAY_WIN32_GUARD + TARGET, 1)
    else:
        # winbase.h may be pulled in transitively; add guard at top-of-includes
        # so windows.h is the very first system header in this TU.
        last_inc = None
        for m in re.finditer(r'^#include\s+"[^"]+"\s*$', src, re.MULTILINE):
            last_inc = m
        if last_inc:
            pos = last_inc.end()
            src = src[:pos] + "\n" + DISPLAY_WIN32_GUARD + src[pos:]
        else:
            print("  WARNING: display.c - could not locate insertion point")

    if src == original:
        print("  WARNING: display.c - no changes applied")
    write(path, src)


# ---------------------------------------------------------------------------
# wol.c  (WinSock setsockopt/sendto use const char* instead of void*)
# ---------------------------------------------------------------------------

def patch_wol_c(path):
    """
    WinSock declares:
      setsockopt(..., const char* optval, ...)
      sendto(...,     const char* buf, ...)
    POSIX uses void* / unsigned char*. On MINGW64 -Werror turns the implicit
    pointer-type mismatch into a hard error. Add explicit casts.
    """
    src = read(path)
    original = src

    # setsockopt optval casts: replace &<var>, with (const char*)&<var>,
    # Only within setsockopt calls — use targeted string replacement.
    src = src.replace(
        'SO_BROADCAST, &wol_bcast,',
        'SO_BROADCAST, (const char*)&wol_bcast,',
    )
    src = src.replace(
        'IPV6_MULTICAST_HOPS, &hops,',
        'IPV6_MULTICAST_HOPS, (const char*)&hops,',
    )
    # sendto buf cast
    src = src.replace(
        'sendto(wol_socket, packet,',
        'sendto(wol_socket, (const char*)packet,',
    )

    if src == original:
        print("  WARNING: wol.c - no patterns matched; check source content")
    write(path, src)


# ---------------------------------------------------------------------------
# Terminal library  (src/terminal/)
# ---------------------------------------------------------------------------

# Common Windows compat preamble injected into every terminal source file.
# Placed after the first local (quoted) #include so config.h is processed
# first, but all subsequent system headers see our stubs.
TERMINAL_WIN32_PREAMBLE = """\
#ifdef _WIN32
/* Windows compatibility stubs for terminal library:
 *   mkdir(path,mode) → _mkdir(path)
 *   pipe(fds)        → _pipe(fds, ...)
 *   wcwidth()        → stub
 *   EBADFD, F_GETFL/F_SETFL/O_NONBLOCK, fcntl()  (via fcntl.h shim)
 */
#include <direct.h>    /* _mkdir */
#include <io.h>        /* _pipe  */
#include <winsock2.h>  /* for ioctlsocket in fcntl stub */
#include <stdarg.h>
#include <errno.h>

/* 2-argument mkdir → 1-argument _mkdir (ignore permission bits) */
#ifdef mkdir
#undef mkdir
#endif
#define mkdir(path, mode) _mkdir(path)

/* pipe() → _pipe() */
#ifndef _O_BINARY
#define _O_BINARY 0x8000
#endif
#ifdef pipe
#undef pipe
#endif
#define pipe(fds) _pipe((fds), 65536, _O_BINARY)

/* wcwidth: display-column width of a Unicode code point */
#ifndef _WCWIDTH_DEFINED
#define _WCWIDTH_DEFINED
static inline int wcwidth(unsigned int ucs) {
    if (ucs == 0) return 0;
    if (ucs < 32 || (ucs >= 0x7f && ucs < 0xa0)) return -1;
    if ((ucs >= 0x1100  && ucs <= 0x115f)  ||
        (ucs >= 0x2e80  && ucs <= 0x303e)  ||
        (ucs >= 0x3041  && ucs <= 0x33ff)  ||
        (ucs >= 0xac00  && ucs <= 0xd7a3)  ||
        (ucs >= 0xff01  && ucs <= 0xff60)  ||
        (ucs >= 0xffe0  && ucs <= 0xffe6)  ||
        (ucs >= 0x1f300 && ucs <= 0x1f9ff))
        return 2;
    return 1;
}
#endif /* _WCWIDTH_DEFINED */
#endif /* _WIN32 */
"""


def _inject_terminal_preamble(src, filename):
    """Insert TERMINAL_WIN32_PREAMBLE after the first quoted #include."""
    if 'TERMINAL_WIN32_PREAMBLE' in src or '_WCWIDTH_DEFINED' in src:
        return src  # already applied

    # Find the first  #include "..."  (local header, e.g. config.h)
    m = re.search(r'^#include\s+"[^"]+"\s*$', src, re.MULTILINE)
    if m:
        pos = m.end()
        return src[:pos] + "\n" + TERMINAL_WIN32_PREAMBLE + src[pos:]

    # Fallback: insert before the first #include of any kind
    m = re.search(r'^#include\s+', src, re.MULTILINE)
    if m:
        return src[:m.start()] + TERMINAL_WIN32_PREAMBLE + "\n" + src[m.start():]

    return src  # give up


def _make_terminal_patcher(extra=None):
    """Return a patch function that injects the preamble + optional extra fn."""
    def patcher(path):
        src = read(path)
        original = src
        src = _inject_terminal_preamble(src, os.path.basename(path))
        if extra:
            src = extra(src)
        if src == original:
            print(f"  WARNING: {os.path.basename(path)} - no changes made")
        write(path, src)
    return patcher


patch_terminal_typescript   = _make_terminal_patcher()
patch_terminal_terminal     = _make_terminal_patcher()
patch_terminal_handlers     = _make_terminal_patcher()
patch_terminal_display      = _make_terminal_patcher()


# ---------------------------------------------------------------------------
# RDP plugin: FreeRDP 3.x API compatibility
# ---------------------------------------------------------------------------
# In FreeRDP 2.x, rdp_inst->input was a direct field on the freerdp struct.
# In FreeRDP 3.x it was moved to rdpContext (rdp_inst->context->input).
# Guacamole 1.6.0 accesses it via GUAC_RDP_CONTEXT(rdp_inst)->input, which
# expands to ((guac_rdp_context*)(rdp_inst->context))->input — failing
# because guac_rdp_context has no direct 'input' field (it's in the embedded
# rdpContext). Replace with rdp_inst->context->input throughout.

def patch_rdp_h(path):
    """Fix GUAC_RDP_CONTEXT macro for FreeRDP 3.x.

    In FreeRDP 2.x, settings/update/input were direct members of the freerdp
    struct, so the macro returned (rdp_instance) directly.
    In FreeRDP 3.x these fields moved to rdpContext; the macro must return
    rdp_instance->context so that member accesses resolve correctly.
    """
    src = read(path)
    original = src
    src = src.replace(
        '#define GUAC_RDP_CONTEXT(rdp_instance) ((rdp_instance))',
        '#define GUAC_RDP_CONTEXT(rdp_instance) ((rdp_instance)->context)'
    )
    if src == original:
        print(f"  WARNING: rdp.h - GUAC_RDP_CONTEXT pattern not found")
    write(path, src)


# Injected before rdp_freerdp_verify_certificate — FreeRDP 3.x compatible
# certificate callback. The 2.x VerifyCertificate signature is incompatible
# with 3.x VerifyCertificateEx, so we supply a simple adapter.
RDP_C_VERIFY_CERT_EX = """\
#ifdef _WIN32
/* FreeRDP 3.x VerifyCertificateEx adapter (signature changed from 2.x).
 * Accepts all certificates — for production, implement proper validation. */
static DWORD guac_rdp_freerdp_verify_cert_ex(freerdp* instance,
        const char* host, UINT16 port, const char* common_name,
        const char* subject, const char* issuer,
        const char* fingerprint, DWORD flags) {
    (void)instance; (void)host;    (void)port;    (void)common_name;
    (void)subject;  (void)issuer; (void)fingerprint; (void)flags;
    return 1; /* 1 = accepted */
}
#endif /* _WIN32 */
"""


def patch_rdp_c(path):
    """Patch rdp.c for FreeRDP 3.x:
    - VerifyCertificate → VerifyCertificateEx (renamed + new signature)
    - Inject FreeRDP-3.x compatible adapter after last #include
    - srandom → srand (BSD function not on Windows; also in windows-posix.h)
    - Silence unused-function warning on old verify fn with __attribute__
    """
    src = read(path)
    original = src

    # 1. Inject VerifyCertificateEx adapter after the last #include (top-level)
    if 'guac_rdp_freerdp_verify_cert_ex' not in src:
        last_inc = None
        for m in re.finditer(r'^#include\s+[<"].+[>"]\s*$', src, re.MULTILINE):
            last_inc = m
        if last_inc:
            pos = last_inc.end()
            src = src[:pos] + "\n" + RDP_C_VERIFY_CERT_EX + src[pos:]

    # 2. Silence the now-unused original verify fn
    src = src.replace(
        'static DWORD rdp_freerdp_verify_certificate(',
        'static DWORD __attribute__((unused)) rdp_freerdp_verify_certificate('
    )
    # Fallback: the function might have a different return type
    src = src.replace(
        'static BOOL rdp_freerdp_verify_certificate(',
        'static BOOL __attribute__((unused)) rdp_freerdp_verify_certificate('
    )

    # 3. Change the callback assignment
    src = src.replace(
        'rdp_inst->VerifyCertificate = rdp_freerdp_verify_certificate;',
        'rdp_inst->VerifyCertificateEx = guac_rdp_freerdp_verify_cert_ex;'
    )

    # 4. srandom → srand
    src = src.replace(
        'srandom(time(NULL));',
        'srand((unsigned int)time(NULL));'
    )

    if src == original:
        print(f"  WARNING: rdp.c - no changes applied; check patterns")
    write(path, src)


def _rdp_fix_input_access(src):
    """Replace GUAC_RDP_CONTEXT(rdp_inst)->input with rdp_inst->context->input."""
    return src.replace(
        'GUAC_RDP_CONTEXT(rdp_inst)->input',
        'rdp_inst->context->input'
    )

def patch_rdp_input_queue(path):
    src = read(path)
    original = src
    src = _rdp_fix_input_access(src)
    if src == original:
        print(f"  WARNING: {os.path.basename(path)} - no input access patterns found")
    write(path, src)

def patch_rdp_keyboard(path):
    src = read(path)
    original = src
    src = _rdp_fix_input_access(src)
    if src == original:
        print(f"  WARNING: {os.path.basename(path)} - no input access patterns found")
    write(path, src)


# ---------------------------------------------------------------------------
# Main
# ---------------------------------------------------------------------------

def main():
    if len(sys.argv) < 2:
        print(f"Usage: {sys.argv[0]} <guacamole-server-root>")
        sys.exit(1)

    root = sys.argv[1]

    patches = [
        (os.path.join(root, "src/guacd/connection.c"),   patch_connection_c),
        (os.path.join(root, "src/guacd/daemon.c"),       patch_daemon_c),
        (os.path.join(root, "src/guacd/proc-map.c"),     patch_proc_map_c),
        (os.path.join(root, "src/guacd/log.c"),          patch_log_c),
        (os.path.join(root, "src/libguac/socket-fd.c"),  patch_socket_fd_c),
        (os.path.join(root, "src/libguac/display.c"),    patch_display_c),
        (os.path.join(root, "src/libguac/wol.c"),        patch_wol_c),
        (os.path.join(root, "src/protocols/rdp/rdp.h"),           patch_rdp_h),
        (os.path.join(root, "src/protocols/rdp/rdp.c"),           patch_rdp_c),
        (os.path.join(root, "src/protocols/rdp/input-queue.c"),  patch_rdp_input_queue),
        (os.path.join(root, "src/protocols/rdp/keyboard.c"),     patch_rdp_keyboard),
        (os.path.join(root, "src/terminal/typescript.c"),        patch_terminal_typescript),
        (os.path.join(root, "src/terminal/terminal.c"),          patch_terminal_terminal),
        (os.path.join(root, "src/terminal/terminal-handlers.c"), patch_terminal_handlers),
        (os.path.join(root, "src/terminal/display.c"),           patch_terminal_display),
    ]

    # Optional patches: these files may not exist in all configurations.
    optional_prefixes = ("src/terminal/", "src/protocols/")

    errors = 0
    for path, fn in patches:
        if not os.path.exists(path):
            is_optional = any(p in path.replace("\\", "/") for p in optional_prefixes)
            if is_optional:
                print(f"  OPTIONAL (skip): {path}")
            else:
                print(f"  MISSING: {path}")
                errors += 1
            continue
        print(f"Patching {os.path.basename(path)} ...")
        fn(path)

    if errors:
        print(f"\n{errors} file(s) missing - aborting")
        sys.exit(1)
    else:
        print("\nAll patches applied successfully.")


if __name__ == "__main__":
    main()
