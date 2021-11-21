// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// proc.c: functions for executing process

#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mount.h>
#include <unistd.h>

#include "fs.h"
#include "net.h"
#include "util.h"

volatile int g_addGui = false;

int start_gns(const int gnsSock)
{
    const pid_t childPid = fork();
    if (childPid < 0)
    {
        LOG_ERROR("fork %d", errno);
        return childPid;
    }

    if (!childPid)
    {
        char str[10];
        memset(str, '\0', sizeof str);
        sprintf(str, "%d", gnsSock);
        if (execl("gns", "gns", "--socket", str, NULL) < 0)
            LOG_ERROR("execl %d", errno);
        _exit(0);
    }

    return 0;
}

int start_localhost(void)
{
    const int sock = connect_hv_socket(LXSS_SERVER_PORT, LXSS_SERVER_FD, false);
    close(sock);
    return 0;
}

int start_telemetry(void)
{
    const int sock = connect_hv_socket(LXSS_SERVER_PORT, LXSS_SERVER_FD, false);
    close(sock);
    return 0;
}

int start_tracker(void)
{
    const int sock = connect_hv_socket(LXSS_SERVER_PORT, -1, false);
    close(sock);
    return 0;
}

int start_init(int sock, char *rootDir, char *initCommand,
    char *vmId, char *distroName, char *sharedMem)
{
#define TOTAL_ENVCOUNT 9

    int ret = 0, envCount = 0;
    char *userDistro = NULL, *initMount = NULL, *systemDistro = NULL;
    size_t rootLen = strlen(rootDir);

    char *envp[TOTAL_ENVCOUNT];
    memset(envp, 0, sizeof envp);

    if (initCommand && *initCommand)
    {
        close(sock);
        sock = -1;
    }
    else
    {
        initCommand = "/init";

        ret = util_mkdtemp(rootDir, &userDistro);
        if (ret < 0) return ret;

        ret = util_mount("/share", userDistro, NULL, MS_BIND | MS_REC, NULL, 0);
        if (ret < 0) return ret;

        if (g_addGui)
        {
            ret = util_mkdtemp(rootDir, &systemDistro);
            if (ret < 0) return ret;

            ret = util_mount("/wslg", systemDistro, NULL, MS_MOVE, NULL, 0);
            if (ret < 0) return ret;
        }

        if (asprintf(&initMount, "%s%s", rootDir, initCommand) < 0)
        {
            LOG_ERROR("asprintf(%s)", rootDir);
            return -1;
        }

        ret = mount_init(initMount);
        if (ret < 0)
            return ret;
        free(initMount);

        if (sock != LXSS_SERVER_FD)
        {
            ret = dup2(sock, LXSS_SERVER_FD);
            if (ret < 0)
            {
                LOG_ERROR("dup2 %d", errno);
                return ret;
            }
            close(sock);
            sock = LXSS_SERVER_FD;
        }

        if (asprintf(&envp[envCount], "WSL2_CROSS_DISTRO=%s",
            &userDistro[rootLen]) < 0)
        {
            LOG_ERROR("asprintf(%s)", userDistro);
            return -1;
        }
        envCount++;

        if (g_addGui)
        {
            envp[envCount] = "WSL2_GUI_APPS_ENABLED=1";
            envCount++;

            if (asprintf(&envp[envCount], "WSL2_SYSTEM_DISTRO_SHARE=%s",
                &systemDistro[rootLen]) < 0)
            {
                LOG_ERROR("asprintf(%s)", systemDistro);
                return -1;
            }
            envCount++;
        }

        if (vmId && *vmId)
        {
            if (asprintf(&envp[envCount], "WSL2_VM_ID=%s", vmId) < 0)
            {
                LOG_ERROR("asprintf(%s)", vmId);
                return -1;
            }
            envCount++;
        }

        if (distroName && *distroName)
        {
            if (asprintf(&envp[envCount], "WSL2_DISTRO_NAME=%s", distroName) < 0)
            {
                LOG_ERROR("asprintf(%s)", distroName);
                return -1;
            }
            envCount++;
        }

        if (sharedMem && *sharedMem)
        {
            if (asprintf(&envp[envCount], "WSL2_SHARED_MEMORY_OB_DIRECTORY=%s",
                sharedMem) < 0)
            {
                LOG_ERROR("asprintf(%s)", sharedMem);
                return -1;
            }
            envCount++;
        }

        if (envp[TOTAL_ENVCOUNT -1])
        {
            LOG_ERROR("envp[%d] has more member than expected", envCount);
            exit(1);
        }

        chdir(rootDir);
        mount(".", "/", NULL, MS_MOVE, NULL);
        chroot(".");

        if (g_addGui)
        {
            util_mount(&systemDistro[strlen(rootDir)], "/mnt/wslg", NULL, MS_MOVE, NULL, 0);

            if (rmdir(&systemDistro[strlen(rootDir)]) < 0)
                LOG_ERROR("rmdir %d", errno);

            if (remove("/tmp/.X11-unix") < 0)
                LOG_ERROR("remove %d", errno);

            if (symlink("/mnt/wsl/.X11-unix", "/tmp/.X11-unix") < 0)
                LOG_ERROR("symlink %d", errno);
        }

        execle(initCommand, initCommand, NULL, envp);
    }

    free(userDistro);
    close(sock);
    exit(ret);
    return 0;
}

int start_overlay_init(int sock, char *rootDir, char *initCommand,
    char *vmId, char *distroName, char *sharedMem)
{
    int ret;
    char *lowerDir = NULL, *overlayData = NULL;

    ret = mount_overlay(rootDir, &lowerDir, &overlayData);
    if (ret < 0) return ret;

    ret = util_mount("/systemvhd", lowerDir, NULL, MS_BIND, NULL, 0);
    if (ret < 0) return ret;

    ret = util_mount(NULL, rootDir, "overlay", 0, overlayData, 0);
    if (ret < 0) return ret;

    free(lowerDir);
    free(overlayData);

    ret = start_init(sock, rootDir, initCommand, vmId, distroName, sharedMem);
    exit(ret);
}
