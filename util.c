// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// util.c: functions for wrapping some common utilities

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/random.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <time.h>
#include <unistd.h>

#include "fs.h"
#include "util.h"

int util_devdelete(const char *scsiPath)
{
    int ret;

    const int fd = open(scsiPath, O_WRONLY);
    if (fd < 0)
    {
        LOG_ERROR("open(%s) %d", scsiPath, errno);
        return fd;
    }

    ret = TEMP_FAILURE_RETRY(write(fd, "1\n", 2));
    if (ret < 0)
        LOG_ERROR("write(%s) %d", scsiPath, errno);

    close(fd);
    return ret;
}

int util_devpath(const char *scsiPath, char **blkDev)
{
    int ret = -1;
    struct timespec start, end;
    DIR *dir = NULL;
    struct dirent *dent = NULL;

    clock_gettime(CLOCK_MONOTONIC, &start);
    while(true)
    {
        if (dir)
            closedir(dir);

        dir = opendir(scsiPath);
        if (dir)
        {
            do
                dent = readdir(dir);
            while (dent && dent->d_name[0] == '.');
        }

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (dent || end.tv_nsec - start.tv_nsec
            + 1000000000 * (end.tv_sec - start.tv_sec) > 15000000000)
        {
            break;
        }
        usleep(10000);
    }

    if (dir)
    {
        if (dent)
        {
            *blkDev = NULL;
            ret = asprintf(blkDev, "/dev/%s", dent->d_name);
        }
        else
            LOG_ERROR("readdir %d", errno);
    }
    else
        LOG_ERROR("opendir %d", errno);

    if (dir)
        closedir(dir);
    return ret;
}

int util_entropy(const void *buf, const int len)
{
    int ret = -1;
    struct rand_pool_info *rinfo = NULL;

    const int fd = open("/dev/random", O_RDWR);
    if (fd < 0)
    {
        LOG_ERROR("open %d", errno);
        return fd;
    }

    rinfo = malloc(len + sizeof (*rinfo));
    if (rinfo)
    {
        rinfo->entropy_count = sizeof (*rinfo) * len;
        rinfo->buf_size = len;
        memcpy(rinfo->buf, buf, len);
        ret = ioctl(fd, RNDADDENTROPY, rinfo);
        if (ret < 0)
            LOG_ERROR("ioctl(RNDADDENTROPY) %d", errno);
        free(rinfo);
    }

    close(fd);
    return ret;
}

int util_mkdir(const char *path, const mode_t mode)
{
    const int ret = mkdir(path, mode);
    if (ret < 0 && errno != EEXIST)
    {
        LOG_ERROR("mkdir(%s) %d", path, errno);
        return ret;
    }

    return 0;
}

int util_mkdtemp(const char *root, char **dest)
{
    int ret;

    ret = asprintf(dest, "%s/wslXXXXXX", root);
    if (ret < 0)
    {
        LOG_ERROR("asprintf(%s)", root);
        return ret;
    }

    if (!mkdtemp(*dest))
    {
        LOG_ERROR("mkdtemp(%s) %d", root, errno);
        ret = -1;
    }

    return ret;
}

int util_mount(const char *source, const char *target, const char *fstype,
    const unsigned long flags, const void *data, const long timeout)
{
    int ret;
    struct timespec start, end;

    ret = util_mkdir(target, 0755);
    if (ret < 0) return ret;

    if (timeout)
        clock_gettime(CLOCK_MONOTONIC, &start);

    while (true)
    {
        ret = mount(source, target, fstype, flags, data);
        if (!ret || errno != ENOENT || !timeout)
            break;

        clock_gettime(CLOCK_MONOTONIC, &end);
        if (end.tv_nsec - start.tv_nsec
            + 1000000000 * (end.tv_sec - start.tv_sec) > timeout)
        {
            break;
        }
        usleep(10000);
    }

    if (ret < 0)
        LOG_ERROR("mount(%s, %s) %d", source, target, errno);

    return ret;
}

int util_symlink(const char *target, const char *link)
{
    const int ret = symlink(target, link);
    if (ret < 0)
        LOG_ERROR("symlink(%s, %s) %d", target, link, errno);

    return ret;
}

int util_writefile(const char *path, const char *data)
{
    int ret;

    const int fd = open(path, O_WRONLY);
    if (fd < 0)
    {
        LOG_ERROR("open(%s) %d", path, errno);
        return fd;
    }

    ret = TEMP_FAILURE_RETRY(write(fd, data, strlen(data)));
    if (ret < 0)
        LOG_ERROR("write(%s) %d", path, errno);

    close(fd);
    return ret;
}

int util_writeport(const unsigned short low, const unsigned short high)
{
    int ret = -1;
    char *str = NULL;

    if (!(low | high))
        return 0;

    if (asprintf(&str, "%i %i", low, high) > 0)
    {
        ret = util_writefile("/proc/sys/net/ipv4/ip_local_port_range", str);
        free(str);
    }

    return ret;
}
