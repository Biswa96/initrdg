// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// msg.h: functions for message transferring with Lxss service

#ifndef INITRD_MSG_H
#define INITRD_MSG_H

enum initrd_msg_type
{
    MSG_START_INIT = 0,
    MSG_IMPORT_DISTRO = 1,
    MSG_EXPORT_DISTRO = 2,
    MSG_EJECT_SCSI = 3,
    MSG_START_PROC = 4,
    MSG_MOUNT_DISK = 5,
    MSG_UNMOUNT_DISK = 6,
    MSG_SEND_CAPS = 9
};

struct initrd_msg_header
{
    enum initrd_msg_type type;
    int len;
};

struct initrd_msg_buffer
{
    enum initrd_msg_type type;
    int len;
    void *msg;
};

// type 0
enum initrd_request_mode
{
    REQUEST_MOUNT_SYSTEM_VHD = 1,
    REQUEST_CREATE_OVERLAY_FS = 2,
    REQUEST_START_SYSTEM_INIT = 4
};

enum initrd_device_mode
{
    DEVICE_MODE_SCSI = 1,
    DEVICE_MODE_PMEM = 2
};

struct initrd_msg_start_init
{
    enum initrd_msg_type type;
    unsigned int len;
    unsigned int distro_scsi_path;
};

// type 3
struct initrd_msg_eject_scsi
{
    enum initrd_msg_type type;
    unsigned int len;
    unsigned int distro_scsi_path;
};

// type 4
struct initrd_msg_start_proc
{
    enum initrd_msg_type type;
    unsigned int len;
    long swap_size;
    int entropy_size;
    int compact_timeout;
    unsigned int eth0_ipaddr;
    unsigned int eth0_gateway;
    unsigned int swap_scsi_path;
    unsigned int entropy_buf;
    char eth0_prefix;
    char enable_telemetry;
    char enable_localhost;
};

// type 9
struct initrd_msg_send_caps
{
    enum initrd_msg_type type;
    int len;
    char seccomp_notif;
    char release[];
};

ssize_t msg_cap(const int msgSock);
ssize_t msg_receive(const int msgSock, struct initrd_msg_buffer **buf, size_t *len);
int msg_process(const int sock, struct initrd_msg_buffer *buf, size_t len);

#endif // INITRD_MSG_H
