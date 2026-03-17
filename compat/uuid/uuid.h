/*
 * Windows compatibility stub for uuid/uuid.h
 *
 * Strategy:
 *   When building with -include windows-posix.h, windows.h is already
 *   pulled in (via winsock2.h) before this header is processed.  That
 *   causes three problems:
 *
 *   1. rpcndr.h / basetyps.h may #define uuid_t UUID (= GUID struct).
 *      Our "typedef unsigned char uuid_t[16]" then expands to
 *      "typedef unsigned char UUID[16]", conflicting with typedef GUID UUID.
 *
 *   2. combaseapi.h already declares CoCreateGuid(GUID*) so our private
 *      _guacd_guid forward-declaration would conflict.
 *
 *   3. The _guacd_guid* argument type differs from GUID* in the declared
 *      CoCreateGuid, causing another mismatch.
 *
 * Fix: if windows.h has been included (GUID is defined), undefine any
 * uuid_t macro and use GUID + CoCreateGuid directly from the system headers.
 * Otherwise fall back to the private _guacd_guid forward declaration
 * (this path is taken when uuid.h is the first include, which no longer
 * happens in practice with the -include flag but keeps the header robust).
 */

#ifndef COMPAT_UUID_UUID_H
#define COMPAT_UUID_UUID_H

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#ifdef _WIN32

/* ------------------------------------------------------------------
 * If windows.h was already included, GUID is already defined.
 * Remove any uuid_t macro that windows headers may have installed
 * (rpcndr.h / basetyps.h sometimes do  #define uuid_t UUID).
 * ------------------------------------------------------------------ */
#ifdef uuid_t
#undef uuid_t
#endif

/* uuid_t: 16 raw bytes, POSIX convention */
typedef unsigned char uuid_t[16];

/* ------------------------------------------------------------------
 * uuid_generate: use CoCreateGuid.
 *
 * If windows.h was included, GUID and CoCreateGuid are already
 * declared in combaseapi.h / objbase.h.  We just use them directly.
 * If windows.h was NOT included, forward-declare what we need.
 * ------------------------------------------------------------------ */
#ifndef _WINDOWS_
/* windows.h not yet included: bring in the minimum needed */
#include <windows.h>
#endif
/* At this point GUID and CoCreateGuid are guaranteed to be declared */

static inline void uuid_generate(uuid_t out) {
    GUID g;
    CoCreateGuid(&g);
    out[0]  = (unsigned char)(g.Data1 >> 24);
    out[1]  = (unsigned char)(g.Data1 >> 16);
    out[2]  = (unsigned char)(g.Data1 >>  8);
    out[3]  = (unsigned char)(g.Data1);
    out[4]  = (unsigned char)(g.Data2 >>  8);
    out[5]  = (unsigned char)(g.Data2);
    out[6]  = (unsigned char)(g.Data3 >>  8);
    out[7]  = (unsigned char)(g.Data3);
    memcpy(&out[8], g.Data4, 8);
}

#else /* !_WIN32 */

typedef unsigned char uuid_t[16];

static inline void uuid_generate(uuid_t out) {
    memset(out, 0, 16); /* fallback: zero UUID */
}

#endif /* _WIN32 */

static inline void uuid_generate_random(uuid_t out) { uuid_generate(out); }
static inline void uuid_generate_time(uuid_t out)   { uuid_generate(out); }

static inline void uuid_unparse(const uuid_t uu, char* out) {
    snprintf(out, 37,
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x"
        "-%02x%02x%02x%02x%02x%02x",
        uu[0],  uu[1],  uu[2],  uu[3],
        uu[4],  uu[5],
        uu[6],  uu[7],
        uu[8],  uu[9],
        uu[10], uu[11], uu[12], uu[13], uu[14], uu[15]);
}

static inline void uuid_unparse_lower(const uuid_t uu, char* out) {
    uuid_unparse(uu, out);
}

static inline void uuid_copy(uuid_t dst, const uuid_t src) {
    memcpy(dst, src, 16);
}

static inline int uuid_compare(const uuid_t uu1, const uuid_t uu2) {
    return memcmp(uu1, uu2, 16);
}

static inline void uuid_clear(uuid_t uu) {
    memset(uu, 0, 16);
}

static inline int uuid_is_null(const uuid_t uu) {
    uuid_t z;
    memset(z, 0, 16);
    return memcmp(uu, z, 16) == 0;
}

#endif /* COMPAT_UUID_UUID_H */
