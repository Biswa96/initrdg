// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// proc.h: functions for executing process

#ifndef INITRD_PROC_H
#define INITRD_PROC_H

extern int g_addGui;

int start_gns(const int gnsSock);
int start_localhost(void);
int start_telemetry(void);
int start_tracker(void);
int start_init(int sock, char *rootDir, char *initCommand,
    char *vmId, char *distroName, char *sharedMem);
int start_overlay_init(int sock, char *rootDir, char *initCommand,
    char *vmId, char *distroName, char *sharedMem);

#endif // INITRD_PROC_H
