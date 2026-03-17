/*
 * Stub libgen.h for Windows/MINGW64.
 * dirname() and basename() are available in MINGW64 via <string.h>.
 */

#ifndef COMPAT_LIBGEN_H
#define COMPAT_LIBGEN_H

#ifdef _WIN32
/* MINGW64 provides dirname/basename via its own headers */
#include <stdlib.h>
#endif

#endif /* COMPAT_LIBGEN_H */
