// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// msg.c: functions for message transferring with Lxss service

#include <errno.h>
#include <sched.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/socket.h>
#include <sys/syscall.h>
#include <sys/utsname.h>
#include <unistd.h>
#include <linux/seccomp.h>

#include "fs.h"
#include "msg.h"
#include "net.h"
#include "proc.h"
#include "util.h"

ssize_t msg_cap(const int msgSock)
{
    ssize_t ret;
    struct utsname unameBuf;
    struct initrd_msg_send_caps *msg = NULL;

    memset(&unameBuf, 0, sizeof unameBuf);
    if (uname(&unameBuf) < 0)
    {
        LOG_ERROR("uname %d\n", errno);
        return -1;
    }

    size_t msgLen = strlen(unameBuf.release) + sizeof(char) + sizeof(*msg);
    msg = malloc(msgLen);
    if (!msg)
    {
        LOG_ERROR("malloc %zd", msgLen);
        return -1;
    }

    const unsigned int arg = SECCOMP_RET_USER_NOTIF;
    msg->type = MSG_SEND_CAPS;
    msg->len = msgLen;
    msg->seccomp_notif = syscall(SYS_seccomp, SECCOMP_GET_ACTION_AVAIL, 0, &arg) == 0;
    strcpy(msg->release, unameBuf.release);
    ret = TEMP_FAILURE_RETRY(write(msgSock, msg, msgLen));
    if (ret != msgLen)
        LOG_ERROR("write %d", errno);

    free(msg);
    return ret;
}

ssize_t msg_receive(const int msgSock, struct initrd_msg_buffer **buf, size_t *len)
{
    ssize_t ret;
    struct initrd_msg_header header = { 0 };

    ret = TEMP_FAILURE_RETRY(recv(msgSock, &header, sizeof header, MSG_WAITALL));
    if (ret <= 0)
    {
        LOG_ERROR("recv %d", errno);
        return ret;
    }

    if (ret != sizeof header)
    {
        LOG_ERROR("header size incorrect %zd", ret);
        return -1;
    }

    LOG_INFO("ret %zd", ret);
    LOG_INFO("header.type %d", header.type);
    LOG_INFO("header.len %d", header.len);

    size_t temp_len = header.len;
    if (!(*buf) | (*len < header.len))
    {
        void *new_buf = realloc(*buf, header.len);
        if (!new_buf)
            return -1;
        *buf = new_buf;
        *len = header.len;
    }

    (*buf)->type = header.type;
    (*buf)->len = header.len;
    void **msg = &((*buf)->msg);

    for (temp_len -= sizeof header; temp_len; temp_len -= ret)
    {
        ret = TEMP_FAILURE_RETRY(recv(msgSock, msg, temp_len, 0));
        if (ret <= 0)
            return ret;

        msg = (void **)((char *)msg + ret);
    }

    return header.len;
}

int msg_process(const int msgSock, struct initrd_msg_buffer *buf, size_t len)
{
    int ret = -1;

    switch (buf->type)
    {
        case MSG_START_INIT:
        case MSG_IMPORT_DISTRO:
        case MSG_EXPORT_DISTRO:
        {
            struct initrd_msg_start_init *msg = (void*)buf;
            LOG_INFO("distro_scsi_path %s", (char*)msg + msg->distro_scsi_path);

            const int writeSock = connect_hv_socket(LXSS_SERVER_PORT, -1, false);
            if (writeSock < 0) return writeSock;

            const int tidUserDistro = syscall(SYS_clone,
                CLONE_NEWPID | CLONE_NEWIPC | CLONE_NEWUTS | CLONE_NEWNS | SIGCHLD, 0, 0, 0, 0);
            if (tidUserDistro < 0)
            {
                LOG_ERROR("clone tidUserDistro %d", errno);
                return tidUserDistro;
            }

            if (!tidUserDistro) // child
            {
                ret = mount_vhd(DEVICE_MODE_SCSI,
                        (char*)msg + msg->distro_scsi_path, 0, "/distro",
                        "ext4", 0, "discard,errors=remount-ro,data=ordered");

                if (buf->type)
                {
                    char *pidData = NULL;
                    if (asprintf(&pidData, "%d\n", getpid()) > 0)
                    {
                        util_writefile("/sys/fs/cgroup/memory/64M/tasks", pidData);
                        free(pidData);
                    }

                    if (buf->type == MSG_IMPORT_DISTRO)
                        ret = start_import("/distro");
                    if (buf->type == MSG_EXPORT_DISTRO)
                        ret = start_export("/distro");
                    if (TEMP_FAILURE_RETRY(write(writeSock, &ret, sizeof ret)) < 0)
                        LOG_ERROR("write(writeSock) %d", errno);
                    close(writeSock);
                    exit(ret);
                }

                util_mount(NULL, "/wslg", "tmpfs", 0, NULL, 0);
                mount(NULL, "/wslg", NULL, MS_SHARED, NULL);
                g_addGui = true;

                // Assume system.vhd is read-only
                if (ret < 0)
                {
                    mount_vhd(DEVICE_MODE_SCSI,
                        (char*)msg + msg->distro_scsi_path, 0, "/systemvhd",
                        "ext4", REQUEST_MOUNT_SYSTEM_VHD, NULL);

                    start_overlay_init(writeSock, "/system", NULL, NULL, NULL, NULL);
                }
                else
                    start_init(writeSock, "/distro", NULL, NULL, NULL, NULL);
            }
            else // parent
            {
                ret = TEMP_FAILURE_RETRY(write(writeSock, &tidUserDistro, sizeof tidUserDistro));
                if (ret < 0)
                    LOG_ERROR("write(writeSock) %d", errno);
            }

            break;
        }
        case MSG_EJECT_SCSI:
        {
            struct initrd_msg_eject_scsi *msg = (void*)buf;
            LOG_INFO("distro_scsi_path %s", (char*)msg + msg->distro_scsi_path);

            const int ejectRet = util_devdelete((char*)msg + msg->distro_scsi_path);

            ret = TEMP_FAILURE_RETRY(write(msgSock, &ejectRet, sizeof ejectRet));
            if (ret < 0)
                LOG_ERROR("write(msgSock) %d", errno);

            break;
        }
        case MSG_START_PROC:
        {
            struct initrd_msg_start_proc *msg = (void*)buf;

            LOG_INFO("swap_size %ld", msg->swap_size);
            LOG_INFO("entropy_size %d", msg->entropy_size);
            LOG_INFO("compact_timeout %d", msg->compact_timeout);
            LOG_INFO("eth0_ipaddr %s", (char*)msg + msg->eth0_ipaddr);
            LOG_INFO("eth0_gateway %s", (char*)msg + msg->eth0_gateway);
            LOG_INFO("swap_scsi_path %s", (char*)msg + msg->swap_scsi_path);
            LOG_INFO("eth0_prefix %d", msg->eth0_prefix);
            LOG_INFO("enable_telemetry %d", msg->enable_telemetry);
            LOG_INFO("enable_localhost %d", msg->enable_localhost);

            nic_addip((char*)msg + msg->eth0_ipaddr,
                (char*)msg + msg->eth0_gateway, msg->eth0_prefix);

            if (msg->enable_telemetry)
                start_telemetry();
            if (msg->enable_localhost)
                start_localhost();
            if (msg->entropy_size > 0)
                util_entropy((char*)msg + msg->entropy_buf, msg->entropy_size);

            break;
        }
        default:
            LOG_INFO("process_msg unimplemented header.type %d.\n", buf->type);
    }

    return ret;
}
