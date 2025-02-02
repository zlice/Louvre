#include <LCompositor.h>
#include <LLauncher.h>
#include <LLog.h>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/prctl.h>
#include <sys/poll.h>

using namespace Louvre;

static int pipeA[2] =
{
    -1, // Daemon read end
    -1  // Compositor write end
};

static int pipeB[2] =
{
    -1, // Compositor read end
    -1  // Daemon write end
};

static pid_t daemonPID = -1;
static pid_t daemonGID = -1;

static Int32 daemonLoop()
{
    close(pipeA[1]);
    close(pipeB[0]);

    fcntl(pipeA[0], F_SETFD, fcntl(pipeA[0], F_GETFD) | FD_CLOEXEC);
    fcntl(pipeB[1], F_SETFD, fcntl(pipeB[1], F_GETFD) | FD_CLOEXEC);

    if (setpgid(0, 0) == 0)
        daemonGID = getpgrp();

    pollfd fds;
    fds.events = POLLIN;
    fds.revents = 0;
    fds.fd = pipeA[0];

    // Read messages from the parent
    std::string cmd =  "";
    Char8 c;

    while (true)
    {
        if (poll(&fds, 1, -1) != 1)
            return 1;

        Int32 n = read(pipeA[0], &c, sizeof(c));

        // Closed pipe
        if (n == 0)
            return 0;

        while (n > 0)
        {
            // Reached end of command
            if (c == '\0')
            {
                pid_t pid = fork();

                if (pid == 0)
                    exit(system(cmd.c_str()));
                else
                {
                    // Send the launched app PID to the compositor
                    cmd = "";
                    Char8 buffer[16];
                    sprintf(buffer, "%d", pid);
                    Char8 *ptr = buffer;

                    while (true)
                    {
                        ssize_t w = write(pipeB[1], ptr, sizeof(c));

                        if (*ptr == '\0')
                            break;

                        if (w > 0)
                            ptr += w;
                    }

                    goto readChar;
                }
            }
            else
            {
                // Append character to the command
                cmd += c;
            }

        readChar:
            n = read(pipeA[0], &c, sizeof(c));
        }
    }

    return 1;
}

pid_t LLauncher::startDaemon(const std::string &name)
{
    setenv("WAYLAND_DISPLAY", "wayland-2", 0);
    setenv("MOZ_ENABLE_WAYLAND", "1", 0);
    setenv("QT_QPA_PLATFORM", "wayland-egl", 0);

    char *wdisplay = getenv("WAYLAND_DISPLAY");

    if (wdisplay)
    {
        // For Firefox
        setenv("DISPLAY", wdisplay, 0);
        setenv("LOUVRE_WAYLAND_DISPLAY", wdisplay, 0);
    }

    LLog::init();

    if (daemonPID != -1)
    {
        LLog::error("[LLauncher::startDaemon] Failed to start daemon, already running.");
        goto error;
    }

    if (LCompositor::compositor())
    {
        LLog::error("[LLauncher::startDaemon] Failed to start daemon. Must be launched before an LCompositor instance is created.");
        goto error;
    }

    if (pipe(pipeA) != 0)
    {
        LLog::error("[LLauncher::startDaemon] Failed to start daemon. Failed to create pipe: %s.", strerror(errno));
        goto error;
    }

    if (pipe(pipeB) != 0)
    {
        LLog::error("[LLauncher::startDaemon] Failed to start daemon. Failed to create pipe: %s.", strerror(errno));
        goto closePipeA;
    }

    daemonGID = -1;
    daemonPID = fork();

    if (daemonPID == -1)
    {
        LLog::error("[LLauncher::startDaemon] Failed to start daemon. Failed to create daemon fork: %s.", strerror(errno));
        goto closePipeB;
    }
    else if (daemonPID == 0)
    {
        prctl(PR_SET_NAME, name.c_str(), 0, 0, 0);
        Int32 ret = daemonLoop();
        close(pipeA[0]);
        close(pipeB[1]);
        LLog::debug("[%s] Daemon exited with status %d.", name.c_str(), ret);

        if (daemonGID != -1)
            kill(-daemonGID , SIGTERM);

        exit(ret);
    }
    else
    {
        close(pipeA[0]);
        close(pipeB[1]);
        LLog::debug("[LLauncher::startDaemon] LLauncher daemon started successfully with PID: %d.", daemonPID);
        return daemonPID;
    }

closePipeB:
    close(pipeB[0]);
    close(pipeB[1]);
closePipeA:
    close(pipeA[0]);
    close(pipeA[1]);
error:
    return -1;
}

pid_t LLauncher::pid()
{
    return daemonPID;
}

pid_t LLauncher::launch(const std::string &command)
{
    if (daemonPID < 0)
    {
        LLog::error("[LLauncher::launch] Can not launch %s. Daemon is not running.", command.c_str());
        return -1;
    }

    if (command.empty())
    {
        LLog::error("[LLauncher::launch] Can not launch %s. Invalid command.", command.c_str());
        return -1;
    }

    pollfd fds;
    fds.events = POLLIN;
    fds.revents = 0;
    fds.fd = pipeB[0];
    Char8 c;
    const Char8 *cmd = command.c_str();

    // Read prev message if any
    if (poll(&fds, 1, 0) == 1)
        while (read(pipeB[0], &c, 1) > 0) {}

    ssize_t s;
    std::string res = "";

    // Send the command
    while (true)
    {
        s = write(pipeA[1], cmd, 1);

        if (s < 0)
            goto stop;

        if (s == 0)
            continue;

        if (*cmd == '\0')
            break;

        cmd += s;
    }

    // Get the launched app PID
    while (true)
    {
        if (poll(&fds, 1, 500) != 1)
            return -1;

        s = read(pipeB[0], &c, 1);

        if (s <= 0)
            goto stop;

        if (c == '\0')
        {
            pid_t pid = atoi(res.c_str());

            if (pid > 0)
                LLog::debug("[LLauncher::launch] Command %s executed successfuly. PID: %d.", command.c_str(), pid);
            else
                LLog::error("[LLauncher::launch] Command %s failed. PID: %d.", command.c_str(), pid);

            return pid;
        }

        res += c;
    }

    return -1;

    stop:
    LLog::error("[LLauncher::launch] Command %s failed. Daemon died.", command.c_str());
    stopDaemon();
    return -1;
}

void LLauncher::stopDaemon()
{
    if (daemonPID < 0)
        return;

    daemonPID = -1;

    close(pipeB[0]);
    close(pipeA[1]);

    LLog::debug("[LLauncher::stopDaemon] Daemon stopped.");
}
