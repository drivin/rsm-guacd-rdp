/*
 * Windows compatibility stub for <sys/statvfs.h>
 *
 * Provides struct statvfs and statvfs() using GetDiskFreeSpaceExA.
 * guacamole's RDP fs.c uses statvfs() to report free/total space
 * for the virtual drive shared over RDP.
 */

#ifndef COMPAT_SYS_STATVFS_H
#define COMPAT_SYS_STATVFS_H

#ifdef _WIN32

#include <windows.h>
#include <stdint.h>
#include <errno.h>

#define ST_RDONLY 0x0001
#define ST_NOSUID 0x0002

struct statvfs {
    unsigned long  f_bsize;    /* filesystem block size        */
    unsigned long  f_frsize;   /* fragment size (== f_bsize)   */
    uint64_t       f_blocks;   /* total blocks in f_frsize units */
    uint64_t       f_bfree;    /* free blocks                  */
    uint64_t       f_bavail;   /* free blocks for unprivileged */
    uint64_t       f_files;    /* total inodes                 */
    uint64_t       f_ffree;    /* free inodes                  */
    uint64_t       f_favail;   /* free inodes for unprivileged */
    unsigned long  f_fsid;     /* filesystem ID                */
    unsigned long  f_flag;     /* mount flags                  */
    unsigned long  f_namemax;  /* max filename length          */
};

static inline int statvfs(const char *path, struct statvfs *buf) {
    ULARGE_INTEGER avail, total, free_bytes;

    if (!GetDiskFreeSpaceExA(path, &avail, &total, &free_bytes)) {
        errno = ENOENT;
        return -1;
    }

    /* Use 4096-byte blocks */
    buf->f_bsize   = 4096;
    buf->f_frsize  = 4096;
    buf->f_blocks  = total.QuadPart      / 4096;
    buf->f_bfree   = free_bytes.QuadPart / 4096;
    buf->f_bavail  = avail.QuadPart      / 4096;
    buf->f_files   = 0xFFFFFFFF;   /* inode count: fake large number */
    buf->f_ffree   = 0xFFFFFFFF;
    buf->f_favail  = 0xFFFFFFFF;
    buf->f_fsid    = 0;
    buf->f_flag    = 0;
    buf->f_namemax = MAX_PATH;
    return 0;
}

#else /* !_WIN32 */
#include_next <sys/statvfs.h>
#endif /* _WIN32 */

#endif /* COMPAT_SYS_STATVFS_H */
