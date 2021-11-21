// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// fs.h: functions for mounting file systems

#ifndef INITRD_FS_H
#define INITRD_FS_H

extern int g_kmsgFd;

int mount_init(const char *target);
int mount_overlay(const char *rootDir, char **lowerDir, char **overlayData);
int mount_root(void);
int mount_vhd(const unsigned int devMode, const char *scsiPath,
    const unsigned int pmemId, const char *rootDir,
    const char *fstype, const unsigned int reqMode, const void *mountData);

#endif // INITRD_FS_H
