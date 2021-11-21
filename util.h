// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// util.h: functions for wrapping some common utilities

#ifndef INITRD_UTIL_H
#define INITRD_UTIL_H

#include <stdio.h>
#include <sys/stat.h>

#define LXSS_SERVER_FD 100
#define LXSS_SERVER_PORT 50000
#define LXSS_CLIENT_PORT 50001

#define LOG_ERROR(str, ...) { dprintf(g_kmsgFd, "<3>ERROR: %s:%u: " str "\n",__func__, __LINE__, ##__VA_ARGS__); }
#define LOG_INFO(str, ...) { dprintf(g_kmsgFd, "<6>INFO: %s:%u: " str "\n", __func__, __LINE__, ##__VA_ARGS__); }

#ifndef TEMP_FAILURE_RETRY
#define TEMP_FAILURE_RETRY(expression) \
    (__extension__ \
        ({ long int __result; \
            do __result = (long int) (expression); \
            while (__result == -1L && errno == EINTR); \
            __result; }))
#endif

int util_devdelete(const char *scsiPath);
int util_devpath(const char *scsiPath, char **blkDev);
int util_entropy(const void *buf, const int len);
int util_mkdir(const char *path, const mode_t mode);
int util_mkdtemp(const char *root, char **dest);
int util_mount(const char *source, const char *target, const char *fstype,
    const unsigned long flags, const void *data, const long timeout);
int util_symlink(const char *target, const char *link);
int util_writefile(const char *path, const char *data);
int util_writeport(const unsigned short low, const unsigned short high);

#endif // INITRD_UTIL_H
