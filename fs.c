// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// fs.c: functions for mounting file systems

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <sys/resource.h>
#include <unistd.h>

#include "msg.h"
#include "net.h"
#include "util.h"

volatile int g_kmsgFd = STDERR_FILENO;

int mount_init(const char *target)
{
    int ret;

    const int fd = open(target, O_WRONLY | O_CREAT | O_TRUNC, 0755);
    if (fd < 0)
    {
        LOG_ERROR("open(%s) %d", target, errno);
        return fd;
    }

    ret = mount("/tools/init", target, NULL, MS_BIND, NULL);
    if (ret < 0)
        LOG_ERROR("mount(%s, %s) %d", "/tools/init", target, errno);

    close(fd);
    return ret;
}

int mount_overlay(const char *rootDir, char **lowerDir, char **overlayData)
{
    int ret;
    char *rwDir = NULL, *upperDir = NULL, *workDir = NULL;

    ret = util_mkdir(rootDir, 0755);
    if (ret < 0) return ret;

    ret = asprintf(lowerDir, "%s/lower", rootDir);
    if (ret < 0) return ret;

    ret = util_mkdir(*lowerDir, 0755);
    if (ret < 0) return ret;

    ret = asprintf(&rwDir, "%s/rw", rootDir);
    if (ret < 0) return ret;

    ret = util_mount(NULL, rwDir, "tmpfs", 0, NULL, 0);
    if (ret < 0) return ret;

    ret = asprintf(&upperDir, "%s/rw/upper", rootDir);
    if (ret < 0) return ret;

    ret = util_mkdir(upperDir, 0755);
    if (ret < 0) return ret;

    ret = asprintf(&workDir, "%s/rw/work", rootDir);
    if (ret < 0) return ret;

    ret = util_mkdir(workDir, 0755);
    if (ret < 0) return ret;

    ret = asprintf(overlayData, "lowerdir=%s,upperdir=%s,workdir=%s",
        *lowerDir, upperDir, workDir);

    free(rwDir);
    free(upperDir);
    free(workDir);
    return ret;
}

int mount_root(void)
{
    int ret;

    ret = util_mount(NULL, "/dev", "devtmpfs", 0, NULL, 0);
    if (ret < 0) return ret;

    int kmsgFd = TEMP_FAILURE_RETRY(open("/dev/kmsg", O_WRONLY | O_CLOEXEC));
    if (kmsgFd < 0)
    {
        LOG_ERROR("open %d", errno);
    }
    else
        g_kmsgFd = kmsgFd;

    ret = util_mount(NULL, "/proc", "proc", 0, NULL, 0);
    if (ret < 0) return ret;

    ret = util_mount(NULL, "/sys", "sysfs", 0, NULL, 0);
    if (ret < 0) return ret;

    ret = util_mount(NULL, "/sys/fs/cgroup", "tmpfs", 0, NULL, 0);
    if (ret < 0) return ret;

    ret = util_mount(NULL, "/sys/fs/cgroup/memory", "cgroup",
        MS_RELATIME | MS_NOEXEC | MS_NODEV | MS_NOSUID, "memory", 0);
    if (ret < 0) return ret;

    ret = util_mkdir("/sys/fs/cgroup/memory/64M", 0755);
    if (ret < 0) return ret;

    ret = util_writefile("/sys/fs/cgroup/memory/64M/memory.limit_in_bytes",
        "64M\n");
    if (ret < 0) return ret;

    ret = util_mount(NULL, "/share", "tmpfs", 0, NULL, 0);
    if (ret < 0) return ret;

    ret = mount(NULL, "/share", NULL, MS_SHARED, NULL);
    if (ret < 0) return ret;

    ret = mount_plan_nine("tools", "/tools");
    if (ret < 0) return ret;

    ret = util_symlink("/tools/init", "/localhost");
    if (ret < 0) return ret;

    ret = util_symlink("/tools/init", "/telagent");
    if (ret < 0) return ret;

    ret = util_symlink("/tools/init", "/gns");
    if (ret < 0) return ret;

    ret = util_mount(NULL, "/proc/sys/fs/binfmt_misc", "binfmt_misc",
        MS_RELATIME, NULL, 0);
    if (ret < 0) return ret;

    ret = util_writefile("/proc/sys/fs/binfmt_misc/register",
        ":WSLInterop:M::MZ::/tools/init:F\n");
    if (ret < 0) return ret;

    ret = util_writefile("/proc/sys/kernel/dmesg_restrict", "0\n");
    if (ret < 0) return ret;

    ret = util_writefile("/proc/sys/fs/inotify/max_user_watches", "524288\n");
    if (ret < 0) return ret;

    struct rlimit rlim = { .rlim_cur = 0x400, .rlim_max = 0x100000 };
    ret = setrlimit(RLIMIT_NOFILE, &rlim);
    if (ret < 0)
    {
        LOG_ERROR("setrlimit %d", errno);
        return ret;
    }

    ret = util_writefile("/proc/sys/kernel/print-fatal-signals", "1\n");
    if (ret < 0) return ret;

    ret = util_writefile("/proc/sys/kernel/printk_devkmsg", "on\n");
    if (ret < 0) return ret;

    ret = util_mkdir("/etc", 0755);
    if (ret < 0) return ret;

    ret = util_symlink("/share/resolv.conf", "/etc/resolv.conf");
    if (ret < 0) return ret;

    return ret;
}

int mount_vhd(const unsigned int devMode, const char *scsiPath,
    const unsigned int pmemId, const char *rootDir,
    const char *fstype, const unsigned int reqMode, const void *mountData)
{
    int ret;
    char *blkDev = NULL, *target = NULL, *overlayData = NULL;

    if (devMode == DEVICE_MODE_SCSI)
        ret = util_devpath(scsiPath, &blkDev);
    else if (devMode == DEVICE_MODE_PMEM)
        ret = asprintf(&blkDev, "/dev/pmem%u", pmemId);
    else
    {
        LOG_ERROR("wrong device mode %u", devMode);
        return -1;
    }

    if (ret < 0) return ret;

    if (reqMode & REQUEST_CREATE_OVERLAY_FS)
        ret = mount_overlay(rootDir, &target, &overlayData);
    else
        target = (char*)rootDir;

    if (ret > 0)
    {
        ret = util_mount(blkDev, target, fstype,
            (reqMode & REQUEST_MOUNT_SYSTEM_VHD), mountData, 15000000000);

        if (ret >= 0 && (reqMode & REQUEST_CREATE_OVERLAY_FS))
            ret = util_mount(NULL, rootDir, "overlay", 0, overlayData, 0);
    }

    free(blkDev);
    if (reqMode & REQUEST_CREATE_OVERLAY_FS)
    {
        free(target);
        free(overlayData);
    }

    return ret;
}
