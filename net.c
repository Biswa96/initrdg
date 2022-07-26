// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// net.c: functions for network connections

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <net/if.h>
#include <net/route.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <sys/socket.h>
#include <linux/vm_sockets.h>
#include <unistd.h>

#include "fs.h"
#include "util.h"

#define VSOCK_BUFFER_SIZE 0x10000

int connect_hv_socket(const unsigned int port, const int newfd,
    const bool cloexec)
{
    int ret;
    int flag;

    flag = SOCK_STREAM | (cloexec ? SOCK_CLOEXEC : 0);
    const int sock = socket(AF_VSOCK, flag, 0);
    if (sock < 0)
    {
        LOG_ERROR("socket %d", errno);
        return sock;
    }

    struct timeval timeout = { .tv_sec = 30, .tv_usec = 0 };
    ret = setsockopt(sock, AF_VSOCK, SO_VM_SOCKETS_CONNECT_TIMEOUT,
        &timeout, sizeof timeout);
    if (ret < 0)
    {
        LOG_ERROR("setsockopt %d", errno);
        goto cleanup;
    }

    struct sockaddr_vm addr = { 0 };
    addr.svm_family = AF_VSOCK;
    addr.svm_cid = VMADDR_CID_HOST;
    addr.svm_port = port;
    ret = connect(sock, (struct sockaddr *)&addr, sizeof addr);
    if (ret < 0)
    {
        LOG_ERROR("connect %d", errno);
        goto cleanup;
    }

    if (newfd < 0 || sock == newfd)
        return sock;

    flag = cloexec ? O_CLOEXEC : O_RDONLY;
    ret = dup3(sock, newfd, flag);
    if (ret < 0)
        LOG_ERROR("dup3 %d", errno);

cleanup:
    close(sock);
    return ret;
}

int mount_plan_nine(const char *source, const char *target)
{
    int ret;
    char *mountData = NULL;

    const int sock = connect_hv_socket(LXSS_CLIENT_PORT, -1, true);
    if (sock < 0) return sock;

    const int size = VSOCK_BUFFER_SIZE;
    ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof size);
    if (ret < 0)
    {
        LOG_ERROR("setsockopt(SO_SNDBUF) %d", errno);
        goto cleanup;
    }

    ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof size);
    if (ret < 0)
    {
        LOG_ERROR("setsockopt(SO_RCVBUF) %d", errno);
        goto cleanup;
    }

    ret = asprintf(&mountData,
        "msize=%d,trans=fd,rfdno=%d,wfdno=%d,cache=loose,aname=%s;fmask=022",
        VSOCK_BUFFER_SIZE, sock, sock, source);
    if (ret < 0)
    {
        LOG_ERROR("asprintf(%s)", source);
        goto cleanup;
    }

    ret = util_mount(source, target, "9p", MS_RDONLY, mountData, 0);

cleanup:
    if (mountData)
        free(mountData);
    close(sock);
    return ret;
}

int nic_enable(const int sock, const char *nic)
{
    int ret;
    struct ifreq ifr;

    memset(&ifr, 0, sizeof ifr);
    strncpy(ifr.ifr_name, nic, IFNAMSIZ);
    ret = ioctl(sock, SIOCGIFFLAGS, &ifr);
    if (ret < 0)
    {
        LOG_ERROR("ioctl(SIOCGIFFLAGS) %d", errno);
        return ret;
    }

    ifr.ifr_flags |= IFF_UP | IFF_RUNNING;
    ret = ioctl(sock, SIOCSIFFLAGS, &ifr);
    if (ret < 0)
        LOG_ERROR("ioctl(SIOCSIFFLAGS) %d", errno);

    return ret;
}

int nic_addip(const char *ipaddr, const char *gateway, const char prefix)
{
    int ret;
    struct ifreq ifr;
    struct rtentry route;
    struct sockaddr_in *addr;

    const int sock = socket(AF_INET, SOCK_DGRAM, 0);
    if (sock < 0)
    {
        LOG_ERROR("socket %d", errno);
        return sock;
    }

    ret = nic_enable(sock, "lo");

    memset(&ifr, 0, sizeof ifr);
    strncpy(ifr.ifr_name, "eth0", IFNAMSIZ);
    addr = (struct sockaddr_in *)&ifr.ifr_addr;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = inet_addr(ipaddr);
    ret = ioctl(sock, SIOCSIFADDR, &ifr);
    if (ret < 0)
        LOG_ERROR("ioctl(SIOCSIFADDR) %d", errno);

    addr = (struct sockaddr_in *)&ifr.ifr_netmask;
    addr->sin_family = AF_INET;
    addr->sin_addr.s_addr = htonl(-1 << (32 - prefix));
    ret = ioctl(sock, SIOCSIFNETMASK, &ifr);
    if (ret < 0)
        LOG_ERROR("ioctl(SIOCSIFNETMASK) %d", errno);

    ret = nic_enable(sock, "eth0");

    memset(&route, 0, sizeof route);
    route.rt_dst.sa_family = AF_INET;
    route.rt_gateway.sa_family = AF_INET;
    route.rt_genmask.sa_family = AF_INET;
    route.rt_flags = RTF_UP | RTF_GATEWAY;
    addr = (struct sockaddr_in *)&route.rt_gateway;
    addr->sin_addr.s_addr = inet_addr(gateway);
    ret = ioctl(sock, SIOCADDRT, &route);
    if (ret < 0)
        LOG_ERROR("ioctl(SIOCADDRT) %d", errno);

    close(sock);
    return ret;
}
