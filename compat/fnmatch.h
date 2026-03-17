/*
 * Windows compatibility stub for <fnmatch.h>
 *
 * Provides a minimal fnmatch() supporting * and ? wildcards,
 * sufficient for guacamole's RDP filesystem filter patterns.
 */

#ifndef COMPAT_FNMATCH_H
#define COMPAT_FNMATCH_H

#ifdef _WIN32

#define FNM_NOMATCH  1
#define FNM_NOESCAPE 0x01
#define FNM_PATHNAME 0x02
#define FNM_PERIOD   0x04
#define FNM_CASEFOLD 0x10

static inline int fnmatch(const char *pattern, const char *string, int flags) {
    (void)flags; /* simplified: ignore flags */
    while (*pattern) {
        if (*pattern == '*') {
            /* skip consecutive stars */
            while (pattern[1] == '*') pattern++;
            pattern++;
            if (!*pattern) return 0; /* trailing * matches everything */
            for (; *string; string++) {
                if (fnmatch(pattern, string, flags) == 0) return 0;
            }
            return FNM_NOMATCH;
        }
        if (*pattern == '?') {
            if (!*string) return FNM_NOMATCH;
            pattern++; string++;
            continue;
        }
        if (!*string || *pattern != *string) return FNM_NOMATCH;
        pattern++; string++;
    }
    return *string ? FNM_NOMATCH : 0;
}

#else /* !_WIN32 */
#include_next <fnmatch.h>
#endif /* _WIN32 */

#endif /* COMPAT_FNMATCH_H */
