// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// net.h: functions for network connections

#ifndef INITRD_NET_H
#define INITRD_NET_H

#include <stdbool.h>

int connect_hv_socket(const unsigned int port, const int newfd,
    const bool cloexec);
int mount_plan_nine(const char *source, const char *target);
int nic_enable(const int sock, const char *nic);
int nic_addip(const char *ipaddr, const char *gateway, const char prefix);

#endif // INITRD_NET_H
