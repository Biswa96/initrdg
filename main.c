// This file is part of initrg project.
// Licensed under the terms of the GNU General Public License v3 or later.

// main.c: main function

#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/signalfd.h>
#include <sys/reboot.h>
#include <sys/wait.h>
#include <unistd.h>

#include "fs.h"
#include "msg.h"
#include "net.h"
#include "proc.h"
#include "util.h"

int main(void)
{
    int ret;

    const int msgSock = connect_hv_socket(LXSS_SERVER_PORT, -1, true);
    if (msgSock < 0)
    {
        LOG_ERROR("msgSock %d", errno);
        return -1;
    }

    if (msg_cap(msgSock) < 0)
        return -1;

    const int writeSock = connect_hv_socket(LXSS_SERVER_PORT, -1, true);
    if (writeSock < 0)
    {
        LOG_ERROR("writeSock %d", errno);
        return -1;
    }

    if (mount_root() < 0)
        return -1;

    sigset_t set;
    sigemptyset(&set);
    sigaddset(&set, SIGCHLD);
    ret = sigprocmask(SIG_BLOCK, &set, NULL);
    const int sigFd = signalfd(-1, &set, SFD_CLOEXEC);
    if (sigFd < 0)
    {
        LOG_ERROR("signalfd %d", errno);
        return -1;
    }

    struct pollfd pfds[2] = {
        { msgSock, POLLIN, 0 },
        { sigFd, POLLIN, 0 }
    };
    size_t msgLen = 0;
    struct initrd_msg_buffer *buf = NULL;

    while (true)
    {
        do
        {
pollstart:
            if (poll(pfds, 2, -1) < 0)
            {
                LOG_ERROR("poll %d", errno);
                goto cleanup;
            }

            if (pfds[0].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                LOG_ERROR("poll msgSock %d", errno);
                goto cleanup;
            }

            if (pfds[0].revents & POLLIN)
            {
                ssize_t recvRet = msg_receive(msgSock, &buf, &msgLen);
                if (recvRet <= 0)
                    goto cleanup;
                msg_process(msgSock, buf, recvRet);
            }

            if (pfds[1].revents & (POLLERR | POLLHUP | POLLNVAL))
            {
                LOG_ERROR("poll sigFd %d", errno);
                goto cleanup;
            }
        }
        while ((pfds[1].revents & POLLIN) == 0);

        struct signalfd_siginfo siginfo;
        ret = TEMP_FAILURE_RETRY(read(sigFd, &siginfo, sizeof siginfo));

        if(ret != sizeof siginfo)
        {
            LOG_ERROR("read(sigFd) %d", errno);
            goto cleanup;
        }

        if (siginfo.ssi_signo != SIGCHLD)
        {
            LOG_ERROR("read(sigFd) signal %d.\n", siginfo.ssi_signo);
            break;
        }

        while (true)
        {
            int wstatus;
            pid_t child = waitpid(-1, &wstatus, WNOHANG);
            if (!child)
                break;
            if (child <= 0)
            {
                if (errno != ECHILD)
                    LOG_ERROR("waitpid %d", errno);
                goto pollstart;
            }
            sync();

            ret = TEMP_FAILURE_RETRY(write(writeSock, &child, sizeof child));
            if (ret < 0)
                LOG_ERROR("write(writeSock) %d", errno);
        }
    }

cleanup:
    free(buf);
    close(sigFd);
    close(msgSock);
    close(writeSock);
    sync();
    LOG_INFO("main exit %d", errno);
    reboot(RB_POWER_OFF);
}
